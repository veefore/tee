#include "tee.h"

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/uio.h> 
#include <errno.h>

#include <iostream>
#include <stdexcept>


const int STDIN_FD = 0;
const int STDOUT_FD = 1;
const int RETRY_LIMIT = 3;


cxxopts::ParseResult ParseArgs(int argc, char* argv[]) {
    cxxopts::Options options("tee", "Branch stdin to stdout and a file");
    options
        .add_options()
        ("a,append", "Append to file", cxxopts::value<bool>()->default_value("false"))
        ("f,filepath", "Path to file", cxxopts::value<std::string>()->default_value(""))
        ("second", "Second positional argument", cxxopts::value<std::string>())
        ("positional", "Other positional arguments", cxxopts::value<std::vector<std::string>>())
        ;

    options.parse_positional({"filepath", "second", "positional"});
    return options.parse(argc, argv);
}

int SysCalls::Open(const std::string filepath, bool append) {
    if (filepath == "")
        throw std::invalid_argument("Empty filepath");

    if (!append)
        unlink(filepath.c_str());

    int fd;
    auto flags = O_WRONLY | O_CREAT;
    if (append)
        flags |= O_APPEND;

    if ((fd = open(filepath.c_str(), flags, S_IRWXU)) == -1)
        throw std::runtime_error("Couldn't not open file at \"" + filepath + "\"");

    return fd;
}

int SysCalls::Close(int fd) { return close(fd); }

ssize_t SysCalls::Read(std::vector<char>& buffer, int fd) {
    ssize_t totalBytesRead = 0;
    ssize_t bytesRead = 0;
    int retries = 0;
    do {
        buffer.push_back('a');
        bytesRead = read(0, &buffer.back(), sizeof(char));
        if (bytesRead == -1) {
            int savedErrno = errno;
            std::cerr << "Syscall read() failed with errno: " << savedErrno << "\n"; 
            buffer.pop_back();

            retries++;
            if (retries == RETRY_LIMIT)
                throw std::runtime_error("Reached RETRY_LIMIT in calling read().\n");
            continue;
        }
        totalBytesRead += bytesRead;
    } while ((bytesRead == -1 || bytesRead == sizeof(char)) && buffer.back() != '\n');

    if (bytesRead == 0)
        buffer.pop_back();
    return totalBytesRead;
}

ssize_t SysCalls::Write(std::vector<char>& buffer, size_t start, size_t end, int fd) {
    if (end >= buffer.size())
        throw std::invalid_argument("Invalid end position provided to SysCallIO::Write()");

    size_t totalBytesWritten = 0;
    int retries = 0;
    for (size_t i = start; i <= end; i++) {
        auto bytesWritten = write(fd, &buffer[i], sizeof(char));
        if (bytesWritten == -1) {
            int savedErrno = errno;
            std::cerr << "Syscall write() failed with errno: " << savedErrno; 

            retries++;
            if (retries == RETRY_LIMIT)
                throw std::runtime_error("Reached RETRY_LIMIT in calling write().\n");
            i--;
            continue;
        }
        totalBytesWritten += bytesWritten;
    }

    return totalBytesWritten;
}

void Tee(int fd) {
    size_t start = 0;
    size_t end;
    std::vector<char> buffer;
    ssize_t bytesRead;

    do {
        // Read a chunk limited by EOF or \n
        bytesRead = SysCalls::Read(buffer, STDIN_FD);

        // Determine boundaries of the current chunk
        end = buffer.size() - 1;
        if (bytesRead != 0) {
            while (end >= start && buffer[end] != '\n')
                end--;
            if (end < start)
                end = buffer.size() - 1;
        }

        // Write the chunk
        SysCalls::Write(buffer, start, end, STDOUT_FD);
        SysCalls::Write(buffer, start, end, fd);
        start = end + 1;
    } while (bytesRead > 0);
}

int main(int argc, char* argv[]) {
    auto parsedArgs = ParseArgs(argc, argv);

    bool append = parsedArgs.count("append");
    std::string filepath = parsedArgs["filepath"].as<std::string>();

    try {
        int fd = SysCalls::Open(filepath, append);
        Tee(fd);
        SysCalls::Close(fd);
    } catch (const std::exception& e) {
        std::cerr << "Caught an exception, unable to continue. What(): \"" << e.what() << "\"";
        return -1;
    }

    return 0;
}

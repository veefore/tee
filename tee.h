#include <sys/types.h> // ssize_t
#include <vector>
#include <string>
#include "cxxopts.hpp"


cxxopts::ParseResult ParseArgs(int argc, char* argv[]);

class SysCalls {
public:

static int Open(const std::string filepath, bool append);

static int Close(int fd);

static ssize_t Read(std::vector<char>& buffer, int fd);

static ssize_t Write(std::vector<char>& buffer, size_t start, size_t end, int fd);

};

void Tee(int fd);

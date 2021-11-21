# tee
Function similar to Linux's tee written in C++ with syscalls.
// Reads from stdin and writes to stdout and a specified file.

Built with `g++ tee.cpp -o tee`  
To call from command line: `./tee [-a/--append] filepath`

#pragma once
#include <ostream>
#include <string>
#include <vector>
#include <sstream>

class Process {
    public:
    enum class Result{
        Success,
        Failed
    };

    public:
    static Result Run(const char* cmd);
    static Result Run(const char* cmd, std::ostream& output);
    static Result Run(std::string cmd) { return Run(cmd.c_str()); }
    static Result Run(std::string cmd, std::ostream& output) { return Run(cmd.c_str(), output); }
};


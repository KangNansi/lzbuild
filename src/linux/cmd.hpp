#pragma once
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
};

void SetEnv(std::string variable, std::string value);
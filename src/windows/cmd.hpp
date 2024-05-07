#include "windows.h"
#include <string>
#include <vector>
#include <sstream>

class Process {
    public:
    enum class Result{
        Success,
        Failed
    };
    private:
    
    struct Pipe{
        HANDLE read;
        HANDLE write;
    };

    public:
    static Result Run(const char* cmd);
    //static Result Run(const char* cmd, std::stringstream& output);
    static Result Run(const char* cmd, std::ostream& output);

    private:
    static PROCESS_INFORMATION create(const char* cmd, HANDLE in, HANDLE out, HANDLE err);
    static Pipe create_pipe();
    static Result get_result(PROCESS_INFORMATION info);
    static void release(PROCESS_INFORMATION info);
};

void SetEnv(std::string variable, std::string value);
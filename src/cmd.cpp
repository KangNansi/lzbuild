#ifdef _WIN32
#include "Windows.h"
#include "cmd.hpp"
#include <iostream>

using namespace std;

std::string get_error(){
    auto error_code = GetLastError();
    LPSTR error_message = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&error_message,
        0,
        NULL
    );
    std::string message(error_message, size);

    LocalFree(error_message);
    return message;
}

void run(const char *cmd){
    STARTUPINFO startup;
    ZeroMemory(&startup, sizeof(startup));
    startup.cb = sizeof(startup);

    auto outHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    auto errHandle = GetStdHandle(STD_ERROR_HANDLE);

    startup.hStdOutput = outHandle;
    startup.hStdError = errHandle;

    PROCESS_INFORMATION process_info;
    ZeroMemory(&process_info, sizeof(process_info));
    auto lpstr = const_cast<LPSTR>(cmd);
    
    cout << "Running cmd " << cmd << endl;
    auto res = CreateProcessA(NULL, lpstr, NULL, NULL, false, NORMAL_PRIORITY_CLASS, NULL, NULL, &startup, &process_info);
    if(res == 0){
        cerr << "Could not create process: " << get_error() << endl;
        return;
    }

    WaitForSingleObject(process_info.hProcess, 100000);

    DWORD exit_status;
    GetExitCodeProcess(process_info.hProcess, &exit_status);
    if(exit_status != 0){
        cerr << "Process error: " << exit_status << endl;
    }

    cout << "cmd exited" << endl;
}

#endif
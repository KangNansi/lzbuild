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

Process::Result Process::Run(const char* cmd)
{
    HANDLE std_out = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE std_in = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE std_err = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION info = create(cmd, std_in, std_out, std_err);
    WaitForSingleObject(info.hProcess, INFINITE);
    auto result = get_result(info);
    release(info);
    return result;
}

// Process::Result Process::Run(const char* cmd, std::stringstream& output) {
//     Pipe pipe = create_pipe();
//     HANDLE std_in = GetStdHandle(STD_INPUT_HANDLE);

//     auto info = create(cmd, std_in, pipe.write, pipe.write);

//     CloseHandle(pipe.write);
//     DWORD read_chars;
//     CHAR rd_buffer[256];
//     while(ReadFile(pipe.read, rd_buffer, 256, &read_chars, NULL) && read_chars > 0){
//         output.write(rd_buffer, read_chars);
//     }
//     CloseHandle(pipe.read);
//     WaitForSingleObject(info.hProcess, INFINITE);
//     auto result = get_result(info);
//     release(info);
//     return result;
// }

Process::Result Process::Run(const char* cmd, std::ostream& output)
{
    Pipe pipe = create_pipe();
    HANDLE std_in = GetStdHandle(STD_INPUT_HANDLE);

    auto info = create(cmd, std_in, pipe.write, pipe.write);

    CloseHandle(pipe.write);
    DWORD read_chars;
    CHAR rd_buffer[256];
    while(ReadFile(pipe.read, rd_buffer, 256, &read_chars, NULL) && read_chars > 0){
        output.write(rd_buffer, read_chars);
    }
    CloseHandle(pipe.read);
    WaitForSingleObject(info.hProcess, INFINITE);
    auto result = get_result(info);
    release(info);
    return result;
}

PROCESS_INFORMATION Process::create(const char* cmd, HANDLE in, HANDLE out, HANDLE err)
{
    STARTUPINFO startup;
    ZeroMemory(&startup, sizeof(STARTUPINFO));
    startup.cb = sizeof(STARTUPINFO);
    startup.hStdOutput = out;
    startup.hStdError = err;
    startup.hStdInput = in;
    startup.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION process_info;
    ZeroMemory(&process_info, sizeof(PROCESS_INFORMATION));
    auto lpstr = const_cast<LPSTR>(cmd); 
    
    auto res = CreateProcess(NULL, lpstr, NULL, NULL, TRUE, 0, NULL, NULL, &startup, &process_info);
    if(res == 0){
        throw "CreateProcess:" + get_error();
    }

    return process_info;
}

Process::Result Process::get_result(PROCESS_INFORMATION info){
    DWORD exit_status;
    if(!GetExitCodeProcess(info.hProcess, &exit_status)){
        throw get_error();
    }
    if(exit_status == STILL_ACTIVE){
        throw "Process did not finish";
    }
    return exit_status != 0 ? Process::Result::Failed : Process::Result::Success;
}

void Process::release(PROCESS_INFORMATION info){
    CloseHandle(info.hProcess);
    CloseHandle(info.hThread);
}

Process::Pipe Process::create_pipe(){
    Process::Pipe pipe;
    SECURITY_ATTRIBUTES sa;
    ZeroMemory(&sa, sizeof(sa)); 
    sa.bInheritHandle = TRUE;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;

    if(!CreatePipe(&pipe.read, &pipe.write, &sa, 0)){
        throw get_error();
    }
    if (!SetHandleInformation(pipe.read, HANDLE_FLAG_INHERIT, 0)){
        throw get_error();
    }

    return pipe;
}

void SetEnv(std::string variable, std::string value){
    SetEnvironmentVariable(const_cast<LPCSTR>(variable.c_str()), const_cast<LPCSTR>(value.c_str()));
}

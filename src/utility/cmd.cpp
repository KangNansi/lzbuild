#include "cmd.hpp"
#ifdef __unix__
#include "sys/types.h"
#include "unistd.h"
#include "stdio.h"
#include <sys/wait.h>
#include <iostream>
#include <string>
#include <string.h>
#elif _WIN32
#include "windows.h"
#endif

std::vector<std::string> parse(const char* cmd)
{
    std::vector<std::string> args;
    std::string current;
    for (int i = 0; cmd[i] != '\0'; i++)
    {
        if (cmd[i] == ' ' || cmd[i] == '"')
        {
            if (current.length() > 0)
            {
                args.push_back(current);
                current.clear();
            }
        }
        else
        {
            current += cmd[i];
        }
    }
    if (current.length() > 0)
    {
        args.push_back(current);
    }
    return args;
}

Process::Result Process::Run(const char* cmd)
{
    return Run(cmd, std::cout);
}

Process::Result Process::Run(const char* cmd, std::ostream& output)
{
    #ifdef __unix__
        int out[2];
        pipe(out);
        int pid = fork();
        if (pid == 0)
        {
            auto args = parse(cmd);
            close(out[0]);
            dup2(out[1], STDOUT_FILENO);
            dup2(out[1], STDERR_FILENO);
            close(out[1]);
            std::vector<char*> fargs;
            fargs.reserve(args.size() + 1);
            for (size_t i = 0; i < args.size() + 1; i++)
            {
                if (i < args.size())
                {
                    fargs.push_back((char*)args[i].c_str());
                }
                else
                {
                    fargs.push_back(nullptr);
                }
            }
            int result = execvp(fargs[0], fargs.data());
            if (result < 0)
            {
                std::cerr << "Error:" << strerror(errno) << std::endl;
            }
            exit(result);
        }
        close(out[1]);
        int status = 0;
        char buf[33];
        while (waitpid(pid, &status, WNOHANG) >= 0)
        {
            while (true)
            {
                int n = read(out[0], buf, 32);
                if (n <= 0)
                {
                    break;
                }
                buf[n] = '\0';
                output << buf;
            }
        }
        close(out[0]);
        return status == 0 ? Process::Result::Success : Process::Result::Failed;
    #elif _WIN32
        HANDLE hPipeRead, hPipeWrite;
        SECURITY_ATTRIBUTES saAttr = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
        if (!CreatePipe(&hPipeRead, &hPipeWrite, &saAttr, 0))
        {
            return Process::Result::Failed;
        }
        if (!SetHandleInformation(hPipeRead, HANDLE_FLAG_INHERIT, 0))
        {
            CloseHandle(hPipeRead);
            CloseHandle(hPipeWrite);
            return Process::Result::Failed;
        }
        PROCESS_INFORMATION pi;
        STARTUPINFO si = {sizeof(STARTUPINFO)};
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = hPipeWrite;
        si.hStdError = hPipeWrite;
        std::string cmdStr = cmd;
        if (!CreateProcess(NULL, &cmdStr[0], NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
        {
            CloseHandle(hPipeRead);
            CloseHandle(hPipeWrite);
            return Process::Result::Failed;
        }
        CloseHandle(hPipeWrite);
        char buf[33];
        DWORD bytesRead;
        while (true)
        {
            if (!ReadFile(hPipeRead, buf, 32, &bytesRead, NULL) || bytesRead == 0)
            {
                break;
            }
            buf[bytesRead] = '\0';
            output << buf;
        }
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hPipeRead);
        return exitCode == 0 ? Process::Result::Success : Process::Result::Failed;
    #endif
}

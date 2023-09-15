#ifdef __linux__
#include "cmd.hpp"
#include "sys/types.h"
#include "unistd.h"
#include "stdio.h"
#include <sys/wait.h>
#include <iostream>
#include <string>
#include <string.h>

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
    int out[2];
    pipe(out);
    int pid = fork();
    if (pid == 0)
    {
        auto args = parse(cmd);
        close(out[0]);
        dup2(out[1], STDOUT_FILENO);
        dup2(out[1], STDERR_FILENO);
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
        close(out[1]);
        exit(result);
    }
    close(out[1]);
    int status;
    char buf[33];
    wait(&status);
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
    close(out[0]);
    return status == 0 ? Process::Result::Success : Process::Result::Failed;
}
#endif
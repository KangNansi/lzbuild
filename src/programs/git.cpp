#include "git.hpp"
#include <sstream>

Process::Result git_clone(std::filesystem::path target, std::string repository, std::ostream& output)
{
    std::stringstream command;
    command << "git clone " << repository << " " << target;
    return Process::Run(command.str().c_str(), output);
}

Process::Result git_update(std::filesystem::path target, std::ostream& output)
{
    std::stringstream fetch_command;
    fetch_command << "git -C " << target << " fetch";
    if (Process::Run(fetch_command.str().c_str(), output) == Process::Result::Failed)
    {
        return Process::Result::Failed;
    }
    std::stringstream pull_command;
    pull_command << "git -C " << target << " pull";
    return Process::Run(pull_command.str().c_str(), output);
}
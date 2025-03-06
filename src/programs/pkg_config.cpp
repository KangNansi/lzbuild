#include "pkg_config.hpp"
#include "../utility/cmd.hpp"
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <string>
#include <vector>

using namespace pkg_config;

std::vector<std::string> parse_arguments(std::stringstream& str)
{
    std::vector<std::string> result;
    std::string arg;
    while(str >> arg)
    {
        result.push_back(arg);
    }
    return result;
}

enum class cmd_type
{
    cflags,
    libs
};

Process::Result run_cmd(std::string name, cmd_type type, std::ostream& output)
{
    const std::string binary = "pkg-config";
    std::string cmd = binary;
    switch (type) {
    case cmd_type::cflags: cmd += " --cflags "; break;
    case cmd_type::libs: cmd += " --libs "; break;
    }
    return Process::Run(cmd + name, output);
}


library_config pkg_config::get_config(std::string name)
{
    library_config config;
    std::stringstream output;
    if(run_cmd(name, cmd_type::cflags, output) == Process::Result::Failed)
    {
        output = std::stringstream();
        name = "lib" + name;
        if(run_cmd(name, cmd_type::cflags, output) == Process::Result::Failed)
        {
            config.lib_flags.push_back("-l" + name);
            return config;
        }
    }
    config.cflags = parse_arguments(output);
    output = std::stringstream();
    if(run_cmd(name, cmd_type::libs, output) == Process::Result::Failed)
    {
        return config;
    }
    config.lib_flags = parse_arguments(output);

    return config;
}
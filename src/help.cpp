#include "help.hpp"
#include <iostream>
#include <algorithm>

std::vector<CommandHelp> get_all_commands() {
    return {
        {
            "init",
            "Initialize a new C++ project in the current directory",
            "init",
            {}
        },
        {
            "install", 
            "Install a package or dependency",
            "install [repository]",
            {
                {"repository", "Git repository URL or package name (optional)"}
            }
        },
        {
            "export",
            "Export the project build artifacts", 
            "export",
            {}
        },
        {
            "build",
            "Build the C++ project (default command)",
            "build [options]",
            {
                {"-v", "Verbose output"},
                {"-fr", "Full rebuild"},
                {"-fl", "Force linking"},
                {"-g, --debug", "Debug build"},
                {"--output-command", "Show build commands"},
                {"-sw, --show-warning", "Show warnings"},
                {"--print-dependencies", "Print dependency tree"},
                {"-c <config>", "Specify config file (default: default.lzb)"},
                {"--export-dir <dir>", "Export directory"}
            }
        },
        {
            "script",
            "Generate build script to output file",
            "script <output_file>",
            {
                {"output_file", "File to write build script to"}
            }
        },
        {
            "compile_commands",
            "Generate compile_commands.json for IDEs",
            "compile_commands [target_folder]",
            {
                {"target_folder", "Target folder for compile_commands.json (optional)"}
            }
        }
    };
}

void show_general_help(const std::string& program_name) {
    std::cout << "Usage: " << program_name << " [command] [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "A C++ project builder and package manager." << std::endl;
    std::cout << std::endl;
    std::cout << "Available commands:" << std::endl;
    
    auto commands = get_all_commands();
    size_t max_name_length = 0;
    for (const auto& cmd : commands) {
        max_name_length = std::max(max_name_length, cmd.name.length());
    }
    
    for (const auto& cmd : commands) {
        std::cout << "  " << cmd.name;
        for (size_t i = cmd.name.length(); i < max_name_length; ++i) {
            std::cout << " ";
        }
        std::cout << "   " << cmd.description << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "Use \"" << program_name << " help <command>\" for more information on a specific command." << std::endl;
    std::cout << "Use \"" << program_name << " --help\" or \"" << program_name << " -h\" to show this help message." << std::endl;
}

void show_command_help(const std::string& command, const std::string& program_name) {
    auto commands = get_all_commands();
    auto it = std::find_if(commands.begin(), commands.end(), 
                          [&command](const CommandHelp& cmd) { 
                              return cmd.name == command; 
                          });
    
    if (it == commands.end()) {
        std::cout << "Unknown command: " << command << std::endl;
        std::cout << "Use \"" << program_name << " help\" to see available commands." << std::endl;
        return;
    }
    
    const auto& cmd = *it;
    std::cout << "Usage: " << program_name << " " << cmd.usage << std::endl;
    std::cout << std::endl;
    std::cout << cmd.description << std::endl;
    
    if (!cmd.options.empty()) {
        std::cout << std::endl;
        std::cout << "Options:" << std::endl;
        
        size_t max_option_length = 0;
        for (const auto& option : cmd.options) {
            max_option_length = std::max(max_option_length, option.first.length());
        }
        
        for (const auto& option : cmd.options) {
            std::cout << "  " << option.first;
            for (size_t i = option.first.length(); i < max_option_length; ++i) {
                std::cout << " ";
            }
            std::cout << "   " << option.second << std::endl;
        }
    }
    
    if (cmd.name == "build") {
        std::cout << std::endl;
        std::cout << "Note: build is the default command when no command is specified." << std::endl;
    }
}
#pragma once
#include <string>
#include <vector>

struct CommandHelp {
    std::string name;
    std::string description;
    std::string usage;
    std::vector<std::pair<std::string, std::string>> options;
};

void show_general_help(const std::string& program_name);
void show_command_help(const std::string& command, const std::string& program_name);
std::vector<CommandHelp> get_all_commands();
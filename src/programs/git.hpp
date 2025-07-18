#pragma once
#include <filesystem>
#include <string>
#include <iostream>
#include "../utility/cmd.hpp"

Process::Result git_clone(std::filesystem::path target, std::string repository, std::ostream& output);
Process::Result git_update(std::filesystem::path target, std::ostream& output);
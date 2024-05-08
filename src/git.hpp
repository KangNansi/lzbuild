#pragma once
#include <filesystem>
#include <string>
#include <iostream>
#ifdef _WIN32
 #include "windows/cmd.hpp"
#else
 #include "linux/cmd.hpp"
#endif

Process::Result git_clone(std::filesystem::path target, std::string repository, std::ostream& output);
Process::Result git_update(std::filesystem::path target, std::ostream& output);
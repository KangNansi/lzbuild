#pragma once
#include "utility/args.hpp"
#include <filesystem>

void init(std::filesystem::path path);
bool install(std::filesystem::path repository, std::filesystem::path self_cmd);
bool export_project();
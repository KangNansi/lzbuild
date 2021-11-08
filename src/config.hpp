#pragma once
#include <string>
#include <filesystem>

struct config{
    std::filesystem::path export_path;
};

void read_config(config& config, std::filesystem::path path);
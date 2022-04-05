#pragma once
#include <string>
#include <filesystem>
#include <vector>

struct config{
    std::filesystem::path export_path;
    std::string name;
    std::string cflags;
    std::vector<std::string> include_folder;
    std::vector<std::string> libraries;
    std::vector<std::string> library_paths;
    std::string link_etc;
};

void read_config(config& config, std::filesystem::path path);
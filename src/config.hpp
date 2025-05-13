#pragma once
#include "programs/pkg_config.hpp"
#include <optional>
#include <string>
#include <filesystem>
#include <vector>
#include <iostream>

#ifdef _WIN32
    const auto BIN_EXT = ".exe";
    const auto LIB_EXT = ".lib";
#else
    const auto BIN_EXT = "";
    const auto LIB_EXT = ".a";
#endif

enum class library_link_type
{
    _static,
    shared
};

struct library_dependency
{
    std::string name;
    pkg_config::library_config config;
};

struct config{
    std::string name = "result";
    std::vector<std::string> cflags;
    std::string compiler = "g++";
    std::string standard = "c++20";
    std::vector<std::string> include_folder;
    std::vector<library_dependency> libraries;
    std::vector<std::string> library_paths;
    std::vector<std::string> source_folders;
    std::vector<std::string> exclude;
    std::vector<std::string> link_etc;
    std::vector<std::string> macros;
    std::optional<std::string> asset_folder;
    bool is_library = false;
    size_t num_thread = 32;

    std::filesystem::path get_binary_path() const
    {
        if (is_library)
        {
            return std::filesystem::path("bin") / ("lib" + name + LIB_EXT);
        }
        return std::filesystem::path("bin") / (name + BIN_EXT);
    }

    bool is_excluded(std::filesystem::path path) const
    {
        for (size_t i = 0; i < exclude.size(); i++)
        {
            auto exclude_path = std::filesystem::path(exclude[i]);
            if (std::filesystem::equivalent(path, exclude_path))
            {
                return true;
            }
        }
        return false;
    }

    void print(std::ostream& stream)
    {
        stream << "is_library: " << is_library << std::endl;
        stream << "name: " << name << std::endl;
    }
};

void read_config(config& config, std::filesystem::path path);
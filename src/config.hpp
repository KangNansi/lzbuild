#pragma once
#include <string>
#include <filesystem>
#include <vector>
#include <iostream>

struct config{
    std::filesystem::path executable_export_path;
    std::filesystem::path include_export_path;
    std::string name;
    std::string cflags;
    std::string compiler;
    std::string standard;
    std::string output_extension;
    std::vector<std::string> include_folder;
    std::vector<std::string> libraries;
    std::vector<std::string> library_paths;
    std::vector<std::string> source_folders;
    std::vector<std::string> exclude;
    std::string link_etc;
    bool is_library = false;
    std::vector<std::string> dependencies;
    size_t num_thread = 32;

    std::filesystem::path get_binary_dir() const{
        return "bin";
    }
    std::filesystem::path get_binary_path() const
    {
        if (is_library)
        {
            return get_binary_dir() / ("lib" + name + output_extension);
        }
        return get_binary_dir() / (name + output_extension);
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
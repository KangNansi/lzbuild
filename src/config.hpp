#pragma once
#include <string>
#include <filesystem>
#include <vector>
#include <iostream>

struct config{
    std::filesystem::path export_path;
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

    std::filesystem::path get_binary_dir() const{
        return "bin";
    }
    std::filesystem::path get_binary_path() const{
        return get_binary_dir() / (name + output_extension);
    }
    bool is_excluded(std::filesystem::path path) const
    {
        for (int i = 0; i < exclude.size(); i++)
        {
            std::cout << std::filesystem::relative(path, exclude[i]) << std::endl;
            if (!std::filesystem::relative(path, exclude[i]).empty())
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
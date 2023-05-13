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
    std::vector<std::string> source_folders;
    std::string link_etc;
    bool is_library = false;

    std::filesystem::path get_binary_dir() const{
        return "bin";
    }
    std::filesystem::path get_binary_path() const{
        if(is_library){
            return get_binary_dir() / (name + ".lib");
        }
        else{
            return get_binary_dir() / (name + ".exe");
        }
    }
    void print(std::ostream& stream) {
        stream << "is_library: " << is_library << std::endl;
        stream << "name: " << name << std::endl;
    }
};

void read_config(config& config, std::filesystem::path path);
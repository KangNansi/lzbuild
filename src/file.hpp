#pragma once
#include <filesystem>
#include <vector> 

namespace fs = std::filesystem;

enum class FILE_TYPE{
    SOURCE,
    HEADER
};

class file{
    fs::path path;
    fs::path object_path;
    FILE_TYPE type;
    std::vector<fs::path> dependencies;
    fs::file_time_type last_write;


    bool read_include(const std::string& line, std::string& path);
    void compute_dependencies();

    public:
    file(fs::path path);

    bool need_rebuild() const;
    FILE_TYPE get_type() const { return type; }
    fs::path get_file_path() const { return path; }
    fs::path get_object_path() const { return object_path; }

    std::string get_last_write_time_string() const;
    friend std::ostream& operator<<(std::ostream &strm, const file &file);
};

std::ostream& operator<<(std::ostream &strm, const file &file);

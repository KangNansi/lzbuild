#pragma once
#include <filesystem>
#include <vector> 
#include <set>

namespace fs = std::filesystem;

enum class FILE_TYPE{
    SOURCE,
    HEADER
};

class file{
    fs::path path;
    fs::path object_path;
    fs::path workspace_folder;
    fs::path source_path;
    FILE_TYPE type;
    std::vector<fs::path> dependencies;
    fs::file_time_type last_write;


    bool read_include(const std::string& line, std::string& path);
    void compute_dependencies();

    public:
    file(fs::path path, fs::path workspace_folder, fs::path source_path);

    bool need_rebuild(const std::vector<file>& files) const;
    bool rebuild_check(std::filesystem::file_time_type last_write, const std::vector<file>& files, std::set<fs::path>& checked) const;
    FILE_TYPE get_type() const { return type; }
    fs::path get_file_path() const
    {
        auto relative = fs::relative(path, fs::current_path());
        return relative.empty() ? path : relative;
    }
    fs::path get_object_path() const
    {
        auto relative = fs::relative(object_path, fs::current_path());
        return relative.empty() ? object_path : relative;
    }
    fs::path get_source_path() const { return source_path; }

    std::string get_last_write_time_string() const;
    friend std::ostream& operator<<(std::ostream &strm, const file &file);
};

std::ostream& operator<<(std::ostream &strm, const file &file);

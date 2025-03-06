#pragma once
#include <filesystem>
#include <vector>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include "utility/args.hpp"
#include "file.hpp"
#include "config.hpp"
#include "dependency_tree.hpp"
#include "utility/cmd.hpp"

struct build_options
{
    bool verbose = false;
    bool full_rebuild = false;
    bool force_linking = false;
    bool debug = true;
    bool output_command = false;
    bool show_warning = false;
    bool print_dependencies = false;
    std::string config = "default.lzb";
    std::filesystem::path root_directory = std::filesystem::current_path();
    std::optional<std::filesystem::path> export_directory;

    build_options(){}
    build_options(const ArgReader& args);
};

enum class BuildStatus
{
    NoChange,
    Failed,
    Changed
};

class project
{
private:
    build_options _options;
    config _config;
    std::vector<file> _files;
    bool _header_only = true;
    std::ostream& _output = std::cout;
    dependency_tree _dep_tree;
    std::filesystem::path _obj_root;

public:
    project(const ArgReader& args);
    project(const build_options& options, std::ostream& output = std::cout);

    Process::Result build();
    void export_binary(std::filesystem::path target);
    std::string get_name() { return _config.name; }
    bool is_library() { return _config.is_library; }
    bool is_header_only() { return _header_only; }
    void export_header_files(std::filesystem::path target);
    void build_file_registry();

private:
    BuildStatus compile_project_async(fs::file_time_type& last_write);
    Process::Result compile_object(const file& file, std::stringstream& output);
    bool binary_requires_rebuild(fs::file_time_type last_write);
    Process::Result link(std::stringstream& output);
    Process::Result link_library(std::stringstream& output);
    std::filesystem::path get_pretty_path(std::filesystem::path path);
    std::filesystem::path get_object_path(const file& file);
};
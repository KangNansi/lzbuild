#pragma once
#include <vector>
#include <sstream>
#include <iostream>
#include "args.hpp"
#include "file.hpp"
#include "config.hpp"
#include "dependency_tree.hpp"
#ifdef _WIN32
 #include "windows/cmd.hpp"
#else
 #include "linux/cmd.hpp"
#endif

struct cppmaker_options
{
    bool verbose = false;
    bool dependencies_only = false;
    bool full_rebuild = false;
    bool force_linking = false;
    bool debug = true;
    bool output_command = false;
    bool show_warning = false;
    bool print_dependencies = false;
    std::string config = "cppmaker.cfg";
    std::filesystem::path root_directory = std::filesystem::current_path();

    cppmaker_options(){}
    cppmaker_options(const ArgReader& args);
};

enum class BuildStatus
{
    NoChange,
    Failed,
    Changed
};

class CPPMaker
{
private:
    cppmaker_options _options;
    config _config;
    std::vector<file> _files;
    std::ostream& _output = std::cout;
    dependency_tree _dep_tree;

public:
    CPPMaker(const ArgReader& args);
    CPPMaker(const cppmaker_options& options, std::ostream& output = std::cout);

    Process::Result build();
    void export_binary();
    void export_binary(std::filesystem::path target);
    void run();
    std::string get_name() { return _config.name; }

private:
    void build_file_registry();
    void export_header_files();
    void export_header_files(std::filesystem::path target);
    Process::Result handle_dependencies();
    BuildStatus compile_project_async(fs::file_time_type& last_write);
    Process::Result compile_object(const file& file, std::stringstream& output);
    bool binary_requires_rebuild(fs::file_time_type last_write);
    Process::Result link(std::stringstream& output);
    Process::Result link_library(std::stringstream& output);
    std::filesystem::path get_pretty_path(std::filesystem::path path);
    void export_dependency(const CPPMaker& target);
};
#pragma once
#include <vector>
#include <sstream>
#include "args.hpp"
#include "file.hpp"
#include "config.hpp"
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
    std::string config = "cppmaker.cfg";

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

public:
    CPPMaker(const ArgReader& args);
    CPPMaker(const cppmaker_options& options);

    Process::Result build();
    void export_binary();
    void run();

private:
    void build_file_registry();
    void export_header_files();
    Process::Result compile_object(const file& file, std::stringstream& output);
    bool binary_requires_rebuild(fs::file_time_type last_write);
    Process::Result link(std::stringstream& output);
    Process::Result link_library(std::stringstream& output);
};
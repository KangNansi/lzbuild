#include "cppmaker.hpp"
#include <filesystem>
#include <iostream>
#include "term.hpp"

namespace fs = std::filesystem;

cppmaker_options::cppmaker_options(const ArgReader& args)
{
    verbose = args.has("-v");
    dependencies_only = args.has("-d");
    full_rebuild = args.has("-fr");
    force_linking = args.has("-fl");
    std::string arg_value;
    if (args.get("-c", arg_value))
    {
        if(!fs::exists(arg_value)){ 
            std::cerr << "Could not find config " << arg_value << std::endl;
            throw "Could not find config " + arg_value;
        }
        config = arg_value;
    }
}

CPPMaker::CPPMaker(const ArgReader& args) : _options(args)
{
    read_config(_config, _options.config);
    build_file_registry();
}

CPPMaker::CPPMaker(const cppmaker_options& options) : _options(options)
{
    read_config(_config, _options.config);
    build_file_registry();
}

Process::Result CPPMaker::build()
{
    if(_options.dependencies_only){
        return Process::Result::Success;
    }

    BuildStatus status = BuildStatus::NoChange;
    fs::file_time_type last_write = fs::file_time_type::min();
    for(auto& f: _files){
        if(f.get_type() == FILE_TYPE::SOURCE){
            bool should_rebuild = _options.full_rebuild || f.need_rebuild(_files);
            if(should_rebuild){
                std::cout << term::cyan << "Rebuilding " << f.get_file_path() << ": " << term::reset << std::flush;
                status = BuildStatus::Changed;
                std::stringstream build_output;
                if(compile_object(f, build_output) == Process::Result::Failed){
                    status = BuildStatus::Failed;
                    std::cout << term::red << "Failed " << term::reset << std::endl;
                    std::cout << build_output.str() << std::flush;
                    break;
                }
                else
                {
                    std::cout << build_output.str() << std::flush;
                    std::cout << term::green << "Rebuilt" << term::reset << std::endl;
                }
            }
            else{
                //cout << "Already done" << endl;
            }
            if (fs::exists(f.get_object_path()))
            {
                auto file_last_write = fs::last_write_time(f.get_object_path());
                if(file_last_write > last_write){
                    last_write = file_last_write;
                }
            }
        }
    }

    if (status == BuildStatus::Failed)
    {
        std::cout << term::red << "Build failed" << term::reset << std::endl;
        return Process::Result::Failed;
    }

    if (!binary_requires_rebuild(last_write))
    {
        std::cout << "Binary is up to date." << std::endl;
        return Process::Result::Success;
    }

    if(!_config.is_library){
        std::cout << "Creating executable..." << std::endl;
        std::stringstream link_output;
        if(link(link_output) == Process::Result::Failed){
            std::cerr << term::red << "Error creating executable" << term::reset << std::endl;
            std::cout << link_output.str() << std::flush;
            return Process::Result::Failed;
        }
    }
    else{
        std::cout << "Creating library..." << std::endl;
        std::stringstream lib_output;
        if(link_library(lib_output) == Process::Result::Failed){
            std::cerr << term::red << "Error creating library" << term::reset << std::endl;
            std::cout << lib_output.str() << std::flush;
            return Process::Result::Failed;
        }
    }

    return Process::Result::Success;
}

void CPPMaker::export_binary()
{
    if (!_config.executable_export_path.empty())
    {
        std::cerr << term::red << "No binary export path specified" << term::reset << std::endl;
    }
    auto binary_path = _config.get_binary_path();
    fs::create_directories(_config.executable_export_path);
    fs::path executable = _config.executable_export_path / binary_path.filename();
    try
    {
        if (fs::exists(executable))
        {
            fs::remove(executable);
        }
        fs::copy(binary_path, executable);
        std::cout << term::green << "Exported " << binary_path << " to " << executable << term::green << std::endl;
    }
    catch (fs::filesystem_error& error)
    {
        std::cerr << term::red << "Could not export executable: " << error.what() << term::reset << std::endl;
    }

    if (_config.is_library)
    {
        export_header_files();
    }
}

void CPPMaker::run()
{
    std::cout << "Running executable" << std::endl;

    try
    {
        Process::Run(_config.get_binary_path().string().c_str(), std::cout);
    }
    catch(const std::string& error){
        std::cerr << term::red << error << term::reset << std::endl;
    }
    catch(const char* error){
        std::cerr << term::red << error << term::reset << std::endl;
    }
}

void CPPMaker::build_file_registry()
{
    _files.clear();
    fs::path obj_dir("obj");
    if(!fs::exists(obj_dir)){
        fs::create_directory(obj_dir);
    }
    fs::path bin_dir("bin");
    if(!fs::exists(bin_dir)){
        fs::create_directory(bin_dir);
    }

    for (auto& src_folder : _config.source_folders)
    {
        auto it = fs::recursive_directory_iterator(src_folder);
        for(decltype(it) end; it != end; ++it)
        {
            if (_config.is_excluded(*it))
            {
                it.disable_recursion_pending();
                continue;
            }
            if (fs::is_directory(*it))
            {
                continue;
            }
            
            auto relative_path = fs::relative(*it, src_folder);
            _files.push_back(file(*it, src_folder));
            if(_options.verbose){
                std::cout << _files.back() << std::endl;
            }
        }
    }   
}

void CPPMaker::export_header_files()
{
    if (_config.include_export_path.empty())
    {
        std::cerr << term::red << "No include export path given" << term::reset << std::endl;
    }
    else
    {
        fs::create_directories(_config.include_export_path);
        for (auto& file : _files)
        {
            if (file.get_type() == FILE_TYPE::HEADER)
            {
                auto path = file.get_file_path().relative_path();
                path = fs::relative(path, *(path.begin()));
                auto dest_path = _config.include_export_path / path;
                if(fs::exists(dest_path)){
                    fs::remove(dest_path);
                }
                fs::create_directories(dest_path.parent_path());
                fs::copy(file.get_file_path(), dest_path);
                std::cout << term::green << "Exported " << file.get_file_path() << " to " << dest_path << term::reset << std::endl;
            }
        }
    }
}

Process::Result CPPMaker::compile_object(const file& file, std::stringstream& output)
{
    auto object_path = file.get_object_path();
    auto dir_path = object_path.remove_filename();
    fs::path directory;
    for(auto &dir : dir_path){
        directory /= dir;
        if(!fs::exists(directory)){
            fs::create_directory(directory);
        }
    }

    std::stringstream command;
    command << _config.compiler << " ";
    command << "-g "; // debug by default
    command << "-Wfatal-errors ";
    command << "-Wall -Wextra ";
    command << "-fdiagnostics-color=always ";
    command << "-std=" << _config.standard << " ";
    command << "-o " << file.get_object_path() << " -c " << file.get_file_path();

    // includes
    for (auto& include : _config.include_folder)
    {
        command << " -I" << include;
    }

    // cflags
    if (_config.cflags.length() > 0)
    {
        command << " " << _config.cflags;
    }

    return Process::Run(command.str().c_str(), output);
}

bool CPPMaker::binary_requires_rebuild(fs::file_time_type last_write)
{
    fs::path binary_path = _config.get_binary_path();
    if (fs::exists(binary_path))
    {
        if (fs::last_write_time(binary_path) < last_write)
        {
            return true;
        }

        fs::file_time_type library_last_write = fs::file_time_type::min();
        for (auto path : _config.library_paths)
        {
            for (auto lib : _config.libraries)
            {
                fs::path full_path = fs::path(path) / (lib + ".lib");
                if (fs::exists(full_path))
                {
                    auto file_last_write = fs::last_write_time(full_path);
                    if (file_last_write > library_last_write)
                    {
                        library_last_write = file_last_write;
                    }
                }
            }
        }

        if (fs::last_write_time(binary_path) < library_last_write)
        {
            return true;
        }
        return false;
    }
    return true;
}

Process::Result CPPMaker::link(std::stringstream& output)
{
    std::stringstream command;
    command << _config.compiler << " ";
    command << "-g "; // debug by default
    command << "-Wfatal-errors ";
    command << "-Wall -Wextra ";
    command << "-fdiagnostics-color=always ";
    command << "-std=" << _config.standard << " ";
    
    command << "-o " << _config.get_binary_path();
    for(auto& f: _files){
        if(f.get_type() == FILE_TYPE::SOURCE){
            command << " " << f.get_object_path();
        }
    }

    for(auto& libpath: _config.library_paths){
        command << " -L" << libpath;
    }

    for(auto& lib: _config.libraries){
        command << " -l" << lib;
    }

    if(_config.link_etc.length() > 0){
        command << " " << _config.link_etc;
    }

    std::cout << command.str() << std::endl;
    return Process::Run(command.str().c_str(), output);
}

Process::Result CPPMaker::link_library(std::stringstream& output)
{
    std::stringstream command;

    command << "ar rcs ";
    
    command << "-o " << _config.get_binary_path();
    
    for (auto& f : _files)
    {
        if(f.get_type() == FILE_TYPE::SOURCE){
            command << " " << f.get_object_path();
        }
    }

    return Process::Run(command.str().c_str(), output);
}
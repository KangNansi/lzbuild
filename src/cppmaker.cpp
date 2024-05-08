#include "cppmaker.hpp"
#include <filesystem>
#include <iostream>
#include <algorithm>
#include "term.hpp"
#include "git.hpp"

namespace fs = std::filesystem;

fs::path compute_path(fs::path root, fs::path target)
{
    return target.is_absolute() ? target : root / target;
}

cppmaker_options::cppmaker_options(const ArgReader& args)
{
    verbose = args.has("-v");
    dependencies_only = args.has("-d");
    full_rebuild = args.has("-fr");
    force_linking = args.has("-fl");
    debug = args.has("--debug") || args.has("-g");
    output_command = args.has("--output-command");
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
    read_config(_config, compute_path(_options.root_directory, _options.config));
    build_file_registry();
}

CPPMaker::CPPMaker(const cppmaker_options& options, std::ostream& output) : _options(options), _output(output)
{
    read_config(_config, compute_path(_options.root_directory, _options.config));
    build_file_registry();
}

Process::Result CPPMaker::build()
{
    if(_options.dependencies_only){
        return Process::Result::Success;
    }

    if (handle_dependencies() == Process::Result::Failed)
    {
        _output << term::red << "Failed to build dependencies" << term::reset << std::endl;
        return Process::Result::Failed;
    }

    BuildStatus status = BuildStatus::NoChange;
    fs::file_time_type last_write = fs::file_time_type::min();
    for(auto& f: _files){
        if(f.get_type() == FILE_TYPE::SOURCE){
            bool should_rebuild = _options.full_rebuild || f.need_rebuild(_files);
            if(should_rebuild){
                _output << term::cyan << "Rebuilding " << f.get_file_path() << ": " << term::reset << std::flush;
                status = BuildStatus::Changed;
                std::stringstream build_output;
                if(compile_object(f, build_output) == Process::Result::Failed){
                    status = BuildStatus::Failed;
                    _output << term::red << "Failed " << term::reset << std::endl;
                    _output << build_output.str() << std::flush;
                    break;
                }
                else
                {
                    _output << build_output.str() << std::flush;
                    _output << term::green << "Rebuilt" << term::reset << std::endl;
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
        _output << term::red << "Build failed" << term::reset << std::endl;
        return Process::Result::Failed;
    }

    if (!_options.force_linking && !binary_requires_rebuild(last_write))
    {
        _output << "Binary is up to date." << std::endl;
        return Process::Result::Success;
    }

    if(!_config.is_library){
        _output << "Creating executable..." << std::endl;
        std::stringstream link_output;
        if(link(link_output) == Process::Result::Failed){
            std::cerr << term::red << "Error creating executable" << term::reset << std::endl;
            _output << link_output.str() << std::flush;
            return Process::Result::Failed;
        }
    }
    else{
        _output << "Creating library..." << std::endl;
        std::stringstream lib_output;
        if(link_library(lib_output) == Process::Result::Failed){
            std::cerr << term::red << "Error creating library" << term::reset << std::endl;
            _output << lib_output.str() << std::flush;
            return Process::Result::Failed;
        }
    }

    return Process::Result::Success;
}

void CPPMaker::export_binary()
{
    if (_config.executable_export_path.empty())
    {
        std::cerr << term::red << "No binary export path specified" << term::reset << std::endl;
        return;
    }
    
    export_binary(_config.executable_export_path);

    if (_config.is_library)
    {
        export_header_files();
    }
}

void CPPMaker::export_binary(std::filesystem::path target)
{
    auto binary_path = compute_path(_options.root_directory, _config.get_binary_path());
    fs::create_directories(target);
    fs::path executable = target / binary_path.filename();
    try
    {
        if (fs::exists(executable))
        {
            fs::remove(executable);
        }
        fs::copy(binary_path, executable);
        _output << term::green << "Exported " << binary_path << " to " << executable << term::green << std::endl;
    }
    catch (fs::filesystem_error& error)
    {
        std::cerr << term::red << "Could not export executable: " << error.what() << term::reset << std::endl;
    }
}

void CPPMaker::run()
{
    _output << "Running executable" << std::endl;

    try
    {
        Process::Run(_config.get_binary_path().string().c_str(), _output);
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
    fs::path obj_dir(_options.root_directory / "obj");
    if(!fs::exists(obj_dir)){
        fs::create_directory(obj_dir);
    }
    fs::path bin_dir(_options.root_directory / "bin");
    if(!fs::exists(bin_dir)){
        fs::create_directory(bin_dir);
    }

    for (auto& src_folder : _config.source_folders)
    {
        auto root_folder = compute_path(_options.root_directory, src_folder);
        auto it = fs::recursive_directory_iterator(root_folder);
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
            
            _files.push_back(file(*it, _options.root_directory, fs::relative(*it, root_folder)));
            if(_options.verbose){
                _output << _files.back() << std::endl;
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
        auto export_path = compute_path(_options.root_directory, _config.include_export_path);
        export_header_files(export_path);
    }
}

void CPPMaker::export_header_files(std::filesystem::path target)
{
    fs::create_directories(target);
    for (auto& file : _files)
    {
        if (file.get_type() == FILE_TYPE::HEADER)
        {
            auto path = file.get_source_path();
            auto dest_path = target / _config.name / path;
            if(fs::exists(dest_path)){
                fs::remove(dest_path);
            }
            fs::create_directories(dest_path.parent_path());
            fs::copy(file.get_file_path(), dest_path);
            _output << term::green << "Exported " << file.get_file_path() << " to " << dest_path << term::reset << std::endl;
        }
    }
}

Process::Result CPPMaker::handle_dependencies()
{
    bool library_added = false;
    for (auto dependency : _config.dependencies)
    {
        fs::path dependency_path(dependency);
        if (dependency_path.extension().compare(".git") == 0)
        {
            auto repo_name = dependency_path.stem();
            auto target_path = compute_path(_options.root_directory, "dep") / repo_name;
            std::stringstream output;
            if (fs::exists(target_path) && fs::exists(target_path / ".git"))
            {
                _output << term::blue << repo_name.string() << ":" << term::reset << " updating git repository..." << std::endl;
                git_update(target_path, output);
            }
            else
            {
                _output << term::blue << repo_name.string() << ":" << term::reset << " cloning git repository" << std::endl;
                fs::create_directories(target_path);
                git_clone(target_path, dependency, output);
            }
            dependency_path = target_path;
        }

        cppmaker_options options;
        options.root_directory = dependency_path;
        options.debug = _options.debug;
        options.full_rebuild = _options.full_rebuild;
        std::stringstream ss;
        CPPMaker maker(options, ss);
        _output << term::blue << "Rebuilding " << maker.get_name() << ": " << term::reset;
        switch (maker.build())
        {
        case Process::Result::Success:
        {
            _output << term::green << "Rebuilt" << term::reset << std::endl;
            if (maker._config.is_library)
            {
                auto libdir = compute_path(_options.root_directory, "lib");
                fs::create_directories(libdir);
                maker.export_binary(libdir);
                auto includedir = compute_path(_options.root_directory, "include");
                fs::create_directories(includedir);
                maker.export_header_files(includedir);
                library_added = true;
                _config.libraries.push_back(maker.get_name());
            }
            else
            {
                auto bindir = compute_path(_options.root_directory, "bin");
                fs::create_directories(bindir);
                maker.export_binary(bindir);
            }
        }
            break;
        case Process::Result::Failed:
            _output << term::red << "Failed" << term::reset << std::endl;
            _output << ss.str() << std::endl;
            return Process::Result::Failed;
            break;
        }
    }

    if (library_added)
    {
        _config.library_paths.push_back(compute_path(_options.root_directory, "lib").string());
        _config.include_folder.push_back(compute_path(_options.root_directory, "include").string());
    }
    return Process::Result::Success;
}

Process::Result CPPMaker::compile_object(const file& file, std::stringstream& output)
{
    auto object_path = file.get_object_path();
    auto dir_path = object_path.remove_filename();
    fs::path directory;
    for(auto &dir : dir_path){
        directory /= dir;
        if (!fs::exists(directory))
        {
            std::error_code code;
            fs::create_directory(directory, code);
        }
    }

    std::stringstream command;
    command << _config.compiler << " ";
    if(_options.debug) command << "-g ";
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

    if (_options.output_command) _output << std::endl << command.str() << std::endl;
    return Process::Run(command.str().c_str(), output);
}

bool CPPMaker::binary_requires_rebuild(fs::file_time_type last_write)
{
    fs::path binary_path = compute_path(_options.root_directory, _config.get_binary_path());
    if (fs::exists(binary_path))
    {
        if (fs::last_write_time(binary_path) < last_write)
        {
            return true;
        }

        fs::file_time_type library_last_write = fs::file_time_type::min();
        for (auto path : _config.library_paths)
        {
            auto lib_path = compute_path(_options.root_directory, path);
            for (auto lib : _config.libraries)
            {
                fs::path full_path = fs::path(lib_path) / (lib + ".lib");
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
    if(_options.debug) command << "-g ";
    command << "-Wfatal-errors ";
    command << "-Wall -Wextra ";
    command << "-fdiagnostics-color=always ";
    command << "-std=" << _config.standard << " ";

    auto binary_path = compute_path(_options.root_directory, _config.get_binary_path());
    command << "-o " << binary_path;
    for(auto& f: _files){
        if(f.get_type() == FILE_TYPE::SOURCE){
            command << " " << f.get_object_path();
        }
    }

    for (auto& libpath : _config.library_paths)
    {
        auto path = compute_path(_options.root_directory, libpath);
        command << " -L" << path;
    }

    for(auto& lib: _config.libraries){
        command << " -l" << lib;
    }

    if(_config.link_etc.length() > 0){
        command << " " << _config.link_etc;
    }

    if(_options.output_command) _output << command.str() << std::endl;
    return Process::Run(command.str().c_str(), output);
}

Process::Result CPPMaker::link_library(std::stringstream& output)
{
    std::stringstream command;

    command << "ar rcs ";
    
    command << "-o " << compute_path(_options.root_directory, _config.get_binary_path());
    
    for (auto& f : _files)
    {
        if(f.get_type() == FILE_TYPE::SOURCE){
            command << " " << f.get_object_path();
        }
    }

    if (_options.output_command) _output << command.str() << std::endl;
    return Process::Run(command.str().c_str(), output);
}

std::filesystem::path CPPMaker::get_pretty_path(std::filesystem::path path)
{
    auto mismatch_pair = std::mismatch(path.begin(), path.end(), _options.root_directory.begin(), _options.root_directory.end());
    return mismatch_pair.second == _options.root_directory.end() ? fs::relative(path, _options.root_directory) : path;
}
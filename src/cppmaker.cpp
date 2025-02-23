#include "cppmaker.hpp"
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <queue>
#include <future>
#include <chrono>
#include <thread>
#include "config.hpp"
#include "linux/cmd.hpp"
#include "term.hpp"
#include "git.hpp"

namespace fs = std::filesystem;
using namespace std::chrono_literals;

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
    show_warning = args.has("--show-warning") || args.has("-sw");
    print_dependencies = args.has("--print-dependencies");
    update_git = args.has("-u") || args.has("--update-git");
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
    _obj_root = _options.root_directory / "obj" / fs::path(_options.config).stem();
}

CPPMaker::CPPMaker(const cppmaker_options& options, std::ostream& output) : _options(options), _output(output)
{
    read_config(_config, compute_path(_options.root_directory, _options.config));
    _obj_root = _options.root_directory / "obj" / fs::path(_options.config).stem();
}

Process::Result CPPMaker::build()
{
    if (handle_dependencies() == Process::Result::Failed)
    {
        _output << term::red << "Failed to build dependencies" << term::reset << std::endl;
        return Process::Result::Failed;
    }

    build_file_registry();

    if (_options.print_dependencies)
    {
        _dep_tree.print(_output);
    }

    if (_options.dependencies_only)
    {
        return Process::Result::Success;
    } 

    BuildStatus status = BuildStatus::NoChange;
    fs::file_time_type last_write = fs::file_time_type::min();

    if (_header_only)
    {
        _output << "Header only library ready" << std::endl;
        return Process::Result::Success;
    }

    status = compile_project_async(last_write);

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
    
    if(!_header_only)
    {
        export_binary(_config.executable_export_path);
    }

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
            if (fs::last_write_time(executable) > fs::last_write_time(binary_path))
            {
                return;
            }
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

    dependency_context ctx;
    for (auto& include_folder : _config.include_folder)
    {
        ctx.include_folders.push_back(include_folder);
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


            fs::path relative_path = fs::relative(*it, _options.root_directory);
            relative_path.replace_extension(".o");
            _files.push_back(file(*it, _options.root_directory, fs::relative(*it, root_folder), ctx));
            if(_options.verbose){
                _output << _files.back() << std::endl;
            }
            auto& file = _files.back();
            if (file.get_type() == FILE_TYPE::SOURCE)
            {
                _dep_tree.add(file.get_file_path(), ctx.include_folders);
                _header_only = false;
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
            if (fs::exists(dest_path))
            {
                if (fs::last_write_time(dest_path) > fs::last_write_time(file.get_file_path()))
                {
                    // ignore if dest file is more recent than current file
                    continue;
                }
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
            if (fs::exists(target_path) && fs::exists(target_path / ".git") && _options.update_git)
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
        options.update_git = _options.update_git;
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
                if(!maker._header_only)
                {
                    auto libdir = compute_path(_options.root_directory, "lib");
                    fs::create_directories(libdir);
                    maker.export_binary(libdir);
                }
                auto includedir = compute_path(_options.root_directory, "include");
                fs::create_directories(includedir);
                maker.export_header_files(includedir);
                library_added = true;
                if(!maker._header_only)
                {
                    _config.libraries.push_back(maker.get_name());
                }
                for (auto libs : maker._config.libraries)
                {
                    auto it = std::find(_config.libraries.begin(), _config.libraries.end(), libs.c_str());
                    if (it == _config.libraries.end())
                    {
                        _config.libraries.push_back(libs);
                    }
                    else
                    {
                        std::rotate(it, it + 1, _config.libraries.end());
                    }
                }
                for (auto include : maker._config.include_folder)
                {
                    _config.include_folder.push_back(include);
                }
            }
            else
            {
                auto bindir = compute_path(_options.root_directory, "bin");
                fs::create_directories(bindir);
                maker.export_binary(bindir);
            }
            maker.export_dependency(*this);
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


BuildStatus CPPMaker::compile_project_async(fs::file_time_type& last_write)
{
    struct task
    {
        file* target_file;
        std::future<Process::Result> result;
        std::stringstream output;
        bool finished = false;
        Process::Result status = Process::Result::Success;
    };
    BuildStatus status = BuildStatus::NoChange;

    std::vector<task> tasks;
    for (auto& f : _files)
    {
        if (f.get_type() == FILE_TYPE::SOURCE)
        {
            auto obj_file_path = get_object_path(f);
            bool should_rebuild = _options.full_rebuild
                || !fs::exists(obj_file_path)
                || _dep_tree.need_rebuild(f.get_file_path(), fs::last_write_time(obj_file_path));
            
            if (should_rebuild)
            {
                status = BuildStatus::Changed;

                task task;
                task.target_file = &f;
                tasks.push_back(std::move(task));
            }
            else if (_options.verbose)
            {
                _output << term::blue << "Skipped " << f.get_file_path() << term::reset << std::endl;
            }
        }
    }

    std::mutex task_mutex;
    int running = 0;
    bool finished = false;

    auto handle_result = [&]()
        {
            size_t finished_task = 0;
            while (finished_task < tasks.size())
            {
                for (size_t i = 0; i < tasks.size(); i++)
                {
                    auto& t = tasks.at(i);
                    if (!t.finished && t.result.valid() && t.result.wait_for(0s) == std::future_status::ready)
                    {
                        task_mutex.lock();
                        running--;
                        task_mutex.unlock();
                        finished_task++;
                        t.finished = true;
                        _output << term::cyan << "Rebuilding " << t.target_file->get_file_path() << ": " << term::reset << std::flush;
                        auto result = t.result.get();
                        t.status = result;
                        if (result == Process::Result::Failed)
                        {
                            status = BuildStatus::Failed;
                            _output << term::red << "Failed " << term::reset << std::endl;
                        }
                        else
                        {
                            _output << term::green << "Rebuilt" << term::reset << std::endl;
                        }
                        auto target_obj_file = get_object_path(*t.target_file);
                        if (fs::exists(target_obj_file))
                        {
                            auto file_last_write = fs::last_write_time(target_obj_file);
                            if(file_last_write > last_write){
                                last_write = file_last_write;
                            }
                        }
                    }
                }
            }
            finished = true;
        };

    auto start_task_handler = [&]()
        {
            for (size_t i = 0; i < tasks.size(); i++)
            {
                while (running >= _config.num_thread)
                {
                    std::this_thread::sleep_for(std::chrono::duration<double>(0.1s));
                }
                task_mutex.lock();
                running++;
                auto& t = tasks.at(i);
                t.result = std::async(std::launch::async, &CPPMaker::compile_object, this, std::ref(*t.target_file), std::ref(t.output));
                task_mutex.unlock();
            }
        };

    std::thread processing(handle_result);
    std::thread starter(start_task_handler);
    
    starter.join();
    processing.join();

    for (auto& t : tasks)
    {
        if (t.status == Process::Result::Failed)
        {
            _output << term::red << t.target_file->get_file_path() << " Failed:" << term::reset << std::endl;
            _output << t.output.str() << std::endl;
        }
        else if (_options.show_warning)
        {
            auto output = t.output.str();
            if (output.size() > 0)
            {
                _output << term::yellow << t.target_file->get_file_path() << " has warning:" << term::reset << std::endl;
                _output << output << std::endl;
            }
        }
    }

    return status;
}

Process::Result CPPMaker::compile_object(const file& file, std::stringstream& output)
{
    auto object_path = get_object_path(file);
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
    std::string compiler = _config.compiler;
    if (compiler.compare("g++") == 0 && file.get_file_path().extension().string().compare(".c") == 0)
    {
        compiler = "gcc";
    }
    command << compiler << " ";
    if(_options.debug) command << "-g ";
    command << "-Wfatal-errors ";
    command << "-Wall -Wextra ";
    command << "-fdiagnostics-color=always ";
    if (compiler.compare("gcc") != 0)
    {
        command << "-std=" << _config.standard << " ";
    }
    command << "-o " << get_object_path(file) << " -c " << file.get_file_path();

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
                fs::path full_path = fs::path(lib_path) / ("lib" + lib + LIB_EXT);
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
            command << " " << get_object_path(f);
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
    auto lib_file = compute_path(_options.root_directory, _config.get_binary_path());
    if (fs::exists(lib_file))
    {
        fs::remove(lib_file);
    }
    
    command << "ar rcs ";
    
    command << "-o " << lib_file;
    
    for (auto& f : _files)
    {
        if(f.get_type() == FILE_TYPE::SOURCE){
            command << " " << get_object_path(f);
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

void CPPMaker::export_dependency(const CPPMaker& target)
{
    // export libs 
    auto lib_dir = compute_path(_options.root_directory, "lib");
    if (fs::exists(lib_dir))
    {
        auto it = fs::directory_iterator(lib_dir);
        auto target_lib_folder = compute_path(target._options.root_directory, "lib");
        for (decltype(it) end; it != end; ++it)
        {
            auto target = target_lib_folder / it->path().filename();
            if (!fs::exists(target))
            {
                fs::copy(*it, target);
            }
        }
    }

    // export includes
    auto inc_dir = compute_path(_options.root_directory, "include");
    if (fs::exists(inc_dir))
    {
        auto inc_it = fs::directory_iterator(inc_dir);
        auto target_include_folder = compute_path(target._options.root_directory, "include");
        for (decltype(inc_it) end; inc_it != end; ++inc_it)
        {
            auto target = target_include_folder / inc_it->path().filename();
            if (!fs::exists(target_include_folder / inc_it->path().filename()))
            {
                fs::copy(*inc_it, target);
            }
        }
    }
        
}

std::filesystem::path CPPMaker::get_object_path(const file& file)
{
    return _obj_root / fs::relative(file.get_file_path(), _options.root_directory).replace_extension(".o");
}
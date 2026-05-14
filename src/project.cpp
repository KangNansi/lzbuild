#include "project.hpp"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <ostream>
#include <future>
#include <chrono>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>
#include "config.hpp"
#include "file.hpp"
#include "utility/cmd.hpp"
#include "utility/term.hpp"
#include "env.hpp"


namespace fs = std::filesystem;
using namespace std::chrono_literals;

fs::path compute_path(fs::path root, fs::path target)
{
    return target.is_absolute() ? target : root / target;
}

const char* set_color(const char* str, bool enable)
{
	return enable ? str : "";
}

build_options::build_options(const ArgReader& args)
{
    verbose = args.has("-v");
    full_rebuild = args.has("-fr");
    force_linking = args.has("-fl");
    debug = args.has("--debug") || args.has("-g");
    output_command = args.has("--output-command");
    show_warning = args.has("--show-warning") || args.has("-sw");
    print_dependencies = args.has("--print-dependencies");
		color_output = !args.has("--no-color");
    std::string arg_value;
    if (args.get("-c", arg_value))
    {
        if(!fs::exists(arg_value)){ 
            std::cerr << "Could not find config " << arg_value << std::endl;
            throw "Could not find config " + arg_value;
        }
        config = arg_value;
    }
    if(args.get("--export-dir", arg_value))
    {
        export_directory = arg_value;
    }
}

project::project(const ArgReader& args) : _options(args)
{
    read_config(_config, compute_path(_options.root_directory, _options.config));
    _obj_root = _options.root_directory / "obj" / fs::path(_options.config).stem();
    build_file_registry();
}

project::project(const build_options& options, std::ostream& output, std::ostream& err) : _options(options), _output(output), _err(err)
{
    read_config(_config, compute_path(_options.root_directory, _options.config));
    _obj_root = _options.root_directory / "obj" / fs::path(_options.config).stem();
    build_file_registry();
}

Process::Result project::build()
{

    if (_options.print_dependencies)
    {
        _dep_tree.print(_output);
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
        _output << set_color(term::red, _options.color_output) << "Build failed" << set_color(term::reset, _options.color_output) << std::endl;
        return Process::Result::Failed;
    }

    if (!_options.force_linking && !binary_requires_rebuild(last_write))
    {
        _output << "Binary is up to date." << std::endl;
        return Process::Result::Success;
    }

    _output << "Creating " << (is_library() ? "library" : "executable") << "..." << std::endl;
    
    auto binary_path = compute_path(_options.root_directory, _config.get_binary_path());
    auto cmd = get_link_command(binary_path.string());
    if(fs::exists(binary_path))
    {
        fs::remove(binary_path);
    }

    if(_options.output_command) _output << cmd << std::endl;
    std::stringstream output;
    if(Process::Run(cmd.c_str(), output) == Process::Result::Failed)
    {
        std::cerr << set_color(term::red, _options.color_output) << "Error creating binary" << set_color(term::reset, _options.color_output) << std::endl;
        _output << output.str() << std::flush;
        return Process::Result::Failed;
    }
    return Process::Result::Success;
}

void project::export_binary(std::filesystem::path target)
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
        _output << set_color(term::green, _options.color_output) << "Exported " << binary_path << " to " << executable << set_color(term::green, _options.color_output) << std::endl;
    }
    catch (fs::filesystem_error& error)
    {
        std::cerr << set_color(term::red, _options.color_output) << "Could not export executable: " << error.what() << set_color(term::reset, _options.color_output) << std::endl;
    }
}

void project::build_file_registry()
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
    // TODO: fix for windows
    if (std::filesystem::exists("/usr/include"))
    {
        ctx.include_folders.push_back("usr/include");
    }
    if(std::filesystem::exists(INCLUDE_EXPORT_PATH))
    {
        ctx.include_folders.push_back(INCLUDE_EXPORT_PATH);
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

std::string project::get_build_commands()
{
    std::stringstream ss;
    // std::vector<std::string> source_files;
    for(auto& file: _files)
    {
        if(file.get_type() == FILE_TYPE::SOURCE)
        {
            // source_files.push_back(fs::relative(file.get_file_path()));
            auto output_file = std::filesystem::relative(get_object_path(file));
            auto cmd = get_object_compilation_command(file);
            ss << "echo \"" << cmd << "\"" << std::endl;
            ss << "mkdir -p " << output_file.remove_filename() << std::endl;
            ss << cmd << std::endl;
        }
    }
    auto binary_path = fs::relative(compute_path(_options.root_directory, _config.get_binary_path()));
    auto cmd = get_link_command(binary_path.string());
    ss << "echo \"" << cmd << "\"" << std::endl;
    ss << "mkdir -p " << std::filesystem::relative(binary_path) << std::endl;
    ss << cmd << std::endl;
    return ss.str();
}

void project::export_header_files(std::filesystem::path target)
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
            _output << set_color(term::green, _options.color_output) << "Exported " << file.get_file_path() << " to " << dest_path << set_color(term::reset, _options.color_output) << std::endl;
        }
    }
}

BuildStatus project::compile_project_async(fs::file_time_type& last_write)
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
                _output << set_color(term::blue, _options.color_output) << "Skipped " << f.get_file_path() << set_color(term::reset, _options.color_output) << std::endl;
            }
        }
    }

    std::mutex task_mutex;
    size_t running = 0;
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
                        _output << set_color(term::cyan, _options.color_output) << "Rebuilding " << t.target_file->get_file_path() << ": " << set_color(term::reset, _options.color_output) << std::flush;
                        auto result = t.result.get();
                        t.status = result;
                        if (result == Process::Result::Failed)
                        {
                            status = BuildStatus::Failed;
                            _output << set_color(term::red, _options.color_output) << "Failed " << set_color(term::reset, _options.color_output) << std::endl;
                        }
                        else
                        {
                            _output << set_color(term::green, _options.color_output) << "Rebuilt" << set_color(term::reset, _options.color_output) << std::endl;
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
                t.result = std::async(std::launch::async, &project::compile_object, this, std::ref(*t.target_file), std::ref(t.output));
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
            _output << set_color(term::red, _options.color_output) << t.target_file->get_file_path() << " Failed:" << set_color(term::reset, _options.color_output) << std::endl;
            _err << t.output.str() << std::endl;
        }
        else if (_options.show_warning)
        {
            auto output = t.output.str();
            if (output.size() > 0)
            {
								_output << set_color(term::yellow, _options.color_output) << t.target_file->get_file_path() << " has warning:" << set_color(term::reset, _options.color_output) << std::endl;
                _err << output << std::endl;
            }
        }
    }

    return status;
}

Process::Result project::compile_object(const file& file, std::stringstream& output)
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

    std::string cmd = get_object_compilation_command(file);

    if (_options.output_command) _output << std::endl << cmd << std::endl;
    return Process::Run(cmd.c_str(), output);
}

std::string project::get_object_compilation_command(const file& file)
{
    std::stringstream command;
    std::string compiler = _config.compiler;
    if (compiler.compare("g++") == 0 && file.get_file_path().extension().string().compare(".c") == 0)
    {
        compiler = "gcc";
    }
    command << compiler << " ";
    if(_options.debug) command << "-g ";
    command << "-Wfatal-errors ";
    command << "-Wall ";

    if (_options.color_output) command << "-fdiagnostics-color=always ";
    
		if (compiler.compare("gcc") != 0)
    {
        command << "-std=" << _config.standard << " ";
    }
    command << "-o " << std::filesystem::relative(get_object_path(file)) << " -c " << std::filesystem::relative(file.get_file_path());

    // includes
    for (auto& include : _config.include_folder)
    {
        command << " -I" << include;
    }
#ifdef _WIN32
    command << " -I" << INCLUDE_EXPORT_PATH;
#endif

    // cflags
    for(auto& flag : _config.cflags)
    {
        command << " " << flag;
    }

    // libs cflags
    for(auto& lib: _config.libraries)
    {
        for(auto& cflag: lib.config.cflags){
            command << " " << cflag;
        }
    }

    // macros
    for(auto& macro: _config.macros)
    {
        command << " -D " << macro;
    }
    return command.str();
}

bool project::binary_requires_rebuild(fs::file_time_type last_write)
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
                fs::path full_path = fs::path(lib_path) / ("lib" + lib.name + LIB_EXT);
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

Process::Result project::link(std::stringstream& output)
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
#ifdef _WIN32
    command << " -L" << LIBRARY_EXPORT_PATH;
#endif

    for(auto& lib: _config.libraries){

        try{
            auto lib_config = lib.config;
            for(auto& cflag: lib_config.cflags){
                command << " " << cflag;
            }
            for(auto& lib: lib_config.lib_flags){
                command << " " << lib;
            }
        }
        catch(const std::runtime_error& error){
            std::cerr << set_color(term::red, _options.color_output) << error.what() << set_color(term::reset, _options.color_output) << std::endl;
            command << " -l" << lib.name;
        }
    }

    if(_config.link_etc.size() > 0){
        for(auto& etc: _config.link_etc)
        {
            command << " " << etc;
        }
    }

    if(_options.output_command) _output << command.str() << std::endl;
    return Process::Run(command.str().c_str(), output);
}

Process::Result project::link_library(std::stringstream& output)
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

std::string project::get_link_command(std::string output)
{
    std::stringstream command;

    if(is_library())
    {
        command << "ar rcs ";
    
        command << "-o " << fs::relative(output);
        
        for (auto& f : _files)
        {
            if(f.get_type() == FILE_TYPE::SOURCE){
                command << " " << fs::relative(get_object_path(f));
            }
        }
    }
    else
    {
        command << _config.compiler << " ";
        if(_options.debug) command << "-g ";
        command << "-Wfatal-errors ";
        command << "-Wall -Wextra ";
        command << "-fdiagnostics-color=always ";
        command << "-std=" << _config.standard << " ";

        auto binary_path = compute_path(_options.root_directory, _config.get_binary_path());
        command << "-o " << fs::relative(binary_path);
        for(auto& f: _files){
            if(f.get_type() == FILE_TYPE::SOURCE){
                command << " " << fs::relative(get_object_path(f));
            }
        }

        for (auto& libpath : _config.library_paths)
        {
            auto path = compute_path(_options.root_directory, libpath);
            command << " -L" << path;
        }

        for(auto& lib: _config.libraries){
            auto lib_config = lib.config;
            for(auto& cflag: lib_config.cflags){
                command << " " << cflag;
            }
            for(auto& lib: lib_config.lib_flags){
                command << " " << lib;
            }
        }

        if(_config.link_etc.size() > 0){
            for(auto& etc: _config.link_etc)
            {
                command << " " << etc;
            }
        }
    }
    return command.str();
}

std::filesystem::path project::get_pretty_path(std::filesystem::path path)
{
    auto mismatch_pair = std::mismatch(path.begin(), path.end(), _options.root_directory.begin(), _options.root_directory.end());
    return mismatch_pair.second == _options.root_directory.end() ? fs::relative(path, _options.root_directory) : path;
}

std::filesystem::path project::get_object_path(const file& file)
{
    return _obj_root / fs::relative(file.get_file_path(), _options.root_directory).replace_extension(".o");
}

void project::export_asset_folder(std::filesystem::path path)
{
    if(_config.asset_folder.has_value())
    {
        auto asset_folder = fs::canonical(_config.asset_folder.value());
        fs::copy(asset_folder, path, fs::copy_options::overwrite_existing | fs::copy_options::recursive);
        _output << set_color(term::green, _options.color_output) << "Exported " << asset_folder << " to " << path << set_color(term::green, _options.color_output) << std::endl;
    }
}

void project::generate_compile_commands(std::filesystem::path folder)
{
    std::ofstream output((folder / "compile_commands.json").string());
    auto sanitize_str = [](std::string& str) -> std::string&
    {
        size_t start_pos = 0;
        while ((start_pos = str.find('"', start_pos)) != std::string::npos)
        {
            str.insert(start_pos, 1, '\\');
            start_pos += 2;
        }
        return str;
    };

    output << "[\n";

    std::vector<size_t> object_files;
    for (size_t i = 0; i < _files.size(); i++)
    {
        if (_files[i].get_type() == FILE_TYPE::SOURCE)
        {
            object_files.push_back(i);
        }
    }

    for (size_t i = 0; i < object_files.size(); i++)
    {
        auto& file = _files[object_files[i]];
        output << "\t{\n";
        auto cmd = get_object_compilation_command(file);
        
        output << "\t\t\"command\": \"" << sanitize_str(cmd) << "\",\n";
        output << "\t\t\"directory\": " << _options.root_directory << ",\n";
        output << "\t\t\"file\": " << file.get_file_path() << ",\n";
        output << "\t\t\"output\": " << get_object_path(file) << "\n";
        output << "\t}";
        if (i < object_files.size() - 1)
        {
            output << ",";
        }
        output << "\n";
    }
    output << "]\n";
    

}

void project::generate_pkg_config(std::filesystem::path folder)
{
    std::filesystem::path file_path = folder / (_config.name + ".pc");
    std::ofstream file(file_path);
    if(!file) {
        _output << set_color(term::red, _options.color_output) << "Failed to write pkg-config configuration at " << file_path << set_color(term::reset, _options.color_output) << std::endl;
        return;
    }
    file << "prefix=/usr/local" << std::endl;
    file << "includedir=${prefix}/include" << std::endl;
    file << "libdir=${prefix}/lib" << std::endl;
    file << std::endl;
    file << "Name: " << _config.name << std::endl;
    file << "Description: " << _config.name << std::endl; // TODO: description is mandatory, add a config for that later
    file << "Version: 1.0" << std::endl;
    file << "Requires.private: ";
    bool add_delim = false;
    for(size_t i = 0; i < _config.libraries.size(); i++)
    {
        if(_config.libraries[i].config.has_pkg_config)
        {
            if(add_delim)
            {
                file << ", ";
            }
            file << _config.libraries[i].name;
            add_delim = true;
        }
    }
    file << std::endl;
    file << "Libs: -L${libdir} -l" << _config.name;
    for(auto& lib: _config.libraries)
    {
        // if(!lib.config.has_pkg_config)
        {
            for(auto& libflag: lib.config.lib_flags)
            {
                file << " " << libflag;
            }
        }
    }
    file << std::endl;
    file << "Cflags: -I${includedir}" << std::endl;

    _output << set_color(term::green, _options.color_output) << "Generated pkg-config file at " << file_path << set_color(term::green, _options.color_output) << std::endl;
}

#include <iostream>
#include <filesystem>
#ifdef WIN32
 #include "windows/cmd.hpp"
#else
 #include "linux/cmd.hpp"
#endif
#include "file.hpp"
#include <sstream>
#include "config.hpp"
#include "args.hpp"
#include "term.hpp"
#include "commands.hpp"

namespace fs = std::filesystem;
using namespace std;

void initialize_compiler_cmd(std::stringstream& stream, const ArgReader& args, const config& cfg){
    stream << cfg.compiler << " ";
    if(args.has("--debug")){
        stream << "-g ";
    }
    stream << "-Wall -Wextra ";
    stream << "-fdiagnostics-color=always ";
    stream << "-std=" << cfg.standard << " ";
}

void initialize_compiler_library_cmd(std::stringstream& stream){
    stream << "ar rcs ";
}

fs::file_time_type library_last_write(const config& cfg)
{
    fs::file_time_type last_write = fs::file_time_type::min();
    for (auto path : cfg.library_paths)
    {
        for (auto lib : cfg.libraries)
        {
            fs::path full_path = fs::path(path) / (lib + ".lib");
            if (fs::exists(full_path))
            {
                auto file_last_write = fs::last_write_time(full_path);
                if (file_last_write > last_write)
                {
                    last_write = file_last_write;
                }
            }
        }
    }
    return last_write;
}

Process::Result build(const file& file, const ArgReader& args, const config& cfg, stringstream& output){
    auto object_path = file.get_object_path();
    auto dir_path = object_path.remove_filename();
    fs::path directory;
    for(auto &dir : dir_path){
        directory /= dir;
        if(!fs::exists(directory)){
            fs::create_directory(directory);
        }
    }

    stringstream ss;

    initialize_compiler_cmd(ss, args, cfg);
    
    ss << "-o " << file.get_object_path() << " -c " << file.get_file_path();
 
    for(auto& include: cfg.include_folder){
        ss << " -I" << include;
    }

    if(cfg.cflags.length() > 0){
        ss << " " << cfg.cflags;
    }

    try
    {
        return Process::Run(ss.str().c_str(), output);
    }
    catch(const char* error){
        cerr << "Error building file " << file.get_file_path() << ": " << error << endl;
        return Process::Result::Failed;
    }
    catch(const std::string& error){
        cerr << "Error building file " << file.get_file_path() << ": " << error << endl;
        return Process::Result::Failed;
    }
    catch(const std::exception& exception){
        cerr << "Error building file " << file.get_file_path() << ": " << exception.what() << endl;
        return Process::Result::Failed;
    }
}

Process::Result link(fs::path binary_path, const std::vector<file>& files, const ArgReader& args, const config& cfg, stringstream& output){
    stringstream cmd;

    initialize_compiler_cmd(cmd, args, cfg);
    
    cmd << "-o " << binary_path;
    for(auto& f: files){
        if(f.get_type() == FILE_TYPE::SOURCE){
            cmd << " " << f.get_object_path();
        }
    }

    for(auto& libpath: cfg.library_paths){
        cmd << " -L" << libpath;
    }

    for(auto& lib: cfg.libraries){
        cmd << " -l" << lib;
    }

    if(cfg.link_etc.length() > 0){
        cmd << " " << cfg.link_etc;
    }

    std::cout << cmd.str() << std::endl;
    return Process::Run(cmd.str().c_str(), output);
}

Process::Result link_library(fs::path binary_path, const std::vector<file>& files, stringstream& output){
    stringstream cmd;

    initialize_compiler_library_cmd(cmd);
    
    cmd << "-o " << binary_path;
    for(auto& f: files){
        if(f.get_type() == FILE_TYPE::SOURCE){
            cmd << " " << f.get_object_path();
        }
    }

    return Process::Run(cmd.str().c_str(), output);
}

void log_output(stringstream& output){
    auto str = output.str();
    cout << str;
    if(str.length() > 0 && str[str.length() - 1] != '\n'){
        cout << endl;
    }
    else{
        cout << flush;
    }
}

bool create_output(const config& cfg, const std::vector<file>& files, const ArgReader& args, fs::file_time_type last_write, bool require_rebuild){
    fs::path binary_path = cfg.get_binary_path();
    if (fs::exists(binary_path))
    {
        if (fs::last_write_time(binary_path) < last_write)
        {
            require_rebuild = true;
        }
        else if (fs::last_write_time(binary_path) < library_last_write(cfg))
        {
            cout << "Library is outdated." << endl;
            require_rebuild = true;
        }
    }
    else
    {
        require_rebuild = true;
    }
    if (!require_rebuild)
    {
        cout << "Executable is up to date." << endl;
        return true;
    }
    if(!cfg.is_library){
        cout << "Creating executable..." << endl;
        stringstream link_output;
        if(link(binary_path, files, args, cfg, link_output) == Process::Result::Failed){
            cerr << term::red << "Error creating executable" << term::reset << endl;
            log_output(link_output);
            return false;
        }
    }
    else{
        cout << "Creating library..." << endl;
        stringstream lib_output;
        if(link_library(binary_path, files, lib_output) == Process::Result::Failed){
            cerr << term::red << "Error creating executable" << term::reset << endl;
            log_output(lib_output);
            return false;
        }
    }
    
    return true;
}

void export_header_files(fs::path destination, const std::vector<file>& files){
    for(auto& file : files){
        if(file.get_type() == FILE_TYPE::HEADER){
            auto path = file.get_file_path().relative_path();
            path = fs::relative(path, *(path.begin()));
            auto dest_path = destination / path;
            if(fs::exists(dest_path)){
                fs::remove(dest_path);
            }
            fs::create_directories(dest_path.parent_path());
            fs::copy(file.get_file_path(), dest_path);
        }
    }
}

int main(int argc, char** argv)
{
    try
    {
        ArgReader args(argc, argv);

        bool verbose = args.has("-v");
        bool dependencies_only = args.has("-d");
        bool full_rebuild = args.has("-fr");
        bool force_linking = args.has("-fl");
        bool run_after_build = args.has("-r");
        bool export_after_build = args.has("-e");

        if (args.has("init"))
        {
            init(fs::current_path());
            return 0;
        }
        
        std::string arg_value;

        config cfg;
        if(args.get("-c", arg_value)){
            if(!fs::exists(arg_value)){ 
                std::cerr << "Could not find config " << arg_value << std::endl;
                return -1;
            }
            read_config(cfg, arg_value);
        }
        else{
            read_config(cfg, "cppmaker.cfg");
        }

        std::vector<file> files;
        fs::path obj_dir("obj");
        if(!fs::exists(obj_dir)){
            fs::create_directory(obj_dir);
        }
        fs::path bin_dir("bin");
        if(!fs::exists(bin_dir)){
            fs::create_directory(bin_dir);
        }

        for (auto& src_folder : cfg.source_folders)
        {
            auto it = fs::recursive_directory_iterator(src_folder);
            for(decltype(it) end; it != end; ++it)
            {
                if (cfg.is_excluded(*it))
                {
                    it.disable_recursion_pending();
                    continue;
                }
                if (fs::is_directory(*it))
                {
                    continue;
                }
                
                auto relative_path = fs::relative(*it, src_folder);
                files.push_back(file(*it, src_folder));
                if(verbose){
                    cout << files.back() << endl;
                }
            }
        }
        if(dependencies_only){
            return 0;
        }

        bool require_rebuild = false;
        bool build_failed = false;
        fs::file_time_type last_write = fs::file_time_type::min();
        for(auto& f: files){
            if(f.get_type() == FILE_TYPE::SOURCE){
                bool should_rebuild = full_rebuild || f.need_rebuild(files);
                if(should_rebuild){
                    cout << term::cyan << "Rebuilding " << f.get_file_path() << ": " << term::reset << std::flush;
                    require_rebuild = true;
                    stringstream build_output;
                    if(build(f, args, cfg, build_output) == Process::Result::Failed){
                        build_failed = true;
                        cout << term::red << "Failed " << term::reset << endl;
                        log_output(build_output);
                    }
                    else
                    {
                        log_output(build_output);
                        cout << term::green << "Rebuilt" << term::reset << endl;
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
            if(build_failed){
                break;
            }
        }
        
        if (build_failed)
        {
            cout << term::red << "Build failed" << term::reset << endl;
            return -1;
        }
        
        if (!create_output(cfg, files, args, last_write, require_rebuild || force_linking))
        {
            return -1;
        }

        auto binary_path = cfg.get_binary_path();
        if(export_after_build && !cfg.export_path.empty()){
            if(!fs::exists(cfg.export_path)){
                cerr << term::red << "Could not find export path at: " << cfg.export_path << term::reset << endl;
                return 1;
            }

            if(!cfg.is_library){
                fs::path final_path(cfg.export_path);
                
                if(fs::is_directory(final_path)){
                    final_path /= binary_path.filename();
                }
                cout << "Exporting executable to " << final_path << endl;
                try{
                    if(fs::exists(final_path)){
                        fs::remove(final_path);
                    }
                    fs::copy(binary_path, final_path);
                }
                catch(const fs::filesystem_error& error){
                    cerr << term::red << "Could not export executable: " << error.what() << term::reset << endl;
                }
            }
            else{
                auto lib_path = cfg.export_path / "libs" / binary_path.filename();
                fs::create_directories(lib_path.parent_path());
                cout << "Exporting library to " << lib_path << endl;
                try{
                    if(fs::exists(lib_path)){
                        fs::remove(lib_path);
                    }
                    fs::copy(binary_path, lib_path);
                    auto include_path = cfg.export_path / "includes" / cfg.name;
                    fs::create_directories(include_path);
                    export_header_files(include_path, files);
                }
                catch(const fs::filesystem_error& error){
                    cerr << term::red << "Could not export executable: " << error.what() << term::reset << endl;
                }
            }
        }

        if(run_after_build){
            cout << "Running executable" << endl;

            try{
                Process::Run(binary_path.string().c_str());
            }
            catch(const std::string& error){
                cerr << term::red << error << term::reset << endl;
            }
            catch(const char* error){
                cerr << term::red << error << term::reset << endl;
            }
        }
    }
    catch (std::string error)
    {
        std::cerr << error << std::endl;
    }
    
    return 0;
}
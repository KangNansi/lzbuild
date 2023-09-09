#include "compile.hpp"
#include <filesystem>
#include <vector>
#include <iostream>
#ifdef WIN32
 #include "windows/cmd.hpp"
#else
 #include "linux/cmd.hpp"
#endif
#include "term.hpp"

namespace fs = std::filesystem;
using namespace std;

void initialize_compiler_cmd(std::stringstream& stream, const ArgReader& args, const config& cfg){
    stream << cfg.compiler << " ";
    stream << "-g "; // debug by default
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
            cout << link_output.str() << flush;
            return false;
        }
    }
    else{
        cout << "Creating library..." << endl;
        stringstream lib_output;
        if(link_library(binary_path, files, lib_output) == Process::Result::Failed){
            cerr << term::red << "Error creating executable" << term::reset << endl;
            cout << lib_output.str() << flush;
            return false;
        }
    }
    
    return true;
}


int build(std::vector<file> files, config& cfg, const ArgReader& args, bool verbose, bool dependencies_only, bool full_rebuild, bool force_linking)
{
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
                    cout << build_output.str() << flush;
                }
                else
                {
                    cout << build_output.str() << flush;
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

    return 0;
}
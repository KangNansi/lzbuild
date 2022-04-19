#include <iostream>
#include <filesystem>
#include "cmd.hpp"
#include "file.hpp"
#include <sstream>
#include "config.hpp"
#include "args.hpp"
#include "term.hpp"

namespace fs = std::filesystem;
using namespace std;

void initialize_compiler_cmd(std::stringstream& stream, const ArgReader& args){
    stream << "g++ ";
    if(args.has("--debug")){
        stream << "-g ";
    }
    stream << "-fdiagnostics-color=always ";
    stream << "-std=c++20 ";
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

    initialize_compiler_cmd(ss, args);
    
    ss << "-o " << file.get_object_path() << " -c " << file.get_file_path();
 
    for(auto& include: cfg.include_folder){
        ss << " -I" << include;
    }

    if(cfg.cflags.length() > 0){
        ss << " " << cfg.cflags;
    }

    try{
        return Process::Run(ss.str().c_str(), output);
    }
    catch(const char* error){
        cerr << error << endl;
        return Process::Result::Failed;
    }
    catch(const std::string& error){
        cerr << error << endl;
        return Process::Result::Failed;
    }
    catch(const std::exception& exception){
        cerr << "Error building file " << file.get_file_path() << ": " << exception.what() << endl;
        return Process::Result::Failed;
    }
}

Process::Result link(fs::path binary_path, const std::vector<file>& files, const ArgReader& args, const config& cfg, stringstream& output){
    stringstream cmd;

    initialize_compiler_cmd(cmd, args);
    
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

int main(int argc, char** argv){
    ArgReader args(argc, argv);

    bool verbose = args.has("-v");
    bool dependencies_only = args.has("-d");
    bool full_rebuild = args.has("-fr");
    bool run_after_build = args.has("-r");

    config cfg;
    read_config(cfg, "cppmaker.cfg");

    fs::path src("src");
    std::vector<file> files;
    fs::path obj_dir("obj");
    if(!fs::exists(obj_dir)){
        fs::create_directory(obj_dir);
    }

    for(auto& p: fs::recursive_directory_iterator(src)){
        if(fs::is_directory(p)){
            continue;
        }
        files.push_back(file(p));
        if(verbose){
            cout << files.back() << endl;
        }
    }
    if(dependencies_only){
        return 0;
    }

    bool require_rebuild = false;
    bool build_failed = false;
    fs::file_time_type last_write = chrono::file_clock::now();
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
                else{
                    cout << term::green << "Rebuilt" << term::reset << endl;
                }
            }
            else{
                //cout << "Already done" << endl;
            }
            auto file_last_write = fs::last_write_time(f.get_object_path());
            if(file_last_write < last_write){
                last_write = file_last_write;
            }
        }
        if(build_failed){
            break;
        }
    }
    if(build_failed){
        cout << term::red << "Build failed" << term::reset << endl;
        return 0;
    }

    fs::path binary_path = fs::path("bin") / (cfg.name + ".exe");
    if(!fs::exists(binary_path) || fs::last_write_time(binary_path) < last_write){
        require_rebuild = true;
    }
    if(require_rebuild){
        cout << "Creating executable..." << endl;
        stringstream link_output;
        if(link(binary_path, files, args, cfg, link_output) == Process::Result::Failed){
            cerr << term::red << "Error creating executable" << term::reset << endl;
            log_output(link_output);
            return 0;
        }
    }
    else{
        cout << "Executable is up to date." << endl;
    }


    
    if(!cfg.export_path.empty()){
        if(!fs::exists(cfg.export_path)){
            cerr << term::red << "Could not find export path at: " << cfg.export_path << term::reset << endl;
            return 1;
        }
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
            cerr << term::red << "Could not export executable: file is busy" << term::reset << endl;
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

    return 0;
}
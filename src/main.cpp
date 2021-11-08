#include <iostream>
#include <filesystem>
#include "cmd.hpp"
#include "file.hpp"
#include <sstream>
#include "config.hpp"

namespace fs = std::filesystem;
using namespace std;

void build(const file& file){
    stringstream ss;
    ss << "g++ -std=c++20 -o " << file.get_object_path() << " -c " << file.get_file_path();

    try{
        run(ss.str().c_str());
    }
    catch(const std::exception& exception){
        cerr << "Error building file " << file.get_file_path() << ": " << exception.what() << endl;
    }
}

void link(fs::path binary_path, const std::vector<file>& files){
    stringstream cmd;
    cmd << "g++ -std=c++20 -o " << binary_path;
    for(auto& f: files){
        if(f.get_type() == FILE_TYPE::SOURCE){
            cmd << " " << f.get_object_path();
        }
    }
    run(cmd.str().c_str());
}

int main(int argc, char** argv){
    fs::path src("src");
    std::vector<file> files;
    fs::path obj_dir("obj");
    if(!fs::exists(obj_dir)){
        fs::create_directory(obj_dir);
    }

    for(auto& p: fs::recursive_directory_iterator(src)){
        files.push_back(file(p));
    }

    bool require_rebuild = false;
    fs::file_time_type last_write = chrono::file_clock::now();
    for(auto& f: files){
        if(f.get_type() == FILE_TYPE::SOURCE){
            if(f.need_rebuild()){
                require_rebuild = true;
                cout << "Building " << f.get_file_path() << "..." << endl;
                build(f);
            }
            auto file_last_write = fs::last_write_time(f.get_object_path());
            if(file_last_write < last_write){
                last_write = file_last_write;
            }
        }
    }


    fs::path binary_path = fs::path("bin") / "cppmaker_auto.exe";
    if(!fs::exists(binary_path) || fs::last_write_time(binary_path) > last_write){
        require_rebuild = true;
    }
    if(require_rebuild){
        cout << "Creating executable..." << endl;
        link(binary_path, files);
    }
    else{
        cout << "Executable is up to date." << endl;
    }

    cout << "Building success" << endl;
    if(fs::exists("config.cfg")){
        config cfg;
        read_config(cfg, "config.cfg");
        if(!fs::exists(cfg.export_path)){
            cerr << "Could not find export path at: " << cfg.export_path << endl;
        }
        else if(!cfg.export_path.empty()){
            cout << "Exporting executable to " << cfg.export_path << endl;
            fs::path final_path(cfg.export_path);
            if(fs::is_directory(final_path)){
                final_path /= binary_path.filename();
            }
            fs::copy(binary_path, final_path, fs::copy_options::update_existing);
        }
    }

    return 0;
}
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
#include "compile.hpp"

namespace fs = std::filesystem;
using namespace std;

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
            cout << term::green << "Exported " << file.get_file_path() << " to " << dest_path << term::reset << endl;
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

        if (build(files, cfg, dependencies_only, full_rebuild, force_linking) != 0)
        {
            return -1;
        }

        auto binary_path = cfg.get_binary_path();
        if (export_after_build && !cfg.executable_export_path.empty())
        {
            fs::create_directories(cfg.executable_export_path);
            fs::path executable = cfg.executable_export_path / binary_path.filename();
            try
            {
                if (fs::exists(executable))
                {
                    fs::remove(executable);
                }
                fs::copy(binary_path, executable);
                cout << term::green << "Exported " << binary_path << " to " << executable << term::green << endl;
            }
            catch (fs::filesystem_error& error)
            {
                cerr << term::red << "Could not export executable: " << error.what() << term::reset << endl;
            }

            if (cfg.is_library)
            {
                if (cfg.include_export_path.empty())
                {
                    cerr << term::red << "No include export path given" << term::reset << endl;
                }
                else
                {
                    fs::create_directories(cfg.include_export_path);
                    export_header_files(cfg.include_export_path, files);
                }
            }
        }

        if(run_after_build){
            cout << "Running executable" << endl;

            try
            {
                Process::Run(binary_path.string().c_str(), std::cout);
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
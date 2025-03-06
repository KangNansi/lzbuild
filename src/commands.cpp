#include "commands.hpp"
#include "utility/term.hpp"
#include "programs/git.hpp"
#include "project.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

const fs::path APP_DATA = std::string(std::getenv("HOME")) + "/.lzbuild";

void init(std::filesystem::path path) {
    fs::create_directory(path / "src");
    fs::create_directory(path / "bin");
    fs::create_directory(path / "obj");
    std::ofstream config(path / "default.lzb");
}

bool install(std::filesystem::path repository, std::filesystem::path self_cmd)
{
    if(repository.extension() == ".git")
    {
        auto data_dir = APP_DATA / "cache";
        std::string repo_name = fs::path(repository).stem();
        data_dir /= repo_name;
        if(fs::exists(data_dir) && fs::exists(data_dir / ".git"))
        {
            std::cout << term::cyan << "Updating repository..." << term::reset << std::endl;
            if(auto result = git_update(data_dir, std::cout); result == Process::Result::Failed)
            {
                std::cerr << term::red << "Failed to update repository: " << term::reset << repository << std::endl;
                return false;
            }
        }
        else
        {
            std::cout << term::cyan << "Cloning repository..." << term::reset << std::endl;
            fs::create_directories(data_dir);
            if(auto result = git_clone(data_dir, repository, std::cout); result == Process::Result::Failed)
            {
                std::cerr << term::red << "Failed to clone repository: " << term::reset << repository << std::endl;
                return false;
            }
        }
        repository = data_dir;
    }
    if(repository.empty())
    {
        repository = fs::current_path();
    }

    fs::current_path(repository);
    build_options sub_options;
    project maker(sub_options);
    if(auto result = maker.build(); result == Process::Result::Failed)
    {
        std::cerr << term::red << "Failed to compile repository: " << term::reset << repository << std::endl;
        return false;
    }

    std::string cmd = "sudo ";
    cmd += self_cmd;
    cmd += " export";
    if(auto result = Process::Run(cmd.c_str()); result == Process::Result::Failed)
    {
        std::cerr << term::red << "Failed to export repository: " << term::reset << repository << std::endl;
        return false;
    }
    
    return true;
}

bool export_project()
{
    try
    {
        build_options sub_options;
        project maker(sub_options);
        maker.build_file_registry();
        if(!maker.is_header_only())
        {
            if(maker.is_library())
            {
                maker.export_header_files("/usr/local/include");
                maker.export_binary("/usr/local/lib");
                // TODO: create pkg-config file into /usr/local/lib/pkgconfig
            }
            else
            {
                maker.export_binary("/usr/local/bin");
            }
        }
        return true;
    }
    catch(std::filesystem::filesystem_error& error)
    {
        std::cout << term::red << "Failed to export files: " << term::reset << error.what() << std::endl;
        return false;
    }
    std::cout << "failed for some reason" << std::endl;
    return false;
}
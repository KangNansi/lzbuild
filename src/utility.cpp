#include "utility.hpp"
#include "cppmaker.hpp"
#include "git.hpp"
#include "linux/cmd.hpp"
#include "term.hpp"
#include <filesystem>
#include <unistd.h>

bool install(std::filesystem::path repository, ArgReader &args, cppmaker_options &options)
{
    std::filesystem::path data_dir(getenv("HOME"));
    data_dir /= ".cppmaker/cache";
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

    fs::current_path(data_dir);
    cppmaker_options sub_options;
    CPPMaker maker(sub_options);
    if(auto result = maker.build(); result == Process::Result::Failed)
    {
        std::cerr << term::red << "Failed to compile repository: " << term::reset << repository << std::endl;
        return false;
    }

    std::cout << term::blue << "Export files" << term::reset << std::endl;
    std::string cmd = "sudo ";
    cmd += args.get_binary_dir();
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
        cppmaker_options sub_options;
        CPPMaker maker(sub_options);
        maker.build_file_registry();
        maker.export_header_files("/usr/local/include");
        if(!maker.is_header_only())
        {
            if(maker.is_library())
            {
                maker.export_binary("/usr/local/lib");
            }
            else
            {
                maker.export_binary("/usr/local/bin");
            }
        }
        return true;
    }
    catch(std::filesystem::filesystem_error error)
    {
        std::cout << term::red << "Failed to export files: " << term::reset << error.what() << std::endl;
        return false;
    }
    std::cout << "failed for some reason" << std::endl;
    return false;
}
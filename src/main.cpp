#include <iostream>
#include <filesystem>
#include "file.hpp"
#include <sstream>
#include <string>
#include "config.hpp"
#include "args.hpp"
#include "git.hpp"
#include "linux/cmd.hpp"
#include "term.hpp"
#include "commands.hpp"
#include "cppmaker.hpp"
#include "utility.hpp"

namespace fs = std::filesystem;
using namespace std;

int main(int argc, char** argv)
{
    try
    {
        ArgReader args(argc, argv);
        cppmaker_options options(args);
        
        if (args.is(0, "init"))
        {
            init(fs::current_path());
            return 0;
        }

        if(std::string path; args.get("install", path))
        {
            if(!path.ends_with(".git"))
            {
                path = "git@github.com:KangNansi/" + path + ".git";
            }
            return install(path, args, options) ? 0 : 1;
        }

        if(args.is(0, "export"))
        {
            return export_project() ? 0 : 1;
        }

        CPPMaker maker(options);
        if (maker.build() == Process::Result::Failed)
        {
            return -1;
        }
        
        if (options.export_after_build)
        {
            maker.export_binary();
        }

        if(options.run_after_build){
            maker.run();
        }

    }
    catch (const std::filesystem::filesystem_error& error)
    {
        std::cerr << error.path1() << " " << error.path2() << std::endl;
        std::cerr << error.what() << std::endl;
    }
    catch (std::string error)
    {
        std::cerr << error << std::endl;
    }
    
    return 0;
}
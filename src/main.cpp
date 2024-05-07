#include <iostream>
#include <filesystem>
#include "file.hpp"
#include <sstream>
#include "config.hpp"
#include "args.hpp"
#include "term.hpp"
#include "commands.hpp"
#include "cppmaker.hpp"

namespace fs = std::filesystem;
using namespace std;

int main(int argc, char** argv)
{
    try
    {
        ArgReader args(argc, argv);
        bool run_after_build = args.has("-r");
        bool export_after_build = args.has("-e");
        
        if (args.has("init"))
        {
            init(fs::current_path());
            return 0;
        }

        CPPMaker maker(args);
        maker.build();
        
        if (export_after_build)
        {
            maker.export_binary();
        }

        if(run_after_build){
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
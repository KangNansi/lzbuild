#include <functional>
#include <iostream>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include "programs/pkg_config.hpp"
#include "utility/args.hpp"
#include "commands.hpp"
#include "project.hpp"
#include <fstream>

namespace fs = std::filesystem;
using namespace std;

int main(int argc, char** argv)
{
    try
    {
        std::unordered_map<std::string, std::function<int()>> commands = {
            {"init", [&]() {
                if(argc != 1) {
                    std::cout << "Usage: " << argv[0] << " init" << std::endl;
                    return 1;
                }
                init(fs::current_path());
                return 0;
            }},
            {"install", [&]() {
                if(argc > 3 || argc < 2) {
                    std::cout << "Usage: " << argv[0] << " install [repository]" << std::endl;
                    return 1;
                }
                std::string path;
                std::string binary_dir = argv[0][0] == '.' ? std::filesystem::absolute(argv[0]) : argv[0];
                if(argc == 3)
                {
                    path = argv[2];
                    if(!fs::exists(path) && !path.ends_with(".git"))
                    {
                        path = "git@github.com:KangNansi/" + path + ".git";
                    }
                }
                return install(path, binary_dir) ? 0 : 1;
            }},
            {"export", [&]() {
                if(argc != 2) {
                    std::cout << "Usage: " << argv[0] << " export" << std::endl;
                    return 1;
                }
                return export_project() ? 0 : 1;
            }},
            {"build", [&]() {
                ArgReader args(argc, argv);
                build_options options(args);
                project maker(options);
                return maker.build() == Process::Result::Failed ? -1 : 0;
            }},
            {"script", [&]() {
                if(argc != 3) {
                    std::cout << "Usage: " << argv[0] << " script <output_file>" << std::endl;
                    return 1;
                }
                auto output = argv[2];
                std::ofstream file(output);
                ArgReader args(argc, argv);
                build_options options(args);
                project maker(options);
                file << maker.get_build_commands();
                return 0;
            }},
        };

        std::string cmd = argc < 2 ? "build" : argv[1];
        auto cmd_func = commands.find(cmd);
        if(cmd_func == commands.end())
        {
            commands.at("build")();
        }
        else 
        {
            cmd_func->second();
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
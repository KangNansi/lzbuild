#include <cstdlib>
#include <functional>
#include <iostream>
#include <filesystem>
#include <string>
#include <unordered_map>
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
                    return EXIT_FAILURE;
                }
                init(fs::current_path());
                return EXIT_SUCCESS;
            }},
            {"install", [&]() {
                if(argc > 3 || argc < 2) {
                    std::cout << "Usage: " << argv[0] << " install [repository]" << std::endl;
                    return EXIT_FAILURE;
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
                return install(path, binary_dir) ? EXIT_SUCCESS : EXIT_FAILURE;
            }},
            {"export", [&]() {
                if(argc != 2) {
                    std::cout << "Usage: " << argv[0] << " export" << std::endl;
                    return EXIT_FAILURE;
                }
                return export_project() ? EXIT_SUCCESS : EXIT_FAILURE;
            }},
            {"build", [&]() {
                ArgReader args(argc, argv);
                build_options options(args);
                project maker(options);
                return maker.build() == Process::Result::Failed ? EXIT_FAILURE : EXIT_SUCCESS;
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
                return EXIT_SUCCESS;
            }},
        };

        std::string cmd = argc < 2 ? "build" : argv[1];
        auto cmd_func = commands.find(cmd);
        if(cmd_func == commands.end())
        {
            return commands.at("build")();
        }
        else 
        {
            return cmd_func->second();
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
    
    return EXIT_FAILURE;
}
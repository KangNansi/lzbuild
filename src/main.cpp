#include <cstdlib>
#include <functional>
#include <iostream>
#include <filesystem>
#include <string>
#include <unordered_map>
#include "utility/args.hpp"
#include "commands.hpp"
#include "project.hpp"
#include "help.hpp"
#include <fstream>

namespace fs = std::filesystem;
using namespace std;

bool ends_with(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() && 
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

int main(int argc, char** argv)
{
    try
    {
        std::string program_name = fs::path(argv[0]).filename().string();
        
        // Handle help flags
        if (argc >= 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
            if (argc >= 3) {
                show_command_help(argv[2], program_name);
            } else {
                show_general_help(program_name);
            }
            return EXIT_SUCCESS;
        }

        std::unordered_map<std::string, std::function<int()>> commands = {
            {"help", [&]() {
                if (argc >= 3) {
                    show_command_help(argv[2], program_name);
                } else {
                    show_general_help(program_name);
                }
                return EXIT_SUCCESS;
            }},
            {"init", [&]() {
                if(argc != 2) {
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
                std::string binary_dir = argv[0][0] == '.' ? std::filesystem::absolute(argv[0]).string() : argv[0];
                if(argc == 3)
                {
                    path = argv[2];
                    if(!fs::exists(path) && !ends_with(path, ".git"))
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
            {"compile_commands", [&]() {
                ArgReader args(argc, argv);
                build_options options(args);
                project maker(options);
                auto target_folder = options.root_directory;
                if (argc > 2) {
                  target_folder = argv[2];
                    if (!std::filesystem::exists(target_folder)) {
                        std::cerr << "Directory " << target_folder << " does not exist." << std::endl;
                    }
                }
                maker.generate_compile_commands(target_folder);
                return EXIT_SUCCESS;
            }}
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
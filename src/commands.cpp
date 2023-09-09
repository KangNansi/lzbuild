#include "commands.hpp"
#include <fstream>

namespace fs = std::filesystem;

void init(std::filesystem::path path) {
    fs::create_directory(path / "src");
    fs::create_directory(path / "bin");
    fs::create_directory(path / "obj");
    std::ofstream config(path / "cppmaker.cfg");
}

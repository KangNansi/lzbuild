#include "dependency_tree.hpp"
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

std::optional<std::string> read_include(const std::string& line)
{
    const std::string includeText = "#include";
    size_t spos = line.find(includeText);
    size_t line_length = line.length();
    if(spos == std::string::npos){
        return std::nullopt;
    }
    spos += includeText.length();
    
    for(; spos < line_length; spos++){
        auto c = line[spos];
        if(c == ' ') continue;
        else if(c == '\"' || c == '<') break;
        else return std::nullopt;
    }
    if(spos >= line_length) return std::nullopt;

    char d = line[spos];
    int dstart = spos + 1;
    spos++;

    for(; spos < line_length; spos++){
        auto c = line[spos];
        if((d == '\"' && c == '\"') || (d == '<' && c == '>')){
            return line.substr(dstart, spos - dstart);
        }
    }
    return std::nullopt;
}

std::vector<fs::path> read_dependencies(fs::path path, const std::vector<fs::path>& include_folders){
    std::ifstream strm(path.c_str());
    std::string line;
    std::vector<fs::path> dependencies;

    //std::cout << "reading " << path << "..." << std::endl;

    while (std::getline(strm, line))
    {
        // parse line
        if (auto include = read_include(line); include.has_value())
        {

            fs::path rel(include.value());
            rel = path.parent_path() / rel;
            rel = fs::absolute(rel).lexically_normal();

            if(fs::exists(rel)){
                dependencies.push_back(rel);
                continue;
            }

            for (auto& folder : include_folders)
            {
                fs::path target = folder / include.value();
                if (fs::exists(target))
                {
                    dependencies.push_back(target);
                }
            }
        }
    }
    strm.close();
    return dependencies;
}

void dependency_tree::add(std::filesystem::path file, const std::vector<fs::path>& include_folders)
{
    if (!fs::exists(file))
    {
        for (auto include_folder : include_folders)
        {
            if (fs::exists(include_folder / file))
            {
                file = include_folder / file;
                break;
            }
        }
    }
    if (!fs::exists(file))
    {
        return;
    }
    auto abs_file = fs::absolute(file).lexically_normal();
    if (_file_tree.find(abs_file.string()) != _file_tree.end())
    {
        return;
    }
    auto dependencies = read_dependencies(abs_file, include_folders);
    _file_tree.emplace(abs_file.string(), dependencies);
    for (const auto& dep : dependencies)
    {
        add(dep, include_folders);
    }
}

bool dependency_tree::need_rebuild(std::filesystem::path source, std::filesystem::file_time_type timestamp)
{
    std::unordered_set<std::string> ignore;
    return need_rebuild(source, timestamp, ignore);
}

bool dependency_tree::need_rebuild(std::filesystem::path source, std::filesystem::file_time_type timestamp, std::unordered_set<std::string>& ignore)
{
    if (fs::last_write_time(source) > timestamp)
    {
        //std::cout << source << " changed" << std::endl;
        return true;
    }
    ignore.emplace(source.string());

    auto it = _file_tree.find(source.string());
    if (it != _file_tree.end())
    {
        for (auto& dep : it->second)
        {
            if (need_rebuild(dep, timestamp))
            {
                return true;
            }
            ignore.emplace(dep.string());
        }
    }
    return false;
}
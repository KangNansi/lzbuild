#include "file.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <chrono>
#include <ctime>

using namespace std;
using uint = unsigned int;

file::file(fs::path path, fs::path workspace_folder, fs::path source_path){
    this->path = path;
    this->workspace_folder = workspace_folder;
    this->source_path = source_path;

    if(path.extension() == ".cpp" || path.extension() == ".c"){
        type = FILE_TYPE::SOURCE;
    }
    else if(path.extension() == ".hpp" || path.extension() == ".h"){
        type = FILE_TYPE::HEADER;
    }
    
    if(type == FILE_TYPE::SOURCE){
        fs::path relative_path = fs::relative(path, workspace_folder);
        relative_path.replace_extension(".o");
        object_path = workspace_folder / "obj" / relative_path;
    }

    last_write = fs::last_write_time(path);
    compute_dependencies();
}

bool file::read_include(const std::string& line, std::string& path)
{
    const std::string includeText = "#include";
    size_t spos = line.find(includeText);
    size_t line_length = line.length();
    if(spos == string::npos){
        return false;
    }
    spos += includeText.length();
    
    for(; spos < line_length; spos++){
        auto c = line[spos];
        if(c == ' ') continue;
        else if(c == '\"' || c == '<') break;
        else return false;
    }
    if(spos >= line_length) return false;

    char d = line[spos];
    int dstart = spos + 1;
    spos++;

    for(; spos < line_length; spos++){
        auto c = line[spos];
        if((d == '\"' && c == '\"') || (d == '<' && c == '>')){
            path = line.substr(dstart, spos - dstart);
            return true;
        }
    }
    return false;
}

void file::compute_dependencies(){
    ifstream strm(path.c_str());
    std::string line;

    while (std::getline(strm, line))
    {
        // parse line
        std::string include;
        if(read_include(line, include)){

            fs::path rel(include);
            rel = path.parent_path() / rel;

            if(fs::exists(rel)){
                dependencies.push_back(rel);
                continue;
            }

            fs::path abs = fs::current_path() / "src" / fs::path(include);
            if(fs::exists(abs)){
                dependencies.push_back(abs);
            }
        }
    }
    strm.close();
}

bool file::need_rebuild(const std::vector<file>& files) const{
    if(!fs::exists(object_path)){
        return true;
    }

    std::set<std::filesystem::path> checked;
    auto obj_last_write = fs::last_write_time(object_path);
    return rebuild_check(obj_last_write, files, checked);
}

bool file::rebuild_check(std::filesystem::file_time_type last_write, const std::vector<file>& files, std::set<fs::path>& checked) const{

    if(fs::last_write_time(path) > last_write){
        return true;
    }

    checked.insert(path);

    for(auto& d: dependencies){
        for(auto& f : files){
            if(auto it{checked.find(f.path)}; it != checked.end()){
                continue;
            }
            if(fs::equivalent(f.path, d) && f.rebuild_check(last_write, files, checked)){
                return true;
            }
        }
    }

    return false;
}

std::string file::get_last_write_time_string() const {
    auto stp = chrono::time_point_cast<chrono::system_clock::duration>(last_write - chrono::file_clock::now() + chrono::system_clock::now());
    auto t = chrono::system_clock::to_time_t(stp);
    return ctime(&t);
}

std::ostream& operator<<(std::ostream &strm, const file &file){
  strm << file.path << '\t';
  switch (file.type)
  {
    case FILE_TYPE::HEADER:
        strm << "HEADER";
        break;
    case FILE_TYPE::SOURCE:
        strm << "SOURCE";
        break;
  
    default:
        break;
  }
  strm << '\t' << file.get_last_write_time_string();
  for(auto& d: file.dependencies){
      strm << '\t' << "path: " << d << '\n';
  }
  strm.flush();
  return strm;
}
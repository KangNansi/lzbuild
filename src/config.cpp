#include "config.hpp"
#include <fstream>
#include <sstream>
#include <map>
#include <iostream>

using namespace std;
namespace fs = std::filesystem;

bool is_alpha(char c){
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

std::string read_value(string& text, size_t& cursor){
    if(text[cursor] != '"'){
        throw "Parse error: '\"' expected";
    }
    cursor++;
    int start = cursor;
    while (cursor < text.length() && text[cursor] != '"')
    {
        cursor++;
    }
    if(text[cursor] != '"'){
        throw "Parse error: '\"' expected";
    }
    std::string value = text.substr(start, cursor - start);
    cursor++;
    return value;
}

std::string read_name(string& text, size_t& cursor){
    if(text[cursor] == '"'){
        return read_value(text, cursor);
    }
    int start = cursor;
    while (cursor < text.length() && is_alpha(text[cursor]))
    {
        cursor++;
    }
    return text.substr(start, cursor - start);
}

void pass_space(string& text, size_t &cursor){
    while (cursor < text.length())
    {
        char c = text[cursor];
        if(is_alpha(c) || c == '=' || c == '"'){
            return;
        }
        cursor++;
    }
}

struct kvp{
    std::string key;
    std::string value;
};

kvp read_kvp(string& text, size_t& cursor){
    pass_space(text, cursor);
    kvp result;
    result.key = read_name(text, cursor);
    pass_space(text, cursor);
    if(text[cursor] != '='){
        throw "Parse error: '=' expected.";
    }
    cursor++;
    pass_space(text, cursor);
    result.value = read_name(text, cursor);
    return result;
}

void parse(ifstream& file, map<string, string>& result){
    stringstream ss;
    ss << file.rdbuf();
    string text = ss.str();

    size_t cursor = 0;
    while (cursor < text.length())
    {
        auto kvp = read_kvp(text, cursor);
        result[kvp.key] = kvp.value;
    }
}

const string get_value(const map<string, string>& map, const string& key, const string& default_value){
    auto it = map.find(key);
    if(it != map.end()){
        return (*it).second;
    }
    else{
        return default_value;
    }
}

void get_values(const map<string, string>& map, const string& key, vector<string> &target){
    auto it = map.find(key);
    if(it != map.end()){
        auto& val = (*it).second;
        int start = 0;
        for(size_t i = 0; i <= val.length(); i++){
            if(i == val.length() || val[i] == ';'){
                target.push_back(val.substr(start, i - start));
                start = i+1;
            }
        }
    }
}

void read_config(config& config, std::filesystem::path path){
    map<string, string> value_map;
    if(fs::exists(path)){
        ifstream file(path);
        try{
            parse(file, value_map);
        }
        catch(exception& exception){
            cerr << exception.what() << endl;
        }
        catch(const char* exception){
            cerr << exception << endl;
        }
        file.close();
    }

    config.export_path = get_value(value_map, "export_path", "");
    config.name = get_value(value_map, "name", "result");
    config.cflags = get_value(value_map, "cflags", "");
    get_values(value_map, "include", config.include_folder);
    get_values(value_map, "libraries", config.libraries);
    get_values(value_map, "libpaths", config.library_paths);
    get_values(value_map, "sources", config.source_folders);
    if(config.source_folders.size() == 0){
        config.source_folders.push_back("src");
    }
    if(get_value(value_map, "is_library", "false") == "true"){
        config.is_library = true;
    }
    config.link_etc = get_value(value_map, "link_etc", "");
}
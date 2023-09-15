#include "config.hpp"
#include <fstream>
#include <sstream>
#include <map>
#include <iostream>
#include <stack>
#include "expression.hpp"

using namespace std;
namespace fs = std::filesystem;

struct kvp{
    std::string key;
    std::string value;
};


bool match_platform(std::string platform)
{
#ifdef WIN32
    return platform == "WIN32";
#elif __linux__
    return platform == "__linux__";
#else
    return false;
#endif
}

struct config_creator : public lzlang::expr_visitor
{
    map<std::string, std::string>& _ctx;

    config_creator(map<std::string, std::string>& ctx) : _ctx(ctx) {}

    void assign(tokenizer::token name, tokenizer::token value)
    {
        _ctx[name.value] = value.value.substr(1, value.value.size() - 2);
    }
    void _if(tokenizer::token condition, lzlang::expression& _true, lzlang::expression* _false)
    {
        if (match_platform(condition.value))
        {
            _true.visit(*this);
        }
        else if(_false != nullptr)
        {
            _false->visit(*this);
        }
    }
};

void parse(ifstream& file, map<string, string>& result){
    stringstream ss;
    ss << file.rdbuf();
    string text = ss.str();

    auto expr = lzlang::parse(text);
    config_creator creator(result);
    expr->visit(creator);
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

void read_config(config& config, std::filesystem::path path)
{
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

    config.executable_export_path = get_value(value_map, "export_path", "");
    config.include_export_path = get_value(value_map, "include_export_path", "");
    config.name = get_value(value_map, "name", "result");
    config.compiler = get_value(value_map, "compiler", "g++");
    config.standard = get_value(value_map, "std", "c++20");
    config.cflags = get_value(value_map, "cflags", "");
    get_values(value_map, "include", config.include_folder);
    get_values(value_map, "libraries", config.libraries);
    get_values(value_map, "libpaths", config.library_paths);
    get_values(value_map, "sources", config.source_folders);
    get_values(value_map, "exclude", config.exclude);
    if (config.source_folders.size() == 0)
    {
        config.source_folders.push_back("src");
    }
    if(get_value(value_map, "is_library", "false") == "true"){
        config.is_library = true;
    }
#ifdef WIN32
    auto bin_ext = ".exe";
    auto lib_ext = ".lib";
#else
    auto bin_ext = "";
    auto lib_ext = ".a";
#endif
    
    config.output_extension = get_value(value_map, "output_extension", config.is_library ? lib_ext: bin_ext);
    config.link_etc = get_value(value_map, "link_etc", "");
}
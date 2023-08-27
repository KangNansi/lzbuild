#include "config.hpp"
#include <fstream>
#include <sstream>
#include <map>
#include <iostream>
#include "tokenizer/tokenizer.hpp"
#include "tokenizer/extensions.hpp"

using namespace std;
namespace fs = std::filesystem;

struct kvp{
    std::string key;
    std::string value;
};

bool match(tokenizer::token token, short type, std::string value)
{
    return token.type == type && token.value == value;
}

void parse(ifstream& file, map<string, string>& result){
    stringstream ss;
    ss << file.rdbuf();
    string text = ss.str();

    tokenizer::engine token_engine;
    short keyword = token_engine.add<tokenizer::keyword_matcher>("if", "else");
    short syntax = token_engine.add("{}=");
    short name = token_engine.add("[@_][@_#]*");
    short comment = token_engine.add<tokenizer::line_comment_matcher>("#");
    short string_literal = token_engine.add<tokenizer::string_literal_matcher>();

    auto tokens = token_engine.tokenize(text);

    for (int i = 0; i < tokens.size(); i++)
    {
        int remaining = tokens.size() - i;
        auto current = tokens[i];
        short type = tokens[i].type;
        if (remaining > 3 && match(current, keyword, "if"))
        {
            //TODO
        }
        else if (remaining > 3 && current.type == name && match(tokens[i+1], syntax, "=") && tokens[i+2].type == string_literal)
        {
            result[current.value] = tokens[i + 2].value;
            i += 2;
        }
        else if (current.type == comment)
        {
            // nothing
        }
        else
        {
            throw "Unexpected token";
        }
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

    config.export_path = get_value(value_map, "export_path", "");
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
#else
    auto bin_exe = "";
#endif
    
    config.output_extension = get_value(value_map, "output_extension", config.is_library ? ".lib" : bin_exe);
    config.link_etc = get_value(value_map, "link_etc", "");
}
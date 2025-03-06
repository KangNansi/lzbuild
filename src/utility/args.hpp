#pragma once
#include <filesystem>
#include <unordered_map>
#include <string>
#include <vector>
#include <iostream>

class ArgReader{
    private:
    std::string binary_dir;
    std::unordered_map<std::string, int> arg_map;
    std::vector<std::string> args;

    public:
    ArgReader(int argc, char** argv) {
        if(argv[0][0] == '.')
        {
            binary_dir = std::filesystem::absolute(argv[0]);
        }
        else
        {
            binary_dir = argv[0];
        }
        for(int i = 1; i < argc; i++){
            arg_map[std::string(argv[i])] = i;
            args.push_back(argv[i]);
        }
    }

    bool has(std::string arg) const{
        auto it = arg_map.find(arg);
        return it != arg_map.end();
    }

    bool get(std::string flag, std::string& arg) const {
        auto it = arg_map.find(flag);
        if(it != arg_map.end() && (size_t)it->second < args.size()){
            arg = args[it->second];
            return true;
        }
        return false;
    }

    bool is(size_t index, std::string value) { return index < args.size() && args[index] == value; }

    std::string get_binary_dir() { return binary_dir; }
};


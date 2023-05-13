#pragma once
#include <unordered_map>
#include <string>

class ArgReader{
    private:
    std::unordered_map<std::string, int> arg_map;
    std::vector<std::string> args;

    public:
    ArgReader(int argc, char** argv) {
        for(int i = 0; i < argc; i++){
            arg_map[std::string(argv[i])] = i;
            args.push_back(argv[i]);
        }
    }

    bool has(std::string arg) const{
        auto it = arg_map.find(arg);
        return it != arg_map.end();
    }

    bool get(std::string flag, std::string& arg) {
        auto it = arg_map.find(flag);
        if(it != arg_map.end() && (size_t)it->second + 1 < args.size()){
            arg = args[it->second + 1];
            return true;
        }
        return false;
    }
};


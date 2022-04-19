#pragma once
#include <unordered_map>
#include <string>

class ArgReader{
    private:
    std::unordered_map<std::string, int> arg_map;

    public:
    ArgReader(int argc, char** argv) {
        for(int i = 0; i < argc; i++){
            arg_map[std::string(argv[i])] = i;
        }
    }

    bool has(std::string arg) const{
        auto it = arg_map.find(arg);
        return it != arg_map.end();
    }
};


#include "config.hpp"
#include <fstream>

using namespace std;
namespace fs = std::filesystem;

std::string read_text(std::string& line, int start){
    int s = -1, f = -1;
    for(int i = start; i < line.length(); i++){
        if(s == -1){
            if(line[i] == '"'){
                s = i;
            }
        }
        else{
            if(line[i] == '"'){
                return line.substr(s+1, i - s - 1);
            }
        }
    }
    return "";
}

void read_config(config& config, std::filesystem::path path){
    ifstream file(path);
    string line;

    while(std::getline(file, line)){
        int eq_pos = line.find('=');
        string name = line.substr(0, eq_pos);
        if(name == "export_path"){
            config.export_path = fs::path(read_text(line, eq_pos+1));
        }
    }

    file.close();
}
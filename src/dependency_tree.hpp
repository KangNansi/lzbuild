#pragma once
#include <unordered_map>
#include <filesystem>
#include <vector>
#include <unordered_set>
#include <optional>

class dependency_tree
{
    std::unordered_map<std::filesystem::path, std::vector<std::filesystem::path>> _file_tree;

public:
    void add(std::filesystem::path file, const std::vector<std::filesystem::path>& include_folders);
    bool need_rebuild(std::filesystem::path source, std::filesystem::file_time_type timestamp);

private:
    bool need_rebuild(std::filesystem::path source, std::filesystem::file_time_type timestamp, std::unordered_set<std::filesystem::path>& ignore);
};
#pragma once
#include <filesystem>

#if _WIN32
const std::filesystem::path APP_DATA = std::string(std::getenv("LOCALAPPDATA")) + "/lzbuild";
const std::filesystem::path INCLUDE_EXPORT_PATH = APP_DATA / "include";
const std::filesystem::path LIBRARY_EXPORT_PATH = APP_DATA / "lib";
const std::filesystem::path BINARY_EXPORT_PATH = APP_DATA / "bin";
const std::filesystem::path ASSET_EXPORT_PATH = APP_DATA / "share";
#else
const std::filesystem::path APP_DATA = std::string(std::getenv("HOME")) + "/.lzbuild";
const std::filesystem::path INCLUDE_EXPORT_PATH = "/usr/local/include";
const std::filesystem::path LIBRARY_EXPORT_PATH = "/usr/local/lib";
const std::filesystem::path BINARY_EXPORT_PATH = "/usr/local/bin";
const std::filesystem::path ASSET_EXPORT_PATH = "/usr/local/share";
#endif
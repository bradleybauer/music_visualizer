#pragma once

#ifdef WINDOWS
#include <filesystem>
namespace filesys = std::filesystem;
#else
#include <experimental/filesystem>
namespace filesys = std::experimental::filesystem;
#endif

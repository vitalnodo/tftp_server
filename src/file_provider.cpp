#include "file_provider.hpp"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

LocalFileProvider::LocalFileProvider(std::string root_dir)
    : root_dir_(fs::canonical(root_dir).string()) {}

std::unique_ptr<std::istream> LocalFileProvider::open(const std::string& filename) {
    if (!is_valid_path(filename))
        return nullptr;

    auto root     = fs::path(root_dir_);
    auto full     = fs::weakly_canonical(root / filename);
    auto relative = full.lexically_relative(root);

    // reject path traversal: relative path must not escape root
    if (relative.empty() || *relative.begin() == "..")
        return nullptr;

    auto stream = std::make_unique<std::ifstream>(full, std::ios::binary);
    if (!*stream)
        return nullptr;

    return stream;
}

std::unique_ptr<std::ostream> LocalFileProvider::create(const std::string& filename) {
    if (!is_valid_path(filename))
        return nullptr;

    auto root     = fs::path(root_dir_);
    auto full     = fs::weakly_canonical(root / filename);
    auto relative = full.lexically_relative(root);

    if (relative.empty() || *relative.begin() == "..")
        return nullptr;

    if (fs::exists(full))
        return nullptr;

    auto stream = std::make_unique<std::ofstream>(full, std::ios::binary);
    if (!*stream)
        return nullptr;

    return stream;
}

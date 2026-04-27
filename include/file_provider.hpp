#pragma once

#include <filesystem>
#include <istream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

// rejects absolute paths and any ".." components
inline bool is_valid_path(const std::string& filename) {
    std::filesystem::path p(filename);
    if (p.is_absolute())
        return false;
    for (const auto& component : p)
        if (component == "..")
            return false;
    return true;
}

class IFileProvider {
public:
    virtual ~IFileProvider() = default;
    // returns nullptr if file not found or access denied
    virtual std::unique_ptr<std::istream> open(const std::string& filename) = 0;
};

// Serves files from root_dir; rejects path traversal attempts (e.g. ../../etc/passwd)
class LocalFileProvider : public IFileProvider {
public:
    explicit LocalFileProvider(std::string root_dir);
    std::unique_ptr<std::istream> open(const std::string& filename) override;

private:
    std::string root_dir_;
};

class MockFileProvider : public IFileProvider {
public:
    void add_file(const std::string& name, const std::vector<uint8_t>& data) {
        files_[name] = std::string(data.begin(), data.end());
    }

    std::unique_ptr<std::istream> open(const std::string& filename) override {
        if (!is_valid_path(filename))
            return nullptr;
        auto it = files_.find(filename);
        if (it == files_.end())
            return nullptr;
        return std::make_unique<std::istringstream>(it->second, std::ios::binary);
    }

private:
    std::map<std::string, std::string> files_;
};

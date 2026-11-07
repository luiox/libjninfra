#pragma once

/// @defgroup jninfra-dump-class-file jninfra-dump-class-file
/// @brief JVMTI ClassFileLoadHook 捕获的类字节视图与落盘 sink。
///

#include <filesystem>
#include <string>

#include "jninfra/error.hpp"
#include "libca/core/result.hpp"

namespace jninfra::dump {

/// @brief Borrowed class bytes received from ClassFileLoadHook.
struct ClassFileView
{
    std::string          name;
    std::string          module;
    const unsigned char* data = nullptr;
    ca::usize            size = 0;

    /// @return true when the view has a name and non-empty byte payload.
    bool valid() const noexcept { return !name.empty() && data != nullptr && size != 0; }
};

/// @brief File-system sink that writes JVM internal names as `.class` files.
class FileClassSink
{
public:
    enum class GroupBy
    {
        None,
        Package,
        Module,
    };

    explicit FileClassSink(std::string output_dir);
    FileClassSink(std::string output_dir, GroupBy group_by);

    /// @return Root output directory.
    const std::string& output_dir() const noexcept { return output_dir_; }

    /// @brief Write one class payload to `<output>/<name>.class`.
    ca::Result<void, NativeError> write(const ClassFileView& view) const;

private:
    std::string output_dir_;
    GroupBy     group_by_ = GroupBy::None;
};

/// @brief Parse CLI/agent grouping labels: none, package, module.
FileClassSink::GroupBy parse_file_group_by(const std::string& value);

/// @brief Build the relative output path used for one class view.
ca::Result<std::filesystem::path, NativeError> class_output_relative_path(
    const ClassFileView& view, FileClassSink::GroupBy group_by);

}   // namespace jninfra::dump

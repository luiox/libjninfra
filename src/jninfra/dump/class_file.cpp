#include "jninfra/dump/class_file.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <utility>

namespace jninfra::dump {
namespace {

bool unsafe_path_segment(const std::string& segment)
{
    return segment.empty() || segment == "." || segment == ".." ||
           segment.find('\\') != std::string::npos || segment.find(':') != std::string::npos;
}

ca::Result<void, NativeError> append_class_path(std::filesystem::path& path,
                                                const std::string&     class_name)
{
    if (class_name.empty() || class_name.front() == '/' || class_name.front() == '\\') {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument, "invalid class name"));
    }

    std::size_t start = 0;
    while (start <= class_name.size()) {
        std::size_t end = class_name.find('/', start);
        std::string segment =
            class_name.substr(start, end == std::string::npos ? end : end - start);
        if (unsafe_path_segment(segment)) {
            return ca::Err(
                make_error(NativeErrorCode::kInvalidArgument, "unsafe class name: " + class_name));
        }
        if (end == std::string::npos) {
            path /= segment + ".class";
            break;
        }
        path /= segment;
        start = end + 1;
    }
    return ca::Ok();
}

ca::Result<void, NativeError> append_directory_path(std::filesystem::path& path,
                                                    const std::string&     relative_path,
                                                    const char*            label)
{
    if (relative_path.empty() || relative_path.front() == '/' || relative_path.front() == '\\') {
        return ca::Err(
            make_error(NativeErrorCode::kInvalidArgument, std::string("invalid ") + label));
    }
    std::size_t start = 0;
    while (start <= relative_path.size()) {
        std::size_t end = relative_path.find('/', start);
        std::string segment =
            relative_path.substr(start, end == std::string::npos ? end : end - start);
        if (unsafe_path_segment(segment)) {
            return ca::Err(make_error(NativeErrorCode::kInvalidArgument,
                                      std::string("unsafe ") + label + ": " + relative_path));
        }
        path /= segment;
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return ca::Ok();
}

std::string package_name(const std::string& class_name)
{
    const auto slash = class_name.rfind('/');
    if (slash == std::string::npos) {
        return "_default";
    }
    return class_name.substr(0, slash);
}

std::string simple_class_name(const std::string& class_name)
{
    const auto slash = class_name.rfind('/');
    if (slash == std::string::npos) {
        return class_name;
    }
    return class_name.substr(slash + 1);
}

std::string module_name_or_unknown(const std::string& module)
{
    return module.empty() ? "unknown" : module;
}

ca::Result<void, NativeError> append_safe_segment(std::filesystem::path& path,
                                                  const std::string& segment, const char* label)
{
    if (unsafe_path_segment(segment) || segment.find('/') != std::string::npos) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument,
                                  std::string("unsafe ") + label + ": " + segment));
    }
    path /= segment;
    return ca::Ok();
}

}   // namespace

FileClassSink::FileClassSink(std::string output_dir)
    : output_dir_(std::move(output_dir))
{}

FileClassSink::FileClassSink(std::string output_dir, GroupBy group_by)
    : output_dir_(std::move(output_dir))
    , group_by_(group_by)
{}

ca::Result<void, NativeError> FileClassSink::write(const ClassFileView& view) const
{
    if (!view.valid()) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument, "class file view is invalid"));
    }
    if (output_dir_.empty()) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument, "output directory is empty"));
    }

    auto path_result = class_output_relative_path(view, group_by_);
    if (path_result.is_err()) {
        return ca::Err(path_result.unwrap_err());
    }

    const auto      path = std::filesystem::path(output_dir_) / path_result.unwrap();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        return ca::Err(make_error(NativeErrorCode::kIoFailure,
                                  "failed to create dump directory: " + ec.message()));
    }

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return ca::Err(make_error(NativeErrorCode::kIoFailure,
                                  "failed to open class dump file: " + path.string()));
    }
    out.write(reinterpret_cast<const char*>(view.data), static_cast<std::streamsize>(view.size));
    if (!out) {
        return ca::Err(make_error(NativeErrorCode::kIoFailure,
                                  "failed to write class dump file: " + path.string()));
    }
    return ca::Ok();
}

FileClassSink::GroupBy parse_file_group_by(const std::string& value)
{
    std::string normalized = value;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (normalized == "package") {
        return FileClassSink::GroupBy::Package;
    }
    if (normalized == "module") {
        return FileClassSink::GroupBy::Module;
    }
    return FileClassSink::GroupBy::None;
}

ca::Result<std::filesystem::path, NativeError> class_output_relative_path(
    const ClassFileView& view, FileClassSink::GroupBy group_by)
{
    if (!view.valid()) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument, "class file view is invalid"));
    }

    std::filesystem::path path;
    if (group_by == FileClassSink::GroupBy::Package) {
        path /= "_by_package";
        auto package_result = append_directory_path(path, package_name(view.name), "package name");
        if (package_result.is_err()) {
            return ca::Err(package_result.unwrap_err());
        }
        const std::string simple = simple_class_name(view.name);
        if (unsafe_path_segment(simple)) {
            return ca::Err(
                make_error(NativeErrorCode::kInvalidArgument, "unsafe class name: " + view.name));
        }
        path /= simple + ".class";
        return ca::Ok(std::move(path));
    }
    if (group_by == FileClassSink::GroupBy::Module) {
        path /= "_by_module";
        auto module_result =
            append_safe_segment(path, module_name_or_unknown(view.module), "module name");
        if (module_result.is_err()) {
            return ca::Err(module_result.unwrap_err());
        }
        auto class_result = append_class_path(path, view.name);
        if (class_result.is_err()) {
            return ca::Err(class_result.unwrap_err());
        }
        return ca::Ok(std::move(path));
    }

    auto class_result = append_class_path(path, view.name);
    if (class_result.is_err()) {
        return ca::Err(class_result.unwrap_err());
    }
    return ca::Ok(std::move(path));
}

}   // namespace jninfra::dump

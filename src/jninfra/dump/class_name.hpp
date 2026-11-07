#pragma once

/// @defgroup jninfra-dump-class-name jninfra-dump-class-name
/// @brief JVM 内部斜线名与点分名的归一化工具。
///

#include <string>

namespace jninfra::dump {

/// @brief Convert a JVMTI class signature to an internal JVM class name.
///
/// Object signatures such as `Ljava/lang/String;` become `java/lang/String`.
/// Array and primitive signatures return an empty string because they do not map
/// to a writable `.class` file path.
std::string class_name_from_signature(const char* signature);

/// @return true when @p class_name belongs to common JDK/internal packages.
bool is_jdk_class_name(const std::string& class_name);

}   // namespace jninfra::dump

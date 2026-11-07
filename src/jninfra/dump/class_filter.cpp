#include "jninfra/dump/class_filter.hpp"

#include "jninfra/dump/class_name.hpp"

#include <regex>
#include <utility>

namespace jninfra::dump {
namespace {

bool matches_pattern(const std::string& class_name, const std::string& pattern)
{
    if (pattern.size() >= 3 && pattern.compare(pattern.size() - 3, 3, "/**") == 0) {
        const std::string prefix = pattern.substr(0, pattern.size() - 3);
        return class_name.compare(0, prefix.size(), prefix) == 0;
    }
    return class_name == pattern;
}

bool matches_any(const std::string& class_name, const std::vector<std::string>& patterns)
{
    for (const auto& pattern : patterns) {
        if (matches_pattern(class_name, pattern)) {
            return true;
        }
    }
    return false;
}

std::string dot_form(const std::string& class_name)
{
    std::string value = class_name;
    for (char& ch : value) {
        if (ch == '/') {
            ch = '.';
        }
    }
    return value;
}

bool matches_regex_any(const std::string& slash_name, const std::string& dot_name,
                       const std::vector<std::regex>& exprs)
{
    for (const auto& expr : exprs) {
        if (std::regex_search(slash_name, expr) || std::regex_search(dot_name, expr)) {
            return true;
        }
    }
    return false;
}

}   // namespace

ClassNameFilter::ClassNameFilter(ClassNameFilterConfig config)
    : config_(std::move(config))
{
    auto compile = [](const std::vector<std::string>&     patterns,
                      std::vector<std::regex>&            exprs) {
        exprs.clear();
        exprs.reserve(patterns.size());
        for (const auto& pattern : patterns) {
            try {
                exprs.emplace_back(pattern);
            }
            catch (const std::regex_error&) {
                // Keep filtering best-effort inside JVMTI callbacks. The caller can
                // still inspect the session metadata to see which regex was passed.
            }
        }
    };
    compile(config_.include_regexes, include_regex_exprs_);
    compile(config_.exclude_regexes, exclude_regex_exprs_);
}

bool ClassNameFilter::matches(const std::string& class_name) const
{
    if (class_name.empty()) {
        return false;
    }
    if (config_.exclude_jdk && is_jdk_class_name(class_name)) {
        return false;
    }
    // has_includes / exclude 门控按配置列表判定而非编译后 exprs：仅含无效正则的
    // include 配置必须保持"恒不匹配 → 全排除"语义（见 IgnoresInvalidRegexRules）。
    const std::string dot_name     = dot_form(class_name);
    const bool        has_includes = !config_.includes.empty() || !config_.include_regexes.empty();
    const bool        include_match =
        matches_any(class_name, config_.includes) ||
        matches_regex_any(class_name, dot_name, include_regex_exprs_);
    if (has_includes && !include_match) {
        return false;
    }
    if (!config_.excludes.empty() && matches_any(class_name, config_.excludes)) {
        return false;
    }
    if (!config_.exclude_regexes.empty() &&
        matches_regex_any(class_name, dot_name, exclude_regex_exprs_)) {
        return false;
    }
    return true;
}

std::vector<std::string> split_filter_patterns(const char* patterns)
{
    std::vector<std::string> result;
    if (patterns == nullptr || patterns[0] == '\0') {
        return result;
    }

    std::string value(patterns);
    std::size_t start = 0;
    while (start <= value.size()) {
        std::size_t end  = value.find(',', start);
        std::string item = value.substr(start, end == std::string::npos ? end : end - start);
        if (!item.empty()) {
            result.push_back(std::move(item));
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return result;
}

std::vector<ClassNameFilterDiagnostic> validate_class_name_filter_config(
    const ClassNameFilterConfig& config)
{
    std::vector<ClassNameFilterDiagnostic> diagnostics;
    auto validate_regexes = [&](const char* field, const std::vector<std::string>& patterns) {
        for (const auto& pattern : patterns) {
            try {
                const std::regex expr(pattern);
                (void)expr;
            }
            catch (const std::regex_error& error) {
                diagnostics.push_back({field, pattern, error.what()});
            }
        }
    };

    validate_regexes("includeRegex", config.include_regexes);
    validate_regexes("excludeRegex", config.exclude_regexes);
    return diagnostics;
}

}   // namespace jninfra::dump

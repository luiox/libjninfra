#pragma once

/// @defgroup jninfra-dump-filter jninfra-dump-filter
/// @brief dump 共享类名过滤策略：include/exclude 子树、精确名与正则的固定求值序。
///
/// 求值序与容错契约见 doc:design-dump-filter。

#include <regex>
#include <string>
#include <vector>

namespace jninfra::dump {

/// @brief Include/exclude policy for JVM internal class names.
///
/// Pattern entries use slash-form JVM names such as `com/example/Foo`.
/// Entries ending in `/**` match a whole package subtree, while other entries
/// match a single class exactly. Regular-expression entries are evaluated
/// against both slash-form and dot-form class names so CLI users can write the
/// style that is natural for them.
struct ClassNameFilterConfig
{
    std::vector<std::string> includes;
    std::vector<std::string> excludes;
    std::vector<std::string> include_regexes;
    std::vector<std::string> exclude_regexes;
    bool                     exclude_jdk = true;
};

/// @brief 过滤规则诊断项。
///
/// 调用方可在 CLI preflight/inspect 阶段提前报告无效规则。JVMTI 回调路径仍
/// 保持 best-effort,避免运行时 callback 因用户输入中断目标 JVM。
struct ClassNameFilterDiagnostic
{
    std::string field;
    std::string pattern;
    std::string message;
};

/// @brief Matches JVM internal class names against exact, subtree, and regex rules.
///
/// A pattern ending in `/**` matches the whole subtree under that prefix.
/// Other non-regex patterns match exact class names. Regex patterns are optional
/// include/exclude gates and invalid regex entries are ignored so a bad CLI
/// pattern cannot break JVMTI callback processing.
class ClassNameFilter
{
public:
    ClassNameFilter() = default;
    explicit ClassNameFilter(ClassNameFilterConfig config);

    /// @return true when @p class_name should be captured.
    bool matches(const std::string& class_name) const;

private:
    ClassNameFilterConfig config_;
    /// 预编译正则（构造时编译一次，matches() 只做 regex_search）。
    /// 原实现每次 matches() 每条 pattern 重新构造 std::regex——配置了
    /// includeRegex/excludeRegex 时这是每类加载一次的正则编译开销。
    /// 无效条目沿用 best-effort 跳过语义（与 validate_… 一致）。
    std::vector<std::regex> include_regex_exprs_;
    std::vector<std::regex> exclude_regex_exprs_;
};

/// @brief Split a comma-separated pattern string into filter entries.
std::vector<std::string> split_filter_patterns(const char* patterns);

/// @brief 校验过滤规则中可提前发现的问题。
/// @param config 待校验的过滤配置。
/// @return 诊断项列表;为空表示规则可被过滤器接受。
std::vector<ClassNameFilterDiagnostic> validate_class_name_filter_config(
    const ClassNameFilterConfig& config);

}   // namespace jninfra::dump

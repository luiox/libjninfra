#include "jninfra/dump/class_file.hpp"
#include "jninfra/dump/class_filter.hpp"
#include "jninfra/dump/class_name.hpp"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <utility>
#include <vector>

#include "test_helper.hpp"

using jninfra::NativeErrorCode;
using jninfra::dump::ClassFileView;
using jninfra::dump::ClassNameFilter;
using jninfra::dump::ClassNameFilterConfig;
using jninfra::dump::FileClassSink;
using jninfra::dump::class_output_relative_path;
using jninfra::dump::class_name_from_signature;
using jninfra::dump::split_filter_patterns;
using jninfra::dump::validate_class_name_filter_config;

TEST(DumpClassNameTest, ConvertsObjectSignaturesToInternalNames)
{
    EXPECT_EQ(class_name_from_signature("Ljava/lang/String;"), "java/lang/String");
    EXPECT_EQ(class_name_from_signature("Lcom/example/Foo$Bar;"), "com/example/Foo$Bar");
    EXPECT_TRUE(class_name_from_signature("[Ljava/lang/String;").empty());
    EXPECT_TRUE(class_name_from_signature("I").empty());
    EXPECT_TRUE(class_name_from_signature(nullptr).empty());
}

TEST(DumpClassFilterTest, AppliesJdkIncludeAndExcludeRules)
{
    ClassNameFilterConfig config;
    config.includes    = split_filter_patterns("com/example/**,org/acme/Exact");
    config.excludes    = split_filter_patterns("com/example/internal/**");
    config.exclude_jdk = true;

    ClassNameFilter filter(std::move(config));
    EXPECT_TRUE(filter.matches("com/example/Foo"));
    EXPECT_TRUE(filter.matches("org/acme/Exact"));
    EXPECT_FALSE(filter.matches("org/acme/Other"));
    EXPECT_FALSE(filter.matches("com/example/internal/Hidden"));
    EXPECT_FALSE(filter.matches("java/lang/String"));
}

TEST(DumpClassFilterTest, AppliesRegexRulesToSlashAndDotNames)
{
    ClassNameFilterConfig config;
    config.include_regexes = split_filter_patterns("com[/.]example[/.].*Service");
    config.exclude_regexes = split_filter_patterns(".*Internal.*");
    config.exclude_jdk     = false;

    ClassNameFilter filter(std::move(config));
    EXPECT_TRUE(filter.matches("com/example/UserService"));
    EXPECT_TRUE(filter.matches("com/example/nested/OrderService"));
    EXPECT_FALSE(filter.matches("com/example/InternalService"));
    EXPECT_FALSE(filter.matches("com/example/UserController"));
}

TEST(DumpClassFilterTest, IgnoresInvalidRegexRules)
{
    ClassNameFilterConfig config;
    config.include_regexes = split_filter_patterns("[");
    config.exclude_regexes = split_filter_patterns("[");
    config.exclude_jdk     = false;

    ClassNameFilter filter(std::move(config));
    EXPECT_FALSE(filter.matches("com/example/Foo"));
}

TEST(DumpClassFilterTest, ReportsInvalidRegexRulesForPreflight)
{
    ClassNameFilterConfig config;
    config.include_regexes = split_filter_patterns("[");
    config.exclude_regexes = split_filter_patterns("ok.*,(");

    auto diagnostics = validate_class_name_filter_config(config);
    ASSERT_EQ(diagnostics.size(), 2u);
    EXPECT_EQ(diagnostics[0].field, "includeRegex");
    EXPECT_EQ(diagnostics[0].pattern, "[");
    EXPECT_EQ(diagnostics[1].field, "excludeRegex");
    EXPECT_EQ(diagnostics[1].pattern, "(");
}

TEST(DumpClassFileTest, WritesClassBytesUnderTestOutput)
{
    auto output_dir = test::getTempOutPath("libjninfra_dump");
    std::filesystem::remove_all(output_dir);

    const unsigned char bytes[] = {0xca, 0xfe, 0xba, 0xbe};
    FileClassSink       sink(output_dir.string());
    ClassFileView       view{"com/example/Foo", "", bytes, sizeof(bytes)};

    auto result = sink.write(view);
    ASSERT_TRUE(result.is_ok()) << result.unwrap_err().message;

    auto written_path = output_dir / "com" / "example" / "Foo.class";
    ASSERT_TRUE(std::filesystem::exists(written_path));

    std::ifstream in(written_path, std::ios::binary);
    ASSERT_TRUE(in);
    std::vector<unsigned char> actual((std::istreambuf_iterator<char>(in)),
                                      std::istreambuf_iterator<char>());
    ASSERT_EQ(actual.size(), sizeof(bytes));
    EXPECT_EQ(actual[0], 0xca);
    EXPECT_EQ(actual[3], 0xbe);
}

TEST(DumpClassFileTest, BuildsGroupedRelativePaths)
{
    const unsigned char bytes[] = {0xca, 0xfe, 0xba, 0xbe};
    ClassFileView       view{"com/example/Foo", "unnamed", bytes, sizeof(bytes)};

    auto plain = class_output_relative_path(view, FileClassSink::GroupBy::None);
    ASSERT_TRUE(plain.is_ok()) << plain.unwrap_err().message;
    EXPECT_EQ(plain.unwrap().generic_string(), "com/example/Foo.class");

    auto package_group = class_output_relative_path(view, FileClassSink::GroupBy::Package);
    ASSERT_TRUE(package_group.is_ok()) << package_group.unwrap_err().message;
    EXPECT_EQ(package_group.unwrap().generic_string(), "_by_package/com/example/Foo.class");

    auto module_group = class_output_relative_path(view, FileClassSink::GroupBy::Module);
    ASSERT_TRUE(module_group.is_ok()) << module_group.unwrap_err().message;
    EXPECT_EQ(module_group.unwrap().generic_string(), "_by_module/unnamed/com/example/Foo.class");
}

TEST(DumpClassFileTest, RejectsUnsafeClassNames)
{
    auto                output_dir = test::getTempOutPath("libjninfra_dump_unsafe");
    const unsigned char bytes[]    = {0xca};
    FileClassSink       sink(output_dir.string());
    ClassFileView       view{"com/../Bad", "", bytes, sizeof(bytes)};

    auto result = sink.write(view);
    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(result.unwrap_err().code, NativeErrorCode::kInvalidArgument);
}

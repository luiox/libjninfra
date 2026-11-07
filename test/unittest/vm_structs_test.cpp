#include "jninfra/hotspot/class_file_source.hpp"
#include "jninfra/hotspot/layout.hpp"
#include "jninfra/hotspot/metadata.hpp"
#include "jninfra/hotspot/method.hpp"

#include <cstddef>
#include <cstring>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

using jninfra::NativeErrorCode;
using jninfra::hotspot::ClassFileByteSourceKind;
using jninfra::hotspot::inspect_method_layout;
using jninfra::hotspot::inspect_class_file_byte_source;
using jninfra::hotspot::ConstantPoolView;
using jninfra::hotspot::MethodView;
using jninfra::hotspot::SymbolView;
using jninfra::hotspot::VMIntConstantEntry;
using jninfra::hotspot::VMIntConstantEntryData;
using jninfra::hotspot::VMLongConstantEntry;
using jninfra::hotspot::VMLongConstantEntryData;
using jninfra::hotspot::VMStructEntry;
using jninfra::hotspot::VMStructEntryData;
using jninfra::hotspot::VMTypeEntry;
using jninfra::hotspot::VMTypeEntryData;
using jninfra::hotspot::VmStructSnapshot;
using jninfra::hotspot::VmStructTable;

namespace {

struct FakeSymbol
{
    unsigned short length;
    char           body[16];
};

struct FakeConstantPool
{
    int   length;
    void* entries[8];
};

struct FakeConstMethod
{
    FakeConstantPool* constants;
    unsigned short    name_index;
    unsigned short    signature_index;
    unsigned short    code_size;
    unsigned short    max_stack;
    unsigned short    max_locals;
    unsigned char     code[4];
};

struct FakeMethod
{
    FakeConstMethod* const_method;
    void*            i2i_entry;
    int              access_flags;
};

void fill_symbol(FakeSymbol& symbol, const char* text)
{
    const std::size_t size = std::strlen(text);
    symbol.length          = static_cast<unsigned short>(size);
    std::memcpy(symbol.body, text, size);
}

}   // namespace

TEST(VmStructTableTest, ResolvesInstanceAndStaticFields)
{
    int           static_value = 42;
    VMStructEntry structs[]    = {
        {"Fake", "instance_value", "int", 0, 4, nullptr},
        {"Fake", "static_value", "int", 1, 0, &static_value},
        {nullptr, nullptr, nullptr, 0, 0, nullptr},
    };
    VMTypeEntry types[] = {
        {"Fake", nullptr, 0, 0, 0, 8},
        {nullptr, nullptr, 0, 0, 0, 0},
    };
    VMIntConstantEntry ints[] = {
        {"SomeInt", 7},
        {nullptr, 0},
    };
    VMLongConstantEntry longs[] = {
        {"SomeLong", 9},
        {nullptr, 0},
    };

    VmStructTable table;
    ASSERT_TRUE(table.load(structs, types, ints, longs).is_ok());

    unsigned char object[8]             = {};
    *reinterpret_cast<int*>(object + 4) = 11;

    auto instance_result = table.read_field<int>("Fake", "instance_value", object);
    ASSERT_TRUE(instance_result.is_ok());
    EXPECT_EQ(instance_result.unwrap(), 11);

    auto static_result = table.read_field<int>("Fake", "static_value", nullptr);
    ASSERT_TRUE(static_result.is_ok());
    EXPECT_EQ(static_result.unwrap(), 42);
}

TEST(VmStructSnapshotTest, BuildsOwnedIndexFromCopiedRows)
{
    int static_value = 99;

    std::vector<VMStructEntryData> structs;
    structs.push_back({"Fake", "instance_value", "int", 0, 4, nullptr});
    structs.push_back({"Fake", "static_value", "int", 1, 0, &static_value});

    std::vector<VMTypeEntryData> types;
    types.push_back({"Fake", "", 0, 0, 0, 8});

    std::vector<VMIntConstantEntryData> ints;
    ints.push_back({"SomeInt", 7});

    std::vector<VMLongConstantEntryData> longs;
    longs.push_back({"SomeLong", 11});

    VmStructSnapshot snapshot;
    ASSERT_TRUE(
        snapshot.load(std::move(structs), std::move(types), std::move(ints), std::move(longs))
            .is_ok());
    EXPECT_EQ(snapshot.struct_count(), 2u);
    EXPECT_EQ(snapshot.type_count(), 1u);
    EXPECT_EQ(snapshot.int_constant_count(), 1u);
    EXPECT_EQ(snapshot.long_constant_count(), 1u);
    ASSERT_NE(snapshot.struct_entries(), nullptr);
    ASSERT_NE(snapshot.type_entries(), nullptr);
    EXPECT_STREQ(snapshot.struct_entries()[0].typeName, "Fake");
    EXPECT_STREQ(snapshot.struct_entries()[0].fieldName, "instance_value");
    EXPECT_STREQ(snapshot.type_entries()[0].typeName, "Fake");

    const VmStructTable& table = snapshot.table();
    ASSERT_NE(table.field("Fake", "instance_value"), nullptr);
    ASSERT_NE(table.type("Fake"), nullptr);
    ASSERT_NE(table.int_constant("SomeInt"), nullptr);
    ASSERT_NE(table.long_constant("SomeLong"), nullptr);

    unsigned char object[8]             = {};
    *reinterpret_cast<int*>(object + 4) = 17;

    auto instance_result = table.read_field<int>("Fake", "instance_value", object);
    ASSERT_TRUE(instance_result.is_ok());
    EXPECT_EQ(instance_result.unwrap(), 17);

    auto static_result = table.read_field<int>("Fake", "static_value", nullptr);
    ASSERT_TRUE(static_result.is_ok());
    EXPECT_EQ(static_result.unwrap(), 99);
}

TEST(ClassFileByteSourceTest, ReportsMissingCachedClassFileField)
{
    std::vector<VMStructEntryData> structs;
    structs.push_back({"ClassFileData", "_length", "u4", 0, 0, nullptr});
    structs.push_back({"ClassFileData", "_bytes", "u1*", 0, 4, nullptr});

    std::vector<VMTypeEntryData> types;
    types.push_back({"ClassFileData", "", 0, 0, 0, 8});

    std::vector<VMIntConstantEntryData>  ints;
    std::vector<VMLongConstantEntryData> longs;

    VmStructSnapshot snapshot;
    ASSERT_TRUE(
        snapshot.load(std::move(structs), std::move(types), std::move(ints), std::move(longs))
            .is_ok());

    const auto report = inspect_class_file_byte_source(snapshot.table());
    EXPECT_EQ(report.kind, ClassFileByteSourceKind::kNone);
    EXPECT_FALSE(report.cached_class_file_field_present);
    EXPECT_EQ(report.unavailable_reason, "InstanceKlass::_cached_class_file missing");
}

TEST(ClassFileByteSourceTest, InfersHotSpot8HiddenCachedClassFileSlot)
{
    std::vector<VMStructEntryData> structs;
    structs.push_back({"InstanceKlass", "_breakpoints", "BreakpointInfo*", 0, 344, nullptr});
    structs.push_back({"InstanceKlass", "_idnum_allocated_count", "u2", 0, 368, nullptr});

    std::vector<VMTypeEntryData> types;
    types.push_back({"InstanceKlass", "Klass", 0, 0, 0, 440});

    std::vector<VMIntConstantEntryData>  ints;
    std::vector<VMLongConstantEntryData> longs;

    VmStructSnapshot snapshot;
    ASSERT_TRUE(
        snapshot.load(std::move(structs), std::move(types), std::move(ints), std::move(longs))
            .is_ok());

    const auto report = inspect_class_file_byte_source(snapshot.table());
    EXPECT_EQ(report.kind, ClassFileByteSourceKind::kHotSpot8HiddenCachedClassFile);
    EXPECT_FALSE(report.cached_class_file_field_present);
    EXPECT_TRUE(report.cached_class_file_offset_inferred);
    EXPECT_EQ(report.cached_class_file_offset, 360);
    EXPECT_EQ(report.class_file_length_offset, 0);
    EXPECT_EQ(report.class_file_bytes_offset, 4);
    EXPECT_TRUE(report.unavailable_reason.empty());
}

TEST(ClassFileByteSourceTest, SelectsCachedClassFileWhenLayoutIsComplete)
{
    std::vector<VMStructEntryData> structs;
    structs.push_back({"InstanceKlass", "_cached_class_file", "ClassFileData*", 0, 24, nullptr});
    structs.push_back({"ClassFileData", "_length", "u4", 0, 0, nullptr});
    structs.push_back({"ClassFileData", "_bytes", "u1*", 0, 4, nullptr});

    std::vector<VMTypeEntryData> types;
    types.push_back({"InstanceKlass", "Klass", 0, 0, 0, 128});
    types.push_back({"ClassFileData", "", 0, 0, 0, 8});

    std::vector<VMIntConstantEntryData>  ints;
    std::vector<VMLongConstantEntryData> longs;

    VmStructSnapshot snapshot;
    ASSERT_TRUE(
        snapshot.load(std::move(structs), std::move(types), std::move(ints), std::move(longs))
            .is_ok());

    const auto report = inspect_class_file_byte_source(snapshot.table());
    EXPECT_EQ(report.kind, ClassFileByteSourceKind::kInstanceKlassCachedClassFile);
    EXPECT_TRUE(report.cached_class_file_field_present);
    EXPECT_TRUE(report.cached_class_file_is_instance);
    EXPECT_TRUE(report.class_file_data_type_present);
    EXPECT_TRUE(report.class_file_data_length_present);
    EXPECT_TRUE(report.class_file_data_bytes_present);
    EXPECT_EQ(report.cached_class_file_offset, 24);
    EXPECT_EQ(report.class_file_length_offset, 0);
    EXPECT_EQ(report.class_file_bytes_offset, 4);
    EXPECT_TRUE(report.unavailable_reason.empty());
}

TEST(MethodViewTest, ReadsConstMethodBytecodeFromHotSpotOffsets)
{
    VMStructEntry structs[] = {
        {"Method", "_constMethod", "ConstMethod*", 0, offsetof(FakeMethod, const_method), nullptr},
        {"Method", "_i2i_entry", "address", 0, offsetof(FakeMethod, i2i_entry), nullptr},
        {"Method", "_access_flags", "AccessFlags", 0, offsetof(FakeMethod, access_flags), nullptr},
        {"ConstMethod",
         "_constants",
         "ConstantPool*",
         0,
         offsetof(FakeConstMethod, constants),
         nullptr},
        {"ConstMethod", "_name_index", "u2", 0, offsetof(FakeConstMethod, name_index), nullptr},
        {"ConstMethod",
         "_signature_index",
         "u2",
         0,
         offsetof(FakeConstMethod, signature_index),
         nullptr},
        {"ConstMethod", "_code_size", "u2", 0, offsetof(FakeConstMethod, code_size), nullptr},
        {"ConstMethod", "_max_stack", "u2", 0, offsetof(FakeConstMethod, max_stack), nullptr},
        {"ConstMethod", "_max_locals", "u2", 0, offsetof(FakeConstMethod, max_locals), nullptr},
        {"ConstantPool", "_length", "int", 0, offsetof(FakeConstantPool, length), nullptr},
        {"Symbol", "_length", "u2", 0, offsetof(FakeSymbol, length), nullptr},
        {"Symbol", "_body", "char", 0, offsetof(FakeSymbol, body), nullptr},
        {nullptr, nullptr, nullptr, 0, 0, nullptr},
    };
    VMTypeEntry types[] = {
        {"Method", nullptr, 0, 0, 0, sizeof(FakeMethod)},
        {"ConstMethod", nullptr, 0, 0, 0, offsetof(FakeConstMethod, code)},
        {"ConstantPool", nullptr, 0, 0, 0, offsetof(FakeConstantPool, entries)},
        {"Symbol", nullptr, 0, 0, 0, sizeof(FakeSymbol)},
        {nullptr, nullptr, 0, 0, 0, 0},
    };
    VMIntConstantEntry  ints[]  = {{nullptr, 0}};
    VMLongConstantEntry longs[] = {{nullptr, 0}};

    VmStructTable table;
    ASSERT_TRUE(table.load(structs, types, ints, longs).is_ok());

    FakeSymbol       name       = {};
    FakeSymbol       descriptor = {};
    FakeConstantPool pool       = {};
    fill_symbol(name, "foo");
    fill_symbol(descriptor, "()V");
    pool.length     = 4;
    pool.entries[1] = &name;
    pool.entries[2] = &descriptor;

    FakeConstMethod const_method = {&pool, 1, 2, 4, 8, 3, {0x2a, 0xb7, 0x00, 0x01}};
    FakeMethod      method       = {&const_method, reinterpret_cast<void*>(0x1234), 0x0001};

    MethodView view(&table, &method);
    auto       cm_result = view.const_method();
    ASSERT_TRUE(cm_result.is_ok());

    auto bytecode_result = cm_result.unwrap().bytecode();
    ASSERT_TRUE(bytecode_result.is_ok());
    auto bytes = bytecode_result.unwrap();
    ASSERT_EQ(bytes.size(), 4u);
    EXPECT_EQ(bytes[0], 0x2a);
    EXPECT_EQ(bytes[1], 0xb7);

    auto entry_result = view.interpreter_entry();
    ASSERT_TRUE(entry_result.is_ok());
    EXPECT_EQ(entry_result.unwrap(), reinterpret_cast<void*>(0x1234));
}

TEST(MetadataViewTest, ResolvesSymbolAndConstantPoolEntries)
{
    VMStructEntry structs[] = {
        {"ConstantPool", "_length", "int", 0, offsetof(FakeConstantPool, length), nullptr},
        {"Symbol", "_length", "u2", 0, offsetof(FakeSymbol, length), nullptr},
        {"Symbol", "_body", "char", 0, offsetof(FakeSymbol, body), nullptr},
        {nullptr, nullptr, nullptr, 0, 0, nullptr},
    };
    VMTypeEntry types[] = {
        {"ConstantPool", nullptr, 0, 0, 0, offsetof(FakeConstantPool, entries)},
        {"Symbol", nullptr, 0, 0, 0, sizeof(FakeSymbol)},
        {nullptr, nullptr, 0, 0, 0, 0},
    };
    VMIntConstantEntry  ints[]  = {{nullptr, 0}};
    VMLongConstantEntry longs[] = {{nullptr, 0}};

    VmStructTable table;
    ASSERT_TRUE(table.load(structs, types, ints, longs).is_ok());

    FakeSymbol symbol = {};
    fill_symbol(symbol, "bar");

    FakeConstantPool pool = {};
    pool.length           = 3;
    pool.entries[2]       = &symbol;

    SymbolView symbol_view(&table, &symbol);
    auto       text_result = symbol_view.text();
    ASSERT_TRUE(text_result.is_ok());
    EXPECT_EQ(text_result.unwrap(), "bar");

    ConstantPoolView pool_view(&table, &pool);
    auto             resolved = pool_view.symbol_at(2);
    ASSERT_TRUE(resolved.is_ok());

    auto resolved_text = resolved.unwrap().text();
    ASSERT_TRUE(resolved_text.is_ok());
    EXPECT_EQ(resolved_text.unwrap(), "bar");

    auto out_of_range = pool_view.symbol_at(3);
    ASSERT_TRUE(out_of_range.is_err());
    EXPECT_EQ(out_of_range.unwrap_err().code, NativeErrorCode::kInvalidArgument);
}

TEST(MethodViewTest, ResolvesMethodMetadataFromConstMethodConstantPool)
{
    VMStructEntry structs[] = {
        {"Method", "_constMethod", "ConstMethod*", 0, offsetof(FakeMethod, const_method), nullptr},
        {"Method", "_i2i_entry", "address", 0, offsetof(FakeMethod, i2i_entry), nullptr},
        {"Method", "_access_flags", "AccessFlags", 0, offsetof(FakeMethod, access_flags), nullptr},
        {"ConstMethod",
         "_constants",
         "ConstantPool*",
         0,
         offsetof(FakeConstMethod, constants),
         nullptr},
        {"ConstMethod", "_name_index", "u2", 0, offsetof(FakeConstMethod, name_index), nullptr},
        {"ConstMethod",
         "_signature_index",
         "u2",
         0,
         offsetof(FakeConstMethod, signature_index),
         nullptr},
        {"ConstMethod", "_code_size", "u2", 0, offsetof(FakeConstMethod, code_size), nullptr},
        {"ConstMethod", "_max_stack", "u2", 0, offsetof(FakeConstMethod, max_stack), nullptr},
        {"ConstMethod", "_max_locals", "u2", 0, offsetof(FakeConstMethod, max_locals), nullptr},
        {"ConstantPool", "_length", "int", 0, offsetof(FakeConstantPool, length), nullptr},
        {"Symbol", "_length", "u2", 0, offsetof(FakeSymbol, length), nullptr},
        {"Symbol", "_body", "char", 0, offsetof(FakeSymbol, body), nullptr},
        {nullptr, nullptr, nullptr, 0, 0, nullptr},
    };
    VMTypeEntry types[] = {
        {"Method", nullptr, 0, 0, 0, sizeof(FakeMethod)},
        {"ConstMethod", nullptr, 0, 0, 0, offsetof(FakeConstMethod, code)},
        {"ConstantPool", nullptr, 0, 0, 0, offsetof(FakeConstantPool, entries)},
        {"Symbol", nullptr, 0, 0, 0, sizeof(FakeSymbol)},
        {nullptr, nullptr, 0, 0, 0, 0},
    };
    VMIntConstantEntry  ints[]  = {{nullptr, 0}};
    VMLongConstantEntry longs[] = {{nullptr, 0}};

    VmStructTable table;
    ASSERT_TRUE(table.load(structs, types, ints, longs).is_ok());

    FakeSymbol name       = {};
    FakeSymbol descriptor = {};
    fill_symbol(name, "length");
    fill_symbol(descriptor, "()I");

    FakeConstantPool pool = {};
    pool.length           = 4;
    pool.entries[1]       = &name;
    pool.entries[2]       = &descriptor;

    FakeConstMethod const_method = {&pool, 1, 2, 4, 2, 1, {0x2a, 0xb6, 0x00, 0x01}};
    FakeMethod      method       = {&const_method, reinterpret_cast<void*>(0x5678), 0x0001};

    MethodView view(&table, &method);
    auto       metadata_result = view.metadata();
    ASSERT_TRUE(metadata_result.is_ok());

    const auto metadata = metadata_result.unwrap();
    EXPECT_EQ(metadata.name, "length");
    EXPECT_EQ(metadata.descriptor, "()I");
    EXPECT_EQ(metadata.code_size, 4);
    EXPECT_EQ(metadata.max_stack, 2);
    EXPECT_EQ(metadata.max_locals, 1);
}

TEST(MethodLayoutTest, SummarizesRequiredHotSpotFields)
{
    VMStructEntry structs[] = {
        {"Method", "_constMethod", "ConstMethod*", 0, offsetof(FakeMethod, const_method), nullptr},
        {"Method", "_i2i_entry", "address", 0, offsetof(FakeMethod, i2i_entry), nullptr},
        {"Method", "_access_flags", "AccessFlags", 0, offsetof(FakeMethod, access_flags), nullptr},
        {"ConstMethod",
         "_constants",
         "ConstantPool*",
         0,
         offsetof(FakeConstMethod, constants),
         nullptr},
        {"ConstMethod", "_name_index", "u2", 0, offsetof(FakeConstMethod, name_index), nullptr},
        {"ConstMethod",
         "_signature_index",
         "u2",
         0,
         offsetof(FakeConstMethod, signature_index),
         nullptr},
        {"ConstMethod", "_code_size", "u2", 0, offsetof(FakeConstMethod, code_size), nullptr},
        {"ConstMethod", "_max_stack", "u2", 0, offsetof(FakeConstMethod, max_stack), nullptr},
        {"ConstMethod", "_max_locals", "u2", 0, offsetof(FakeConstMethod, max_locals), nullptr},
        {"ConstantPool", "_length", "int", 0, offsetof(FakeConstantPool, length), nullptr},
        {"Symbol", "_length", "u2", 0, offsetof(FakeSymbol, length), nullptr},
        {"Symbol", "_body", "char", 0, offsetof(FakeSymbol, body), nullptr},
        {nullptr, nullptr, nullptr, 0, 0, nullptr},
    };
    VMTypeEntry types[] = {
        {"Method", nullptr, 0, 0, 0, sizeof(FakeMethod)},
        {"ConstMethod", nullptr, 0, 0, 0, offsetof(FakeConstMethod, code)},
        {"ConstantPool", nullptr, 0, 0, 0, offsetof(FakeConstantPool, entries)},
        {"Symbol", nullptr, 0, 0, 0, sizeof(FakeSymbol)},
        {nullptr, nullptr, 0, 0, 0, 0},
    };
    VMIntConstantEntry  ints[]  = {{nullptr, 0}};
    VMLongConstantEntry longs[] = {{nullptr, 0}};

    VmStructTable table;
    ASSERT_TRUE(table.load(structs, types, ints, longs).is_ok());

    auto layout = inspect_method_layout(table);
    ASSERT_TRUE(layout.is_ok());

    const auto info = layout.unwrap();
    EXPECT_EQ(info.method_size, sizeof(FakeMethod));
    EXPECT_EQ(info.const_method_size, offsetof(FakeConstMethod, code));
    EXPECT_EQ(info.constant_pool_size, offsetof(FakeConstantPool, entries));
    EXPECT_EQ(info.method_const_method_offset, offsetof(FakeMethod, const_method));
    EXPECT_EQ(info.method_i2i_entry_offset, offsetof(FakeMethod, i2i_entry));
    EXPECT_EQ(info.const_method_constants_offset, offsetof(FakeConstMethod, constants));
    EXPECT_EQ(info.const_method_name_index_offset, offsetof(FakeConstMethod, name_index));
    EXPECT_EQ(info.const_method_signature_index_offset, offsetof(FakeConstMethod, signature_index));
    EXPECT_EQ(info.const_method_code_size_offset, offsetof(FakeConstMethod, code_size));
    EXPECT_EQ(info.constant_pool_length_offset, offsetof(FakeConstantPool, length));
    EXPECT_EQ(info.symbol_length_offset, offsetof(FakeSymbol, length));
    EXPECT_EQ(info.symbol_body_offset, offsetof(FakeSymbol, body));
}

TEST(MethodLayoutTest, ReportsMissingRequiredField)
{
    VMStructEntry structs[] = {
        {"Method", "_constMethod", "ConstMethod*", 0, offsetof(FakeMethod, const_method), nullptr},
        {"Method", "_access_flags", "AccessFlags", 0, offsetof(FakeMethod, access_flags), nullptr},
        {"ConstMethod",
         "_constants",
         "ConstantPool*",
         0,
         offsetof(FakeConstMethod, constants),
         nullptr},
        {"ConstMethod", "_name_index", "u2", 0, offsetof(FakeConstMethod, name_index), nullptr},
        {"ConstMethod",
         "_signature_index",
         "u2",
         0,
         offsetof(FakeConstMethod, signature_index),
         nullptr},
        {"ConstMethod", "_code_size", "u2", 0, offsetof(FakeConstMethod, code_size), nullptr},
        {"ConstMethod", "_max_stack", "u2", 0, offsetof(FakeConstMethod, max_stack), nullptr},
        {"ConstMethod", "_max_locals", "u2", 0, offsetof(FakeConstMethod, max_locals), nullptr},
        {"ConstantPool", "_length", "int", 0, offsetof(FakeConstantPool, length), nullptr},
        {"Symbol", "_length", "u2", 0, offsetof(FakeSymbol, length), nullptr},
        {"Symbol", "_body", "char", 0, offsetof(FakeSymbol, body), nullptr},
        {nullptr, nullptr, nullptr, 0, 0, nullptr},
    };
    VMTypeEntry types[] = {
        {"Method", nullptr, 0, 0, 0, sizeof(FakeMethod)},
        {"ConstMethod", nullptr, 0, 0, 0, offsetof(FakeConstMethod, code)},
        {"ConstantPool", nullptr, 0, 0, 0, offsetof(FakeConstantPool, entries)},
        {"Symbol", nullptr, 0, 0, 0, sizeof(FakeSymbol)},
        {nullptr, nullptr, 0, 0, 0, 0},
    };
    VMIntConstantEntry  ints[]  = {{nullptr, 0}};
    VMLongConstantEntry longs[] = {{nullptr, 0}};

    VmStructTable table;
    ASSERT_TRUE(table.load(structs, types, ints, longs).is_ok());

    auto layout = inspect_method_layout(table);
    ASSERT_TRUE(layout.is_err());
    EXPECT_EQ(layout.unwrap_err().code, NativeErrorCode::kMissingVmStruct);
}

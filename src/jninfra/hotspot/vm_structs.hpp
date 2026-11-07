#pragma once

/// @defgroup jninfra-vmstructs jninfra-vmstructs
/// @brief HotSpot 导出 VMStruct/VMType/常量表索引与外部快照。
///
/// 表装载、快照所有权与远端地址语义见 doc:design-vmstructs。

#include <cstdint>
#include <cstring>
#include <deque>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "jninfra/error.hpp"
#include "libca/core/result.hpp"

namespace jninfra::hotspot {

/// @brief Native layout of one entry in HotSpot gHotSpotVMStructs.
struct VMStructEntry
{
    const char* typeName;
    const char* fieldName;
    const char* typeString;
    ca::i32     isStatic;
    ca::u64     offset;
    void*       address;
};

/// @brief Native layout of one entry in HotSpot gHotSpotVMTypes.
struct VMTypeEntry
{
    const char* typeName;
    const char* superclassName;
    ca::i32     isOopType;
    ca::i32     isIntegerType;
    ca::i32     isUnsigned;
    ca::u64     size;
};

/// @brief Native layout of one entry in HotSpot gHotSpotVMIntConstants.
struct VMIntConstantEntry
{
    const char* name;
    ca::i32     value;
};

/// @brief Native layout of one entry in HotSpot gHotSpotVMLongConstants.
struct VMLongConstantEntry
{
    const char* name;
    ca::u64     value;
};

/// @brief Owned copy of one VMStruct row copied from a JVM metadata table.
///
/// External readers cannot keep HotSpot table string pointers directly because
/// those pointers belong to another process. This data form stores the copied
/// strings as normal C++ values before a VmStructSnapshot turns them back into
/// stable C-style rows for VmStructTable indexing.
struct VMStructEntryData
{
    std::string type_name;
    std::string field_name;
    std::string type_string;
    ca::i32     is_static = 0;
    ca::u64     offset    = 0;
    void*       address   = nullptr;
};

/// @brief Owned copy of one VMType row copied from a JVM metadata table.
struct VMTypeEntryData
{
    std::string type_name;
    std::string superclass_name;
    ca::i32     is_oop_type     = 0;
    ca::i32     is_integer_type = 0;
    ca::i32     is_unsigned     = 0;
    ca::u64     size            = 0;
};

/// @brief Owned copy of one integer VM constant row.
struct VMIntConstantEntryData
{
    std::string name;
    ca::i32     value = 0;
};

/// @brief Owned copy of one long VM constant row.
struct VMLongConstantEntryData
{
    std::string name;
    ca::u64     value = 0;
};

/// @brief Indexed view over HotSpot VMStruct/VMType exported tables.
class VmStructTable
{
public:
    /// @brief Index raw HotSpot exported metadata tables.
    /// @param structs gHotSpotVMStructs.
    /// @param types gHotSpotVMTypes.
    /// @param ints gHotSpotVMIntConstants.
    /// @param longs gHotSpotVMLongConstants.
    /// @return Ok when all table pointers are non-null and indexing succeeds.
    ca::Result<void, NativeError> load(VMStructEntry* structs, VMTypeEntry* types,
                                       VMIntConstantEntry* ints, VMLongConstantEntry* longs);

    /// @return Field metadata for @p type_name::@p field_name, or nullptr.
    const VMStructEntry* field(const std::string& type_name,
                               const std::string& field_name) const noexcept;

    /// @return Type metadata for @p type_name, or nullptr.
    const VMTypeEntry* type(const std::string& type_name) const noexcept;

    /// @return Integer constant metadata for @p name, or nullptr.
    const VMIntConstantEntry* int_constant(const std::string& name) const noexcept;

    /// @return Long constant metadata for @p name, or nullptr.
    const VMLongConstantEntry* long_constant(const std::string& name) const noexcept;

    /// @brief Resolve the address of a static or instance HotSpot field.
    /// @param type_name HotSpot VM type name.
    /// @param field_name Field name on that type.
    /// @param instance Instance pointer for non-static fields.
    /// @return Address of the field storage.
    ca::Result<void*, NativeError> field_address(const std::string& type_name,
                                                 const std::string& field_name,
                                                 void*              instance) const;

    /// @brief Read a typed HotSpot field value.
    /// @param type_name HotSpot VM type name.
    /// @param field_name Field name on that type.
    /// @param instance Instance pointer for non-static fields.
    /// @return Field value copied from VM memory.
    template<typename T>
    ca::Result<T, NativeError> read_field(const std::string& type_name,
                                          const std::string& field_name, void* instance) const
    {
        static_assert(std::is_trivially_copyable<T>::value,
                      "HotSpot field reads require trivially copyable T");
        auto address = field_address(type_name, field_name, instance);
        if (address.is_err()) {
            return ca::Err(address.unwrap_err());
        }
        T value;
        std::memcpy(&value, address.unwrap(), sizeof(T));
        return ca::Ok(std::move(value));
    }

    /// @brief Write a typed HotSpot field value.
    /// @param type_name HotSpot VM type name.
    /// @param field_name Field name on that type.
    /// @param instance Instance pointer for non-static fields.
    /// @param value Value to write into VM memory.
    /// @return Ok on success.
    template<typename T>
    ca::Result<void, NativeError> write_field(const std::string& type_name,
                                              const std::string& field_name, void* instance,
                                              T value) const
    {
        static_assert(std::is_trivially_copyable<T>::value,
                      "HotSpot field writes require trivially copyable T");
        auto address = field_address(type_name, field_name, instance);
        if (address.is_err()) {
            return ca::Err(address.unwrap_err());
        }
        std::memcpy(address.unwrap(), &value, sizeof(T));
        return ca::Ok();
    }

private:
    using FieldMap = std::unordered_map<std::string, const VMStructEntry*>;

    std::unordered_map<std::string, FieldMap>                   structs_;
    std::unordered_map<std::string, const VMTypeEntry*>         types_;
    std::unordered_map<std::string, const VMIntConstantEntry*>  ints_;
    std::unordered_map<std::string, const VMLongConstantEntry*> longs_;
};

/// @brief Owning HotSpot VMStruct snapshot for tables copied out of a JVM.
///
/// VmStructTable is a borrowed view over raw HotSpot export arrays. That is
/// ideal inside a JVM process, but P2 external readers first copy table rows and
/// referenced strings into local memory. VmStructSnapshot owns that copied data,
/// builds sentinel-terminated rows, and exposes a VmStructTable over the stable
/// local arrays.
///
/// The addresses stored in static field rows remain target-process addresses
/// when the snapshot came from an external process. Callers may use them as
/// remote addresses with their own memory reader; they must not dereference them
/// directly unless the snapshot was built from the current process.
class VmStructSnapshot
{
public:
    VmStructSnapshot()                                   = default;
    VmStructSnapshot(const VmStructSnapshot&)            = delete;
    VmStructSnapshot& operator=(const VmStructSnapshot&) = delete;
    VmStructSnapshot(VmStructSnapshot&&)                 = delete;
    VmStructSnapshot& operator=(VmStructSnapshot&&)      = delete;

    /// @brief Replace snapshot contents with copied HotSpot metadata rows.
    /// @param structs Copied VMStruct rows, without the sentinel row.
    /// @param types Copied VMType rows, without the sentinel row.
    /// @param ints Copied integer constants, without the sentinel row.
    /// @param longs Copied long constants, without the sentinel row.
    /// @return Ok when the owned rows can be indexed by VmStructTable.
    ca::Result<void, NativeError> load(std::vector<VMStructEntryData>       structs,
                                       std::vector<VMTypeEntryData>         types,
                                       std::vector<VMIntConstantEntryData>  ints,
                                       std::vector<VMLongConstantEntryData> longs);

    /// @brief Remove every copied row and reset the indexed view.
    void clear() noexcept;

    /// @return Indexed view over the copied tables.
    const VmStructTable& table() const noexcept { return table_; }

    /// @return Number of copied VMStruct rows, excluding the sentinel.
    ca::usize struct_count() const noexcept { return struct_count_; }

    /// @brief Borrow copied VMStruct rows for diagnostics.
    ///
    /// The returned array excludes the sentinel row and remains valid until the
    /// snapshot is cleared or destroyed. This is intended for layout reporting;
    /// normal field lookup should use table().
    const VMStructEntry* struct_entries() const noexcept { return structs_.data(); }

    /// @return Number of copied VMType rows, excluding the sentinel.
    ca::usize type_count() const noexcept { return type_count_; }

    /// @brief Borrow copied VMType rows for diagnostics.
    ///
    /// The returned array excludes the sentinel row and remains valid until the
    /// snapshot is cleared or destroyed.
    const VMTypeEntry* type_entries() const noexcept { return types_.data(); }

    /// @return Number of copied integer constant rows, excluding the sentinel.
    ca::usize int_constant_count() const noexcept { return int_count_; }

    /// @return Number of copied long constant rows, excluding the sentinel.
    ca::usize long_constant_count() const noexcept { return long_count_; }

private:
    const char* keep_string(const std::string& value);

    std::deque<std::string>          strings_;
    std::vector<VMStructEntry>       structs_;
    std::vector<VMTypeEntry>         types_;
    std::vector<VMIntConstantEntry>  ints_;
    std::vector<VMLongConstantEntry> longs_;
    VmStructTable                    table_;
    ca::usize                        struct_count_ = 0;
    ca::usize                        type_count_   = 0;
    ca::usize                        int_count_    = 0;
    ca::usize                        long_count_   = 0;
};

}   // namespace jninfra::hotspot

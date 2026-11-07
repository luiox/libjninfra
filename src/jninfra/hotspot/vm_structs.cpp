#include "jninfra/hotspot/vm_structs.hpp"

namespace jninfra::hotspot {

ca::Result<void, NativeError> VmStructTable::load(VMStructEntry* structs, VMTypeEntry* types,
                                                  VMIntConstantEntry*  ints,
                                                  VMLongConstantEntry* longs)
{
    if (structs == nullptr || types == nullptr || ints == nullptr || longs == nullptr) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument,
                                  "HotSpot VMStruct tables must not be null"));
    }

    structs_.clear();
    types_.clear();
    ints_.clear();
    longs_.clear();

    for (ca::usize i = 0; structs[i].fieldName != nullptr; ++i) {
        const VMStructEntry* entry = &structs[i];
        if (entry->typeName != nullptr) {
            structs_[entry->typeName][entry->fieldName] = entry;
        }
    }
    for (ca::usize i = 0; types[i].typeName != nullptr; ++i) {
        types_[types[i].typeName] = &types[i];
    }
    for (ca::usize i = 0; ints[i].name != nullptr; ++i) {
        ints_[ints[i].name] = &ints[i];
    }
    for (ca::usize i = 0; longs[i].name != nullptr; ++i) {
        longs_[longs[i].name] = &longs[i];
    }

    return ca::Ok();
}

const VMStructEntry* VmStructTable::field(const std::string& type_name,
                                          const std::string& field_name) const noexcept
{
    auto type_it = structs_.find(type_name);
    if (type_it == structs_.end()) {
        return nullptr;
    }
    auto field_it = type_it->second.find(field_name);
    if (field_it == type_it->second.end()) {
        return nullptr;
    }
    return field_it->second;
}

const VMTypeEntry* VmStructTable::type(const std::string& type_name) const noexcept
{
    auto it = types_.find(type_name);
    return it == types_.end() ? nullptr : it->second;
}

const VMIntConstantEntry* VmStructTable::int_constant(const std::string& name) const noexcept
{
    auto it = ints_.find(name);
    return it == ints_.end() ? nullptr : it->second;
}

const VMLongConstantEntry* VmStructTable::long_constant(const std::string& name) const noexcept
{
    auto it = longs_.find(name);
    return it == longs_.end() ? nullptr : it->second;
}

ca::Result<void*, NativeError> VmStructTable::field_address(const std::string& type_name,
                                                            const std::string& field_name,
                                                            void*              instance) const
{
    const VMStructEntry* entry = field(type_name, field_name);
    if (entry == nullptr) {
        return ca::Err(make_error(NativeErrorCode::kMissingVmStruct,
                                  "Missing HotSpot field: " + type_name + "::" + field_name));
    }
    if (entry->isStatic != 0) {
        if (entry->address == nullptr) {
            return ca::Err(make_error(
                NativeErrorCode::kMissingVmStruct,
                "Static HotSpot field has null address: " + type_name + "::" + field_name));
        }
        return ca::Ok(entry->address);
    }
    if (instance == nullptr) {
        return ca::Err(make_error(
            NativeErrorCode::kInvalidArgument,
            "Instance field requires an instance pointer: " + type_name + "::" + field_name));
    }
    auto* base = reinterpret_cast<unsigned char*>(instance);
    return ca::Ok(static_cast<void*>(base + entry->offset));
}

const char* VmStructSnapshot::keep_string(const std::string& value)
{
    if (value.empty()) {
        return nullptr;
    }
    strings_.push_back(value);
    return strings_.back().c_str();
}

void VmStructSnapshot::clear() noexcept
{
    strings_.clear();
    structs_.clear();
    types_.clear();
    ints_.clear();
    longs_.clear();
    table_        = VmStructTable();
    struct_count_ = 0;
    type_count_   = 0;
    int_count_    = 0;
    long_count_   = 0;
}

ca::Result<void, NativeError> VmStructSnapshot::load(std::vector<VMStructEntryData>       structs,
                                                     std::vector<VMTypeEntryData>         types,
                                                     std::vector<VMIntConstantEntryData>  ints,
                                                     std::vector<VMLongConstantEntryData> longs)
{
    clear();

    struct_count_ = structs.size();
    type_count_   = types.size();
    int_count_    = ints.size();
    long_count_   = longs.size();

    structs_.reserve(structs.size() + 1);
    types_.reserve(types.size() + 1);
    ints_.reserve(ints.size() + 1);
    longs_.reserve(longs.size() + 1);

    // Convert copied string rows into HotSpot-compatible raw rows. The string
    // pool owns every name for the full lifetime of this snapshot.
    for (const auto& row : structs) {
        VMStructEntry entry = {};
        entry.typeName      = keep_string(row.type_name);
        entry.fieldName     = keep_string(row.field_name);
        entry.typeString    = keep_string(row.type_string);
        entry.isStatic      = row.is_static;
        entry.offset        = row.offset;
        entry.address       = row.address;
        structs_.push_back(entry);
    }
    structs_.push_back({nullptr, nullptr, nullptr, 0, 0, nullptr});

    for (const auto& row : types) {
        VMTypeEntry entry    = {};
        entry.typeName       = keep_string(row.type_name);
        entry.superclassName = keep_string(row.superclass_name);
        entry.isOopType      = row.is_oop_type;
        entry.isIntegerType  = row.is_integer_type;
        entry.isUnsigned     = row.is_unsigned;
        entry.size           = row.size;
        types_.push_back(entry);
    }
    types_.push_back({nullptr, nullptr, 0, 0, 0, 0});

    for (const auto& row : ints) {
        VMIntConstantEntry entry = {};
        entry.name               = keep_string(row.name);
        entry.value              = row.value;
        ints_.push_back(entry);
    }
    ints_.push_back({nullptr, 0});

    for (const auto& row : longs) {
        VMLongConstantEntry entry = {};
        entry.name                = keep_string(row.name);
        entry.value               = row.value;
        longs_.push_back(entry);
    }
    longs_.push_back({nullptr, 0});

    return table_.load(structs_.data(), types_.data(), ints_.data(), longs_.data());
}

}   // namespace jninfra::hotspot

#include "jninfra/hotspot/layout.hpp"

#include <utility>

namespace jninfra::hotspot {
namespace {

ca::Result<const VMTypeEntry*, NativeError> require_type(const VmStructTable& table,
                                                         const std::string&   type_name)
{
    const VMTypeEntry* type = table.type(type_name);
    if (type == nullptr) {
        return ca::Err(
            make_error(NativeErrorCode::kMissingVmType, "Missing HotSpot type: " + type_name));
    }
    return ca::Ok(type);
}

ca::Result<const VMStructEntry*, NativeError> require_instance_field(const VmStructTable& table,
                                                                     const std::string&   type_name,
                                                                     const std::string& field_name)
{
    const VMStructEntry* field = table.field(type_name, field_name);
    if (field == nullptr) {
        return ca::Err(make_error(NativeErrorCode::kMissingVmStruct,
                                  "Missing HotSpot field: " + type_name + "::" + field_name));
    }
    if (field->isStatic != 0) {
        return ca::Err(
            make_error(NativeErrorCode::kUnsupportedHotSpotLayout,
                       "Expected instance HotSpot field: " + type_name + "::" + field_name));
    }
    return ca::Ok(field);
}

}   // namespace

ca::Result<MethodLayoutInfo, NativeError> inspect_method_layout(const VmStructTable& table)
{
    // Keep this compatibility gate stricter than any single view method: later
    // probes should fail at startup before touching partially supported layouts.
    auto method_type = require_type(table, "Method");
    if (method_type.is_err()) {
        return ca::Err(method_type.unwrap_err());
    }
    auto const_method_type = require_type(table, "ConstMethod");
    if (const_method_type.is_err()) {
        return ca::Err(const_method_type.unwrap_err());
    }
    auto constant_pool_type = require_type(table, "ConstantPool");
    if (constant_pool_type.is_err()) {
        return ca::Err(constant_pool_type.unwrap_err());
    }
    auto symbol_type = require_type(table, "Symbol");
    if (symbol_type.is_err()) {
        return ca::Err(symbol_type.unwrap_err());
    }
    auto const_method_field = require_instance_field(table, "Method", "_constMethod");
    if (const_method_field.is_err()) {
        return ca::Err(const_method_field.unwrap_err());
    }
    auto i2i_entry_field = require_instance_field(table, "Method", "_i2i_entry");
    if (i2i_entry_field.is_err()) {
        return ca::Err(i2i_entry_field.unwrap_err());
    }
    auto access_flags_field = require_instance_field(table, "Method", "_access_flags");
    if (access_flags_field.is_err()) {
        return ca::Err(access_flags_field.unwrap_err());
    }
    auto constants_field = require_instance_field(table, "ConstMethod", "_constants");
    if (constants_field.is_err()) {
        return ca::Err(constants_field.unwrap_err());
    }
    auto name_index_field = require_instance_field(table, "ConstMethod", "_name_index");
    if (name_index_field.is_err()) {
        return ca::Err(name_index_field.unwrap_err());
    }
    auto signature_index_field = require_instance_field(table, "ConstMethod", "_signature_index");
    if (signature_index_field.is_err()) {
        return ca::Err(signature_index_field.unwrap_err());
    }
    auto code_size_field = require_instance_field(table, "ConstMethod", "_code_size");
    if (code_size_field.is_err()) {
        return ca::Err(code_size_field.unwrap_err());
    }
    auto max_stack_field = require_instance_field(table, "ConstMethod", "_max_stack");
    if (max_stack_field.is_err()) {
        return ca::Err(max_stack_field.unwrap_err());
    }
    auto max_locals_field = require_instance_field(table, "ConstMethod", "_max_locals");
    if (max_locals_field.is_err()) {
        return ca::Err(max_locals_field.unwrap_err());
    }
    auto constant_pool_length_field = require_instance_field(table, "ConstantPool", "_length");
    if (constant_pool_length_field.is_err()) {
        return ca::Err(constant_pool_length_field.unwrap_err());
    }
    auto symbol_length_field = require_instance_field(table, "Symbol", "_length");
    if (symbol_length_field.is_err()) {
        return ca::Err(symbol_length_field.unwrap_err());
    }
    auto symbol_body_field = require_instance_field(table, "Symbol", "_body");
    if (symbol_body_field.is_err()) {
        return ca::Err(symbol_body_field.unwrap_err());
    }

    MethodLayoutInfo info;
    info.method_size                         = method_type.unwrap()->size;
    info.const_method_size                   = const_method_type.unwrap()->size;
    info.constant_pool_size                  = constant_pool_type.unwrap()->size;
    info.method_const_method_offset          = const_method_field.unwrap()->offset;
    info.method_i2i_entry_offset             = i2i_entry_field.unwrap()->offset;
    info.method_access_flags_offset          = access_flags_field.unwrap()->offset;
    info.const_method_constants_offset       = constants_field.unwrap()->offset;
    info.const_method_name_index_offset      = name_index_field.unwrap()->offset;
    info.const_method_signature_index_offset = signature_index_field.unwrap()->offset;
    info.const_method_code_size_offset       = code_size_field.unwrap()->offset;
    info.const_method_max_stack_offset       = max_stack_field.unwrap()->offset;
    info.const_method_max_locals_offset      = max_locals_field.unwrap()->offset;
    info.constant_pool_length_offset         = constant_pool_length_field.unwrap()->offset;
    info.symbol_length_offset                = symbol_length_field.unwrap()->offset;
    info.symbol_body_offset                  = symbol_body_field.unwrap()->offset;
    return ca::Ok(std::move(info));
}

}   // namespace jninfra::hotspot

#pragma once

/// @defgroup jninfra-layout jninfra-layout
/// @brief 视图层布局校验：探针依赖的字段/类型导出完整性检查。
///

#include "jninfra/hotspot/vm_structs.hpp"

namespace jninfra::hotspot {

/// @brief HotSpot Method/ConstMethod fields required by MethodView.
///
/// The values are copied from HotSpot's exported VMStruct and VMType tables at
/// agent startup. They are useful both as a compatibility summary for logs and
/// as an explicit record of the runtime layout assumptions required by
/// `MethodView`, `ConstMethodView`, `ConstantPoolView`, and `SymbolView`.
struct MethodLayoutInfo
{
    /// @brief Runtime `Method` VM type size.
    ca::u64 method_size = 0;
    /// @brief Runtime `ConstMethod` VM type size; bytecode starts after this header.
    ca::u64 const_method_size = 0;
    /// @brief Runtime `ConstantPool` VM type size; entry slots start after this header.
    ca::u64 constant_pool_size = 0;
    /// @brief Offset of `Method::_constMethod`.
    ca::u64 method_const_method_offset = 0;
    /// @brief Offset of `Method::_i2i_entry`.
    ca::u64 method_i2i_entry_offset = 0;
    /// @brief Offset of `Method::_access_flags`.
    ca::u64 method_access_flags_offset = 0;
    /// @brief Offset of `ConstMethod::_constants`.
    ca::u64 const_method_constants_offset = 0;
    /// @brief Offset of `ConstMethod::_name_index`.
    ca::u64 const_method_name_index_offset = 0;
    /// @brief Offset of `ConstMethod::_signature_index`.
    ca::u64 const_method_signature_index_offset = 0;
    /// @brief Offset of `ConstMethod::_code_size`.
    ca::u64 const_method_code_size_offset = 0;
    /// @brief Offset of `ConstMethod::_max_stack`.
    ca::u64 const_method_max_stack_offset = 0;
    /// @brief Offset of `ConstMethod::_max_locals`.
    ca::u64 const_method_max_locals_offset = 0;
    /// @brief Offset of `ConstantPool::_length`.
    ca::u64 constant_pool_length_offset = 0;
    /// @brief Offset of `Symbol::_length`.
    ca::u64 symbol_length_offset = 0;
    /// @brief Offset of `Symbol::_body`.
    ca::u64 symbol_body_offset = 0;
};

/// @brief Validate and summarize the HotSpot layout required by MethodView.
///
/// This is intended for agent startup probes. A successful result means the
/// current JVM exports the Method/ConstMethod/ConstantPool/Symbol VMStruct
/// entries that libjninfra needs before it can safely inspect method metadata
/// and bytecode payloads.
ca::Result<MethodLayoutInfo, NativeError> inspect_method_layout(const VmStructTable& table);

}   // namespace jninfra::hotspot

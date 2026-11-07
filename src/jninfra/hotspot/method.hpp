#pragma once

/// @defgroup jninfra-method-views jninfra-method-views
/// @brief jmethodID 到 Method/ConstMethod 元数据的只读视图。
///
/// 视图所有权与 jmethodID 间接语义见 doc:design-metadata-views。

#include <string>
#include <vector>

#include "jninfra/hotspot/metadata.hpp"
#include "jninfra/hotspot/vm_structs.hpp"

namespace jninfra::hotspot {

/// @brief Stable copy of the HotSpot metadata libjninfra needs for a method.
///
/// `MethodView` and `ConstMethodView` are borrowed VM-memory views. This value
/// type is the safe handoff shape for callers that need to keep names,
/// descriptors, and bytecode sizing information after the immediate probe.
struct MethodMetadata
{
    std::string name;
    std::string descriptor;
    ca::u16     max_stack  = 0;
    ca::u16     max_locals = 0;
    ca::u16     code_size  = 0;
};

/// @brief Borrowed view over a HotSpot `ConstMethod` object.
///
/// HotSpot stores immutable method payload data, including bytecode and name /
/// descriptor indexes, in `ConstMethod`. This view reads only fields exported by
/// `gHotSpotVMStructs` and computes the bytecode start from the runtime
/// `ConstMethod` type size, avoiding compile-time assumptions about the local
/// C++ compiler's object layout.
///
/// The view does not own VM memory. It is intended for short-lived inspection
/// while a Java thread is attached and the referenced method remains valid.
class ConstMethodView
{
public:
    ConstMethodView() noexcept = default;
    ConstMethodView(const VmStructTable* table, void* address) noexcept;

    /// @brief Check whether this view carries a VMStruct table and address.
    ///
    /// A valid result does not guarantee HotSpot object lifetime; it only means
    /// subsequent field reads have enough local context to attempt inspection.
    bool valid() const noexcept { return table_ != nullptr && address_ != nullptr; }

    /// @return Borrowed HotSpot ConstMethod pointer.
    void* address() const noexcept { return address_; }

    /// @brief Read `ConstMethod::_constants`.
    /// @return Borrowed view over the method's owning constant pool.
    ca::Result<ConstantPoolView, NativeError> constants() const;

    /// @brief Read `ConstMethod::_name_index`.
    /// @return Constant-pool index for the method name symbol.
    ca::Result<ca::u16, NativeError> name_index() const;

    /// @brief Read `ConstMethod::_signature_index`.
    /// @return Constant-pool index for the method descriptor symbol.
    ca::Result<ca::u16, NativeError> signature_index() const;

    /// @brief Resolve the method name through `ConstantPool` and `Symbol`.
    /// @return Copied method name such as `length` or `<init>`.
    ca::Result<std::string, NativeError> name() const;

    /// @brief Resolve the JVM method descriptor through `ConstantPool` and `Symbol`.
    /// @return Copied descriptor such as `()I` or `(Ljava/lang/String;)V`.
    ca::Result<std::string, NativeError> descriptor() const;

    /// @return ConstMethod::_code_size.
    ca::Result<ca::u16, NativeError> code_size() const;

    /// @return ConstMethod::_max_stack.
    ca::Result<ca::u16, NativeError> max_stack() const;

    /// @return ConstMethod::_max_locals.
    ca::Result<ca::u16, NativeError> max_locals() const;

    /// @return Pointer to the first bytecode byte.
    ca::Result<unsigned char*, NativeError> bytecode_start() const;

    /// @return Copy of the method bytecode payload.
    ca::Result<std::vector<ca::u8>, NativeError> bytecode() const;

private:
    const VmStructTable* table_   = nullptr;
    void*                address_ = nullptr;
};

/// @brief Borrowed view over a HotSpot `Method` object.
///
/// A JNI `jmethodID` is backed by a HotSpot method pointer in the JVM versions
/// this library targets. `MethodView` is the public entry point for turning that
/// pointer into higher-level metadata without copying or mutating HotSpot
/// internals. Layout compatibility must be checked with `inspect_method_layout`
/// before installing probes that rely on this view.
class MethodView
{
public:
    MethodView() noexcept = default;
    MethodView(const VmStructTable* table, void* address) noexcept;

    /// @brief Build a MethodView from the HotSpot Method* behind a jmethodID.
    /// @param table Loaded HotSpot VMStruct metadata.
    /// @param method_id JNI method ID.
    /// @return Method view, invalid when @p method_id is null.
    ///
    /// jmethodID semantics differ by JDK generation: on JDK <= 8 it is the
    /// `Method*` itself; from JDK 9 on (the versions this library targets) it
    /// is an indirection whose first word holds the `Method*`. The resolution
    /// is pinned by unit tests and a live smoke on JDK 17.0.8.
    static MethodView from_jmethod_id(const VmStructTable* table, jmethodID method_id) noexcept;

    /// @return true when the view has metadata and a non-null VM address.
    bool valid() const noexcept { return table_ != nullptr && address_ != nullptr; }

    /// @return Borrowed HotSpot Method pointer.
    void* address() const noexcept { return address_; }

    /// @return View over Method::_constMethod.
    ca::Result<ConstMethodView, NativeError> const_method() const;

    /// @return Method name resolved through ConstMethod/ConstantPool.
    ca::Result<std::string, NativeError> name() const;

    /// @return Method descriptor resolved through ConstMethod/ConstantPool.
    ca::Result<std::string, NativeError> descriptor() const;

    /// @brief Resolve a stable metadata copy for this method.
    ///
    /// This combines `ConstMethod` indexes, `ConstantPool` symbol slots, and
    /// bytecode sizing fields into a value that can safely leave the immediate
    /// HotSpot view layer.
    ca::Result<MethodMetadata, NativeError> metadata() const;

    /// @return Method::_i2i_entry.
    ca::Result<void*, NativeError> interpreter_entry() const;

    /// @return Address of Method::_access_flags.
    ca::Result<void*, NativeError> access_flags_address() const;

private:
    const VmStructTable* table_   = nullptr;
    void*                address_ = nullptr;
};

}   // namespace jninfra::hotspot

#pragma once

/// @defgroup jninfra-pool-views jninfra-pool-views
/// @brief ConstantPool/Symbol 只读视图：方法名、描述符解析。
///

#include <string>

#include "jninfra/hotspot/vm_structs.hpp"

namespace jninfra::hotspot {

/// @brief Borrowed view over a HotSpot `Symbol` object.
///
/// HotSpot stores many internal names and descriptors as `Symbol` instances.
/// The view does not own the target VM memory; it only uses a `VmStructTable`
/// to resolve the runtime offsets exported by the current JVM and copies data
/// out when a caller asks for `text()`.
///
/// A `SymbolView` is valid only while the target JVM object is alive and the
/// associated `VmStructTable` remains alive. Callers should treat it as a
/// short-lived inspection helper, not as a cached string handle.
class SymbolView
{
public:
    SymbolView() noexcept = default;
    SymbolView(const VmStructTable* table, void* address) noexcept;

    /// @brief Check whether the view has the minimum state needed for reads.
    ///
    /// This does not prove the target address still points to a live HotSpot
    /// object; it only checks that the view carries a metadata table and a
    /// non-null VM address.
    bool valid() const noexcept { return table_ != nullptr && address_ != nullptr; }

    /// @return Borrowed HotSpot `Symbol*` address.
    void* address() const noexcept { return address_; }

    /// @brief Read `Symbol::_length`.
    /// @return Number of bytes in the symbol body.
    ca::Result<ca::u16, NativeError> length() const;

    /// @brief Resolve the address of `Symbol::_body`.
    /// @return Borrowed pointer to the first symbol byte.
    ca::Result<const char*, NativeError> body() const;

    /// @brief Copy the symbol body into a C++ string.
    ///
    /// HotSpot symbols are byte sequences with an explicit length. The returned
    /// string is not read as a null-terminated C string.
    ca::Result<std::string, NativeError> text() const;

private:
    const VmStructTable* table_   = nullptr;
    void*                address_ = nullptr;
};

/// @brief Borrowed view over a HotSpot `ConstantPool` object.
///
/// `ConstMethod` stores method names and descriptors as indexes into its
/// owning `ConstantPool`. This view exposes only the narrow symbol lookup needed
/// by libjninfra metadata probes. The entry-array layout follows HotSpot's
/// exported `ConstantPool` VM type size: entries start immediately after the
/// fixed object header described by `gHotSpotVMTypes`.
class ConstantPoolView
{
public:
    ConstantPoolView() noexcept = default;
    ConstantPoolView(const VmStructTable* table, void* address) noexcept;

    /// @brief Check whether the view has the minimum state needed for reads.
    ///
    /// As with other HotSpot views, this is a local sanity check rather than a
    /// lifetime guarantee for the VM object behind the address.
    bool valid() const noexcept { return table_ != nullptr && address_ != nullptr; }

    /// @return Borrowed HotSpot `ConstantPool*` address.
    void* address() const noexcept { return address_; }

    /// @brief Read `ConstantPool::_length`.
    /// @return Number of slots exported by the current constant pool.
    ca::Result<ca::i32, NativeError> length() const;

    /// @brief Resolve the first raw constant-pool entry slot.
    ///
    /// The entry base is computed from the runtime `ConstantPool` type size, not
    /// from a compile-time C++ layout, so the code remains tied to the inspected
    /// JVM's exported layout.
    ca::Result<void**, NativeError> entry_base() const;

    /// @brief Read a `Symbol*` entry at a constant-pool index.
    ///
    /// This helper is intentionally narrow: it validates the slot range and
    /// returns a `SymbolView`, but it does not attempt to decode arbitrary
    /// constant-pool tags.
    ///
    /// @param index Constant-pool index stored by ConstMethod.
    /// @return Borrowed symbol view for the selected entry.
    ca::Result<SymbolView, NativeError> symbol_at(ca::u16 index) const;

private:
    const VmStructTable* table_   = nullptr;
    void*                address_ = nullptr;
};

}   // namespace jninfra::hotspot

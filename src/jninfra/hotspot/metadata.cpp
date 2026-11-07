#include "jninfra/hotspot/metadata.hpp"

#include <cstring>

namespace jninfra::hotspot {

SymbolView::SymbolView(const VmStructTable* table, void* address) noexcept
    : table_(table)
    , address_(address)
{}

ca::Result<ca::u16, NativeError> SymbolView::length() const
{
    if (!valid()) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument, "SymbolView is invalid"));
    }
    return table_->read_field<ca::u16>("Symbol", "_length", address_);
}

ca::Result<const char*, NativeError> SymbolView::body() const
{
    if (!valid()) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument, "SymbolView is invalid"));
    }
    auto result = table_->field_address("Symbol", "_body", address_);
    if (result.is_err()) {
        return ca::Err(result.unwrap_err());
    }
    return ca::Ok(static_cast<const char*>(result.unwrap()));
}

ca::Result<std::string, NativeError> SymbolView::text() const
{
    auto length_result = length();
    if (length_result.is_err()) {
        return ca::Err(length_result.unwrap_err());
    }
    auto body_result = body();
    if (body_result.is_err()) {
        return ca::Err(body_result.unwrap_err());
    }

    ca::u16     size = length_result.unwrap();
    const char* data = body_result.unwrap();
    if (size == 0) {
        return ca::Ok(std::string());
    }
    if (data == nullptr) {
        return ca::Err(
            make_error(NativeErrorCode::kUnsupportedHotSpotLayout, "Symbol::_body is null"));
    }
    return ca::Ok(std::string(data, static_cast<std::string::size_type>(size)));
}

ConstantPoolView::ConstantPoolView(const VmStructTable* table, void* address) noexcept
    : table_(table)
    , address_(address)
{}

ca::Result<ca::i32, NativeError> ConstantPoolView::length() const
{
    if (!valid()) {
        return ca::Err(
            make_error(NativeErrorCode::kInvalidArgument, "ConstantPoolView is invalid"));
    }
    return table_->read_field<ca::i32>("ConstantPool", "_length", address_);
}

ca::Result<void**, NativeError> ConstantPoolView::entry_base() const
{
    if (!valid()) {
        return ca::Err(
            make_error(NativeErrorCode::kInvalidArgument, "ConstantPoolView is invalid"));
    }
    const VMTypeEntry* type = table_->type("ConstantPool");
    if (type == nullptr) {
        return ca::Err(
            make_error(NativeErrorCode::kMissingVmType, "Missing HotSpot type: ConstantPool"));
    }
    // HotSpot stores the variable-width ConstantPool slots immediately after
    // the fixed portion represented by the VMType size.
    auto* base = reinterpret_cast<unsigned char*>(address_);
    return ca::Ok(reinterpret_cast<void**>(base + type->size));
}

ca::Result<SymbolView, NativeError> ConstantPoolView::symbol_at(ca::u16 index) const
{
    auto length_result = length();
    if (length_result.is_err()) {
        return ca::Err(length_result.unwrap_err());
    }
    ca::i32 constant_pool_length = length_result.unwrap();
    if (constant_pool_length < 0 || static_cast<ca::i32>(index) >= constant_pool_length) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument,
                                  "ConstantPool symbol index is out of range"));
    }

    auto base_result = entry_base();
    if (base_result.is_err()) {
        return ca::Err(base_result.unwrap_err());
    }

    void** entry_base     = base_result.unwrap();
    void*  symbol_address = nullptr;
    std::memcpy(&symbol_address, entry_base + index, sizeof(symbol_address));
    if (symbol_address == nullptr) {
        return ca::Err(make_error(NativeErrorCode::kUnsupportedHotSpotLayout,
                                  "ConstantPool symbol entry is null"));
    }
    return ca::Ok(SymbolView(table_, symbol_address));
}

}   // namespace jninfra::hotspot

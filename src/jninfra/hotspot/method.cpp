#include "jninfra/hotspot/method.hpp"

#include <cstring>

namespace jninfra::hotspot {

ConstMethodView::ConstMethodView(const VmStructTable* table, void* address) noexcept
    : table_(table)
    , address_(address)
{}

ca::Result<ca::u16, NativeError> ConstMethodView::code_size() const
{
    if (!valid()) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument, "ConstMethodView is invalid"));
    }
    return table_->read_field<ca::u16>("ConstMethod", "_code_size", address_);
}

ca::Result<ca::u16, NativeError> ConstMethodView::max_stack() const
{
    if (!valid()) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument, "ConstMethodView is invalid"));
    }
    return table_->read_field<ca::u16>("ConstMethod", "_max_stack", address_);
}

ca::Result<ca::u16, NativeError> ConstMethodView::max_locals() const
{
    if (!valid()) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument, "ConstMethodView is invalid"));
    }
    return table_->read_field<ca::u16>("ConstMethod", "_max_locals", address_);
}

ca::Result<ConstantPoolView, NativeError> ConstMethodView::constants() const
{
    if (!valid()) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument, "ConstMethodView is invalid"));
    }
    auto result = table_->read_field<void*>("ConstMethod", "_constants", address_);
    if (result.is_err()) {
        return ca::Err(result.unwrap_err());
    }
    void* constant_pool = result.unwrap();
    if (constant_pool == nullptr) {
        return ca::Err(make_error(NativeErrorCode::kUnsupportedHotSpotLayout,
                                  "ConstMethod::_constants is null"));
    }
    return ca::Ok(ConstantPoolView(table_, constant_pool));
}

ca::Result<ca::u16, NativeError> ConstMethodView::name_index() const
{
    if (!valid()) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument, "ConstMethodView is invalid"));
    }
    return table_->read_field<ca::u16>("ConstMethod", "_name_index", address_);
}

ca::Result<ca::u16, NativeError> ConstMethodView::signature_index() const
{
    if (!valid()) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument, "ConstMethodView is invalid"));
    }
    return table_->read_field<ca::u16>("ConstMethod", "_signature_index", address_);
}

ca::Result<std::string, NativeError> ConstMethodView::name() const
{
    auto constants_result = constants();
    if (constants_result.is_err()) {
        return ca::Err(constants_result.unwrap_err());
    }
    auto index_result = name_index();
    if (index_result.is_err()) {
        return ca::Err(index_result.unwrap_err());
    }
    auto symbol_result = constants_result.unwrap().symbol_at(index_result.unwrap());
    if (symbol_result.is_err()) {
        return ca::Err(symbol_result.unwrap_err());
    }
    return symbol_result.unwrap().text();
}

ca::Result<std::string, NativeError> ConstMethodView::descriptor() const
{
    auto constants_result = constants();
    if (constants_result.is_err()) {
        return ca::Err(constants_result.unwrap_err());
    }
    auto index_result = signature_index();
    if (index_result.is_err()) {
        return ca::Err(index_result.unwrap_err());
    }
    auto symbol_result = constants_result.unwrap().symbol_at(index_result.unwrap());
    if (symbol_result.is_err()) {
        return ca::Err(symbol_result.unwrap_err());
    }
    return symbol_result.unwrap().text();
}

ca::Result<unsigned char*, NativeError> ConstMethodView::bytecode_start() const
{
    if (!valid()) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument, "ConstMethodView is invalid"));
    }
    const VMTypeEntry* type = table_->type("ConstMethod");
    if (type == nullptr) {
        return ca::Err(
            make_error(NativeErrorCode::kMissingVmType, "Missing HotSpot type: ConstMethod"));
    }
    // The bytecode payload follows the fixed ConstMethod header described by
    // HotSpot's exported VMType table.
    auto* base = reinterpret_cast<unsigned char*>(address_);
    return ca::Ok(base + type->size);
}

ca::Result<std::vector<ca::u8>, NativeError> ConstMethodView::bytecode() const
{
    auto size_result = code_size();
    if (size_result.is_err()) {
        return ca::Err(size_result.unwrap_err());
    }
    auto start_result = bytecode_start();
    if (start_result.is_err()) {
        return ca::Err(start_result.unwrap_err());
    }

    ca::u16             size = size_result.unwrap();
    std::vector<ca::u8> bytes(size);
    if (size != 0) {
        std::memcpy(bytes.data(), start_result.unwrap(), size);
    }
    return ca::Ok(std::move(bytes));
}

MethodView::MethodView(const VmStructTable* table, void* address) noexcept
    : table_(table)
    , address_(address)
{}

MethodView MethodView::from_jmethod_id(const VmStructTable* table, jmethodID method_id) noexcept
{
    if (method_id == nullptr) {
        return MethodView(table, nullptr);
    }
    // jmethodID 语义随 JDK 大版本不同（实证 + Jaroslav Bachorík
    // "Mysterious jmethodID"）：
    //   - JDK ≤ 8：jmethodID 直接就是 Method*；
    //   - JDK 9+ ：jmethodID 是间接句柄，指向的小 wrapper 首字才是 Method*
    //     （本库目标版本；JDK 17.0.8 Windows/x64 实测 first_word 为 Method*，
    //     直接用 jmethodID 作基址会读到 garbage 并在首个字段偏移处 SIGSEGV）。
    // 单一间接读在 JDK 9+ 上正确；若未来需要支持 JDK 8，需按
    // java_version 分派（暂不引入）。
    return MethodView(table, *reinterpret_cast<void**>(method_id));
}

ca::Result<ConstMethodView, NativeError> MethodView::const_method() const
{
    if (!valid()) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument, "MethodView is invalid"));
    }
    auto result = table_->read_field<void*>("Method", "_constMethod", address_);
    if (result.is_err()) {
        return ca::Err(result.unwrap_err());
    }
    void* const_method = result.unwrap();
    if (const_method == nullptr) {
        return ca::Err(
            make_error(NativeErrorCode::kUnsupportedHotSpotLayout, "Method::_constMethod is null"));
    }
    return ca::Ok(ConstMethodView(table_, const_method));
}

ca::Result<std::string, NativeError> MethodView::name() const
{
    auto const_method_result = const_method();
    if (const_method_result.is_err()) {
        return ca::Err(const_method_result.unwrap_err());
    }
    return const_method_result.unwrap().name();
}

ca::Result<std::string, NativeError> MethodView::descriptor() const
{
    auto const_method_result = const_method();
    if (const_method_result.is_err()) {
        return ca::Err(const_method_result.unwrap_err());
    }
    return const_method_result.unwrap().descriptor();
}

ca::Result<MethodMetadata, NativeError> MethodView::metadata() const
{
    auto const_method_result = const_method();
    if (const_method_result.is_err()) {
        return ca::Err(const_method_result.unwrap_err());
    }
    ConstMethodView const_method_view = const_method_result.unwrap();

    auto name_result = const_method_view.name();
    if (name_result.is_err()) {
        return ca::Err(name_result.unwrap_err());
    }
    auto descriptor_result = const_method_view.descriptor();
    if (descriptor_result.is_err()) {
        return ca::Err(descriptor_result.unwrap_err());
    }
    auto max_stack_result = const_method_view.max_stack();
    if (max_stack_result.is_err()) {
        return ca::Err(max_stack_result.unwrap_err());
    }
    auto max_locals_result = const_method_view.max_locals();
    if (max_locals_result.is_err()) {
        return ca::Err(max_locals_result.unwrap_err());
    }
    auto code_size_result = const_method_view.code_size();
    if (code_size_result.is_err()) {
        return ca::Err(code_size_result.unwrap_err());
    }

    MethodMetadata metadata;
    metadata.name       = name_result.unwrap();
    metadata.descriptor = descriptor_result.unwrap();
    metadata.max_stack  = max_stack_result.unwrap();
    metadata.max_locals = max_locals_result.unwrap();
    metadata.code_size  = code_size_result.unwrap();
    return ca::Ok(std::move(metadata));
}

ca::Result<void*, NativeError> MethodView::interpreter_entry() const
{
    if (!valid()) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument, "MethodView is invalid"));
    }
    return table_->read_field<void*>("Method", "_i2i_entry", address_);
}

ca::Result<void*, NativeError> MethodView::access_flags_address() const
{
    if (!valid()) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument, "MethodView is invalid"));
    }
    return table_->field_address("Method", "_access_flags", address_);
}

}   // namespace jninfra::hotspot

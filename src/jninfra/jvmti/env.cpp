#include "jninfra/jvmti/env.hpp"

namespace jninfra {

JvmtiBuffer::JvmtiBuffer(jvmtiEnv* env, unsigned char* data) noexcept
    : env_(env)
    , data_(data)
{}

JvmtiBuffer::~JvmtiBuffer()
{
    reset();
}

JvmtiBuffer::JvmtiBuffer(JvmtiBuffer&& other) noexcept
    : env_(other.env_)
    , data_(other.data_)
{
    other.env_  = nullptr;
    other.data_ = nullptr;
}

JvmtiBuffer& JvmtiBuffer::operator=(JvmtiBuffer&& other) noexcept
{
    if (this != &other) {
        reset();
        env_        = other.env_;
        data_       = other.data_;
        other.env_  = nullptr;
        other.data_ = nullptr;
    }
    return *this;
}

unsigned char* JvmtiBuffer::release() noexcept
{
    unsigned char* data = data_;
    data_               = nullptr;
    return data;
}

void JvmtiBuffer::reset() noexcept
{
    if (env_ != nullptr && data_ != nullptr) {
        env_->Deallocate(data_);
    }
    env_  = nullptr;
    data_ = nullptr;
}

ca::Result<void, NativeError> JvmtiEnv::add_capabilities(
    const jvmtiCapabilities& capabilities) const
{
    if (env_ == nullptr) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument, "jvmtiEnv is null"));
    }
    jvmtiCapabilities copy  = capabilities;
    jvmtiError        error = env_->AddCapabilities(&copy);
    if (error != JVMTI_ERROR_NONE) {
        return ca::Err(make_jvmti_error(error, "AddCapabilities failed"));
    }
    return ca::Ok();
}

ca::Result<void, NativeError> JvmtiEnv::enable_event(jvmtiEvent event, jthread thread) const
{
    if (env_ == nullptr) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument, "jvmtiEnv is null"));
    }
    jvmtiError error = env_->SetEventNotificationMode(JVMTI_ENABLE, event, thread);
    if (error != JVMTI_ERROR_NONE) {
        return ca::Err(make_jvmti_error(error, "SetEventNotificationMode enable failed"));
    }
    return ca::Ok();
}

ca::Result<void, NativeError> JvmtiEnv::disable_event(jvmtiEvent event, jthread thread) const
{
    if (env_ == nullptr) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument, "jvmtiEnv is null"));
    }
    jvmtiError error = env_->SetEventNotificationMode(JVMTI_DISABLE, event, thread);
    if (error != JVMTI_ERROR_NONE) {
        return ca::Err(make_jvmti_error(error, "SetEventNotificationMode disable failed"));
    }
    return ca::Ok();
}

ca::Result<void, NativeError> JvmtiEnv::set_event_callbacks(
    const jvmtiEventCallbacks& callbacks) const
{
    if (env_ == nullptr) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument, "jvmtiEnv is null"));
    }
    jvmtiEventCallbacks copy  = callbacks;
    jvmtiError          error = env_->SetEventCallbacks(&copy, sizeof(copy));
    if (error != JVMTI_ERROR_NONE) {
        return ca::Err(make_jvmti_error(error, "SetEventCallbacks failed"));
    }
    return ca::Ok();
}

ca::Result<JvmtiArray<jclass>, NativeError> JvmtiEnv::loaded_classes() const
{
    if (env_ == nullptr) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument, "jvmtiEnv is null"));
    }
    jint       count   = 0;
    jclass*    classes = nullptr;
    jvmtiError error   = env_->GetLoadedClasses(&count, &classes);
    if (error != JVMTI_ERROR_NONE) {
        return ca::Err(make_jvmti_error(error, "GetLoadedClasses failed"));
    }
    return ca::Ok(JvmtiArray<jclass>(env_, classes, count));
}

ca::Result<std::string, NativeError> JvmtiEnv::class_signature(jclass klass) const
{
    if (env_ == nullptr) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument, "jvmtiEnv is null"));
    }
    if (klass == nullptr) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument, "jclass is null"));
    }
    char*      signature = nullptr;
    jvmtiError error     = env_->GetClassSignature(klass, &signature, nullptr);
    if (error != JVMTI_ERROR_NONE) {
        return ca::Err(make_jvmti_error(error, "GetClassSignature failed"));
    }
    JvmtiBuffer signature_buffer(env_, reinterpret_cast<unsigned char*>(signature));
    if (signature == nullptr) {
        return ca::Err(
            make_error(NativeErrorCode::kJvmtiFailure, "GetClassSignature returned null"));
    }
    return ca::Ok(std::string(signature));
}

ca::Result<void, NativeError> JvmtiEnv::retransform_class(jclass klass) const
{
    if (env_ == nullptr) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument, "jvmtiEnv is null"));
    }
    if (klass == nullptr) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument, "jclass is null"));
    }
    jclass     target = klass;
    jvmtiError error  = env_->RetransformClasses(1, &target);
    if (error != JVMTI_ERROR_NONE) {
        return ca::Err(make_jvmti_error(error, "RetransformClasses failed"));
    }
    return ca::Ok();
}

ca::Result<jvmtiEnv*, NativeError> get_jvmti_env(JavaVM* vm, jint version)
{
    if (vm == nullptr) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument, "JavaVM is null"));
    }
    jvmtiEnv* env    = nullptr;
    jint      result = vm->GetEnv(reinterpret_cast<void**>(&env), version);
    if (result != JNI_OK || env == nullptr) {
        return ca::Err(make_jni_error(result, "JavaVM::GetEnv(JVMTI) failed"));
    }
    return ca::Ok(env);
}

}   // namespace jninfra

#include "jninfra/jni/env.hpp"

#include <utility>

namespace jninfra {

LocalFrame::LocalFrame(JNIEnv* env, jint capacity) noexcept
    : env_(env)
{
    pushed_ = env_ != nullptr && env_->PushLocalFrame(capacity) == JNI_OK;
}

LocalFrame::~LocalFrame()
{
    pop();
}

LocalFrame::LocalFrame(LocalFrame&& other) noexcept
    : env_(other.env_)
    , pushed_(other.pushed_)
{
    other.env_    = nullptr;
    other.pushed_ = false;
}

LocalFrame& LocalFrame::operator=(LocalFrame&& other) noexcept
{
    if (this != &other) {
        pop();
        env_          = other.env_;
        pushed_       = other.pushed_;
        other.env_    = nullptr;
        other.pushed_ = false;
    }
    return *this;
}

void LocalFrame::pop() noexcept
{
    if (env_ != nullptr && pushed_) {
        env_->PopLocalFrame(nullptr);
    }
    env_    = nullptr;
    pushed_ = false;
}

LocalRef::LocalRef(JNIEnv* env, jobject ref) noexcept
    : env_(env)
    , ref_(ref)
{}

LocalRef::~LocalRef()
{
    reset();
}

LocalRef::LocalRef(LocalRef&& other) noexcept
    : env_(other.env_)
    , ref_(other.ref_)
{
    other.env_ = nullptr;
    other.ref_ = nullptr;
}

LocalRef& LocalRef::operator=(LocalRef&& other) noexcept
{
    if (this != &other) {
        reset();
        env_       = other.env_;
        ref_       = other.ref_;
        other.env_ = nullptr;
        other.ref_ = nullptr;
    }
    return *this;
}

jobject LocalRef::release() noexcept
{
    jobject ref = ref_;
    ref_        = nullptr;
    return ref;
}

void LocalRef::reset() noexcept
{
    if (env_ != nullptr && ref_ != nullptr) {
        env_->DeleteLocalRef(ref_);
    }
    env_ = nullptr;
    ref_ = nullptr;
}

GlobalRef::GlobalRef(JNIEnv* env, jobject local_ref)
{
    if (env != nullptr) {
        env->GetJavaVM(&vm_);
    }
    if (env != nullptr && local_ref != nullptr) {
        ref_ = env->NewGlobalRef(local_ref);
    }
}

GlobalRef::~GlobalRef()
{
    reset();
}

GlobalRef::GlobalRef(GlobalRef&& other) noexcept
    : vm_(other.vm_)
    , ref_(other.ref_)
{
    other.vm_  = nullptr;
    other.ref_ = nullptr;
}

GlobalRef& GlobalRef::operator=(GlobalRef&& other) noexcept
{
    if (this != &other) {
        reset();
        vm_        = other.vm_;
        ref_       = other.ref_;
        other.vm_  = nullptr;
        other.ref_ = nullptr;
    }
    return *this;
}

jobject GlobalRef::release() noexcept
{
    jobject ref = ref_;
    ref_        = nullptr;
    return ref;
}

void GlobalRef::reset() noexcept
{
    if (vm_ != nullptr && ref_ != nullptr) {
        JNIEnv* env      = nullptr;
        bool    attached = false;
        jint    result   = vm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8);
        if (result == JNI_EDETACHED) {
            result   = vm_->AttachCurrentThreadAsDaemon(reinterpret_cast<void**>(&env), nullptr);
            attached = result == JNI_OK;
        }
        if (result == JNI_OK && env != nullptr) {
            env->DeleteGlobalRef(ref_);
        }
        if (attached) {
            vm_->DetachCurrentThread();
        }
    }
    vm_  = nullptr;
    ref_ = nullptr;
}

UtfChars::UtfChars(JNIEnv* env, jstring value)
    : env_(env)
    , value_(value)
{
    if (env_ != nullptr && value_ != nullptr) {
        chars_ = env_->GetStringUTFChars(value_, nullptr);
    }
}

UtfChars::~UtfChars()
{
    reset();
}

UtfChars::UtfChars(UtfChars&& other) noexcept
    : env_(other.env_)
    , value_(other.value_)
    , chars_(other.chars_)
{
    other.env_   = nullptr;
    other.value_ = nullptr;
    other.chars_ = nullptr;
}

UtfChars& UtfChars::operator=(UtfChars&& other) noexcept
{
    if (this != &other) {
        reset();
        env_         = other.env_;
        value_       = other.value_;
        chars_       = other.chars_;
        other.env_   = nullptr;
        other.value_ = nullptr;
        other.chars_ = nullptr;
    }
    return *this;
}

std::string UtfChars::str() const
{
    return chars_ == nullptr ? std::string() : std::string(chars_);
}

void UtfChars::reset() noexcept
{
    if (env_ != nullptr && value_ != nullptr && chars_ != nullptr) {
        env_->ReleaseStringUTFChars(value_, chars_);
    }
    env_   = nullptr;
    value_ = nullptr;
    chars_ = nullptr;
}

ca::Result<void, NativeError> JniEnv::clear_exception_if_pending() const
{
    if (env_ == nullptr) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument, "JNIEnv is null"));
    }
    if (env_->ExceptionCheck() == JNI_TRUE) {
        env_->ExceptionClear();
    }
    return ca::Ok();
}

ca::Result<jclass, NativeError> JniEnv::find_class(const char* class_name) const
{
    if (env_ == nullptr || class_name == nullptr) {
        return ca::Err(
            make_error(NativeErrorCode::kInvalidArgument, "FindClass received null input"));
    }
    jclass klass = env_->FindClass(class_name);
    if (env_->ExceptionCheck() == JNI_TRUE) {
        env_->ExceptionClear();
        return ca::Err(make_error(NativeErrorCode::kJniFailure,
                                  std::string("FindClass failed: ") + class_name));
    }
    if (klass == nullptr) {
        return ca::Err(make_error(NativeErrorCode::kJniFailure,
                                  std::string("FindClass returned null: ") + class_name));
    }
    return ca::Ok(klass);
}

ca::Result<jmethodID, NativeError> JniEnv::method_id(jclass klass, const char* name,
                                                     const char* signature) const
{
    if (env_ == nullptr || klass == nullptr || name == nullptr || signature == nullptr) {
        return ca::Err(
            make_error(NativeErrorCode::kInvalidArgument, "GetMethodID received null input"));
    }
    jmethodID id = env_->GetMethodID(klass, name, signature);
    if (env_->ExceptionCheck() == JNI_TRUE) {
        env_->ExceptionClear();
        return ca::Err(make_error(NativeErrorCode::kJniFailure,
                                  std::string("GetMethodID failed: ") + name + signature));
    }
    if (id == nullptr) {
        return ca::Err(make_error(NativeErrorCode::kJniFailure,
                                  std::string("GetMethodID returned null: ") + name + signature));
    }
    return ca::Ok(id);
}

ca::Result<jfieldID, NativeError> JniEnv::field_id(jclass klass, const char* name,
                                                   const char* signature) const
{
    if (env_ == nullptr || klass == nullptr || name == nullptr || signature == nullptr) {
        return ca::Err(
            make_error(NativeErrorCode::kInvalidArgument, "GetFieldID received null input"));
    }
    jfieldID id = env_->GetFieldID(klass, name, signature);
    if (env_->ExceptionCheck() == JNI_TRUE) {
        env_->ExceptionClear();
        return ca::Err(make_error(NativeErrorCode::kJniFailure,
                                  std::string("GetFieldID failed: ") + name + signature));
    }
    if (id == nullptr) {
        return ca::Err(make_error(NativeErrorCode::kJniFailure,
                                  std::string("GetFieldID returned null: ") + name + signature));
    }
    return ca::Ok(id);
}

ca::Result<JNIEnv*, NativeError> attach_current_thread(JavaVM* vm, bool daemon)
{
    if (vm == nullptr) {
        return ca::Err(make_error(NativeErrorCode::kInvalidArgument, "JavaVM is null"));
    }

    JNIEnv* env    = nullptr;
    jint    result = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8);
    if (result == JNI_OK && env != nullptr) {
        return ca::Ok(env);
    }
    if (result != JNI_EDETACHED) {
        return ca::Err(make_jni_error(result, "JavaVM::GetEnv failed"));
    }

    result = daemon ? vm->AttachCurrentThreadAsDaemon(reinterpret_cast<void**>(&env), nullptr)
                    : vm->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr);
    if (result != JNI_OK || env == nullptr) {
        return ca::Err(make_jni_error(result, "AttachCurrentThread failed"));
    }
    return ca::Ok(env);
}

ca::Result<JavaVM*, NativeError> first_created_java_vm()
{
    JavaVM* vm     = nullptr;
    jsize   count  = 0;
    jint    result = JNI_GetCreatedJavaVMs(&vm, 1, &count);
    if (result != JNI_OK) {
        return ca::Err(make_jni_error(result, "JNI_GetCreatedJavaVMs failed"));
    }
    if (count == 0 || vm == nullptr) {
        return ca::Err(make_error(NativeErrorCode::kJvmUnavailable,
                                  "No created JavaVM is visible in this process"));
    }
    return ca::Ok(vm);
}

}   // namespace jninfra

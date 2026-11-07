#pragma once

/// @defgroup jninfra-jni-env jninfra-jni-env
/// @brief JNI JNIEnv 获取与异常检查封装。
///

#include <string>

#include "jninfra/error.hpp"
#include "libca/core/result.hpp"

namespace jninfra {

/// @brief RAII wrapper around PushLocalFrame/PopLocalFrame.
class LocalFrame
{
public:
    LocalFrame(JNIEnv* env, jint capacity) noexcept;
    ~LocalFrame();

    LocalFrame(const LocalFrame&)            = delete;
    LocalFrame& operator=(const LocalFrame&) = delete;

    LocalFrame(LocalFrame&& other) noexcept;
    LocalFrame& operator=(LocalFrame&& other) noexcept;

    /// @return true when PushLocalFrame succeeded and the destructor will pop it.
    bool pushed() const noexcept { return pushed_; }

private:
    void pop() noexcept;

    JNIEnv* env_    = nullptr;
    bool    pushed_ = false;
};

/// @brief Move-only owner for a JNI local reference on the current JNI thread.
class LocalRef
{
public:
    LocalRef() noexcept = default;
    LocalRef(JNIEnv* env, jobject ref) noexcept;
    ~LocalRef();

    LocalRef(const LocalRef&)            = delete;
    LocalRef& operator=(const LocalRef&) = delete;

    LocalRef(LocalRef&& other) noexcept;
    LocalRef& operator=(LocalRef&& other) noexcept;

    /// @return Borrowed local JNI reference.
    jobject get() const noexcept { return ref_; }

    /// @brief Release ownership without deleting the local reference.
    /// @return The previously owned reference.
    jobject  release() noexcept;
    explicit operator bool() const noexcept { return ref_ != nullptr; }

private:
    void reset() noexcept;

    JNIEnv* env_ = nullptr;
    jobject ref_ = nullptr;
};

/// @brief Move-only owner for a JNI global reference.
///
/// The object stores JavaVM instead of JNIEnv because JNIEnv is thread-local.
class GlobalRef
{
public:
    GlobalRef() noexcept = default;
    GlobalRef(JNIEnv* env, jobject local_ref);
    ~GlobalRef();

    GlobalRef(const GlobalRef&)            = delete;
    GlobalRef& operator=(const GlobalRef&) = delete;

    GlobalRef(GlobalRef&& other) noexcept;
    GlobalRef& operator=(GlobalRef&& other) noexcept;

    /// @return Borrowed global JNI reference.
    jobject get() const noexcept { return ref_; }

    /// @brief Release ownership without deleting the global reference.
    /// @return The previously owned reference.
    jobject  release() noexcept;
    explicit operator bool() const noexcept { return ref_ != nullptr; }

private:
    void reset() noexcept;

    JavaVM* vm_  = nullptr;
    jobject ref_ = nullptr;
};

/// @brief RAII wrapper around GetStringUTFChars/ReleaseStringUTFChars.
class UtfChars
{
public:
    UtfChars() noexcept = default;
    UtfChars(JNIEnv* env, jstring value);
    ~UtfChars();

    UtfChars(const UtfChars&)            = delete;
    UtfChars& operator=(const UtfChars&) = delete;

    UtfChars(UtfChars&& other) noexcept;
    UtfChars& operator=(UtfChars&& other) noexcept;

    /// @return Borrowed modified-UTF-8 C string, or nullptr on failure.
    const char* c_str() const noexcept { return chars_; }

    /// @return Copy of the current chars, or an empty string when unavailable.
    std::string str() const;
    explicit    operator bool() const noexcept { return chars_ != nullptr; }

private:
    void reset() noexcept;

    JNIEnv*     env_   = nullptr;
    jstring     value_ = nullptr;
    const char* chars_ = nullptr;
};

/// @brief Error-checked convenience wrapper for a borrowed JNIEnv pointer.
class JniEnv
{
public:
    explicit JniEnv(JNIEnv* env) noexcept
        : env_(env)
    {}

    /// @return Borrowed raw JNIEnv pointer.
    JNIEnv* raw() const noexcept { return env_; }

    /// @return true when the wrapper has a non-null JNIEnv.
    bool valid() const noexcept { return env_ != nullptr; }

    /// @brief Clear a pending Java exception if one exists.
    /// @return Ok on success, Err when the wrapped JNIEnv is null.
    ca::Result<void, NativeError> clear_exception_if_pending() const;

    /// @brief Find a Java class by JNI slash name.
    /// @param class_name JNI class name such as "java/lang/String".
    /// @return Local jclass reference on success.
    ca::Result<jclass, NativeError> find_class(const char* class_name) const;

    /// @brief Resolve an instance method ID with exception clearing.
    /// @param klass Class that owns the method.
    /// @param name Method name.
    /// @param signature JNI method descriptor.
    /// @return Resolved jmethodID.
    ca::Result<jmethodID, NativeError> method_id(jclass klass, const char* name,
                                                 const char* signature) const;

    /// @brief Resolve an instance field ID with exception clearing.
    /// @param klass Class that owns the field.
    /// @param name Field name.
    /// @param signature JNI field descriptor.
    /// @return Resolved jfieldID.
    ca::Result<jfieldID, NativeError> field_id(jclass klass, const char* name,
                                               const char* signature) const;

private:
    JNIEnv* env_ = nullptr;
};

/// @brief Attach the current native thread to a JVM if needed.
/// @param vm Target JVM.
/// @param daemon Attach as daemon thread when true.
/// @return Current thread's JNIEnv.
ca::Result<JNIEnv*, NativeError> attach_current_thread(JavaVM* vm, bool daemon = true);

/// @brief Return the first JavaVM visible through JNI_GetCreatedJavaVMs.
/// @return Borrowed JavaVM pointer.
ca::Result<JavaVM*, NativeError> first_created_java_vm();

}   // namespace jninfra

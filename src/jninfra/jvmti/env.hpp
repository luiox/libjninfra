#pragma once

/// @defgroup jninfra-jvmti-env jninfra-jvmti-env
/// @brief JVMTI jvmtiEnv 获取封装与 JVMTI 内存Owner。
///

#include <string>
#include <utility>

#include "jninfra/error.hpp"
#include "libca/core/result.hpp"

namespace jninfra {

/// @brief Move-only owner for memory allocated by JVMTI.
class JvmtiBuffer
{
public:
    JvmtiBuffer() noexcept = default;
    JvmtiBuffer(jvmtiEnv* env, unsigned char* data) noexcept;
    ~JvmtiBuffer();

    JvmtiBuffer(const JvmtiBuffer&)            = delete;
    JvmtiBuffer& operator=(const JvmtiBuffer&) = delete;

    JvmtiBuffer(JvmtiBuffer&& other) noexcept;
    JvmtiBuffer& operator=(JvmtiBuffer&& other) noexcept;

    /// @return Borrowed JVMTI-allocated buffer.
    unsigned char* get() const noexcept { return data_; }

    /// @brief Release ownership without calling jvmtiEnv::Deallocate.
    /// @return Previously owned buffer.
    unsigned char* release() noexcept;
    explicit       operator bool() const noexcept { return data_ != nullptr; }

private:
    void reset() noexcept;

    jvmtiEnv*      env_  = nullptr;
    unsigned char* data_ = nullptr;
};

/// @brief Move-only owner for JVMTI arrays returned through out-parameters.
/// @tparam T Element type returned by JVMTI.
template<typename T>
class JvmtiArray
{
public:
    JvmtiArray() noexcept = default;
    JvmtiArray(jvmtiEnv* env, T* data, jint count) noexcept
        : env_(env)
        , data_(data)
        , count_(count)
    {}
    ~JvmtiArray() { reset(); }

    JvmtiArray(const JvmtiArray&)            = delete;
    JvmtiArray& operator=(const JvmtiArray&) = delete;

    JvmtiArray(JvmtiArray&& other) noexcept
        : env_(other.env_)
        , data_(other.data_)
        , count_(other.count_)
    {
        other.env_   = nullptr;
        other.data_  = nullptr;
        other.count_ = 0;
    }

    JvmtiArray& operator=(JvmtiArray&& other) noexcept
    {
        if (this != &other) {
            reset();
            env_         = other.env_;
            data_        = other.data_;
            count_       = other.count_;
            other.env_   = nullptr;
            other.data_  = nullptr;
            other.count_ = 0;
        }
        return *this;
    }

    /// @return Borrowed array pointer.
    T* data() const noexcept { return data_; }

    /// @return Number of elements in the array.
    jint     count() const noexcept { return count_; }
    T&       operator[](jint index) const noexcept { return data_[index]; }
    T*       begin() const noexcept { return data_; }
    T*       end() const noexcept { return data_ + count_; }
    explicit operator bool() const noexcept { return data_ != nullptr; }

private:
    void reset() noexcept
    {
        if (env_ != nullptr && data_ != nullptr) {
            env_->Deallocate(reinterpret_cast<unsigned char*>(data_));
        }
        env_   = nullptr;
        data_  = nullptr;
        count_ = 0;
    }

    jvmtiEnv* env_   = nullptr;
    T*        data_  = nullptr;
    jint      count_ = 0;
};

/// @brief Error-checked convenience wrapper for a borrowed jvmtiEnv pointer.
class JvmtiEnv
{
public:
    explicit JvmtiEnv(jvmtiEnv* env) noexcept
        : env_(env)
    {}

    /// @return Borrowed raw JVMTI pointer.
    jvmtiEnv* raw() const noexcept { return env_; }

    /// @return true when the wrapper has a non-null jvmtiEnv.
    bool valid() const noexcept { return env_ != nullptr; }

    /// @brief Add capabilities to the JVMTI environment.
    /// @param capabilities Capability flags to request.
    /// @return Ok on success, Err with JVMTI error on failure.
    ca::Result<void, NativeError> add_capabilities(const jvmtiCapabilities& capabilities) const;

    /// @brief Enable a JVMTI event globally or for one thread.
    /// @param event Event to enable.
    /// @param thread Optional thread filter.
    /// @return Ok on success.
    ca::Result<void, NativeError> enable_event(jvmtiEvent event, jthread thread = nullptr) const;

    /// @brief Disable a JVMTI event globally or for one thread.
    /// @param event Event to disable.
    /// @param thread Optional thread filter.
    /// @return Ok on success.
    ca::Result<void, NativeError> disable_event(jvmtiEvent event, jthread thread = nullptr) const;

    /// @brief Install JVMTI event callbacks.
    /// @param callbacks Callback table copied
    /// by JVMTI.
    /// @return Ok on success.
    ca::Result<void, NativeError> set_event_callbacks(const jvmtiEventCallbacks& callbacks) const;

    /// @brief Read currently loaded classes.
    /// @note The returned array contains JNI
    /// local references for each jclass. JvmtiArray owns only the JVMTI-allocated array buffer;
    /// callers must manage the individual local references with LocalFrame or DeleteLocalRef.
    /// @return Move-only RAII array that deallocates through JVMTI.
    ca::Result<JvmtiArray<jclass>, NativeError> loaded_classes() const;

    /// @brief Read the JVM signature for a class.
    /// @param klass JNI class reference.

    /// * @return Signature such as `Ljava/lang/String;` or array signatures.
    ca::Result<std::string, NativeError> class_signature(jclass klass) const;

    /// @brief Request retransformation for one already loaded class.
    /// @param klass JNI
    /// class reference.
    /// @return Ok when JVMTI accepts the retransform request.
    ca::Result<void, NativeError> retransform_class(jclass klass) const;

private:
    jvmtiEnv* env_ = nullptr;
};

/// @brief Resolve a JVMTI environment from a JavaVM.
/// @param vm Target JVM.
/// @param version Requested JVMTI version.
/// @return Borrowed jvmtiEnv pointer.
ca::Result<jvmtiEnv*, NativeError> get_jvmti_env(JavaVM* vm, jint version = JVMTI_VERSION_1_2);

}   // namespace jninfra

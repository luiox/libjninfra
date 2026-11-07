#pragma once

/// @defgroup jninfra-error jninfra-error
/// @brief 跨 JNI / JVMTI / HotSpot 助手共享的错误模型。
///
/// 错误分类与构造契约见 doc:design-error-model。

#include <string>

#include "libca/core/datatype.hpp"
#include <jni.h>
#include <jvmti.h>

namespace jninfra {

/// @brief Error categories shared by JNI, JVMTI, and HotSpot helpers.
enum class NativeErrorCode : ca::u32
{
    kInvalidArgument          = 1,
    kJvmUnavailable           = 2,
    kJniFailure               = 3,
    kJvmtiFailure             = 4,
    kMissingVmStruct          = 5,
    kMissingVmType            = 6,
    kUnsupportedHotSpotLayout = 7,
    kIoFailure                = 8,
};

/// @brief Structured error returned by libjninfra Result APIs.
struct NativeError
{
    NativeErrorCode code = NativeErrorCode::kInvalidArgument;
    std::string     message;
    jint            jni_result  = JNI_OK;
    jvmtiError      jvmti_error = JVMTI_ERROR_NONE;
};

/// @brief Build a generic native-layer error.
/// @param code Stable error category.
/// @param message Human-readable diagnostic.
/// @return NativeError carrying @p code and @p message.
NativeError make_error(NativeErrorCode code, std::string message);

/// @brief Build an error for a failed JNI call.
/// @param result JNI status code returned by the JVM.
/// @param message Human-readable diagnostic.
/// @return NativeError with NativeErrorCode::kJniFailure.
NativeError make_jni_error(jint result, std::string message);

/// @brief Build an error for a failed JVMTI call.
/// @param error JVMTI error code returned by the JVM.
/// @param message Human-readable diagnostic.
/// @return NativeError with NativeErrorCode::kJvmtiFailure.
NativeError make_jvmti_error(jvmtiError error, std::string message);

}   // namespace jninfra

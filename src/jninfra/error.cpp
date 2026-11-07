#include "jninfra/error.hpp"

#include <utility>

namespace jninfra {

NativeError make_error(NativeErrorCode code, std::string message)
{
    NativeError error;
    error.code    = code;
    error.message = std::move(message);
    return error;
}

NativeError make_jni_error(jint result, std::string message)
{
    NativeError error = make_error(NativeErrorCode::kJniFailure, std::move(message));
    error.jni_result  = result;
    return error;
}

NativeError make_jvmti_error(jvmtiError jvmti_error, std::string message)
{
    NativeError error = make_error(NativeErrorCode::kJvmtiFailure, std::move(message));
    error.jvmti_error = jvmti_error;
    return error;
}

}   // namespace jninfra

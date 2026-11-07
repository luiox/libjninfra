#pragma once

/// @defgroup jninfra-dump-retransform jninfra-dump-retransform
/// @brief 与 RetransformClasses 配合的已加载类兜底抓取。
///

#include "jninfra/dump/class_filter.hpp"
#include "jninfra/jvmti/env.hpp"

namespace jninfra::dump {

/// @brief Counters from a loaded-class retransformation sweep.
struct RetransformStats
{
    ca::u32 seen          = 0;
    ca::u32 matched       = 0;
    ca::u32 retransformed = 0;
    ca::u32 failed        = 0;
};

/// @brief Retransform loaded classes that pass @p filter.
///
/// Retransformation triggers `ClassFileLoadHook` for compatible classes. When a
/// JNI environment is provided, local class references returned by JVMTI are
/// deleted as the sweep progresses.
ca::Result<RetransformStats, NativeError> retransform_loaded_classes(JNIEnv*                env,
                                                                     const JvmtiEnv&        jvmti,
                                                                     const ClassNameFilter& filter);

}   // namespace jninfra::dump

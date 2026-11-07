#pragma once

/// @defgroup jninfra-overview jninfra-overview
/// @brief libjninfra 聚合头：一次包含全部公共契约。
///
/// 分层与模块职责见 doc:design-overview。

#include "jninfra/dump/class_file.hpp"
#include "jninfra/dump/class_filter.hpp"
#include "jninfra/dump/class_name.hpp"
#include "jninfra/dump/retransform.hpp"
#include "jninfra/error.hpp"
#include "jninfra/hotspot/layout.hpp"
#include "jninfra/hotspot/metadata.hpp"
#include "jninfra/hotspot/method.hpp"
#include "jninfra/hotspot/runtime.hpp"
#include "jninfra/hotspot/vm_structs.hpp"
#include "jninfra/jni/env.hpp"
#include "jninfra/jvmti/env.hpp"

#pragma once

/// @defgroup jninfra-runtime jninfra-runtime
/// @brief HotSpot 运行时入口探测（gHotSpotVMStructs 等导出符号定位）。
///

#include "jninfra/hotspot/vm_structs.hpp"

namespace jninfra::hotspot {

/// @brief Load HotSpot VMStruct metadata from the current JVM process.
///
/// This uses HotSpot's exported Serviceability Agent tables:
/// `gHotSpotVMStructs`, `gHotSpotVMTypes`, `gHotSpotVMIntConstants`, and
/// `gHotSpotVMLongConstants`. Call this from a JNI/JVMTI agent after the JVM has
/// loaded `jvm.dll`/`libjvm`.
ca::Result<VmStructTable, NativeError> load_current_vm_structs();

}   // namespace jninfra::hotspot

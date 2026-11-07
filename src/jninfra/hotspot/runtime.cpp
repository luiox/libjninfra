#include "jninfra/hotspot/runtime.hpp"

#include <jni.h>

extern "C" JNIIMPORT jninfra::hotspot::VMStructEntry* gHotSpotVMStructs;
extern "C" JNIIMPORT jninfra::hotspot::VMTypeEntry* gHotSpotVMTypes;
extern "C" JNIIMPORT jninfra::hotspot::VMIntConstantEntry* gHotSpotVMIntConstants;
extern "C" JNIIMPORT jninfra::hotspot::VMLongConstantEntry* gHotSpotVMLongConstants;

namespace jninfra::hotspot {

ca::Result<VmStructTable, NativeError> load_current_vm_structs()
{
    VmStructTable table;
    auto          result = table.load(
        gHotSpotVMStructs, gHotSpotVMTypes, gHotSpotVMIntConstants, gHotSpotVMLongConstants);
    if (result.is_err()) {
        return ca::Err(result.unwrap_err());
    }
    return ca::Ok(std::move(table));
}

}   // namespace jninfra::hotspot

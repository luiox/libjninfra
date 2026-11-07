#pragma once

/// @defgroup jninfra-classfile-source jninfra-classfile-source
/// @brief 进程内/外部 class 字节来源可用性门禁（兼容性报告，不做 dump）。
///
/// 门禁判据与推断形态见 doc:design-classfile-source。

#include "jninfra/hotspot/vm_structs.hpp"

#include <string>

namespace jninfra::hotspot {

/// @brief External class-file byte source selected from HotSpot metadata.
///
/// P2 readers must not assume every HotSpot build exports the same class-byte
/// cache layout. This enum records the source that can be used safely according
/// to the copied VMStruct metadata.
enum class ClassFileByteSourceKind
{
    /// No supported external class-file byte source is available.
    kNone,
    /// `InstanceKlass::_cached_class_file` points at a `ClassFileData` payload.
    kInstanceKlassCachedClassFile,
    /// HotSpot 8 hidden cached-class-file slot points at `JvmtiCachedClassFileData`.
    kHotSpot8HiddenCachedClassFile,
};

/// @brief Compatibility report for externally copying original class bytes.
///
/// The report is intentionally layout-only. It describes whether the VMStruct
/// metadata exposes a known byte source, but it does not dereference target JVM
/// memory and does not prove that a particular class has cached bytes populated.
struct ClassFileByteSourceReport
{
    ClassFileByteSourceKind kind = ClassFileByteSourceKind::kNone;

    bool cached_class_file_field_present   = false;
    bool cached_class_file_is_instance     = false;
    bool cached_class_file_offset_inferred = false;
    bool class_file_data_type_present      = false;
    bool class_file_data_length_present    = false;
    bool class_file_data_bytes_present     = false;

    ca::u64 cached_class_file_offset = 0;
    ca::u64 class_file_length_offset = 0;
    ca::u64 class_file_bytes_offset  = 0;

    std::string unavailable_reason;
};

/// @brief Inspect VMStruct metadata for a supported external class-byte source.
///
/// The preferred P2 source is HotSpot's optional
/// `InstanceKlass::_cached_class_file` cache. It is usable when the pointer
/// field is present as an instance field and the `ClassFileData` layout exposes
/// both length and byte storage. Some HotSpot 8 builds hide the field from
/// VMStructs; for those, this gate can infer the slot from neighboring
/// `InstanceKlass` fields and use the known `JvmtiCachedClassFileData` payload
/// shape.
///
/// @param table Indexed HotSpot metadata table copied from the target JVM.
/// @return A layout-only source compatibility report.
ClassFileByteSourceReport inspect_class_file_byte_source(const VmStructTable& table);

}   // namespace jninfra::hotspot

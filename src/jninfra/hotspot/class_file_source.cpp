#include "jninfra/hotspot/class_file_source.hpp"

namespace jninfra::hotspot {
namespace {

bool is_instance_field(const VMStructEntry* entry)
{
    return entry != nullptr && entry->isStatic == 0;
}

bool try_infer_hotspot8_cached_slot(const VmStructTable& table, ClassFileByteSourceReport& report)
{
    const VMStructEntry* breakpoints = table.field("InstanceKlass", "_breakpoints");
    const VMStructEntry* idnum       = table.field("InstanceKlass", "_idnum_allocated_count");
    if (!is_instance_field(breakpoints) || !is_instance_field(idnum)) {
        return false;
    }

    constexpr ca::u64 kPointerSize = sizeof(void*);
    if (idnum->offset <= breakpoints->offset || idnum->offset < kPointerSize) {
        return false;
    }

    const ca::u64 gap = idnum->offset - breakpoints->offset;
    if (gap != kPointerSize * 3) {
        return false;
    }

    report.kind = ClassFileByteSourceKind::kHotSpot8HiddenCachedClassFile;
    report.cached_class_file_offset_inferred = true;
    report.cached_class_file_is_instance     = true;
    report.cached_class_file_offset          = idnum->offset - kPointerSize;
    report.class_file_length_offset          = 0;
    report.class_file_bytes_offset           = 4;
    report.unavailable_reason.clear();
    return true;
}

}   // namespace

ClassFileByteSourceReport inspect_class_file_byte_source(const VmStructTable& table)
{
    ClassFileByteSourceReport report;

    const VMStructEntry* cached_field      = table.field("InstanceKlass", "_cached_class_file");
    report.cached_class_file_field_present = cached_field != nullptr;
    if (cached_field == nullptr) {
        if (try_infer_hotspot8_cached_slot(table, report)) {
            return report;
        }
        report.unavailable_reason = "InstanceKlass::_cached_class_file missing";
        return report;
    }

    report.cached_class_file_is_instance = is_instance_field(cached_field);
    report.cached_class_file_offset      = cached_field->offset;
    if (!report.cached_class_file_is_instance) {
        report.unavailable_reason = "InstanceKlass::_cached_class_file is not an instance field";
        return report;
    }

    report.class_file_data_type_present = table.type("ClassFileData") != nullptr;
    if (!report.class_file_data_type_present) {
        report.unavailable_reason = "ClassFileData type missing";
        return report;
    }

    const VMStructEntry* length_field     = table.field("ClassFileData", "_length");
    report.class_file_data_length_present = is_instance_field(length_field);
    if (length_field != nullptr) {
        report.class_file_length_offset = length_field->offset;
    }
    if (!report.class_file_data_length_present) {
        report.unavailable_reason = "ClassFileData::_length missing";
        return report;
    }

    const VMStructEntry* bytes_field     = table.field("ClassFileData", "_bytes");
    report.class_file_data_bytes_present = is_instance_field(bytes_field);
    if (bytes_field != nullptr) {
        report.class_file_bytes_offset = bytes_field->offset;
    }
    if (!report.class_file_data_bytes_present) {
        report.unavailable_reason = "ClassFileData::_bytes missing";
        return report;
    }

    // The report is a capability gate only. Later P2 code still needs to read
    // each Klass pointer and reject null or suspicious cached payloads.
    report.kind = ClassFileByteSourceKind::kInstanceKlassCachedClassFile;
    report.unavailable_reason.clear();
    return report;
}

}   // namespace jninfra::hotspot

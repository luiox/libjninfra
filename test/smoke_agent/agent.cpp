#include "jninfra/hotspot/layout.hpp"
#include "jninfra/hotspot/method.hpp"
#include "jninfra/hotspot/runtime.hpp"

#include <cstdio>
#include <cstdlib>
#include <jni.h>
#include <jvmti.h>

namespace {

void print_error(const char* prefix, const jninfra::NativeError& error)
{
    std::fprintf(stderr, "[libjninfra-smoke] %s: %s\n", prefix, error.message.c_str());
}

void clear_jni_exception(JNIEnv* env, const char* context)
{
    if (env->ExceptionCheck() == JNI_FALSE) {
        return;
    }
    std::fprintf(stderr, "[libjninfra-smoke] JNI exception while %s\n", context);
    env->ExceptionDescribe();
    env->ExceptionClear();
}

[[noreturn]] void fail_smoke(const char* prefix, const jninfra::NativeError& error)
{
    print_error(prefix, error);
    std::fflush(stderr);
    std::exit(EXIT_FAILURE);
}

[[noreturn]] void fail_smoke(const char* message)
{
    std::fprintf(stderr, "[libjninfra-smoke] %s\n", message);
    std::fflush(stderr);
    std::exit(EXIT_FAILURE);
}

void JNICALL on_vm_init(jvmtiEnv*, JNIEnv* env, jthread)
{
    using jninfra::hotspot::inspect_method_layout;
    using jninfra::hotspot::load_current_vm_structs;
    using jninfra::hotspot::MethodView;

    // Java class lookup is intentionally delayed until VMInit. During
    // Agent_OnLoad the VM can still be partially initialized, and resolving
    // bootstrap classes may re-enter HotSpot too early.
    auto table_result = load_current_vm_structs();
    if (table_result.is_err()) {
        fail_smoke("failed to load HotSpot VMStruct tables", table_result.unwrap_err());
    }
    auto table = table_result.unwrap();

    auto layout = inspect_method_layout(table);
    if (layout.is_err()) {
        fail_smoke("unsupported HotSpot Method layout", layout.unwrap_err());
    }

    const auto info = layout.unwrap();
    std::fprintf(stderr,
                 "[libjninfra-smoke] Method size=%llu ConstMethod size=%llu "
                 "Method::_constMethod=%llu Method::_i2i_entry=%llu "
                 "Method::_access_flags=%llu ConstMethod::_code_size=%llu\n",
                 static_cast<unsigned long long>(info.method_size),
                 static_cast<unsigned long long>(info.const_method_size),
                 static_cast<unsigned long long>(info.method_const_method_offset),
                 static_cast<unsigned long long>(info.method_i2i_entry_offset),
                 static_cast<unsigned long long>(info.method_access_flags_offset),
                 static_cast<unsigned long long>(info.const_method_code_size_offset));

    jclass object_class = env->FindClass("java/lang/Object");
    if (object_class == nullptr) {
        clear_jni_exception(env, "finding java/lang/Object");
        fail_smoke("failed to find java/lang/Object");
    }

    jmethodID method_id = env->GetMethodID(object_class, "toString", "()Ljava/lang/String;");
    if (method_id == nullptr) {
        clear_jni_exception(env, "resolving java/lang/Object.toString");
        env->DeleteLocalRef(object_class);
        fail_smoke("failed to resolve java/lang/Object.toString");
    }

    // Resolve a real bootstrap method through the same metadata path used by
    // later runtime probes.
    auto method_metadata = MethodView::from_jmethod_id(&table, method_id).metadata();
    if (method_metadata.is_err()) {
        print_error("failed to resolve java/lang/Object.toString metadata",
                    method_metadata.unwrap_err());
        env->DeleteLocalRef(object_class);
        std::fflush(stderr);
        std::exit(EXIT_FAILURE);
    }

    const auto metadata = method_metadata.unwrap();
    std::fprintf(stderr,
                 "[libjninfra-smoke] java/lang/Object::%s descriptor=%s "
                 "code_size=%u max_stack=%u max_locals=%u\n",
                 metadata.name.c_str(),
                 metadata.descriptor.c_str(),
                 static_cast<unsigned>(metadata.code_size),
                 static_cast<unsigned>(metadata.max_stack),
                 static_cast<unsigned>(metadata.max_locals));

    env->DeleteLocalRef(object_class);
}

}   // namespace

extern "C" JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* vm, char*, void*)
{
    jvmtiEnv* jvmti = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&jvmti), JVMTI_VERSION_1_2) != JNI_OK ||
        jvmti == nullptr) {
        std::fprintf(stderr, "[libjninfra-smoke] failed to get JVMTI env\n");
        return JNI_ERR;
    }

    jvmtiEventCallbacks callbacks = {};
    callbacks.VMInit              = &on_vm_init;

    jvmtiError error = jvmti->SetEventCallbacks(&callbacks, static_cast<jint>(sizeof(callbacks)));
    if (error != JVMTI_ERROR_NONE) {
        std::fprintf(stderr, "[libjninfra-smoke] SetEventCallbacks failed: %d\n", error);
        return JNI_ERR;
    }

    error = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, nullptr);
    if (error != JVMTI_ERROR_NONE) {
        std::fprintf(stderr, "[libjninfra-smoke] enabling VMInit failed: %d\n", error);
        return JNI_ERR;
    }

    return JNI_OK;
}

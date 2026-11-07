#include "jninfra/dump/retransform.hpp"

#include "jninfra/dump/class_name.hpp"

#include <utility>

namespace jninfra::dump {

ca::Result<RetransformStats, NativeError> retransform_loaded_classes(JNIEnv*                env,
                                                                     const JvmtiEnv&        jvmti,
                                                                     const ClassNameFilter& filter)
{
    auto loaded = jvmti.loaded_classes();
    if (loaded.is_err()) {
        return ca::Err(loaded.unwrap_err());
    }

    auto             classes = std::move(loaded.storage().get<JvmtiArray<jclass>>());
    RetransformStats stats;
    for (jclass klass : classes) {
        ++stats.seen;

        auto signature = jvmti.class_signature(klass);
        if (signature.is_ok()) {
            const std::string raw_signature = signature.unwrap();
            const std::string class_name    = class_name_from_signature(raw_signature.c_str());
            if (filter.matches(class_name)) {
                ++stats.matched;
                auto retransformed = jvmti.retransform_class(klass);
                if (retransformed.is_ok()) {
                    ++stats.retransformed;
                }
                else {
                    ++stats.failed;
                }
            }
        }
        else {
            ++stats.failed;
        }

        if (env != nullptr && klass != nullptr) {
            env->DeleteLocalRef(klass);
        }
    }
    return ca::Ok(stats);
}

}   // namespace jninfra::dump

#include "jninfra/dump/class_name.hpp"

namespace jninfra::dump {

std::string class_name_from_signature(const char* signature)
{
    if (signature == nullptr || signature[0] == '\0') {
        return {};
    }
    std::string value(signature);
    if (value.size() >= 3 && value.front() == 'L' && value.back() == ';') {
        return value.substr(1, value.size() - 2);
    }
    return {};
}

bool is_jdk_class_name(const std::string& class_name)
{
    return class_name.rfind("java/", 0) == 0 || class_name.rfind("javax/", 0) == 0 ||
           class_name.rfind("jdk/", 0) == 0 || class_name.rfind("sun/", 0) == 0 ||
           class_name.rfind("oracle/", 0) == 0;
}

}   // namespace jninfra::dump

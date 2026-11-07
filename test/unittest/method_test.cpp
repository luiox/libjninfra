/// MethodView::from_jmethod_id 的基址语义单测。
///
/// jmethodID 语义随 JDK 大版本不同（实证 JDK 17.0.8 + "Mysterious jmethodID"）：
///   - JDK ≤ 8：jmethodID 直接就是 Method*；
///   - JDK 9+（本库目标）：jmethodID 是间接句柄，指向的小 wrapper 首字才是
///     Method*。把 jmethodID 直接当 Method* 用会读到 garbage，后续字段偏移
///     读取在 near-null 处 SIGSEGV。
/// 本测试用伪造 wrapper 把「首字放 Method 地址」钉死：实现必须取首字。
#include "jninfra/hotspot/method.hpp"

#include <cstring>

#include <gtest/gtest.h>

using jninfra::hotspot::MethodView;
using jninfra::hotspot::VMIntConstantEntry;
using jninfra::hotspot::VMLongConstantEntry;
using jninfra::hotspot::VMStructEntry;
using jninfra::hotspot::VMTypeEntry;
using jninfra::hotspot::VmStructTable;

namespace {

struct FakeConstMethod
{
    void*          constants;
    unsigned short name_index;
    unsigned short signature_index;
    unsigned short code_size;
    unsigned short max_stack;
    unsigned short max_locals;
};

struct FakeMethod
{
    FakeConstMethod* const_method;
};

/// JDK 9+ 的 jmethodID wrapper：首字为 Method*。
struct FakeJmethodIdWrapper
{
    void* method;
};

/// 仅含 Method::_constMethod（offset 0）的最小 VMStruct 表。VmStructTable::load
/// 非拥有地记录数组指针，调用方须保证四个哨兵数组与 table 同生命周期。
struct MinimalMethodTable
{
    VMStructEntry       structs[2];
    VMTypeEntry         types[2];
    VMIntConstantEntry  ints[1];
    VMLongConstantEntry longs[1];
    VmStructTable       table;

    MinimalMethodTable()
    {
        structs[0] = {"Method", "_constMethod", "address", 0, 0, nullptr};
        structs[1] = {nullptr, nullptr, nullptr, 0, 0, nullptr};
        types[0]   = {"Method", nullptr, 0, 0, 0, sizeof(FakeMethod)};
        types[1]   = {nullptr, nullptr, 0, 0, 0, 0};
        ints[0]    = {nullptr, 0};
        longs[0]   = {nullptr, 0};
        ok         = table.load(structs, types, ints, longs).is_ok();
    }

    bool ok;
};

}   // namespace

TEST(MethodViewTest, FromJmethodIdResolvesIndirectionToMethodPointer)
{
    MinimalMethodTable mt;
    ASSERT_TRUE(mt.ok);

    FakeConstMethod      const_method_storage{};
    FakeMethod           method_storage{&const_method_storage};
    FakeJmethodIdWrapper wrapper{&method_storage};

    // jmethodID 指向 wrapper：view 必须经 wrapper 首字取到 Method 基址，
    // 而不是把 jmethodID 自身当基址。
    auto       id   = reinterpret_cast<jmethodID>(&wrapper);
    MethodView view = MethodView::from_jmethod_id(&mt.table, id);

    EXPECT_TRUE(view.valid());
    EXPECT_EQ(view.address(), &method_storage);

    auto const_method = view.const_method();
    ASSERT_TRUE(const_method.is_ok());
    EXPECT_EQ(const_method.unwrap().address(), &const_method_storage);
}

TEST(MethodViewTest, FromJmethodIdNullIdIsInvalid)
{
    MinimalMethodTable mt;
    ASSERT_TRUE(mt.ok);

    MethodView view = MethodView::from_jmethod_id(&mt.table, nullptr);
    EXPECT_FALSE(view.valid());
    EXPECT_EQ(view.address(), nullptr);
}

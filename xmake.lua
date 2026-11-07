-- libjninfra：JNI/JVMTI 边界与 HotSpot 内部结构只读视图库（独立构建根）。
--
-- 依赖：
--   - libca（luiox-repo git 直连包）：core 为运行依赖，test 为测试门面
--     依赖；版本锁固定 0.0.16，升级须显式换版。
--   - gtest（xrepo，仅 --with_tests=y）。
--   - JDK：必须设置 JAVA_HOME（jni.h / jvmti.h 与 jvm 链接库），未设置时
--     配置阶段直接报错——本库是纯 JNI 库，静默跳过等于假安装。
--
-- 仓库注册：本文件不声明 add_repositories——包沙箱会继承宿主的仓库注册，
-- 在工程内重复注册同名仓库会导致嵌套包构建时 remote 路径错乱。独立开发/CI
-- 先执行：
--   xmake repo --add luiox-repo https://github.com/luiox/luiox-repo.git

set_languages("cxx17")
add_rules("mode.debug", "mode.release")

add_requires("libca 0.0.16", {configs = {modules = "core,test"}})

option("with_tests")
    set_default(false)
    set_showmenu(true)
    set_description("Enable unit tests and smoke agent (pulls gtest)")
option_end()

if has_config("with_tests") then
    add_requires("gtest")
end

local java_home = os.getenv("JAVA_HOME")
if not java_home then
    raise("JAVA_HOME is not set. Required for libjninfra (jni.h / jvmti.h / jvm).")
end

target("libjninfra")
    set_kind("static")
    set_languages("cxx17")
    set_warnings("all")

    -- Linux 上 smoke_agent（shared）会链接本静态库，非 PIC 目标代码里对
    -- gHotSpotVM* 导出符号取址在 -shared 链接时报 R_X86_64_PC32；显式全 PIC。
    set_policy("build.fpic", true)

    add_packages("libca")
    add_files("src/jninfra/**.cpp")
    add_headerfiles("src/(jninfra/**.hpp)")
    add_includedirs("src", {public = true})

    -- JNI/JVMTI 头随公开头传递（jninfra 公开头的签名里出现 jint/jvmtiError），
    -- 消费方只需自带 JAVA_HOME 环境，不必手工加 include。
    add_includedirs(path.join(java_home, "include"), {public = true})
    if is_plat("windows") then
        add_includedirs(path.join(java_home, "include", "win32"), {public = true})
        add_cxflags("/utf-8", {tools = "cl"})
    elseif is_plat("linux") then
        add_includedirs(path.join(java_home, "include", "linux"), {public = true})
        add_syslinks("pthread")
    elseif is_plat("macosx") then
        add_includedirs(path.join(java_home, "include", "darwin"), {public = true})
        add_syslinks("pthread")
    end

-- 单元测试（--with_tests=y 时定义；gmock 随 gtest 包提供）。
-- 全部用例基于假 VMStruct 表与文件系统，不要求目标机有 JVM 运行时。
if has_config("with_tests") then

target("libjninfra_unittest")
    set_kind("binary")
    set_default(false)
    add_deps("libjninfra")
    add_packages("libca", "gtest")
    add_files("test/unittest/*.cpp")
    add_includedirs("test_helper")
    set_rundir("$(projectdir)")

    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    elseif is_plat("linux") then
        add_syslinks("pthread")
    end

-- 真机 smoke agent（-agentpath 挂载；验证 metadata 全链路，见 doc/design.rst）。
target("libjninfra_smoke_agent")
    set_kind("shared")
    set_default(false)
    add_deps("libjninfra")
    add_packages("libca")
    add_files("test/smoke_agent/*.cpp")
    add_includedirs("src")

    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
        add_linkdirs(path.join(java_home, "lib"))
        add_links("jvm")
    elseif is_plat("linux") then
        add_syslinks("pthread")
        add_linkdirs(path.join(java_home, "lib", "server"))
        add_links("jvm")
    elseif is_plat("macosx") then
        add_syslinks("pthread")
        add_linkdirs(path.join(java_home, "lib", "server"))
        add_links("jvm")
    end

end

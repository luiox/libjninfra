# libjninfra：JNI/JVMTI 基础设施库

JVM native 基础设施库：封装 JNI / JVMTI 边界，并基于 HotSpot 导出的 `gHotSpotVMStructs` 元数据表提供进程内只读访问 JVM 内部结构（`Method` / `ConstMethod` / `ConstantPool` / `Symbol`）的视图层。定位是「寄生在目标 JVM 进程内，让 JVM 自己跑完运算，在结果落到内存/栈帧时取走数据」，避免在 native 侧模拟 JVM 执行，为 JVM 运行时类 dump 与探针类工具提供 native 地基。

## 模块（src/jninfra/）

- **hotspot/**：VMStruct 表装载与快照（`vm_structs.hpp`）、HotSpot 运行时入口探测（`runtime.hpp`）、`Method`/`ConstMethod`/`ConstantPool`/`Symbol` 只读元数据视图（`method.hpp`/`metadata.hpp`）、class 字节来源可用性门禁（`class_file_source.hpp`）与布局校验（`layout.hpp`）。jmethodID 语义随 JDK 版本不同：JDK ≤8 直接是 `Method*`；JDK 9+（本库目标）是间接句柄，wrapper 首字才是 `Method*`（JDK 17.0.8 实测，`MethodView::from_jmethod_id` 按间接取首字实现，真机 smoke 已验证 metadata 全链路）。
- **jni/**：JNI `JNIEnv` 获取与异常检查封装（`env.hpp`）。
- **jvmti/**：JVMTI `jvmtiEnv` 获取封装（`env.hpp`）。
- **dump/**：基于 JVMTI 的类转储（`class_file.hpp`）、类名过滤器（`class_filter.hpp`）、类名定位（`class_name.hpp`）与 retransform 配合（`retransform.hpp`）。

## 明确未实现（roadmap 规划项，勿当现有能力引用）

- Detours / inline hook 后端：roadmap 第 4 层规划项，源码未落地。
- ConstantCallSite 拦截、解释器断点：probe 层规划项。

## 构建

依赖 JDK 与 luiox-repo 包仓（libca git 直连包），须先设置 `JAVA_HOME` 并注册包仓：

```sh
xmake repo --add luiox-repo https://github.com/luiox/luiox-repo.git
xmake f --with_tests=y
xmake build libjninfra                # 库
xmake build libjninfra_unittest       # 单测（含 from_jmethod_id 基址语义回归；假表，无需 JVM）
xmake run libjninfra_unittest --gtest_brief=1
xmake build libjninfra_smoke_agent    # 真机 smoke agent（-agentpath 挂载）
# 真机验证（JDK 9+）：
# java -agentpath:<repo>/build/windows/x64/release/libjninfra_smoke_agent.dll -version
```

## 文档索引

- [设计规格（RST）](doc/_build/html/index.html)：`doc/design.rst` 承载宏观契约——错误模型、VMStruct 快照、元数据视图、字节来源门禁、dump 过滤器；`python doc/check_refs.py .` 校验代码侧 `doc:<label>` 引用。
- `doc/roadmap.md`：分层推进计划（metadata → snapshot → probe → hook）。

## 注意事项

1. **GC 引发的指针失效**：在 C++ 层长期持有 Java 对象指针时，必须用 `NewGlobalRef` 或禁止 GC；否则 Full GC 后对象移动、指针变野，进程 SIGSEGV。
2. **线程自挂死锁**：JVMTI 回调（如 `SingleStep`）内部严禁挂起当前线程等待自身释放资源。正确做法是零延迟读内存，或另起监控线程异步处理。
3. **I-Cache 一致性**（面向 roadmap hook 层）：inline hook 修改 JVM 代码段后，Windows 上须调 `FlushInstructionCache`，Linux 上须调 `__builtin___clear_cache`，否则 CPU 流水线里的旧指令导致 hook 时灵时不灵。
4. **真机验证**：视图层字段偏移依赖 VMStruct 表与真实 HotSpot 布局一致，真机 smoke（`libjninfra_smoke_agent` 挂真 JVM 跑 metadata 全链路）是发布前置条件。

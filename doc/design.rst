.. _design-overview:

设计规格
=================================

分层
----

libjninfra 按「边界 → 布局 → 视图 → dump」四层组织。边界层封装 JNI / JVMTI 环境获取与错误模型；布局层装载 HotSpot 导出的 VMStruct 元数据表并在其上做兼容性校验；视图层把布局元数据翻译成对 ``Method``\、\ ``ConstMethod``\、\ ``ConstantPool``\、\ ``Symbol`` 的只读借用视图；dump 层在 JVMTI 事件与 retransform 路径上消费视图完成类字节转储。定位是「寄生在目标 JVM 进程内，让 JVM 自己跑完运算，在结果落到内存/栈帧时取走数据」，避免在 native 侧模拟 JVM 执行。

模块地图见 ``jninfra-overview`` 头注。各层只依赖更低的层；平台差异（进程访问、PE 导出解析、远端内存读取）不属于本库，由平台侧 dump 工具承担。

.. _design-error-model:

错误模型
--------

全库公开 API 走 ``Result`` 化返回，错误统一收敛到 ``jninfra-error`` 头注定义的 ``NativeError``：稳定分类 ``NativeErrorCode``\ 加人类可读诊断串，并按来源附 ``jni_result`` / ``jvmti_error`` 原始码。分类覆盖非法参数、JVM 不可达、JNI/JVMTI 调用失败、VMStruct 字段或类型缺失、HotSpot 布局不支持与 IO 失败。构造入口只有 ``make_error``\ 、\ ``make_jni_error``\ 、\ ``make_jvmti_error`` 三个工厂，调用方不得手填原始码字段。

.. _design-metadata-views:

HotSpot 元数据视图
------------------

目的
~~~~

元数据视图层给上层一条窄的、可测试的路径，从存活 JVM 中检视方法身份与字节码规模，是后续运行时探针（如 ConstantCallSite 捕获、解释器断点）的地基。模块刻意只读：把 HotSpot 导出的布局元数据翻译成借用型 C++ 视图，上层需要留存的信息一律拷贝出 ``MethodMetadata`` 值。

架构
~~~~

三层构件：``VmStructTable`` 索引 HotSpot 导出的 VMStruct 与 VMType 表；``SymbolView`` 与 ``ConstantPoolView`` 解析方法名与描述符；``MethodView`` 与 ``ConstMethodView`` 把 ``jmethodID`` 连接到方法元数据、字节码边界、max stack 与 max locals。

视图从不拥有 JVM 内存，只携带指向已索引表的指针与借用的 HotSpot 对象地址。``jninfra-method-views`` 头注是这一层的微观契约入口。

布局策略
~~~~~~~~

HotSpot 对象布局不编译进本库，字段偏移与类型尺寸一律运行时从当前 JVM 读取：

- ``Method::_constMethod`` 定位不可变方法负载。
- ``ConstMethod::_constants``\ 、\ ``_name_index``\ 、\ ``_signature_index`` 定位方法名与描述符符号。
- ``ConstantPool`` 条目区起始于导出的 ``ConstantPool`` VM 类型尺寸之后。
- ``Symbol::_length`` 与 ``Symbol::_body`` 定义拷贝进普通 C++ 字符串的字节串。
- ``ConstMethod`` 字节码区起始于导出的 ``ConstMethod`` VM 类型尺寸之后。

jmethodID 语义随 JDK 版本不同：JDK ≤8 直接是 ``Method*``；JDK 9+ 是间接句柄，wrapper 首字才是 ``Method*``（JDK 17.0.8 实测）。``MethodView::from_jmethod_id`` 按间接取首字实现，单元测试锁定该语义。

校验模型
~~~~~~~~

``inspect_method_layout()`` 是本模块的兼容性门禁：探针依赖视图之前，先确认当前 JVM 导出了方法元数据检视所需的全部类型与字段。单元测试用假 VMStruct 表与假 HotSpot 对象保证偏移处理确定性；真机 smoke agent（``libjninfra_smoke_agent``）在真实 JVM 内校验运行时布局、解析已知 Java 方法并打印拷贝出的元数据。

边界
~~~~

本模块不解析任意常量池 tag、不改写字节码、不安装 hook、不拥有任何 JNI/JVMTI 生命周期；这些职责属于构建在元数据原语之上的后续模块。

.. _design-vmstructs:

HotSpot 外部 VMStruct 快照
--------------------------

目的
~~~~

外部快照层（``jninfra-vmstructs`` 头注）桥接外部内存读取路径与 HotSpot 布局视图。从另一个进程读取 HotSpot Serviceability Agent 表时，原始表指针与被引用字符串都无法直接索引；快照把这些行拷贝进本进程内存、接管全部拷贝字符串的所有权，再在稳定的本地数组上暴露正常的 ``VmStructTable``。

架构
~~~~

``VmStructSnapshot`` 持有四族拷贝表：VMStruct 行（字段与静态地址）、VMType 行（类型尺寸）、VMIntConstant 行、VMLongConstant 行。调用方提供不含哨兵的拷贝 ``*EntryData`` 行，快照构建与 HotSpot 导出布局一致的哨兵终止数组，再在其上装载 ``VmStructTable``——保证进程内探针与外部内存探针的字段查找行为完全一致。

所有权模型
~~~~~~~~~~

快照拥有字符串与行数组；经 ``snapshot.table()`` 返回的视图在快照清除或销毁前有效。``VmStructSnapshot`` 刻意不可拷贝、不可移动，因为其索引表存储指向自有行数组的指针。兼容性诊断场景下，快照也可借出不含哨兵行的 VMStruct/VMType 行数组；生产读取路径仍应走 ``snapshot.table()``，让布局访问集中收口。

静态字段 ``address`` 保持原义：若行拷贝自另一进程，这些地址是远端目标地址，仅可作为调用方自有内存读取器的元数据，绝不允许在本进程内直接解引用。

边界
~~~~

本模块不枚举进程、不解析 PE 导出、不挂起线程、不调用远端内存读取 API——这些平台职责在平台侧 dump 工具。class 字节提取路径应先运行 :ref:`design-classfile-source` 描述的门禁，再进入遍历。

.. _design-classfile-source:

class 字节来源门禁
------------------

目的
~~~~

class 字节来源门禁回答一个问题：目标 HotSpot VM 是否暴露了可用于「不执行目标进程代码、直接拷出原始 ``.class`` 字节」的布局。它是兼容性检查而非 dump 器——只读 ``VmStructSnapshot`` 持有的 VMStruct 元数据拷贝，不解引用目标内存，也不判定某个具体类当前是否有字节缓存。

判据
~~~~

``inspect_class_file_byte_source()`` 返回 ``ClassFileByteSourceReport``。首个受支持来源是 ``InstanceKlass::_cached_class_file``\，\ 应指向 ``ClassFileData`` 负载。来源判可用当且仅当以下元数据全部导出：

- ``InstanceKlass::_cached_class_file`` 作为实例字段存在；
- ``ClassFileData`` 作为 VM 类型存在；
- ``ClassFileData::_length`` 与 ``ClassFileData::_bytes`` 作为实例字段存在。

任一缺失时报告保持 ``kind = kNone``\，\ 并附供 CLI 诊断的简短原因。

部分 HotSpot 8 构建会在 VMStructs 里隐藏 ``InstanceKlass::_cached_class_file`` 与 ``JvmtiCachedClassFileData``\，\ 尽管字段在原生布局中存在。对该家族，门禁可从导出的 ``InstanceKlass::_breakpoints`` 与 ``InstanceKlass::_idnum_allocated_count`` 偏移推断缓存字节槽位，推断的负载形态为 ``jint length`` 后接内联类字节。该推断刻意按版本形态收紧：把 ``_idnum_allocated_count`` 排在 ``_breakpoints`` 之前的现代 JDK 布局不会命中。

消费约定
~~~~~~~~

平台侧 dump 工具在外部 VMStruct 快照拷贝完成后打印该报告；后续字节拷贝层在遍历命中的 ``Klass`` 条目之前消费同一报告。即便缓存字节布局可用，读取器仍须逐类拒绝空缓存指针、可疑长度与远端读取失败。

.. _design-dump-filter:

dump 类名过滤器
----------------

目的
~~~~

``jninfra-dump-filter`` 头注的 ``ClassNameFilter`` 是 dump 家族共享的类名策略。把过滤行为从工具特定回调里抽出来，事件钩子路径与 retransform 全量扫描路径才能执行同一套类名规则。

名称形态
~~~~~~~~

主输入是 JVM 内部斜线形类名（如 ``com/example/Foo``\）\；正则规则同时用点分形（如 ``com.example.Foo``）测试，因为命令行用户两种写法都会自然出现。

求值序
~~~~~~

过滤器按固定顺序应用规则：

#. 拒绝空名；
#. ``exclude_jdk`` 开启时拒绝 JDK 类；
#. 存在任何 include 规则时，include 子树/精确名规则与 include 正则规则构成白名单；
#. 应用 exclude 子树/精确名规则；
#. 应用 exclude 正则规则。

非法正则条目直接忽略。这保证 dump 回调是 best-effort 的：一条坏 pattern 不会扰动目标 JVM 的稳定性。

所有权
~~~~~~

过滤器不知道 CLI 选项、输出目录与 JVMTI 事件时机。调用方把用户意图转换成 ``ClassNameFilterConfig``\，\ 然后在事件钩子抓取与 retransform 扫描两条路径上复用同一个 ``ClassNameFilter``。

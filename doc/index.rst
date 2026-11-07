libjninfra 设计规格
=================================

libjninfra 是 JVM native 基础设施库：封装 JNI / JVMTI 边界，并基于 HotSpot 导出的 VMStruct 元数据表提供进程内只读访问 JVM 内部结构的视图层。本目录承载宏观设计规格；符号级契约在头文件 ``///``\ 注释（Doxygen 微观层）；流程与路线图留在 ``doc/roadmap.md``\（markdown 方向层）。

.. toctree::
   :maxdepth: 2

   design

索引
----

- ``jninfra-overview``\ 头注：模块地图与聚合头契约。
- 代码侧 ``doc:<label>`` 引用由 ``doc/check_refs.py``\ 门禁校验（规范见 CONTRIBUTING.md「文档规范」）。

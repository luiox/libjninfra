# CONTRIBUTING

## 开发流程

- 一切改动走 PR：从 `main` 拉 `fix/**` / `feat/**` / `docs/**` 分支，CI 全绿后 squash 合入。
- 提交信息用中文短句描述语义变化，不携带与本仓无关的出处引用。

## 构建

依赖 JDK（`JAVA_HOME` 必须设置）与 luiox-repo 包仓（libca）。见 README「构建」。

## 文档规范

本仓执行三层文档体系，目标是让文档随代码走、漂移可被门禁拦截：

1. **微观层（头文件 `///` 注释）**：符号契约与 `@defgroup` 组注。组注里用 `doc:<label>` 指向宏观规格。
2. **宏观层（`doc/*.rst`）**：设计规格，Sphinx `-W` 构建，警告即失败。中文写作约定：
   - 行内标记后紧跟全角开括号或汉字会被 docutils 判 start-string/end-string，用反斜杠空格转义（``x``\（）；全角闭括号（。、：）安全。
   - 标题下划线长度按东亚列宽计算（汉字计 2 列）。
   - 禁用 grid table，表格一律 list-table。
   - 段落一行一段，不硬折行（渲染器自动折行）。
3. **方向层（`*.md`）**：只收流程与人读文档（README / CONTRIBUTING / roadmap）。代码注释与 RST 不得引用设计内容的 markdown。

### 引用门禁

`python doc/check_refs.py .` 校验：

- 代码侧 `doc:<label>` 必须有 `doc/**.rst` 的 `.. _<label>:` 声明；
- RST 侧 `:ref:` / `:doc:` 目标必须在册；
- RST「``X`` 头注」的 `X` 必须是真实 `@defgroup` 组名或头文件名；
- 匿名链接禁止；跨仓出处必须带仓名标记（libca / luiox 等）。

CI 的 docs job 依次跑 check_refs → sphinx `-E -W --keep-going` → Doxygen（WARN_AS_ERROR）。三者任一告警即失败。

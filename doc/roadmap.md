# libjninfra Roadmap

## Ordered Native JVM Probe Plan

`libjninfra` is being built in layers. Keep later hook work behind smaller metadata and probe primitives so every step can be tested against a real JVM.

## 1. HotSpot Method Metadata Views

Scope for `[libjninfra] add HotSpot method metadata views`:

- Add HotSpot `Symbol` and `ConstantPool` views.
- Extend `MethodView` / `ConstMethodView` so callers can resolve method name, method descriptor, bytecode, max stack, and max locals from a `jmethodID`.
- Extend layout validation and smoke tests to verify the metadata path on a real HotSpot VM.

**Status: verified.** The metadata path (jmethodID → Method → ConstMethod → ConstantPool → Symbol → name/descriptor/code_size/max_stack/max_locals) passed the live smoke on JDK 17.0.8 Windows/x64 via `libjninfra_smoke_agent` (`java/lang/Object.toString` → `()Ljava/lang/String;`, code_size=36, max_stack=2, max_locals=1). jmethodID semantics confirmed: JDK ≤8 is a direct `Method*`; JDK 9+ is an indirection whose first word is the `Method*` — see `MethodView::from_jmethod_id` and its unit tests.

Design note: see `doc/design.rst`「HotSpot 元数据视图」.

This layer is the prerequisite for both ConstantCallSite capture and interpreter breakpoint probes.

## 2. HotSpot External VMStruct Snapshot

Scope（外部 VMStruct 快照层）:

- Add copied-row data types for VMStruct, VMType, int constant, and long constant tables.
- Add `VmStructSnapshot` so external-memory readers can own externally copied strings and expose a stable `VmStructTable` over local sentinel-terminated arrays.
- Keep process access, PE export lookup, and remote memory reads outside `libjninfra`.

Design note: see `doc/design.rst`「HotSpot 外部 VMStruct 快照」.

This layer is the prerequisite for external ClassLoaderDataGraph traversal.

## 3. ConstantCallSite Capture MVP

After method metadata is available, add a runtime probe focused on `java/lang/invoke/ConstantCallSite`:

- Prefer JVMTI/JNI-visible events and Java-visible object inspection first.
- Record enough method/class context to map `invokedynamic` sites to resolved stable targets.
- Fall back to HotSpot hook support only when the JVMTI/JNI path cannot observe the required transition.

## 4. Interpreter Breakpoint Backend

The final step is the platform-sensitive backend:

- HotSpot only.
- x64 first.
- Require explicit layout and VM version validation before install.
- Hook install/uninstall must flush the instruction cache on supported platforms.
- The backend must remain optional; `libjninfra` default targets should not depend on Detours-style hook libraries.

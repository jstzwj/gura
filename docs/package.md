# Gura 包、模块与导入设计草案

本文描述 Gura 当前 package / module / import 的实现状态和后续路线。

## 1. 概念分层

Gura 将源码组织分为三层：

```text
package  编译、发布和依赖管理单位
module   package 内的命名空间单位
import   跨 module 引用符号的机制
```

当前编译器已经支持目录作为 package 输入，并支持基础 `module` / `import` 语法与函数解析。

## 2. Package

命令行输入可以是单个文件：

```bash
gura build hello.gura -o hello
```

也可以是目录：

```bash
gura build examples/my_package -o my_package
```

当输入是目录时，driver 会：

1. 递归收集目录下所有 `.gura` 文件。
2. 按相对路径字典序排序，保证编译顺序稳定。
3. 分别 lex/parse 每个文件。
4. 保留每个文件的 module/import 元数据。
5. 暂时仍提供 flattened compatibility view 给现有 sema/codegen。
6. 生成一个可执行文件或一个 LLVM module。

支持目录输入的命令：

```text
parse
check
emit-llvm
build
```

`lex` 当前仍然是单文件命令。

## 3. Module

每个源码文件属于一个 module。

显式 module：

```gura
module math.vector

fn add(a: i64, b: i64): i64 {
  return a + b
}
```

如果没有显式 `module`，driver 会从 package 相对路径推导 module path：

```text
main.gura          -> module main
math/vector.gura   -> module math.vector
util/log.gura      -> module util.log
```

当前不实现 `mod.gura` 特殊语义。

## 4. Import

当前支持两种导入：

```gura
import math
import math.vector as vec
```

普通导入把目标 module 的顶层函数带入 simple lookup：

```gura
module math

fn add(a: i64, b: i64): i64 {
  return a + b
}
```

```gura
module app
import math

fn main(): i64 {
  return add(40, 2) - 42
}
```

Alias import 通过 alias qualified call 使用：

```gura
module app
import math.vector as vec

fn main(): i64 {
  return vec.add(40, 2) - 42
}
```

也可以直接使用 absolute qualified call：

```gura
fn main(): i64 {
  return math.vector.add(40, 2) - 42
}
```

## 5. 当前名称解析规则

函数查找当前按以下顺序进行：

1. 当前 module 内的函数。
2. 普通 `import module.path` 导入的函数。
3. Alias import 的 qualified path，例如 `vec.add(...)`。
4. Absolute qualified path，例如 `math.vector.add(...)`。
5. compiler-known prelude/builtin，例如 `println_i64(...)` 和 `std.core.println_i64(...)`。
6. 兼容旧 flat package 的 simple name fallback。

不同 module 可以定义同名函数，codegen 会对非入口函数使用 module-aware mangling，避免 LLVM symbol 冲突。`fn main(): i64` 仍作为可执行入口生成 `@main`。

## 6. 当前限制

当前还不支持：

- selective import：`import foo.{A, B}`。
- re-export。
- `pub` / `export` 可见性。
- package manifest，例如 `gura.toml`。
- 跨 package 依赖。
- package 名称、版本、target 配置。
- module-scoped struct/type 的完整解析与 codegen mangling。

当前 module/import 首先覆盖函数调用路径；类型、trait、impl、method 的完整 module-aware 解析还会继续收敛。

## 7. 与标准库的关系

标准库最终应该是普通 package 或多个 package，而不是永久写死在编译器中。

当前 `std.core.print`、`std.core.println_i64` 等路径仍然是 compiler-known builtin。后续应迁移为 synthetic `std.core` module，然后再逐步替换为真实 `std/` 源码：

```text
std/
  core.gura
  io.gura
  collections/
    vec.gura
```

## 8. 后续路线

建议路线：

1. 当前阶段：目录 package、`module`、`import`、alias import、module-aware function codegen。
2. 增加 module-aware type/struct/trait/impl lookup。
3. 增加 visibility：区分 package 内可见、公开导出和私有实现。
4. 增加 package manifest：定义 package 名称、入口、源码根、依赖。
5. 增加跨 package dependency graph。
6. 把标准库从 compiler-known builtin 逐步迁移为 `std` package 源码。

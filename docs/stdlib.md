# Gura 标准库设计草案

本文描述 Gura 标准库的设计方向、分层方式和实现路线。它不是最终标准，而是当前编译器原型继续演进 `std` 的设计基线。

## 1. 目标

Gura 标准库的目标不是一开始就做成庞大的通用库，而是围绕语言核心能力逐步提供稳定抽象：

1. **让最小程序可观察**：提供 `print`、`println`、`readln` 等输入输出能力。
2. **承接语言内建能力**：把字符串、数组、区域、错误处理、任务并发等能力逐步从编译器 builtin 迁移到标准库接口。
3. **保持内存模型一致**：标准库类型必须遵守 region / capability / bridge 规则，不能绕开语言安全模型。
4. **可分阶段实现**：早期允许少量 compiler-known builtin；成熟后尽量用 Gura 自身代码实现。
5. **低层可控，高层易用**：系统编程需要显式控制分配、生命周期和 I/O；普通程序也需要简洁的默认 prelude。

## 2. 总体分层

标准库建议分为三层：

```text
std API 层
  用户可见模块，例如 std.core、std.io、std.mem、std.collections

std implementation 层
  用 Gura 编写的标准库实现，调用更低层 primitive 或 runtime ABI

compiler/runtime primitive 层
  编译器内建函数、LLVM/libc lowering、Gura runtime C++ ABI
```

早期原型可以先跳过 `std implementation` 层，让 `std.core.print_i64` 这类函数直接由编译器 lowering 到 libc 或 runtime。随着语言支持模块、泛型、trait、字符串和包管理，再把实现迁移到 Gura 源码。

## 3. 模块结构

推荐最终模块结构如下：

```text
std.core          最小默认核心：print/println/assert/panic/basic conversions
std.io            文件、stdin/stdout/stderr、Reader/Writer、buffered I/O
std.mem           region、allocation strategy、manual/arena utilities
std.collections   Array、Vec、List、Map、Set 等集合
std.string        String、StringBuilder、字符串转换和格式化
std.task          task、cown、channel、并发调度抽象
std.result        Result、Option 风格错误处理和组合工具
std.math          数值函数、min/max、整数工具
std.test          测试断言和测试 runner 支持
```

当前阶段不需要一次实现所有模块。最先落地的应该是 `std.core`。

## 4. `std.core`

`std.core` 是默认可用的最小核心库。它应该通过 prelude 自动导入，因此用户可以直接写：

```gura
println("hello")
println_i64(42)
```

当前原型已经支持一组精确的 `std.core` builtin 路径：

```gura
std.core.print("hello")
std.core.println("hello")
std.core.print_i64(42)
std.core.println_i64(42)
let n: i64 = std.core.readln_i64()
```

裸函数名来自 prelude alias，而不是全局命名空间污染。当前这仍然不是完整模块/import 系统：只有上面这些 `std.core.<builtin>` 路径会被编译器识别，其他 `std.*` 路径暂不解析。

### 4.1 当前原型 API

当前编译器已经适合先实现以下 core/prelude builtin：

```gura
print("hello")        // 输出字符串，不换行
println("hello")      // 输出字符串并换行
print_i64(42)         // 输出 i64，不换行
println_i64(42)       // 输出 i64 并换行
readln_i64()          // 从 stdin 读取一个 i64
```

这些函数暂时作为 compiler-known builtin 存在，而不是由 `std/core.gura` 实现。

原因：

- 语言还没有真正的模块/import 系统。
- 还没有函数重载，因此不能同时支持 `println("x")` 和 `println(42)`。
- 还没有 owned `String`，因此 `readln(): String` 暂时不能安全表达。
- 还没有 trait-based formatting，因此不能实现通用 `print<T>`。

### 4.2 成熟 API

当模块、重载或 trait 可用后，`std.core` 应收敛为：

```gura
fn print(value: impl Display): unit
fn println(value: impl Display): unit
fn readln(): String
fn panic(message: String): Never
fn assert(condition: bool): unit
fn assert(condition: bool, message: String): unit
```

如果 Gura 不采用函数重载，也可以使用 trait 静态分派或显式命名：

```gura
println(value)
println_i64(value)
println_bool(value)
println_string(value)
```

长期更推荐 `Display`/format trait，因为标准库和用户类型都可以扩展输出行为。

## 5. Prelude

Prelude 是每个源文件默认可见的一小组名字。它不等于整个标准库，只包含最常用且低风险的接口。

建议 prelude 初始包含：

```text
std.core.print
std.core.println
std.core.print_i64
std.core.println_i64
std.core.readln_i64
```

未来可以加入：

```text
std.core.assert
std.core.panic
std.result.Option
std.result.Result
std.collections.Array
std.string.String
```

Prelude 规则：

1. prelude 名字可以被用户局部绑定遮蔽，但不能与同作用域声明冲突。
2. 完整路径 `std.core.println` 永远指向标准库定义。
3. compiler builtin 只是实现细节，不应成为长期语言语义。

## 6. 模块与路径解析

当前 parser 把 `a.b(...)` 解析为字段访问或方法调用，因此 `std.core.println(...)` 还不能正确表示模块路径。

未来需要引入路径 AST：

```text
Path ::= ident ("." ident)*
Call ::= Path "(" args ")"
```

并在 sema 中区分：

```text
value.field       字段访问
value.method()    方法调用
Module.path.fn()  模块函数调用
Type.assoc_fn()   关联函数调用
```

建议使用名称解析阶段统一处理：

1. 先解析局部变量、参数和类型名。
2. 再解析当前模块导入。
3. 再解析 prelude。
4. 最后解析完整 `std.*` 路径。

## 7. 标准库与编译器 builtin 的关系

早期 Gura 可以把以下能力作为 builtin：

```text
puts / print / println / readln_i64
array indexing and assignment
region enter / exit / freeze / merge
allocation strategy metadata
```

但长期应该区分三类能力：

### 7.1 语言 primitive

这些是语言语义的一部分，不能只是普通库函数：

```text
move
freeze
merge
enter/explore
capability checking
region lifetime checking
```

它们需要编译器理解类型和生命周期。

### 7.2 runtime intrinsic

这些是低层 ABI，可由标准库包装：

```text
__gura_region_new_iso
__gura_region_enter
__gura_region_exit
__gura_region_freeze
__gura_region_merge
```

用户不应直接调用这些符号。

### 7.3 library function

这些应该最终由标准库实现：

```text
print / println
Vec.push
String.append
Array.len
Result.map
```

早期可以 compiler-lower，后期迁移到 Gura 源码。

## 8. I/O 设计

### 8.1 最小 I/O

当前阶段只需要同步 stdout/stdin：

```gura
print("answer: ")
println_i64(42)
let n: i64 = readln_i64()
```

实现方式：

```text
print/println      -> printf
print_i64          -> printf("%lld", value)
println_i64        -> printf("%lld\n", value)
readln_i64         -> scanf("%lld", &slot)
```

这是原型阶段可接受的实现，因为可执行文件已经通过 clang/linker 链接 libc。

### 8.2 完整 I/O

成熟标准库应提供：

```gura
struct File
struct Stdin
struct Stdout
trait Reader
trait Writer

fn stdin(): Stdin
fn stdout(): Stdout
fn stderr(): Stdout
```

接口示例：

```gura
let line: String = std.io.stdin().read_line()
std.io.stdout().write_line(line)
```

需要 `String`、错误处理和资源释放能力后再实现。

### 8.3 错误处理

I/O 不应长期用裸返回值或 panic。推荐：

```gura
fn read_line(reader: mut Reader): Result<String, IoError>
fn write(writer: mut Writer, bytes: imm [u8]): Result<unit, IoError>
```

当前没有 `Result`、泛型数组和 `u8` 时，可以先保留简单 builtin。

## 9. String 设计

`String` 是 `readln()`、formatting 和常规 I/O 的前置能力。

建议：

```gura
struct String {
    // opaque runtime-managed buffer
}
```

能力关系：

```text
String           默认不可变值或拥有值
mut String       可修改 buffer
imm String       可共享不可变字符串
iso String       可跨 region/task 转移
```

字符串字面量初期类型可以继续是 `cstring`，成熟后应为：

```gura
"hello" : imm String
```

或者保留两个层次：

```text
cstring      FFI/静态只读 C 字符串
String       标准库 owned 字符串
```

推荐长期保留 `cstring` 作为 FFI 类型，但普通用户默认看到 `String`。

## 10. Array / Vec / Slice

当前 `[i64]` 是固定长度本地数组能力，主要用于 quicksort 里程碑。

标准库应逐步提供：

```gura
Array<T, N>       固定长度数组
Slice<T>          借用视图，不拥有元素
Vec<T>            可增长数组，拥有 buffer
```

示例：

```gura
let data: mut Vec<i64> = Vec.new()
data.push(5)
data.push(2)
quicksort(data.as_mut_slice())
```

region 关系：

- `mut Slice<T>` 是对某个 region 中连续内存的可变借用。
- `Vec<T>` 的 buffer 属于创建它的 region。
- `iso Vec<T>` 可以作为闭合 region 的桥跨任务移动。
- 不允许 `mut Slice<T>` 逃逸出其 region 生命周期。

## 11. Region-aware 标准库

Gura 的标准库必须把 region 作为基础约束，而不是后期补丁。

例如：

```gura
let arena: iso Vec<i64> = new iso<Arena> Vec<i64> {}
enter arena as data {
    data.push(1)
    data.push(2)
}
```

标准库容器的分配策略应继承当前 active region，或显式指定：

```gura
Vec.with_strategy<Arena>()
Vec.with_strategy<RC>()
Vec.with_strategy<GC>()
Vec.with_strategy<Manual>()
```

规则：

1. 容器内部对象属于容器所在 region。
2. region closed 后，外部不能持有内部 `mut` 引用。
3. `imm` 容器可共享读取。
4. `iso` 容器可跨任务或 region 移动。
5. `Manual` 策略只能通过安全 API 或 unsafe API 暴露逐对象释放。

## 12. FFI 边界

标准库需要一层 FFI 模块包装 C ABI：

```text
std.ffi.c.printf
std.ffi.c.scanf
std.ffi.c.malloc
std.ffi.c.free
```

普通用户不应直接用 FFI 完成核心任务。`std.core.print` 应包装底层 FFI，并把不安全细节隐藏起来。

FFI 类型建议：

```text
cstring
cptr<T>
usize
isize
```

FFI 调用默认 unsafe，除非标准库提供安全包装。

## 13. 实现路线

### 阶段 0：compiler-known core builtins

目标：让程序可见输出和最小输入可用。

```text
print(cstring)
println(cstring)
print_i64(i64)
println_i64(i64)
readln_i64() -> i64
```

实现位置：

```text
src/hir/Sema.cpp              注册 builtin 签名
src/codegen/LLVMCodeGen.cpp   lowering 到 printf/scanf
```

### 阶段 1：package MVP

目标：先让目录可以作为 package 输入，多文件 `.gura` 源码能合并为一个编译单元。

当前支持：

```text
单文件 package
目录 package
多文件 flat namespace
package-wide sema/codegen
```

这一步为后续 `std/` 源码目录打基础，但还不是完整 module/import 系统。

### 阶段 2：模块路径与 prelude

目标：让 `std.core.println` 成为真实路径，裸 `println` 只是 prelude alias。

需要：

```text
Path AST
import/prelude name resolution
module symbol table
标准库源码目录，例如 std/core.gura
```

### 阶段 3：String 与 formatting

目标：支持：

```gura
println("hello")
println(42)
let line: String = readln()
```

需要：

```text
String 类型
Display/Format trait 或函数重载
Result/IoError
runtime/string buffer
```

### 阶段 4：collections

目标：把当前固定数组能力推广到标准容器：

```text
Array
Slice
Vec
List
Map
Set
```

需要：

```text
泛型 codegen 或 monomorphization
len/capacity
bounds check 策略
region-aware allocation
```

### 阶段 5：I/O、文件和任务

目标：完整系统编程标准库：

```text
File
Path
Reader/Writer
Task
Channel
Cown helpers
```

需要：

```text
错误处理
资源释放/drop
并发 runtime API 包装
跨平台抽象
```

## 14. 当前建议

短期内，`std` 应该采用“package 基础先行、compiler builtin 过渡、标准库接口后补”的路线：

1. 先让 `print` / `println_i64` / `readln_i64` 可用。
2. 再支持目录 package，让未来 `std/` 源码目录有编译单元基础。
3. 再做模块路径和 prelude，让这些 builtin 看起来属于 `std.core`。
4. 再引入 `String`、`Display` 和真正的 `println(value)`。
5. 最后把 builtin lowering 收敛为标准库源码调用 runtime/FFI primitive。

这样既能快速支撑 hello world、quicksort、调试输出，又不会过早锁死完整标准库设计。

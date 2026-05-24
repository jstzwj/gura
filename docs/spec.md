# gura 语言规范草案

本文档定义 gura 的核心规范。它不是完整标准，而是编译器原型和后续形式化工作的设计基线。

## 1. 语言层次

gura 可分为四个语义层：

1. **表层语言**：用户编写的 Rust / Swift / Kotlin-like 语法。
2. **核心命令语言**：A-normal 风格表达式、显式移动、赋值、调用和控制流。
3. **区域语言**：显式区域栈、堆、加载、交换、分配、打开、关闭、冻结、合并。
4. **运行时并发层**：任务、cown 获取/释放、调度和同步。

规范重点是第 2 和第 3 层的交互。

## 2. 程序实体

### 2.1 值

```text
v ::= pure
    | ref(cap, object_id)
    | cown_id
    | none
```

`cap` 是引用能力：

```text
cap ::= mut | tmp | iso | imm | pau
```

`var` 是存储槽能力，不是值能力。

### 2.2 对象

```text
object ::= Object(class_id, fields)
fields ::= field_name -> value
```

每个可变对象属于唯一一个区域。冻结对象位于冻结堆。

### 2.3 区域

```text
region ::= Region(region_id, store, bridge_id, strategy)
store ::= object_id -> object
```

### 2.4 配置

```text
Config ::= <RS; H_open; H_closed; H_frozen; H_cown>
RS ::= RegionFrame*
RegionFrame ::= (region_id, temp_store, locals)
```

栈顶 RegionFrame 对应 active 区域。其下 frame 对应 paused 区域。

## 3. 静态语义

### 3.1 类型环境

类型判断形式：

```text
Γ ⊢ expr : τ ⊣ Γ'
```

含义：在输入环境 `Γ` 下，表达式 `expr` 类型为 `τ`，并产生输出环境 `Γ'`。输出环境用于表达移动后变量失效、`var` 强更新和模式窄化。

### 3.2 类型

```text
τ ::= K C[τ*]
    | Store[S, τ]
    | τ?
    | τ | τ
    | (τ*)
    | fn(τ*) -> τ
    | Never

K ::= mut | tmp | iso | imm | pau | cown
S ::= var | field | tmp_slot
```

### 3.3 能力谓词

```text
copyable(τ)     // 可复制
movable(τ)      // 可移动
send_safe(τ)    // 可跨任务边界
return_safe(τ)  // 可从当前区域块返回
openable(τ)     // 可 enter/explore
```

基本规则：

- `copyable(imm T)` 为真；
- `copyable(cown T)` 为真；
- `copyable(iso T)` 为假；
- `send_safe(iso T | imm T | cown T | pure)` 为真；
- `send_safe(mut T | tmp T | pau T)` 为假；
- `return_safe(iso T | imm T | pure)` 为真；
- `return_safe(mut T | tmp T | pau T)` 默认为假。

## 4. 变量与移动

### 4.1 普通读取

若 `Γ(x) = τ` 且 `copyable(τ)`，则：

```text
Γ ⊢ x : τ ⊣ Γ
```

### 4.2 iso 读取必须移动

若 `Γ(x) = iso T`，普通读取非法。必须使用：

```text
Γ[x: iso T] ⊢ move x : iso T ⊣ Γ[x: undef]
```

### 4.3 var 存储槽

`var x: τ = e` 产生：

```text
Γ(x) = Store[var, τ]
```

读取 `var` 中的 `iso` 也必须 `move` 或交换。

### 4.4 赋值强更新

若：

```text
Γ ⊢ rhs : τ2 ⊣ Γ1
Γ1(place) = Store[var, τ1]
```

则：

```text
Γ ⊢ place = rhs : unit ⊣ Γ1[place: Store[var, τ2]]
```

对字段赋值不改变字段声明类型，只要求赋值类型与字段类型兼容。

## 5. 原始值类型与数值字面量

Gura 的原始值类型使用小写名称：`i32`、`i64`、`f32`、`f64`、`bool`、`unit`、`none`。

```ebnf
integer_literal ::= decimal_digits integer_suffix?
float_literal   ::= decimal_digits "." decimal_digits float_suffix?
integer_suffix  ::= "i32" | "i64"
float_suffix    ::= "f32" | "f64"
```

无后缀整数字面量类型为 `i64`，无后缀浮点字面量类型为 `f64`。数值字面量后缀决定该字面量的精确类型，语言当前不对数值类型做隐式提升或上下文推断；例如 `i32` 返回位置需要写 `0i32`，而不是依赖 `0` 从默认 `i64` 转换。

同类型的 `i32`、`i64`、`f32`、`f64` 可以使用 `+`、`-`、`*`、`/`；`%` 仅适用于同类型的 `i32` 或 `i64`。大小比较要求两侧是同一种数值类型，结果为 `bool`。

十六进制、二进制、八进制、指数形式和 `_` 分隔符保留给后续扩展。

## 6. 视点适配

字段访问判断：

```text
Γ ⊢ x : K C ⊣ Γ1
field_type(C, f) = Kf T
K ⊙ Kf = Kr
Γ ⊢ x.f : Kr T ⊣ Γ1
```

若 `K ⊙ Kf = ⊥`，字段不可访问。

### 5.1 适配表

| `K ⊙ Kf` | `mut` | `tmp` | `imm` | `iso` | `paused` |
| --- | --- | --- | --- | --- | --- |
| `mut` | `mut` | `⊥` | `imm` | `⊥/iso-place` | `⊥` |
| `tmp` | `mut` | `tmp` | `imm` | `⊥/iso-place` | `pau` |
| `imm` | `imm` | `imm` | `imm` | `imm` | `imm` |
| `iso` | `⊥` | `⊥` | `⊥` | `⊥` | `⊥` |
| `pau` | `pau` | `pau` | `imm` | `⊥/iso-place` | `pau` |

`⊥/iso-place` 表示不能作为普通值读取，但可以作为存储位置参与 `enter *slot`、`move` 或 `swap`。

## 6. 对象分配

### 6.1 new mut

```text
Γ ⊢ args : fields(T) ⊣ Γ'
active_region_exists
Γ ⊢ new mut T(args) : mut T ⊣ Γ'
```

效果：对象加入 active 区域。

### 6.2 new tmp

```text
Γ ⊢ new tmp T(args) : tmp T
```

限制：只能在区域块内出现；结果不得逃逸。

### 6.3 new iso

```text
Γ ⊢ args : iso_constructor_args(T) ⊣ Γ'
Γ ⊢ new iso[S] T(args) : iso T ⊣ Γ'
```

效果：创建新 closed 区域，桥对象为 `T`，内存策略为 `S`。

构造参数允许：

- 纯值；
- `imm`；
- 被移动的 `iso`，作为嵌套区域；
- 编译器可证明不会造成逃逸的临时 `mut` 构造值。

### 6.4 new imm

```text
Γ ⊢ new imm T(args) : imm T
```

语义等价于创建闭合区域后冻结，但实现可直接分配到冻结堆。

## 7. enter 静态规则

若：

```text
Γ ⊢ source : iso T ⊣ Γ1
suspend(Γ1) = Γpaused
Γpaused, binding: mut T ⊢ body : τ ⊣ Γbody
return_safe(τ)
no_escape(binding_region, Γbody)
```

则：

```text
Γ ⊢ enter source as binding { body } : τ ⊣ restore(Γ1, Γbody)
```

### 7.1 suspend

进入内层区域时，外层环境中：

- `mut T` -> `pau T`；
- `tmp T` -> 不可捕获，除非生命周期严格内嵌且不逃逸；
- `Store[var, T]` -> `paused Store[T]`；
- `iso T` 保持 `iso T`，但仍不可复制；
- `imm T` 保持 `imm T`；
- `cown T` 保持 `cown T`。

### 7.2 返回限制

`enter` 块不能返回指向其 active 区域内部的 `mut`、`tmp`、`pau` 引用。

合法：

```gura
enter tree as t {
    return freeze(move t.child_region)
}
```

非法：

```gura
let leaked = enter tree as t {
    t.left   // mut Node，不能返回
}
```

### 7.3 动态打开检查

通过局部 `iso` 变量打开区域通常可静态证明未打开。通过字段或存储槽打开时，可能存在别名路径，需运行期检查目标区域是否已在当前区域栈中。

## 8. explore 静态规则

`explore source as binding { body }` 要求 `source: iso T`，块内 `binding: pau T`。动态上，目标区域被打开后立即作为 paused 视图暴露，`body` 在词法受限的临时 active 上下文中执行。

规则：

- 不能调用 `self: mut` 方法；
- 不能写入被探索区域；
- 可以创建 `tmp` 对象保存 paused 引用；
- `tmp`、`pau` 和指向被探索区域内部的引用不能逃逸；
- 返回值必须 `return_safe`；
- 被探索区域的不变式在整个块内视为稳定。

## 9. freeze 规则

```text
Γ ⊢ e : iso T ⊣ Γ'
Γ ⊢ freeze(e) : imm T ⊣ Γ'
```

动态要求：区域闭合。若静态上 `e` 是当前可用 `iso`，闭合通常已保证；若来自 cown 或 FFI，需要运行期闭合检查。

## 10. merge 规则

```text
Γ ⊢ e : iso T ⊣ Γ'
active_region_exists
Γ ⊢ merge(e) : mut T ⊣ Γ'
```

效果：源区域并入 active 区域。源区域的直接嵌套区域仍保持闭合嵌套区域。

## 11. cown 规则

### 11.1 创建

```text
Γ ⊢ e : iso T ⊣ Γ'
Γ ⊢ cown(e) : cown T ⊣ Γ'
```

或：

```text
Γ ⊢ e : imm T ⊣ Γ'
Γ ⊢ cown(e) : cown T ⊣ Γ'
```

### 11.2 获取

若：

```text
Γ ⊢ c : cown T ⊣ Γ1
suspend(Γ1) = Γpaused
Γpaused, binding: mut T ⊢ body : τ ⊣ Γbody
return_safe(τ)
no_escape(binding_region, Γbody)
```

则：

```text
Γ ⊢ acquire c as binding { body } : τ ⊣ restore(Γ1, Γbody)
```

释放时运行期检查 cown 内部区域闭合。

## 12. spawn 规则

```text
Γ ⊢ arg_i : τ_i ⊣ Γ_i
∀i. send_safe(τ_i)
Γ ⊢ spawn f(args...) : TaskHandle ⊣ Γ_n
```

`iso` 参数必须通过 `move` 传递。`imm` 与 `cown` 参数可共享。

## 13. 动态语义概要

动态语义由命令语言产生效果，区域语言消费效果。

### 13.1 效果集合

```text
Eff ::= load(dest, place)
      | swap(dest_old, place, value)
      | move(dest, place)
      | alloc_mut(dest, class, args)
      | alloc_tmp(dest, class, args)
      | alloc_iso(dest, strategy, class, args)
      | enter(region_ref, binding)
      | exit(region_id, new_bridge, result)
      | freeze(dest, iso_ref)
      | merge(dest, iso_ref)
      | cown_new(dest, value)
      | cown_acquire(cown_ids)
      | cown_release(cown_ids)
      | spawn(fn, args)
```

### 13.2 load

`load(dest, x.f)`：

1. 查找 `x` 指向对象；
2. 读取字段 `f`；
3. 根据 receiver capability 做视点适配；
4. 绑定到 `dest`。

### 13.3 swap

`swap(dest_old, place, value)`：

1. 检查 place 可写；
2. 读取旧值到 `dest_old`；
3. 写入新值；
4. 更新区域拓扑元数据；
5. 若违反拓扑不变式，触发确定性错误。

### 13.4 enter

`enter(region_ref, binding)`：

1. 检查 region_ref 指向 closed 区域桥对象；
2. 若区域已 open，失败；
3. 将区域从 `H_closed` 移到 `H_open`；
4. 压入 RegionFrame；
5. `binding` 绑定为 `mut bridge`；
6. 外层 frame 变为 paused。

### 13.5 exit

`exit(region_id, new_bridge, result)`：

1. 检查 region_id 是栈顶 active 区域；
2. 检查无非法逃逸引用；
3. 设置新桥对象；
4. 弹出 RegionFrame；
5. 把区域移回 `H_closed`；
6. 外层 paused 区域恢复 active。

### 13.6 freeze

`freeze(dest, iso_ref)`：

1. 检查 iso_ref 指向 closed 区域；
2. 遍历该区域及嵌套区域；
3. 所有对象移动到 `H_frozen`；
4. 结果以 `imm` 绑定到 dest。

### 13.7 merge

`merge(dest, iso_ref)`：

1. 检查 iso_ref 指向 closed 区域；
2. 把源区域对象并入 active 区域；
3. 删除源区域边界；
4. 结果以 `mut` 绑定到 dest。

## 14. 区域写屏障

即使静态类型系统能证明大多数写入安全，运行时仍定义写屏障作为实现规范和 unsafe/FFI 兜底。

写入 `src.field = tgt` 时：

1. 若 `src` 属于 frozen 或 cown，拒绝写入；
2. 若 `src` 与 `tgt` 同区域，允许；
3. 若 `tgt` 是 `imm` 或 cown，允许；
4. 若 `src` 在 active 区域且 `tgt` 在 paused 区域，只有当目标存储位置是 `tmp` 时允许；
5. 若 `tgt` 是 closed 区域桥对象，要求外部唯一并更新区域父子关系；
6. 其他跨区域可变引用拒绝。

## 15. 闭合判定

区域 closed 当且仅当：

1. 它不在任何线程区域栈中；
2. 没有从局部栈、tmp 对象或其他非桥外部位置指向其内部对象的引用；
3. 外部入边至多一个，且指向桥对象；
4. 所有嵌套区域也满足对应外部唯一约束。

实现可用静态生命周期证明、局部借用计数或调试遍历验证闭合性。

## 16. 数据竞争自由性质

定义直接访问为读取或写入对象字段，且不经过 cown acquire 协议。

定理草案：对任何安全 gura 程序，如果两个线程同时直接访问同一对象，则该对象在 `H_frozen` 中；因此不存在一个线程写、另一个线程读/写同一可变对象的数据竞争。

证明思路：

1. 非冻结可变对象属于唯一一个区域；
2. 可变区域要么 closed、active、paused，要么被 cown 持有；
3. closed 区域不可直接访问；
4. active 区域只在一个线程区域栈顶；
5. paused 区域只在同一线程区域栈内可读；
6. cown 内部区域只能在 acquire 排他持有期间 active；
7. 线程边界只允许移动 `iso`、共享 `imm` 或共享 `cown`；
8. 因此可变对象不会被两个线程同时直接访问。

## 17. 内存安全性质

### 17.1 无悬垂引用

- `tmp` 和 `paused` 引用受词法区域栈限制；
- 区域释放要求外部唯一桥引用被丢弃；
- 释放整个区域时不存在外部非桥引用。

### 17.2 无重复释放

- `iso` 不可复制；
- 区域释放由唯一桥引用或 cown 所有权触发；
- 移动后源位置 undef。

### 17.3 无释放后使用

- 释放区域会消耗唯一 `iso`；
- 所有内部 `mut/tmp/paused` 引用不能逃逸到释放点之后；
- cown 释放前执行闭合检查。

## 18. FFI 规范

FFI 函数默认视为 unsafe。可通过契约声明能力行为：

```gura
@ffi
@unsafe_contract("returns_imm")
fn intern_string(ptr: Ptr[U8], len: USize): imm String
```

FFI 不得：

- 返回伪造 `mut` 指向其他线程区域；
- 保存 `tmp` 或 `paused` 引用；
- 在未 acquire cown 时访问其内部；
- 修改 frozen 对象的程序可见状态。

## 19. 编译器诊断要求

错误消息应解释违反的是哪个模型规则：

- `iso` 被复制；
- `mut` 引用逃逸出 `enter`；
- `paused` 引用被写入普通对象；
- `self: mut` 方法在 paused 视图上调用；
- `spawn` 参数不是 send-safe；
- cown 释放时区域未闭合；
- `freeze` 目标不是闭合区域。

示例诊断：

```text
error[E0304]: cannot return `mut Node` from `enter` block
  reason: `Node` belongs to the active region opened by `enter tree`
  help: return an `iso` by moving a closed subregion, or return an `imm` value using `freeze`
```

## 20. 未决设计点

1. 是否在首个版本支持 `explore read cown` 多读者模式。
2. `new imm` 是独立分配还是强制脱糖为 `freeze(new iso ...)`。
3. 是否提供隐式冻结白名单，例如字符串字面量、类型元数据和小型元组。
4. `var Store[iso T]` 的表层语法是否暴露解引用操作，或只通过 `move`/赋值表达。
5. async/await 与区域栈交互：是否禁止 active 区域跨 await，或把任务帧提升为 cown。

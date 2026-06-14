# gura 语言规范草案

本文档定义 gura 的核心规范。它不是完整标准，而是编译器原型和后续形式化工作的设计基线。

## 1. 语言层次

gura 可分为四个语义层：

1. **表层语言**：用户编写的 Rust / Swift / Kotlin-like 语法。
2. **核心命令语言**：A-normal 风格表达式、显式移动、赋值、调用和控制流。
3. **区域语言**：显式区域栈、堆、加载、交换、分配、打开、关闭、冻结、合并。
4. **运行时并发层**：任务、cown 获取/释放、调度和同步。

规范重点是第 2 和第 3 层的交互。

### 1.1 Core Gura v0 安全边界

Core Gura v0 先收敛到一个可实现、可检查、可逐步形式化的安全子集。凡是本节禁止或保留的能力，后续版本必须先补充静态规则、动态语义和安全性证明，再进入安全语言。

v0 支持：

- `mut`、`tmp`、`iso`、`imm`、`pau`、`cown` 引用能力；
- `new mut`、`new tmp`、`new iso`、`new imm`；
- `enter`、`explore`、`freeze`、`merge`；
- 单 cown 或多 cown 的排他 `acquire`；
- `spawn`，但参数必须满足 `send_safe`。

v0 明确禁止或保留：

- 闭包、函数值、trait object 捕获 `mut`、`tmp`、`pau` 后逃逸；
- active explicit region 跨越 `await`，async/await 整体保留；
- mutable global，顶层可变状态必须通过 `cown` 或后续显式 global-region 设计表达；
- safe 逐对象 `free`，`Manual` 策略只允许 unsafe 或未来线性证明；
- 多读者 `acquire read`；
- 隐式冻结，除非是语言内建且明确列白名单的值；
- 普通命名 `mut` 对象隐式搬入 `new iso`，除非它是当前表达式中可证明 fresh 的构造树。

v0 的安全性目标是：所有安全程序中，闭合区域的 external uniqueness、开放区域栈的 LIFO 顺序、单一可变窗口、`tmp/pau/mut` 不逃逸和 cown 排他访问都能由静态检查保证；只有字段/槽打开、cown 释放、FFI/unsafe 边界和调试拓扑断言需要运行期检查。

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

`bridge_id` 是闭合区域的当前桥对象。闭合区域满足：除 `bridge_id` 外，区域内对象没有来自区域外的直接可变引用；`bridge_id` 至多有一个区域外引用。这个外部唯一引用的静态类型写作 `iso T`，其中 `T` 是当前桥对象的对象类型。

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

### 6.1 适配表

| `K ⊙ Kf` | `mut` | `tmp` | `imm` | `iso` | `paused` |
| --- | --- | --- | --- | --- | --- |
| `mut` | `mut` | `⊥` | `imm` | `⊥/iso-place` | `⊥` |
| `tmp` | `mut` | `tmp` | `imm` | `⊥/iso-place` | `pau` |
| `imm` | `imm` | `imm` | `imm` | `imm` | `imm` |
| `iso` | `⊥` | `⊥` | `⊥` | `⊥` | `⊥` |
| `pau` | `pau` | `pau` | `imm` | `⊥/iso-place` | `pau` |

`⊥/iso-place` 表示不能作为普通值读取，但可以作为存储位置参与 `enter *slot`、`move` 或 `swap`。

## 7. 对象分配

### 7.1 new mut

```text
Γ ⊢ args : fields(T) ⊣ Γ'
active_region_exists
Γ ⊢ new mut T(args) : mut T ⊣ Γ'
```

效果：对象加入 active 区域。

### 7.2 new tmp

```text
Γ ⊢ new tmp T(args) : tmp T
```

限制：只能在区域块内出现；结果不得逃逸。

### 7.3 new iso

```text
Γ ⊢ args : iso_constructor_args(T) ⊣ Γ'
Γ ⊢ new iso<S> T(args) : iso T ⊣ Γ'
```

效果：创建新 closed 区域，桥对象为 `T`，内存策略为 `S`。`S` 属于 `Arena | RC | GC | Manual`；省略策略时由实现选择默认策略。安全语义只允许消费唯一 `iso` 后释放整个区域，`Manual` 的逐对象释放必须处于 unsafe 或线性证明约束下。

构造参数允许：

- 纯值；
- `imm`；
- 被移动的 `iso`，作为嵌套区域；
- 当前表达式内的 fresh `mut` 构造树。

fresh `mut` 构造树必须满足：

1. 根对象和所有可达 `mut` 子对象都由本次 `new iso` 参数表达式直接构造；
2. 构造树中的 `mut` 对象没有被绑定到外部名称、写入外部字段或作为函数调用结果返回后再传入；
3. 构造树中只能引用纯值、`imm`、被移动的 `iso` 或同一 fresh 构造树内对象；
4. 构造完成后，这些 `mut` 对象直接归属新 closed 区域，外层 active 区域中不得留下任何 `mut/tmp/pau` 别名。

因此普通命名 `mut` 不能隐式搬入新区域：

```gura
let n: mut Node = new mut Node(value: 1)
let box: iso Box = new iso Box(child: n)   // 非法：n 不是 fresh 构造树的一部分
```

合法写法是直接构造，或先显式闭合为 `iso` 后移动：

```gura
let box: iso Box = new iso Box(child: new mut Node(value: 1))
let child: iso Node = new iso Node(value: 1)
let box2: iso Box = new iso Box(child: move child)
```

### 7.4 new imm

```text
Γ ⊢ new imm T(args) : imm T
```

语义等价于创建闭合区域后冻结，但实现可直接分配到冻结堆。

## 8. enter 静态规则

`enter source as binding { body }` 打开 `source` 指向的闭合区域。`binding` 是当前桥对象在块内的可变视图，不是特殊关键字。

`source` 必须是可打开位置或一次性 `iso` 值。进入期间，打开源被埋藏为 `open-borrowed` 状态：块内不能读取、移动、复制、重新打开或强更新同一个 `iso` 槽。退出时，区域重新闭合，打开源恢复为指向当前桥对象的 `iso U`；若打开源不是可恢复位置，则返回值只能使用块结果，不能让临时打开源继续存在。

若：

```text
Γ ⊢ open_place(source) : OpenSlot[iso T] ⊣ Γ1
bury_open_source(Γ1, source) = Γopen
suspend(Γopen) = Γpaused
Γpaused, binding: BridgeSlot[mut T] ⊢ body : τ ⊣ Γbody
return_safe(τ)
no_escape(binding_region, Γbody)
resolve_bridge(binding, Γbody) = new_bridge : mut U
```

则：

```text
Γ ⊢ enter source as binding { body } : τ ⊣ restore_with_bridge(Γ1, Γbody, source, iso U)
```

`BridgeSlot[mut T]` 表示 `binding` 既可作为 `mut T` 使用，也可作为当前 active 区域的桥槽被强更新。若块内未给 `binding` 赋新值，`resolve_bridge` 返回进入时的桥对象。

打开源分类：

- `let x: iso T`：进入时埋藏 `x`，块内 `x` 不可用；退出时同名绑定恢复为 `iso U`，但只有 `U` 与原绑定类型兼容时合法。
- `var x: iso T`：进入时埋藏槽内容；退出时槽强更新为 `iso U`，允许桥对象类型改变。
- `obj.field: iso T` 或 `*slot: iso T`：进入时需要运行期检查目标区域未打开；持有该字段的外层对象在 body 中必须是 paused 或不可写视图，防止打开期间移动区域。
- 临时 `iso` 值：只能被立即打开，块结束后若没有可恢复存储位置，不允许桥类型改变，也不允许把源值再次使用。

非法：

```gura
var tree: iso Tree = make_tree()
enter tree as t {
    spawn worker(move tree)   // tree 处于 open-borrowed，不能移动
    enter tree as again { }   // 不能重复打开同一区域
}
```

### 8.1 suspend

进入内层区域时，外层环境中：

- `mut T` -> `pau T`；
- `tmp T` -> 不可捕获，除非生命周期严格内嵌且不逃逸；
- `Store[var, T]` -> `paused Store[T]`；
- `iso T` 保持 `iso T`，但仍不可复制；
- `imm T` 保持 `imm T`；
- `cown T` 保持 `cown T`。

### 8.2 返回限制

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

### 8.3 动态打开检查

通过局部 `iso` 变量打开区域通常可静态证明未打开。通过字段或存储槽打开时，可能存在别名路径，需运行期检查目标区域是否已在当前区域栈中。

### 8.4 桥对象解析

`enter` 退出时必须选择一个新的当前桥对象：

```text
resolve_bridge(binding, Γbody) = object_id
```

规则：

- `object_id` 必须属于当前 active 区域；
- `object_id` 的值能力必须可闭合为 `iso U`，不能是 `tmp U` 或指向外层 paused 区域的 `pau U`；
- 若 `binding` 没有被赋值，`object_id` 为进入时的 `bridge_id`；
- 若 `binding = e`，则 `e` 的结果对象成为新的 `bridge_id`；
- 若新桥对象类型 `U` 不同于旧类型 `T`，则 `source` 必须是可强更新位置，使外层环境从 `iso T` 更新为 `iso U`；
- 若 `source` 是不可写的 `let` 名称或临时值，则只允许解析为与原类型兼容的桥对象。

示例：

```gura
var list: iso List = make_list()
enter list as bridge {
    let cursor: mut Cursor = new mut Cursor(current: bridge)
    bridge = cursor
}
// list: iso Cursor
```

非法：

```gura
enter list as bridge {
    let cursor: tmp Cursor = new tmp Cursor(current: bridge)
    bridge = cursor    // tmp 不能成为闭合区域桥对象
}
```

## 9. explore 静态规则

Core Gura v0 采用严格区域栈语义：`explore source as binding { body }` 与 `enter` 一样会打开 `source` 并挂起原 active 区域，但目标区域以只读 suspended 视图暴露，块内 `binding: pau T`。因此 `explore` body 中没有可写 active 用户区域；它只能读取被探索区域和外层 suspended 区域，创建词法受限的 `tmp` 对象，并返回 `return_safe` 的值。

规则：

- 不能调用 `self: mut` 方法；
- 不能写入被探索区域；
- 不能写入外层被 `explore` 挂起的区域；
- 不能 `new mut` 到任一用户区域，只能创建 `new tmp`；
- 可以创建 `tmp` 对象保存 paused 引用；
- `tmp`、`pau` 和指向被探索区域或外层 suspended 区域内部的引用不能逃逸；
- 返回值必须 `return_safe`；
- 被探索区域和外层 suspended 区域的不变式在整个块内视为稳定。

若未来希望支持“保持当前 active 区域可写，同时只读借用另一个 closed region”的轻量查询形式，需要作为独立构造设计，不能与 v0 的 `explore` 混用。该扩展必须额外限制可调用方法、回调、嵌套 `enter` 和引用保存位置。

## 10. freeze 规则

```text
Γ ⊢ e : iso T ⊣ Γ'
Γ ⊢ freeze(e) : imm T ⊣ Γ'
```

动态要求：区域闭合。若静态上 `e` 是当前可用 `iso`，闭合通常已保证；若来自 cown 或 FFI，需要运行期闭合检查。

## 11. merge 规则

```text
Γ ⊢ e : iso T ⊣ Γ'
active_region_exists
Γ ⊢ merge(e) : mut T ⊣ Γ'
```

效果：源区域并入 active 区域。源区域的直接嵌套区域仍保持闭合嵌套区域。

## 12. cown 规则

### 12.1 创建

```text
Γ ⊢ e : iso T ⊣ Γ'
Γ ⊢ cown(e) : cown T ⊣ Γ'
```

或：

```text
Γ ⊢ e : imm T ⊣ Γ'
Γ ⊢ cown(e) : cown T ⊣ Γ'
```

### 12.2 获取

若：

```text
Γ ⊢ c_i : cown T_i ⊣ Γ_i
all_distinct(c_i)
acquire_view(c_i) = binding_i : K_i T_i
suspend(Γ_n) = Γpaused
Γpaused, binding_i: K_i T_i ⊢ body : τ ⊣ Γbody
return_safe(τ)
no_escape(acquired_region_i, Γbody)
```

则：

```text
Γ ⊢ acquire c_1 as binding_1, ... { body } : τ ⊣ restore(Γ_n, Γbody)
```

其中 `acquire_view(cown(iso T)) = mut T`，`acquire_view(cown(imm T)) = imm T`，`acquire_view(cown(cown T)) = cown T`。

语义要求：

- 多个 cown 按运行时定义的全序获取，避免死锁；
- 同一 `acquire` 列表中重复 cown 非法；
- 对已经在当前线程 acquire 的同一 cown 再次 acquire 非法，除非未来定义显式 reentrant cown；
- `acquire` 期间 cown 内部 closed region 变为当前线程的 active region 入口，外层 active 区域被挂起；
- 块正常返回、提前 `return` 或 panic/unwind 时，运行时都必须执行释放路径；
- 释放时运行期检查 cown 内部区域重新闭合；若闭合失败，程序进入确定性 panic，cown 不得被其他线程观察到半开放状态。

Core v0 中 `cown(imm T)` 的获取绑定为 `imm T`，不进入可写 region；`cown(cown T)` 的获取只返回内部 cown 值，不递归获取。若 acquire 期间允许切换桥对象类型，则 cown 存储槽也必须支持强更新；v0 暂不允许 cown 内部桥类型改变。

## 13. spawn 规则

```text
Γ ⊢ arg_i : τ_i ⊣ Γ_i
∀i. send_safe(τ_i)
no_captured_region_borrows(f)
Γ ⊢ spawn f(args...) : TaskHandle ⊣ Γ_n
```

`iso` 参数必须通过 `move` 传递。`imm` 与 `cown` 参数可共享。`f` 若是函数值或闭包，不能捕获 `mut`、`tmp`、`pau`、open-borrowed `iso` 或 suspended store；v0 可先只允许命名函数作为 `spawn` 目标。

## 14. 动态语义概要

动态语义由命令语言产生效果，区域语言消费效果。

### 14.1 效果集合

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

### 14.2 load

`load(dest, x.f)`：

1. 查找 `x` 指向对象；
2. 读取字段 `f`；
3. 根据 receiver capability 做视点适配；
4. 绑定到 `dest`。

### 14.3 swap

`swap(dest_old, place, value)`：

1. 检查 place 可写；
2. 读取旧值到 `dest_old`；
3. 写入新值；
4. 更新区域拓扑元数据；
5. 若违反拓扑不变式，触发确定性错误。

### 14.4 enter

`enter(region_ref, binding)`：

1. 检查 region_ref 指向 closed 区域的当前桥对象；
2. 若区域已 open，失败；
3. 将区域从 `H_closed` 移到 `H_open`；
4. 压入 RegionFrame；
5. `binding` 绑定为当前桥对象的 `BridgeSlot[mut T]`；
6. 外层 frame 变为 paused。

### 14.5 exit

`exit(region_id, new_bridge, result)`：

1. 检查 region_id 是栈顶 active 区域；
2. 检查无非法逃逸引用；
3. 检查 `new_bridge` 属于 region_id 且不是临时对象；
4. 设置 `bridge_id = new_bridge`；
5. 弹出 RegionFrame；
6. 把区域移回 `H_closed`；
7. 外层 paused 区域恢复 active；
8. 若桥类型改变，更新打开源对应的外层存储槽类型。

### 14.6 freeze

`freeze(dest, iso_ref)`：

1. 检查 iso_ref 指向 closed 区域；
2. 遍历该区域及嵌套区域；
3. 所有对象移动到 `H_frozen`；
4. 结果以 `imm` 绑定到 dest。

### 14.7 merge

`merge(dest, iso_ref)`：

1. 检查 iso_ref 指向 closed 区域；
2. 把源区域对象并入 active 区域；
3. 删除源区域边界；
4. 结果以 `mut` 绑定到 dest。

## 15. 区域写屏障

即使静态类型系统能证明大多数写入安全，运行时仍定义写屏障作为实现规范和 unsafe/FFI 兜底。

写入 `src.field = tgt` 时：

1. 若 `src` 属于 frozen 或 cown，拒绝写入；
2. 若 `src` 与 `tgt` 同区域，允许；
3. 若 `tgt` 是 `imm` 或 cown，允许；
4. 若 `src` 在 active 区域且 `tgt` 在 paused 区域，只有当目标存储位置是 `tmp` 时允许；
5. 若 `tgt` 是 closed 区域桥对象，要求外部唯一并更新区域父子关系；
6. 其他跨区域可变引用拒绝。

## 16. 闭合判定

区域 closed 当且仅当：

1. 它不在任何线程区域栈中；
2. 没有从局部栈、tmp 对象或其他非桥外部位置指向其内部对象的引用；
3. 外部入边至多一个，且指向桥对象；
4. 所有嵌套区域也满足对应外部唯一约束。

实现可用静态生命周期证明、局部借用计数或调试遍历验证闭合性。

## 17. 数据竞争自由性质

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

## 18. 内存安全性质

### 18.1 无悬垂引用

- `tmp` 和 `paused` 引用受词法区域栈限制；
- 区域释放要求外部唯一桥引用被丢弃；
- 释放整个区域时不存在外部非桥引用。

### 18.2 无重复释放

- `iso` 不可复制；
- 区域释放由唯一桥引用或 cown 所有权触发；
- 移动后源位置 undef。

### 18.3 无释放后使用

- 释放区域会消耗唯一 `iso`；
- 所有内部 `mut/tmp/paused` 引用不能逃逸到释放点之后；
- cown 释放前执行闭合检查。

## 19. Escape 与 effect 边界

### 19.1 Escape 分类

编译器必须为每个表达式和存储动作计算逃逸类别：

```text
Escape ::= NoEscape
         | ReturnValue
         | StoreInActiveRegion
         | StoreInTmp
         | StoreInClosedRegion
         | SpawnArg
         | CownStore
         | Global
         | ClosureCapture
```

v0 规则：

- `mut T` 只能 `NoEscape` 或存入同一 active region 内普通对象；
- `tmp T` 只能 `NoEscape`、`StoreInTmp`，且 tmp 对象生命周期严格包含被存引用；
- `pau T` 只能 `NoEscape`、`StoreInTmp`，不得存入普通 `mut` 对象、closed region、cown、global 或 closure；
- `iso T` 可 `ReturnValue`、`SpawnArg`、`CownStore`、`StoreInActiveRegion`，但每次使用必须是 move/swap，不能复制；
- open-borrowed `iso` 不能逃逸，也不能作为 `spawn` 参数或 closure capture；
- `imm T` 和纯值可自由返回、共享和捕获。

### 19.2 Effect 集合

函数、方法和闭包在 HIR 中至少携带以下 effect 上界：

```text
Effect ::= Read | Write | AllocMut | AllocTmp | Enter | Explore | Freeze | Merge | Acquire | Spawn | Unsafe | Ffi | Panic
```

v0 中 effect 主要用于限制 `pau`/`imm` 方法和闭包逃逸：

- `self: imm` 方法不得具有 `Write` 到程序可见对象的 effect；
- `self: pau` 方法不得具有 `Write`、`Merge`、`Freeze` 当前 paused region、或保存 `self` 派生引用的 effect；
- 可被 `spawn` 的函数值必须没有非 send-safe capture；
- unsafe/FFI 调用必须显式带 `Unsafe` 或 `Ffi` effect，并在边界插入拓扑校验。

Core v0 可以先把 effect 检查实现为保守规则：未知函数调用视为可能 `Write | Enter | Acquire | Spawn | Panic`；只有编译器可见函数体或显式声明的纯/只读函数可在 `pau`/`imm` 上下文中调用。

## 20. FFI 规范

FFI 函数默认视为 unsafe。可通过契约声明能力行为：

```gura
@ffi
@unsafe_contract("returns_imm")
fn intern_string(ptr: Ptr<U8>, len: USize): imm String
```

FFI 不得：

- 返回伪造 `mut` 指向其他线程区域；
- 保存 `tmp` 或 `paused` 引用；
- 在未 acquire cown 时访问其内部；
- 修改 frozen 对象的程序可见状态。

## 21. 编译器诊断要求

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

## 22. 未决设计点

1. 是否在后续版本支持 `acquire read cown` 多读者模式。
2. `new imm` 是独立分配还是强制脱糖为 `freeze(new iso ...)`。
3. 是否提供隐式冻结白名单，例如字符串字面量、类型元数据和小型元组。
4. `var Store[iso T]` 的表层语法是否暴露解引用操作，或只通过 `move`/赋值表达。
5. async/await 与区域栈交互：v0 禁止 active explicit region 和 `mut/tmp/pau` live value 跨 await；后续可研究把任务帧提升为 cown。
6. destructor/drop 顺序、panic 语义和 effect 限制。
7. `imm` 对象的全局 liveness 策略：atomic RC、tracing、epoch 或运行时托管。
8. 轻量 readonly borrow closed region 是否作为不同于 v0 `explore` 的独立构造加入。

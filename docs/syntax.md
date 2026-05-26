# gura 语法设计

本文档描述 gura 的表层语法。语法偏向 Rust / Swift / Kotlin-like：块使用 `{}`，声明使用 `fn`、`struct`、`enum`、`trait`，变量使用 `let`/`var`，泛型使用 `[]`，类型标注使用 `:`。

## 1. 词法

### 1.1 注释

```gura
// 单行注释
/* 块注释 */
```

### 1.2 标识符

```ebnf
Identifier ::= XID_Start XID_Continue*
```

关键字不能作为普通标识符。需要使用关键字名称时，可使用反引号：

```gura
let `type` = "keyword as name"
```

### 1.3 字面量

```gura
42          // i64，默认整数字面量类型
42i32       // i32
42i64       // i64
3.14        // f64，默认浮点字面量类型
3.14f32     // f32
3.14f64     // f64
true
false
'字'
"hello"
"""
多行字符串
"""
none
```

当前数值字面量支持十进制整数和带小数点的十进制浮点数；十六进制、二进制、八进制、指数形式和 `_` 分隔符保留给后续扩展。数值后缀是字面量 token 的一部分，`1i32` 与 `1.0f32` 分别表示精确的 `i32` 与 `f32` 字面量；无后缀整数默认为 `i64`，无后缀浮点默认为 `f64`。

`none` 是空值字面量，只能用于显式可空类型 `T?` 或被语言定义为可空的上下文。

## 2. 模块与导入

```gura
module net.http

import std.io.File
import std.collections.{Map, List}
import crypto.hash as hash
```

```ebnf
ModuleDecl ::= "module" ModulePath
ImportDecl ::= "import" ImportPath ("as" Identifier)?
```

## 3. 顶层声明

```ebnf
File ::= ModuleDecl? ImportDecl* Decl*
Decl ::= FnDecl | StructDecl | EnumDecl | TraitDecl | ImplDecl | TypeAliasDecl | ConstDecl
```

## 4. 变量声明

```gura
let x: i64 = 1        // 不可重新绑定
var y: i64 = 2        // 可重新绑定，具备 var 存储槽能力
let z = x + y         // 类型推导
```

`let` 绑定不可重新绑定，但其引用能力决定对象是否可变。`let p: mut Point` 表示变量 `p` 不可改绑，但可通过 `p` 修改 active 区域中的 Point 对象。

`var` 绑定是强更新存储槽，可用于交换 `iso` 引用：

```gura
var tree: iso Tree = build_tree()
let old = move tree
// tree 进入未定义状态，必须重新赋值后才能读取
```

## 5. 类型语法

```ebnf
Type ::= Capability? TypeAtom
       | Type "?"
       | Type "|" Type
       | FnType

Capability ::= "mut" | "tmp" | "iso" | "imm" | "pau" | "cown"
TypeAtom ::= Identifier TypeArgs? | "(" TypeList? ")"
TypeArgs ::= "[" Type ("," Type)* "]"
FnType ::= "fn" "(" ParamTypes? ")" "->" Type
```

示例：

```gura
mut Node
iso Tree[String]
imm List[imm String]
cown Account
fn(mut Buffer, imm Bytes) -> Result[unit, Error]
```

## 6. 函数

```gura
fn add(a: i64, b: i64): i64 {
    return a + b
}

fn abs(x: i64): i64 = if x < 0 { -x } else { x }
```

```ebnf
FnDecl ::= "fn" Identifier TypeParams? "(" ParamList? ")" ReturnType? BlockOrExpr
Param ::= Identifier ":" Type
ReturnType ::= ":" Type
BlockOrExpr ::= Block | "=" Expr
```

### 6.1 `self` 能力

方法必须声明 `self` 能力，决定调用者需要的引用视图。

```gura
struct Counter {
    var value: i64

    fn inc(self: mut) {
        self.value += 1
    }

    fn get(self: imm): i64 {
        return self.value
    }

    fn peek(self: pau): i64 {
        return self.value
    }
}
```

规则：

- `self: mut` 可读写字段；
- `self: imm` 只能读取深不可变字段；
- `self: pau` 只能读取 paused 视图允许读取的字段；
- `self: iso` 不可直接解引用，必须通过 `enter` 打开区域；
- 同名方法可按 `self` 能力重载；
- 方法调用默认要求 receiver 能力与声明的 `self` 能力匹配，不能把 `mut` receiver 隐式降级为 `paused` 调用；未来若支持 self 能力多态，必须显式声明。

## 7. 结构体

```gura
struct Node[T] {
    var value: T
    var next: mut Node[T]?
}
```

字段默认为不可重新赋值字段；使用 `var` 表示字段可更新。

```gura
struct Pair[A, B] {
    let first: A
    let second: B
}
```

字段类型可以带引用能力：

```gura
struct Tree {
    var left: iso Tree?
    var right: iso Tree?
    var label: imm String
}
```

## 8. 枚举与模式匹配

```gura
enum Option[T] {
    Some(T)
    none
}

fn unwrap_or(opt: Option[i64], fallback: i64): i64 {
    match opt {
        case Some(x) => x
        case none => fallback
    }
}
```

## 9. trait 与 impl

```gura
trait Display {
    fn format(self: imm): imm String
}

impl Display for Account {
    fn format(self: imm): imm String {
        return "Account(...)"
    }
}
```

trait 方法也必须声明 `self` 能力。

## 10. 表达式

### 10.1 基本表达式

```ebnf
Expr ::= Literal
       | Identifier
       | FieldExpr
       | CallExpr
       | IfExpr
       | MatchExpr
       | Block
       | LoopExpr
       | ReturnExpr
       | BreakExpr
       | ContinueExpr
       | AssignExpr
       | MoveExpr
       | NewExpr
       | EnterExpr
       | ExploreExpr
       | FreezeExpr
       | MergeExpr
       | CownExpr
       | AcquireExpr
```

### 10.2 if

```gura
let sign = if x < 0 { -1 } else { 1 }
```

### 10.3 match

```gura
match value {
    case Ok(v) => v
    case Err(e) => return Err(e)
}
```

### 10.4 块

块是表达式。最后一个无分号表达式作为块值。

```gura
let x = {
    let a = 1
    a + 1
}
```

## 11. 对象创建

### 11.1 active 区域内分配

```gura
let node: mut Node[i64] = new mut Node(value: 1, next: none)
```

`new mut T(...)` 在当前 active 区域分配对象，返回 `mut T`。

### 11.2 创建新闭合区域

```gura
let tree: iso Tree = new iso<Arena> Tree(label: "root", left: none, right: none)
```

`new iso T(...)` 创建一个新闭合区域，其桥对象类型为 `T`，返回 `iso T`。

可选内存管理策略：

```gura
new iso<Arena> RequestArena()
new iso<RC> Graph()
new iso<GC> ObjectGraph()
```

策略省略时默认为实现定义，建议原型期使用 `Arena`。`Manual` 可作为显式策略，但安全代码只支持消费唯一 `iso` 后释放整个区域；逐对象手动释放需要 unsafe 或线性证明，不能作为普通安全 `free(obj)` 使用。

### 11.3 临时对象

```gura
let it: tmp Iterator = new tmp Iterator(source: list)
```

`new tmp` 只能出现在 `enter` 或 `explore` 块内，其对象生命周期不能超过当前块。

### 11.4 不可变对象

```gura
let s: imm String = new imm String("hello")
```

`new imm` 创建深不可变对象。实现可把它降低为创建临时闭合区域后 `freeze`。

## 12. 移动

```gura
let b = move a
obj.field = move other.field
```

`move` 是破坏性读取：读取后源位置进入未定义状态。对 `iso` 和 `var` 存储槽的读取默认要求显式 `move`。

非法：

```gura
let x = tree      // tree: iso Tree，非法复制
foo(tree, tree)   // 第二次使用非法
```

合法：

```gura
foo(move tree)
```

## 13. 区域操作

### 13.1 enter

`enter` 打开一个闭合区域，使它成为 active 区域。块结束时区域重新闭合。

```gura
let tree: iso Tree = new iso Tree(label: "root")

enter tree as root {
    root.label = "new root"
    root.left = new iso Tree(label: "left")
}
```

`enter source as bridge { body }` 中的 `bridge` 不是关键字，而是用户声明的绑定名。它表示被打开区域当前桥对象的块内视图，类型为 `mut T`。闭合区域外部只能通过 `source: iso T` 指向这个桥对象；区域打开后，桥对象和区域内其他对象一样可变。

进入规则：

- 输入必须是 `iso T`，或能通过存储槽读到 `iso T`；
- 块内 `bridge` 类型为 `mut T`；
- 外层 active 区域中的 `mut`、`tmp`、`var` 捕获变量变为 `paused` 视图；
- 块返回值只能是 `iso`、`imm` 或不含 active 区域引用的纯值；
- 块结束时所有指向该区域内部非桥对象的局部引用失效；
- 块结束时必须能解析出一个新的当前桥对象；如果没有显式切换，则沿用进入时的桥对象。

#### 13.1.1 桥对象切换

桥对象可以在 `enter` 块内切换为同一区域中的另一个对象。推荐写法是让 `as` 绑定成为一个可强更新的桥槽：

```gura
struct List {
    var head: imm String
}

struct Cursor {
    var current: mut List
}

var list: iso List = new iso List(head: "a")

enter list as bridge {
    let cursor: mut Cursor = new mut Cursor(current: bridge)
    bridge = cursor
}

// list 的静态类型现在是 iso Cursor；它仍代表同一个闭合区域，但当前桥对象变成 cursor。
```

桥切换规则：

- 新桥对象必须属于当前 active 区域；
- 新桥对象不能是 `tmp`，因为 `tmp` 会在块结束时失效；
- 新桥对象不能来自外层 paused 区域，也不能是另一个闭合区域的 `iso` 桥；
- 切换桥对象只改变闭合区域的外部入口，不复制或移动区域内对象；
- 如果桥对象类型改变，打开源必须是可强更新位置，例如 `var list: iso List`，这样块结束后外层存储槽可更新为新的 `iso Cursor`；不可写的 `let` 绑定只能沿用兼容的桥类型。

#### 13.1.2 通过字段或槽打开

当 `enter` 的源是字段、索引或暂停区域中的桥槽时，编译器可能无法静态证明目标区域尚未打开，需要插入动态检查。

```gura
explore map as m {
    let slot: pau Store[iso Value] = m.get(key)
    enter *slot as value {
        value.touch()
    }
}
```

这里 `*slot` 表示从可打开的桥槽中打开其 `iso Value`。如果该区域已经在当前区域栈中，`enter` 必须确定性失败，而不是形成重复打开。

### 13.2 explore

`explore` 以只读方式打开一个闭合区域。它允许遍历区域并读取数据，但不允许修改区域对象。

```gura
explore users as u {
    for user in u.iter() {
        print(user.name)
    }
}
```

`explore x as y { body }` 可理解为：打开 `x` 后立即把被探索区域置为 paused，并在一个词法受限的临时 active 上下文中执行 `body`。块内 `y` 的类型为 `pau T`，不能写入被探索区域，`tmp`/`pau` 引用不能逃逸，返回值限制与 `enter` 相同。因此被探索区域保持不变式稳定。

### 13.3 freeze

```gura
let immutable_tree: imm Tree = freeze(move tree)
```

输入必须是 `iso T`。结果是 `imm T`。冻结递归影响该区域及其嵌套区域。

### 13.4 merge

```gura
enter target as t {
    let child: mut Child = merge(move child_region)
    t.child = child
}
```

`merge(iso T) -> mut T`，把闭合区域并入当前 active 区域。

## 14. cown 与并发

`cown`/`acquire`/`spawn` 是 gura 在 Reggio 区域模型之上采用 Verona 并发所有者思想的并发层扩展；Reggio 核心只要求区域隔离与 `iso` 外部唯一性。

### 14.1 创建 cown

```gura
let account: iso Account = make_account(100)
let shared: cown Account = cown(move account)
```

`cown` 至少支持包装闭合区域 `iso T`。gura 还允许包装深不可变值 `imm T` 和另一个 `cown T`；这两者是 gura 对 Verona `cown` 的扩展。

### 14.2 acquire

```gura
acquire shared as account {
    account.deposit(10)
}
```

多 cown 原子获取：

```gura
acquire from as src, to as dst {
    src.withdraw(amount)
    dst.deposit(amount)
}
```

规则：

- `acquire` 块开始前运行时获得所有 cown 的排他访问；
- 块内绑定类型为 `mut T`；
- 块结束时写回闭合区域；
- 如果有 `mut` 或 `paused` 引用逃逸，释放失败；
- 多 cown 获取必须使用全序或运行时事务协议避免死锁。

### 14.3 spawn

```gura
spawn worker(move job, config, result)
```

传给新任务的值必须满足以下之一：

- `iso T`：移动到新任务；
- `imm T`：共享；
- `cown T`：共享并通过 acquire 协调；
- 纯值：复制。

## 15. 错误处理

```gura
enum Result[T, E] { Ok(T), Err(E) }

fn read_file(path: imm String): Result[imm Bytes, IOError] {
    ...
}
```

语言不使用异常作为普通错误机制。区域动态检查失败是 panic 级别的确定性错误，通常表示内存安全模型被动态别名、FFI 或运行时状态破坏。

## 16. 运算符

常见算术、比较、逻辑和位运算沿用 C-family 优先级。赋值表达式返回旧值，便于交换，但普通代码建议显式 `swap` 或 `move`。

```gura
let old = (cell.value := new_value)
```

## 17. 最小示例

```gura
module demo

struct LogEntry {
    let text: imm String
}

struct Account {
    var balance: i64
    var logs: mut List[imm LogEntry]

    fn deposit(self: mut, amount: i64) {
        self.balance += amount
        self.logs.push(new imm LogEntry(text: "deposit"))
    }

    fn balance(self: pau): i64 {
        return self.balance
    }
}

fn main() {
    let acc = new iso<RC> Account(balance: 0, logs: new mut List())
    let shared = cown(move acc)

    acquire shared as a {
        a.deposit(100)
    }
}
```

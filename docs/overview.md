# gura 语言设计总览

## 1. 定位

gura 是一门面向系统编程的静态类型语言，采用 Rust / Swift / Kotlin-like 的表层语法，目标是在接近手动内存管理可控性的同时，提供默认内存安全、数据竞争自由和可预测的并发内存管理。

gura 的核心设计不是“每个对象一个所有者”，而是“每个可变对象属于一个隔离区域”。区域是内存管理、所有权转移、并发排他访问和生命周期推理的基本单位。

## 2. 设计目标

1. **内存安全**：禁止悬垂引用、重复释放、越界访问、释放后使用和未初始化读取。
2. **数据竞争自由**：可变对象在任意时刻只能被一个线程直接访问；共享对象必须是深不可变对象或通过并发所有者协调访问。
3. **可预测内存管理**：程序员可以按区域选择内存管理策略，例如 arena、引用计数或 tracing GC。
4. **零拷贝所有权转移**：闭合区域可以通过移动一个桥对象引用在线程、任务或其他区域之间转移。
5. **表达复杂对象拓扑**：区域内部允许任意图结构，包括循环、双向链表和图；限制只发生在区域边界。
6. **少量显式概念**：表层语法保留现代主流语言风格，只把区域、引用能力和并发所有者暴露为必要的一等概念。
7. **编译期优先，运行期兜底**：静态类型系统保证大部分区域隔离；涉及动态别名、并发所有者释放或 FFI 边界时允许运行期检查给出确定性错误。

## 3. 相关设计文档

- [语言规范草案](spec.md)
- [语法草案](syntax.md)
- [AST 设计](ast.md)
- [内存安全与区域模型](memory-safety.md)
- [标准库设计草案](stdlib.md)
- [包系统设计草案](package.md)
- [快速排序编译路线](quicksort-roadmap.md)

## 4. 核心概念

### 4.1 区域 Region

区域是一组一起管理的可变对象。每个可变对象恰好属于一个区域。区域可以处于三种状态：

| 状态 | 含义 | 可读 | 可写 | 可分配 | 可释放单对象 | 可释放整个区域 |
| --- | --- | --- | --- | --- | --- | --- |
| active | 当前线程的可变窗口所在区域 | 是 | 是 | 是 | 由策略决定 | 否 |
| paused | 已打开但被更内层区域挂起 | 是 | 否 | 否 | 否 | 否 |
| closed | 闭合且隔离的区域 | 否，除非通过 `enter`/`explore` | 否 | 否 | 否 | 是 |

一个线程任意时刻只有一个 active 区域，这称为**单一可变窗口**。打开新的区域时，原 active 区域变成 paused；退出时恢复上一层区域。

#### 4.1.1 region 是什么

region 不是一个普通对象，也不是必须手动传来传去的 runtime handle。它更像是一条对象边界：边界内部的可变对象可以任意互相引用，边界外部不能随便指向内部对象，只能持有当前桥对象的 `iso` 引用。

```gura
struct Node {
    var value: i64
    var next: mut Node?
}

let list: iso Node = new iso Node(value: 1, next: none)
```

上面代码创建了一个新的闭合 region：

```text
closed region R1
  bridge: Node(value: 1)
  objects: Node(value: 1)

outside
  list: iso Node  ---> R1.bridge
```

`list` 不是“拥有一个 Node 对象”这么简单，而是拥有整个闭合 region `R1`。以后如果这个链表 region 内部有 1000 个节点、环、缓存或索引，外部仍然只通过一个 `iso Node` 持有整个区域。

#### 4.1.2 local region 与 explicit region

线程执行普通代码时有一个隐含的 local region，用来放栈帧和当前可直接使用的局部可变对象。`new mut` 在当前 active region 中分配对象；`new iso` 创建一个新的 explicit closed region。

```gura
fn build_pair(): iso Pair {
    let tmp_left: mut Node = new mut Node(value: 1, next: none)
    let pair: iso Pair = new iso Pair(left: tmp_left, right: new mut Node(value: 2, next: none))
    return pair
}
```

概念上：

```text
local/active region
  tmp_left: Node

new iso Pair(...)
  creates closed region R2
  bridge: Pair
  objects moved/created inside R2: Pair, tmp_left, right Node

return value
  iso Pair ---> R2.bridge
```

也就是说，`new iso` 的结果不是一个裸对象，而是一个已闭合的 explicit region 的桥引用。

#### 4.1.3 active / paused / closed 的变化

```gura
var tree: iso Tree = make_tree()

enter tree as root {
    root.value = 1
    let child: mut Tree = new mut Tree(value: 2)
    root.left = child
}
```

执行过程可以理解为：

```text
before enter:
  local region: active
  tree region: closed, only accessible through tree: iso Tree

inside enter:
  tree region: active, root: mut Tree
  previous local region: paused
  allocation new mut Tree goes into tree region

退出 enter:
  tree region: closed again
  root/child mut references expire
  tree remains the unique iso bridge reference
```

这就是单一可变窗口：同一时刻只有树所在 region 可变，外层 region 暂停为只读/不可写状态。

#### 4.1.4 nested region

region 可以嵌套。一个 region 内部可以保存另一个 closed region 的桥对象引用，但不能直接保存对另一个 region 内部普通对象的引用。

```gura
struct Document {
    var title: imm String
    var index: iso Index
}

let index: iso Index = build_index()
let doc: iso Document = new iso Document(title: "spec", index: move index)
```

概念上：

```text
closed region R_doc
  bridge: Document
  field index ---> R_index.bridge

closed region R_index
  bridge: Index
  objects: index nodes...
```

`R_index` 是 `R_doc` 的嵌套子 region。移动 `doc` 时不需要复制 index，只要移动 `R_doc` 的桥引用，整个 region tree 的所有权就一起转移。

#### 4.1.5 什么不属于 region

纯值和深不可变对象不需要可变 region 边界：

```gura
let n: i64 = 42
let ok: bool = true
let name: imm String = new imm String("gura")
```

这些值可以复制或共享；region 主要管理的是可变对象图的隔离、打开、关闭、转移和回收。

### 4.2 桥对象 Bridge Object

每个闭合区域有一个当前桥对象。桥对象把区域具体化为一个可被持有的值：区域外部最多只能有一个引用指向当前桥对象，且闭合区域除当前桥对象外没有其他对象可被区域外部直接引用。区域内部可以有任意对象图，任意区域内对象都可以在退出 `enter` 块时成为新的桥对象。

桥对象提供两个能力：

- 代表整个区域的所有权；
- 作为打开闭合区域的唯一入口；
- 允许闭合区域被移动、冻结、合并或放入并发所有者。

在表层语法中没有单独的 `bridge` 关键字。`iso T` 表示“指向某个闭合区域当前桥对象的外部唯一引用”，`enter r as b { ... }` 中的 `b` 是该桥对象在打开区域内的 `mut T` 视图。块结束时，如果 `b` 仍指向同一个区域内的对象，则该对象成为闭合后的当前桥对象；如果没有显式切换，则沿用原桥对象。

### 4.3 引用能力 Reference Capability

gura 使用能力修饰类型，描述引用能做什么，而不是只描述值是什么。

| 能力 | 示例 | 含义 |
| --- | --- | --- |
| `mut T` | `mut Node` | 指向 active 区域内部的可变对象 |
| `tmp T` | `tmp Iter` | 生命周期受当前 `enter`/`explore` 块限制的临时对象 |
| `iso T` | `iso Tree` | 指向闭合区域桥对象的外部唯一引用 |
| `imm T` | `imm String` | 深不可变对象，可跨线程共享 |
| `pau T` | `pau Map` | 指向 paused 区域中的临时只读视图 |
| `var T` | `var iso Tree` | 可强更新的本地存储槽能力，仅用于局部变量和可变绑定 |
| `cown T` | `cown Account` | 并发所有者，内部持有闭合区域或不可变值 |

### 4.4 单一可变窗口

gura 不允许程序同时持有多个可变区域。嵌套打开区域时，外层区域仍可读，但不可写。这样带来三个结果：

1. 当前代码修改了哪个区域非常清楚；
2. active 区域的内存管理成本局部化；
3. 跨区域临时引用只在栈式作用域内存在，不会逃逸成长期别名。

### 4.5 深冻结

`freeze` 把一个闭合区域及其嵌套区域全部转为深不可变对象。冻结后区域边界消失，对象可被任意线程共享。冻结是显式操作，默认不隐式改变对象可变性。

### 4.6 合并

`merge` 把一个闭合区域的对象并入当前 active 区域或指定目标区域。合并保留源区域的直接嵌套子区域，只改变源区域本身的归属。

### 4.7 并发所有者 cown

`cown` 是可共享的并发所有者。它持有一个闭合区域、另一个 cown 或不可变值。线程不能直接访问 cown 内部对象，必须通过 `acquire` 块取得排他访问。

`acquire` 块结束时，cown 内部值必须再次闭合；否则编译器或运行时报告“区域逃逸”。

## 4. gura 的内存模型摘要

- 可变对象只存在于区域中。
- 不可变对象不属于可变区域，或视为在冻结堆中。
- 闭合区域之间形成森林，不允许任意交叉引用。
- 区域内部引用无限制。
- 跨区域引用只允许：
  - 指向嵌套闭合区域桥对象的 `iso` 引用；
  - 指向不可变对象的 `imm` 引用；
  - `enter`/`explore` 期间从更内层 active 区域指向外层 paused 区域的临时引用；
  - `cown` 持有的闭合区域引用。
- `iso` 是线性/仿射能力，默认移动，不可复制。
- `imm` 可复制共享。
- `mut` 不跨越区域边界。
- `paused` 不可写，不可调用需要 `mut self` 的方法。

## 5. 表层风格

gura 采用类 Kotlin/Swift 的声明风格，结合 Rust 的移动语义和模式匹配。

```gura
struct Account {
    var balance: i64
    var logs: mut List<imm LogEntry>

    fn deposit(self: mut, amount: i64) {
        self.balance += amount
    }
}

fn make_account(initial: i64): iso Account {
    let account = new iso Account(balance: initial, logs: new mut List())
    return account
}

let a: iso Account = make_account(100)
let shared: cown Account = cown(move a)

acquire shared as account {
    account.deposit(50)
}
```

## 6. 文档组织

- `docs/syntax.md`：词法、声明、表达式、模式、区域相关语法。
- `docs/memory-safety.md`：区域所有权、引用能力、单一可变窗口、冻结、合并、并发所有者。
- `docs/ast.md`：抽象语法树设计。
- `docs/spec.md`：静态语义、动态语义、类型检查与运行期检查边界。

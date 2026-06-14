# gura 内存安全模型

本文档定义 gura 的区域所有权与引用能力模型。设计吸收两类思想：

- 动态区域所有权：区域、桥对象、闭合检查、借用引用、写屏障和 cown；
- 引用能力区域系统：`mut`、`tmp`、`iso`、`imm`、`paused`、单一可变窗口、区域栈、视点适配。

gura 是静态类型语言，因此默认用类型系统保证区域隔离；运行期只在静态信息不足处兜底，例如通过字段打开已打开区域、FFI 返回值、cown 释放和调试模式下的拓扑断言。

## 1. 堆模型

程序堆被划分为：

1. **开放区域栈** `RS`：当前线程打开的区域栈，栈顶为 active 区域，其余为 paused 区域。
2. **闭合区域集合** `H_closed`：不可直接读写的隔离区域。
3. **冻结堆** `H_frozen`：深不可变对象集合。
4. **cown 集合** `H_cown`：可共享并发所有者，每个 cown 持有一个闭合区域或不可变值。
5. **纯值空间**：整数、布尔、浮点、枚举无负载值等可复制值。

每个可变对象恰好属于一个区域。每个闭合区域有一个桥对象。区域内部允许任意对象图；区域边界限制由引用能力和拓扑不变式维护。

直观例子：

```gura
struct Graph {
    var root: mut Node
}

struct Node {
    var next: mut Node?
}

let graph: iso Graph = new iso Graph(root: new mut Node(next: none))
```

可以把它看成：

```text
H_closed:
  Region R_graph
    bridge = Graph
    store = {
      Graph(root -> Node#1),
      Node#1(next -> none)
    }

local stack:
  graph: iso Graph -> R_graph.bridge
```

闭合 region 的关键点不是对象数量，而是边界：`Graph` 和 `Node#1` 可以在 region 内部互相引用；外部只能有 `graph` 这一条指向桥对象的 `iso` 入边，不能直接拿到 `Node#1` 的长期 `mut` 引用。

## 2. 区域状态

### 2.1 active

active 区域是当前线程唯一可变区域。允许：

- 读取区域内对象；
- 写入区域内 `var` 字段；
- 分配 `mut` 和 `tmp` 对象；
- 按该区域策略执行局部内存管理；
- 创建到外层 paused 区域的临时引用，前提是引用不逃逸。

例子：

```gura
var tree: iso Tree = make_tree()

enter tree as root {
    root.value = 1
    let child: mut Tree = new mut Tree(value: 2)
    root.left = child
}
```

在 `enter` 块内，`tree` 所属 region 是 active：

```text
open region stack:
  top: R_tree active
       root: mut Tree
       child: mut Tree

允许：
  root.value = 1
  new mut Tree(...)
  root.left = child
```

active 的重点是“当前可变窗口”。当前线程能直接改 `R_tree`，但不能同时直接修改另一个 region。

### 2.2 paused

paused 区域已经打开，但由于更内层 `enter` 或 `explore` 成为当前区域栈顶而临时挂起。允许：

- 通过 `pau T` 读取；
- 调用 `self: pau` 或 `self: imm` 可用的方法；
- 作为 `tmp` 对象中的临时引用目标。

禁止：

- 写字段；
- 分配对象到该区域；
- 释放对象；
- 调用 `self: mut` 方法。

Core Gura v0 中，`explore` 采用严格区域栈语义：它会挂起原 active 区域，并把被探索区域作为只读 paused 视图暴露。因此 `explore` body 不能写外层区域。

```gura
var users: iso UserList = load_users()
var log: iso Log = make_log()

enter users as u {
    let cached: i64 = explore log as l {
        l.count()
    }
    u.cached_log_count = cached
}
```

执行过程：

```text
inside enter users:
  R_users: active, u: mut UserList

inside explore log:
  R_log: paused/read-only view, l: pau Log
  R_users: paused, u becomes pau UserList

允许：
  l.count()

禁止：
  l.append(...)
  u.cached_log_count = 1
  new mut Entry(...)
```

这样保证被探索区域和外层区域在 `explore` 期间都保持拓扑稳定。若未来加入“只读借用另一个 closed region，同时继续写当前 active region”的轻量查询构造，它必须作为独立语义设计。

### 2.3 closed

closed 区域完全隔离。区域外只能持有一个 `iso` 引用指向桥对象，或由 cown 持有。闭合区域不可直接读写，必须通过 `enter` 或 `explore` 打开。

例子：

```gura
let graph: iso Graph = make_graph()

// 非法：closed region 不能直接读写字段
let root = graph.root

// 合法：先打开
enter graph as g {
    let root = g.root
    g.root = normalize(root)
}
```

`make_graph()` 返回后，`graph` 指向的是 closed region 的桥对象：

```text
local stack:
  graph: iso Graph ---> R_graph.bridge

H_closed:
  R_graph
    bridge: Graph
    internal objects: Graph, Node, Edge...
```

closed 的重点是“隔离且可移动”：因为外部只有一条 `iso` 桥引用，移动这一个引用就等价于移动整个对象图的所有权。

```gura
let graph: iso Graph = make_graph()
spawn worker(move graph)   // 零拷贝转移整个 R_graph
```

如果要共享而不是转移，可以先冻结：

```gura
let graph: iso Graph = make_graph()
let frozen: imm Graph = freeze(move graph)
```

冻结后对象不再需要可变 region 边界，可以跨线程复制共享。

## 3. 拓扑不变式

gura 维护以下拓扑不变式：

1. 每个非冻结可变对象属于唯一一个区域。
2. 闭合区域最多有一个外部入边，且该入边必须指向桥对象。
3. 闭合区域的出边只能指向：
   - 本区域对象；
   - 嵌套闭合区域的桥对象；
   - 冻结对象；
   - cown。
4. active 区域可临时指向 paused 区域对象，但该引用只能存放于局部变量或 `tmp` 对象，不能存入普通 `mut` 对象。
5. paused 区域不能指向后来打开的 active 区域，除非该引用是打开路径上的稳定桥对象引用。
6. 冻结对象只能引用冻结对象或纯值，不能引用可变区域对象。
7. cown 内部值释放时必须闭合。

这些不变式保证：如果两个线程能同时直接访问某个对象，则该对象必定是不可变对象；可变对象的直接访问总是由某个线程独占。

## 4. 引用能力

### 4.1 `mut T`

`mut T` 是 active 区域内部对象的可变引用。它允许：

- 读取字段；
- 写 `var` 字段；
- 调用 `self: mut` 方法；
- 创建同区域内部别名。

限制：

- 不得存入其他闭合区域；
- 不得跨线程传递；
- 不得从 `enter` 块返回，除非通过 `merge`、`freeze` 或重新包装成 `iso` 闭合区域。

### 4.2 `tmp T`

`tmp T` 是 active 区域栈帧中的临时对象引用。它和 `mut T` 类似，但生命周期被当前 `enter`/`explore` 块限制。

`tmp` 的主要用途是保存指向 paused 区域的临时引用，例如迭代器、zip 视图和短期索引结构。

规则：

- `tmp` 可以引用 paused 对象；
- `tmp` 不能赋给外层变量；
- `tmp` 不能存入闭合区域；
- `tmp` 块结束时整体失效。

### 4.3 `iso T`

`iso T` 是外部唯一引用，指向闭合区域的桥对象。`iso` 表示持有整个区域的所有权。

性质：

- 不可复制；
- 默认移动；
- 不可直接解引用字段；
- 可用于 `enter`、`explore`、`freeze`、`merge`、`cown` 和 `spawn`。

使用示例：

```gura
let tree: iso Tree = new iso Tree()
enter tree as t {
    t.add(...)
}
let frozen: imm Tree = freeze(move tree)
```

### 4.4 `imm T`

`imm T` 是深不可变引用。它可复制、可跨线程共享、可放入任意区域对象中。

要求：

- `imm` 对象的传递闭包必须全部不可变；
- `imm` 方法不能观察可变内部状态；
- 实现层面的引用计数、缓存和延迟初始化必须不破坏程序可见不可变性。

### 4.5 `pau T`

`pau T` 是 paused 区域中的临时只读视图。

允许：

- 读字段；
- 调用 `self: pau` 或 `self: imm` 方法；
- 被 `tmp` 对象引用。

禁止：

- 写字段；
- 调用 `self: mut` 方法；
- 存入普通 `mut` 对象；
- 跨越当前区域栈作用域逃逸。

### 4.6 `var T`

`var T` 是可强更新局部存储槽，不是一种普通对象引用能力。它用于表达局部变量可重新赋值，并支持 `iso` 交换。

```gura
var root: iso Node = new iso Node()
let old = move root
root = new iso Node()
```

### 4.7 `cown T`

`cown T` 是可共享的并发所有者。它不提供直接字段访问，必须通过 `acquire`。

```gura
acquire account as a {
    a.deposit(10)
}
```

`acquire` 期间，cown 内部区域转移到当前线程并成为可打开区域；释放时必须闭合。

## 5. 视点适配

字段类型不是固定暴露的，而是由接收者能力决定。若表达式 `x` 的能力为 `Kx`，字段声明能力为 `Kf`，则访问 `x.f` 的能力为 `Kx ⊙ Kf`。

| `x` \ `f` | `mut` | `tmp` | `imm` | `iso` | `pau` |
| --- | --- | --- | --- | --- | --- |
| `mut` | `mut` | 不可直接读 | `imm` | 不可解引用，只能 move/swap | 不可直接读 |
| `tmp` | `mut` | `tmp` | `imm` | 不可解引用，只能 move/swap | `paused` |
| `imm` | `imm` | `imm` | `imm` | `imm` | `imm` |
| `iso` | 不可访问 | 不可访问 | 不可访问 | 不可访问 | 不可访问 |
| `paused` | `paused` | `paused` | `imm` | 不可解引用，只能 enter/swap | `paused` |

解释：

- `iso` 表示闭合区域外部唯一引用，不能直接读字段；必须 `enter`。
- `imm` 视图把所有字段都看成 `imm`，确保深冻结。
- `paused` 视图把可变字段看成 `paused`，禁止写入。
- `tmp` 可以持有 paused 引用，因此能构造只在当前块有效的遍历结构。

## 6. 区域创建

### 6.1 `new mut`

`new mut T(...)` 在当前 active 区域分配对象。如果当前没有显式 active 区域，编译器为线程入口创建隐式根区域。

### 6.2 `new iso`

`new iso T(...)` 创建新闭合区域，区域初始只包含桥对象。构造参数必须是：

- 纯值；
- `imm`；
- `iso`，并被移动为嵌套区域；
- 当前表达式内的 fresh `mut` 构造树。

fresh 构造树中的对象不能先绑定到外部名称、写入外部字段或经过未知函数返回后再传入 `new iso`。普通命名 `mut` 不会被隐式搬入新区域，因为外层 active 区域中可能仍有别名。

### 6.3 `new tmp`

`new tmp T(...)` 在当前区域栈帧的临时存储中分配对象，作用域结束即失效。

## 7. 打开与关闭区域

### 7.1 `enter`

`enter x as y { body }`：

1. 要求 `x: iso T` 或 `x` 是存放 `iso T` 的可打开存储位置；
2. 动态确认目标区域未打开；
3. 把目标区域从 closed 移入开放区域栈顶；
4. 外层 active 区域变为 paused；
5. 在块内绑定 `y: mut T`；
6. 块结束时检查没有非法逃逸引用；
7. 选择当前桥对象并关闭区域。

如果 `x` 是 `var iso T`，块内桥对象存储槽可强更新，从而改变桥对象。

### 7.2 `explore`

`explore x as y { body }` 以 paused 方式访问区域。目标区域被打开后立即作为 paused 视图暴露，原 active 区域也被挂起；块内 `y: pau T`，不能写入被探索区域或外层区域，不能 `new mut`，`tmp`/`pau` 引用不能逃逸，返回值限制与 `enter` 相同。它保证被探索区域和外层区域在只读遍历期间都保持稳定，适合：

- 只读遍历；
- 查询索引结构；
- 在不触发区域内存管理成本的情况下读取大型结构。

### 7.3 桥对象切换

区域打开期间，`enter x as bridge` 中的 `bridge` 是当前桥对象的桥槽。它可以像 `mut T` 一样访问对象，也可以被赋值为同一区域中的另一个非临时对象，从而指定区域关闭后的新桥对象。`bridge` 不是关键字，只是示例绑定名。

示例：

```gura
struct List {
    var head: imm String
}

struct Iterator {
    var current: mut List
}

var list: iso List = make_list()
enter list as bridge {
    let iter: mut Iterator = new mut Iterator(current: bridge)
    bridge = iter
}
```

块结束后，`list` 仍是同一个闭合区域的唯一外部引用，但当前桥对象从 `List` 切换为 `Iterator`，因此外层存储槽类型更新为 `iso Iterator`。

桥对象切换必须满足：新桥对象属于当前 active 区域；新桥对象不能是 `tmp`；新桥对象不能来自外层 paused 区域或另一个闭合区域；块结束后外层 `var` 存储槽恢复为新的 `iso` 桥对象。若打开源不是可强更新位置，只能沿用原桥对象或切换到类型兼容的桥对象。

## 8. 移动、交换与埋藏

### 8.1 移动

`move x` 是破坏性读取。读取后 `x` 变为未定义，直到重新赋值。

### 8.2 交换

赋值可被视为交换：写入新值，返回旧值。对 `iso` 存储槽，交换是保持外部唯一性的主要方式。

```gura
let old: iso Tree = (slot := new iso Tree())
```

### 8.3 埋藏

当 `let` 绑定中的 `iso` 被读取时，原绑定被埋藏，之后不可再使用。这阻止同一个 `iso` 被多次传参或复制。

## 9. 冻结

`freeze(move x)` 要求 `x: iso T`，返回 `imm T`。

冻结规则：

1. 输入区域必须闭合；
2. 区域及所有嵌套区域递归冻结；
3. 冻结后区域边界消失；
4. 冻结对象只能引用冻结对象；
5. 冻结是深操作，时间复杂度与被冻结对象图大小相关。

不采用默认隐式冻结，因为隐式冻结可能意外改变对象可变性，并破坏局部引用计数或闭合判断的精确性。实现可为字符串、类型元数据等内建对象提供受控隐式冻结。

## 10. 合并

`merge(move x)` 要求 `x: iso T`，返回 `mut T`，并把源区域对象并入当前 active 区域。

规则：

- 源区域本身消失；
- 源区域的直接嵌套子区域仍保持独立闭合区域；
- 返回的桥对象变为当前 active 区域内普通对象；
- 合并不能把区域并入 paused 或 closed 区域。

## 11. cown 并发模型

`cown` 是 gura 在 Reggio 区域隔离模型之上采用 Verona 并发所有者思想的并发层扩展。Reggio 核心模型只要求闭合区域通过 `iso` 外部唯一引用访问；`cown` 负责把这种唯一访问延迟到运行时排他获取。

### 11.1 创建

```gura
let c: cown Account = cown(move account)
```

`account` 必须是闭合区域 `iso T`。gura 还允许 `cown` 包装深不可变值 `imm T` 或另一个 `cown T`；这两者是对 Verona `cown` 的扩展。

### 11.2 获取

```gura
acquire c as value {
    value.do_work()
}
```

获取语义：

1. 运行时排他获取 cown；
2. 取出内部闭合区域；
3. 允许在块内以 `mut` 方式访问；
4. 块结束前必须关闭区域；
5. 把闭合区域放回 cown 并释放。

多 cown 获取必须按运行时全序进行，重复获取同一 cown 非法。块正常返回、提前返回或 panic/unwind 时都必须执行释放路径；如果释放时区域未闭合，运行时报告确定性 panic，且不得让其他线程观察到半开放 cown。Core v0 中 `cown(imm T)` 获取为 `imm T`，不提供 `mut` 绑定；`cown(cown T)` 不递归获取内部 cown。

### 11.3 多 cown 获取

```gura
acquire a as x, b as y {
    ...
}
```

运行时必须保证获取顺序确定，避免死锁。语言层面对多个 cown 的块提供原子排他访问语义。

### 11.4 读获取

后续版本可加入：

```gura
acquire read c as value {
    ...
}
```

读获取返回 `pau T` 或 `imm-like T`，允许多个读者并发。原型阶段可先只支持写获取。

## 12. 线程与任务边界

`spawn` 的参数必须是安全可转移或安全可共享：

| 参数能力 | 行为 |
| --- | --- |
| `iso T` | 移动到新任务 |
| `imm T` | 共享 |
| `cown T` | 共享 cown 本身 |
| 纯值 | 复制 |
| `mut T` | 禁止 |
| `tmp T` | 禁止 |
| `pau T` | 禁止 |

这保证新任务不能直接获得其他线程 active 或 paused 区域中的可变对象引用。

## 13. 内存管理策略

区域创建时可指定策略：

```gura
new iso<Arena> RequestContext()
new iso<RC> GraphNode(...)
new iso<GC> ObjectGraph(...)
```

策略含义：

- `Arena`：区域整体释放，适合请求生命周期和临时批处理；
- `RC`：区域内部引用计数，适合资源及时回收；
- `GC`：区域内部 tracing，适合循环图结构；
- `Manual`：手动管理区域内部对象生命周期，只能在 `unsafe` 或静态线性证明可保证无悬垂引用的上下文中使用。

安全 Gura 默认支持的是区域级手动释放：当 `iso` 区域的唯一外部引用被消费且不再使用时，整个 closed region 可以确定性销毁。安全代码不提供任意 `free(obj)`，因为区域内对象可能被同一区域内其他对象引用；若需要逐对象手动释放，应显式选择 `Manual` 策略并通过 unsafe/线性约束证明释放点之后没有别名继续访问该对象。

由于区域隔离，区域内部 GC 不需要追踪任意跨区域可变引用。active 区域的内存管理不必同步其他线程。

## 14. 动态检查边界

虽然 gura 是静态语言，以下情况需要运行期检查：

1. 通过字段中的 `iso` 打开区域时，目标区域可能已被外层打开；
2. cown 释放时检查内部区域闭合；
3. FFI 返回对象的 owner/capability 校验；
4. unsafe 块退出时的区域拓扑校验；
5. 调试模式下写屏障断言。

动态错误是确定性的结构错误，而不是依赖调度时序的数据竞争错误。

## 15. Core v0 限制

为先得到一个可实现的安全核心，v0 对若干高风险特性采取保守限制：

- `explore` 会挂起当前 active 区域，body 不能写任一区域；
- `new iso` 只接受纯值、`imm`、被移动的 `iso` 和 fresh `mut` 构造树；
- safe 代码不提供逐对象 `free`，`Manual` 策略的逐对象释放只能在 unsafe 或未来线性证明中使用；
- active explicit region、`mut`、`tmp`、`pau` 和 open-borrowed `iso` 不能跨 `await`；async/await 保留；
- mutable global 禁止，模块级可变共享状态必须放在 `cown` 中；
- 闭包或函数值不能捕获 `mut`、`tmp`、`pau` 后逃逸，也不能作为 `spawn` 目标；
- `acquire read` 多读者模式保留；
- destructor/drop、`imm` 对象回收策略和隐式冻结白名单仍是未决设计。

## 16. unsafe 边界

`unsafe` 可用于调用底层 API 或手动内存操作，但必须满足：

- 不伪造引用能力；
- 不创建跨区域非法引用；
- 不把 `mut` 或 `tmp` 引用存入全局或其他线程；
- 不绕过 cown 获取访问内部对象；
- unsafe 块出口可由编译器插入拓扑校验。

## 17. 安全性结论

在不违反 `unsafe` 契约的程序中：

1. 不存在悬垂引用：`tmp` 和 `paused` 受区域栈作用域限制，闭合区域不可直接访问。
2. 不存在释放后使用：区域释放需要外部唯一桥引用被丢弃，区域内部无外部别名。
3. 不存在数据竞争：可变对象只通过 active 区域或 acquired cown 访问，二者均排他。
4. 不可变共享安全：`imm` 是深不可变，跨线程共享不需要锁。
5. 区域转移零拷贝：`iso` 移动只移动桥对象引用，不复制对象图。

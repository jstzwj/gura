# gura AST 设计

本文档定义 gura 编译器前端使用的抽象语法树。AST 分为四层：

1. **Concrete AST / CST**：保留 token、注释、括号和分号，用于格式化与错误恢复。
2. **Surface AST**：解析后的表层语法树，接近用户代码。
3. **Typed AST / HIR**：名称解析、类型推导、引用能力检查后的高层 IR。
4. **Region IR**：显式区域效果、移动、打开、关闭、冻结、合并和 cown 操作。

本文主要描述 Surface AST，并说明降低到 HIR/Region IR 时需要补充的字段。

## 1. AST 通用结构

所有节点包含：

```text
NodeId       编译单元内唯一 ID
Span         源码范围
Attrs        属性列表
```

概念结构：

```rust
struct AstNode<T> {
    id: NodeId,
    span: Span,
    attrs: Vec<Attribute>,
    kind: T,
}
```

文档中用伪 Rust/TypeScript 混合表示。

## 2. 编译单元

```text
SourceFile {
    module: ModuleDecl?
    imports: [ImportDecl]
    decls: [Decl]
}

ModuleDecl {
    path: ModulePath
}

ImportDecl {
    path: ImportPath
    alias: Ident?
    items: [ImportItem]?
}
```

## 3. 顶层声明

```text
Decl =
    FnDecl
  | StructDecl
  | EnumDecl
  | TraitDecl
  | ImplDecl
  | TypeAliasDecl
  | ConstDecl
```

### 3.1 函数

```text
FnDecl {
    name: Ident
    generics: GenericParams
    params: [Param]
    return_type: TypeRef?
    body: FnBody
    effects: EffectSet?
    visibility: Visibility
}

Param {
    pattern: Pattern
    type: TypeRef
    default: Expr?
}

FnBody = BlockBody(Block) | ExprBody(Expr)
```

方法中的 `self` 是特殊参数：

```text
SelfParam {
    capability: SelfCapability
}

SelfCapability = mut | imm | pau | tmp
```

`iso self` 不作为普通方法 receiver，必须先 `enter`。

### 3.2 结构体

```text
StructDecl {
    name: Ident
    generics: GenericParams
    fields: [FieldDecl]
    visibility: Visibility
}

FieldDecl {
    mutability: FieldMutability   // let | var
    name: Ident
    type: TypeRef
    default: Expr?
    visibility: Visibility
}
```

### 3.3 枚举

```text
EnumDecl {
    name: Ident
    generics: GenericParams
    variants: [EnumVariant]
}

EnumVariant {
    name: Ident
    payload: VariantPayload
}

VariantPayload = none | Tuple([TypeRef]) | Struct([FieldDecl])
```

### 3.4 trait

```text
TraitDecl {
    name: Ident
    generics: GenericParams
    items: [TraitItem]
}

TraitItem = RequiredFn(FnSignature) | ProvidedFn(FnDecl) | AssociatedType(TypeAssoc)
```

### 3.5 impl

```text
ImplDecl {
    generics: GenericParams
    trait_ref: TypeRef?
    self_type: TypeRef
    items: [ImplItem]
}
```

### 3.6 类型别名与常量

```text
TypeAliasDecl {
    name: Ident
    generics: GenericParams
    target: TypeRef
}

ConstDecl {
    name: Ident
    type: TypeRef?
    value: Expr
}
```

## 4. 类型 AST

```text
TypeRef =
    PathType
  | CapabilityType
  | OptionalType
  | UnionType
  | TupleType
  | FnType
  | NeverType
  | InferType
```

### 4.1 路径类型

```text
PathType {
    path: TypePath
    args: [TypeRef]
}
```

示例：`List<imm String>`。

### 4.2 能力类型

```text
CapabilityType {
    capability: Capability
    inner: TypeRef
}

Capability = Mut | Tmp | Iso | Imm | Paused | Cown
```

示例：`iso Tree`、`mut Node`、`cown Account`。

注意：`var` 不是普通 `CapabilityType`，而是局部绑定在 HIR 中产生的 `StoreType`。

### 4.3 可空类型

```text
OptionalType {
    base: TypeRef
}
```

`T?` 降低为 `Option<T>` 或 nullable niche representation。

### 4.4 联合类型

```text
UnionType {
    members: [TypeRef]
}
```

用于错误建模、渐进 FFI 和模式匹配窄化。

### 4.5 函数类型

```text
FnType {
    params: [TypeRef]
    return_type: TypeRef
    effects: EffectSet?
}
```

## 5. 语句与块

```text
Block {
    stmts: [Stmt]
    tail: Expr?
}

Stmt =
    LetStmt
  | VarStmt
  | ExprStmt
  | SemiStmt
  | ItemStmt
```

```text
LetStmt {
    pattern: Pattern
    type: TypeRef?
    value: Expr?
}

VarStmt {
    pattern: Pattern
    type: TypeRef?
    value: Expr?
}
```

`let` 生成不可重新绑定名称；`var` 生成可强更新存储槽。

## 6. 表达式 AST

```text
Expr =
    LiteralExpr
  | NameExpr
  | PathExpr
  | FieldExpr
  | IndexExpr
  | CallExpr
  | MethodCallExpr
  | UnaryExpr
  | BinaryExpr
  | AssignExpr
  | IfExpr
  | MatchExpr
  | BlockExpr
  | LoopExpr
  | WhileExpr
  | ForExpr
  | ReturnExpr
  | BreakExpr
  | ContinueExpr
  | NewExpr
  | MoveExpr
  | EnterExpr
  | ExploreExpr
  | FreezeExpr
  | MergeExpr
  | CownExpr
  | AcquireExpr
  | SpawnExpr
  | UnsafeExpr
```

### 6.1 字面量

```text
LiteralExpr {
    kind: LiteralKind
    value: String      // 原始 token 文本
    suffix: String?    // i32/i64/f32/f64，若无后缀则为空
}

LiteralKind = Integer | Float | bool | none | Char | String
IntegerLiteral = { value, suffix?: i32 | i64 }
FloatLiteral = { value, suffix?: f32 | f64 }
```

### 6.2 名称与路径

```text
NameExpr { name: Ident }
PathExpr { path: ValuePath }
```

### 6.3 字段与索引

```text
FieldExpr {
    receiver: Expr
    field: Ident
}

IndexExpr {
    receiver: Expr
    index: Expr
}
```

HIR 中字段访问会附加：

```text
ResolvedFieldAccess {
    field_id: FieldId
    receiver_type: Type
    declared_field_type: Type
    adapted_type: Type
    requires_move: bool
}
```

### 6.4 调用

```text
CallExpr {
    callee: Expr
    args: [Arg]
}

MethodCallExpr {
    receiver: Expr
    method: Ident
    type_args: [TypeRef]
    args: [Arg]
}

Arg {
    label: Ident?
    value: Expr
}
```

方法调用在 HIR 中根据 receiver 能力解析到匹配的 `self` 能力实现；默认不进行 `mut` 到 `paused` 的隐式降级。

### 6.5 运算

```text
UnaryExpr {
    op: UnaryOp
    expr: Expr
}

BinaryExpr {
    op: BinaryOp
    lhs: Expr
    rhs: Expr
}
```

### 6.6 赋值

```text
AssignExpr {
    target: PlaceExpr
    op: AssignOp
    value: Expr
}

PlaceExpr = NamePlace | FieldPlace | IndexPlace | DerefPlace
```

赋值在 Region IR 中降低为 `swap(place, value)`，并返回旧值；语句形式会丢弃旧值。

### 6.7 移动

```text
MoveExpr {
    place: PlaceExpr
}
```

HIR 附加：

```text
MoveInfo {
    moved_type: Type
    source_state_after: Undef | ReassignedRequired
}
```

### 6.8 new

```text
NewExpr {
    allocation: AllocationKind
    type: TypeRef
    strategy: MemoryStrategy?
    args: [Arg]
}

AllocationKind = Mut | Tmp | Iso | Imm
MemoryStrategy = Arena | RC | GC | Manual | Custom(TypePath)
```

降低规则：

- `new mut` -> `RegionAlloc(active, T)`；
- `new tmp` -> `TempAlloc(current_frame, T)`；
- `new iso` -> `NewClosedRegion(strategy, bridge: T)`；
- `new imm` -> `NewImmutable(T)` 或 `Freeze(NewClosedRegion(...))`。

### 6.9 enter

```text
EnterExpr {
    source: Expr
    binding: Pattern
    body: Block
}
```

示例：

```gura
enter tree as root { ... }
```

HIR 中补充：

```text
EnterInfo {
    region_source: PlaceOrValue
    bridge_type: Type
    capture_set: [Capture]
    outer_capability_adaptations: [Adaptation]
    dynamic_open_check: bool
    result_constraint: IsoOrImmOrPure
}
```

Region IR 降低：

```text
effect Enter(region_source, binding)
body
new_bridge = ResolveBridge(...)
effect Exit(new_bridge, result)
```

### 6.10 explore

```text
ExploreExpr {
    source: Expr
    binding: Pattern
    body: Block
}
```

`ExploreExpr` 可以降低成特殊 Region IR，也可脱糖为：打开目标区域为 paused，然后创建临时 active 区域执行 body。

HIR 中 `binding` 类型为 `pau T`。

### 6.11 freeze

```text
FreezeExpr {
    value: Expr
}
```

要求输入类型 `iso T`，输出 `imm T`。

### 6.12 merge

```text
MergeExpr {
    value: Expr
    target: Expr?
}
```

无 target 时合并入当前 active 区域。输入 `iso T`，输出 `mut T`。

### 6.13 cown

```text
CownExpr {
    value: Expr
}
```

输入 `iso T | imm T | cown T`，输出 `cown T`。

### 6.14 acquire

```text
AcquireExpr {
    acquisitions: [Acquisition]
    mode: AcquireMode
    body: Block
}

Acquisition {
    cown: Expr
    binding: Pattern
}

AcquireMode = Write | Read
```

示例：

```gura
acquire from as src, to as dst { ... }
```

HIR 中补充：

```text
AcquireInfo {
    cown_ids: [ExprId]
    acquisition_order_key: [OrderKey]
    release_checks: [RegionClosedCheck]
}
```

### 6.15 spawn

```text
SpawnExpr {
    callee: Expr
    args: [Arg]
}
```

HIR 要求所有参数满足 `SendSafe`：`iso`、`imm`、`cown` 或纯值。

### 6.16 unsafe

```text
UnsafeExpr {
    body: Block
}
```

HIR 标记 unsafe 边界，并可插入 Region IR 拓扑校验。

## 7. 模式 AST

```text
Pattern =
    WildcardPattern
  | BindingPattern
  | LiteralPattern
  | TuplePattern
  | StructPattern
  | EnumPattern
  | OrPattern
  | TypeTestPattern
```

```text
BindingPattern {
    mutability: BindingMutability  // let | var?
    name: Ident
}
```

模式匹配会进行类型窄化，不能隐式复制 `iso` 负载。匹配含 `iso` 的枚举时，必须显式 `move` 或使用借用视图。

## 8. 属性 AST

```text
Attribute {
    path: AttrPath
    args: [AttrArg]
}
```

常见属性：

```gura
@repr(C)
@inline
@region_strategy(Arena)
@unsafe_contract("preserves_region_topology")
```

## 9. HIR 类型结构

名称解析与类型检查后，HIR 使用规范化类型：

```text
Type =
    NominalType(TypeId, [Type])
  | Capability(Capability, Type)
  | Store(StoreCapability, Type)
  | Tuple([Type])
  | Function(FnSig)
  | Union([Type])
  | Optional(Type)
  | Never
  | Error

StoreCapability = Var | FieldMut | FieldLet | TmpSlot
```

引用能力和存储槽分离，以避免把 `var` 当成对象引用能力。

## 10. Region IR

Region IR 显式描述会影响区域拓扑的操作。

```text
RegionInstr =
    Load { dest, place }
  | Swap { dest_old, place, value }
  | Move { dest, place }
  | AllocMut { dest, type, args }
  | AllocTmp { dest, type, args }
  | AllocIso { dest, strategy, type, args }
  | Freeze { dest, iso_value }
  | Merge { dest, iso_value, target_region }
  | Enter { region, bridge_slot, dynamic_check }
  | Exit { region, new_bridge, result }
  | ExploreEnter { region }
  | ExploreExit { region }
  | CownNew { dest, value }
  | CownAcquire { cowns, mode }
  | CownRelease { cowns, closed_checks }
  | Spawn { fn, args }
  | TopologyAssert
```

Region IR 的作用：

- 让类型检查后的区域效果可验证；
- 为 borrow/escape 检查提供控制流图输入；
- 为运行时插入最小必要检查；
- 为后端选择内存管理策略生成元数据。

## 11. 能力检查需要的附加表

编译器在 HIR/Region IR 阶段维护：

```text
CapabilityEnv: NameId -> Type
MoveState: Place -> Available | Moved | MaybeMoved
RegionStackModel: [RegionFrame]
BorrowSet: RegionId -> [Borrow]
EscapeSet: ExprId -> EscapeClass
CownState: CownId -> Unknown | Acquired | Released
```

### 11.1 EscapeClass

```text
EscapeClass =
    NoEscape
  | ReturnValue
  | StoreInActiveRegion
  | StoreInTmp
  | StoreInClosedRegion
  | SpawnArg
  | CownStore
  | Global
```

`mut`、`tmp`、`pau` 的逃逸类别受严格限制。

## 12. AST 到 Region IR 示例

源码：

```gura
let tree = new iso Tree()
enter tree as t {
    t.left = new iso Tree()
}
```

Surface AST：

```text
LetStmt(tree, NewExpr(Iso, Tree))
EnterExpr(Name(tree), Binding(t), Block[
    AssignExpr(Field(t, left), NewExpr(Iso, Tree))
])
```

Region IR：

```text
AllocIso tree, strategy=default, type=Tree
Enter region=tree, bridge_slot=t, dynamic_check=false
AllocIso tmp0, strategy=default, type=Tree
Swap _, place=t.left, value=move tmp0
Exit region=tree, new_bridge=t, result=unit
```

## 13. AST 稳定性原则

1. Surface AST 保留用户意图，不做复杂脱糖。
2. HIR 显式化能力、移动和重载解析。
3. Region IR 显式化所有区域拓扑效果。
4. MIR/后端不再理解复杂表层语法，只消费 Region IR 的安全事实。
5. 错误消息应尽量引用 Surface AST 的 Span，而不是脱糖后的内部节点。

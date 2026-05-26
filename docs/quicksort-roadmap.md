# Gura 快速排序编译路线

本文记录让 Gura 最终能够编译并运行快速排序程序所需的阶段路线。

## 当前状态

当前 Gura 已经可以完成最小可执行程序路线：

```gura
fn main(): i64 {
  return 0
}
```

也就是说，`gura build <input.gura> [-o output]` 已经可以把满足 `fn main(): i64` 入口约束的程序编译、链接成可执行文件。

但目前还不能直接编译快速排序，因为快速排序依赖数组、索引、索引赋值、可变数组参数等能力，这些还需要继续实现。

## 阶段 1：支持 `puts("hello")`

下一步先实现最小外部函数调用或内建输出能力，让 Gura 可以编译运行类似：

```gura
fn main(): i64 {
  puts("hello")
  return 0
}
```

需要补齐的能力包括：

- 字符串字面量 codegen
- `puts` 的外部函数声明或 builtin lowering
- 可执行文件链接时能正确解析 `puts`
- stdout 端到端测试

完成后，Gura 将从“只能返回退出码”进入“可以产生可观察输出”的阶段。

## 阶段 2：支持数组最小能力

快速排序的核心依赖数组，因此第二阶段需要实现最小数组模型。

建议先支持固定元素类型的数组或 slice，例如：

```gura
var data: mut [i64] = [5, 2, 9, 1, 3]
```

需要补齐的语言能力包括：

- 数组或 slice 类型语法：`[i64]`、`mut [i64]`
- 数组字面量：`[5, 2, 9, 1, 3]`
- 索引表达式：`a[i]`
- 索引赋值：`a[i] = value`
- 可变数组参数：`fn f(a: mut [i64]): unit`
- 数组长度能力：`len(a)` 或 `a.len`

这一阶段完成后，应能编译运行简单数组程序，例如交换两个元素：

```gura
fn swap(a: mut [i64], i: i64, j: i64): unit {
  let tmp: i64 = a[i]
  a[i] = a[j]
  a[j] = tmp
}

fn main(): i64 {
  var data: mut [i64] = [5, 2, 9]
  swap(data, 0, 2)
  return 0
}
```

## 阶段 3：支持快速排序

当数组、索引、索引赋值和可变数组参数都可用后，可以实现快速排序。

目标代码大致如下：

```gura
fn swap(a: mut [i64], i: i64, j: i64): unit {
  let tmp: i64 = a[i]
  a[i] = a[j]
  a[j] = tmp
}

fn partition(a: mut [i64], lo: i64, hi: i64): i64 {
  let pivot: i64 = a[hi]
  var i: i64 = lo - 1
  var j: i64 = lo

  while j < hi {
    if a[j] <= pivot {
      i = i + 1
      swap(a, i, j)
    }
    j = j + 1
  }

  swap(a, i + 1, hi)
  return i + 1
}

fn quicksort(a: mut [i64], lo: i64, hi: i64): unit {
  if lo < hi {
    let p: i64 = partition(a, lo, hi)
    quicksort(a, lo, p - 1)
    quicksort(a, p + 1, hi)
  }
}

fn main(): i64 {
  var data: mut [i64] = [5, 2, 9, 1, 3]
  quicksort(data, 0, 4)
  return 0
}
```

这一阶段需要确认或补齐：

- 递归函数调用是否已能正确通过 sema 和 codegen
- 数组元素读取和写入是否正确生成 LLVM IR
- 可变数组参数是否不会被错误复制
- 快排后数组内容是否可验证

## 建议测试顺序

推荐按以下端到端测试逐步推进：

1. `return 0` 可执行程序
2. `puts("hello")` 输出程序
3. 数组字面量初始化
4. 数组索引读取
5. 数组索引赋值
6. `swap(mut [i64], i64, i64)`
7. `partition(mut [i64], i64, i64)`
8. `quicksort(mut [i64], i64, i64)`

每一步都应该有 parser、sema、codegen 或 executable 层面的测试，避免一次性实现完整快排时定位困难。

## 当前结论

快速排序是一个合适的中期里程碑，但不应该作为下一步直接实现。

推荐路线是：

1. 先实现 `puts("hello")`
2. 再实现数组最小能力
3. 最后实现并测试快速排序

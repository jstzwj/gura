# Gura

A systems programming language with region-based memory management and data-race-free concurrency.

> **Status:** Early development / research prototype.

## Overview

Gura is designed around a simple but powerful principle: **each mutable object belongs to one isolated region**. This region-ownership model provides strong memory safety guarantees without a garbage collector, while enabling zero-copy concurrency through region transfer.

### Key Features

- **Region-based memory management** — objects are grouped into regions, each with a clear lifecycle (active → paused → closed)
- **Data-race-free concurrency** — mutable objects are only accessible by one thread at a time; shared objects must be deeply immutable
- **Reference capabilities** — types encode what operations are allowed (`mut`, `imm`, `iso`, `tmp`, `pau`, `var`, `cown`)
- **Zero-copy region transfer** — move entire regions between threads via bridge objects
- **Deep freeze** — convert closed regions into immutable, freely shareable objects
- **Cown (concurrent owner)** — Verona-inspired acquire/release protocol for coordinated concurrent access
- **LLVM backend** — compiles to native code via LLVM

### Memory Safety Guarantees

| Guarantee | Mechanism |
|---|---|
| No dangling references | References are bound to region lifetimes |
| No double free | Region release requires unique external bridge reference |
| No use-after-free | Region isolation prevents external aliasing |
| No buffer overflows | Compile-time and runtime checks |
| No uninitialized reads | All objects are initialized |

## Example

```gura
fn main() {
    print("Hello, Gura!")
}
```

```gura
fn main() {
    let region = new iso Point(10, 20)
    let value = region.enter(|r| {
        r.x + r.y
    })
    print(value)  // 30
}
```

More examples are available in the [`examples/`](examples/) directory.

## Building

### Prerequisites

- C++23 compatible compiler (GCC 13+, Clang 16+)
- CMake 3.24+
- Conan 2.x
- LLVM development libraries

### Build Steps

```bash
# Install dependencies
conan install . --build=missing -s compiler.cppstd=23

# Configure and build
cmake --preset default
cmake --build --preset default
```

### Running Tests

```bash
ctest --preset default
```

## Documentation

| Document | Description |
|---|---|
| [Overview](docs/overview.md) | Language goals and design philosophy |
| [Specification](docs/spec.md) | Formal language specification |
| [Syntax](docs/syntax.md) | Syntax reference |
| [AST Design](docs/ast.md) | Abstract Syntax Tree structure |
| [Memory Safety](docs/memory-safety.md) | Region model and safety guarantees |

## Language Design

### Reference Capabilities

| Capability | Meaning |
|---|---|
| `mut T` | Mutable reference to active region object |
| `tmp T` | Temporary reference limited to current block |
| `iso T` | Unique external reference to closed region bridge |
| `imm T` | Deep immutable, freely shareable across threads |
| `pau T` | Read-only view of paused region object |
| `var T` | Strong-updating local storage slot |
| `cown T` | Concurrent owner with acquire/release protocol |

### Region Lifecycle

```
new iso → active (mutable) → paused (readable) → closed (isolated)
                                                        │
                                          ┌─────────────┼─────────────┐
                                          │             │             │
                                        freeze        merge       spawn
                                          │             │             │
                                        imm T       active R    another thread
                                    (shareable)   (mutable)
```

## Contributing

Contributions are welcome! This project is in its early stages, and there are many areas to help with:

- Compiler implementation (lexer, parser, type checking, codegen)
- Runtime library
- Documentation and examples
- Test coverage

## License

Licensed under the [Apache License 2.0](LICENSE).

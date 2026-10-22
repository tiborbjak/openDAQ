# AGENTS.md

Guidance for coding agents (and humans) working in this repository.

## Formatting

C++ layout is defined by `.clang-format` and checked by the hooks in `.pre-commit-config.yaml`.
Do not hand-format against it, and do not argue with it in review.

- **Version is pinned to clang-format 21.1.8.** Other major versions format differently. The
  pre-commit hook installs the pinned version itself. If you run clang-format by hand, check
  `clang-format --version` first, or use `pip install clang-format==21.1.8`.
- **Format only the lines you change.** Large parts of the codebase predate `.clang-format`.
  Do not reformat untouched code, because whole-file reformats bury the real change in the diff.
  - With the hooks installed (`pip install pre-commit && pre-commit install`, or the faster drop-in
    `pip install prek && prek install`), staged lines are formatted automatically on commit.
    Run them by hand with `pre-commit run` or `prek run`.
  - Without hooks, run `git clang-format --binary <path-to-clang-format-21> HEAD` before committing.
  - Never run `clang-format -i` on whole files you did not create. New files may be formatted in full.
- **Do not mix tabs and spaces in a file.** A few legacy files (e.g. `coretypes/delegate.hpp`) are
  indented with tabs. The hook keeps tabs in those files. By hand, add
  `--style "{BasedOnStyle: InheritParentConfig, UseTab: AlignWithSpaces}"` to the
  `git clang-format` command. New files always use spaces.
- **Builder chains** (`Builder().setA(...).setB(...).build()`) are joined onto one line whenever they
  fit in 140 columns. clang-format cannot keep one call per line, so accept the result. Use
  `// clang-format off` only where the layout really matters.
- **Never formatted:** `external/`, `*/external/`, `bindings/c/`, `bindings/python/**/generated/` and
  `shared/tools/RTGen/`. These are third-party or RTGen-generated code; to change generated
  bindings, change the generator.
- Non-C++ files (CMake, JSON, YAML) follow `.editorconfig`: 4-space indent (YAML: 2), no tabs,
  no trailing whitespace, final newline.

What clang-format enforces, in short: 4-space indent, 140 columns, Allman braces, `Type* ptr`, one
argument per line once a call wraps, constructor initializers on their own lines with leading commas,
no indentation inside namespaces. Lambda bodies are indented relative to the enclosing statement:

```cpp
return daqTry([&]
{
    auto lock = getRecursiveConfigLock2();
    ...
});
```

## Conventions clang-format cannot check

These describe how code has been written since late 2025. Follow them in new code; do not
retrofit old code you are not otherwise changing.

### Files

- File names are `snake_case`. Public headers go in `include/<library>/`, sources in `src/`, tests in `tests/`.
- A new `.h` file starts with the license header from
  `shared/tools/license-header-checker/license.in` (enforced by CI and the `check-new-files` hook),
  followed by `#pragma once`. Do not use `#ifndef` guards.
  - Exception: `*.custom.h` files are fragments that RTGen pastes into generated headers, so they
    have no `#pragma once`.
- Example code under `examples/` is exempt from the license header.

### Includes

- Use angle brackets with the library prefix, including for headers in the same library:
  `#include <opendaq/signal_impl.h>`. Use quotes only for test-local headers next to the test.
- In a `.cpp` file, include its own header first.
- Include order is not sorted automatically, because some platform headers are order-sensitive.
  Add new includes next to related ones and do not re-sort existing blocks.

### Namespaces

- Wrap library code in `BEGIN_NAMESPACE_OPENDAQ` / `END_NAMESPACE_OPENDAQ`. Do not indent the
  contents and do not add closing-namespace comments.

### Naming

| Kind | Style | Example |
|---|---|---|
| Types, enum values | PascalCase | `DeviceUpdateMode::Load` |
| Interfaces | `I` + PascalCase | `ISignalConfig` |
| Implementations | `<Name>Impl` | `SignalImpl` |
| Smart pointers | `<Name>Ptr` | `SignalPtr`, `ListPtr<IString>` |
| Functions, methods, variables, members | camelCase | `getDomainSignal`, `keepLastValue` |
| Static serialization hooks | PascalCase | `SerializeId()`, `Deserialize(...)` |
| Class/namespace-scope constants | UPPER_SNAKE | `BASEOBJECT_INTERFACE_ID` |
| Test fixtures | `<Subject>Test` | `TEST_F(SignalTest, SetPublicWithPrivateDomainFails)` |

- Members have no prefix or suffix (no `m_`, no trailing `_`). When a parameter shadows a member,
  qualify the member with `this->`.
- Test names are PascalCase sentences that state the expected behaviour.

### Interface implementations and error handling

- Interface methods are declared `ErrCode INTERFACE_FUNC name(...) override;` and return
  `OPENDAQ_SUCCESS` or an error code. They never let exceptions escape.
- Check out-parameters first with `OPENDAQ_PARAM_NOT_NULL(param);`. Return objects through
  them with `*out = value.addRefAndReturn();` or `.detach()`.
- To report a failure, use `return DAQ_MAKE_ERROR_INFO(OPENDAQ_ERR_..., "message");` and format
  messages with `fmt::format`.
- To propagate a failure, use `OPENDAQ_RETURN_IF_FAILED(errCode);`. Use
  `OPENDAQ_RETURN_IF_FAILED_EXCEPT(errCode, OPENDAQ_ERR_...)` to tolerate a specific code.
- Wrap code that may throw in `daqTry([&] { ... })`.
- In code that uses smart pointers, throw with `DAQ_THROW_EXCEPTION(...)`.

### C++ idioms

- Use `nullptr`, `auto` for locals with obvious types, `override` (never together with
  `virtual`), `static_cast` over C casts, `using` over `typedef`, and `enum class` over plain `enum`.
- Use `fmt::format`, not `std::format`.
- Take smart pointers as `const XxxPtr&` parameters.
- For single-statement `if`/`for`/`while` bodies, omit braces when the statement fits on one
  line. Use braces when the condition or the body spans multiple lines. clang-format does not add
  or remove braces, so this is your call.

### Comments

- Line comments are `// text`, with a space after the slashes.
- Public interface documentation uses Doxygen `/*! ... */` blocks with `@brief`, `@param` and `@retval`.
- clang-format does not re-wrap comments. Wrap long comments yourself at 140 columns.

## Generated code

RTGen (`shared/tools/RTGen`) generates code from interface headers listed with `rtgen(...)` in
each library's `src/CMakeLists.txt`:
- Most `*_ptr.h` smart-pointer headers are generated into the build directory at build time and are
  not committed. Extend one with a `<name>_ptr.custom.h` fragment next to the interface header.
- The C and Python bindings under `bindings/` are generated and committed.

Change the interface header or the generator, not the generated output.

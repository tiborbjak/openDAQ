# AGENTS.md

## Formatting

C++ layout comes from `.clang-format` (pinned to clang-format 21.1.8) and the hooks in
`.pre-commit-config.yaml` (`pre-commit install` or `prek install`).

- Format only the lines you change; never reformat whole existing files. Without hooks, run
  `git clang-format --binary <clang-format-21> HEAD` before committing
  (`pip install clang-format==21.1.8` gets the pinned binary).
- Files indented with tabs (e.g. `coretypes/delegate.hpp`) stay tab-indented; the hook handles this.
  By hand, add `--style "{BasedOnStyle: InheritParentConfig, UseTab: AlignWithSpaces}"`.
- Never format `external/`, `bindings/c/`, `bindings/python/**/generated/` or `shared/tools/RTGen/`.
- Builder chains are joined onto one line when they fit in 140 columns; accept it.

## Conventions

Follow these in new code; do not retrofit code you are not otherwise changing.

**Files and includes**
- New `.h` files: license header from `shared/tools/license-header-checker/license.in`, then
  `#pragma once` (no `#ifndef` guards). Exception: `*.custom.h` fragments get no `#pragma once`.
- `#include <library/header.h>` with angle brackets, even within the same library; quotes only for
  test-local headers. In a `.cpp`, include its own header first. Do not re-sort existing includes.
- Library code goes inside `BEGIN_NAMESPACE_OPENDAQ` / `END_NAMESPACE_OPENDAQ`.

**Naming**
- PascalCase: types and enum values. camelCase: functions, variables and members (no `m_` or
  trailing `_`; use `this->` when a parameter shadows a member). Exception: static serialization
  hooks are PascalCase (`SerializeId()`, `Deserialize(...)`).
- Interfaces are `IName`, implementations `NameImpl`, smart pointers `NamePtr`.
- Class- and namespace-scope constants are `UPPER_SNAKE`.
- Tests: `TEST_F(SubjectTest, PascalCaseExpectedBehaviour)`.

**Interface implementations**
- Declare as `ErrCode INTERFACE_FUNC name(...) override;`, return `OPENDAQ_SUCCESS`, and never let
  exceptions escape.
- Check out-parameters first with `OPENDAQ_PARAM_NOT_NULL`. Return objects with
  `addRefAndReturn()` or `detach()`.
- Errors: `return DAQ_MAKE_ERROR_INFO(OPENDAQ_ERR_..., fmt::format(...))`, propagate with
  `OPENDAQ_RETURN_IF_FAILED`, wrap throwing code in `daqTry([&] { ... })`. In smart-pointer code,
  throw with `DAQ_THROW_EXCEPTION`.

**C++ style**
- `override` (not with `virtual`), `auto`, `fmt::format` (not `std::format`). Pass smart pointers
  as `const XxxPtr&`.
- Omit braces on single-line `if`/`for`/`while` bodies; use braces when the condition or the
  body spans multiple lines.
- Interface docs use Doxygen `/*! @brief ... */`. Wrap long comments
  yourself at 140 columns.

## Generated code

RTGen generates `*_ptr.h` headers (at build time, not committed) and the bindings in `bindings/`
from interface headers listed with `rtgen(...)` in `src/CMakeLists.txt`. Edit the interface header,
a `<name>_ptr.custom.h` fragment, or the generator, never the generated output.

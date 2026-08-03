# AGENTS.md

Guidelines for AI agents working on the ListTalk repository.

## Project Overview

ListTalk is an experimental Lisp-family language and virtual machine written in
C11. It provides a Scheme-like S-expression syntax with Smalltalk-style message
sends, packages, classes, methods, macros, dynamic variables, conditions/restarts,
modules, and a C embedding API.

The repository builds two artifacts:

- `listtalk` — an interactive REPL and script runner.
- `libListTalkVM` — the VM/runtime library, used by the executable and by native
  modules.

## Build

The build system is **Meson** with a **Ninja** backend. All build commands must
be run from the repository root.

```sh
# First-time setup (or after changing meson.build)
meson setup build

# Compile
meson compile -C build
```

External dependencies: `bdw-gc` (Boehm GC, required), `libedit` (optional,
enables REPL line editing). On most Linux systems these are available from the
package manager; on macOS use Homebrew. `python3` is also required at build time.

## Running Tests

```sh
meson test -C build
```

The test suite covers:

| Test | What it exercises |
| --- | --- |
| `reader_test` | S-expression reader |
| `c_api_test` | Public C embedding API |
| `eval_test` | Full language evaluation via `.lt` scripts in `tests/` |
| `listtalk_cli_test` | Command-line flag and REPL behaviour (Python harness) |
| `throw_catch_test` | Non-local exits |
| `conditions_test` | Condition/restart system |
| `identity_dictionary_test`, `dictionary_test` | Dictionary classes |
| `cmdopts_test` | Command-option parsing |
| `ini_test` | INI parser |
| `uuid_test` | UUID generation |
| `lock_test`, `synchronization_classes_test`, `message_queue_test` | Concurrency primitives |

Always run `meson test -C build` after making changes to confirm nothing is
broken. If a test binary needs to be rebuilt, `meson compile -C build` first.

## Repository Layout

```
src/
  vm/           Evaluator, compiler, loader, environments, base forms, VM services
  classes/      Native class implementations (one file per class)
  modules/      Native loadable modules (os, gc, cmdopts, json, ini)
  bin/listtalk/ Command-line executable and REPL
  utils/        Internal utilities (base64, hex, INI, crypto, locks, UTF-8)
runtime/
  init.lt       ListTalk-language runtime definitions, embedded into the VM at build time
modules/
  html-gen.lt   Source module installed with the runtime
ListTalk/       Public C headers
tests/          C unit tests, CLI tests (Python), and ListTalk eval scripts
design/         Design notes for syntax and VM internals
tools/          Build-time helper scripts
```

## Coding Conventions

- The implementation language is **C11**. All source files must compile cleanly
  under `-D_POSIX_C_SOURCE=200809L`.
- Native class implementations live one-per-file under `src/classes/`. Naming
  follows `ClassName.c` / `ClassName.h`.
- Native loadable modules go under `src/modules/` and are built as shared
  libraries with the `.ltm` extension. The module filename is the module name
  that ListTalk code uses with `(load! :name)` or `(require :name)`.
- New classes, modules, or test executables require corresponding entries in
  `meson.build`.
- The runtime init script (`runtime/init.lt`) is embedded as a C byte array
  during the build — changes to it require recompiling but no manual embedding
  step.

## Adding Features

### New native class
1. Add `src/classes/MyClass.c` (and `MyClass.h` in `ListTalk/` if public).
2. Register the class in the init sequence (see `src/vm/init.c`).
3. Add the source file to the `vm_lib` sources in `meson.build`.
4. If the header is public, add it to `ListTalk/` and it will be installed automatically.

### New native module
1. Add `src/modules/mymodule.c`.
2. Add a `shared_module(...)` block in `meson.build`, following the pattern of
   the existing `os_module`, `gc_module`, etc.
3. Use `(load! :mymodule)` or `(require :mymodule)` from ListTalk code.

### New tests
- **C tests**: add a `_test.c` file to `tests/`, create an `executable(...)` and
  a `test(...)` entry in `meson.build`.
- **ListTalk eval tests**: add forms to an existing `.lt` script in `tests/` or
  create a new one and hook it into the `eval_test` test target in `meson.build`.
- **CLI tests**: extend `tests/listtalk_cli_test.py`.

## Common Pitfalls

- The build directory (`build/`) is not committed. When exploring or modifying
  `meson.build`, re-run `meson setup build` if the build directory is stale or
  missing.
- Native modules are dynamically loaded at runtime; the `eval_test` and
  `c_api_test` targets declare `depends:` on the relevant module targets so
  Meson rebuilds them when needed.
- `libedit` is optional. Avoid adding unconditional references to REPL/readline
  APIs without guarding them with `LT_HAVE_LIBEDIT`.

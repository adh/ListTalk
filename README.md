# ListTalk

ListTalk is an experimental Lisp-family language and virtual machine written in
C. The surface syntax is mostly Scheme-like S-expressions with a Smalltalk-style
message-send shorthand, packages, classes, methods, macros, dynamic variables,
conditions/restarts, modules, and a C embedding API.

The repository builds both:

- `listtalk`, an interactive REPL and script runner.
- `libListTalkVM`, the VM/runtime library used by the executable and native
  modules.

## Status

ListTalk is under active development. The implementation is usable for
experimentation, tests, and embedding work, but the language and C API should be
treated as unstable.

## Requirements

- A C11 compiler
- [Meson](https://mesonbuild.com/) and Ninja
- `python3`
- Boehm-Demers-Weiser GC (`bdw-gc`)
- `libedit` or compatible readline headers, optional

When `libedit` is available at configure time, the REPL is built with editable
line input, history, and symbol/package completion. Without it, the REPL falls
back to standard line input.

## Build

```sh
meson setup build
meson compile -C build
```

Run the test suite:

```sh
meson test -C build
```

Install:

```sh
meson install -C build
```

The install target installs the executable, `libListTalkVM`, headers under
`ListTalk/`, pkg-config metadata, native modules, and source modules.

## Running ListTalk

Start the REPL:

```sh
./build/listtalk
```

Evaluate code without printing the result:

```sh
./build/listtalk -e '(define answer 42)'
```

Evaluate code and print the result:

```sh
./build/listtalk -E '(+ 1 2)'
```

Load a source file before continuing:

```sh
./build/listtalk -l path/to/file.lt
```

Run a script:

```sh
./build/listtalk path/to/script.lt
```

Pass script arguments:

```sh
./build/listtalk path/to/script.lt arg1 arg2
```

In script mode, `ListTalk:command-line` is bound to a list of strings containing
the script path followed by the script arguments.

## Command-Line Options

Options are processed in order before the optional script path:

| Option | Description |
| --- | --- |
| `-e CODE`, `--eval CODE` | Evaluate one or more forms without printing their results. |
| `-E CODE`, `--eval-print CODE` | Evaluate one or more forms and print each result. |
| `-l PATH`, `--load PATH` | Load a source file into the current environment. |
| `-r MODULE`, `--require MODULE` | Require a module unless it is already provided. A leading `:` is accepted. |
| `-L PATH`, `--load-path PATH` | Prepend a module resolver path. |
| `--no-std-lib` | Do not add the standard installed source/native module paths. Must appear before environment-modifying options. |
| `--` | Stop option parsing; remaining entries are positional arguments. |

If no script path is provided and at least one action option was used, ListTalk
exits after processing those actions. If no script path and no action option are
provided, it starts the REPL.

## Language Overview

ListTalk reads Scheme/CL-style S-expressions:

```scheme
(define (square x)
  (* x x))

(square 12)
```

Names are package-qualified with `Package:symbol`. Keywords are symbols in the
keyword package and start with `:`.

```scheme
:ready
ListTalk:command-line
```

ListTalk also supports bracket syntax for message sends:

```scheme
[object selector]
[object selector: argument anotherSelector: value]
```

These forms expand to `send` calls:

```scheme
(send object :selector)
(send object :selector:anotherSelector: argument value)
```

Slot shorthand is available in method bodies. A token beginning with `.` reads
or writes a slot on `ListTalk:self`:

```scheme
.name
(set! .name "new value")
```

## Objects And Methods

Classes and methods are defined in ListTalk code:

```scheme
(define-class Point (Object) (x y))

(define-constructor [Point newX: x y: y]
  (set! .x x)
  (set! .y y)
  self)

(define-method [Point x]
  .x)

(define-method [Point y]
  .y)

(define-method [Point magnitude]
  (sqrt (+ (* .x .x) (* .y .y))))
```

The core runtime includes native classes for objects, booleans, nil, symbols,
strings, lists, vectors, bytevectors, numbers, dictionaries, sets, functions,
macros, packages, streams, weak references, conditions, restarts, and date/time
values.

Numeric support includes small integers, big integers, exact fractions, floats,
and exact or inexact complex numbers:

```scheme
(+ 1/2 1/3)
(* 1+2i 3+4i)
(sqrt -4)
```

## Control Flow And Conditions

The runtime provides ordinary Lisp forms and macros such as `lambda`, `let`,
`let*`, `define`, `set!`, `begin`, `if`, `when`, `unless`, `while`, `catch`,
`throw`, and `unwind-protect`.

Conditions and restarts are available through `handler-bind`, `restart-bind`,
`current-restarts`, `find-restart`, and `invoke-restart`.

```scheme
(catch :done
  (throw :done 42))

(restart-bind ((:use-value value)
               "Return a replacement value."
               value)
  (invoke-restart :use-value 10))
```

## Modules

Modules can be loaded or required from ListTalk code:

```scheme
(load! :os)
(require :cmdopts)
```

`provide` records that a module has been loaded:

```scheme
(provide :my-module)
```

The build currently provides native modules for:

- `os`
- `gc`
- `cmdopts`

It also installs the source module:

- `html-gen`

Additional module search paths can be supplied with `-L` or by passing resolver
paths to `load!`/`require`.

## Embedding

Installed consumers can use `pkg-config`:

```sh
pkg-config --cflags --libs ListTalkVM
```

The umbrella header is:

```c
#include <ListTalk/ListTalk.h>
```

Initialize the runtime before using VM APIs:

```c
LT_INIT();
```

Useful public entry points include `LT_eval`, `LT_eval_sequence`,
`LT_eval_sequence_string`, `LT_apply`, `LT_send`, and the `LT_SEND`/`LT_APPLY`
helper macros. Public class/value headers are installed under `ListTalk/`.

## Repository Layout

- `src/vm/` - evaluator, compiler, loader, environments, base forms, and VM
  services.
- `src/classes/` - native class implementations.
- `src/modules/` - native loadable modules.
- `src/bin/listtalk/` - command-line executable and REPL.
- `runtime/init.lt` - ListTalk runtime definitions embedded into the VM.
- `modules/` - source modules installed with the runtime.
- `ListTalk/` - public C headers.
- `tests/` - C, CLI, and ListTalk language tests.
- `design/` - design notes for syntax and VM internals.

## License

MIT. See [LICENSE](./LICENSE).

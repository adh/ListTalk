# ListTalk

A small Lisp-like runtime and REPL written in C.

## Build

```sh
meson setup build
meson compile -C build
```

If `libedit` is available when configuring the build, the REPL links against it
for editable line input and history support. Builds without `libedit` keep using
standard line input.

## Run

```sh
./build/listtalk
```

Run source files:

```sh
./build/listtalk path/to/file.lt
```

Pass script arguments:

```sh
./build/listtalk path/to/file.lt arg1 arg2
```

In script mode, `*command-line*` is bound to a list of strings containing
all command-line entries from `argv[1]` onward (script path included).

## Test

```sh
meson test -C build
```

## License

MIT. See [LICENSE](./LICENSE).

# ListTalk

A small Lisp-like runtime and REPL written in C.

## Build

```sh
meson setup build
meson compile -C build
```

## Run

```sh
./build/listtalk
```

Run source files:

```sh
./build/listtalk path/to/file.lt
```

## Test

```sh
meson test -C build
```

## License

MIT. See [LICENSE](./LICENSE).

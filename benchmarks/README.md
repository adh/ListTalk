# ListTalk Benchmarks

Run from the repository root after building:

```sh
./build/listtalk -l benchmarks/common.lt benchmarks/gabriel.lt
```

The benchmarks time themselves with `[Instant now]` and print elapsed
microseconds. They are intentionally outside the Meson test suite.

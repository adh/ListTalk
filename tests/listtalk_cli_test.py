#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2023 - 2026 Ales Hakl

import subprocess
import sys


def run_case(exe, args, expected_stdout):
    completed = subprocess.run(
        [exe] + args,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if completed.returncode != 0:
        sys.stderr.write(
            "FAIL: command exited with {0}: {1}\n{2}".format(
                completed.returncode,
                " ".join(args),
                completed.stderr,
            )
        )
        return 1
    if completed.stdout != expected_stdout:
        sys.stderr.write(
            "FAIL: stdout mismatch for {0}\nexpected: {1!r}\nactual:   {2!r}\n".format(
                " ".join(args),
                expected_stdout,
                completed.stdout,
            )
        )
        return 1
    return 0


def run_failure_case(exe, args):
    completed = subprocess.run(
        [exe] + args,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if completed.returncode == 0:
        sys.stderr.write(
            "FAIL: command unexpectedly succeeded: {0}\nstdout: {1}\n".format(
                " ".join(args),
                completed.stdout,
            )
        )
        return 1
    return 0


def main():
    exe, build_dir, fixture_dir = sys.argv[1:4]
    failures = 0

    failures += run_case(exe, ["-E", "(+ 1 2)"], "3\n")
    failures += run_case(
        exe,
        ["-e", "(define cli-side-effect 41)", "-E", "(+ cli-side-effect 1)"],
        "42\n",
    )
    failures += run_case(
        exe,
        ["-l", fixture_dir + "/test-module-foo.lt", "-E", "loaded-from-foo"],
        "42\n",
    )
    failures += run_case(
        exe,
        ["-L", fixture_dir, "-r", "test-module-foo", "-E", "loaded-from-foo"],
        "42\n",
    )
    failures += run_case(
        exe,
        [
            "-L",
            build_dir,
            "-E",
            '(begin (require :json) [[ListTalk-JSON:JSONDecoder newWithArrayConversion: :vector objectConversion: :plist] decode: "{\\"a\\":[1,2],\\"b\\":false}"])',
        ],
        '("a" #(1 2) "b" #false)\n',
    )
    failures += run_case(
        exe,
        [
            "-L",
            build_dir,
            "-E",
            '(begin (require :json) [[ListTalk-JSON:JSONEncoder new] encode: (list 1 "x" #true #nil)])',
        ],
        '"[1,\\"x\\",true,null]"\n',
    )
    failures += run_case(
        exe,
        [
            "-L",
            build_dir,
            "-E",
            '(begin (require :json) (define e [ListTalk-JSON:JSONEncoder new]) [e output: :byte-vector] [e encode: (list "ok")])',
        ],
        '#"[\\"ok\\"]"\n',
    )
    failures += run_case(
        exe,
        [fixture_dir + "/command-line.lt", "arg1", "arg2"],
        '("{0}/command-line.lt" "arg1" "arg2")\n'.format(fixture_dir),
    )
    failures += run_failure_case(
        exe,
        ["--no-std-lib", "-r", "test-module-foo", "-L", fixture_dir],
    )

    if failures:
        return 1
    print("listtalk cli tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())

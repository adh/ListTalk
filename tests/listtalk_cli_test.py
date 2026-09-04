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


def run_interactive_syntax_error_case(exe):
    completed = subprocess.run(
        [exe],
        input=")\n(+ 4 5)\n",
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if completed.returncode != 0:
        sys.stderr.write(
            "FAIL: interactive syntax recovery exited with {0}\n{1}".format(
                completed.returncode,
                completed.stderr,
            )
        )
        return 1
    if "Debugger entered on:" in completed.stdout or "debug> " in completed.stdout:
        sys.stderr.write(
            "FAIL: syntax error unexpectedly entered debugger\n{0}".format(
                completed.stdout
            )
        )
        return 1
    if "Error: #<ReaderError" not in completed.stderr:
        sys.stderr.write(
            "FAIL: syntax error was not reported by the REPL\n{0}".format(
                completed.stderr
            )
        )
        return 1
    if not completed.stdout.rstrip().endswith("9"):
        sys.stderr.write(
            "FAIL: top-level REPL did not recover from syntax error\n{0}".format(
                completed.stdout
            )
        )
        return 1
    return 0


def main():
    exe, build_dir, fixture_dir = sys.argv[1:4]
    failures = 0

    failures += run_case(exe, ["-E", "(+ 1 2)"], "3\n")
    failures += run_case(exe, ["-d", "-E", "(+ 2 3)"], "5\n")
    failures += run_case(exe, ["--debug", "-E", "(+ 3 4)"], "7\n")
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
        [fixture_dir + "/command-line.lt", "arg1", "arg2"],
        '("{0}/command-line.lt" "arg1" "arg2")\n'.format(fixture_dir),
    )
    failures += run_failure_case(
        exe,
        ["--no-std-lib", "-r", "test-module-foo", "-L", fixture_dir],
    )
    failures += run_interactive_syntax_error_case(exe)

    if failures:
        return 1
    print("listtalk cli tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())

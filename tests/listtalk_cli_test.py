#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2023 - 2026 Ales Hakl

import os
import pty
import select
import subprocess
import sys
import time


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
    if "Condition: #<ReaderError" in completed.stdout:
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


def run_debugger_restart_case(exe, input_text, expected_result, description):
    completed = subprocess.run(
        [exe],
        input=input_text,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if completed.returncode != 0:
        sys.stderr.write(
            "FAIL: {0} exited with {1}\n{2}".format(
                description,
                completed.returncode,
                completed.stderr,
            )
        )
        return 1
    if not completed.stdout.rstrip().endswith(expected_result):
        sys.stderr.write(
            "FAIL: {0} did not produce {1!r}\n{2}".format(
                description,
                expected_result,
                completed.stdout,
            )
        )
        return 1
    return 0


def run_debugger_banner_case(exe, input_text, expected_banners, description):
    completed = subprocess.run(
        [exe],
        input=input_text,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    banner_count = completed.stdout.count("Debugger entered on:")
    if completed.returncode != 0 or banner_count != expected_banners:
        sys.stderr.write(
            "FAIL: {0}: expected {1} banners, got {2}\n{3}{4}".format(
                description,
                expected_banners,
                banner_count,
                completed.stdout,
                completed.stderr,
            )
        )
        return 1
    return 0


def read_pty_until(fd, expected, timeout=5):
    output = bytearray()
    deadline = time.monotonic() + timeout
    while expected not in output and time.monotonic() < deadline:
        readable, _, _ = select.select([fd], [], [], deadline - time.monotonic())
        if not readable:
            break
        try:
            output.extend(os.read(fd, 4096))
        except OSError:
            break
    return bytes(output)


def drain_pty(fd, timeout=1):
    output = bytearray()
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        readable, _, _ = select.select([fd], [], [], deadline - time.monotonic())
        if not readable:
            break
        try:
            output.extend(os.read(fd, 4096))
        except OSError:
            break
    return bytes(output)


def run_debugger_prompt_depth_case(exe):
    master, slave = pty.openpty()
    process = subprocess.Popen(
        [exe],
        stdin=slave,
        stdout=slave,
        stderr=slave,
        close_fds=True,
    )
    os.close(slave)
    transcript = bytearray()

    try:
        transcript.extend(read_pty_until(master, b"listtalk> "))
        os.write(master, b"missing-name\r")
        transcript.extend(read_pty_until(master, b"debug[1]> "))
        os.write(master, b'(error "nested")\r')
        transcript.extend(read_pty_until(master, b"debug[2]> "))
        os.write(master, b":return-to-debugger\r")
        transcript.extend(read_pty_until(master, b"debug[1]> "))
        os.write(master, b"(:use-value 1)\r")
        transcript.extend(read_pty_until(master, b"listtalk> "))
        os.write(master, b"\x04")
        transcript.extend(drain_pty(master))
        process.wait(timeout=5)
    except (OSError, subprocess.TimeoutExpired):
        process.kill()
        process.wait()
    finally:
        os.close(master)

    required = (b"debug[1]> ", b"debug[2]> ")
    if any(item not in transcript for item in required):
        sys.stderr.write(
            "FAIL: nested debugger prompts\n{0}".format(
                transcript.decode(errors="replace")
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
    failures += run_debugger_restart_case(
        exe,
        "missing-name\n(:use-value (+ 20 21))\n",
        "41",
        "debugger restart shorthand",
    )
    failures += run_debugger_restart_case(
        exe,
        "missing-name\n:use-value\n(+ 20 21)\n",
        "(+ 20 21)",
        "interactive debugger restart arguments",
    )
    failures += run_debugger_restart_case(
        exe,
        "missing-name\n1\n(+ 20 21)\n",
        "(+ 20 21)",
        "numbered debugger restart arguments",
    )
    failures += run_debugger_banner_case(
        exe,
        "missing-name\n:show\n(:use-value 1)\n",
        2,
        "debugger :show command",
    )
    failures += run_debugger_banner_case(
        exe,
        'missing-name\n(error "nested")\n:return-to-debugger\n(:use-value 1)\n',
        3,
        "parent debugger banner redisplay",
    )
    failures += run_debugger_prompt_depth_case(exe)

    if failures:
        return 1
    print("listtalk cli tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())

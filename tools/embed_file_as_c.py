#!/usr/bin/env python3
"""Generate a C source file that embeds a file as byte array data.

Example:
    ./tools/embed_file_as_c.py path/to/source.lt source_text -o /tmp/source_text.c
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys


C_IDENTIFIER_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Convert an input file into a C source fragment defining "
            "unsigned char <symbol>[] and size_t <symbol>_length."
        )
    )
    parser.add_argument("input_file", help="Path to file to embed")
    parser.add_argument("symbol_name", help="C symbol name for the generated array")
    parser.add_argument(
        "-o",
        "--output",
        default="-",
        help="Output path for generated C source (default: stdout)",
    )
    return parser.parse_args()


def validate_symbol_name(symbol_name: str) -> None:
    if not C_IDENTIFIER_RE.match(symbol_name):
        raise ValueError(
            f"Invalid symbol name '{symbol_name}'. Must be a valid C identifier."
        )



def format_bytes(data: bytes, bytes_per_line: int = 12) -> str:
    if not data:
        return ""

    lines: list[str] = []
    for idx in range(0, len(data), bytes_per_line):
        chunk = data[idx : idx + bytes_per_line]
        lines.append("    " + ", ".join(f"0x{b:02x}" for b in chunk))
    return ",\n".join(lines)



def generate_c_source(symbol_name: str, data: bytes) -> str:
    body = format_bytes(data)
    if body:
        array_block = f"{body}\n"
    else:
        array_block = ""

    # For empty input, an empty initializer is valid in C and keeps length == 0.
    return (
        "#include <stddef.h>\n\n"
        f"unsigned char {symbol_name}[] = {{\n"
        f"{array_block}"
        "};\n\n"
        f"size_t {symbol_name}_length = sizeof({symbol_name});\n"
    )



def write_output(text: str, output: str) -> None:
    if output == "-":
        sys.stdout.write(text)
        return

    output_path = pathlib.Path(output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(text, encoding="utf-8")



def main() -> int:
    args = parse_args()

    try:
        validate_symbol_name(args.symbol_name)
    except ValueError as exc:
        print(exc, file=sys.stderr)
        return 2

    input_path = pathlib.Path(args.input_file)
    try:
        data = input_path.read_bytes()
    except OSError as exc:
        print(f"Failed to read '{input_path}': {exc}", file=sys.stderr)
        return 1

    source = generate_c_source(args.symbol_name, data)

    try:
        write_output(source, args.output)
    except OSError as exc:
        print(f"Failed to write output '{args.output}': {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

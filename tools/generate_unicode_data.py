#!/usr/bin/env python3
"""Generate ListTalk's compact Unicode character-property tables."""

import argparse
from pathlib import Path


CODEPOINT_COUNT = 0x110000
BLOCK_SIZE = 256


def parse_unicode_data(path):
    properties = [("Cn", 0, 0, 0)] * CODEPOINT_COUNT
    range_start = None

    with path.open(encoding="ascii") as source:
        for line_number, line in enumerate(source, 1):
            fields = line.rstrip("\n").split(";")
            if len(fields) != 15:
                raise ValueError(f"{path}:{line_number}: expected 15 fields")
            codepoint = int(fields[0], 16)
            name = fields[1]
            entry = (
                fields[2],
                int(fields[12], 16) - codepoint if fields[12] else 0,
                int(fields[13], 16) - codepoint if fields[13] else 0,
                int(fields[14], 16) - codepoint if fields[14] else 0,
            )

            if name.endswith("First>"):
                if range_start is not None:
                    raise ValueError(f"{path}:{line_number}: nested range")
                range_start = (codepoint, entry)
            elif name.endswith("Last>"):
                if range_start is None:
                    raise ValueError(f"{path}:{line_number}: range end without start")
                start, start_entry = range_start
                properties[start : codepoint + 1] = [start_entry] * (
                    codepoint - start + 1
                )
                range_start = None
            else:
                properties[codepoint] = entry

    if range_start is not None:
        raise ValueError(f"{path}: unterminated range")
    return properties


def parse_case_folding(path):
    mappings = {}
    with path.open(encoding="utf-8") as source:
        for line_number, line in enumerate(source, 1):
            content = line.split("#", 1)[0].strip()
            if not content:
                continue
            fields = [field.strip() for field in content.split(";")]
            if len(fields) < 3:
                raise ValueError(f"{path}:{line_number}: expected at least 3 fields")
            codepoint = int(fields[0], 16)
            status = fields[1]
            if status in ("C", "F"):
                mappings[codepoint] = tuple(
                    int(value, 16) for value in fields[2].split()
                )
    return sorted(mappings.items())


def parse_special_casing(path):
    mappings = []
    with path.open(encoding="utf-8") as source:
        for line_number, line in enumerate(source, 1):
            content = line.split("#", 1)[0].strip()
            if not content:
                continue
            fields = [field.strip() for field in content.split(";")]
            if len(fields) < 5:
                raise ValueError(f"{path}:{line_number}: expected at least 5 fields")
            if fields[4]:
                continue
            mappings.append(
                (
                    int(fields[0], 16),
                    tuple(int(value, 16) for value in fields[1].split()),
                    tuple(int(value, 16) for value in fields[2].split()),
                    tuple(int(value, 16) for value in fields[3].split()),
                )
            )
    return sorted(mappings)


def unique_with_indices(items):
    unique = []
    indices = {}
    result = []
    for item in items:
        if item not in indices:
            indices[item] = len(unique)
            unique.append(item)
        result.append(indices[item])
    return unique, result


def format_array(values, width=16):
    lines = []
    for offset in range(0, len(values), width):
        chunk = values[offset : offset + width]
        lines.append("    " + ", ".join(str(v) for v in chunk) + ",")
    return "\n".join(lines)


def generate(properties, case_folding, special_casing):
    distinct_properties, property_indices = unique_with_indices(properties)
    raw_blocks = [
        bytes(property_indices[offset : offset + BLOCK_SIZE])
        for offset in range(0, CODEPOINT_COUNT, BLOCK_SIZE)
    ]
    blocks, block_indices = unique_with_indices(raw_blocks)
    if len(distinct_properties) > 256 or len(blocks) > 256:
        raise ValueError("Unicode tables no longer fit in eight-bit indices")

    property_rows = [
        f'    {{"{category}", {upper}, {lower}, {title}}},'
        for category, upper, lower, title in distinct_properties
    ]
    block_rows = [
        "    {\n" + format_array(block) + "\n    }," for block in blocks
    ]
    fold_values = []
    fold_rows = []
    for codepoint, mapping in case_folding:
        if len(fold_values) > 0xffff or len(mapping) > 0xff:
            raise ValueError("Case-fold tables no longer fit compact offsets")
        fold_rows.append(
            f"    {{UINT32_C(0x{codepoint:x}), {len(fold_values)}, {len(mapping)}}},"
        )
        fold_values.extend(mapping)
    special_values = []
    special_rows = []
    for codepoint, lower, title, upper in special_casing:
        offsets = []
        lengths = []
        for mapping in (lower, title, upper):
            offsets.append(len(special_values))
            lengths.append(len(mapping))
            special_values.extend(mapping)
        if len(special_values) > 0xffff or max(lengths) > 0xff:
            raise ValueError("Special-casing tables no longer fit compact offsets")
        special_rows.append(
            "    {UINT32_C(0x%x), {%s}, {%s}},"
            % (
                codepoint,
                ", ".join(str(value) for value in offsets),
                ", ".join(str(value) for value in lengths),
            )
        )
    return f"""/* Generated from Unicode 17.0.0 UnicodeData.txt, CaseFolding.txt,
 * and SpecialCasing.txt.
 * Generated by tools/generate_unicode_data.py. Do not edit.
 */
#include \"src/utils/unicode_data.h\"

#include <stdint.h>

typedef struct {{
    char category[3];
    int32_t upper_offset;
    int32_t lower_offset;
    int32_t title_offset;
}} LT_UnicodeProperty;

typedef struct {{
    uint32_t codepoint;
    uint16_t offset;
    uint8_t length;
}} LT_CaseFold;

typedef struct {{
    uint32_t codepoint;
    uint16_t offsets[3];
    uint8_t lengths[3];
}} LT_SpecialCase;

static const LT_UnicodeProperty properties[] = {{
{chr(10).join(property_rows)}
}};

static const uint8_t block_map[] = {{
{format_array(block_indices)}
}};

static const uint8_t blocks[][256] = {{
{chr(10).join(block_rows)}
}};

static const uint32_t case_fold_values[] = {{
{format_array([f'UINT32_C(0x{value:x})' for value in fold_values], 8)}
}};

static const LT_CaseFold case_folds[] = {{
{chr(10).join(fold_rows)}
}};

static const uint32_t special_case_values[] = {{
{format_array([f'UINT32_C(0x{value:x})' for value in special_values], 8)}
}};

static const LT_SpecialCase special_cases[] = {{
{chr(10).join(special_rows)}
}};

static const LT_UnicodeProperty* property_for(uint32_t codepoint) {{
    return &properties[blocks[block_map[codepoint >> 8]][codepoint & 0xff]];
}}

const char* LT_unicode_category(uint32_t codepoint) {{
    return property_for(codepoint)->category;
}}

uint32_t LT_unicode_lowercase(uint32_t codepoint) {{
    return (uint32_t)((int32_t)codepoint + property_for(codepoint)->lower_offset);
}}

uint32_t LT_unicode_uppercase(uint32_t codepoint) {{
    return (uint32_t)((int32_t)codepoint + property_for(codepoint)->upper_offset);
}}

uint32_t LT_unicode_titlecase(uint32_t codepoint) {{
    return (uint32_t)((int32_t)codepoint + property_for(codepoint)->title_offset);
}}

const uint32_t* LT_unicode_casefold(uint32_t codepoint, size_t* length_out) {{
    size_t low = 0;
    size_t high = sizeof(case_folds) / sizeof(case_folds[0]);
    while (low < high) {{
        size_t middle = low + (high - low) / 2;
        if (case_folds[middle].codepoint < codepoint) {{
            low = middle + 1;
        }} else {{
            high = middle;
        }}
    }}
    if (low == sizeof(case_folds) / sizeof(case_folds[0])
            || case_folds[low].codepoint != codepoint) {{
        *length_out = 0;
        return NULL;
    }}
    *length_out = case_folds[low].length;
    return &case_fold_values[case_folds[low].offset];
}}

static const uint32_t* special_case(uint32_t codepoint,
                                    size_t kind,
                                    size_t* length_out) {{
    size_t low = 0;
    size_t high = sizeof(special_cases) / sizeof(special_cases[0]);
    while (low < high) {{
        size_t middle = low + (high - low) / 2;
        if (special_cases[middle].codepoint < codepoint) {{
            low = middle + 1;
        }} else {{
            high = middle;
        }}
    }}
    if (low == sizeof(special_cases) / sizeof(special_cases[0])
            || special_cases[low].codepoint != codepoint) {{
        *length_out = 0;
        return NULL;
    }}
    *length_out = special_cases[low].lengths[kind];
    return &special_case_values[special_cases[low].offsets[kind]];
}}

const uint32_t* LT_unicode_full_lowercase(uint32_t codepoint,
                                          size_t* length_out) {{
    return special_case(codepoint, 0, length_out);
}}

const uint32_t* LT_unicode_full_titlecase(uint32_t codepoint,
                                          size_t* length_out) {{
    return special_case(codepoint, 1, length_out);
}}

const uint32_t* LT_unicode_full_uppercase(uint32_t codepoint,
                                          size_t* length_out) {{
    return special_case(codepoint, 2, length_out);
}}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("case_folding", type=Path)
    parser.add_argument("special_casing", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.write_text(
        generate(
            parse_unicode_data(args.input),
            parse_case_folding(args.case_folding),
            parse_special_casing(args.special_casing),
        ),
        encoding="ascii",
    )


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Read exported_runtime_functions.json and emit a CMake include file."""
import json
import sys

def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input.json> <output.cmake>", file=sys.stderr)
        sys.exit(1)

    input_path = sys.argv[1]
    output_path = sys.argv[2]

    with open(input_path, 'r', encoding='utf-8') as f:
        data = json.load(f)

    def fmt_list(key):
        values = data.get(key, [])
        return ','.join(f"'{v}'" for v in values)

    lines = [
        f'set(WASM_EXPORT_FUNCTIONS "{fmt_list("EXPORTED_FUNCTIONS")}")',
        f'set(WASM_EXPORT_RUNTIME "{fmt_list("EXPORTED_RUNTIME_METHODS")}")',
        f'set(WASM_ASYNCIFY_IMPORTS "{fmt_list("ASYNCIFY_IMPORTS")}")',
        f'set(WASM_ASYNCIFY_STACK_SIZE "{data.get("ASYNCIFY_STACK_SIZE", 65536)}")',
        f'set(WASM_EXPORT_NAME "{data.get("EXPORT_NAME", "Module")}")',
    ]

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines) + '\n')

if __name__ == '__main__':
    main()

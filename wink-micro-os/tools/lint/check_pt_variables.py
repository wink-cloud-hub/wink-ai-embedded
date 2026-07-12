#!/usr/bin/env python3
"""Static analyzer to catch auto variables inside protothreads.

This is the #1 footgun in protothread programming - catch it early at build time!

Detects:
1. Functions that take wink_pt_t* as first argument
2. Contain WINK_PT_YIELD, WINK_PT_DELAY_MS, or WINK_PT_WAIT_*
3. Declare non-static automatic variables inside

False positive suppression:
- Loop counters (i, j, k) declared immediately before for(...)
  BUT ONLY IF the for loop body contains NO yield points
- Variables declared before WINK_PT_BEGIN (executed on every call)

Additional multi-instance safety:
- Warns about static local variables inside protothreads (not reentrant-safe)
"""
import re
import sys
import os


def _find_matching_brace(content, start_pos):
    """Find matching closing brace, counting nested braces.

    Args:
        content: Source code string
        start_pos: Position of opening '{' or '('

    Returns:
        Position of matching closing brace, or -1 if not found
    """
    if start_pos >= len(content):
        return -1

    opening = content[start_pos]
    if opening == '{':
        closing = '}'
    elif opening == '(':
        closing = ')'
    else:
        return -1

    brace_count = 1
    pos = start_pos + 1
    while pos < len(content) and brace_count > 0:
        if content[pos] == opening:
            brace_count += 1
        elif content[pos] == closing:
            brace_count -= 1
        pos += 1

    return pos - 1 if brace_count == 0 else -1


def _has_yield_in_range(content, start, end):
    """Check if yield macros exist in the specified range.

    Args:
        content: Source code string
        start: Start position (inclusive)
        end: End position (exclusive)

    Returns:
        True if any yield macro is found
    """
    if start < 0 or end > len(content) or start >= end:
        return False

    body = content[start:end]
    return (
        'WINK_PT_YIELD' in body or
        'WINK_PT_DELAY_MS' in body or
        'WINK_PT_WAIT_' in body
    )


def _strip_c_comments(text):
    """Remove C-style comments from text for more reliable pattern matching.

    Preserves original string length by replacing comments with spaces
    so position calculations remain valid.
    """
    result = list(text)
    i = 0
    in_single_comment = False
    in_multi_comment = False

    while i < len(result) - 1:
        if not in_multi_comment and result[i] == '/' and result[i+1] == '/':
            in_single_comment = True
            result[i] = ' '
            result[i+1] = ' '
            i += 2
        elif not in_single_comment and result[i] == '/' and result[i+1] == '*':
            in_multi_comment = True
            result[i] = ' '
            result[i+1] = ' '
            i += 2
        elif in_multi_comment and result[i] == '*' and result[i+1] == '/':
            in_multi_comment = False
            result[i] = ' '
            result[i+1] = ' '
            i += 2
        elif in_single_comment and result[i] == '\n':
            in_single_comment = False
            i += 1
        elif in_single_comment or in_multi_comment:
            result[i] = ' '
            i += 1
        else:
            i += 1

    return ''.join(result)


def find_all_pt_functions(content):
    """Find all protothread functions and their line ranges."""
    # Match function definition: wink_status_t name(wink_pt_t *pt, ...) {
    func_pattern = re.compile(
        r'^\s*wink_status_t\s+(\w+)\s*\(\s*wink_pt_t\s*\*\s*\w+',
        re.MULTILINE
    )

    functions = []
    for match in func_pattern.finditer(content):
        func_name = match.group(1)
        start_pos = match.end()

        # First find the opening '{' of the function body
        # It should be after the function parameters (the closing ')')
        paren_open = content.find('(', match.start())
        if paren_open == -1:
            continue

        paren_close = _find_matching_brace(content, paren_open)
        if paren_close == -1:
            continue

        # Now find the '{' after the closing ')'
        brace_open = -1
        pos = paren_close + 1
        while pos < len(content):
            if content[pos] == '{':
                brace_open = pos
                break
            elif not content[pos].isspace() and content[pos] != ';':
                # Found non-whitespace before '{' - probably not a function def
                # (e.g. function declaration without body, or macro)
                break
            pos += 1

        if brace_open == -1:
            continue

        # Find matching closing brace
        brace_close = _find_matching_brace(content, brace_open)
        if brace_close == -1:
            continue

        func_body_start = brace_open + 1
        func_body_end = brace_close

        # Check if this function has any yield points
        if func_body_start >= func_body_end:
            continue

        func_body = content[func_body_start:func_body_end]
        has_yield = (
            'WINK_PT_YIELD' in func_body or
            'WINK_PT_DELAY_MS' in func_body or
            'WINK_PT_WAIT_' in func_body
        )

        if has_yield:
            # Find line number
            line_no = content[:match.start()].count('\n') + 1
            functions.append({
                'name': func_name,
                'line': line_no,
                'body_start': func_body_start,
                'body_end': func_body_end,
                'body': func_body
            })

    return functions


def _check_single_variable(content, var_name, var_start, var_end_after_name,
                           var_qual, func_info, pt_begin_offset, errors):
    """Check a single variable (handles multi-declaration like int j, k;).

    Returns True if this variable was handled specially (e.g. safe for loop
    without yield), False if standard auto variable check should apply.
    """
    func_start = func_info['body_start']
    func_end = func_info['body_end']
    line_no = content[:var_start].count('\n') + 1

    # === Check 1: static local variables - multi-instance hazard ===
    if var_qual:
        errors.append({
            'line': line_no,
            'func': func_info['name'],
            'var': var_name,
            'type': 'STATIC_LOCAL',
            'severity': 'MEDIUM',
            'message': (
                f"[MULTI-INSTANCE BUG] static variable '{var_name}' "
                f"in protothread '{func_info['name']}' - "
                f"not safe for multiple coroutine instances. "
                f"Use WINK_PT_STATE_* macros instead for reentrancy safety."
            )
        })
        return True  # Handled

    # === Check 2: Loop counters (i, j, k) with for loop - check yield inside ===
    # Strip comments from the remainder for more reliable matching
    remainder = content[var_end_after_name:var_end_after_name + 200]
    remainder_clean = _strip_c_comments(remainder)

    if var_name in ('i', 'j', 'k'):
        # Find 'for' keyword after the variable declaration
        # Using word boundary to avoid matching 'xfor' or other substrings
        for_match = re.search(r'\bfor\s*\(', remainder_clean)
        if for_match:
            # Found a for loop - now check if the loop body contains yield
            for_start_in_remainder = for_match.start()
            for_absolute_start = var_end_after_name + for_start_in_remainder

            # Find the '(' after 'for'
            paren_open = content.find('(', for_absolute_start)
            if paren_open != -1:
                # Find matching ')' to skip for loop header
                paren_close = _find_matching_brace(content, paren_open)
                if paren_close != -1:
                    # Find the loop body starting '{'
                    body_start = -1
                    pos = paren_close + 1
                    # Scan past whitespace and newlines to find '{'
                    while pos < len(content) and pos < func_end:
                        if content[pos] == '{':
                            body_start = pos
                            break
                        elif content[pos] == ';':
                            # for loop without braces, single statement
                            body_start = -1
                            break
                        elif not content[pos].isspace():
                            # Found non-whitespace before '{' - complex form
                            body_start = -1
                            break
                        pos += 1

                    if body_start != -1:
                        body_end = _find_matching_brace(content, body_start)
                        if body_end != -1:
                            # Check if there's yield in the for loop body
                            if _has_yield_in_range(content, body_start, body_end):
                                # DANGER! for loop body has yield - counter is footgun!
                                errors.append({
                                    'line': line_no,
                                    'func': func_info['name'],
                                    'var': var_name,
                                    'type': 'FOR_LOOP_FOOTGUN',
                                    'severity': 'HIGH',
                                    'message': (
                                        f"[FOOTGUN] Loop counter '{var_name}' "
                                        f"in protothread '{func_info['name']}' - "
                                        f"for loop contains yield! Value will be garbage "
                                        f"after WINK_PT_DELAY_MS/WINK_PT_YIELD. "
                                        f"Use WINK_PT_STATE_* to store persistent state."
                                    )
                                })
                                return True  # Handled - flagged
                            # else: no yield in for body - it's safe, skip this var
                            return True  # Handled - safe

    return False  # Not specially handled - should check as normal auto var


def find_auto_variables(func_info, content):
    """Find suspicious non-static auto variables in a protothread function.

    Also detects static local variables (multi-instance safety hazard).
    Handles multi-variable declarations: int j, k;
    """
    errors = []
    func_start = func_info['body_start']
    func_end = func_info['body_end']

    # Find variable declarations inside the function
    # Pattern: [static] type [*] name [, name2] [;= []
    # First match finds START of declaration (with type prefix)
    # Then we scan forward to find ALL variable names separated by commas
    var_start_pattern = re.compile(
        r'(?P<qual>static\s+)?'
        r'\b(?P<type>'
        r'int|uint8_t|uint16_t|uint32_t|uint64_t|'
        r'int8_t|int16_t|int32_t|int64_t|'
        r'float|double|char|bool|size_t|'
        r'struct\s+\w+|enum\s+\w+|union\s+\w+|'
        r'[A-Za-z_]\w*_t'
        r')\b\s*\*?\s*'
        r'(?P<firstname>[A-Za-z_]\w*)',
        re.MULTILINE
    )

    # Pattern to match subsequent variable names in multi-declaration
    # e.g. the 'k' in 'int j, k;'
    next_var_pattern = re.compile(r'^\s*,?\s*([A-Za-z_]\w*)')

    # Find WINK_PT_BEGIN position in function
    pt_begin_match = re.search(r'WINK_PT_BEGIN', func_info['body'])
    if not pt_begin_match:
        return errors  # No PT_BEGIN - not a real protothread

    pt_begin_offset = func_start + pt_begin_match.end()

    for var_match in var_start_pattern.finditer(content, func_start, func_end):
        var_qual = var_match.group('qual')
        var_type = var_match.group('type')
        first_name = var_match.group('firstname')
        var_start = var_match.start()
        first_var_end = var_match.end()

        # Skip variables BEFORE WINK_PT_BEGIN (always safe)
        if var_start < pt_begin_offset:
            continue

        line_no = content[:var_start].count('\n') + 1

        # === Step 1: Check the first variable ===
        # For first variable, use its end position for for-loop detection
        handled = _check_single_variable(
            content, first_name, var_start, first_var_end,
            var_qual, func_info, pt_begin_offset, errors
        )

        # If not handled and not static, we need to check as normal auto var
        if not handled and not var_qual:
            errors.append({
                'line': line_no,
                'func': func_info['name'],
                'var': first_name,
                'type': 'AUTO_VAR',
                'severity': 'HIGH',
                'message': (
                    f"[FOOTGUN] Non-static auto variable '{first_name}' "
                    f"in protothread '{func_info['name']}' - "
                    f"value will be GARBAGE after yield! "
                    f"Use 'static {first_name}' (single instance only) or "
                    f"WINK_PT_STATE_* macros for persistent state."
                )
            })

        # === Step 2: Check for more variables (j, k, ...) in same declaration ===
        scan_pos = first_var_end
        while scan_pos < func_end:
            remainder = content[scan_pos:scan_pos + 50]
            if ';' in remainder:
                semicolon_pos = remainder.find(';')
                remainder = remainder[:semicolon_pos + 1]

            next_match = next_var_pattern.match(remainder)
            if next_match:
                next_name = next_match.group(1)
                name_end_in_remainder = next_match.end(1)
                next_name_end = scan_pos + name_end_in_remainder

                # Check this variable - for-loop detection uses position after this name
                handled = _check_single_variable(
                    content, next_name, var_start, next_name_end,
                    var_qual, func_info, pt_begin_offset, errors
                )

                if not handled and not var_qual:
                    errors.append({
                        'line': line_no,
                        'func': func_info['name'],
                        'var': next_name,
                        'type': 'AUTO_VAR',
                        'severity': 'HIGH',
                        'message': (
                            f"[FOOTGUN] Non-static auto variable '{next_name}' "
                            f"in protothread '{func_info['name']}' - "
                            f"value will be GARBAGE after yield! "
                            f"Use WINK_PT_STATE_* macros for persistent state."
                        )
                    })

                scan_pos = next_name_end
            else:
                # No more variables in this declaration
                break

    return errors


def check_file(filepath):
    """Check a single C file for protothread footguns."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
    except (OSError, UnicodeDecodeError):
        return []

    all_errors = []
    functions = find_all_pt_functions(content)

    for func in functions:
        errors = find_auto_variables(func, content)
        all_errors.extend(errors)

    return all_errors


def main():
    all_errors = []

    # Check all C files in wink-micro-os
    for root, dirs, files in os.walk('.'):
        for f in files:
            if f.endswith('.c'):
                filepath = os.path.join(root, f)
                all_errors.extend(check_file(filepath))

    if all_errors:
        high_count = sum(1 for e in all_errors if e['severity'] == 'HIGH')
        medium_count = sum(1 for e in all_errors if e['severity'] == 'MEDIUM')

        print("")
        print("=" * 80)
        print(f"[FAIL] FOUND {len(all_errors)} PROTOTHREAD ISSUES!")
        if high_count > 0:
            print(f"         HIGH severity: {high_count} (safety footguns)")
        if medium_count > 0:
            print(f"         MEDIUM severity: {medium_count} (multi-instance hazards)")
        print("=" * 80)

        for e in all_errors:
            print("")
            print(f"  {e['line']:4d}: {e['message']}")

        print("")
        print("=" * 80)
        print("Protothread Safety Quick Reference:")
        print("  [OK] WINK_PT_STATE_* macros - SAFE (per-instance state, reentrant)")
        print("  [OK] static int x;         - SAFE FOR SINGLE INSTANCE ONLY")
        print("  [X]  int x;                - UNSAFE (garbage after yield!)")
        print("=" * 80)
        print("")
        return 1
    else:
        print("[OK] No protothread footguns detected - you are safe!")
        return 0


if __name__ == '__main__':
    sys.exit(main())

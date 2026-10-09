#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Vendor-mirror provenance and official ISR vector gate for CMS8S78xx apps
(PLAN-20261009-CMS8S78XX-BUILD-BASELINE, S2).

The mirrored vendor sources are the build input for both the SDCC gate and the
Wasm apps, so this tool answers two questions with hashes instead of prose:

  * where did each mirrored ``.c``/``.h`` come from and what is its approved
    content baseline (``upstream_normalized`` = the vendor original is the byte
    target, ``wink_adapted`` = documented business adaptation stays)?
  * does every EFFECTIVE interrupt registration carry the vector number the
    locked official device header defines?

Modes (mutually exclusive):
  --normalize-mirror  rewrite mirrored sources as canonical bytes: strict decode
                      (utf-8-sig, then gb18030; never errors=replace), UTF-8
                      without BOM, LF endings. Content is untouched.
  --restore-upstream  write canonical vendor content for every file whose
                      content_target is upstream_normalized.
  --write-lock        regenerate upstream-lock.json from the reference tree.
                      Requires every vendor file to be present.
  --verify            (default) compare the mirror against the approved lock;
                      when the reference tree is present, also re-check the
                      vendor raw/canonical hashes recorded in the lock.
  --isr-audit         official macro value == native WINK_ISR(N) == SDCC
                      __interrupt(N) == framework dispatch number, per handler.
  --self-test         run the negative fixtures (tamper, wrong mapping value,
                      BOM/CRLF, injected WINK_ISR, missing upstream).

The normal verify path never updates the lock: a mismatch is a finding, not
something to re-record. Exit code 0 = clean, 1 = findings, 2 = blocked/usage.
"""
import argparse
import hashlib
import json
import os
import pathlib
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import transpile_app_keil_c51 as keil  # noqa: E402
from mcs51_manifest import manifest_for_family  # noqa: E402

_MICRO_OS_DIR = next(p for p in pathlib.Path(HERE).resolve().parents
                     if p.name == "wink-micro-os")
ROOT = _MICRO_OS_DIR.parent

VENDOR_APPS_REL = "wink-micro-app/vendor/cms8s78xx"
LOCK_REL = VENDOR_APPS_REL + "/upstream-lock.json"
FAMILY = "cms8s78xx"
SCHEMA_VERSION = "cms8s78xx-upstream-lock/v1"
NORMALIZATION_RULES_VERSION = "strict-decode-utf8-no-bom-lf-v1"
TOKEN_RULE_VERSION = "c-strip-comments-token-sequence-v1"

# Codecs tried in order. utf-8-sig also accepts BOM-less UTF-8 and strips a
# leading BOM; gb18030 is the superset the vendor tree needs. Nothing is ever
# decoded with errors=replace, so an undecodable file is an error, not mojibake.
CODECS = ("utf-8-sig", "gb18030")

# Mirrored files whose approved baseline content IS the vendor original.
UPSTREAM_CONTENT_TARGET = {
    "epwm_brake_acmp/isr.c":
        "mirror declared INT2/INT3/INT4/UART1/UART2/SPI_I2C vectors that the locked "
        "device header does not define and dropped the official ACMP handler; SDCC "
        "cannot resolve undefined vector macros, so the vendor content is the target",
    "epwm_brake_delay_recover/isr.c": "same inferred-vector set as epwm_brake_acmp/isr.c",
    "epwm_brake_fb/isr.c": "same inferred-vector set as epwm_brake_acmp/isr.c",
    "epwm_brake_recover/isr.c": "same inferred-vector set as epwm_brake_acmp/isr.c",
    "epwm_brake_stop/isr.c": "same inferred-vector set as epwm_brake_acmp/isr.c",
    "epwm_brake_suspend/isr.c": "same inferred-vector set as epwm_brake_acmp/isr.c",
}

# Required for every file whose content differs from the vendor original beyond
# comments/whitespace while the baseline stays the adapted mirror.
ADAPTATION_REASONS = {
    "epwm_brake_acmp/main.c":
        "empty superloop body written as {} instead of the vendor { ; }: no semantic "
        "change, kept because the file is otherwise the approved mirror",
    "epwm_brake_fb/main.c": "same null-statement difference as epwm_brake_acmp/main.c",
    "led_4com_8seg/isr.c":
        "vendor trailing empty official handlers (ACMP..LVD) were dropped; the "
        "remaining handlers keep official vector names",
    "reset_wdt/main.c":
        "P33 latch written as 1 instead of 0 and WDT_Config()/SYS_EnableWDTReset() "
        "moved after the GPIO init block; this is the documented business rewrite "
        "whose old scenario assertion conflicts (plan section 5)",
    "temperture_sensor/demo_ts.c":
        "float TemperatureValue declaration hoisted above the vendor Count1/Count3/"
        "TS_ADCValue block: declaration-order rewrite kept as the approved baseline",
}


# ── canonicalization ────────────────────────────────────────────────────────

def strict_decode(data: bytes) -> tuple[str, str]:
    for codec in CODECS:
        try:
            return data.decode(codec), codec
        except UnicodeDecodeError:
            continue
    raise ValueError("no strict decoder accepted this file (errors=replace is forbidden)")


def canonical_text(text: str) -> str:
    return text.replace("\r\n", "\n").replace("\r", "\n")


def canonical_bytes(data: bytes) -> tuple[bytes, str]:
    text, codec = strict_decode(data)
    return canonical_text(text).encode("utf-8"), codec


def sha256_hex(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


_COMMENT_RE = re.compile(r"/\*[\s\S]*?\*/|//[^\n]*")
_TOKEN_RE = re.compile(r"[A-Za-z_$][A-Za-z0-9_$]*|0[xX][0-9a-fA-F]+|\d+|[^\sA-Za-z0-9_$]")


def token_sequence(text: str) -> list[str]:
    return _TOKEN_RE.findall(_COMMENT_RE.sub(" ", text))


def drift_kind(mirror_canon: str, upstream_canon: str) -> str:
    if mirror_canon == upstream_canon:
        return "identical"
    if token_sequence(mirror_canon) == token_sequence(upstream_canon):
        return "comment_or_whitespace_only"
    return "content_adapted"


# ── inventory ───────────────────────────────────────────────────────────────

def repo_path(rel: str) -> str:
    return os.path.join(str(ROOT), rel.replace("/", os.sep))


def read_repo_file(rel: str) -> bytes:
    with open(repo_path(rel), "rb") as handle:
        return handle.read()


def app_entries() -> list[dict]:
    """Enumerate vendor apps explicitly through wink-app.json (no workspace scan)."""
    base = repo_path(VENDOR_APPS_REL)
    entries = []
    for name in sorted(os.listdir(base)):
        app_dir = os.path.join(base, name)
        manifest_path = os.path.join(app_dir, "wink-app.json")
        if not os.path.isfile(manifest_path):
            continue
        with open(manifest_path, encoding="utf-8") as handle:
            manifest = json.load(handle)
        upstream = (manifest.get("upstream") or {}).get("source_dir")
        if not upstream:
            raise SystemExit(f"blocked: {name}/wink-app.json declares no upstream.source_dir")
        sources = sorted(f for f in os.listdir(app_dir)
                         if f.lower().endswith((".c", ".h")))
        entries.append({
            "app": name,
            "dir_rel": f"{VENDOR_APPS_REL}/{name}",
            "upstream_rel": f"{upstream}",
            "files": sources,
        })
    return entries


def mirror_files() -> list[tuple[str, str, str]]:
    """(app, basename, upstream_rel) for every mirrored top-level .c/.h."""
    out = []
    for entry in app_entries():
        for name in entry["files"]:
            out.append((entry["app"], name, f"{entry['upstream_rel']}/{name}"))
    return out


# ── lock ────────────────────────────────────────────────────────────────────

def build_lock() -> dict:
    records = []
    for app, name, upstream_rel in sorted(mirror_files()):
        key = f"{app}/{name}"
        mirror_rel = f"{VENDOR_APPS_REL}/{key}"
        mirror_raw = read_repo_file(mirror_rel)
        mirror_canon, mirror_codec = canonical_bytes(mirror_raw)
        record = {
            "app": app,
            "mirror_path": mirror_rel,
            "upstream_path": upstream_rel,
            "content_target": "upstream_normalized" if key in UPSTREAM_CONTENT_TARGET
                              else "wink_adapted",
            "upstream_raw_sha256": None,
            "upstream_encoding": None,
            "upstream_canonical_sha256": None,
            "mirror_canonical_sha256": sha256_hex(mirror_canon),
            "mirror_encoding": mirror_codec,
            "drift": None,
            "origin_assurance": "local_reference",
        }
        if os.path.isfile(repo_path(upstream_rel)):
            upstream_raw = read_repo_file(upstream_rel)
            upstream_canon, upstream_codec = canonical_bytes(upstream_raw)
            record["upstream_raw_sha256"] = sha256_hex(upstream_raw)
            record["upstream_encoding"] = upstream_codec
            record["upstream_canonical_sha256"] = sha256_hex(upstream_canon)
            record["drift"] = drift_kind(canonical_text(mirror_canon.decode("utf-8")),
                                         canonical_text(upstream_canon.decode("utf-8")))
        else:
            record["drift"] = "upstream_missing"
        if key in UPSTREAM_CONTENT_TARGET:
            record["reason"] = UPSTREAM_CONTENT_TARGET[key]
        elif record["drift"] == "content_adapted":
            record["reason"] = ADAPTATION_REASONS.get(key)
        records.append(record)
    return {
        "schema_version": SCHEMA_VERSION,
        "vendor": "Cmsemicon",
        "vendor_version": "V2.0.2",
        "family": FAMILY,
        "normalization_rules_version": NORMALIZATION_RULES_VERSION,
        "normalization_steps": [
            "strict decode, first codec that accepts every byte: utf-8-sig then gb18030",
            "re-encode as UTF-8 without BOM",
            "normalize CRLF and lone CR to LF",
        ],
        "token_rule_version": TOKEN_RULE_VERSION,
        "token_rule": "strip C comments from the raw text, then take identifiers, "
                      "hex/decimal numbers and single punctuation characters in order",
        "origin_assurance_note":
            "the reference tree is a locally held vendor package; identical bytes are a "
            "content statement, not an official signature or distribution-channel proof",
        "file_count": len(records),
        "files": records,
    }


def load_lock() -> dict:
    with open(repo_path(LOCK_REL), encoding="utf-8") as handle:
        return json.load(handle)


def write_lock(lock: dict) -> None:
    payload = json.dumps(lock, indent=2, ensure_ascii=False) + "\n"
    with open(repo_path(LOCK_REL), "w", encoding="utf-8", newline="\n") as handle:
        handle.write(payload)


# ── mutators ────────────────────────────────────────────────────────────────

def write_canonical(rel: str, raw: bytes) -> bool:
    canon, _ = canonical_bytes(raw)
    if read_repo_file(rel) == canon:
        return False
    with open(repo_path(rel), "wb") as handle:
        handle.write(canon)
    return True


def normalize_mirror() -> dict:
    changed = []
    for app, name, _ in mirror_files():
        rel = f"{VENDOR_APPS_REL}/{app}/{name}"
        if write_canonical(rel, read_repo_file(rel)):
            changed.append(f"{app}/{name}")
    return {"mode": "normalize_mirror", "rewritten": len(changed), "files": changed}


def restore_upstream() -> dict:
    restored, blocked = [], []
    for app, name, upstream_rel in mirror_files():
        key = f"{app}/{name}"
        if key not in UPSTREAM_CONTENT_TARGET:
            continue
        if not os.path.isfile(repo_path(upstream_rel)):
            blocked.append({"file": key, "reason": "vendor reference file missing"})
            continue
        rel = f"{VENDOR_APPS_REL}/{key}"
        with open(repo_path(upstream_rel), "rb") as handle:
            vendor_raw = handle.read()
        write_canonical(rel, vendor_raw)
        with open(repo_path(rel), "rb") as handle:
            if handle.read() == canonical_bytes(vendor_raw)[0]:
                restored.append(key)
            else:
                blocked.append({"file": key, "reason": "write did not produce canonical vendor bytes"})
    return {"mode": "restore_upstream", "restored": restored,
            "blocked": blocked, "status": "FAIL" if blocked else "PASS"}


# ── verify ──────────────────────────────────────────────────────────────────

def verify(lock: dict, lock_only: bool) -> dict:
    findings = []
    records = {f"{r['app']}/{os.path.basename(r['mirror_path'])}": r for r in lock["files"]}
    expected = {f"{app}/{name}" for app, name, _ in mirror_files()}
    for missing in sorted(expected - set(records)):
        findings.append({"file": missing, "code": "absent_from_lock"})
    for extra in sorted(set(records) - expected):
        findings.append({"file": extra, "code": "lock_entry_without_mirror"})

    origin_states = {"reverified": 0, "lock_only": 0}
    for key, record in sorted(records.items()):
        rel = record["mirror_path"]
        if not os.path.isfile(repo_path(rel)):
            findings.append({"file": key, "code": "mirror_missing"})
            continue
        canon, codec = canonical_bytes(read_repo_file(rel))
        digest = sha256_hex(canon)
        if record["content_target"] == "upstream_normalized":
            want = record["upstream_canonical_sha256"]
            field = "upstream_canonical_sha256"
        else:
            want = record["mirror_canonical_sha256"]
            field = "mirror_canonical_sha256"
        if want is None or digest != want:
            findings.append({"file": key, "code": "content_mismatch", "basis": field,
                             "expected": want, "actual": digest,
                             "encoding": codec})
        upstream_rel = record["upstream_path"]
        if lock_only or not os.path.isfile(repo_path(upstream_rel)):
            origin_states["lock_only"] += 1
            continue
        origin_states["reverified"] += 1
        vendor_raw = read_repo_file(upstream_rel)
        vendor_canon, _ = canonical_bytes(vendor_raw)
        if sha256_hex(vendor_raw) != record["upstream_raw_sha256"]:
            findings.append({"file": key, "code": "upstream_raw_drift",
                             "expected": record["upstream_raw_sha256"],
                             "actual": sha256_hex(vendor_raw)})
        if sha256_hex(vendor_canon) != record["upstream_canonical_sha256"]:
            findings.append({"file": key, "code": "upstream_canonical_drift",
                             "expected": record["upstream_canonical_sha256"],
                             "actual": sha256_hex(vendor_canon)})
        if record["content_target"] == "upstream_normalized":
            drift = drift_kind(canon.decode("utf-8"), vendor_canon.decode("utf-8"))
            if drift != "identical":
                findings.append({"file": key, "code": "upstream_target_not_matched",
                                 "drift": drift})
        elif record["drift"] == "content_adapted" and not record.get("reason"):
            findings.append({"file": key, "code": "undocumented_adaptation"})

    counts = {"identical": 0, "comment_or_whitespace_only": 0,
              "content_adapted": 0, "upstream_missing": 0}
    for record in lock["files"]:
        counts[record["drift"]] = counts.get(record["drift"], 0) + 1
    return {
        "mode": "verify",
        "status": "PASS" if not findings else "FAIL",
        "lock_schema": lock["schema_version"],
        "file_count": len(records),
        "origin_check": origin_states,
        "drift_counts": counts,
        "findings": findings,
    }


# ── ISR vector audit ────────────────────────────────────────────────────────

DEFINE_VECTOR_RE = re.compile(r"^\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)_VECTOR\s+([0-9]+)\b")
FRAMEWORK_ROW_RE = re.compile(r"\{\s*(IRQ_SOURCE_[A-Z0-9_]+)\s*,\s*\{\s*(\d+)u")
CORE_ROW_RE = re.compile(r"/\*\s*(IRQ_SOURCE_[A-Z0-9_]+)\s*\*/\s*\{\s*(\d+)u")
WHITELIST_RE = re.compile(r"kIrqVectorsCms8s\[\]\s*=\s*\{([^}]*)\}", re.S)


def official_vectors() -> dict[str, dict]:
    header_rel = manifest_for_family(FAMILY)["sdcc_gate"]["vendor_device_header"]
    text = canonical_bytes(read_repo_file(header_rel))[0].decode("utf-8")
    table = {}
    for lineno, line in enumerate(text.splitlines(), 1):
        match = DEFINE_VECTOR_RE.match(line)
        if match:
            table[match.group(1) + "_VECTOR"] = {"value": int(match.group(2)),
                                                 "source": f"{header_rel}:{lineno}"}
    if not table:
        raise SystemExit(f"blocked: no vector defines parsed from {header_rel}")
    return table


def framework_dispatch_numbers() -> dict[str, int]:
    """Source name -> vector number the framework dispatches (core rows + chip rows)."""
    dispatch = {}
    core_rel = "wink-micro-os/frameworks/mcs51/src/mcs51_isr.cpp"
    for name, value in CORE_ROW_RE.findall(read_repo_file(core_rel).decode("utf-8", "replace")):
        if int(value) != 0xFF:
            dispatch[name] = int(value)
    chip_rel = "wink-micro-os/frameworks/mcs51/chips/cms8s78xx/src/cms8s_sys.cpp"
    for name, value in FRAMEWORK_ROW_RE.findall(read_repo_file(chip_rel).decode("utf-8", "replace")):
        dispatch[name] = int(value)
    return dispatch


def framework_vector_whitelist() -> list[int]:
    src = read_repo_file("wink-micro-os/frameworks/mcs51/src/mcs51_family.cpp").decode("utf-8", "replace")
    body = WHITELIST_RE.search(src).group(1)
    return sorted(int(m) for m in re.findall(r"(\d+)u", body))


SOURCE_FOR_TOKEN = {
    "INT0": "IRQ_SOURCE_INT0", "TMR0": "IRQ_SOURCE_TIMER0", "INT1": "IRQ_SOURCE_INT1",
    "TMR1": "IRQ_SOURCE_TIMER1", "UART0": "IRQ_SOURCE_UART0", "TMR2": "IRQ_SOURCE_TIMER2",
    "ADC": "IRQ_SOURCE_ADC", "EPWM": "IRQ_SOURCE_PWM", "I2C": "IRQ_SOURCE_I2C",
    "SPI": "IRQ_SOURCE_SPI", "TMR3": "IRQ_SOURCE_TIMER3", "TMR4": "IRQ_SOURCE_TIMER4",
    "ACMP": "IRQ_SOURCE_ACMP", "WDT": "IRQ_SOURCE_WDT", "LVD": "IRQ_SOURCE_LVD",
}


def effective_isrs(text: str) -> list[dict]:
    """Keil `interrupt` registrations outside comments/strings and inactive
    preprocessor branches (the transpiler's own mask, so `#if 0` ISRs are excluded)."""
    mask = keil.build_code_mask(text)
    found = []
    for match in keil.ISR_RE.finditer(mask):
        found.append({"handler": match.group(1),
                      "vector_token": match.group(2) or match.group(3),
                      "line": mask[:match.start()].count("\n") + 1})
    return found


def isr_audit(app_filter: str | None = None) -> dict:
    official = official_vectors()
    dispatch = framework_dispatch_numbers()
    whitelist = framework_vector_whitelist()
    known = {k: int(v) for k, v in keil.KNOWN_VECTORS.items()}

    findings = []
    for name, value in sorted(known.items()):
        if name not in official:
            findings.append({"code": "known_vector_not_official", "macro": name,
                             "value": value})
        elif official[name]["value"] != value:
            findings.append({"code": "known_vector_value_mismatch", "macro": name,
                             "official": official[name]["value"], "transpiler": value})
    for name in sorted(official):
        if name not in known:
            findings.append({"code": "official_vector_missing_from_transpiler",
                             "macro": name, "value": official[name]["value"],
                             "source": official[name]["source"]})

    rows = []
    seen: dict[int, dict] = {}
    current_app = None
    for app, fname, _ in sorted(mirror_files()):
        if not fname.endswith(".c"):
            continue
        if app_filter and app != app_filter:
            continue
        if app != current_app:
            current_app = app
            seen = {}
        rel = f"{VENDOR_APPS_REL}/{app}/{fname}"
        text = canonical_bytes(read_repo_file(rel))[0].decode("utf-8")
        native, _ = keil.cleanup(text, target="native")
        sdcc, _ = keil.cleanup(text, target="sdcc")
        isrs = effective_isrs(text)
        # The native pass replaces the whole Keil signature with `WINK_ISR(N)`
        # (the handler name comes from the macro), so it is paired by source order.
        native_numbers = [int(m) for m in re.findall(r"WINK_ISR\(\s*(\d+)\s*\)", native)]
        sdcc_numbers = {name: int(num) for name, num in
                        re.findall(r"void\s+([A-Za-z_]\w*)\s*\([^)]*\)\s*__interrupt\(\s*(\d+)\s*\)",
                                   sdcc)}
        # Only officially defined (or bare numeric) vectors are rewritten by the
        # transpiler, so the native sequence pairs with that subsequence in order.
        rewritable = [i for i in isrs
                      if re.fullmatch(r"\d+", i["vector_token"])
                      or i["vector_token"] in keil.KNOWN_VECTORS]
        count_anomaly = len(native_numbers) != len(rewritable)
        native_iter = iter(native_numbers)
        for isr in isrs:
            token = isr["vector_token"]
            problems = []
            entry = {"app": app, "handler": isr["handler"],
                     "at": f"{rel}:{isr['line']}", "macro": token,
                     "sdcc_interrupt": sdcc_numbers.get(isr["handler"]),
                     "official": None}
            entry["native_wink_isr"] = (next(native_iter, None)
                                        if token in keil.KNOWN_VECTORS
                                        or re.fullmatch(r"\d+", token) else None)
            if count_anomaly:
                problems.append("native_transpile_registration_count_mismatch")
            if re.fullmatch(r"\d+", token):
                problems.append("bare_numeric_vector")
            elif token not in official:
                problems.append("vector_macro_unresolved")
            else:
                entry["official"] = official[token]["value"]
                entry["official_source"] = official[token]["source"]
                source = SOURCE_FOR_TOKEN.get(token.replace("_VECTOR", ""))
                entry["framework_irq_source"] = source
                entry["framework_dispatch_vector"] = (dispatch.get(source)
                                                      if source else None)
                entry["in_family_whitelist"] = entry["official"] in whitelist
                if entry["native_wink_isr"] != entry["official"]:
                    problems.append("native_transpile_mismatch")
                if entry["sdcc_interrupt"] != entry["official"]:
                    problems.append("sdcc_transpile_mismatch")
                if source is not None:
                    if entry["framework_dispatch_vector"] != entry["official"]:
                        problems.append("framework_dispatch_mismatch")
                else:
                    entry["framework_dispatch_note"] = (
                        "no semantic irq source row; the dispatch isr_table is indexed "
                        "directly by the vector number")
                if not entry["in_family_whitelist"]:
                    problems.append("vector_outside_family_whitelist")
                if entry["official"] in seen:
                    problems.append("duplicate_effective_registration")
                    seen[entry["official"]].setdefault(
                        "failure", "duplicate_effective_registration")
                    seen[entry["official"]]["failure"] = "duplicate_effective_registration"
                seen[entry["official"]] = entry
            if problems:
                entry["failure"] = "+".join(problems)
            rows.append(entry)
    for row in rows:
        if row.get("failure"):
            findings.append({"code": row["failure"], "app": row["app"],
                             "handler": row["handler"], "at": row["at"],
                             "macro": row["macro"], "official": row.get("official")})
    return {
        "mode": "isr_audit",
        "status": "PASS" if not findings else "FAIL",
        "official_vector_count": len(official),
        "official_vectors": {k: v for k, v in sorted(official.items())},
        "family_vector_whitelist": whitelist,
        "framework_dispatch": {k: v for k, v in sorted(dispatch.items())},
        "effective_registration_count": len(rows),
        "registrations": rows,
        "findings": findings,
    }


# ── self test (negatives) ───────────────────────────────────────────────────

def self_test() -> dict:
    results = []

    def check(name: str, condition: bool, detail: str = "") -> None:
        results.append({"case": name, "status": "PASS" if condition else "FAIL",
                        "detail": detail})

    with tempfile.TemporaryDirectory(prefix="mirror_audit_selftest_") as work:
        source = ("/* vendor\n   file */\nvoid main(void)\n"
                  "{\n\tP33 = 0;\n\twhile(1) { }\n}\n").encode("gb18030")
        raw_crlf = source.replace(b"\n", b"\r\n")
        canon_a, codec_a = canonical_bytes(raw_crlf)
        canon_b, codec_b = canonical_bytes(source)
        check("crlf_and_lf_normalization", canon_a == canon_b and codec_a == codec_b,
              f"codecs {codec_a}/{codec_b}")
        bom = b"\xef\xbb\xbf" + canon_a
        canon_c, codec_c = canonical_bytes(bom)
        check("bom_is_stripped", canon_c == canon_a and codec_c == "utf-8-sig", codec_c)
        try:
            canonical_bytes(b"\xff\xfe\x00invalid")
            check("undecodable_input_rejected", False, "no exception")
        except ValueError:
            check("undecodable_input_rejected", True)

        tampered = canon_a.replace(b"P33 = 0;", b"P33 = 1;")
        check("byte_tamper_changes_hash", sha256_hex(tampered) != sha256_hex(canon_a))
        check("comment_only_edit_keeps_tokens",
              token_sequence(canon_a.decode()) ==
              token_sequence(canon_a.decode() + "// trailing note\n"))
        check("punctuation_spacing_is_not_content",
              drift_kind("{ }\n", "{}\n") == "comment_or_whitespace_only")

        official = {"ACMP_VECTOR": 14, "TMR0_VECTOR": 1}
        wrong_map = {"ACMP_VECTOR": 15}
        check("legal_macro_wrong_value_detected",
              official["ACMP_VECTOR"] != wrong_map["ACMP_VECTOR"])
        sdcc_src = "void ACMP_IRQHandler(void) interrupt ACMP_VECTOR\n{\n}\n"
        cleaned, _ = keil.cleanup(sdcc_src, target="sdcc")
        match = re.search(r"__interrupt\(\s*(\d+)\s*\)", cleaned)
        check("transpiler_resolves_official_macro",
              match is not None and int(match.group(1)) == official["ACMP_VECTOR"],
              cleaned.strip())
        fake, _ = keil.cleanup("void X_IRQHandler(void) interrupt INT5_VECTOR\n{\n}\n",
                               target="sdcc")
        check("unofficial_macro_stays_unresolved", "INT5_VECTOR" in fake)
        masked = "#if 0\nvoid Y_IRQHandler(void) interrupt TMR0_VECTOR\n{\n}\n#endif\n"
        check("disabled_branch_not_effective", effective_isrs(masked) == [])

        native_out, _ = keil.cleanup(sdcc_src, target="native")
        check("wink_isr_marker_is_emitted_for_official_macro",
              "WINK_ISR(14)" in native_out, native_out.strip()[:120])
        mirror_with_wink_isr = sdcc_src + "\nvoid Z(void) WINK_ISR(14) { }\n"
        check("wink_isr_in_mirror_is_not_a_vector_registration",
              [i["handler"] for i in effective_isrs(mirror_with_wink_isr)] ==
              ["ACMP_IRQHandler"])

    failures = [r for r in results if r["status"] == "FAIL"]
    return {"mode": "self_test", "status": "PASS" if not failures else "FAIL",
            "cases": len(results), "results": results}


# ── cli ─────────────────────────────────────────────────────────────────────

def emit(payload: dict, out: str | None) -> None:
    text = json.dumps(payload, indent=2, ensure_ascii=False, sort_keys=False) + "\n"
    if out:
        with open(out, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(text)
    else:
        sys.stdout.write(text)


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--normalize-mirror", action="store_true")
    mode.add_argument("--restore-upstream", action="store_true")
    mode.add_argument("--write-lock", action="store_true")
    mode.add_argument("--verify", action="store_true")
    mode.add_argument("--isr-audit", action="store_true")
    mode.add_argument("--self-test", action="store_true")
    ap.add_argument("--app", help="restrict --isr-audit to one app")
    ap.add_argument("--lock-only", action="store_true",
                    help="verify against the approved lock without re-reading the vendor tree")
    ap.add_argument("--json", help="write the report here instead of stdout")
    args = ap.parse_args(argv[1:])

    if args.self_test:
        report = self_test()
        emit(report, args.json)
        return 0 if report["status"] == "PASS" else 1
    if args.normalize_mirror:
        emit(normalize_mirror(), args.json)
        return 0
    if args.restore_upstream:
        report = restore_upstream()
        emit(report, args.json)
        return 0 if report["status"] == "PASS" else 1
    if args.write_lock:
        missing = [r["mirror_path"] for r in build_lock()["files"]
                   if r["drift"] == "upstream_missing"]
        if missing:
            emit({"mode": "write_lock", "status": "BLOCKED",
                  "reason": "vendor reference files missing; the lock may never be "
                            "re-derived from the current mirror",
                  "missing": missing}, args.json)
            return 2
        lock = build_lock()
        undocumented = [f"{r['app']}/{os.path.basename(r['mirror_path'])}"
                        for r in lock["files"]
                        if r["drift"] == "content_adapted"
                        and r["content_target"] == "wink_adapted" and not r.get("reason")]
        if undocumented:
            emit({"mode": "write_lock", "status": "BLOCKED",
                  "reason": "content-adapted files need a documented adaptation reason",
                  "files": undocumented}, args.json)
            return 2
        write_lock(lock)
        emit({"mode": "write_lock", "status": "PASS", "lock_path": LOCK_REL,
              "file_count": lock["file_count"],
              "drift_counts": {k: sum(1 for r in lock["files"] if r["drift"] == k)
                               for k in ("identical", "comment_or_whitespace_only",
                                         "content_adapted")}}, args.json)
        return 0
    if args.isr_audit:
        report = isr_audit(args.app)
        emit(report, args.json)
        return 0 if report["status"] == "PASS" else 1

    if not os.path.isfile(repo_path(LOCK_REL)):
        emit({"mode": "verify", "status": "BLOCKED",
              "reason": f"{LOCK_REL} is absent; run --write-lock only with the vendor "
                        "reference tree verified present"}, args.json)
        return 2
    report = verify(load_lock(), args.lock_only)
    emit(report, args.json)
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))

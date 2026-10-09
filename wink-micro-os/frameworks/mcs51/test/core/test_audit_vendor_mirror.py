#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Negative and positive gate tests for audit_vendor_mirror.py
(PLAN-20261009-CMS8S78XX-BUILD-BASELINE, S2).

Fixtures build a synthetic vendor tree under a temp directory and point the
auditor's ROOT at it, so byte tampering, missing originals and legal-macro /
wrong-value mappings are exercised without touching the production mirror.
"""
import json
import os
import pathlib
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "tools"))
import audit_vendor_mirror as avm  # noqa: E402
import transpile_app_keil_c51 as keil  # noqa: E402
from mcs51_manifest import manifest_for_family  # noqa: E402

HEADER_REL = manifest_for_family(avm.FAMILY)["sdcc_gate"]["vendor_device_header"]
# Self-consistent stand-in for the vendor device header: every vector the
# transpiler claims to know, at the number the auditor has to agree with.
OFFICIAL_HEADER = "".join(f"#define {name} {value}\n"
                          for name, value in sorted(keil.KNOWN_VECTORS.items(),
                                                    key=lambda kv: int(kv[1])))
ISR_SOURCE = ("void INT0_IRQHandler(void) interrupt INT0_VECTOR { }\n"
              "void ACMP_IRQHandler(void) interrupt ACMP_VECTOR { }\n")


class MirrorFixture(unittest.TestCase):
    def setUp(self):
        self.work = os.path.abspath(tempfile.mkdtemp(prefix="avm_test_"))
        self.addCleanup(shutil.rmtree, self.work, True)
        self._root = avm.ROOT
        avm.ROOT = pathlib.Path(self.work)
        self.addCleanup(setattr, avm, "ROOT", self._root)
        for table in ("ADAPTATION_REASONS", "UPSTREAM_CONTENT_TARGET"):
            original = getattr(avm, table)
            setattr(avm, table, dict(original))
            self.addCleanup(setattr, avm, table, original)
        avm.ADAPTATION_REASONS["acmp/isr.c"] = "fixture: declares one extra inferred handler"

        self.write_text(HEADER_REL, OFFICIAL_HEADER)
        self.write_text("wink-micro-os/frameworks/mcs51/src/mcs51_isr.cpp",
                        "    /* IRQ_SOURCE_INT0 */   { 0u, 0xA8u, 0u, 0x88u, 1u, 0xB8u, 0u, X },\n"
                        "    /* IRQ_SOURCE_TIMER0 */ { 1u, 0xA8u, 1u, 0x88u, 5u, 0xB8u, 1u, X },\n")
        self.write_text("wink-micro-os/frameworks/mcs51/chips/cms8s78xx/src/cms8s_sys.cpp",
                        "    { IRQ_SOURCE_ACMP,   { 14u, 0xFFu, 0u, 0xFFu, 0u, 0xB9u, 7u, X } },\n"
                        "    { IRQ_SOURCE_PWM,    { 18u, 0xAAu, 3u, 0xB2u, 3u, 0xBAu, 3u, X } },\n")
        self.write_text("wink-micro-os/frameworks/mcs51/src/mcs51_family.cpp",
                        "const uint8_t kIrqVectorsCms8s[] = {\n    0u, 1u, 14u, 15u,\n};\n")

        # gpio: the mirror is the vendor original, written with CRLF endings.
        self.add_app("gpio", "up/code")
        self.write_text("up/code/isr.c", ISR_SOURCE)
        self.write_bytes(f"{avm.VENDOR_APPS_REL}/gpio/isr.c",
                         ISR_SOURCE.replace("\n", "\r\n").encode("utf-8"))
        # acmp: the mirror declares an extra vector the header does not define.
        self.add_app("acmp", "up/acmp")
        self.write_text("up/acmp/isr.c", ISR_SOURCE)
        self.write_text(f"{avm.VENDOR_APPS_REL}/acmp/isr.c",
                        ISR_SOURCE + "void INT5_IRQHandler(void) interrupt INT5_VECTOR { }\n")

    def add_app(self, app, upstream_dir):
        self.write_text(f"{avm.VENDOR_APPS_REL}/{app}/wink-app.json",
                        json.dumps({"app_name": app, "mcu": "cms8s78xx",
                                    "upstream": {"source_dir": upstream_dir}}))

    def path(self, rel):
        return os.path.join(self.work, rel)

    def write_text(self, rel, text):
        target = self.path(rel)
        os.makedirs(os.path.dirname(target), exist_ok=True)
        with open(target, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(text)

    def write_bytes(self, rel, payload):
        target = self.path(rel)
        os.makedirs(os.path.dirname(target), exist_ok=True)
        with open(target, "wb") as handle:
            handle.write(payload)

    def read_bytes(self, rel):
        with open(self.path(rel), "rb") as handle:
            return handle.read()

    def mirror_rel(self, app, name):
        return f"{avm.VENDOR_APPS_REL}/{app}/{name}"

    def write_lock(self):
        avm.write_lock(avm.build_lock())
        return avm.load_lock()

    def codes(self, report):
        return [f["code"] for f in report["findings"]]

    def assert_code(self, report, needle):
        self.assertTrue(any(needle in code for code in self.codes(report)),
                        self.codes(report))


class TestCanonicalization(MirrorFixture):
    def test_crlf_and_lf_normalize_to_the_same_canonical_bytes(self):
        crlf = self.read_bytes(self.mirror_rel("gpio", "isr.c"))
        lf = ISR_SOURCE.encode("utf-8")
        self.assertEqual(avm.canonical_bytes(crlf)[0], avm.canonical_bytes(lf)[0])
        self.assertEqual(avm.canonical_bytes(lf)[1], "utf-8-sig")

    def test_bom_is_stripped_and_undecodable_input_is_rejected(self):
        self.assertEqual(avm.canonical_bytes(b"\xef\xbb\xbfint a;")[0], b"int a;")
        with self.assertRaises(ValueError):
            avm.canonical_bytes(b"\xff\x00\xff\x00\x80\x81\xfe\xff\xfe")

    def test_non_ascii_vendor_encoding_decodes_strictly(self):
        payload = "/* \u7cfb\u7edf\u65f6\u949f */\nvoid f(void) { }\n".encode("gb18030")
        canon, codec = avm.canonical_bytes(payload)
        self.assertEqual(codec, "gb18030")
        self.assertIn("\u7cfb\u7edf", canon.decode("utf-8"))

    def test_drift_levels(self):
        self.assertEqual(avm.drift_kind("int a;\n", "int a;\n"), "identical")
        self.assertEqual(avm.drift_kind("int a; // note\n", "/* t */ int a;\n"),
                         "comment_or_whitespace_only")
        self.assertEqual(avm.drift_kind("{ }\n", "{}\n"), "comment_or_whitespace_only")
        self.assertEqual(avm.drift_kind("int a = 1;\n", "int a = 0;\n"), "content_adapted")

    def test_normalize_mirror_is_content_preserving(self):
        before = avm.token_sequence(self.read_bytes(self.mirror_rel("gpio", "isr.c")).decode("utf-8"))
        report = avm.normalize_mirror()
        self.assertEqual(report["rewritten"], 1)
        after = avm.token_sequence(self.read_bytes(self.mirror_rel("gpio", "isr.c")).decode("utf-8"))
        self.assertEqual(before, after)
        self.assertNotIn(b"\r", self.read_bytes(self.mirror_rel("gpio", "isr.c")))


class TestLockAndVerify(MirrorFixture):
    def test_lock_records_both_identities_and_verify_passes(self):
        lock = self.write_lock()
        self.assertEqual(lock["file_count"], 2)
        self.assertEqual(lock["normalization_rules_version"], avm.NORMALIZATION_RULES_VERSION)
        record = [r for r in lock["files"] if r["app"] == "gpio"][0]
        self.assertEqual(record["drift"], "identical")
        self.assertEqual(record["content_target"], "wink_adapted")
        self.assertIsNotNone(record["upstream_raw_sha256"])
        self.assertEqual(record["origin_assurance"], "local_reference")
        report = avm.verify(lock, lock_only=False)
        self.assertEqual(report["status"], "PASS", report["findings"])
        self.assertEqual(report["origin_check"], {"reverified": 2, "lock_only": 0})

    def test_business_byte_tamper_is_rejected_and_lock_is_untouched(self):
        lock = self.write_lock()
        with open(self.path(avm.LOCK_REL), "rb") as handle:
            lock_before = handle.read()
        self.write_bytes(self.mirror_rel("gpio", "isr.c"),
                         self.read_bytes(self.mirror_rel("gpio", "isr.c")).replace(b"ACMP", b"TMR3"))
        report = avm.verify(avm.load_lock(), lock_only=False)
        self.assertEqual(report["status"], "FAIL")
        self.assertEqual(self.codes(report), ["content_mismatch"])
        with open(self.path(avm.LOCK_REL), "rb") as handle:
            self.assertEqual(handle.read(), lock_before, "verify must never update the lock")

    def test_upstream_content_target_must_equal_the_vendor_original(self):
        target = self.mirror_rel("gpio", "isr.c")[len(avm.VENDOR_APPS_REL) + 1:]
        avm.UPSTREAM_CONTENT_TARGET = {target: "fixture: vendor content is the target"}
        lock = self.write_lock()
        record = [r for r in lock["files"] if r["app"] == "gpio"][0]
        self.assertEqual(record["content_target"], "upstream_normalized")
        self.assertEqual(record["reason"], "fixture: vendor content is the target")
        self.assertEqual(avm.verify(lock, lock_only=False)["status"], "PASS")
        # One extra space is still a deviation once the vendor text is the target.
        self.write_text(self.mirror_rel("gpio", "isr.c"),
                        ISR_SOURCE.replace("{ }\n", "{  }\n", 1))
        report = avm.verify(avm.load_lock(), lock_only=False)
        self.assertIn("content_mismatch", self.codes(report))

    def test_missing_vendor_file_blocks_lock_regeneration(self):
        os.remove(self.path("up/code/isr.c"))
        lock = avm.build_lock()
        record = [r for r in lock["files"] if r["app"] == "gpio"][0]
        self.assertEqual(record["drift"], "upstream_missing")
        self.assertIsNone(record["upstream_raw_sha256"])
        self.assertIsNone(record["upstream_canonical_sha256"])

    def test_verify_without_vendor_tree_reports_lock_only_state(self):
        lock = self.write_lock()
        os.remove(self.path("up/code/isr.c"))
        report = avm.verify(avm.load_lock(), lock_only=False)
        self.assertEqual(report["origin_check"], {"reverified": 1, "lock_only": 1})
        self.assertEqual(report["status"], "PASS")
        self.assertEqual(report["drift_counts"]["upstream_missing"], 0)

    def test_undocumented_adaptation_is_a_finding(self):
        lock = self.write_lock()
        record = [r for r in lock["files"] if r["app"] == "gpio"][0]
        record["drift"] = "content_adapted"
        record.pop("reason", None)
        self.assertIn("undocumented_adaptation", self.codes(avm.verify(lock, lock_only=False)))

    def test_vendor_reference_drift_is_rejected(self):
        self.write_lock()
        self.write_text("up/code/isr.c", ISR_SOURCE + "\nvoid inserted(void) { }\n")
        report = avm.verify(avm.load_lock(), lock_only=False)
        self.assertEqual(report["status"], "FAIL")
        self.assertIn("upstream_raw_drift", self.codes(report))

    def test_mirror_file_disappearing_from_the_app_is_a_finding(self):
        lock = self.write_lock()
        os.remove(self.path(self.mirror_rel("gpio", "isr.c")))
        self.assertIn("mirror_missing", self.codes(avm.verify(lock, lock_only=False)))

    def test_app_added_without_lock_entry_is_a_finding(self):
        lock = self.write_lock()
        self.add_app("newapp", "up/code")
        self.write_text(self.mirror_rel("newapp", "isr.c"), ISR_SOURCE)
        self.assertIn("absent_from_lock", self.codes(avm.verify(lock, lock_only=False)))

    def test_app_without_upstream_declaration_is_blocked(self):
        self.write_text(f"{avm.VENDOR_APPS_REL}/orphan/wink-app.json",
                        json.dumps({"app_name": "orphan", "mcu": "cms8s78xx"}))
        with self.assertRaises(SystemExit):
            avm.mirror_files()


class TestIsrVectorAudit(MirrorFixture):
    def test_official_values_match_both_transpiles_and_dispatch(self):
        report = avm.isr_audit("gpio")
        self.assertEqual(report["official_vector_count"], len(keil.KNOWN_VECTORS))
        self.assertEqual(report["framework_dispatch"],
                         {"IRQ_SOURCE_ACMP": 14, "IRQ_SOURCE_INT0": 0,
                          "IRQ_SOURCE_PWM": 18, "IRQ_SOURCE_TIMER0": 1})
        rows = {r["macro"]: r for r in report["registrations"]}
        acmp = rows["ACMP_VECTOR"]
        self.assertEqual((acmp["official"], acmp["native_wink_isr"],
                          acmp["sdcc_interrupt"], acmp["framework_dispatch_vector"]),
                         (14, 14, 14, 14))
        self.assertTrue(acmp["in_family_whitelist"])
        self.assertEqual(report["status"], "PASS", report["findings"])

    def test_inferred_vector_without_official_definition_fails(self):
        report = avm.isr_audit("acmp")
        self.assert_code(report, "vector_macro_unresolved")
        self.assertEqual(report["status"], "FAIL")

    def test_transpiler_map_missing_an_official_vector_fails(self):
        original = keil.KNOWN_VECTORS["EPWM_VECTOR"]
        del keil.KNOWN_VECTORS["EPWM_VECTOR"]
        self.addCleanup(keil.KNOWN_VECTORS.__setitem__, "EPWM_VECTOR", original)
        report = avm.isr_audit("gpio")
        self.assert_code(report, "official_vector_missing_from_transpiler")

    def test_legal_macro_with_wrong_transpiled_value_fails(self):
        original = keil.KNOWN_VECTORS["ACMP_VECTOR"]
        keil.KNOWN_VECTORS["ACMP_VECTOR"] = "15"
        self.addCleanup(keil.KNOWN_VECTORS.__setitem__, "ACMP_VECTOR", original)
        report = avm.isr_audit("gpio")
        self.assert_code(report, "known_vector_value_mismatch")
        row = [r for r in report["registrations"] if r["macro"] == "ACMP_VECTOR"][0]
        self.assertEqual(row["official"], 14)
        self.assertEqual(row["sdcc_interrupt"], 15)
        self.assertIn("sdcc_transpile_mismatch", row["failure"])

    def test_duplicate_effective_registration_across_tus_fails(self):
        self.write_text(self.mirror_rel("gpio", "extra.c"),
                        "void Second_INT0(void) interrupt INT0_VECTOR { }\n")
        report = avm.isr_audit("gpio")
        self.assert_code(report, "duplicate_effective_registration")

    def test_official_vector_outside_family_whitelist_fails(self):
        self.write_text(self.mirror_rel("gpio", "epwm.c"),
                        "void EPWM_IRQHandler(void) interrupt EPWM_VECTOR { }\n")
        report = avm.isr_audit("gpio")
        self.assertEqual(self.codes(report), ["vector_outside_family_whitelist"])

    def test_bare_numeric_vector_fails(self):
        self.write_text(self.mirror_rel("gpio", "numeric.c"),
                        "void Raw(void) interrupt 4 { }\n")
        report = avm.isr_audit("gpio")
        self.assert_code(report, "bare_numeric_vector")

    def test_dead_branch_registration_is_not_effective(self):
        self.write_text(self.mirror_rel("gpio", "dead.c"),
                        "#if 0\nvoid Dead(void) interrupt INT5_VECTOR { }\n#endif\n")
        report = avm.isr_audit("gpio")
        self.assertEqual(report["status"], "PASS", report["findings"])

    def test_wink_isr_written_into_a_mirror_source_is_not_a_registration(self):
        self.write_text(self.mirror_rel("gpio", "injected.c"),
                        "void Fake(void) WINK_ISR(14) { }\n")
        report = avm.isr_audit("gpio")
        self.assertEqual([r for r in report["registrations"] if r["handler"] == "Fake"], [])
        self.assertEqual(report["status"], "PASS", report["findings"])


class TestProductionLock(unittest.TestCase):
    def test_generated_lock_matches_the_production_mirror(self):
        if not os.path.isfile(avm.repo_path(avm.LOCK_REL)):
            self.skipTest(f"{avm.LOCK_REL} has not been generated yet")
        report = avm.verify(avm.load_lock(), lock_only=False)
        self.assertEqual(report["file_count"], 148)
        self.assertEqual(report["status"], "PASS", report["findings"][:3])
        self.assertEqual(report["origin_check"]["lock_only"], 0)

    def test_production_isr_audit_is_clean(self):
        if not os.path.isfile(avm.repo_path(avm.LOCK_REL)):
            self.skipTest(f"{avm.LOCK_REL} has not been generated yet")
        report = avm.isr_audit()
        self.assertEqual(report["status"], "PASS", report["findings"][:3])
        self.assertEqual(report["official_vector_count"], 20)

    def test_production_lock_targets_only_the_six_epwm_isr_files(self):
        if not os.path.isfile(avm.repo_path(avm.LOCK_REL)):
            self.skipTest(f"{avm.LOCK_REL} has not been generated yet")
        lock = avm.load_lock()
        targeted = sorted(f"{r['app']}/{os.path.basename(r['mirror_path'])}"
                          for r in lock["files"]
                          if r["content_target"] == "upstream_normalized")
        self.assertEqual(targeted, sorted(avm.UPSTREAM_CONTENT_TARGET))
        self.assertTrue(all(r.get("reason") for r in lock["files"]
                            if r["content_target"] == "upstream_normalized"))
        self.assertTrue(all(r.get("reason") for r in lock["files"]
                            if r["drift"] == "content_adapted"))
        drifts = [r["drift"] for r in lock["files"]]
        self.assertNotIn("upstream_missing", drifts)
        self.assertEqual(len(drifts), 148)


if __name__ == "__main__":
    unittest.main()

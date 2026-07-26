#!/usr/bin/env python3
import json
import os
import tempfile
import unittest
from tools.codegen.config_h import find_config_key, main


class TestConfigH(unittest.TestCase):
    def test_find_config_key_flat(self):
        data = {"sim_heap_quota_kb": 128}
        self.assertEqual(find_config_key(data, "sim_heap_quota_kb"), 128)

    def test_find_config_key_nested(self):
        data = {
            "target_config": {
                "memory": {
                    "sim_heap_quota_kb": 512
                }
            }
        }
        self.assertEqual(find_config_key(data, "sim_heap_quota_kb"), 512)

    def test_find_config_key_not_found(self):
        data = {"other": 123}
        self.assertIsNone(find_config_key(data, "sim_heap_quota_kb"))
        self.assertEqual(find_config_key(data, "sim_heap_quota_kb", 256), 256)

    def test_config_h_generation_with_nested_memory(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            app_json = os.path.join(tmpdir, "wink_app.json")
            out_header = os.path.join(tmpdir, "wink_config.h")
            
            app_data = {
                "app_name": "test_app",
                "target_config": {
                    "memory": {
                        "sim_heap_quota_kb": 1024
                    }
                }
            }
            with open(app_json, "w", encoding="utf-8") as f:
                json.dump(app_data, f)
            
            test_args = ["config_h.py", "--input", app_json, "--output", out_header, "--target", "esp32"]
            import sys
            old_argv = sys.argv
            try:
                sys.argv = test_args
                exit_code = main()
                self.assertEqual(exit_code, 0)
            finally:
                sys.argv = old_argv

            with open(out_header, "r", encoding="utf-8") as f:
                content = f.read()

            self.assertIn("#define WINK_SIM_HEAP_QUOTA_BYTES   (1048576UL)", content)


if __name__ == "__main__":
    unittest.main()

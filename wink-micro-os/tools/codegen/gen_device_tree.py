#!/usr/bin/env python3
"""
设备树代码生成器 - C 头文件生成
SSOT: 从 YAML 设备树生成 C 侧使用的引脚定义

用法: python gen_device_tree.py ../device-trees/avoidance-car.yaml output/generated_device_tree.h
"""

import sys
import yaml
from datetime import datetime


def generate_c_header(yaml_path: str, output_path: str):
    with open(yaml_path, 'r', encoding='utf-8') as f:
        dt = yaml.safe_load(f)

    lines = [
        "/* ========================================",
        f" * 此文件由 gen_device_tree.py 自动生成！",
        f" * 生成时间: {datetime.now().isoformat()}",
        f" * 源文件: {yaml_path}",
        " *",
        " * 警告：不要手动修改此文件！",
        " *       修改 YAML 源文件后重新编译即可",
        " * ========================================",
        " */",
        "",
        "#ifndef WINK_GENERATED_DEVICE_TREE_H",
        "#define WINK_GENERATED_DEVICE_TREE_H",
        "",
        "// ========================================",
        "// 板级信息",
        "// ========================================",
        f'#define DT_BOARD_NAME "{dt["board"]["name"]}"',
        f'#define DT_BOARD_MCU "{dt["board"]["mcu"]}"',
        f'#define DT_VERSION "{dt["version"]}"',
        "",
        "// ========================================",
        "// 外设引脚定义",
        "// ========================================",
        "",
    ]

    # 生成每个外设的引脚宏
    for periph in dt["peripherals"]:
        instance_id = periph["instanceId"]
        periph_type = periph["type"]
        name_upper = instance_id.replace(":", "_").upper()

        lines.append(f"// {periph.get('displayName', instance_id)} ({periph_type})")
        for pin_name, pin_num in periph["pins"].items():
            macro_name = f"DT_{name_upper}_PIN_{pin_name.upper()}"
            lines.append(f"#define {macro_name}  {pin_num}")
        lines.append("")

    # 生成属性宏
    lines.append("// ========================================")
    lines.append("// 外设属性配置")
    lines.append("// ========================================")
    lines.append("")

    for periph in dt["peripherals"]:
        if "properties" not in periph:
            continue
        instance_id = periph["instanceId"]
        name_upper = instance_id.replace(":", "_").upper()

        for prop_name, prop_val in periph["properties"].items():
            macro_name = f"DT_{name_upper}_PROP_{prop_name.upper()}"
            if isinstance(prop_val, bool):
                val_str = "1" if prop_val else "0"
            elif isinstance(prop_val, str):
                val_str = f'"{prop_val}"'
            else:
                val_str = str(prop_val)
            lines.append(f"#define {macro_name}  {val_str}")
        lines.append("")

    # 固件选项
    if "firmware" in dt:
        lines.append("// ========================================")
        lines.append("// 固件编译选项")
        lines.append("// ========================================")
        lines.append("")
        for key, val in dt["firmware"].items():
            macro_name = f"DT_FW_{key.upper()}"
            if isinstance(val, bool):
                val_str = "1" if val else "0"
            else:
                val_str = str(val)
            lines.append(f"#define {macro_name}  {val_str}")
        lines.append("")

    lines.append("#endif // WINK_GENERATED_DEVICE_TREE_H")
    lines.append("")

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write("\n".join(lines))

    print(f"[OK] Generated C header: {output_path}")
    print(f"   - {len(dt['peripherals'])} peripherals defined")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python gen_device_tree.py <input_yaml> <output_h>")
        sys.exit(1)
    generate_c_header(sys.argv[1], sys.argv[2])

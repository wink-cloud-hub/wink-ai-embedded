# Buzzer (`buzzer` 1.0.0) 外置外设插件

本目录为 Wink-AI 系统的标准外设插件 **Buzzer 蜂鸣器 (`buzzer` 1.0.0)**。

- **Type**: `buzzer`
- **Version**: `1.0.0`
- **Category**: `output`
- **Wokwi Element**: `<wokwi-buzzer>`
- **C DAL 驱动**: `wink-micro-os/dal/include/output/dal_buzzer.h`
- **Codegen YAML**: `wink-micro-os/codegen/drivers/buzzer.yaml`

## 变体支持 (Variants)

1. **`passive_pwm`** (默认): 无源压电蜂鸣器，由 PWM 频率和占空比驱动。
2. **`active_gpio`**: 有源蜂鸣器，由数字 GPIO 电平控制通断发声。

## 引脚拓扑 (Pins)

- **`1`** (Signal/PWM/GPIO): 信号输入脚 (Anode / PWM / GPIO)，必选。
- **`2`** (GND): 电源地引脚 (Cathode / GND)，可选接地。

## 状态通道 (State Channels)

- **`hasSignal`** (`boolean`): 是否处于发声状态（对应 `wokwi-buzzer` 的音符动画展示）。
- **`frequency`** (`number`, Hz): 当前发声频率。
- **`duty`** (`number`, %): 当前 PWM 占空比百分比。

## 事件 (Events)

- **`PLAY_TONE`**: `{ freqHz?: number }` 发出指定频率声音。
- **`STOP_TONE`**: `{}` 停止发声。
- **`SET_SIGNAL`**: `{ hasSignal: boolean }` 切换发声状态。

# 排插式平行布线 + 外设旋转

## Context

当前画布支持两种 wireStyle：`pcb`（45° 倒角）和 `curved`（贝塞尔曲线），均基于 A* 自由布线。用户希望新增第三种 `bus`（排插式平行布线）——引出到外设的线如 IDC 排线般平行引出，配合外设可旋转（0/90/180/270°），实现"直插"体验：用户旋转外设让引脚朝向排针，线条自然平行对齐。这对多引脚外设（OLED、HC-SR04）尤其美观，符合真实排针+排线的硬件直觉。

三种 wireStyle 各司其职：PCB 工业感 / Curved 教育感 / Bus 总线感。用户通过顶部下拉框手动切换。

## 实现方案

### 一、peripheral-pins.ts 新增两个导出函数

**1. `rotatePinOffset`**（放在 `boardDescriptor` 之后，约 L146）

离散旋转矩阵（屏幕坐标系 Y 轴向下，CSS `rotate(90deg)` 顺时针）。以组件中心 (W/2, H/2) 为原点：

```ts
export function rotatePinOffset(relX, relY, W, H, rotation): { x, y } {
  const cx = W / 2, cy = H / 2;
  const dx = relX - cx, dy = relY - cy;
  switch (((rotation % 360) + 360) % 360) {
    case 0:   return { x: relX, y: relY };
    case 90:  return { x: cx - dy, y: cy + dx };
    case 180: return { x: cx - dx, y: cy - dy };
    case 270: return { x: cx + dy, y: cy - dx };
    default:  return { x: relX, y: relY };
  }
}
```

**2. `generateBusStripPath`**（放在 `generateSmartPCBPath` 之后，约 L847）

三段式平行路径：入口短桩 → 平行干道 → 出口短桩。**完全绕过 A*，不做避障**。

```ts
export function generateBusStripPath(
  start, end, startDir, endDir, lane, signalType
): WirePathResult
```

- **启用条件**：仅当 startDir 与 endDir 反向同轴（如 right+left 水平、up+down 垂直）时生成排插路径；否则回退 `generateSmartPCBPath(..., 'pcb')`，避免错误路径
- **水平总线**（endDir='left'）：`start → (fanX, start.y) → (fanX, end.y) → end`，`fanX = end.x - stubLen`；endDir='right' 时 `fanX = end.x + stubLen`。多引脚外设各引脚 Y 不同 → 段1天然平行
- **垂直总线**（endDir='up'）：`start → (start.x, fanY) → (end.x, fanY) → end`，`fanY = end.y - stubLen`
- **间距**：`stubLen = 18 + lane * 4`，lane 越大 fan 越远，自然错开
- **复用渲染管线**：`chamferPathCorners(points, 6)` + `pointsToSvgPath` 生成 segments（单层 layer 0）；`generateTeardropPath` 在 start/end 端生成泪滴；`vias: []`；width 沿用 signalType 映射

### 二、EmbeddedWorkbench.vue 修改清单

| # | 位置 | 修改 |
|---|------|------|
| 1 | L40-43 下拉框 | 新增 `<option value="bus">Bus Strip (Parallel)</option>` |
| 2 | L303-308 wrapper style | 加 `'--rot': ${comp.rotation\|\|0}deg` |
| 3 | L473 后属性面板 | 新增 Rotation 按钮组（0/90/180/270°）+ `setRotation(comp, deg)` 函数 |
| 4 | L618-632 import | 加 `generateBusStripPath, rotatePinOffset` |
| 5 | L641-647 ComponentInstance | 加 `rotation: number` |
| 6 | L673 wireStyle | 类型改 `'pcb' \| 'curved' \| 'bus'` |
| 7 | L729-758 初始数据 + L842 addComponent | 各加 `rotation: 0` |
| 8 | L1369-1387 | 提取 `componentSizes` 常量；`getComponentWidth/Height` 在 90/270° 互换宽高 |
| 9 | L1413-1421 getPeripheralPinPosition | 调用 `rotatePinOffset` 变换 relX/relY |
| 10 | L1480-1494 getWirePCBPath | `startDir = rotateDir(baseStartDir, rotation)`；末尾加 bus 分流：`if (wireStyle==='bus' && routingMode==='auto') return generateBusStripPath(...)` |
| 11 | L1490 endDir | **既有问题修复**：`pts.end.x < boardPosition.value.x + boardDescriptor.width/2 ? 'left' : 'right'`（原硬编码 400，开发板拖动后失效） |
| 12 | L1498-1508 wiresToRender obstacle | 改用新增 `getComponentObstacle(comp)`（90/270° AABB 宽高互换，中心不变） |
| 13 | L1336 拖拽边界 | 改用 `getComponentWidth/Height`（旋转感知） |
| 14 | L1712-1731 CSS | `.canvas-peripheral-wrapper` 基础 `transform: rotate(var(--rot,0deg))`，hover/drag 组合 `rotate() scale()`，`transform-origin: center` |
| 15 | CSS 末尾 | 新增 `.rotation-btn-group` / `.rotation-btn` 样式 |
| 16 | script 区 | 新增 `rotateDir(dir, rotation)`（顺时针序数组 `[up,right,down,left]`）和 `getComponentObstacle(comp)` 辅助函数 |

### 三、协同机制

用户拖外设到开发板左侧 → 旋转 270°（LED 引脚朝右）→ startDir='right'，endDir='left'（左排针）→ 反向同轴 → 生成水平平行路径。若未正确旋转（startDir 与 endDir 不同轴），bus 自动回退 pcb，不产生错误路径。manual 模式下 bus 也回退 pcb（waypoint 需精细控制）。

## 验证

1. `npx vue-tsc --noEmit` 类型检查通过
2. `npm run dev` 启动，切到 Canvas tab：
   - 切换 Wire Style 下拉框，PCB/Curved/Bus 三种风格均正常渲染
   - Bus 模式下，OLED 拖到开发板右侧，属性面板点 90° 旋转 → 4 根线（SDA/SCL/3V3/GND）平行引出
   - HC-SR04 拖到左侧，旋转 270° → 4 根线平行
   - 旋转后拖拽外设，边界约束正确，不超出画布
   - 开发板拖动后，Bus 线条端点跟随排针新位置（验证 endDir 修复）
   - Button 旋转后点击仍有响应（CSS 旋转 hit-testing）
3. 切换 manual 模式，Bus 下拖拽 waypoint → 回退 pcb 行为，无闪烁

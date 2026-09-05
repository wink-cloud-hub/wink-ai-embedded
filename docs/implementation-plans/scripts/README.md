# 实施计划（Implementation Plans）状态查询工具使用说明

这是一个用于快速扫描、统计和过滤当前仓库中所有**实施计划**状态的脚本工具。

## 工具组成

1. **[list_plans.py](./list_plans.py)**：状态解析核心 Python 3 脚本。
2. **[list_plans.bat](./list_plans.bat)**：Windows 下的一键运行批处理脚本（自动设置终端为 UTF-8 编码，防止中文及 Emoji 乱码）。

---

## 快速使用

### 1. 默认展示（仅显示活跃的标准计划）
默认情况下，脚本**会自动过滤掉非标准模板格式、旧格式或无法解析出状态的文档**（显示为 `Unknown`），仅列出属于标准格式且处于 **活跃状态**（未完成、执行中、暂停）的计划，并给出统计概要。
```powershell
# 在项目根目录下运行
python docs/implementation-plans/scripts/list_plans.py
```

### 2. 展示全部标准计划
展示所有标准格式的计划（包括已完成、已取消的旧计划）。
```powershell
python docs/implementation-plans/scripts/list_plans.py -a
# 或
python docs/implementation-plans/scripts/list_plans.py --all
```

### 3. 包含旧格式/非标准文档 (`--include-unknown`)
如果需要显示或统计那些旧格式、架构评审报告等非标准计划的文档，可附加 `--include-unknown` 参数：
```powershell
python docs/implementation-plans/scripts/list_plans.py -a --include-unknown
```

---

## 过滤与筛选

### 1. 按状态过滤 (`-s` / `--status`)
脚本能智能识别文档元数据中的状态，并归类为 5 种分类。你可以输入分类英文名、中文名或模糊状态字符进行过滤：
* **已完成 (Completed)**：匹配关键字 `completed`, `done`, `已完成`, `实施完成`, `已执行`, `执行完成`, `结项` 等。
* **执行中 (In Progress)**：匹配关键字 `progress`, `in-progress`, `doing`, `执行中`, `进行中` 等。
* **草拟 (Draft)**：匹配关键字 `draft`, `proposed`, `草稿`, `草拟`, `未完成`, `待确认` 等。
* **暂停 (Paused)**：匹配关键字 `paused`, `暂停` 等。
* **已取消 (Canceled)**：匹配关键字 `canceled`, `cancelled`, `obsolete`, `取消`, `作废`, `废弃` 等。

**使用示例：**
```powershell
# 筛选已完成的计划
python docs/implementation-plans/scripts/list_plans.py -s completed
python docs/implementation-plans/scripts/list_plans.py -s 已完成

# 筛选正在执行中的计划
python docs/implementation-plans/scripts/list_plans.py -s progress
python docs/implementation-plans/scripts/list_plans.py -s 执行中

# 模糊查找状态描述中包含 "master" 的计划
python docs/implementation-plans/scripts/list_plans.py -s master
```

### 2. 按负责人过滤 (`-o` / `--owner`)
支持按计划的负责人（Owner）名字进行不区分大小写的模糊搜索。
```powershell
# 查找负责人名字包含 "wink-ai" 的计划
python docs/implementation-plans/scripts/list_plans.py -o wink-ai
```

### 3. 按优先级过滤 (`-p` / `--priority`)
支持按优先级（如 P0, P1, P2）进行模糊匹配。
```powershell
# 查找所有 P0 优先级的计划
python docs/implementation-plans/scripts/list_plans.py -p P0
```

### 4. 复合筛选
各个筛选条件可以任意组合叠加使用。
```powershell
# 查找负责人是 wink-ai 的已完成的 P0 计划
python docs/implementation-plans/scripts/list_plans.py -o wink-ai -s completed -p P0
```

---

## Windows 一键批处理运行

如果你在 Windows CMD 环境下，可以直接双击运行 `list_plans.bat` 或在命令行执行它。它将自动启用 UTF-8 编码页，并调起 Python 执行：
```cmd
cd docs\design\implementation-plans\scripts
list_plans.bat -a
```

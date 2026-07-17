# GitHub Release 草稿：mixed-nozzle-u1

## 标题

Snapmaker Orca 2.3.5 U1 Experimental Alpha

## Tag

`u1-experimental-2.3.5-alpha.1`

## 目标分支

`Min/2.3.5-beta-mixed-nozzle`

## 摘要

这是 Snapmaker U1 的实验性混合口径构建，用于一个喷嘴打印细外墙，另一个喷嘴打印内墙/填充。首个真实打印验证组合是 0.2 mm 外墙 + 0.4 mm 内墙/填充。

这个 release 需要同时使用：

- 当前分支的补丁版 Snapmaker Orca
- 带混合口径校验补丁的 SnapmakerU1-Extended-Firmware

## 亮点

- U1 每个物理头的喷嘴直径可以独立编辑。
- 喷嘴页签显示当前喷嘴直径。
- 准备页新增 Mixed Nozzle / 混合口径工作站。
- `outer_wall_filament` 支持外墙使用独立喷嘴。
- `mixed_nozzle_mode` 支持：
  - Same layer, different line widths
  - Mixed layer, different line widths
- 稀疏填充、内墙、内部实心填充跨层合并拆成独立开关。
- 自动粗层高默认取粗喷嘴直径的一半，并受粗喷嘴最大层高限制。
- 旧手动 ratio 仍保留为高级兼容路径。
- 支持从连接的 U1 同步机器耗材。
- 机器物理头顺序通过 `extruder_map_table` 映射到切片器逻辑 T 槽。
- 附带 G-code role/tool 检查脚本：`scripts/check_mixed_nozzle_gcode.py`。

## 需要附加的资产

- Windows Snapmaker Orca 构建产物。
- 带固件补丁的 U1 `.bin`。
- 可选：0.10 mixed-layer profile 生成的立方体 G-code。
- 真实打印验证照片：`docs/mixed-nozzle-u1/assets/real-print-cube.jpg`

固件二进制只能从已公开的对应源码提交构建，并同时附 SHA-256、GPL-3.0 许可证和对应源码链接。

## 验证结果

- Snapmaker Orca Release 构建通过。
- `MachineFilamentSync` 单测通过。
- `MixedLayerHeight` 单测覆盖 span 规划、自动粗层高、最大层高限制和旧 ratio 回退。
- Snapmaker profile validator 通过。
- 20 mm 立方体 G-code 检查通过：
  - 外墙 T1 / 0.2 mm / 0.10 mm
  - 内墙 T0 / 0.4 mm / 0.20 mm
  - 稀疏填充 T0 / 0.4 mm / 主要 0.20 mm
  - 内部实心填充 T0 / 0.10 mm 或 0.20 mm，取决于重叠区域
  - T2/T3 不参与对象挤出
- 0.2/0.8 mm 组合的 G-code 检查也通过，但还没有真实打印验证。
- 2026-06-18 Snapmaker U1 真实混合口径打印通过。

检查命令示例：

```powershell
python scripts\check_mixed_nozzle_gcode.py `
  --gcode path\to\plate_1.gcode `
  --outer-tool T1 --inner-tool T0 --sparse-infill-tool T0 --solid-infill-tool T0 `
  --forbid-object-tools T2,T3
```

## 真实打印照片

![Snapmaker U1 混合口径真实打印成功样件](https://raw.githubusercontent.com/MOVIBALE/OrcaSlicer/mixed-nozzle-u1/docs/mixed-nozzle-u1/assets/real-print-cube.jpg)

## 刷机警告

这是实验固件和实验切片行为。只有在你有已知可用固件恢复路径时才建议刷入。

固件补丁只改变喷嘴直径校验，不保证流量、排料、擦嘴、温度、压力提前或工具偏移已经适合你的具体硬件组合。

## 已知限制

- 内墙跨层合并是实验功能，没有针对不同细层几何重新裁剪。
- 内部实心填充跨层合并只处理完全重叠区域；顶面/底面保持原工艺层高。
- 0.2/0.4 以外的口径组合需要继续实物验证。
- Prime tower、擦嘴和排料量还没有针对非对称喷嘴彻底调优。
- 多个槽位同名同色时，预览和摘要可能不够直观。
- 当前没有 CI 打包。

## 发布前 checklist

- [ ] 确认 Windows 构建启动的是 mixed-nozzle 分支。
- [ ] 确认连接 U1 后，机器耗材同步会按 `extruder_map_table` 映射到 T0/T1/T2/T3。
- [ ] 切一个 20 mm 立方体并按工具查看预览。
- [ ] 导出 G-code 并运行 role/tool 检查脚本。
- [x] 只在测试 U1 上刷入固件。
- [x] 打印小样件并记录真实打印成功。

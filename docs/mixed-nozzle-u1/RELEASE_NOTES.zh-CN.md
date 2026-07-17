# Mixed Nozzle U1 发布说明

## 范围

这是 Snapmaker U1 的实验性混合口径软件构建，需要配套刷入包含混合口径校验补丁的 U1 Extended Firmware。

已经真实打印验证的场景：

- 0.2 mm 喷嘴打印可见外墙
- 0.4 mm 喷嘴打印内墙和填充

切片器现在提供两个通用混合口径模式：

- Same layer, different line widths
- Mixed layer, different line widths

准备页新增 **Mixed Nozzle / 混合口径** 工作站，把模式、特征分配、喷嘴映射、层高合并和校验集中在一个入口里。

## 软件变化

- 新增 `outer_wall_filament`，外墙可以独立选择喷嘴。
- 外墙挤出路由到 `outer_wall_filament`。
- 保留内墙、稀疏填充、实心填充原有特征喷嘴设置。
- 新增混合口径工作站。
- 将 mixed-layer 合并拆成三个独立开关：
  - 稀疏填充合并
  - 内墙合并
  - 内部实心填充合并
- 稀疏填充合并是保守默认；内墙和内部实心填充合并是实验选项。
- 新增自动粗层高：默认取粗喷嘴直径的一半，并受粗喷嘴最大层高限制。
- 旧的手动 ratio 参数仍保留为高级兼容路径。
- U1 喷嘴直径可以逐头独立编辑，并显示在喷嘴页签上。
- 支持从连接的 U1 同步机器耗材，并按 `extruder_map_table` 从物理头顺序映射到逻辑 T 槽。
- 新增 `scripts/check_mixed_nozzle_gcode.py`，用于重复验证 G-code role/tool 分配。
- 新增/更新 Snapmaker U1 混合口径示例工艺。

## 固件要求

需要使用 `SnapmakerU1-Extended-Firmware` 中带混合口径校验补丁的 U1 固件。

补丁会按下面的路径校验喷嘴：

`logical_index -> extruder_map_table[logical_index] -> actual physical nozzle`

没有这个补丁时，即使物理喷嘴配置正确，机器也可能拒绝混合口径 G-code。

## 验证

本地 Windows 验证项：

- Snapmaker Orca Release 构建：通过。
- `MachineFilamentSync` 单测：通过。
- `MixedLayerHeight` 单测：覆盖 span 规划、自动粗层高、最大层高限制和旧 ratio 回退。
- Snapmaker profile validator：通过。
- 立方体 G-code role/tool 检查：通过。
- 2026-06-18 Snapmaker U1 真实混合口径打印：通过。

G-code 检查命令示例：

```powershell
python scripts\check_mixed_nozzle_gcode.py `
  --gcode path\to\plate_1.gcode `
  --outer-tool T1 --inner-tool T0 --sparse-infill-tool T0 --solid-infill-tool T0 `
  --forbid-object-tools T2,T3
```

真实打印图片：

![Snapmaker U1 混合口径真实打印成功样件](assets/real-print-cube.jpg)

## 兼容性

- 目标机器：Snapmaker U1。
- 目标固件基线：SnapmakerU1-Extended-Firmware 1.4.1 系列构建。
- 目标切片器基线：Snapmaker Orca 2.3.5 Beta（`761718a5`）。
- 已真实打印验证：逻辑 T0 0.4 mm + 逻辑 T1 0.2 mm。

其他口径组合可以使用同一套工作站和模式控制。0.2/0.8 mm 已做过 G-code 自动 4 层合并检查，但还没有真实打印验证。

## 限制

- 打印质量仍是实验状态，长时间打印前需要调速度、温度、流量、压力提前、擦嘴和排料。
- 内墙跨层合并还没有针对不同 Z 截面的几何变化重新裁剪，复杂斜面和薄壁可能需要继续改切片逻辑。
- 内部实心填充只会合并完全重叠区域；顶面和底面保持原工艺层高。
- 同层高模式依赖你选择的喷嘴和线宽本身合理。
- 多个槽位使用相同耗材名/颜色时，预览和摘要可能不够直观。
- 固件补丁只改变校验逻辑，不保证所有机械和材料组合都安全。
- 发布包仍是实验构建，必须标记为 Pre-release。

## 恢复

刷测试固件前保留已知可用的 U1 固件 `.bin`。如果测试固件启动异常，走 extended firmware 的恢复/重刷路径。

同一切片器构建中的 ESP32 延时摄影盒子支持与混合口径固件补丁相互独立。它通过 Klipper 的 `ESP_TIMELAPSE_SHOT` 工作，不得写成依赖混合口径固件。

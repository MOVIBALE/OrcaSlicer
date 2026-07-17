# Snapmaker U1 混合口径打印

这个分支是 Snapmaker U1 的实验性混合口径打印工作流，用于在同一次打印中混用不同喷嘴直径。

已经真实打印验证的首个组合是：

- 逻辑 T0 / 喷嘴 1：0.4 mm，内墙和填充
- 逻辑 T1 / 喷嘴 2：0.2 mm，外墙
- 逻辑 T2/T3：可用，但示例工艺不参与对象挤出

实现不绑定 0.2/0.4 这一组。你可以在 U1 打印机面板里分别设置每个喷嘴直径，然后在准备页的 **Mixed Nozzle / 混合口径** 工作站里选择特征分配和层高合并策略。

## 模式

- `Same layer, different line widths`：同一层高内，外墙、内墙、填充可以使用不同逻辑喷嘴和线宽。
- `Mixed layer, different line widths`：外墙保持细层高，选中的内部特征由粗喷嘴跨多个细层合并打印。稳定默认只合并稀疏填充；内墙和内部实心填充需要显式打开实验选项。

必须配套刷入包含混合口径校验补丁的 U1 固件。原厂固件会把所有使用到的物理喷嘴都拿去和切片文件的第一个喷嘴直径比较，导致混合口径任务被拒绝。补丁会按 `extruder_map_table` 把逻辑 T 槽映射到实际物理头后逐个校验。

## 改了什么

- 新增 `outer_wall_filament`，让外墙可以独立于普通墙选择喷嘴。
- 外墙 G-code 路由到 `outer_wall_filament`，内墙和填充继续使用各自配置。
- U1 每个喷嘴直径可以独立编辑，喷嘴页签会显示当前直径。
- 支持从连接的 U1 同步四个物理头的耗材，并按 `extruder_map_table` 映射到切片器逻辑 T 槽。
- 新增混合口径工作站：快速方案、喷嘴映射、特征分配、层高合并、校验。
- 新增独立开关：
  - `mixed_nozzle_sparse_infill_combination`
  - `mixed_nozzle_inner_wall_combination`
  - `mixed_nozzle_internal_solid_infill_combination`
- 新增自动粗层高：默认取粗喷嘴直径的一半，并受粗喷嘴最大层高限制。
- 新增 `scripts/check_mixed_nozzle_gcode.py`，用于重复检查 G-code 里的 role/tool 是否正确。

## 使用步骤

1. 刷入包含混合口径校验补丁的 U1 固件。
2. 启动这个分支编译出的 Snapmaker Orca。
3. 选择 Snapmaker U1。
4. 在设备页连接打印机。
5. 在准备页耗材区域使用机器耗材同步按钮，同步四个物理头耗材。
6. 设置喷嘴 1 为 `0.4 mm`，喷嘴 2 为 `0.2 mm`。
7. 选择 `0.10 Mixed Layer Outer Nozzle2 Inner Nozzle1 @Snapmaker U1`。
8. 打开准备页里的 **Mixed Nozzle / 混合口径** 工作站。
9. 确认特征分配：
   - 外墙：slot 2 / T1 / 0.2 mm
   - 墙：slot 1 / T0 / 0.4 mm
   - 稀疏填充：slot 1 / T0 / 0.4 mm
   - 实心填充：slot 1 / T0 / 0.4 mm
10. 保守测试时只开启稀疏填充合并；内墙和内部实心填充合并属于实验选项。当前 0.10 示例工艺为了保持之前真实打印验证过的行为，会显式打开这些实验开关。
11. 先切一个简单立方体验证，再打印真实零件。

## G-code 验证

示例命令：

```powershell
python scripts\check_mixed_nozzle_gcode.py `
  --gcode path\to\plate_1.gcode `
  --outer-tool T1 --inner-tool T0 --sparse-infill-tool T0 --solid-infill-tool T0 `
  --forbid-object-tools T2,T3
```

期望结果：

- 外墙：T1 / 0.2 mm / 0.10 mm 层高
- 内墙：T0 / 0.4 mm / 0.20 mm 合并层
- 稀疏填充：T0 / 0.4 mm / 主要为 0.20 mm 合并层
- 内部实心填充：T0，根据重叠区域可能是 0.10 mm 或 0.20 mm
- T2/T3 不参与对象挤出

## 真实打印验证

2026-06-18，使用补丁版切片器和补丁版 U1 固件完成了一次真实打印验证。样件显示细外壳和较粗内部网格/填充可以在 U1 上完成混合口径打印。

![Snapmaker U1 混合口径真实打印成功样件](assets/real-print-cube.jpg)

## 风险

- 这是实验分支，不是生产稳定配置。
- 固件补丁只修正喷嘴直径校验，不保证流量、压力提前、擦嘴、换料、偏移和打印质量都已经安全。
- U1 多头偏移校准是喷嘴中心到中心。不同口径可以共用中心偏移，但线宽、搭接和首层接触仍要实物调。
- 稀疏填充跨层合并是保守默认；内墙和内部实心填充跨层合并仍需要更多模型验证。
- 复杂斜面、小岛、薄壁、孔洞、顶底面过渡都可能暴露 V1 切片逻辑问题。
- 刷机前保留可恢复的 U1 固件。

## 安装与恢复

1. 从本 fork 的 Experimental/Alpha Pre-release 下载 Snapmaker Orca 安装包和 SHA-256 文件。
2. 校验哈希，备份项目与自定义预设，再安装实验构建；不要先删除官方安装包。
3. 只有测试混合口径打印时才需要刷配套的混合口径固件。ESP32 延时摄影不依赖这个固件补丁。
4. 恢复官方切片器时，卸载实验构建并从 `Snapmaker/OrcaSlicer` Releases 安装官方版本。
5. 恢复打印机固件时，按当前 Extended Firmware 的恢复流程刷回已知可用的官方 U1 镜像。

这是独立社区 fork，不是 Snapmaker 官方发布。切片器继续使用 AGPL-3.0；Extended Firmware fork 使用 GPL-3.0。分发固件二进制时必须同时提供其精确对应源码和许可证声明。

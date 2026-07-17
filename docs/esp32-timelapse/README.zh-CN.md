# ESP32 延时摄影盒子

Snapmaker Orca 可通过 Klipper 宏 `ESP_TIMELAPSE_SHOT` 触发外接 ESP32
相机。启用延时摄影前，请先在 Klipper 中安装该宏，并使用已声明 ESP32 能力的
打印机预设。

最小 Klipper 宏契约：

```ini
[gcode_macro ESP_TIMELAPSE_SHOT]
variable_seq: 0
gcode:
    SET_GCODE_VARIABLE MACRO=ESP_TIMELAPSE_SHOT VARIABLE=seq VALUE={printer["gcode_macro ESP_TIMELAPSE_SHOT"].seq + 1}
```

ESP32 盒子轮询 `gcode_macro ESP_TIMELAPSE_SHOT.seq`。Snapmaker Orca 不直接连接相机、ESP32 串口，也不直接调用 Moonraker API。

打印机预设使用以下六个字段：

- `supports_esp32_timelapse`
- `esp32_timelapse_gcode`
- `esp32_timelapse_park_x`
- `esp32_timelapse_park_y`
- `esp32_timelapse_travel_speed`
- `esp32_timelapse_dwell_ms`

等待时间默认为 2000 ms，且不得低于该值；仍可配置更大的值。

- **关闭**：不输出原生或 ESP32 延时摄影命令。
- **传统模式**：保留原生拍摄，并在每个完整层结束后依次输出 `M400`、配置的
  ESP32 命令和配置的等待时间；不强制生成稳定塔。
- **平滑模式**：保留原生拍摄，原生与 ESP32 共用一座 Orca 稳定塔；ESP32
  拍摄前会安全抬升并停车。此模式会增加耗材和打印时间。

标准 Snapmaker U1 预设默认启用此能力并保留 `TIMELAPSE_TAKE_FRAME`，因此原生
拍摄与 ESP32 拍摄可以同时工作。通用 Klipper 预设可显式设置上述六个字段启用；
没有盒子或宏的预设必须保持 `supports_esp32_timelapse=false`。

本功能继续遵循仓库的 AGPL-3.0 许可证。

## 使用与验证

1. 安装宏并重启 Klipper。
2. 在打印机控制台手动执行 `ESP_TIMELAPSE_SHOT`，确认 `seq` 增加。
3. 在“工艺 > 其他 > 特殊模式 > 延时摄影”选择“关闭 / 传统模式 / 平滑模式”。
4. 首次打印前先导出 G-code：关闭模式不得出现 `ESP_TIMELAPSE_SHOT`；启用模式应当每个实际打印层一条；平滑模式还应在预览中显示一座共用稳定塔。
5. 先用 10-20 层单材料小模型观察首轮拍摄，再尝试长时间或多材料打印。

混合口径 U1 固件补丁与 ESP32 延时摄影无关，也不是它的依赖。要停用盒子，请选择“关闭”，或使用 `supports_esp32_timelapse=false` 的打印机预设。恢复官方切片器时，重新安装 `Snapmaker/OrcaSlicer` 的正式版本；安装或卸载切片器不会修改打印机或 ESP32 固件。

这是实验性社区集成，不是 Snapmaker、Klipper、Sony 或 Bambu Lab 官方功能。

# 固件文件说明（ESP32-S3，16MB Flash，分区 app3M_fat9M_16MB）

| 文件 | 用途 |
|------|------|
| **desktop_widget.ino.merged.bin** | **CI 与 Release 均会生成**：用 esptool 将 bootloader + 分区表 + 应用合并并 padding 到 16MB，**从 0x0 整片烧录**（量产 / 救砖 / 与 IDE「导出合并 bin」用途相同）。**Arduino IDE / `arduino-cli compile` 默认不会产出此文件名**，本地若看不到该文件属正常现象。 |
| **desktop_widget.ino.bin** | 仅应用程序，用于 **OTA** 或与 bootloader、partitions 分地址烧录。 |
| **desktop_widget.ino.bootloader.bin** | Bootloader。 |
| **desktop_widget.ino.partitions.bin** | 分区表。 |

分区表中 `app0` 起始地址为 **0x10000**，与合并命令中的偏移一致。

烧录工具可选用：乐鑫 **Flash Download Tools**、**esptool.py**、或 Arduino IDE **上传**（自动处理多文件）。

## 本地自己生成 merged（可选）

在已能编译出 `build-ci` 三件套的前提下，安装 `esptool`（4.x）后：

```bash
cd build-ci   # 或你的输出目录，内含三个 bin
python3 -m esptool --chip esp32s3 merge_bin -o desktop_widget.ino.merged.bin \
  --flash_mode dio --flash_freq 80m --flash_size 16MB --fill-flash-size 16MB \
  0x0 desktop_widget.ino.bootloader.bin \
  0x8000 desktop_widget.ino.partitions.bin \
  0x10000 desktop_widget.ino.bin
```

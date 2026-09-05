# 嵌入式开发工具

## 工具列表

### 1. serial_monitor.py
嵌入式串口监控工具。

**功能：**
- 列出可用串口
- 实时显示接收数据（ASCII/HEX 模式切换）
- 时间戳显示
- 发送数据（ASCII/HEX）
- 日志保存到文件
- 自动重连

**依赖：**
```bash
pip install pyserial
```

**用法：**
```bash
# 列出可用串口
python serial_monitor.py --list

# 连接串口
python serial_monitor.py -p COM3 -b 115200

# HEX 模式 + 日志保存
python serial_monitor.py -p /dev/ttyUSB0 -b 9600 --hex --log output.txt
```

**运行时命令：**
- `h` - 切换 HEX/ASCII 显示
- `t` - 切换时间戳
- `c` - 清屏
- `q` - 退出
- 其他输入 - 发送到串口

### 2. elf_analyzer.py
ELF 文件分析工具（嵌入式固件分析）。

**功能：**
- ELF 头部信息（架构、入口点、字节序）
- 段（Section）信息和大小统计
- 程序头（Program Header）信息
- Flash/RAM 使用量估算
- 符号表（函数/变量，按大小排序）
- 未使用符号查找
- 内存映射概览
- 生成完整分析报告

**依赖：**
```bash
pip install pyelftools
```

**用法：**
```bash
# 基本信息（头部 + 段 + 内存映射）
python elf_analyzer.py firmware.elf

# 显示符号表
python elf_analyzer.py firmware.elf --symbols

# 查找未使用符号
python elf_analyzer.py firmware.elf --unused

# 显示所有信息
python elf_analyzer.py firmware.elf --all

# 生成报告
python elf_analyzer.py firmware.elf --report report.txt
```

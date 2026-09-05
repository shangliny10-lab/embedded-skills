#!/usr/bin/env python3
"""
嵌入式串口监控工具
功能：
  - 列出可用串口
  - 连接串口并实时显示接收数据
  - 支持时间戳、HEX/ASCII 显示切换
  - 支持发送数据（ASCII/HEX）
  - 支持日志保存到文件
  - 自动重连

依赖：pip install pyserial
用法：
  python serial_monitor.py --list
  python serial_monitor.py -p COM3 -b 115200
  python serial_monitor.py -p /dev/ttyUSB0 -b 9600 --hex
  python serial_monitor.py -p COM3 -b 115200 --log output.txt
"""

import argparse
import sys
import time
import threading
from datetime import datetime

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("错误：未安装 pyserial，请运行: pip install pyserial")
    sys.exit(1)


class SerialMonitor:
    def __init__(self, port, baudrate, timeout=1, hex_mode=False,
                 show_timestamp=True, log_file=None, auto_reconnect=True):
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.hex_mode = hex_mode
        self.show_timestamp = show_timestamp
        self.log_file = log_file
        self.auto_reconnect = auto_reconnect
        self.ser = None
        self.running = False
        self.log_fp = None

        if log_file:
            self.log_fp = open(log_file, 'a', encoding='utf-8')
            print(f"日志保存到: {log_file}")

    def connect(self):
        """连接串口"""
        try:
            self.ser = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=self.timeout,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                bytesize=serial.EIGHTBITS
            )
            print(f"已连接 {self.port} @ {self.baudrate} bps")
            print("按 Ctrl+C 退出，输入 'h' 切换 HEX 模式，'t' 切换时间戳，'q' 退出")
            print("-" * 60)
            return True
        except serial.SerialException as e:
            print(f"连接失败: {e}")
            return False

    def disconnect(self):
        """断开连接"""
        if self.ser and self.ser.is_open:
            self.ser.close()
        if self.log_fp:
            self.log_fp.close()

    def format_data(self, data):
        """格式化接收数据"""
        timestamp = ""
        if self.show_timestamp:
            timestamp = f"[{datetime.now().strftime('%H:%M:%S.%f')[:-3]}] "

        if self.hex_mode:
            hex_str = ' '.join(f'{b:02X}' for b in data)
            ascii_str = ''.join(chr(b) if 32 <= b < 127 else '.' for b in data)
            return f"{timestamp}HEX: {hex_str}  | {ascii_str}"
        else:
            try:
                text = data.decode('utf-8', errors='replace')
                return f"{timestamp}{text}"
            except Exception:
                hex_str = ' '.join(f'{b:02X}' for b in data)
                return f"{timestamp}[二进制] {hex_str}"

    def read_thread(self):
        """读取线程"""
        while self.running:
            try:
                if self.ser and self.ser.is_open:
                    data = self.ser.read(256)
                    if data:
                        line = self.format_data(data)
                        # 不换行的情况处理
                        if not line.endswith('\n') and not self.hex_mode:
                            print(line, end='', flush=True)
                        else:
                            print(line, end='' if not self.hex_mode else '\n', flush=True)

                        if self.log_fp:
                            self.log_fp.write(line)
                            self.log_fp.flush()
                else:
                    time.sleep(0.1)
            except serial.SerialException as e:
                print(f"\n串口错误: {e}")
                if self.auto_reconnect:
                    print("尝试重连...")
                    self._reconnect()
                else:
                    self.running = False
                    break
            except Exception as e:
                print(f"\n读取错误: {e}")
                time.sleep(0.1)

    def _reconnect(self):
        """自动重连"""
        if self.ser:
            self.ser.close()
        time.sleep(1)
        while self.running:
            try:
                self.ser = serial.Serial(self.port, self.baudrate, timeout=self.timeout)
                print(f"\n重连成功 {self.port} @ {self.baudrate}")
                return
            except serial.SerialException:
                time.sleep(2)
                print(".", end='', flush=True)

    def send_data(self, text):
        """发送数据"""
        if not self.ser or not self.ser.is_open:
            print("未连接")
            return

        # HEX 发送模式：输入格式 "AA BB CC" 或 "AABBCC"
        if text.startswith('0x') or ' ' in text and all(len(p) <= 2 for p in text.split()):
            try:
                # 尝试解析为 HEX
                clean = text.replace('0x', '').replace(' ', '')
                data = bytes.fromhex(clean)
                self.ser.write(data)
                print(f"\n>> 发送 HEX: {' '.join(f'{b:02X}' for b in data)}")
                return
            except ValueError:
                pass

        # ASCII 发送
        data = (text + '\r\n').encode('utf-8')
        self.ser.write(data)
        print(f"\n>> 发送: {text}")

    def run(self):
        """运行监控"""
        if not self.connect():
            if self.auto_reconnect:
                print("等待设备连接...")
                self._reconnect()
            else:
                return

        self.running = True
        reader = threading.Thread(target=self.read_thread, daemon=True)
        reader.start()

        try:
            while self.running:
                cmd = input()
                if cmd.lower() in ('q', 'quit', 'exit'):
                    break
                elif cmd.lower() == 'h':
                    self.hex_mode = not self.hex_mode
                    print(f"\nHEX 模式: {'开启' if self.hex_mode else '关闭'}")
                elif cmd.lower() == 't':
                    self.show_timestamp = not self.show_timestamp
                    print(f"\n时间戳: {'开启' if self.show_timestamp else '关闭'}")
                elif cmd.lower() == 'c':
                    print("\n清屏")
                    print('\033[2J\033[H', end='')
                elif cmd:
                    self.send_data(cmd)
        except KeyboardInterrupt:
            pass
        finally:
            self.running = False
            reader.join(timeout=1)
            self.disconnect()
            print("\n已断开连接")


def list_serial_ports():
    """列出所有可用串口"""
    ports = list_ports.comports()
    if not ports:
        print("未找到可用串口")
        return

    print(f"{'端口':<20} {'描述':<40} {'硬件ID'}")
    print("-" * 90)
    for p in ports:
        print(f"{p.device:<20} {p.description:<40} {p.hwid}")


def main():
    parser = argparse.ArgumentParser(description='嵌入式串口监控工具')
    parser.add_argument('-p', '--port', help='串口名称 (如 COM3, /dev/ttyUSB0)')
    parser.add_argument('-b', '--baudrate', type=int, default=115200,
                        help='波特率 (默认: 115200)')
    parser.add_argument('--list', action='store_true', help='列出可用串口')
    parser.add_argument('--hex', action='store_true', help='以 HEX 模式显示')
    parser.add_argument('--no-timestamp', action='store_true', help='不显示时间戳')
    parser.add_argument('--log', help='保存日志到文件')
    parser.add_argument('--no-reconnect', action='store_true', help='禁用自动重连')

    args = parser.parse_args()

    if args.list:
        list_serial_ports()
        return

    if not args.port:
        print("错误：请指定串口 (-p)，或使用 --list 查看可用串口")
        parser.print_help()
        return

    monitor = SerialMonitor(
        port=args.port,
        baudrate=args.baudrate,
        hex_mode=args.hex,
        show_timestamp=not args.no_timestamp,
        log_file=args.log,
        auto_reconnect=not args.no_reconnect
    )
    monitor.run()


if __name__ == '__main__':
    main()

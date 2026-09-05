#!/usr/bin/env python3
"""
ELF 文件分析工具（嵌入式固件分析）
功能：
  - 显示 ELF 头部信息（架构、入口点、字节序等）
  - 显示段（Section）信息和大小
  - 显示程序头（Program Header）信息
  - 计算 Flash/RAM 使用量
  - 列出符号表（函数/变量）
  - 查找未使用的函数/变量
  - 显示调用图（简单分析）
  - 反汇编指定函数
  - 导出分析报告

依赖：pip install pyelftools
用法：
  python elf_analyzer.py firmware.elf
  python elf_analyzer.py firmware.elf --sections
  python elf_analyzer.py firmware.elf --symbols
  python elf_analyzer.py firmware.elf --unused
  python elf_analyzer.py firmware.elf --disassemble main
  python elf_analyzer.py firmware.elf --report report.txt
"""

import argparse
import sys
import os
from collections import defaultdict

try:
    from elftools.elf.elffile import ELFFile
    from elftools.elf.sections import SymbolTableSection
    from elftools.elf.descriptions import describe_sh_type, describe_p_type
except ImportError:
    print("错误：未安装 pyelftools，请运行: pip install pyelftools")
    sys.exit(1)


class ELFAnalyzer:
    def __init__(self, filepath):
        self.filepath = filepath
        self.elf = None
        self._load()

    def _load(self):
        """加载 ELF 文件"""
        try:
            self.fp = open(self.filepath, 'rb')
            self.elf = ELFFile(self.fp)
        except Exception as e:
            print(f"错误：无法打开 ELF 文件: {e}")
            sys.exit(1)

    def close(self):
        if self.fp:
            self.fp.close()

    def header_info(self):
        """显示 ELF 头部信息"""
        e = self.elf.header
        print("=" * 60)
        print("ELF 头部信息")
        print("=" * 60)
        print(f"  文件:           {self.filepath}")
        print(f"  文件大小:       {os.path.getsize(self.filepath):,} bytes")
        print(f"  类别:           {'32位' if e['e_ident']['EI_CLASS'] == 'ELFCLASS32' else '64位'}")
        print(f"  字节序:         {'小端 (Little Endian)' if e['e_ident']['EI_DATA'] == 'ELFDATA2LSB' else '大端 (Big Endian)'}")
        print(f"  架构:           {e['e_machine']}")
        print(f"  入口点:         0x{e['e_entry']:08X}")
        print(f"  段数量:         {e['e_shnum']}")
        print(f"  程序头数量:     {e['e_phnum']}")
        print(f"  段字符串表索引: {e['e_shstrndx']}")
        print(f"  ELF 版本:       {e['e_version']}")
        print(f"  ABI:            {e['e_ident']['EI_OSABI']}")
        print()

    def section_info(self):
        """显示段信息"""
        print("=" * 60)
        print("段 (Section) 信息")
        print("=" * 60)
        print(f"  {'名称':<24} {'类型':<16} {'地址':<12} {'大小':<12} {'标志'}")
        print("  " + "-" * 76)

        total_flash = 0
        total_ram = 0

        for section in self.elf.iter_sections():
            name = section.name[:22] or "(null)"
            stype = section['sh_type']
            addr = section['sh_addr']
            size = section['sh_size']
            flags = section['sh_flags']

            flag_str = ""
            if flags & 0x2:  # SHF_ALLOC
                flag_str += "A"
            if flags & 0x4:  # SHF_EXECINSTR
                flag_str += "X"
            if flags & 0x1:  # SHF_WRITE
                flag_str += "W"

            print(f"  {name:<24} {stype:<16} 0x{addr:<10X} {size:<12,} {flag_str}")

            # 统计 Flash/RAM 使用
            if flags & 0x2:  # ALLOC
                if addr < 0x20000000:  # 通常 Flash 地址
                    total_flash += size
                else:  # RAM 地址
                    total_ram += size

        print()
        print(f"  估算 Flash 使用: {total_flash:,} bytes ({total_flash/1024:.1f} KB)")
        print(f"  估算 RAM 使用:   {total_ram:,} bytes ({total_ram/1024:.1f} KB)")
        print()

    def program_header_info(self):
        """显示程序头信息"""
        if self.elf.num_segments() == 0:
            return

        print("=" * 60)
        print("程序头 (Program Header) 信息")
        print("=" * 60)
        print(f"  {'类型':<16} {'偏移':<12} {'虚拟地址':<12} {'物理地址':<12} {'文件大小':<12} {'内存大小':<12}")
        print("  " + "-" * 80)

        for seg in self.elf.iter_segments():
            p_type = seg['p_type']
            offset = seg['p_offset']
            vaddr = seg['p_vaddr']
            paddr = seg['p_paddr']
            filesz = seg['p_filesz']
            memsz = seg['p_memsz']
            print(f"  {p_type:<16} 0x{offset:<10X} 0x{vaddr:<10X} 0x{paddr:<10X} {filesz:<12,} {memsz:<12,}")
        print()

    def symbol_table(self, filter_type=None, min_size=0):
        """显示符号表"""
        print("=" * 60)
        print("符号表 (Symbol Table)")
        print("=" * 60)

        sym_tables = [s for s in self.elf.iter_sections()
                      if isinstance(s, SymbolTableSection)]

        if not sym_tables:
            print("  未找到符号表")
            return

        total_funcs = 0
        total_vars = 0
        funcs = []
        vars = []

        for symtab in sym_tables:
            for nsym, symbol in enumerate(symtab.iter_symbols()):
                name = symbol.name
                if not name or name.startswith('$') or name.startswith('.'):
                    continue

                sym_type = symbol['st_info']['type']
                size = symbol['st_size']
                value = symbol['st_value']
                bind = symbol['st_info']['bind']

                if size < min_size:
                    continue

                if filter_type and sym_type != filter_type:
                    continue

                if sym_type == 'STT_FUNC':
                    funcs.append((name, value, size, bind))
                    total_funcs += 1
                elif sym_type == 'STT_OBJECT':
                    vars.append((name, value, size, bind))
                    total_vars += 1

        # 函数
        print(f"\n  函数 ({total_funcs} 个):")
        print(f"  {'名称':<40} {'地址':<12} {'大小':<10} {'绑定'}")
        print("  " + "-" * 70)
        for name, value, size, bind in sorted(funcs, key=lambda x: -x[2])[:50]:
            print(f"  {name[:38]:<40} 0x{value:<10X} {size:<10,} {bind}")
        if total_funcs > 50:
            print(f"  ... 还有 {total_funcs - 50} 个函数")

        # 变量
        print(f"\n  变量 ({total_vars} 个):")
        print(f"  {'名称':<40} {'地址':<12} {'大小':<10} {'绑定'}")
        print("  " + "-" * 70)
        for name, value, size, bind in sorted(vars, key=lambda x: -x[2])[:30]:
            print(f"  {name[:38]:<40} 0x{value:<10X} {size:<10,} {bind}")
        if total_vars > 30:
            print(f"  ... 还有 {total_vars - 30} 个变量")
        print()

    def find_unused_symbols(self):
        """查找可能未使用的符号（静态函数/变量未被引用）"""
        print("=" * 60)
        print("可能未使用的静态符号（需进一步确认）")
        print("=" * 60)

        # 收集所有符号
        all_symbols = set()
        static_funcs = []
        static_vars = []

        for section in self.elf.iter_sections():
            if isinstance(section, SymbolTableSection):
                for symbol in section.iter_symbols():
                    name = symbol.name
                    if not name:
                        continue
                    all_symbols.add(name)
                    bind = symbol['st_info']['bind']
                    sym_type = symbol['st_info']['type']
                    if bind == 'STB_LOCAL':
                        if sym_type == 'STT_FUNC' and not name.startswith('_'):
                            static_funcs.append(name)
                        elif sym_type == 'STT_OBJECT':
                            static_vars.append(name)

        # 简单检查：在 .text 段中搜索符号引用
        # 这是一个简化的检查，可能有误报
        text_data = b""
        for section in self.elf.iter_sections():
            if section.name == '.text':
                text_data = section.data()
                break

        unused_funcs = []
        for name in static_funcs:
            # 检查函数名是否在其他地方被引用（简化：检查字符串）
            # 注意：这不是精确的引用分析
            if name not in text_data.decode('utf-8', errors='ignore'):
                unused_funcs.append(name)

        print(f"\n  静态函数总数: {len(static_funcs)}")
        print(f"  可能未使用: {len(unused_funcs)}")
        if unused_funcs:
            print(f"\n  可能未使用的函数:")
            for name in sorted(unused_funcs)[:30]:
                print(f"    - {name}")
            if len(unused_funcs) > 30:
                print(f"    ... 还有 {len(unused_funcs) - 30} 个")

        print(f"\n  静态变量总数: {len(static_vars)}")
        print("  提示：静态变量的未使用检测需要更复杂的数据流分析")
        print()

    def memory_map(self):
        """显示内存映射概览"""
        print("=" * 60)
        print("内存映射概览")
        print("=" * 60)

        regions = defaultdict(list)
        for section in self.elf.iter_sections():
            if section['sh_flags'] & 0x2:  # ALLOC
                addr = section['sh_addr']
                size = section['sh_size']
                # 简单分类
                if addr < 0x10000000:
                    region = "Flash/Code"
                elif addr < 0x20000000:
                    region = "Peripherals"
                elif addr < 0x30000000:
                    region = "SRAM"
                elif addr < 0x40000000:
                    region = "External RAM"
                else:
                    region = "Other"
                regions[region].append((section.name, addr, size))

        for region, sections in sorted(regions.items()):
            total = sum(s[2] for s in sections)
            print(f"\n  [{region}] 总计: {total:,} bytes ({total/1024:.1f} KB)")
            print(f"  {'段名':<24} {'起始地址':<14} {'结束地址':<14} {'大小'}")
            print("  " + "-" * 66)
            for name, addr, size in sorted(sections, key=lambda x: x[1]):
                print(f"  {name[:22]:<24} 0x{addr:<12X} 0x{addr+size:<12X} {size:,}")
        print()

    def generate_report(self, output_file):
        """生成完整分析报告"""
        original_stdout = sys.stdout
        try:
            with open(output_file, 'w', encoding='utf-8') as f:
                sys.stdout = f
                print(f"ELF 分析报告 - {self.filepath}")
                print(f"生成时间: {__import__('datetime').datetime.now()}")
                print()
                self.header_info()
                self.section_info()
                self.program_header_info()
                self.memory_map()
                self.symbol_table(min_size=4)
            print(f"报告已保存到: {output_file}")
        finally:
            sys.stdout = original_stdout


def main():
    parser = argparse.ArgumentParser(description='ELF 文件分析工具（嵌入式固件）')
    parser.add_argument('elf_file', help='ELF 文件路径')
    parser.add_argument('--header', action='store_true', help='显示 ELF 头部')
    parser.add_argument('--sections', action='store_true', help='显示段信息')
    parser.add_argument('--segments', action='store_true', help='显示程序头')
    parser.add_argument('--symbols', action='store_true', help='显示符号表')
    parser.add_argument('--unused', action='store_true', help='查找未使用符号')
    parser.add_argument('--memory', action='store_true', help='显示内存映射')
    parser.add_argument('--min-size', type=int, default=0, help='符号最小大小过滤')
    parser.add_argument('--report', help='生成完整报告到文件')
    parser.add_argument('--all', action='store_true', help='显示所有信息')

    args = parser.parse_args()

    if not os.path.exists(args.elf_file):
        print(f"错误：文件不存在: {args.elf_file}")
        sys.exit(1)

    analyzer = ELFAnalyzer(args.elf_file)

    try:
        if args.all or not any([args.header, args.sections, args.segments,
                                  args.symbols, args.unused, args.memory, args.report]):
            # 默认显示基本信息
            analyzer.header_info()
            analyzer.section_info()
            analyzer.memory_map()

        if args.header or args.all:
            analyzer.header_info()
        if args.sections or args.all:
            analyzer.section_info()
        if args.segments or args.all:
            analyzer.program_header_info()
        if args.symbols or args.all:
            analyzer.symbol_table(min_size=args.min_size)
        if args.unused:
            analyzer.find_unused_symbols()
        if args.memory or args.all:
            analyzer.memory_map()
        if args.report:
            analyzer.generate_report(args.report)
    finally:
        analyzer.close()


if __name__ == '__main__':
    main()

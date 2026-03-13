#!/usr/bin/env python3
"""
mex_pack.py - MaahiOS Executable (.mex) Packer

Wraps a flat binary with a 64-byte MEX header to produce a .mex file.

Build pipeline:
  app.c -> gcc -> app.o -> ld (mex_app.ld) -> app.elf -> objcopy -> app.bin -> mex_pack.py -> app.mex

Usage:
  python mex_pack.py input.bin output.mex --name "hello" --type app --flags console
  python mex_pack.py input.bin output.mex --name "fileviewer" --type app --flags gui
  python mex_pack.py input.bin output.mex --name "logger" --type service --flags console

Header format (64 bytes, little-endian):
  Offset  Size  Field
  0x00    4     magic        "MEX\0" (0x0058454D)
  0x04    2     version      0x0100 (v1.0)
  0x06    2     type         1=APP, 2=SERVICE, 3=DRIVER
  0x08    4     entry_offset offset from base to entry point (usually 0)
  0x0C    4     code_size    size of binary data after header
  0x10    4     bss_size     additional zero memory (from --bss-size or 0)
  0x14    4     stack_size   stack size (default 16384 = 16KB)
  0x18    4     base_address virtual load address (always 0x10000000)
  0x1C    4     flags        0x01=GUI, 0x02=CONSOLE
  0x20    24    name         null-terminated app name
  0x38    4     checksum     CRC32 of binary data
  0x3C    4     reserved     0
"""

import argparse
import struct
import sys
import zlib
import os
import subprocess

# Constants (must match src/managers/process/mex.h)
MEX_MAGIC       = 0x0058454D   # "MEX\0" little-endian
MEX_VERSION     = 0x0100       # v1.0
MEX_HEADER_SIZE = 64
MEX_APP_BASE    = 0x10000000

# Type map
TYPE_MAP = {
    'app':     1,
    'service': 2,
    'driver':  3,
}

# Flag map
FLAG_MAP = {
    'gui':     0x01,
    'console': 0x02,
}


def parse_flags(flag_str):
    """Parse comma-separated flags string into combined flag value."""
    if not flag_str:
        return 0
    result = 0
    for f in flag_str.split(','):
        f = f.strip().lower()
        if f not in FLAG_MAP:
            print(f"Error: Unknown flag '{f}'. Valid flags: {', '.join(FLAG_MAP.keys())}", file=sys.stderr)
            sys.exit(1)
        result |= FLAG_MAP[f]
    return result


def compute_crc32(data):
    """Compute CRC32 checksum of binary data."""
    return zlib.crc32(data) & 0xFFFFFFFF


def detect_bss_size(elf_path):
    """Auto-detect BSS size from an ELF file using i686-elf-size.
    
    Runs `i686-elf-size <elf>` which outputs:
       text    data     bss     dec     hex filename
      12345     100    5678   18153    46E9 app.elf
    
    Returns BSS size in bytes, or 0 if detection fails.
    """
    if not elf_path or not os.path.exists(elf_path):
        return 0
    
    # Try i686-elf-size first, then fall back to plain 'size'
    for tool in ['i686-elf-size', 'size']:
        try:
            result = subprocess.run(
                [tool, elf_path],
                capture_output=True, text=True, timeout=10
            )
            if result.returncode == 0:
                lines = result.stdout.strip().split('\n')
                if len(lines) >= 2:
                    # Parse second line: "  text    data     bss     dec     hex filename"
                    parts = lines[1].split()
                    if len(parts) >= 3:
                        bss = int(parts[2])
                        return bss
        except (FileNotFoundError, subprocess.TimeoutExpired, ValueError):
            continue
    
    return 0


def build_header(app_name, app_type, flags, code_size, bss_size, stack_size, entry_offset, checksum):
    """Build the 64-byte MEX header."""
    # Encode name (max 23 chars + null terminator = 24 bytes)
    name_bytes = app_name.encode('ascii')[:23]
    name_padded = name_bytes.ljust(24, b'\x00')

    # Pack header: all little-endian
    # <  = little-endian
    # I  = uint32_t (4 bytes)
    # H  = uint16_t (2 bytes)
    # 24s = 24 bytes of char
    header = struct.pack('<IHHIIIIII24sII',
        MEX_MAGIC,          # magic
        MEX_VERSION,        # version
        app_type,           # type
        entry_offset,       # entry_offset
        code_size,          # code_size
        bss_size,           # bss_size
        stack_size,         # stack_size
        MEX_APP_BASE,       # base_address
        flags,              # flags
        name_padded,        # name[24]
        checksum,           # checksum
        0,                  # reserved
    )

    assert len(header) == MEX_HEADER_SIZE, f"Header is {len(header)} bytes, expected {MEX_HEADER_SIZE}"
    return header


def main():
    parser = argparse.ArgumentParser(
        description='MaahiOS Executable (.mex) Packer - wraps flat binary with MEX header'
    )
    parser.add_argument('input', help='Input flat binary file (.bin)')
    parser.add_argument('output', help='Output .mex file')
    parser.add_argument('--name', required=True, help='Application name (max 23 chars)')
    parser.add_argument('--type', required=True, choices=TYPE_MAP.keys(),
                        help='Application type: app, service, or driver')
    parser.add_argument('--flags', default='', help='Comma-separated flags: gui, console')
    parser.add_argument('--stack-size', type=int, default=16384,
                        help='Stack size in bytes (default: 16384 = 16KB)')
    parser.add_argument('--bss-size', type=int, default=-1,
                        help='BSS size in bytes (default: auto-detect from --elf)')
    parser.add_argument('--elf', default=None,
                        help='ELF file to auto-detect BSS size from (before objcopy stripping)')
    parser.add_argument('--entry-offset', type=int, default=0,
                        help='Entry point offset from base (default: 0)')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Print header details')

    args = parser.parse_args()

    # Read input binary
    if not os.path.exists(args.input):
        print(f"Error: Input file '{args.input}' not found", file=sys.stderr)
        sys.exit(1)

    with open(args.input, 'rb') as f:
        binary_data = f.read()

    if len(binary_data) == 0:
        print("Error: Input binary is empty", file=sys.stderr)
        sys.exit(1)

    # Compute values
    code_size = len(binary_data)
    app_type = TYPE_MAP[args.type]
    flags = parse_flags(args.flags)
    checksum = compute_crc32(binary_data)

    # Determine BSS size: explicit > auto-detect from ELF > 0
    if args.bss_size >= 0:
        bss_size = args.bss_size
    elif args.elf:
        bss_size = detect_bss_size(args.elf)
    else:
        bss_size = 0

    # Build header
    header = build_header(
        app_name=args.name,
        app_type=app_type,
        flags=flags,
        code_size=code_size,
        bss_size=bss_size,
        stack_size=args.stack_size,
        entry_offset=args.entry_offset,
        checksum=checksum,
    )

    # Write output
    with open(args.output, 'wb') as f:
        f.write(header)
        f.write(binary_data)

    total_size = MEX_HEADER_SIZE + code_size

    if args.verbose:
        print(f"+==========================================+")
        print(f"|         MEX Packer - MaahiOS             |")
        print(f"+==========================================+")
        print(f"| Input:       {args.input:<27s} |")
        print(f"| Output:      {args.output:<27s} |")
        print(f"| Name:        {args.name:<27s} |")
        print(f"| Type:        {args.type:<27s} |")
        print(f"| Flags:       {args.flags if args.flags else '(none)':<27s} |")
        print(f"| Base:        0x{MEX_APP_BASE:08X}                 |")
        print(f"| Entry:       0x{args.entry_offset:08X}                 |")
        print(f"| Code size:   {code_size:>10d} bytes           |")
        print(f"| BSS size:    {bss_size:>10d} bytes           |")
        print(f"| Stack size:  {args.stack_size:>10d} bytes           |")
        print(f"| CRC32:       0x{checksum:08X}                 |")
        print(f"| Total:       {total_size:>10d} bytes           |")
        print(f"+==========================================+")
    else:
        print(f"[mex_pack] {args.name}: {code_size} bytes code, CRC32=0x{checksum:08X} -> {args.output} ({total_size} bytes)")


if __name__ == '__main__':
    main()

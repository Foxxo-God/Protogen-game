#!/usr/bin/env python3
"""Build a small ARMv4T game object into a standalone Game Boy Advance ROM."""

import os
import struct
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "src" / "game.c"
OUTPUT = ROOT / "build" / "protogen-frontier.gba"
ROM_BASE = 0x08000000
ROM_CODE_OFFSET = 0xC0
BSS_BASE = 0x02000000
GBA_BOOT_LOGO = bytes.fromhex(
    "24ffae51699aa2213d84820a84e409ad11248b98c0817f21"
    "a352be199309ce2010464a4af82731ec58c7e83382e3cebf"
    "85f4df94ce4b09c194568ac01372a7fc9f844d73a3ca9a61"
    "5897a327fc039876231dc7610304ae56bf38840040a70efd"
    "ff52fe036f9530f197fbc08560d68025a963be03014e38e2"
    "f9a234ffbb3e0344780090cb88113a9465c07c6387f03caf"
    "d625e48b380aac7221d4f807"
)


def unpack_from(fmt, data, offset):
    return struct.unpack_from("<" + fmt, data, offset)


def read_elf(path):
    data = path.read_bytes()
    if data[:7] != b"\x7fELF\x01\x01\x01":
        raise ValueError("Clang did not produce a little-endian ELF32 object")
    header = unpack_from("HHIIIIIHHHHHH", data, 16)
    if header[1] != 40:
        raise ValueError("The game object is not an ARM object file")
    section_offset, section_entry_size, section_count, names_index = header[5], header[10], header[11], header[12]
    sections = []
    for index in range(section_count):
        values = unpack_from("IIIIIIIIII", data, section_offset + index * section_entry_size)
        sections.append({
            "name_offset": values[0], "type": values[1], "flags": values[2],
            "offset": values[4], "size": values[5], "link": values[6],
            "info": values[7], "align": max(values[8], 1), "entry_size": values[9],
        })
    names_section = sections[names_index]
    names_data = data[names_section["offset"]:names_section["offset"] + names_section["size"]]

    def c_string(blob, offset):
        end = blob.find(b"\0", offset)
        return blob[offset:end].decode("ascii")

    for section in sections:
        section["name"] = c_string(names_data, section["name_offset"])
        if section["type"] == 8:
            section["data"] = bytearray(section["size"])
        else:
            start = section["offset"]
            section["data"] = bytearray(data[start:start + section["size"]])

    return data, sections, c_string


def align(value, boundary):
    return (value + boundary - 1) & ~(boundary - 1)


def build_rom(object_path):
    if len(GBA_BOOT_LOGO) != 0x9C:
        raise ValueError("The GBA cartridge logo field must be exactly 156 bytes")
    data, sections, c_string = read_elf(object_path)
    placements = {}
    rom_sections = [s for s in sections if s["name"] == ".text.startup"]
    rom_sections += [s for s in sections if s["name"] == ".text"]
    rom_sections += sorted(
        (s for s in sections if s["name"].startswith((".text.", ".rodata", ".data"))
         and s["name"] != ".text.startup"), key=lambda item: item["name"]
    )
    code = bytearray()
    for section in rom_sections:
        offset = align(len(code), section["align"])
        code.extend(bytes(offset - len(code)))
        placements[sections.index(section)] = (ROM_BASE + ROM_CODE_OFFSET + offset, offset)
        code.extend(section["data"])

    bss_sections = [s for s in sections if s["type"] == 8 and s["name"].startswith((".bss", ".sbss"))]
    bss_cursor = BSS_BASE
    for section in sorted(bss_sections, key=lambda item: item["name"]):
        bss_cursor = align(bss_cursor, section["align"])
        placements[sections.index(section)] = (bss_cursor, 0)
        bss_cursor += section["size"]
    bss_end = align(bss_cursor, 4)

    symbol_tables = {}
    symbol_addresses = {}
    for index, section in enumerate(sections):
        if section["type"] != 2:
            continue
        strings_section = sections[section["link"]]
        strings = strings_section["data"]
        symbols = []
        for offset in range(0, section["size"], section["entry_size"]):
            name_offset, value, size, info, other, section_index = unpack_from(
                "IIIBBH", section["data"], offset
            )
            name = c_string(strings, name_offset) if name_offset else ""
            symbols.append((name, value, size, info, section_index))
            if section_index in placements:
                symbol_addresses[(index, offset // section["entry_size"])] = placements[section_index][0] + value
            elif section_index == 0xFFF1:
                symbol_addresses[(index, offset // section["entry_size"])] = value
        symbol_tables[index] = symbols

    for table_index, symbols in symbol_tables.items():
        for symbol_index, symbol in enumerate(symbols):
            if symbol[0] == "__bss_start__":
                symbol_addresses[(table_index, symbol_index)] = BSS_BASE
            elif symbol[0] == "__bss_end__":
                symbol_addresses[(table_index, symbol_index)] = bss_end

    for relocation_section in sections:
        if relocation_section["type"] != 9:
            continue
        target_section_index = relocation_section["info"]
        if target_section_index not in placements:
            continue
        target_address, target_offset = placements[target_section_index]
        table_index = relocation_section["link"]
        symbols = symbol_tables[table_index]
        target_bytes = sections[target_section_index]["data"]
        for offset in range(0, relocation_section["size"], relocation_section["entry_size"]):
            place_offset, info = unpack_from("II", relocation_section["data"], offset)
            symbol_index, kind = info >> 8, info & 0xFF
            if kind == 42:
                continue
            if (table_index, symbol_index) not in symbol_addresses:
                name = symbols[symbol_index][0]
                raise ValueError(f"Unresolved ROM symbol: {name or symbol_index}")
            target = symbol_addresses[(table_index, symbol_index)]
            addend = struct.unpack_from("<I", target_bytes, place_offset)[0]
            place = target_address + place_offset
            if kind == 2:
                value = (target + addend) & 0xFFFFFFFF
                struct.pack_into("<I", target_bytes, place_offset, value)
            elif kind == 28:
                instruction = addend
                branch_addend = (instruction & 0x00FFFFFF)
                if branch_addend & 0x00800000:
                    branch_addend -= 0x01000000
                branch_addend <<= 2
                displacement = target + branch_addend - place
                if displacement & 3 or not -(1 << 25) <= displacement < (1 << 25):
                    raise ValueError("ARM branch target is misaligned or out of range")
                instruction = (instruction & 0xFF000000) | ((displacement >> 2) & 0x00FFFFFF)
                struct.pack_into("<I", target_bytes, place_offset, instruction)
            else:
                raise ValueError(f"Unsupported ARM relocation: {kind}")

    for section_index, (address, offset) in placements.items():
        if sections[section_index]["type"] == 8:
            continue
        code[offset:offset + len(sections[section_index]["data"])] = sections[section_index]["data"]

    rom = bytearray(ROM_CODE_OFFSET)
    struct.pack_into("<I", rom, 0, 0xEA00002E)
    rom[0x04:0xA0] = GBA_BOOT_LOGO
    rom[0xA0:0xAC] = b"PROTOGEN FRT"
    rom[0xAC:0xB0] = b"PGEN"
    rom[0xB0:0xB2] = b"00"
    rom[0xB2] = 0x96
    rom[0xB3] = 0
    rom[0xBC] = 0
    rom[0xBD] = (-sum(rom[0xA0:0xBD]) - 0x19) & 0xFF
    rom.extend(code)
    rom.extend(bytes([0xFF]) * (align(max(len(rom), 32768), 4) - len(rom)))
    return rom


def main():
    compiler = os.environ.get("CLANG", "clang")
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="protogen-gba-") as temp_dir:
        object_path = Path(temp_dir) / "game.o"
        command = [
            compiler, "-target", "armv4t-none-eabi", "-mcpu=arm7tdmi", "-marm",
            "-std=c11", "-O2", "-ffreestanding", "-fno-builtin", "-fno-pic",
            "-fno-pie", "-fno-stack-protector", "-fno-unwind-tables",
            "-fno-asynchronous-unwind-tables", "-fno-function-sections",
            "-c", str(SOURCE), "-o", str(object_path),
        ]
        subprocess.run(command, check=True)
        rom = build_rom(object_path)
    OUTPUT.write_bytes(rom)
    print(f"Built {OUTPUT.relative_to(ROOT)} ({len(rom):,} bytes)")


if __name__ == "__main__":
    main()
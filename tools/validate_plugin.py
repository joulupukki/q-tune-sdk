#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Boyd Timothy
"""
validate_plugin.py — offline pre-flight check for a Q-Tune plugin .so.

Runs the same checks the firmware ELF loader performs, on your host, BEFORE you
upload — so you fail fast instead of discovering a problem at boot:

  * the file is a well-formed shared object,
  * it exports the `qtune_plugin_descriptor` symbol (default visibility),
  * its abi_version matches this SDK,
  * its LVGL major.minor matches this SDK,
  * its descriptor type is TUNER or STANDBY,
  * every undefined `lv_*` symbol it references is in the exported allowlist
    (docs/ALLOWED_SYMBOLS.md) — this catches "called an LVGL function the
    firmware does not export".

Note: the plugin's numeric ID comes from get_id() (a function), so it cannot be
read statically; the firmware enforces the reserved ID range at load time.

Usage:
    python3 tools/validate_plugin.py build/my_plugin.so
    python3 tools/validate_plugin.py --strict build/my_plugin.so   # warnings fail too

Requires: pyelftools  (pip install pyelftools)
"""

import argparse
import pathlib
import re
import struct
import sys

try:
    from elftools.elf.elffile import ELFFile
    from elftools.elf.sections import SymbolTableSection
except ImportError:
    sys.stderr.write(
        "error: pyelftools is required. Install it with:\n"
        "    pip install pyelftools\n")
    sys.exit(2)

SDK_ROOT = pathlib.Path(__file__).resolve().parent.parent
ABI_HEADER = SDK_ROOT / "include" / "qtune_plugin_abi.h"
ALLOWED_DOC = SDK_ROOT / "docs" / "ALLOWED_SYMBOLS.md"

# LVGL major.minor this SDK targets. Kept in step with the firmware by
# tools/sync_sdk.py at release time; override on the CLI with --lvgl if needed.
EXPECTED_LVGL = (9, 2)

DESCRIPTOR_SYMBOL = "qtune_plugin_descriptor"   # data object (validator reads it)
ENTRY_SYMBOL = "qtune_plugin_entry"             # function (the loader calls it)
TYPE_NAMES = {1: "TUNER", 2: "STANDBY"}

# Symbols the espressif/elf_loader component itself exports (its built-in libc /
# libm / lwIP / pthread table), in addition to the firmware's curated table in
# ALLOWED_SYMBOLS.md. A plugin can resolve a symbol iff it is in the UNION of
# these two sets. This list is from elf_loader >= 1.3.x (src/esp_elf_symbol.c).
# NOTE: it deliberately does NOT include snprintf or any <math.h> functions —
# those are provided by the firmware's curated table, so a plugin that uses them
# only loads on firmware new enough to export them.
ELF_LOADER_BUILTINS = {
    "calloc", "clock_gettime", "close", "ets_printf", "exit", "fprintf",
    "fputc", "fputs", "free", "fwrite", "getopt_long", "ip4addr_ntoa",
    "ipaddr_addr", "longjmp", "malloc", "memcpy", "memset", "optarg", "opterr",
    "optind", "optopt", "printf", "putchar", "puts", "realloc", "setjmp",
    "sleep", "strchr", "strcmp", "strcspn", "strerror", "strftime", "strlen",
    "strncat", "strrchr", "strtod", "strtol", "usleep", "vfprintf", "_ctype_",
    "lwip_accept", "lwip_bind", "lwip_connect", "lwip_htonl", "lwip_htons",
    "lwip_listen", "lwip_recv", "lwip_recvfrom", "lwip_send", "lwip_sendto",
    "lwip_setsockopt", "lwip_socket", "pthread_attr_init",
    "pthread_attr_setstacksize", "pthread_create", "pthread_detach",
    "pthread_exit", "pthread_join",
}


def read_expected_abi() -> int:
    if not ABI_HEADER.exists():
        return 1
    m = re.search(r"#define\s+QTUNE_PLUGIN_ABI_VERSION\s+(\d+)",
                  ABI_HEADER.read_text())
    return int(m.group(1)) if m else 1


def read_allowlist() -> set:
    """Pull every `identifier` inline-code token out of ALLOWED_SYMBOLS.md."""
    if not ALLOWED_DOC.exists():
        sys.stderr.write(
            f"warning: {ALLOWED_DOC} not found — skipping symbol allowlist "
            "check (generate it with tools/gen_plugin_symbols_doc.py).\n")
        return set()
    text = ALLOWED_DOC.read_text()
    return set(re.findall(r"`([A-Za-z_][A-Za-z0-9_]*)`", text))


def find_symbol(elf, name):
    """Return (symbol, section) for `name`, searching dynsym then symtab."""
    for sec_name in (".dynsym", ".symtab"):
        sec = elf.get_section_by_name(sec_name)
        if not isinstance(sec, SymbolTableSection):
            continue
        for sym in sec.iter_symbols():
            if sym.name == name:
                return sym
    return None


def read_descriptor(elf, sym):
    """Read abi_version, lvgl_version, type from the descriptor's section data.

    st_shndx is unreliable after --strip-all (section indices shift), so locate
    the containing section by virtual-address range using st_value — the same
    address the dynamic loader resolves.
    """
    if sym["st_shndx"] == "SHN_UNDEF":
        return None
    value = sym["st_value"]
    endian = "<" if elf.little_endian else ">"
    for section in elf.iter_sections():
        addr, size = section["sh_addr"], section["sh_size"]
        if not addr or section["sh_type"] == "SHT_NOBITS":
            continue
        if addr <= value < addr + size:
            data = section.data()[value - addr:value - addr + 12]
            if len(data) < 12:
                return None
            abi, lvgl, typ = struct.unpack(endian + "III", data)
            return abi, lvgl, typ
    return None


def undefined_dynsyms(elf):
    sec = elf.get_section_by_name(".dynsym")
    if not isinstance(sec, SymbolTableSection):
        return []
    out = []
    for sym in sec.iter_symbols():
        if sym.name and sym["st_shndx"] == "SHN_UNDEF":
            out.append(sym.name)
    return sorted(set(out))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("so", type=pathlib.Path, help="path to the plugin .so")
    ap.add_argument("--strict", action="store_true",
                    help="treat warnings as failures")
    ap.add_argument("--lvgl", metavar="MAJ.MIN",
                    help="expected LVGL major.minor (default %d.%d)" % EXPECTED_LVGL)
    args = ap.parse_args()

    expected_lvgl = EXPECTED_LVGL
    if args.lvgl:
        maj, _, minr = args.lvgl.partition(".")
        expected_lvgl = (int(maj), int(minr or 0))

    if not args.so.exists():
        print(f"FAIL: file not found: {args.so}")
        return 1

    errors, warnings = [], []

    with args.so.open("rb") as fh:
        try:
            elf = ELFFile(fh)
        except Exception as exc:  # noqa: BLE001
            print(f"FAIL: not a valid ELF file: {exc}")
            return 1

        if elf["e_type"] not in ("ET_DYN",):
            errors.append(f"ELF type is {elf['e_type']}, expected ET_DYN "
                          "(shared object). Did the build link with -shared?")

        # The loader resolves the descriptor by calling the entry FUNCTION, so it
        # must be present in .dynsym (the data descriptor symbol is invisible to
        # the loader's function-only dlsym).
        dynsym = elf.get_section_by_name(".dynsym")
        entry = isinstance(dynsym, SymbolTableSection) and any(
            s.name == ENTRY_SYMBOL and s.entry.st_info.type == "STT_FUNC"
            for s in dynsym.iter_symbols())
        if not entry:
            errors.append(
                f"missing exported function '{ENTRY_SYMBOL}'. Every plugin must "
                f"export `const QTunePluginDescriptor *{ENTRY_SYMBOL}(void)` "
                "(the ELF loader's dlsym resolves only functions, not the "
                "descriptor data object).")

        sym = find_symbol(elf, DESCRIPTOR_SYMBOL)
        if sym is None:
            errors.append(
                f"missing exported symbol '{DESCRIPTOR_SYMBOL}'. Declare the "
                "descriptor with QTUNE_PLUGIN_EXPORT inside extern \"C\".")
        else:
            desc = read_descriptor(elf, sym)
            if desc is None:
                warnings.append("could not read descriptor bytes statically; "
                                "version/type checks skipped.")
            else:
                abi, lvgl, typ = desc
                expected_abi = read_expected_abi()
                if abi != expected_abi:
                    errors.append(f"abi_version is {abi}, firmware expects "
                                  f"{expected_abi}.")
                plv = (lvgl >> 16 & 0xFF, lvgl >> 8 & 0xFF, lvgl & 0xFF)
                if (plv[0], plv[1]) != expected_lvgl:
                    errors.append(
                        f"built against LVGL {plv[0]}.{plv[1]}.{plv[2]}, this "
                        f"SDK targets {expected_lvgl[0]}.{expected_lvgl[1]}.x. "
                        "Pin lvgl/lvgl in idf_component.yml.")
                if typ not in TYPE_NAMES:
                    errors.append(f"descriptor type is {typ}, expected 1 "
                                  "(TUNER) or 2 (STANDBY).")
                else:
                    print(f"  type:         {TYPE_NAMES[typ]}")
                    print(f"  abi_version:  {abi}")
                    print(f"  lvgl_version: {plv[0]}.{plv[1]}.{plv[2]}")

        # Undefined-symbol check: a symbol resolves iff it is in the firmware's
        # curated table (ALLOWED_SYMBOLS.md) or elf_loader's built-in table.
        resolvable = read_allowlist() | ELF_LOADER_BUILTINS
        for name in undefined_dynsyms(elf):
            if name == DESCRIPTOR_SYMBOL or name in resolvable:
                continue
            if name.startswith("__"):
                continue  # compiler runtime (libgcc) — resolved by the loader
            if name.startswith("lv_"):
                errors.append(
                    f"references unexported LVGL symbol '{name}' — not in "
                    "docs/ALLOWED_SYMBOLS.md. The plugin will fail to load.")
            else:
                errors.append(
                    f"references '{name}', which the firmware does not export "
                    "(not in docs/ALLOWED_SYMBOLS.md nor elf_loader's libc). The "
                    "plugin will fail to load — request it be added to the "
                    "firmware's qtune_plugin_symbols.txt.")

    for w in warnings:
        print(f"  WARN: {w}")
    for e in errors:
        print(f"  ERROR: {e}")

    failed = bool(errors) or (args.strict and warnings)
    print()
    if failed:
        print(f"FAIL: {args.so.name} — "
              f"{len(errors)} error(s), {len(warnings)} warning(s)")
        return 1
    print(f"OK: {args.so.name} passed "
          f"({len(warnings)} warning(s))")
    return 0


if __name__ == "__main__":
    sys.exit(main())

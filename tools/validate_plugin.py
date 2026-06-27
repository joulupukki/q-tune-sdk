#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Boyd Timothy
"""
validate_plugin.py — offline pre-flight check for a Q-Tune plugin .so.

Runs the same checks the firmware ELF loader performs, on your host, BEFORE you
upload — so you fail fast instead of discovering a problem at boot:

  * the file is a well-formed shared object,
  * it exports the `qtune_plugin_descriptor` symbol (default visibility),
  * its ABI major matches this SDK and its ABI minor is <= this SDK's,
  * its LVGL major.minor matches this SDK,
  * its descriptor type is TUNER or STANDBY,
  * its descriptor carries a stable `uid` string — present, non-empty, and not a
    bare integer (bare integers are reserved for built-in UIs),
  * every undefined `lv_*` symbol it references is in the exported allowlist
    (docs/ALLOWED_SYMBOLS.md) — this catches "called an LVGL function the
    firmware does not export".

Note: a plugin no longer chooses a numeric ID — the firmware assigns one
dynamically at load time. The plugin's permanent identity is the descriptor's
`uid` string, which this validator reads statically from the .so (below).

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
        "error: pyelftools is required.\n"
        "  - It already ships with ESP-IDF: run this from a shell where you've\n"
        "    sourced esp-idf/export.sh, and no install is needed.\n"
        "  - Otherwise install it for python3 (not 'pip', which may be Python 2):\n"
        "        python3 -m pip install --user pyelftools\n"
        "    or in a venv:  python3 -m venv .venv && . .venv/bin/activate && pip install pyelftools\n"
        "  - Or just run the validator inside the build container (it has pyelftools).\n")
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
    """Packed major.minor ABI the SDK targets (major << 16 | minor)."""
    major, minor = 1, 0
    if ABI_HEADER.exists():
        text = ABI_HEADER.read_text()
        mj = re.search(r"#define\s+QTUNE_PLUGIN_ABI_MAJOR\s+(\d+)", text)
        mn = re.search(r"#define\s+QTUNE_PLUGIN_ABI_MINOR\s+(\d+)", text)
        if mj:
            major = int(mj.group(1))
        if mn:
            minor = int(mn.group(1))
    return (major << 16) | minor


def read_allowlist() -> set:
    """Pull every `identifier` inline-code token out of ALLOWED_SYMBOLS.md.

    The allowlist is the heart of the symbol check; validating without it would
    silently pass a plugin that calls unexported functions (and then fails to
    load on the device). A missing allowlist is therefore a hard configuration
    error, not a skippable warning.
    """
    if not ALLOWED_DOC.exists():
        sys.stderr.write(
            f"error: {ALLOWED_DOC} not found — cannot run the symbol allowlist "
            "check. This file ships with the SDK; run the validator from a "
            "complete q-tune-sdk checkout (or regenerate it with "
            "tools/gen_plugin_symbols_doc.py).\n")
        sys.exit(2)
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


def _read_vaddr(elf, vaddr, nbytes):
    """Read `nbytes` from the loadable section containing virtual address `vaddr`.

    st_shndx is unreliable after --strip-all (section indices shift), so locate
    the containing section by virtual-address range — the same address the
    dynamic loader resolves. Returns the bytes, or None if unmapped/out of range.
    """
    for section in elf.iter_sections():
        addr, size = section["sh_addr"], section["sh_size"]
        if not addr or section["sh_type"] == "SHT_NOBITS":
            continue
        if addr <= vaddr < addr + size:
            off = vaddr - addr
            data = section.data()[off:off + nbytes]
            return data if len(data) == nbytes else None
    return None


def _read_cstring(elf, vaddr, maxlen=256):
    """Resolve a pointer vaddr to its NUL-terminated string in the .so."""
    for section in elf.iter_sections():
        addr, size = section["sh_addr"], section["sh_size"]
        if not addr or section["sh_type"] == "SHT_NOBITS":
            continue
        if addr <= vaddr < addr + size:
            blob = section.data()[vaddr - addr:vaddr - addr + maxlen]
            nul = blob.find(b"\x00")
            if nul >= 0:
                blob = blob[:nul]
            try:
                return blob.decode("utf-8", "replace")
            except Exception:  # noqa: BLE001
                return None
    return None


def read_descriptor(elf, sym):
    """Read the descriptor fields from its section data.

    Layout: abi_version(u32), lvgl_version(u32), type(u32), sdk_build(ptr),
    interface(ptr), uid(ptr). The three u32s sit at offsets 0/4/8; the uid
    pointer is the last field. Returns (abi, lvgl, type, uid_str) where uid_str
    is the resolved string, "" if the uid pointer is NULL, or None if the uid
    pointer could not be resolved to a string.
    """
    if sym["st_shndx"] == "SHN_UNDEF":
        return None
    value = sym["st_value"]
    endian = "<" if elf.little_endian else ">"
    ptr_size = 8 if elf.elfclass == 64 else 4
    pfmt = "Q" if ptr_size == 8 else "I"

    head = _read_vaddr(elf, value, 12)
    if head is None:
        return None
    abi, lvgl, typ = struct.unpack(endian + "III", head)

    # uid is the final pointer: after 3 u32s + sdk_build ptr + interface ptr,
    # honouring pointer alignment (the first pointer follows the 12-byte u32 head,
    # padded up to ptr_size).
    first_ptr_off = (12 + ptr_size - 1) & ~(ptr_size - 1)
    uid_ptr_off = first_ptr_off + 2 * ptr_size  # skip sdk_build, interface
    raw = _read_vaddr(elf, value + uid_ptr_off, ptr_size)
    uid_str = None
    if raw is not None:
        (uid_vaddr,) = struct.unpack(endian + pfmt, raw)
        if uid_vaddr == 0:
            uid_str = ""        # NULL pointer
        else:
            uid_str = _read_cstring(elf, uid_vaddr)
    return abi, lvgl, typ, uid_str


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
        m = re.fullmatch(r"(\d+)(?:\.(\d+))?", args.lvgl.strip())
        if not m:
            print(f"error: --lvgl must be MAJ or MAJ.MIN (e.g. 9.2), got "
                  f"'{args.lvgl}'.", file=sys.stderr)
            return 2
        expected_lvgl = (int(m.group(1)), int(m.group(2) or 0))

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
                abi, lvgl, typ, uid = desc
                expected_abi = read_expected_abi()
                p_major, p_minor = abi >> 16, abi & 0xFFFF
                e_major, e_minor = expected_abi >> 16, expected_abi & 0xFFFF
                # Loader rule: same major, and the plugin's minor must not exceed
                # the firmware's (additive minors are backward-compatible).
                if p_major != e_major or p_minor > e_minor:
                    errors.append(
                        f"abi_version is {p_major}.{p_minor}, firmware provides "
                        f"{e_major}.{e_minor} (needs same major, minor <= firmware).")
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
                    print(f"  abi_version:  {p_major}.{p_minor}")
                    print(f"  lvgl_version: {plv[0]}.{plv[1]}.{plv[2]}")

                # uid: the plugin's stable identity. Must be present, non-empty,
                # and not a bare integer (that space is reserved for built-ins).
                if uid is None:
                    warnings.append(
                        "could not resolve the descriptor 'uid' string "
                        "statically; uid content check skipped.")
                elif uid == "":
                    errors.append(
                        "descriptor 'uid' pointer is NULL. Every plugin must set "
                        "a stable uid (the scaffold auto-generates one, e.g. "
                        "\"qtune.my-tuner.k7f2q9\").")
                elif re.fullmatch(r"\d+", uid):
                    errors.append(
                        f"descriptor 'uid' is \"{uid}\", a bare integer — that "
                        "space is reserved for built-in UIs. Use a namespaced "
                        "uid like \"qtune.my-tuner.k7f2q9\".")
                else:
                    print(f"  uid:          {uid}")

        # Undefined-symbol check: a symbol resolves iff it is in the firmware's
        # curated table (ALLOWED_SYMBOLS.md) or elf_loader's built-in table.
        resolvable = read_allowlist() | ELF_LOADER_BUILTINS
        for name in undefined_dynsyms(elf):
            if name == DESCRIPTOR_SYMBOL or name in resolvable:
                continue
            if name.startswith("__"):
                # libgcc/compiler-runtime helper (e.g. __divsf3 float division,
                # __extendsfdf2 float->double). The elf_loader does NOT provide
                # these; the firmware must export them. Only the ones in the
                # allowlist resolve — flag the rest (they fail to relocate on the
                # device with "Can't find symbol <name>").
                errors.append(
                    f"references compiler-runtime symbol '{name}' (libgcc soft-"
                    "float/conversion helper) that the firmware does not export. "
                    "The plugin will fail to load — request it be added to the "
                    "firmware's qtune_plugin_symbols.txt.")
            elif name.startswith("lv_"):
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

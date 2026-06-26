#!/usr/bin/env python3
"""
new_plugin.py — scaffold a new Q-Tune plugin project from the SDK template.

This solves the "make everything unique" problem for you: it fills in a unique
function prefix, a stable auto-generated UID (the plugin's permanent identity),
and a unique build tag, and points the project at this SDK checkout. The result
is a complete, buildable project — edit its main/<name>.cpp, then build and
validate.

You never choose a numeric plugin ID: the firmware assigns each plugin a number
dynamically at load time. Identity is the descriptor's `uid` string (auto-
generated here, e.g. "qtune.glow-needle.k7f2q9"). It must never change after you
publish the plugin — changing it loses the user's saved selection of that UI.

Usage:
    python3 tools/new_plugin.py --name "Glow Needle" --type tuner
    python3 tools/new_plugin.py --name "Star Field" --type standby --dest ~/plugins
    python3 tools/new_plugin.py --name "Strobe X" --type tuner --prefix sx
    python3 tools/new_plugin.py --name "My Tuner" --type tuner --uid qtune.my-tuner.ab12cd

Options:
    --name      Display name shown in the pedal menu (required).
    --type      tuner | standby (required).
    --dest      Parent directory to create the project in
                (default: the SDK's plugins/ folder — the standard home for your
                projects; gitignored, so it stays separate from the SDK).
    --uid       Stable plugin UID (auto-generated if omitted). Must be non-empty
                and NOT a bare integer (that space is reserved for built-in UIs).
    --prefix    C function prefix (auto-derived from the name if omitted).
    --sdk-dir   Path to the q-tune-sdk checkout (default: this SDK).

After it runs:
    ./docker-build.sh <project>
    python3 tools/validate_plugin.py <project>/build/<name>.so
"""

import argparse
import os
import pathlib
import re
import sys

SDK_ROOT = pathlib.Path(__file__).resolve().parent.parent


def slugify(name: str) -> str:
    """'Glow Needle!' -> 'glow_needle' (a valid C/CMake identifier)."""
    slug = re.sub(r"[^a-z0-9]+", "_", name.strip().lower()).strip("_")
    if not slug:
        slug = "my_plugin"
    if slug[0].isdigit():
        slug = "p_" + slug
    return slug


def uid_slug(name: str) -> str:
    """'Glow Needle!' -> 'glow-needle' (dash-separated, for the namespaced uid)."""
    s = re.sub(r"[^a-z0-9]+", "-", name.strip().lower()).strip("-")
    return s or "plugin"


def derive_prefix(slug: str) -> str:
    """Short function prefix from the slug: initials of words, else first letters."""
    words = [w for w in slug.split("_") if w]
    if len(words) >= 2:
        prefix = "".join(w[0] for w in words)
    else:
        prefix = words[0][:3] if words else "p"
    if not prefix[0].isalpha():
        prefix = "p" + prefix
    return prefix


_B32 = "abcdefghijklmnopqrstuvwxyz234567"


def gen_token(n: int = 6) -> str:
    """A short, unique lowercase base32 token from os.urandom (unique per scaffold)."""
    raw = os.urandom(n)  # one byte per output char; 5 bits of each are used
    return "".join(_B32[b & 0x1F] for b in raw)[:n]


def gen_uid(name: str) -> str:
    """Auto-generate a stable, namespaced uid: qtune.<slug>.<token>."""
    return f"qtune.{uid_slug(name)}.{gen_token()}"


def is_bare_integer(s: str) -> bool:
    """True if `s` is a plain integer (e.g. '0', '142') — reserved for built-ins."""
    return bool(re.fullmatch(r"\d+", s.strip()))


def main() -> int:
    ap = argparse.ArgumentParser(description="Scaffold a new Q-Tune plugin project.")
    ap.add_argument("--name", required=True, help="Display name (shown in the menu).")
    ap.add_argument("--type", required=True, choices=["tuner", "standby"])
    ap.add_argument("--dest", default=str(SDK_ROOT / "plugins"),
                    help="Parent dir for the new project (default: the SDK's plugins/ folder).")
    ap.add_argument("--uid", default=None,
                    help="Stable plugin UID (auto-generated if omitted).")
    ap.add_argument("--prefix", default=None, help="C function prefix (auto if omitted).")
    ap.add_argument("--sdk-dir", default=str(SDK_ROOT), help="q-tune-sdk path.")
    args = ap.parse_args()

    sdk_dir = pathlib.Path(args.sdk_dir).expanduser().resolve()
    template_dir = sdk_dir / "template" / args.type
    if not template_dir.is_dir():
        print(f"ERROR: template not found: {template_dir}", file=sys.stderr)
        return 1

    slug = slugify(args.name)
    prefix = (args.prefix or derive_prefix(slug)).strip()
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", prefix):
        print(f"ERROR: invalid prefix '{prefix}'", file=sys.stderr)
        return 1

    if args.uid is not None:
        plugin_uid = args.uid.strip()
        if not plugin_uid:
            print("ERROR: --uid must not be empty", file=sys.stderr)
            return 1
        if is_bare_integer(plugin_uid):
            print(f"ERROR: --uid '{plugin_uid}' must not be a bare integer "
                  "(that space is reserved for built-in UIs; use a namespaced "
                  "uid like 'qtune.my-tuner.ab12cd').", file=sys.stderr)
            return 1
    else:
        plugin_uid = gen_uid(args.name)

    dest_root = pathlib.Path(args.dest).expanduser().resolve() / slug
    if dest_root.exists():
        print(f"ERROR: destination already exists: {dest_root}", file=sys.stderr)
        return 1

    tokens = {
        "@PROJECT_NAME@": slug,
        "@PREFIX@": prefix,
        "@DISPLAY_NAME@": args.name,
        "@PLUGIN_UID@": plugin_uid,
        "@SDK_BUILD_TAG@": f"{slug}-1.0",
        # Forward slashes so the baked fallback path is valid in CMake on every
        # OS (Windows backslashes would be mis-parsed). Docker builds override
        # QTUNE_SDK_DIR anyway; this only matters for non-Docker local builds.
        "@QTUNE_SDK_DIR@": sdk_dir.as_posix(),
    }

    # Copy the template, substituting tokens in every file. Rename plugin.cpp.
    for src in sorted(template_dir.rglob("*")):
        rel = src.relative_to(template_dir)
        out = dest_root / rel
        if src.is_dir():
            out.mkdir(parents=True, exist_ok=True)
            continue
        if src.name == "plugin.cpp":
            out = out.with_name(f"{slug}.cpp")
        out.parent.mkdir(parents=True, exist_ok=True)
        text = src.read_text()
        for tok, val in tokens.items():
            text = text.replace(tok, val)
        out.write_text(text)

    print(f"Created {args.type} plugin '{args.name}'")
    print(f"  folder:   {dest_root}")
    print(f"  uid:      {plugin_uid}   (stable identity — never change after publishing)")
    print(f"  prefix:   {prefix}_")
    print(f"  source:   {dest_root / 'main' / (slug + '.cpp')}")
    print(f"  sdk:      {sdk_dir}")
    is_windows = os.name == "nt"
    builder = sdk_dir / ("docker-build.ps1" if is_windows else "docker-build.sh")
    py = "python" if is_windows else "python3"
    print()
    print("Next steps:")
    print(f"  1. Edit the UI:   {dest_root / 'main' / (slug + '.cpp')}")
    print(f"  2. Build:         {builder} {dest_root}")
    print(f"     (the build validates the .so automatically; to validate manually:")
    print(f"      {py} {sdk_dir / 'tools' / 'validate_plugin.py'} {dest_root / 'build' / (slug + '.so')})")
    print(f"  3. Upload the .so to http://<pedal-ip>/plugins, restart, and select it.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

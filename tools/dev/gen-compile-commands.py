#!/usr/bin/env python3
import json
import os
import shlex
import sys


def main():
    if len(sys.argv) < 6:
        raise SystemExit("usage: gen-compile-commands.py <cc> <cflags> <cxx> <cxxflags> <sources...>")

    cc = sys.argv[1]
    cflags = shlex.split(sys.argv[2])
    cxx = sys.argv[3]
    cxxflags = shlex.split(sys.argv[4])
    sources = sys.argv[5:]
    root = os.getcwd()
    build_dir = os.path.join(root, "build")
    os.makedirs(build_dir, exist_ok=True)

    entries = []
    for source in sources:
        output = os.path.join("build", os.path.splitext(source)[0] + ".o")
        compiler = cxx if source.endswith((".cc", ".cpp", ".cxx")) else cc
        flags = cxxflags if source.endswith((".cc", ".cpp", ".cxx")) else cflags
        entries.append({
            "directory": root,
            "file": os.path.join(root, source),
            "arguments": [compiler, *flags, "-c", source, "-o", output],
        })

    with open(os.path.join(build_dir, "compile_commands.json"), "w", encoding="utf-8") as handle:
        json.dump(entries, handle, indent=2)
        handle.write("\n")

    print(os.path.join("build", "compile_commands.json"))


if __name__ == "__main__":
    main()

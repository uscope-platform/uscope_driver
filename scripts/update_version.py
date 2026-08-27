#!/usr/bin/python3

import sys, subprocess, os


def git_head_short_hash(repo_dir):
    try:
        proc = subprocess.Popen(
            ["git", "rev-parse", "--short", "HEAD"],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            cwd=repo_dir,
        )
        out, _ = proc.communicate()
        return out.decode().rstrip()
    except OSError:
        return "unknown"


output_path = sys.argv[1]
source_dir = sys.argv[2] if len(sys.argv) > 2 else os.getcwd()

driver_ver = git_head_short_hash(source_dir)

toolchain_ver = "unknown"
toolchain_dir = os.path.join(os.getcwd(), "_deps", "fcore_toolchain-src")
if os.path.isdir(toolchain_dir):
    toolchain_ver = git_head_short_hash(toolchain_dir)

# Generated as a translation unit (not a header): only the TU that contains
# these definitions depends on it, so regenerating it never recompiles the
# translation units that merely include the stable driver_version.h header.
content = (
    '#include <string>\n'
    '#include "driver_version.h"\n'
    '\n'
    'const std::string uscope_driver_versions = "{driver_version}";\n'
    'const std::string uscope_toolchain_versions = "{toolchain_version}";\n'
).format(driver_version=driver_ver, toolchain_version=toolchain_ver)

# Only rewrite the file when the content actually changed: this preserves the
# output mtime so a plain `make` after a commit does not trigger a rebuild.
if os.path.exists(output_path):
    with open(output_path) as f:
        if f.read() == content:
            sys.exit(0)

with open(output_path, "w") as f:
    f.write(content)

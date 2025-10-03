#!/usr/bin/python3

import sys, subprocess, os

file_path = sys.argv[1]
proc = subprocess.Popen(["git", "rev-parse",  "--short", "HEAD"], stdout=subprocess.PIPE)
driver_ver = proc.stdout.read().decode().rstrip()
cwd = os.getcwd()
os.chdir("_deps/fcore_toolchain-src/")
proc = subprocess.Popen(["git", "rev-parse",  "--short", "HEAD"], stdout=subprocess.PIPE)
toolchain_ver = proc.stdout.read().decode().rstrip()
os.chdir(cwd)
version_file_content = ('#ifndef USCOPE_DRIVER_VERSION_H\n#define USCOPE_DRIVER_VERSION_H\n#include <string>\n\nconst '
                        'std::string uscope_driver_versions = "{driver_version}";\nconst std::string uscope_toolchain_versions = "{toolchain_version}";\n#endif').format(
    driver_version=driver_ver,
    toolchain_version=toolchain_ver
)

with open(file_path, "w") as f:
    f.write(version_file_content)

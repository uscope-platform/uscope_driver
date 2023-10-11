#!/usr/bin/python3

import sys, subprocess

file_path = sys.argv[1]
proc = subprocess.Popen(["git", "rev-parse",  "--short", "HEAD"], stdout=subprocess.PIPE)
ver = proc.stdout.read().decode().rstrip()
version_file_content = ('#ifndef USCOPE_DRIVER_VERSION_H\n#define USCOPE_DRIVER_VERSION_H\n#include <string>\n\nconst '
                        'std::string uscope_driver_versions = "{version}";\n#endif').format(version=ver)

with open(file_path, "w") as f:
    f.write(version_file_content)

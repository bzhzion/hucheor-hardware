Import("env")

import os

# Mirrors the mobile apps' release pattern (see admin repo docs/mobile-app-releases.md):
# the version is never committed in source, it is stamped in at build time from a Git
# tag. Local/dev builds (no FIRMWARE_VERSION env var, e.g. CI's push/PR build) fall back
# to "dev" so it is always obvious a binary was not built from a tagged release.
version = os.environ.get("FIRMWARE_VERSION", "dev")

header_dir = os.path.join(env.subst("$PROJECT_DIR"), "include")
os.makedirs(header_dir, exist_ok=True)

with open(os.path.join(header_dir, "version.h"), "w") as f:
    f.write("#pragma once\n")
    f.write('#define FIRMWARE_VERSION "{}"\n'.format(version))

"""Configure host-specific native simulator link dependencies."""

import sys


Import("env")  # noqa: F821  -- provided by PlatformIO at script load

if sys.platform.startswith("linux"):
    env.Append(LIBS=["crypto"])  # noqa: F821  -- OpenSSL-backed simulator MD5Builder

# PlatformIO pre-build hook: read the VERSION file and inject the value as a
# C macro (GD_VERSION) so the firmware can display its own version on screen.
# Falls back to "dev" if VERSION is missing.

from pathlib import Path

Import("env")  # noqa: F821 - provided by PlatformIO

version_path = Path(env["PROJECT_DIR"]) / "VERSION"  # noqa: F821
try:
    version = version_path.read_text().strip()
except FileNotFoundError:
    version = "dev"

env.Append(CPPDEFINES=[("GD_VERSION", env.StringifyMacro(version))])  # noqa: F821
print(f"== Geometry Dash version: {version}")

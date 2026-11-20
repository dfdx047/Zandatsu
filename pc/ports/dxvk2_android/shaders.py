"""Generate DXVK 2.7.1 built-in shader headers (same glslang call as its meson.build: Vulkan 1.3)."""
import pathlib
import subprocess
import sys
root = pathlib.Path(sys.argv[1]).resolve()
output = root / "generated"
output.mkdir(exist_ok=True)
count = 0
for directory in ("src/dxvk/shaders", "src/dxvk/hud/shaders", "src/d3d9/shaders"):
    for shader in sorted((root / directory).iterdir()):
        if shader.suffix not in (".comp", ".frag", ".vert", ".geom"):
            continue
        header = output / (shader.stem + ".h")
        if header.exists() and header.stat().st_mtime >= shader.stat().st_mtime:
            count += 1
            continue
        subprocess.run(["glslangValidator", "--quiet", "--target-env", "vulkan1.3", "--vn", shader.stem,
                        str(shader), "-o", str(header)], check=True)
        count += 1
print(f"{count} built-in shaders generated for Vulkan 1.3")

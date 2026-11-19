"""Generate and validate embedded shaders against the Vulkan 1.1 environment."""
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
        binary = output / (shader.stem + ".spv")
        subprocess.run(["glslangValidator", "--quiet", "--target-env", "vulkan1.1", str(shader), "-o", str(binary)], check=True)
        subprocess.run(["spirv-val", "--target-env", "vulkan1.1", str(binary)], check=True)
        subprocess.run(["glslangValidator", "--quiet", "--target-env", "vulkan1.1", "--vn", shader.stem, str(shader), "-o", str(output / (shader.stem + ".h"))], check=True)
        count += 1
print(f"{count} built-in shaders generated and validated for Vulkan 1.1")

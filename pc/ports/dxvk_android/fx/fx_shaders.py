"""fx_shaders.py: regenerate fx/generated/*.h (SPIR-V as uint32 arrays) for the present effects (present_fx.cpp).
Same toolchain and target as ../shaders.py (glslangValidator + spirv-val, Vulkan 1.1). The headers are committed,
so building DXVK does not need glslang; rerun this after editing a shader:  python3 fx_shaders.py"""
import pathlib
import subprocess

here = pathlib.Path(__file__).resolve().parent
out = here / "generated"
out.mkdir(exist_ok=True)
names = ["dxvk_fsr_easu.frag", "dxvk_fsr_rcas.frag", "dxvk_cas.frag",
         "dxvk_fi_p1.comp", "dxvk_fi_p2.comp", "dxvk_fi_p3.comp", "dxvk_fi_p4.comp"]
for name in names:
    shader = here / name
    stem = shader.stem
    spv = out / (stem + ".spv")
    subprocess.run(["glslangValidator", "--quiet", "--target-env", "vulkan1.1", str(shader), "-o", str(spv)], check=True)
    subprocess.run(["spirv-val", "--target-env", "vulkan1.1", str(spv)], check=True)
    subprocess.run(["glslangValidator", "--quiet", "--target-env", "vulkan1.1", "--vn", stem, str(shader),
                    "-o", str(out / (stem + ".h"))], check=True)
    spv.unlink()
print(f"{len(names)} present effect shaders generated in {out}")

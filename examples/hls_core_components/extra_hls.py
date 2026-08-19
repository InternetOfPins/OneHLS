"""
PlatformIO custom targets for HLS synthesis via PandA-Bambu, plus wiring
AC_TYPES_INCLUDE into the native build.

HAPI/OneData are declared in platformio.ini's [env] as real lib_deps
(their published repos), not a sibling monorepo checkout -- this example
must build for anyone who clones just github.com/InternetOfPins/OneHLS,
not only someone with the whole IOP tree on disk. Bambu can't be handed
a git URL directly, though -- it needs real files on disk -- so this
script locates HAPI/OneData's headers from THIS SAME env's own resolved
lib_deps under .pio/libdeps/<env>/, the exact copies PlatformIO's own
native build already uses, rather than assuming any particular local
checkout layout.

Five custom HLS targets, one per core component (see the main README's
"Verified results" table -- these are the same targets, reproducible
from a clean clone rather than only from local .RnD/ scratch):

    pio run -e hls -t synthesize-fir
    pio run -e hls -t synthesize-biquad
    pio run -e hls -t synthesize-pid
    pio run -e hls -t synthesize-accumulator
    pio run -e hls -t synthesize-complex-mac

Requires:
  BAMBU_APPIMAGE   - path to a bambu AppImage
                     (https://release.bambuhls.eu/bambu-2024.10.AppImage)
  AC_TYPES_INCLUDE - path to a clone's include/ dir
                     (git clone --depth 1 https://github.com/hlslibs/ac_types)
                     Bambu also bundles its own older ac_types fork on its
                     default include path, but that's irrelevant to
                     whether this specific -I is present: omitting
                     AC_TYPES_INCLUDE here fails the compile outright, it
                     doesn't silently fall back to anything -- same
                     AC_VERSION guard convention as the main library's
                     own ac_types_support.h.

Synthesizes against the same device/clock convention as every other
target in this library (see the main README.md's "Verified results"):
xc7a100t-1csg324-VVD (Xilinx Artix-7, Digilent Arty A7/Nexys A7 --
widely owned, not a special-order part), 10ns clock (100MHz).
"""
import os
Import("env")

BAMBU = os.environ.get("BAMBU_APPIMAGE")
AC_TYPES_INC = os.environ.get("AC_TYPES_INCLUDE")
DEVICE = "xc7a100t-1csg324-VVD"
CLOCK_PERIOD = "10"

HERE = env.subst("$PROJECT_DIR")
LIBDEPS_DIR = os.path.join(env.subst("$PROJECT_LIBDEPS_DIR"), env.subst("$PIOENV"))
ONEHLS_INC = os.path.join(LIBDEPS_DIR, "OneHLS", "include")
HAPI_INC = os.path.join(LIBDEPS_DIR, "HAPI", "include")
ONEDATA_INC = os.path.join(LIBDEPS_DIR, "OneData", "include")

if AC_TYPES_INC:
    env.Append(CPPPATH=[AC_TYPES_INC])


def _bambu_cmd(name, top_fname, src_name):
    if not BAMBU:
        return (
            'echo "BAMBU_APPIMAGE is not set -- point it at a bambu AppImage '
            '(e.g. https://release.bambuhls.eu/bambu-2024.10.AppImage) and '
            're-run. Not auto-installing anything." && exit 1'
        )
    if not AC_TYPES_INC:
        return (
            'echo "AC_TYPES_INCLUDE is not set -- point it at a clone of '
            'https://github.com/hlslibs/ac_types (its include/ dir) and '
            're-run. Not auto-cloning anything." && exit 1'
        )
    if not os.path.isdir(HAPI_INC) or not os.path.isdir(ONEDATA_INC):
        # Expected layout, per PlatformIO's documented lib_deps resolution
        # convention -- not independently confirmed against a real `pio`
        # run in the environment this script was written in. If either
        # directory is missing, `pio run -e hls` (a normal build, not
        # this custom target) must run at least once first so PlatformIO
        # actually fetches lib_deps into .pio/libdeps/hls/ before this
        # target can find them there.
        return (
            f'echo "Expected HAPI/OneData under {LIBDEPS_DIR} but did not '
            f'find them -- run \\"pio run -e hls\\" once first (even if it '
            f'fails) so PlatformIO fetches lib_deps, then retry this '
            f'target." && exit 1'
        )
    outdir = os.path.join(HERE, f".hls_out_{name}")
    os.makedirs(outdir, exist_ok=True)
    src = os.path.join(HERE, "hls", f"{src_name}.cpp")
    return (
        f'cd "{outdir}" && "{BAMBU}" '
        f'-I"{ONEHLS_INC}" -I"{HAPI_INC}" -I"{ONEDATA_INC}" -I"{AC_TYPES_INC}" '
        f'--std=gnu++17 --compiler=I386_CLANG16 '
        f'--device-name={DEVICE} --clock-period={CLOCK_PERIOD} '
        f'--top-fname={top_fname} -v2 "{src}"'
    )


TARGETS = [
    dict(name="fir", top_fname="oneHlsFirTop", src_name="fir_top",
         title="HLS: synthesize Fir<> (4-tap, Hamming-LPF coefficients)",
         description="Fir<Sample,Accum,10,118,118,10> -- see the main "
                      "README's Verified results table for the reference "
                      "FF/area/DSP numbers this target reproduces."),
    dict(name="biquad", top_fname="oneHlsBiquadTop", src_name="biquad_top",
         title="HLS: synthesize Biquad<> (direct-form-I IIR section)",
         description="Biquad<Sample,Accum,128,64,128,-64> -- feedforward "
                      "Tap reused verbatim from Fir<>, feedback via "
                      "FBTap's two-phase fbSum()/fbPush() split."),
    dict(name="pid", top_fname="oneHlsPidTop", src_name="pid_top",
         title="HLS: synthesize Pid<> (P + accumulator-shaped I + tap-delay D)",
         description="Pid<Sample,Accum,256,64,128> -- Kp=1.0, Ki=0.25, Kd=0.5."),
    dict(name="accumulator", top_fname="oneHlsAccumulatorTop", src_name="accumulator_top",
         title="HLS: synthesize Accumulator<> (narrow-width wraparound)",
         description="Accumulator<ac_int<8,true>,ac_int<8,true>> -- "
                      "deliberate 2's-complement wraparound, no saturation."),
    dict(name="complex-mac", top_fname="oneHlsComplexMacTop", src_name="complex_mac_top",
         title="HLS: synthesize ComplexMac<> (complex multiply-accumulate)",
         description="ComplexMac<Sample,Accum,2,-1> over OneHLS's own "
                      "vendor-agnostic Complex<T> -- see the main README's "
                      "BRAM-binding caveat for this one non-zero-cost "
                      "component."),
]

for t in TARGETS:
    env.AddCustomTarget(
        name=f"synthesize-{t['name']}",
        dependencies=None,
        actions=[_bambu_cmd(t["name"], t["top_fname"], t["src_name"])],
        title=t["title"],
        description=t["description"],
        always_build=True,
    )

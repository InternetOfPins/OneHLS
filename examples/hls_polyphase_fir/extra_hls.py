"""
PlatformIO custom target for HLS synthesis via PandA-Bambu, plus wiring
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

Custom HLS target:

    pio run -e hls -t synthesize-polyphase-fir

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


def _bambu_cmd():
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
    outdir = os.path.join(HERE, ".hls_out_polyphase_fir")
    os.makedirs(outdir, exist_ok=True)
    src = os.path.join(HERE, "hls", "polyphase_fir_top.cpp")
    return (
        f'cd "{outdir}" && "{BAMBU}" '
        f'-I"{ONEHLS_INC}" -I"{HAPI_INC}" -I"{ONEDATA_INC}" -I"{AC_TYPES_INC}" '
        f'--std=gnu++17 --compiler=I386_CLANG16 '
        f'--device-name={DEVICE} --clock-period={CLOCK_PERIOD} '
        f'--top-fname=polyphaseFirTop -v2 "{src}"'
    )


env.AddCustomTarget(
    name="synthesize-polyphase-fir",
    dependencies=None,
    actions=[_bambu_cmd()],
    title="HLS: synthesize a polyphase FIR decimator (M=2)",
    description="PolyphaseFirDecim<ac_fixed<16,16,true>,ac_fixed<32,32,true>,2,10,118,118,10> "
                 "-- see ../../docs/PHASE9_GENERIC_POLYPHASE.md and "
                 "../../docs/PHASE4_POLYPHASE_EXPERIMENT.md for the full "
                 "derivation and resource comparisons this example's README "
                 "numbers come from.",
    always_build=True,
)

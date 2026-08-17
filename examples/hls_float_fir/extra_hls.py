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

Unlike HAPI's own hls_fir example (whose src/main.cpp is plain
int16_t/int32_t, needing no vendor type at all), this example's native
demo uses ac_std_float directly, so env:native ALSO needs the same -I
ac_types real upstream headers as the HLS target, or `pio run -e native`
fails to find <ac_std_float.h> at all -- see the AC_TYPES_INCLUDE
handling below, which applies to both envs (platformio.ini's
extra_scripts line is identical in both).

Custom HLS target:

    pio run -e hls -t synthesize-float-fir

Requires:
  BAMBU_APPIMAGE   - path to a bambu AppImage
                     (https://release.bambuhls.eu/bambu-2024.10.AppImage)
  AC_TYPES_INCLUDE - path to a clone's include/ dir
                     (git clone --depth 1 https://github.com/hlslibs/ac_types)
                     ac_std_float.h lives in real upstream ac_types, same
                     as ac_fixed/ac_int -- see the main README.md's
                     "Vendor support headers" section. Bambu also bundles
                     its own older ac_types fork on its default include
                     path, but that's irrelevant to whether this specific
                     -I is present: omitting AC_TYPES_INCLUDE here fails
                     the compile outright, it doesn't silently fall back
                     to anything.

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
            f'fails on ac_std_float.h) so PlatformIO fetches lib_deps, then '
            f'retry this target." && exit 1'
        )
    outdir = os.path.join(HERE, ".hls_out_float_fir")
    os.makedirs(outdir, exist_ok=True)
    src = os.path.join(HERE, "hls", "fir_std_float_top.cpp")
    return (
        f'cd "{outdir}" && "{BAMBU}" '
        f'-I"{ONEHLS_INC}" -I"{HAPI_INC}" -I"{ONEDATA_INC}" -I"{AC_TYPES_INC}" '
        f'--std=gnu++17 --compiler=I386_CLANG16 '
        f'--device-name={DEVICE} --clock-period={CLOCK_PERIOD} '
        f'--top-fname=oneHlsFloatFirTop -v2 "{src}"'
    )


env.AddCustomTarget(
    name="synthesize-float-fir",
    dependencies=None,
    actions=[_bambu_cmd()],
    title="HLS: synthesize 4-tap FIR over ac_std_float (IEEE754 float)",
    description="Fir<ac_std_float<32,8>, ac_std_float<64,11>, 10,118,118,10> "
                 "-- see ../../README.md's 'Experimental: ac_std_float' "
                 "section for the verified resource comparison against the "
                 "ac_fixed baseline.",
    always_build=True,
)

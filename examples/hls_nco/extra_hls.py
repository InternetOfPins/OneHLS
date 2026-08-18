"""
PlatformIO custom target for HLS synthesis via PandA-Bambu, plus wiring
AC_TYPES_INCLUDE and AC_MATH_INCLUDE into the native build.

HAPI/OneData are declared in platformio.ini's [env] as real lib_deps
(their published repos), not a sibling monorepo checkout -- this example
must build for anyone who clones just github.com/InternetOfPins/OneHLS,
not only someone with the whole IOP tree on disk. Bambu can't be handed
a git URL directly, though -- it needs real files on disk -- so this
script locates HAPI/OneData's headers from THIS SAME env's own resolved
lib_deps under .pio/libdeps/<env>/, the exact copies PlatformIO's own
native build already uses, rather than assuming any particular local
checkout layout.

Unlike every other example in this library, this one needs TWO opt-in
vendor headers, not one: ac_fixed/ac_int (from ac_types, same as
everywhere else) AND ac_math/ac_sincos_cordic.h (from ac_math -- this
example is the one place in this library that actually composes an
ac_math primitive directly, see src/nco.h). Neither is vendored into
this repo, matching the main README's stated policy for ac_types/
ap_types.

Custom HLS target:

    pio run -e hls -t synthesize-nco

Requires:
  BAMBU_APPIMAGE   - path to a bambu AppImage
                     (https://release.bambuhls.eu/bambu-2024.10.AppImage)
  AC_TYPES_INCLUDE - path to a clone's include/ dir
                     (git clone --depth 1 https://github.com/hlslibs/ac_types)
  AC_MATH_INCLUDE  - path to a clone's include/ dir
                     (git clone --depth 1 https://github.com/hlslibs/ac_math)
                     Bambu also bundles its own older ac_types fork on its
                     default include path, but that's irrelevant to
                     whether AC_TYPES_INCLUDE is present: omitting either
                     variable here fails the compile outright, it doesn't
                     silently fall back to anything -- same AC_VERSION
                     guard convention as the main library's own
                     ac_types_support.h.

Synthesizes against the same device/clock convention as every other
target in this library (see the main README.md's "Verified results"):
xc7a100t-1csg324-VVD (Xilinx Artix-7, Digilent Arty A7/Nexys A7 --
widely owned, not a special-order part), 10ns clock (100MHz).
"""
import os
Import("env")

BAMBU = os.environ.get("BAMBU_APPIMAGE")
AC_TYPES_INC = os.environ.get("AC_TYPES_INCLUDE")
AC_MATH_INC = os.environ.get("AC_MATH_INCLUDE")
DEVICE = "xc7a100t-1csg324-VVD"
CLOCK_PERIOD = "10"

HERE = env.subst("$PROJECT_DIR")
LIBDEPS_DIR = os.path.join(env.subst("$PROJECT_LIBDEPS_DIR"), env.subst("$PIOENV"))
ONEHLS_INC = os.path.join(LIBDEPS_DIR, "OneHLS", "include")
HAPI_INC = os.path.join(LIBDEPS_DIR, "HAPI", "include")
ONEDATA_INC = os.path.join(LIBDEPS_DIR, "OneData", "include")

if AC_TYPES_INC:
    env.Append(CPPPATH=[AC_TYPES_INC])
if AC_MATH_INC:
    env.Append(CPPPATH=[AC_MATH_INC])


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
    if not AC_MATH_INC:
        return (
            'echo "AC_MATH_INCLUDE is not set -- point it at a clone of '
            'https://github.com/hlslibs/ac_math (its include/ dir) and '
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
    outdir = os.path.join(HERE, ".hls_out_nco")
    os.makedirs(outdir, exist_ok=True)
    src = os.path.join(HERE, "hls", "nco_top.cpp")
    return (
        f'cd "{outdir}" && "{BAMBU}" '
        f'-I"{ONEHLS_INC}" -I"{HAPI_INC}" -I"{ONEDATA_INC}" -I"{AC_TYPES_INC}" -I"{AC_MATH_INC}" '
        f'--std=gnu++17 --compiler=I386_CLANG16 '
        f'--device-name={DEVICE} --clock-period={CLOCK_PERIOD} '
        f'--top-fname=oneHlsNcoTop -v2 "{src}"'
    )


env.AddCustomTarget(
    name="synthesize-nco",
    dependencies=None,
    actions=[_bambu_cmd()],
    title="HLS: synthesize an NCO (numerically controlled oscillator)",
    description="Nco<ac_fixed<16,1,true>,ac_fixed<24,2,true>,8192> -- "
                 "composes ac_math::ac_sin_cordic/ac_cos_cordic directly, "
                 "see src/nco.h and ../../ECOSYSTEM.md's NCO section for "
                 "the full derivation and the real scale=1.0-wraps-at-I=1 "
                 "gotcha found verifying this.",
    always_build=True,
)

"""
PlatformIO custom targets for HLS synthesis via PandA-Bambu, plus wiring
AC_TYPES_INCLUDE into the native build.

HAPI/OneData are declared in platformio.ini's [env] as real lib_deps
(their published repos), not a sibling monorepo checkout -- this example
must build for anyone who clones just github.com/InternetOfPins/OneHLS.
Bambu can't be handed a git URL, so this script locates HAPI/OneData
headers from THIS env's own resolved lib_deps under .pio/libdeps/<env>/.

One target per synthesis top in hls/ -- e.g.:

    pio run -e hls -t synthesize-line-buffer
    pio run -e hls -t synthesize-conv-layer
    pio run -e hls -t synthesize-two-layer
    pio run -e hls -t synthesize-multi-conv
    pio run -e hls -t synthesize-axis
    pio run -e hls -t synthesize-erode

Requires:
  BAMBU_APPIMAGE   - path to a bambu AppImage
                     (https://release.bambuhls.eu/bambu-2024.10.AppImage)
  AC_TYPES_INCLUDE - a clone's include/ dir
                     (git clone --depth 1 https://github.com/hlslibs/ac_types)
                     omitting it fails the compile outright -- same
                     AC_VERSION guard as the library's own ac_types_support.h.

Same device/clock as the rest of the library:
xc7a100t-1csg324-VVD (Artix-7), 10 ns (100 MHz). Vitis HLS numbers in the
README were taken separately with `xc7a100tcsg324-1`.
"""
import os
Import("env")

BAMBU = os.environ.get("BAMBU_APPIMAGE")
AC_TYPES_INC = os.environ.get("AC_TYPES_INCLUDE")
DEVICE = "xc7a100t-1csg324-VVD"
CLOCK_PERIOD = "10"

HERE = env.subst("$PROJECT_DIR")
LIBDEPS_DIR = os.path.join(env.subst("$PROJECT_LIBDEPS_DIR"), env.subst("$PIOENV"))
HAPI_INC = os.path.join(LIBDEPS_DIR, "HAPI", "include")
ONEDATA_INC = os.path.join(LIBDEPS_DIR, "OneData", "include")
ONEHLS_INC = os.path.join(LIBDEPS_DIR, "OneHLS", "include")

if AC_TYPES_INC:
    env.Append(CPPPATH=[AC_TYPES_INC])

# (target name, top-fname, source file in hls/, one-line title)
TARGETS = [
    ("synthesize-line-buffer",   "oneHlsLineBufferTop",     "line_buffer_top.cpp",
     "LineBuffer<ac16,8,1,3> -- ring = (K-1)*W+1 = 17 words, no FIFO"),
    ("synthesize-window-extract", "oneHlsWindowExtractTop",  "window_extract_top.cpp",
     "WindowExtract composed on LineBuffer -- no hidden buffering at the seam"),
    ("synthesize-conv-layer",    "oneHlsConvLayerTop",       "conv_layer_top.cpp",
     "one streaming conv layer -- literal-weight kernel"),
    ("synthesize-two-layer",     "oneHlsTwoLayerTop",        "two_layer_top.cpp",
     "conv -> 2x2 maxpool -> conv, one step() chain, zero inter-stage FIFO"),
    ("synthesize-two-layer-rom", "oneHlsTwoLayerRomTop",     "two_layer_rom_top.cpp",
     "same 2-layer net with ROM weights -- the literal-vs-ROM DSP comparison"),
    ("synthesize-multi-conv",    "oneHlsMultiConvTop",       "multi_conv_top.cpp",
     "multi-channel conv (Cin=Cout=4), weights from ROM"),
    ("synthesize-pad-conv",      "oneHlsPadConvTop",         "pad_conv_top.cpp",
     "Pad2D + WindowFront + Dot == SAME conv, zero border storage"),
    ("synthesize-axis",          "oneHlsAxisTop",            "axis_top.cpp",
     "AXI4-Stream adapter -- skid FIFO sized from the pipeline's LatencyBound"),
    ("synthesize-transpose",     "oneHlsTransposeRdTop",     "transpose_top.cpp",
     "Transpose2D -- the one full-frame (H*W*C) buffer in the set"),
    ("synthesize-erode",         "oneHlsErodeTop",           "morph_top.cpp",
     "streaming erosion -- WindowExtract + ReduceTree<Min>, no new primitive"),
]


def _bambu_cmd(top, srcname):
    if not BAMBU:
        return ('echo "BAMBU_APPIMAGE not set -- point it at a bambu AppImage '
                'and re-run." && exit 1')
    if not AC_TYPES_INC:
        return ('echo "AC_TYPES_INCLUDE not set -- point it at a clone of '
                'https://github.com/hlslibs/ac_types (its include/ dir)." && exit 1')
    if not os.path.isdir(HAPI_INC) or not os.path.isdir(ONEDATA_INC):
        return (f'echo "HAPI/OneData not under {LIBDEPS_DIR} -- run '
                f'\\"pio run -e hls\\" once first so PlatformIO fetches '
                f'lib_deps, then retry." && exit 1')
    outdir = os.path.join(HERE, ".hls_out_" + top)
    os.makedirs(outdir, exist_ok=True)
    src = os.path.join(HERE, "hls", srcname)
    return (
        f'cd "{outdir}" && "{BAMBU}" '
        f'-I"{ONEHLS_INC}" -I"{HAPI_INC}" -I"{ONEDATA_INC}" -I"{AC_TYPES_INC}" '
        f'--std=gnu++17 --compiler=I386_CLANG16 '
        f'--device-name={DEVICE} --clock-period={CLOCK_PERIOD} '
        f'--top-fname={top} -v2 "{src}"'
    )


for name, top, srcname, title in TARGETS:
    env.AddCustomTarget(
        name=name,
        dependencies=None,
        actions=[_bambu_cmd(top, srcname)],
        title=title,
        description="See ../README.md 'Verified results'.",
        always_build=True,
    )

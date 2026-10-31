# PCMenu

PC style pause / title / options menu for GTA San Andreas Android (AML mod).
Game builds: 2.00 armeabi-v7a (32-bit) and 2.10 arm64-v8a (64-bit).

## Variants

| File | What it is |
|------|------------|
| `libPCMenu32.so` / `libPCMenu64.so` | PC menu with the opaque PC backdrop |
| `libPCMenuT32.so` / `libPCMenuT64.so` | PC menu (Transparent): the paused game shows, blurred and darkened, behind the menu (Android port of the idea of [III.VC.SA.TransparentMenu](https://github.com/ThirtyFoxMCx00E1/III.VC.SA.TransparentMenu)) |

Install one variant only. Both share the AML GUID `net.gtasa.pcmenu`, so AML loads just one of them.

## Install

1. Install [AndroidModLoader](https://github.com/ThirtyFoxMCx00E1/AndroidModLoader).
2. Copy the `.so` for your game (32 or 64-bit, normal or `T`) into `Android/data/com.rockstargames.gtasa/mods/`.
3. Copy the `PCMenu` folder (from `assets/`) into `Android/data/com.rockstargames.gtasa/files/`.
4. Settings: `files/PCMenu/pcmenu.ini`.

## Features (1.2.0)

- Language system: every text, including options other mods add, goes through one translator:
  your own `PCMenu/lang/<fr|de|it|es|id|ar>.txt` (ENGLISH=TRANSLATION), then the built-in dictionary
  (`source/tools/pcdict.txt`, 330+ entries, EN/FR/DE/IT/ES/ID/AR), then patterns (`ENABLE %s`, `%s VOLUME`, ...),
  then word by word. Untranslated texts are logged to `PCMenu/lang_missing.txt` (`LangMissingLog=1`).
- Controller support: D-pad / stick up and down select, left and right change toggles and sliders,
  A / Enter accepts, B / Back goes back. Hold to repeat. `Controller=1`.
- Transparent backdrop (T build): the game frame is copied from `CHud::Draw` while you play
  (`RwRasterPushContext` + `RwRasterRenderFast`), then drawn blurred under the menu. The title menu stays opaque.
- PC style loading screen: cycles `PCMenu/pc/lds/mobilesc0.png` .. `mobilesc27.png` (missing numbers are skipped)
  with a crossfade, plus the PC style progress bar: a thin grey bar with a black outline just under the "San Andreas" logo (no text, no background strip).
  The picture is framed like the PC game (`SplashFit=PC`; `Fit` / `Fill` also available). All pictures are prepared on the
  main thread before loading starts (`SplashPreload`, default 3); the loading thread only draws, which fixes the freeze after New Game / Load Game.
  Options: `LoadingScreen`, `SplashFirst`, `SplashLast`, `SplashSeconds`, `Order`, `ProgressBar`, `LoadingMusic`.

## Build

```sh
cd source
ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=jni/Android.mk NDK_APPLICATION_MK=jni/Application.mk -j8
# -> libs/armeabi-v7a/libPCMenu32.so, libPCMenuT32.so, libs/arm64-v8a/libPCMenu64.so, libPCMenuT64.so
```

After editing `source/tools/pcdict.txt` run `python3 tools/gen_dict.py` (regenerates `jni/pcdict.h`).

Host test (fake game, 64-bit layout): `cd source/hosttest && PCM_DIR=$PWD/../../assets/PCMenu/ ./run.sh`

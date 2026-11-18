# Building PotentialKits

You don't need to build it to use it: download the zip from [Releases](../../releases). This is for people who want to change it.

It's built with the [Open MPC kit](https://github.com/open-mpc/mpc-vst-plugins), and needs Linux or WSL, `gcc`/`g++`, Python 3, `pip install ziglang pillow numpy playwright` and `python -m playwright install chromium`. `.github/workflows/release.yml` has the exact, current commands, including the kit commit it's pinned to.

```
git clone https://github.com/open-mpc/mpc-vst-plugins
git clone https://github.com/PotentialBreakfast/mpc-vst-potentialkits
cd mpc-vst-plugins && git checkout <KIT_REF from release.yml> && git apply ../mpc-vst-potentialkits/shared/mpc-vst-plugins-label-fit.patch && cd ..
K=$PWD/mpc-vst-plugins; P=$PWD/mpc-vst-potentialkits; cd $P

# 1. the three looks' skins and params.h
mkdir -p vst/build
gcc -O2 -I $K/tools/vendor/force-shadow/tools -o vst/build/shadow_art $K/tools/shadow_art.c -lm
python3 shared/looks/build_looks.py vst $K

# 2. tests on your computer (x86, AddressSanitizer)
bash $K/tools/test_port.sh vst/vst.json                       # the plugin as MPC loads it
cp $K/wrapper/engine.h vst/build/
g++ -fsanitize=address,undefined -g -std=gnu++11 -I src tests/core_test.cpp -o /tmp/core_test && /tmp/core_test
g++ -fsanitize=address,undefined -g -std=gnu++11 -pthread -I vst/build -I src tests/engine_test.cpp src/engine.cpp -o /tmp/engine_test && /tmp/engine_test

# 3. build for the MPC: see "Build for the MPC" in release.yml (zig c++, armv7 and aarch64, glibc 2.29)
```

**Notes**
- **The engine** is Lucky Dip's (`src/engine.cpp`, `ld_core.h`, `ld_audio.h`, `ld_xpm.h`, `xpm_template.h`), with PotentialKits' additions marked in the comments: kit styles, ROLL, pitch/decay/choke (playback, state and export), the pad tiles, USER kits, the change reports (`mpc_engine_changed`), the look switch and the manual page.
- **Parameters are append-only** once released: `vst/params.lock` keeps saved projects working.
- **State:** Lucky Dip's `LD1` text, plus `px <pad> <pitch> <decay> <choke>` lines and `wild=`. A USER kit is the same text without the folder and library lines.
- **The looks:** `vst/layout.base.conf` is the only layout to edit; `shared/looks/make_looks.py` turns it into the three looks and adds the MANUAL tab from `vst/manual.txt` (its page names must match the options of `manual_page`) and the LOOK panel on the SETUP tab.
- **Exported kits:** `ld_xpm.h`. A pad with pitch, decay or gain changed is written by `render_pad()` (`src/engine.cpp`) as a 16-bit stereo WAV; choke is `<MuteGroup>`; in link mode pitch is `<TuneCoarse>`.

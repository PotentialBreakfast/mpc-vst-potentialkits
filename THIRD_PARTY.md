# Third-party code in PotentialKits

PotentialKits is a fork of **Lucky Dip** by sd88me ([sd88me/mpc-vst-luckydip](https://github.com/sd88me/mpc-vst-luckydip), MIT). Its full history is kept in this repo.

| Part | Where | Licence |
|---|---|---|
| Lucky Dip's kit engine: library scan, classifier, random assignment, loudness, WAV/AIFF decode, MPC program export | `src/engine.cpp`, `src/ld_core.h`, `src/ld_audio.h`, `src/ld_xpm.h`, `src/xpm_template.h`, `tests/`, `tools/` | MIT, Sam DiDomizio (`LICENSE`) |
| The VST2 wrapper and skin tools (not in this repo: fetched at build time) | [open-mpc/mpc-vst-plugins](https://github.com/open-mpc/mpc-vst-plugins) | MIT |
| Fonts | `vst/skin/looks/fonts/` | SIL Open Font License 1.1 (the `OFL-*.txt` files beside them) |

PotentialKits isn't affiliated with or endorsed by Akai Professional or inMusic.

## dr_mp3 (David Reid) – public domain (Unlicense) or MIT-0

`src/vendor/dr_mp3.h`, from https://github.com/mackron/dr_libs (commit 35b5c392cc15456736c440a68dbe073178bbffe0), reads MP3 sounds. Its licence is at the end of the file.

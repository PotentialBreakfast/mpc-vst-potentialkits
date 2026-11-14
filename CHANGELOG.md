# Changelog

## 1.1.1 (2026-11-14)
- **MP3 sounds.** MP3 files in your sound folders are found and played like WAV and AIFF. When you export a kit, an MP3 pad is saved as a WAV beside the program, because the MPC loads WAVs there.

## 1.1.0 (2026-11-14)
- **Fixed: SAVE, then a USER KITS slot, saved nothing.** One touch on a button reaches the plugin as two quick flips (on, then straight back off); SAVE switched itself off before the slot tap. The second flip is now ignored and SAVE stays lit until you tap a slot.
- **Fixed: LOCK unlocked straight away** (the * showed for a moment), for the same reason.
- **Simpler pages:** KIT, STYLES, SAVE, SETUP. The POOL page is gone: **TYPE** on the KIT page picks what a pad plays (AUTO, KICK, SNARE ... BASS, SYNTH ... VOX, OTHER) and gives the pad a new sound of that type in one tap. **ROLL** is on the KIT page too.
- USER KITS and EXPORT KIT share the new SAVE page, with the status line, so you can see "Saved to USER 3".
- PLAY, CLEAR, FAVOURITE and REJECT are off the screen (tap a tile to hear it). Saved projects, USER kits and automation are unchanged.

## 1.0.0 (2026-11)
First release of PotentialKits, a fork of sd88me's Lucky Dip 1.0.0.
- **KIT STYLES:** CLASSIC, BOOM BAP, LO-FI, TRAP, HOUSE, DUSTY JAZZ, PERCUSSION, ELECTRO and WEIRD, one tap each.
- **ROLL** with MILD, MEDIUM and WILD.
- **PITCH, DECAY and CHOKE** on every pad, in playback and in the exported kit (changed pads written as heard, choke as mute groups).
- **USER KITS:** 8 slots, kept beside the VST2 Plugins folder.
- **The KIT page:** 16 tiles laid out like the MPC's pads, with the selected pad beside them.
- The PotentialSuite looks (Print Plate, Night Rack, Colour Block) and a MANUAL page.
- Files live in the plugin's `data/` folder (kept by updates); kits go to `PotentialKits/Kits` on the plugin's drive by default.
- Lucky Dip's engine, library scan, classifier, level matching and export are unchanged otherwise.

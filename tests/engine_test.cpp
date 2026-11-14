/* PotentialKits engine test (x86, ASan): the real mpc_engine() driven the way the wrapper drives it, against a scratch
 * sample library: scan -> Generate -> decode -> pad notes sound -> state round trip -> reroll/lock/clear ->
 * Match Levels -> Export (Lucky Dip's own checks), then PotentialKits': pad tiles, PITCH / DECAY / CHOKE in playback and
 * in the exported kit, KIT STYLES, ROLL at each wildness, USER kits, and the change reports to the host.
 * Build: see BUILD.md (g++ -fsanitize=address,undefined ... tests/engine_test.cpp src/engine.cpp). */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <thread>
#include <chrono>
extern "C" {
#include "engine.h"
}
#include "ld_core.h"
#include "ld_xpm.h"
static int fails;
#define CHECK(c, ...) do { bool ok_ = (c); printf("%s ", ok_ ? "ok  " : "FAIL"); printf(__VA_ARGS__); printf("\n"); if (!ok_) fails++; } while (0)   /* c once: it may play audio */
static void put_le(std::string &s, uint32_t v, int n) { for (int i = 0; i < n; i++) s += (char)((v >> (8 * i)) & 255); }
static std::string wav(int amp, int frames) {
    std::string d;
    for (int i = 0; i < frames; i++) put_le(d, (uint16_t)(int16_t)((i % 40) < 20 ? amp : -amp), 2);
    std::string f = "RIFF"; put_le(f, 36 + d.size(), 4); f += "WAVEfmt "; put_le(f, 16, 4); put_le(f, 1, 2); put_le(f, 1, 2);
    put_le(f, 44100, 4); put_le(f, 88200, 4); put_le(f, 2, 2); put_le(f, 16, 2); f += "data"; put_le(f, d.size(), 4); return f + d;
}
static const mpc_engine_t *E;
static std::string get(void *h, const char *k) { char b[8192] = ""; E->get_param(h, k, b, sizeof b); return b; }
static void trig(void *h, const char *k) { E->set_param(h, k, "1.000000"); }
static bool wait_for(void *h, const char *key, const char *prefix, int ms = 5000) {
    for (int t = 0; t < ms; t += 20) { if (get(h, key).compare(0, strlen(prefix), prefix) == 0) return true; std::this_thread::sleep_for(std::chrono::milliseconds(20)); }
    return false;
}
static int peak(void *h, int blocks) {
    int pk = 0; int16_t out[256];
    for (int b = 0; b < blocks; b++) { E->render(h, out, 128); for (int i = 0; i < 256; i++) pk = std::max(pk, abs((int)out[i])); }
    return pk;
}

int main() {
    E = mpc_engine();
    char tmpl[] = "/tmp/pkengXXXXXX";
    std::string root = mkdtemp(tmpl), lib = root + "/lib", plug = root + "/VST2 Plugins/PotentialBreaks - VST - PotentialKits", data = plug + "/data";
    const char *dirs[] = {"Kicks", "Snares", "Claps", "Fx", "Synth"};
    for (size_t d = 0; d < 5; d++)
        for (int k = 1; k <= 3; k++) {
            ld::make_dirs(lib + "/" + dirs[d]);
            ld::write_text(lib + "/" + dirs[d] + "/s" + std::to_string(k) + ".wav", wav(d == 0 ? 20000 : 5000, 8000));
        }
    ld::make_dirs(data);
    ld::write_text(data + "/potentialkits.conf", "root=" + lib + "\nexport_dir=" + root + "/kits\n");

    void *h = E->create(plug.c_str());
    CHECK(h, "create");
    CHECK(wait_for(h, "status", "Library: 15"), "scan finished: \"%s\"", get(h, "status").c_str());
    CHECK(get(h, "pad1_name") == "(empty)", "pads start empty");
    /* a brand-new track (no saved state): the selected pad's default categories are lit from the start, locks are not */
    CHECK(get(h, "sel_pad") == "1" && get(h, "sel_cat_kick_on") == "1" && get(h, "sel_cat_snare_on") == "0" && get(h, "sel_lock_on") == "0",
          "fresh instance: pad 1 shows its default pool (kick) lit (kick=%s snare=%s)", get(h, "sel_cat_kick_on").c_str(), get(h, "sel_cat_snare_on").c_str());
    CHECK(peak(h, 4) == 0, "silent with no kit");

    trig(h, "generate");
    CHECK(get(h, "status").compare(0, 3, "Kit") == 0, "generate: \"%s\"", get(h, "status").c_str());
    CHECK(get(h, "pad1_name") != "(empty)" && get(h, "pad1_pill") == "KICK", "pad 1 = %s (%s)", get(h, "pad1_name").c_str(), get(h, "pad1_pill").c_str());
    int filled = 0; for (int i = 1; i <= 16; i++) filled += get(h, ("pad" + std::to_string(i) + "_name").c_str()) != "(empty)";
    CHECK(filled > 0, "%d pads assigned", filled);
    std::this_thread::sleep_for(std::chrono::milliseconds(600));       /* decode */
    uint8_t on[3] = {0x90, 0, 127};
    E->midi(h, on, 3);
    int pk = peak(h, 8);
    CHECK(pk > 10000, "note 0 (drum-pad layout) plays pad 1, peak %d", pk);
    peak(h, 100);      /* let the 8000-frame one-shot finish */
    CHECK(peak(h, 4) == 0, "one-shot ends");
    on[1] = 36; E->midi(h, on, 3);
    CHECK(peak(h, 8) > 10000, "note 36 plays pad 1 too");
    peak(h, 100);
    on[1] = 20; E->midi(h, on, 3);
    CHECK(peak(h, 8) == 0, "note 20 is not a pad");

    std::string st = get(h, "state");
    void *h2 = E->create(plug.c_str());
    E->set_param(h2, "state", st.c_str());
    CHECK(get(h2, "pad1_name") == get(h, "pad1_name"), "state restores the kit on a second instance");
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    { uint8_t m0[3] = {0x90, 0, 127}; E->midi(h2, m0, 3); }
    CHECK(peak(h2, 8) > 10000, "restored kit plays");

    std::string before = get(h, "pad1_name");
    E->set_param(h, "pad1_lock", "1");
    trig(h, "generate");
    CHECK(get(h, "pad1_name") == before && get(h, "pad1_pill") == "KICK" && get(h, "pad1_lock_on") == "1", "locked pad survives Generate and is lit (%s)", get(h, "pad1_pill").c_str());
    trig(h, "pad1_reroll");
    CHECK(get(h, "status").find("locked") != std::string::npos, "reroll on a locked pad refuses: \"%s\"", get(h, "status").c_str());
    E->set_param(h, "pad1_lock", "0");
    E->set_param(h, "sel_pad", "2");
    CHECK(get(h, "sel_pad") == "2" && get(h, "sel_name") == get(h, "pad2_name"), "sel_* follows the selected pad");
    CHECK(get(h, "sel_cat_snare_on") == "1" && get(h, "sel_cat_fx_on") == "0", "pad 2 shows its default pool (snare) lit");
    E->set_param(h, "sel_cat_fx", "1");
    E->set_param(h, "sel_cat_snare", "0");
    CHECK(get(h, "pad2_pill") == "FX" && get(h, "sel_cat_fx_on") == "1", "per-pad pool override: %s", get(h, "pad2_pill").c_str());
    E->set_param(h, "sel_cat_synth", "1");
    CHECK(get(h, "pad2_pill") == "FX+", "two categories: %s", get(h, "pad2_pill").c_str());
    std::this_thread::sleep_for(std::chrono::milliseconds(450));   /* a flip back within 400 ms is the same touch */
    E->set_param(h, "sel_cat_synth", "0");
    trig(h, "sel_reroll");
    CHECK(get(h, "sel_cat") == "fx", "pad 2 rerolled from the fx pool (%s)", get(h, "sel_cat").c_str());
    trig(h, "sel_clear");
    CHECK(get(h, "pad2_name") == "(empty)", "clear");

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    trig(h, "normalise");
    CHECK(get(h, "status") == "Levels matched", "match levels");
    int g1 = atoi(get(h, "pad1_gain").c_str());
    CHECK(g1 < 100 && g1 >= 15, "loud kick turned down toward the quiet pads: %d%%", g1);

    trig(h, "export");
    CHECK(wait_for(h, "status", "Exported"), "export: \"%s\"", get(h, "status").c_str());
    CHECK(get(h, "export_name").compare(0, 13, "PotentialKits") == 0, "export name %s", get(h, "export_name").c_str());
    std::vector<uint8_t> xb;
    CHECK(ld::read_file(root + "/kits/" + get(h, "export_name") + "/" + get(h, "export_name") + ".xpm", xb), "xpm written");

    /* settings: a saved source/export folder is applied, a scan follows, exports skip nothing but land where chosen */
    CHECK(get(h, "src_loc_name") == "Default (auto)" && get(h, "lib_info") == "15 samples", "default source: %s / %s", get(h, "src_loc_name").c_str(), get(h, "lib_info").c_str());
    std::string st2 = get(h, "state");
    CHECK(st2.find("\nsrc=\n") != std::string::npos, "state records the default source");
    void *h3 = E->create(plug.c_str());
    E->set_param(h3, "state", ("LD1\nsrc=" + lib + "/Kicks\nexp=" + root + "/chosen\n").c_str());
    CHECK(wait_for(h3, "status", "Library: 3"), "saved source triggers a scan of just that folder: \"%s\"", get(h3, "status").c_str());
    CHECK(get(h3, "lib_info") == "3 samples", "lib_info %s", get(h3, "lib_info").c_str());
    CHECK(get(h3, "src_loc_name").find("Kicks") != std::string::npos && get(h3, "exp_loc_name").find("chosen") != std::string::npos, "labels: %s | %s", get(h3, "src_loc_name").c_str(), get(h3, "exp_loc_name").c_str());
    trig(h3, "generate");
    int k3 = 0; for (int i = 1; i <= 16; i++) k3 += get(h3, ("pad" + std::to_string(i) + "_name").c_str()) != "(empty)";
    CHECK(k3 > 0 && get(h3, "pad1_pill") == "KICK" && get(h3, "pad2_name") == "(empty)", "only kicks to draw from: %d pads, pad1 %s/%s, pad2 %s", k3, get(h3, "pad1_pill").c_str(), get(h3, "pad1_name").c_str(), get(h3, "pad2_name").c_str());
    trig(h3, "export");
    CHECK(wait_for(h3, "status", "Exported"), "export: \"%s\"", get(h3, "status").c_str());
    std::vector<uint8_t> xb3;
    CHECK(ld::read_file(root + "/chosen/" + get(h3, "export_name") + "/" + get(h3, "export_name") + ".xpm", xb3), "xpm landed in the chosen export folder");
    trig(h3, "src_loc_next");
    CHECK(get(h3, "status").compare(0, 6, "Source") == 0, "stepping the source says to rescan: \"%s\"", get(h3, "status").c_str());
    /* follow: the selection moves to the last pad played */
    {
                CHECK(get(h3, "follow") == "1", "follow is on by default");
        uint8_t n3[3] = {0x90, 3, 100};
        E->midi(h3, n3, 3);
        CHECK(get(h3, "sel_pad") == "4", "note 3 selects pad 4");
        trig(h3, "pad7_play");
        CHECK(get(h3, "sel_pad") == "7", "a PLAY button selects its pad too");
        E->set_param(h3, "follow", "0");
        n3[1] = 9; E->midi(h3, n3, 3);
        CHECK(get(h3, "sel_pad") == "7", "follow off: the selection stays");
        CHECK(get(h3, "state").find("\nfollow=0\n") != std::string::npos, "follow is saved");
        E->set_param(h3, "follow", "1");
    }
    /* the category toggles show the SELECTED pad's pool and follow the selection; Unlock All clears the lit locks */
    {
        E->set_param(h3, "pad2_lock", "1");
        E->set_param(h3, "sel_pad", "2");
        E->set_param(h3, "sel_cat_fx", "1");
        CHECK(get(h3, "sel_cat_fx_on") == "1" && get(h3, "sel_cat_snare_on") == "1" && get(h3, "sel_cat_kick_on") == "0", "pad 2 selected: default snare plus the added fx lit");
        uint8_t n0[3] = {0x90, 0, 100};
        E->midi(h3, n0, 3);
        CHECK(get(h3, "sel_pad") == "1" && get(h3, "sel_cat_fx_on") == "0" && get(h3, "sel_cat_kick_on") == "1", "following to pad 1: its own default (kick) is lit, fx is not");
        n0[1] = 1; E->midi(h3, n0, 3);
        CHECK(get(h3, "sel_cat_fx_on") == "1" && get(h3, "pad2_lock_on") == "1" && get(h3, "sel_lock_on") == "1", "back on pad 2: fx and lock lit");
        trig(h3, "unlock_all");
        CHECK(get(h3, "pad2_lock_on") == "0" && get(h3, "sel_lock_on") == "0", "Unlock All clears the lit locks");
        std::this_thread::sleep_for(std::chrono::milliseconds(450));
        E->set_param(h3, "sel_cat_fx", "0");
    }
    /* link mode: symlinks beside the .xpm instead of copies; loops/size settings are saved in the state */
    E->set_param(h3, "export_mode", "1");
    trig(h3, "export");
    CHECK(wait_for(h3, "status", "Exported"), "linked export: \"%s\"", get(h3, "status").c_str());
    {
        std::string kd = root + "/chosen/" + get(h3, "export_name");
        std::string cmd2 = "find '" + kd + "' -type l | grep -c . > " + root + "/links.txt";
        if (system(cmd2.c_str()) < 0) {}
        std::vector<uint8_t> lk; ld::read_file(root + "/links.txt", lk);
        CHECK(!lk.empty() && lk[0] != '0', "samples are symlinks to the originals (%c)", lk.empty() ? '?' : lk[0]);
    }
    E->set_param(h3, "skip_loops", "0");
    E->set_param(h3, "max_size", "3");
    std::string st3 = get(h3, "state");
    CHECK(st3.find("\nloops=0\n") != std::string::npos && st3.find("\nsize=3\n") != std::string::npos && st3.find("\nlink=1\n") != std::string::npos, "loops/size/link saved in the state");
    CHECK(get(h3, "status") == "'RESCAN' to apply" && get(h3, "max_size") == "3" && get(h3, "skip_loops") == "0", "settings read back");
    E->destroy(h3);

    /* ---- PotentialKits ---- */
    {
        void *k = E->create(plug.c_str());
        CHECK(wait_for(k, "status", "Library: 15"), "PK: library");
        char key[64];
        while (mpc_engine_changed(k, key, sizeof key)) {}
        /* KIT STYLES: pools, pitch, decay and choke on every unlocked pad, then a dip */
        trig(k, "style_2");   /* BOOM BAP */
        CHECK(get(k, "status") == "BOOM BAP kit", "style: \"%s\"", get(k, "status").c_str());
        E->set_param(k, "sel_pad", "9");
        CHECK(get(k, "sel_pitch") == "-2" && get(k, "sel_cat_kick_on") == "1" && get(k, "pad9_pill") == "KICK", "BOOM BAP pad 9: a kick 2 down (%s, %s)", get(k, "sel_pitch").c_str(), get(k, "pad9_pill").c_str());
        E->set_param(k, "sel_pad", "6");
        CHECK(get(k, "sel_choke") == "1" && get(k, "sel_decay") == "70", "BOOM BAP pad 6: open hat, choke 1, decay 70 (%s, %s)", get(k, "sel_choke").c_str(), get(k, "sel_decay").c_str());
        bool saw_gain = false, saw_pitch = false;
        while (mpc_engine_changed(k, key, sizeof key)) { saw_gain |= !strcmp(key, "pad1_gain"); saw_pitch |= !strcmp(key, "sel_pitch"); }
        CHECK(saw_gain && saw_pitch, "a style tells the host what changed");
        /* tiles: the sample's name, "*" when locked; a tap selects and plays (not in the first 3 s, not twice in one touch) */
        CHECK(get(k, "tile_1") == get(k, "pad1_name").substr(0, 16) || get(k, "tile_1").size() > 1, "tile text %s", get(k, "tile_1").c_str());
        E->set_param(k, "pad1_lock", "1");
        CHECK(get(k, "tile_1").compare(0, 2, "* ") == 0, "locked tile: %s", get(k, "tile_1").c_str());
        E->set_param(k, "pad1_lock", "0");
        E->set_param(k, "tile_3", "1");
        CHECK(get(k, "sel_pad") == "6", "a tile tap during the first 3 s is ignored");
        std::this_thread::sleep_for(std::chrono::milliseconds(3100));
        peak(k, 200);
        E->set_param(k, "tile_3", "1");
        CHECK(get(k, "sel_pad") == "3" && get(k, "tile_3_on") == "1" && get(k, "tile_6_on") == "0", "a tile tap selects its pad");
        CHECK(peak(k, 8) > 1000, "and plays it");
        peak(k, 200);
        /* PITCH: an octave up plays the 8000-frame sample in half the time; DECAY shortens it; both survive a state round trip */
        E->set_param(k, "sel_pad", "1");
        E->set_param(k, "sel_decay", "100");
        auto length = [&](void *hh) { uint8_t m[3] = {0x90, 0, 127}; E->midi(hh, m, 3); int b = 0; int16_t o[256];
            for (; b < 200; b++) { E->render(hh, o, 128); int pk2 = 0; for (int i = 0; i < 256; i++) pk2 = std::max(pk2, abs((int)o[i])); if (b > 0 && pk2 < 50) break; } return b; };
        E->set_param(k, "sel_pitch", "0");
        int full = length(k);
        E->set_param(k, "sel_pitch", "12");
        int up = length(k);
        CHECK(full >= 60 && up <= full / 2 + 2 && up >= full / 2 - 2, "PITCH +12 plays twice as fast: %d blocks vs %d", up, full);
        E->set_param(k, "sel_pitch", "0");
        E->set_param(k, "sel_decay", "20");
        int dec = length(k);
        CHECK(dec < full / 3, "DECAY 20 fades it out sooner: %d blocks vs %d", dec, full);
        /* CHOKE: a pad cuts off another in its group */
        E->set_param(k, "sel_decay", "100");
        E->set_param(k, "sel_choke", "2");
        E->set_param(k, "sel_pad", "2"); E->set_param(k, "sel_choke", "2"); E->set_param(k, "sel_decay", "100"); E->set_param(k, "sel_pitch", "0");
        E->set_param(k, "sel_gain", "0");   /* pad 2 silent, so what we hear is pad 1 */
        { uint8_t m[3] = {0x90, 0, 127}; E->midi(k, m, 3); }
        peak(k, 4);
        { uint8_t m[3] = {0x90, 1, 127}; E->midi(k, m, 3); }
        peak(k, 4);
        CHECK(peak(k, 4) < 50, "choke group 2: pad 2 cut pad 1 off");
        E->set_param(k, "sel_gain", "100");
        std::string ks = get(k, "state");
        CHECK(ks.find("px\t0\t0\t100\t2\n") != std::string::npos && ks.find("px\t1\t0\t100\t2\n") != std::string::npos, "pitch/decay/choke are saved");
        void *k2 = E->create(plug.c_str());
        E->set_param(k2, "state", ks.c_str());
        E->set_param(k2, "sel_pad", "9");
        CHECK(get(k2, "sel_pitch") == "-2", "and restored (%s)", get(k2, "sel_pitch").c_str());
        E->destroy(k2);
        /* export: a changed pad is written as heard (a new WAV), choke becomes the mute group, plain pads are copied */
        E->set_param(k, "export_mode", "0");
        E->set_param(k, "sel_pad", "1"); E->set_param(k, "sel_pitch", "12");
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        trig(k, "export");
        CHECK(wait_for(k, "status", "Exported"), "PK export: \"%s\"", get(k, "status").c_str());
        {
            std::string nm = get(k, "export_name"), x = root + "/kits/" + nm + "/" + nm + ".xpm";
            std::vector<uint8_t> xb2; ld::read_file(x, xb2);
            std::string xs(xb2.begin(), xb2.end());
            CHECK(xs.find("<MuteGroup>2</MuteGroup>") != std::string::npos, "choke 2 -> MuteGroup 2 in the program");
            std::vector<uint8_t> mf; ld::read_file(root + "/kits/" + nm + "/MANIFEST.txt", mf);
            std::string ms(mf.begin(), mf.end());
            CHECK(ms.find("[as heard") != std::string::npos, "pad 1 written as heard");
            size_t a = ms.find('\t');
            std::string first = ms.substr(ms.find("\n\n") + 2); first = first.substr(0, first.find('\t'));
            ld::Pcm pc; CHECK(ld::decode(root + "/kits/" + nm + "/" + first, pc) && pc.frames() >= 3900 && pc.frames() <= 4100, "the as-heard WAV is an octave up: %u frames", (unsigned)pc.frames()); (void)a;
        }
        /* linked export: the original is linked and PITCH becomes the program's tune */
        E->set_param(k, "export_mode", "1");
        trig(k, "export");
        CHECK(wait_for(k, "status", "Exported"), "PK linked export");
        {
            std::string nm = get(k, "export_name");
            std::vector<uint8_t> xb2; ld::read_file(root + "/kits/" + nm + "/" + nm + ".xpm", xb2);
            std::string xs(xb2.begin(), xb2.end());
            CHECK(xs.find("<TuneCoarse>12</TuneCoarse>") != std::string::npos, "linked: PITCH as TuneCoarse 12");
        }
        E->set_param(k, "sel_pitch", "0");
        /* ROLL at each wildness: MILD keeps most pads, MEDIUM and WILD make a new kit */
        std::string b1[16];
        for (int i = 0; i < 16; i++) b1[i] = get(k, ("pad" + std::to_string(i + 1) + "_name").c_str());
        E->set_param(k, "wild", "0"); trig(k, "dice");
        int same = 0; for (int i = 0; i < 16; i++) same += get(k, ("pad" + std::to_string(i + 1) + "_name").c_str()) == b1[i];
        CHECK(same >= 10, "MILD rolls only a few pads: %d of 16 kept (%s)", same, get(k, "status").c_str());
        E->set_param(k, "wild", "2"); trig(k, "dice");
        CHECK(get(k, "status") == "Rolled: WILD", "WILD: \"%s\"", get(k, "status").c_str());
        CHECK(get(k, "state").find("\nwild=2\n") != std::string::npos, "the wildness is saved");
        E->set_param(k, "wild", "1"); trig(k, "dice");
        CHECK(get(k, "status") == "Rolled: new kit", "MEDIUM: \"%s\"", get(k, "status").c_str());
        /* USER kits: SAVE then a slot keeps the kit in <drive>/PotentialSuite Presets/PotentialKits; a slot brings it back */
        CHECK(get(k, "user_3") == "EMPTY 3", "slot empty: %s", get(k, "user_3").c_str());
        std::string kit_now = get(k, "pad1_name");
        E->set_param(k, "user_save", "1");
        E->set_param(k, "user_3", "1");
        CHECK(get(k, "user_3") == "USER 3" && get(k, "user_3_on") == "1" && get(k, "user_save") == "0", "saved to USER 3 (%s)", get(k, "status").c_str());
        bool saw_save = false; while (mpc_engine_changed(k, key, sizeof key)) saw_save |= !strcmp(key, "user_save");
        CHECK(saw_save, "SAVE going out is told to the host");
        std::vector<uint8_t> uf;
        CHECK(ld::read_file(root + "/PotentialSuite Presets/PotentialKits/kit3.txt", uf) && uf.size() > 20, "the kit file is beside VST2 Plugins");
        std::string ukit(uf.begin(), uf.end());
        CHECK(ukit.find("src=") == std::string::npos && ukit.find("\npad\t0\t") != std::string::npos, "a USER kit holds the pads, not the folders");
        trig(k, "generate");
        std::this_thread::sleep_for(std::chrono::milliseconds(450));
        void *k3 = E->create(plug.c_str());   /* another copy of the plugin sees the slot */
        std::this_thread::sleep_for(std::chrono::milliseconds(3100));
        CHECK(get(k3, "user_3") == "USER 3", "another copy sees USER 3");
        E->set_param(k3, "user_3", "1");
        CHECK(get(k3, "pad1_name") == kit_now && get(k3, "status") == "Loaded USER 3", "and loads it: %s (%s)", get(k3, "pad1_name").c_str(), get(k3, "status").c_str());
        E->set_param(k3, "user_5", "1");
        CHECK(get(k3, "status") == "USER 5 is empty", "an empty slot loads nothing");
        /* one touch on a drawn button arrives as two flips, on then straight back off (MPC One, 2026-11-14) */
        std::this_thread::sleep_for(std::chrono::milliseconds(450));
        while (mpc_engine_changed(k3, key, sizeof key)) {}
        E->set_param(k3, "sel_pad", "4");
        E->set_param(k3, "sel_lock", "1"); E->set_param(k3, "sel_lock", "0");
        CHECK(get(k3, "pad4_lock_on") == "1" && get(k3, "tile_4").compare(0, 2, "* ") == 0, "LOCK: one touch (two flips) stays locked (%s)", get(k3, "tile_4").c_str());
        std::this_thread::sleep_for(std::chrono::milliseconds(450));
        E->set_param(k3, "sel_lock", "0"); E->set_param(k3, "sel_lock", "1");
        CHECK(get(k3, "pad4_lock_on") == "0", "LOCK: a second touch unlocks");
        E->set_param(k3, "user_save", "1"); E->set_param(k3, "user_save", "0");
        CHECK(get(k3, "user_save") == "1", "SAVE: one touch (two flips) stays armed");
        bool told = false; while (mpc_engine_changed(k3, key, sizeof key)) told |= !strcmp(key, "user_save");
        CHECK(told, "SAVE: the host is told it is still on");
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        E->set_param(k3, "user_6", "1");
        CHECK(get(k3, "user_6") == "USER 6" && get(k3, "status") == "Saved to USER 6", "SAVE then an EMPTY slot saves the rolled kit (%s)", get(k3, "status").c_str());
        /* TYPE: AUTO, then one kind of sound at a time; the pad is rerolled to that kind */
        E->set_param(k3, "sel_pad", "1");
        trig(k3, "unlock_all");
        for (int i = 0; i < 30 && get(k3, "sel_type").compare(0, 4, "AUTO") != 0; i++) trig(k3, "sel_type_prev");
        CHECK(get(k3, "sel_type") == "AUTO: KICK", "TYPE on AUTO shows pad 1's usual sound (%s)", get(k3, "sel_type").c_str());
        trig(k3, "sel_type_next");
        CHECK(get(k3, "sel_type") == "KICK" && get(k3, "sel_cat_kick_on") == "1", "TYPE > KICK (%s)", get(k3, "sel_type").c_str());
        trig(k3, "sel_type_next");
        CHECK(get(k3, "sel_type") == "SNARE" && get(k3, "pad1_cat") == "snare", "TYPE > SNARE rerolls pad 1 to a snare (%s, %s, %s)", get(k3, "sel_type").c_str(), get(k3, "pad1_cat").c_str(), get(k3, "status").c_str());
        trig(k3, "sel_type_prev"); trig(k3, "sel_type_prev");
        CHECK(get(k3, "sel_type").compare(0, 4, "AUTO") == 0, "TYPE back to AUTO (%s)", get(k3, "sel_type").c_str());
        trig(k3, "sel_type_prev");
        CHECK(get(k3, "sel_type") == "OTHER", "TYPE wraps around (%s)", get(k3, "sel_type").c_str());
        E->destroy(k3);
        E->destroy(k);
    }
    E->destroy(h2);
    E->destroy(h);      /* joins the worker and scanner threads; ASan checks nothing leaks or races a freed buffer */
    std::string cmd = "rm -rf " + root; if (system(cmd.c_str())) {}
    printf(fails ? "FAILED (%d)\n" : "PASSED\n", fails);
    return fails ? 1 : 0;
}

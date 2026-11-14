#!/bin/sh
# Puts sd88me's Lucky Dip back to its own name, "Lucky Dip", in the MPC's plugin list.
# (The old personal PotentialKits installer renamed it. The real PotentialKits is its own plugin now.)
#
# What it changes, only where Lucky Dip (plugin ID 4c6b4470 / LkDp) is called "PotentialKits":
#   1. "name" in Lucky Dip's plugin.json, in any VST2 Plugins or Synths folder on any drive
#   2. name + descriptiveName of Lucky Dip's entry in MPC.settings (MPC is stopped for this and started again)
# Backups: plugin.json.bak-luckydip and MPC.settings.bak-luckydip-<date>. The real PotentialKits is not touched.
# Run on the MPC:  sh /tmp/luckydip-name.sh        Undo: put the .bak-luckydip files back.

NEW="Lucky Dip"
changed=0

echo "== plugin.json files"
for pj in /media/*/"VST2 Plugins"/*/plugin.json /media/*/Synths*/*/plugin.json /media/*/Synths*/*/*/plugin.json; do
    [ -f "$pj" ] || continue
    dir=$(dirname "$pj")
    case "$dir" in *"PotentialBreaks - VST - PotentialKits"*) continue ;; esac
    is_ld=0
    grep -qi '4c6b4470\|1282098288\|"LkDp"' "$pj" && is_ld=1
    case "$dir" in *[Ll]ucky*[Dd]ip*) is_ld=1 ;; esac
    [ $is_ld = 1 ] || continue
    if grep -q '"name"[[:space:]]*:[[:space:]]*"PotentialKits"' "$pj"; then
        cp "$pj" "$pj.bak-luckydip"
        sed -i 's/"name"[[:space:]]*:[[:space:]]*"PotentialKits"/"name": "Lucky Dip"/' "$pj"
        echo "renamed: $pj"
        changed=1
    else
        echo "already fine: $pj ($(grep -o '"name"[^,]*' "$pj" | head -1))"
    fi
done

echo "== MPC.settings"
S=""
for f in /media/az01-internal/Settings/MPC/MPC.settings /data/Settings/MPC/MPC.settings; do
    [ -f "$f" ] && { S="$f"; break; }
done
if [ -z "$S" ]; then
    echo "no MPC.settings found (nothing to do there)"
elif ! awk 'BEGIN{IGNORECASE=1} /<PLUGIN/{b=""; inb=1} inb{b=b $0 "\n"} inb && /\/>/{ if (tolower(b) ~ /4c6b4470/ && b ~ /PotentialKits/) f=1; inb=0 } END{exit !f}' "$S"; then
    echo "Lucky Dip's entry is not called PotentialKits there (nothing to do)"
else
    echo "stopping MPC to edit $S ..."
    systemctl stop acvs 2>/dev/null; sleep 3
    cp "$S" "$S.bak-luckydip-$(date +%Y%m%d-%H%M%S)"
    awk -v NEW="$NEW" '
        function fix(b) {
            if (tolower(b) ~ /4c6b4470/ && b ~ /PotentialKits/) {
                gsub(/descriptiveName="PotentialKits"/, "descriptiveName=\"" NEW "\"", b)
                n = split(b, L, "\n"); out = ""
                for (i = 1; i <= n; i++) {
                    l = L[i]
                    if (match(l, /(^|[ \t<])name="PotentialKits"/)) {
                        pre = substr(l, 1, RSTART - 1); hit = substr(l, RSTART, RLENGTH); post = substr(l, RSTART + RLENGTH)
                        lead = substr(hit, 1, 1); if (lead == "n") lead = ""
                        l = pre lead "name=\"" NEW "\"" post
                    }
                    out = out l (i < n ? "\n" : "")
                }
                b = out
            }
            return b
        }
        /<PLUGIN/ { inb = 1; b = "" }
        inb { b = b (b == "" ? "" : "\n") $0; if ($0 ~ /\/>/) { print fix(b); inb = 0 }; next }
        { print }
        END { if (inb) print b }
    ' "$S" > "$S.new" && [ -s "$S.new" ] && mv "$S.new" "$S" && sync && echo "renamed in MPC.settings" && changed=1
    systemctl start acvs 2>/dev/null
fi

[ $changed = 1 ] && echo "DONE: Lucky Dip is called Lucky Dip again (restart the MPC if the list still shows the old name)" \
                 || echo "DONE: nothing needed changing"

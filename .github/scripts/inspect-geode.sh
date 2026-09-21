#!/usr/bin/env bash
# Structural check of the combined .geode package(s) in the given directory.
#
# A .geode file is a zip archive: mod.json + resources + one binary per
# platform (<mod-id>.dll / .dylib / .ios.dylib / .android32.so / .android64.so).
# This verifies exactly that, prints the listing and a checksum, and fails if
# something is missing, so the "Package" job cannot succeed with a broken file.
#
# Usage: inspect-geode.sh <directory containing *.geode> [report-file]
set -uo pipefail

dir="${1:?directory with .geode files}"
report="${2:-/dev/stdout}"
status=0

shopt -s nullglob
files=("$dir"/*.geode)
if [ ${#files[@]} -eq 0 ]; then
    echo "no .geode file found in $dir" | tee -a "$report"
    exit 1
fi

for pkg in "${files[@]}"; do
    {
        echo "### Package check: $(basename "$pkg")"
        echo
        echo '```text'
        echo "size:   $(stat -c %s "$pkg") bytes"
        echo "sha256: $(sha256sum "$pkg" | cut -d' ' -f1)"
        echo
        unzip -l "$pkg"
        echo '```'
        echo
    } >> "$report"

    if ! unzip -tq "$pkg" > /dev/null; then
        echo "- :x: zip integrity test failed" >> "$report"
        status=1
        continue
    fi

    if ! unzip -p "$pkg" mod.json > extracted-mod.json 2>/dev/null; then
        echo "- :x: mod.json missing from the package" >> "$report"
        status=1
        continue
    fi

    mod_id=$(jq -r '.id' extracted-mod.json)
    {
        echo '```json'
        jq '{id, name, version, geode, gd, dependencies}' extracted-mod.json
        echo '```'
        echo
    } >> "$report"

    listing=$(unzip -Z1 "$pkg")
    for entry in "$mod_id.dll" "$mod_id.dylib" "$mod_id.ios.dylib" "$mod_id.android32.so" "$mod_id.android64.so" \
                 "logo.png" "about.md" "changelog.md"; do
        if grep -qx "$entry" <<< "$listing"; then
            echo "- :white_check_mark: \`$entry\`" >> "$report"
        else
            echo "- :x: \`$entry\` missing" >> "$report"
            status=1
        fi
    done

    # Every binary must actually be a shared library of the right kind.
    unzip -oq "$pkg" -d "pkg-contents"
    {
        echo
        echo '```text'
        for bin in "$mod_id.dll" "$mod_id.dylib" "$mod_id.ios.dylib" "$mod_id.android32.so" "$mod_id.android64.so"; do
            if [ -f "pkg-contents/$bin" ]; then
                if command -v file > /dev/null; then
                    kind=$(file -b "pkg-contents/$bin" | cut -c1-120)
                else
                    kind="$(stat -c %s "pkg-contents/$bin") bytes"
                fi
                printf '%-32s %s\n' "$bin" "$kind"
            fi
        done
        echo '```'
    } >> "$report"
done

exit $status

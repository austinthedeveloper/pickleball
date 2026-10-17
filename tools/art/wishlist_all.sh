#!/bin/sh
# Builds every sprite wish list draft (docs/IDEAS.md) and a contact sheet,
# build/wishlist/sheet.png: authoring only. ASEPRITE overrides the path.
set -e
cd "$(dirname "$0")/../.."
A="${ASEPRITE:-$HOME/Library/Application Support/Steam/steamapps/common/Aseprite/Aseprite.app/Contents/MacOS/aseprite}"
mkdir -p build/wishlist
for src in assets/source/wishlist/*.txt; do
    name=$(basename "$src" .txt)
    "$A" -b --script-param src="$src" \
        --script-param out="assets/source/wishlist/$name" \
        --script-param preview="build/wishlist/$name" \
        --script tools/art/wishlist.lua
done
"$A" -b --script-param dir=assets/source/wishlist \
    --script-param out=build/wishlist/sheet.png \
    --script tools/art/wishlist_sheet.lua
echo "built $(ls assets/source/wishlist/*.txt | wc -l | tr -d ' ') drafts; sheet: build/wishlist/sheet.png"

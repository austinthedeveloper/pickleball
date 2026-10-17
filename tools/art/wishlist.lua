-- Sprite wish list drafts (docs/IDEAS.md, "Sprite wish list"): a text grid
-- (one character a pixel, '.' and 1-9 A-F the palette index) to an indexed
-- .aseprite and .png, plus 8x previews (plain, and over the court's teal).
-- Authoring only, not part of the build. Once a draft is opened and edited
-- in Aseprite, the .aseprite is its source.
--
--   aseprite -b --script-param src=assets/source/wishlist/slump.txt \
--     --script-param out=assets/source/wishlist/slump \
--     --script-param preview=build/wishlist/slump \
--     --script-param pal=kit --script tools/art/wishlist.lua
--
-- pal (or "pal=<name>" in the grid's header): kit (the player roles, ADR-013, blue team), green (the same with a
-- green team part), objects (objects.png's palette).
local src = app.params["src"]
local out = app.params["out"]
local preview = app.params["preview"] or out
local kit = {
  {255,0,255},{16,16,48},{176,104,72},{240,168,120},{248,208,160},
  {24,48,152},{48,96,224},{120,168,248},{168,176,200},{248,248,248},
  {240,240,240},{32,56,160},{72,120,240},{56,40,32},{255,255,255},{96,56,32}}
local objects = {
  {255,0,255},{40,40,16},{224,232,48},{255,255,200},{0,0,0},{255,220,64},
  {255,255,255},{80,88,104},{136,144,160},{192,200,212},{0,0,0},{0,0,0},
  {0,0,0},{0,0,0},{0,0,0},{96,56,32}}
-- The palette: the pal param, else "pal=<name>" in the grid's header.
local which = app.params["pal"]
if not which then
  for line in io.lines(src) do
    which = which or line:match("^#.*pal=(%w+)")
  end
end
local pal = kit
if which == "objects" then pal = objects end
if which == "green" then
  pal = {}
  for i, c in ipairs(kit) do pal[i] = c end
  pal[6] = {16,88,24}; pal[7] = {48,152,48}; pal[8] = {128,208,96}
end
local rows = {}
for line in io.lines(src) do
  if line:sub(1,1) ~= "#" and #line > 0 then table.insert(rows, line) end
end
local h, w = #rows, #rows[1]
local spr = Sprite(w, h, ColorMode.INDEXED)
local p = Palette(16)
for i, c in ipairs(pal) do p:setColor(i - 1, Color{r=c[1], g=c[2], b=c[3]}) end
spr:setPalette(p)
spr.transparentColor = 0
local img = spr.cels[1].image
local sym = ".123456789ABCDEF"
for y = 1, h do
  local row = rows[y]
  assert(#row == w, src .. ": row " .. y .. " is " .. #row .. " wide")
  for x = 1, w do
    local ch = row:sub(x, x)
    local idx = sym:find(ch, 1, true)
    assert(idx, src .. ": bad char " .. ch)
    img:drawPixel(x - 1, y - 1, idx - 1)
  end
end
spr:saveAs(out .. ".aseprite")
spr:saveCopyAs(out .. ".png")
local scale = tonumber(app.params["scale"] or "8")
app.command.SpriteSize{ ui=false, scale=scale, method="nearest" }
spr:saveCopyAs(preview .. "_preview.png")
-- And over the court's teal, to judge the art where it is drawn.
local q = Palette(spr.palettes[1])
q:resize(17)
q:setColor(0, Color{r=48, g=144, b=128})
q:setColor(16, Color{r=255, g=0, b=255})
spr:setPalette(q)
spr.transparentColor = 16
spr:saveCopyAs(preview .. "_court.png")

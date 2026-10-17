-- Contact sheet of the sprite wish list drafts (tools/art/wishlist_all.sh):
-- every .png in dir, in name order, at 4x over the court's teal, in rows.
local dir = app.params["dir"]
local out = app.params["out"]
local scale, gap, width = 4, 8, 640
local names = {}
for _, f in ipairs(app.fs.listFiles(dir)) do
  if f:match("%.png$") then table.insert(names, f) end
end
table.sort(names)
-- Lay out first, to size the sheet.
local images, places = {}, {}
local x, y, row_h = gap, gap, 0
for _, f in ipairs(names) do
  local spr = app.open(app.fs.joinPath(dir, f))
  app.command.ChangePixelFormat{ format="rgb" }
  local img = Image(spr.width * scale, spr.height * scale, ColorMode.RGB)
  local src = spr.cels[1].image
  for py = 0, spr.height - 1 do
    for px = 0, spr.width - 1 do
      local v = src:getPixel(px, py)
      for sy = 0, scale - 1 do
        for sx = 0, scale - 1 do
          img:drawPixel(px * scale + sx, py * scale + sy, v)
        end
      end
    end
  end
  spr:close()
  if x + img.width + gap > width then
    x = gap; y = y + row_h + gap; row_h = 0
  end
  table.insert(images, img); table.insert(places, {x, y})
  x = x + img.width + gap
  if img.height > row_h then row_h = img.height end
end
local sheet = Sprite(width, y + row_h + gap, ColorMode.RGB)
local base = sheet.cels[1].image
base:clear(app.pixelColor.rgba(48, 144, 128, 255))
for i, img in ipairs(images) do
  base:drawImage(img, Point(places[i][1], places[i][2]))
end
sheet:saveAs(out)

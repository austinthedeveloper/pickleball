-- Headless MesenCE driver: plays a scripted pad timeline, saves screenshots,
-- and prints work-RAM probes. It supplies controller input and writes
-- emulated memory only when SMOKE_POKES asks (bug repros, never smoke or
-- perf runs).
--
-- Environment:
--   SMOKE_TIMELINE  file with lines "<first> <last> <buttons>" (inclusive
--                   frame range, buttons comma-separated: b,start,left,...)
--                   and "shot <frame>" lines; '#' starts a comment.
--   SMOKE_OUT       directory for frame-NNNNN.png screenshots.
--   SMOKE_FRAMES    total frames to run before exiting (default 600).
--   SMOKE_PROBES    optional "name=wramhex:size,..." values. Sizes 1 and 2
--                   print as decimal; larger sizes print as a hex dump.
--   SMOKE_PROBE_EVERY  "1" to print probes on every frame (golden traces).
--   SMOKE_NOSHOT    "1" to skip screenshots (probes still print).
--   SMOKE_POKES     optional work-RAM writes, ";"-separated, each
--                   "first,last,once,condaddr,condsize,condvalue,addr,size,
--                   value" in decimal (condsize 0: no condition; last -1:
--                   no end). tools/mesen_probe.py builds them from symbols.

local timeline_path = os.getenv("SMOKE_TIMELINE")
local out = os.getenv("SMOKE_OUT") or "."
local total = tonumber(os.getenv("SMOKE_FRAMES") or "600")
local every = os.getenv("SMOKE_PROBE_EVERY") == "1"
local noshot = os.getenv("SMOKE_NOSHOT") == "1"
local ranges = {}
local shots = {}
local frame = 0
local probes = {}
local pokes = {}

for name, addr, size in (os.getenv("SMOKE_PROBES") or ""):gmatch(
        "([%w_]+)=(%x+):(%d+)") do
    table.insert(probes, { name = name, addr = tonumber(addr, 16),
        size = tonumber(size) })
end

local function probeLine()
    local parts = {}
    for _, p in ipairs(probes) do
        local value
        if p.size == 1 then
            value = emu.read(p.addr, emu.memType.snesWorkRam, false)
        elseif p.size == 2 then
            value = emu.readWord(p.addr, emu.memType.snesWorkRam, false)
        else
            local bytes = {}
            for i = 0, p.size - 1 do
                bytes[#bytes + 1] = string.format("%02x",
                    emu.read(p.addr + i, emu.memType.snesWorkRam, false))
            end
            value = table.concat(bytes)
        end
        table.insert(parts, p.name .. "=" .. value)
    end
    return table.concat(parts, " ")
end

for spec in (os.getenv("SMOKE_POKES") or ""):gmatch("[^;]+") do
    local v = {}
    for n in spec:gmatch("-?%d+") do
        v[#v + 1] = tonumber(n)
    end
    table.insert(pokes, { first = v[1], last = v[2], once = v[3] == 1,
        caddr = v[4], csize = v[5], cvalue = v[6],
        addr = v[7], size = v[8], value = v[9] })
end

local function readRam(addr, size)
    if size == 2 then
        return emu.readWord(addr, emu.memType.snesWorkRam, false)
    end
    return emu.read(addr, emu.memType.snesWorkRam, false)
end

-- Pokes run at the end of a frame, so the game sees them on the next one.
local function applyPokes()
    for _, p in ipairs(pokes) do
        if not p.done and frame >= p.first and
                (p.last < 0 or frame <= p.last) and
                (p.csize == 0 or readRam(p.caddr, p.csize) == p.cvalue) then
            if p.size == 2 then
                emu.writeWord(p.addr, p.value, emu.memType.snesWorkRam)
            else
                emu.write(p.addr, p.value, emu.memType.snesWorkRam)
            end
            if not p.logged then
                print(string.format("frame %d poke %x=%d", frame, p.addr,
                    p.value))
                p.logged = true
            end
            p.done = p.once
        end
    end
end

if timeline_path then
    for line in io.lines(timeline_path) do
        line = line:gsub("#.*$", "")
        local shot = line:match("^%s*shot%s+(%d+)")
        if shot then
            shots[tonumber(shot)] = true
        else
            local first, last, buttons = line:match("^%s*(%d+)%s+(%d+)%s+(%S+)")
            if first then
                table.insert(ranges, {
                    first = tonumber(first), last = tonumber(last),
                    buttons = buttons,
                })
            end
        end
    end
end

local function inputForFrame(n)
    local input = {
        a = false, b = false, x = false, y = false,
        l = false, r = false, select = false, start = false,
        up = false, down = false, left = false, right = false,
    }
    for _, range in ipairs(ranges) do
        if n >= range.first and n <= range.last then
            for button in range.buttons:gmatch("[^,]+") do
                input[button] = true
            end
        end
    end
    return input
end

emu.addEventCallback(function()
    emu.setInput(inputForFrame(frame), 0, 0)
end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    applyPokes()
    if #probes > 0 and (every or shots[frame]) then
        print(string.format("frame %d %s", frame, probeLine()))
    end
    if shots[frame] and not noshot then
        local file = assert(io.open(
            string.format("%s/frame-%05d.png", out, frame), "wb"))
        file:write(emu.takeScreenshot())
        file:close()
    end
    frame = frame + 1
    if frame >= total then
        emu.stop(0)
    end
end, emu.eventType.endFrame)

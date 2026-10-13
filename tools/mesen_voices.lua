-- Headless MesenCE sound probe (ADR-029/031, `make perf`): plays a pad
-- timeline and prints "frame N voice V starts" whenever an SPC700 voice's
-- envelope rises from silence, from emu.getState()'s DSP voice state. It
-- shows that sounds really play, not just that the game asked for them.
--   SMOKE_TIMELINE  absolute path of a test/smoke timeline
--   SMOKE_FRAMES    frames to run
local ranges = {}
for line in io.lines(os.getenv("SMOKE_TIMELINE")) do
  line = line:gsub("#.*$", "")
  local a, b, btn = line:match("^%s*(%d+)%s+(%d+)%s+(%S+)")
  if a then table.insert(ranges, {tonumber(a), tonumber(b), btn}) end
end
local total = tonumber(os.getenv("SMOKE_FRAMES") or "600")
local frame, previous = 0, {}

emu.addEventCallback(function()
  local input = {}
  for _, r in ipairs(ranges) do
    if frame >= r[1] and frame <= r[2] then
      for btn in r[3]:gmatch("[^,]+") do input[btn] = true end
    end
  end
  emu.setInput(input, 0, 0)
end, emu.eventType.inputPolled)

emu.addEventCallback(function()
  frame = frame + 1
  local state = emu.getState()
  for v = 0, 7 do
    local env = state["spc.dsp.voices[" .. v .. "].envVolume"] or 0
    if env > 0 and (previous[v] or 0) == 0 then
      print(string.format("frame %d voice %d starts", frame, v))
    end
    previous[v] = env
  end
  if frame >= total then
    print("done " .. frame)
    emu.stop(0)
  end
end, emu.eventType.endFrame)

# Technical spikes (T0.4.4)

Run on 2026-09-28 against the real application (headless under Xvfb) through a temporary MCP tool that is not
committed. The only code kept is the `InputContext::handleSynthetic()` hook, because the pen engine (T2.2.1) needs it.

## (a) Synthetic pen input through the real input pipeline — ✅ feasible

**Mechanism.** A `InputEvent` is built by hand and passed to
`InputContext::handleSynthetic()`. This is a new 3-line public entry point that shares `dispatch()` with
`InputContext::handle(GdkEvent*)`, so it follows the same route as hardware events: hand recognition →
geometry-tool handler → `StylusInputHandler` → `PenInputHandler` → the tool handlers (`StrokeHandler`,
`EraseHandler`, selection, …). The fields set on the event:

| Field | Value |
|---|---|
| `deviceClass` | `INPUT_DEVICE_PEN` |
| `device` / `deviceId` | the default seat's pointer (`gdk_seat_get_pointer`); the event is only valid with a non-null device |
| `relative` | layout pixel coordinates = `XojPageView::getPixelPosition() + page_point * zoom` |
| `absolute` | `relative - (hadjustment, vadjustment)`, i.e. widget coordinates |
| `type` | `BUTTON_PRESS_EVENT`, then `MOTION_EVENT`…, then `BUTTON_RELEASE_EVENT` |
| `button`, `state` | `1`; `GDK_BUTTON1_MASK` while moving |
| `pressure` | raw 0..1; `timestamp` in ms |

**Results**

| Test | Outcome |
|---|---|
| Pen, pressure ramp 0.1 → 1.0 → 0.1 over 41 points | Stroke created with 41 points. Per-point width `z` = 0.14 → 1.41 → 0.14, i.e. exactly `max(minPressure, p × multiplier) × toolWidth` (width 1.41, multiplier 1) |
| Highlighter, same trajectory | Stroke created with width 8.5 and **no** per-point pressure (`z = -1`) |
| Eraser swipe across three existing strokes | All three strokes removed (the default eraser type erases whole strokes); the agent can erase "by hand" |
| Page not scrolled into view | Works. The page is found through the layout, not the viewport |

**Findings and design consequences**

1. The hardware pressure mapping lives in `PenInputHandler::filterPressure()`:
   `max(settings.minimumPressure, p × settings.pressureMultiplier)`. `StrokeHandler` then multiplies by the tool
   width. Pressure is only used if *Settings → pressure sensitivity* is on (`getInputDataRelativeToCurrentPage`).
   The pen engine therefore reproduces the user's own stylus exactly, and the direct engine must apply the same
   formula to be "hardware-equivalent" (PressureModel, T2.1.1).
2. Only the pen tool uses pressure (`StrokeTool::isPressureSensitive()` is true only for `PEN`). Highlighter strokes
   are constant-width, as they are for a human.
3. The pen engine drives the *current* tool. It must save the user's tool, select the requested one and restore it
   afterwards (verified: `Control::selectTool()` works from a tool call).
4. `StylusInputHandler::handle()` returns `false` for motion events even when they are processed. The return value
   is not a success signal; check the model instead.
5. Under Xvfb, every synthetic pen event logs `No device found …` from `DeviceListHelper::getDeviceList()`, which
   hand recognition triggers. This is harmless and does not appear on a desktop with real input devices. The
   pen engine can later use a quieter path if needed.
6. The events are processed synchronously. For visible animation the engine has to spread them over main-loop
   ticks (`g_timeout_add`), and it must not run while the user is mid-stroke (collision queue).

## (b) nanosvg → strokes — ✅ feasible

`nsvgParse(text, "pt", 72)` on a test SVG with a rectangle, a circle, a cubic path and `<text>`:

| Element | Parsed as |
|---|---|
| `<rect … stroke="red" fill="none">` | 1 closed path, 4 cubic segments, stroke paint, width 2 |
| `<circle … fill="#00f">` | 1 closed path, 5 cubic segments, fill paint only |
| `<path d="M10 90 C …">` | 1 open path, 1 cubic segment |
| `<text>` | **ignored** (nanosvg has no text support) |

Consequences for `create_from_svg` (T2.1.5):
- Flatten the cubics adaptively (by chord length) into polylines, with points spaced for stylus-like density.
- Map stroke paint to the stroke color, stroke-width to width, and fill paint to the Xournal fill (closed paths).
- `<text>` needs our own pre-pass that turns it into text elements.
- Gradients are not supported by Xournal strokes; fall back to the first stop color and report a warning.

## (c) Highlighter and pressure behaviour — ✅ understood

- The highlighter never uses pressure, whether drawn by the pen engine or created directly. Profiles apply to the
  pen only; for highlighter strokes the width is constant.
- Stroke `z` stores the absolute width in points, or `-1` for "no pressure". The file format stores it the same
  way, so xjson can round-trip widths exactly.

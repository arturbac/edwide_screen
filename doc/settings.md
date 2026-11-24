# edwide_screen settings

<!-- Made by edwide_screen from its table of settings (src/settings_log.cc, `EDWIDE_SETTINGS_DOC`); tools/test.sh checks
     it is up to date. Change the table, not this file. -->

In `edwide_screen.ini`, in `edloader\config` (beside the dll without edloader); edwide_screen makes it at the game's first start with every setting as a comment, so the correction is off until it is set. A line without `;` sets a value (`;` and `#` start a comment).

- **Read while the game runs**: edwide_screen looks at the ini when the game shows a frame, at most once a second, and reads it again when the file was saved since; the log has the values read (`settings: ...`).
- **Read at the game's start**: a change takes a new start of the game.
- **Range**: the values that make sense; *(clamped)* = edwide_screen holds the value to it, elsewhere it does not check; empty = any value.

## Turning it on

### Read while the game runs

| setting | default | range | what |
|---|---|---|---|
| `wings` | `0` | 0.5 or above = on | 1 = the sides laid over monitors set at an angle; 0 = off |

### Read at the game's start (a change needs a restart)

None.

## The monitors

### Read while the game runs

| setting | default | range | what |
|---|---|---|---|
| `wings_angle` | `30` | -80..80 (clamped) | the side monitors' angle towards you, degrees (0 = in one line with the middle) |
| `wings_center_px` | `0` | 0 or less = off | the middle monitor's width in pixels of the game's picture; 0 = not set (off); e.g. 3000 of a picture 9000 wide |
| `wings_side_px` | `0` | above 0: the picture's width must be wings_center_px + 2 x (wings_side_px + wings_bezel_px) | each side monitor's width in pixels; 0 = the rest of the picture (any width); when set, a picture of another width is left alone |
| `wings_bezel_px` | `0` |  | pixels of the picture behind the bezels between the middle and a side (0 = none); only when the graphics driver adds them |

### Read at the game's start (a change needs a restart)

None.

## The distance

### Read while the game runs

| setting | default | range | what |
|---|---|---|---|
| `wings_eye_cm` | `0` | used when it and wings_center_cm are above 0 | your eyes' distance from the middle monitor, cm (with wings_center_cm: the sides as seen from there; 0 = from the field of view) |
| `wings_center_cm` | `0` | used when it and wings_eye_cm are above 0 | the middle monitor's picture width, cm (0 = from the field of view) |
| `wings_fov` | `0` | used above 0; works between 1 and 179 | only without wings_eye_cm: the game's vertical field of view, degrees; 0 = read from the game's Settings.xml; used unless wings_eye_cm and wings_center_cm are both above 0 |

### Read at the game's start (a change needs a restart)

None.

## The look

### Read while the game runs

| setting | default | range | what |
|---|---|---|---|
| `wings_zoom` | `1` | 0.25..4 (clamped) | the sides' size: above 1 larger (less black at the corners, more cut off) |
| `wings_curve` | `0` | 0..1 (clamped) | 0 flat side monitors .. 1 curved ones |
| `wings_offset_y` | `0` |  | the sides moved up (-) or down (+), pixels |

### Read at the game's start (a change needs a restart)

None.

## Calibration and diagnostics

### Read while the game runs

| setting | default | range | what |
|---|---|---|---|
| `wings_calibrate` | `0` | above 0.5 = on | 1 = shapes to set the angle by: circles, squares and rows across the monitors' edges, round, square and straight from where you sit when wings_angle is right |

### Read at the game's start (a change needs a restart)

| setting | default | range | what |
|---|---|---|---|
| `wings_hook` | `1` | 0.5 or above = on | read when the game starts: 1 = the hook in place; 0 = nothing hooked (the calls only passed on), to tell whether another mod is disturbed by it |

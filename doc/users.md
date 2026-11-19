# edwide_screen for players

## Install

Tried only on Linux (Proton, DXVK); on Windows it should work the same way, but nobody has tried that yet.

1. Install edloader first: [edloader's guide, Install](https://github.com/arturbac/edloader/blob/main/doc/users.md#install).
   Its folder `edloader` is `C:\Users\<you>\edloader` on Windows, `.../steamapps/compatdata/359320/pfx/drive_c/users/steamuser/edloader`
   on Linux with Steam.
2. `edwide_screen.dll` into `edloader\plugins`, and a line `edwide_screen.dll` in `edloader\edloader.txt`, before
   EDHM's line if EDHM is in the list. It is a small element of the list: it passes the game's call on and only
   hooks the picture's showing.
3. Start the game once: `edloader\config\edwide_screen.ini` is made, every line a comment, and the correction is off.

## Set it

In `edwide_screen.ini` (a line without `;` sets a value; read again within a second of saving):

| setting | default | what |
|---|---|---|
| `wings` | `0` | `1` = on |
| `wings_center_px` | `0` | the middle monitor's width in pixels of the game's picture (e.g. 3000 of 9000); `0` = off |
| `wings_side_px` | `0` | each side monitor's width; set it, and a picture of another width is left alone; `0` = the rest of the picture, any width |
| `wings_bezel_px` | `0` | pixels of the picture hidden behind the bezels between the middle and a side, when the driver adds them |
| `wings_angle` | `30` | the side monitors' angle towards you, degrees (0 = in one line with the middle) |
| `wings_eye_cm` | `0` | your eyes' distance from the middle monitor, cm |
| `wings_center_cm` | `0` | the middle monitor's picture width, cm; with `wings_eye_cm` the sides are laid as seen from your eyes |
| `wings_fov` | `0` | only when the two above are not set: the game's vertical field of view (then the eyes are taken to be where that field of view puts them); `0` = read from the game's `Settings.xml` |
| `wings_zoom` | `1` | the sides' size: above 1 larger (less black at the outer corners, more cut off) |
| `wings_curve` | `0` | `0` flat side monitors .. `1` curved |
| `wings_offset_y` | `0` | the sides moved down (+) or up (-), pixels |
| `wings_calibrate` | `0` | `1` = the calibration's shapes (below); `0` = none |
| `wings_hook` | `1` | read when the game starts: `0` = nothing hooked (only to tell whether another mod is disturbed by it) |

Measure: the angle from the monitors' widths and the distance between the outer edges (three monitors of width `w`,
the outer edges `e` apart: `cos(angle) = (e - w) / (2 w)`), your eyes' distance from the middle monitor, its picture
width. Example (three 4K monitors 64 cm wide, the outer edges 160 cm apart, the eyes 80 cm away, the game's picture
9000 wide centred, so 3840 on the middle and 2580 on each side):

```
wings = 1
wings_center_px = 3840
wings_side_px = 2580
wings_angle = 41.4
wings_eye_cm = 80
wings_center_cm = 64
```

The game's field of view does not change the correction once `wings_eye_cm` is set: the sides go on from the middle
monitor as your eyes see it. For the middle itself to be true to your eyes too, the game's vertical field of view is
`2 atan(height_px / 2 / (eye_cm x px_per_cm))` - 25 degrees in the example; a wider one shows more, as it does now.

Then fine-tune the angle by eye with `wings_calibrate = 1`. On each edge between the middle monitor and a side one it
draws two yellow circles and a green square, half on each monitor, as if they stood on the middle monitor carried on;
magenta rows every eighth of the height, the white line at the eyes' level, the edges cyan. From where you sit, with
the angle right, the circles are round, the square square and the rows run on straight across the edges. Change
`wings_angle` in the ini a degree or two at a time (it is read again within a second; the log names each value):

- the side half of a circle narrower than the middle half (squeezed): the angle is too large, make it smaller;
- the side half wider (stretched): the angle is too small, make it larger;
- no angle makes both round and straight at the edge: check `wings_eye_cm`.

Then `wings_calibrate = 0`.

![The calibration's shapes on three monitors](images/calibration.jpg)

The angle measured is where to start, not always where it ends: on the setup of the example (measured 41.4) the
shapes looked right at `wings_angle = 16`.

Black at the outer corners: the game's picture does not reach that far for the angle; a larger `wings_zoom` fills it
by cutting a little off, a larger field of view in the game draws more.

## Good to know

- The game's HUD and anything a plugin draws into the picture (edworld) are corrected with it.
- The mouse is not: on the side monitors the cursor and what it points at part a little (in Elite Dangerous the
  mouse is used on the middle monitor).
- The log: `edloader\logs\edwide_screen.log` says when it is on, and why it is off ("off: the picture is 1920x1080,
  the settings expect a width of 9000").

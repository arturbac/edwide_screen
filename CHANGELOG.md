# Changelog

## Unreleased

- `wings_eye_cm` and `wings_center_cm`: the sides laid as seen from your eyes' distance; the game's field of view is
  used only without them (it put the eyes at ~41 cm for 58 degrees at 9000x2160, so the sides were turned too much
  for eyes 80 cm away).
- First version: the side parts of the picture laid over side monitors turned towards you (angle, curve, zoom,
  offset, bezels, guide lines), the game's field of view read from its Settings.xml, everything in edwide_screen.ini
  read again while the game runs; a picture of another size left alone and said in the log, on again by itself.

# edwide_screen

A plugin for [edloader](https://github.com/arturbac/edloader) that makes Elite Dangerous look right on three
monitors where the side ones are turned towards you.

The game draws one flat picture over all three monitors. Seen on side monitors set at an angle, the picture's sides
look stretched (at a field of view of about 130 degrees across, things near the outer edges are several times wider
than they should be). edwide_screen takes the finished picture just before it is shown (after the game's upscaling
and its HUD) and lays each side part over its monitor as the eye sees it through that monitor: the middle monitor's
part stays exactly as it was, the sides are squeezed back, and lines that run straight in the world run straight
across the monitor's edge.

- Not a d3d11 proxy: a plain dll in edloader's list, works with or without edworld.
- Everything in `edwide_screen.ini`, read again while the game runs; off until you set it.
- A picture of another size than the one set (the game in a window on the middle monitor, a launcher) is left
  alone, and it comes back on by itself when the size is right again.
- Cost on the graphics card: about 0.1 ms a frame at 9000x2160 (RX 7900 XTX, DXVK).

How to set it up: [doc/users.md](doc/users.md). How it works: [doc/developers.md](doc/developers.md).

License: MIT ([LICENSE](LICENSE)).

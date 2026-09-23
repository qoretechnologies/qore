# MP4 metadata fixtures

Copyright 2026 Qore Technologies, s.r.o. Licensed under MIT (see COPYING.MIT).

Both files contain original synthetic blue video, generated with FFmpeg 7.0.2
and encoded as base64 for source control. They contain 24 H.264 frames at
160 × 160 pixels, with a one-second duration and 24 fps. The audio variant
also contains an original 440 Hz sine wave encoded as AAC.

Generation commands (base64-encode the resulting MP4 files):

```sh
ffmpeg -f lavfi -i color=blue:s=160x160:r=24:d=1 -c:v libx264 -pix_fmt yuv420p -movflags +faststart blue.mp4
ffmpeg -f lavfi -i color=blue:s=160x160:r=24:d=1 -f lavfi -i sine=frequency=440:sample_rate=48000:duration=1 -c:v libx264 -pix_fmt yuv420p -c:a aac -shortest -movflags +faststart blue-audio.mp4
```

Both files were independently decoded in full with FFmpeg (`-v error -i FILE
-f null -`), and their dimensions, duration, rate, and 24 decoded frames were
checked with imageio-ffmpeg. These are local parser fixtures, not evidence of
Black Forest Labs generation. The runtime helper checks bounded container
metadata; it does not decode compressed frames or verify audio playback.

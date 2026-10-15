# Pictures, videos and sounds

A game's files lie in its directory `/games/<id>/` on the SD card. Media assets need to be created with the requirements below. 

## Pictures

A picture is at most 320 × 240 pixels. A pixel may be partly transparent. The display shows its opacity in 32 steps. A picture can lie anywhere in the game's directory.

The pictures of one layer share one palette of 255 colors. The tool therefore converts them together and writes each file beside its PNG. Each `--color` puts a color into the palette exactly, such as the color of a text.

```
python tools/picture.py --color ffffff table.png lamp.png ball.png
```

The tool reduces the set to 255 colors with their opacity and dithers each picture, so a picture with more colors keeps its look.

## Videos

A video lies in `video/` and plays at 25 frames a second, in full color and without transparency. A fullscreen video is 320 × 240 pixels. A smaller one may have any height. Its width goes in steps of 16 pixels.

A video's sound is the sound file of the same name, so `video/clip.rgb` plays with `audio/clip.wav`. This command makes both files from one MP4:

```
ffmpeg -i clip.mp4 \
  -map 0:v -vf "fps=25,scale=320:240" -pix_fmt rgb565be -f rawvideo clip.rgb \
  -map 0:a -ac 1 -ar 44100 -c:a pcm_s16le clip.wav
```

For a smaller video, `scale` takes its width and height. For a video without sound, the command ends after `clip.rgb`.

## Sounds

A sound lies in `audio/`. It is a WAV file with 16-bit samples at 44.1 kHz, in mono.

```
ffmpeg -i music.mp3 -ac 1 -ar 44100 -c:a pcm_s16le music.wav
```

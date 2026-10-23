# Pictures, videos and sounds

A game's files lie in its directory `/games/<id>/` on the SD card. Media assets need to be created with the requirements below. 

## Pictures

A picture is in full color and at most 320 × 240 pixels, the size of the screen. Any picture may be partly transparent. A picture can lie anywhere in the game's directory.

```
python tools/picture.py background.png lamp.png
```

The tool writes each picture beside its PNG in LVGL's image format. A picture without transparent pixels takes two bytes a pixel. LVGL copies it as it is. A picture with transparency takes three bytes a pixel. LVGL blends it with what lies behind it.

## Videos

A video lies in `video/` and plays at 25 frames a second, in full color and without transparency. A fullscreen video is 320 × 240 pixels. A smaller one may have any size.

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

# TaikoArcadeLoader

This is a loader for Taiko no Tatsujin Nijiiro ver.

## Setup

Copy the extracted contents of dist.zip to the same directory as Taiko.exe

### config.toml

```toml
[amauth]
# connection server
server = "127.0.0.1"

[patches]
# patch version
# auto: hash detection (you need to use the original exe)
# jp_nov_2020: 08.18
# cn_jun_2023: 00.32 CHN (32.09)
version = "auto"
# window resolution
res = { x = 1920, y = 1080 }
# window mode
windowed = false
# vertical sync
vsync = false
# wasapi shared mode
shared_audio = true
# unlock all songs
unlock_songs = true

  [patches.cn_jun_2023]
  # sync test mode language to attract etc.
  fix_language = false
  # show demo movie
  demo_movie = true
  # enable one piece collab mode
  mode_collabo025 = false
  # enable ai soshina mode
  mode_collabo026 = false

[qr]
# qr string used for login (must start with BNTTCNID and then card id)
card = ""
# qr data used for other events (ex. gaiden, custom folder)
data = { serial = "", type = 0, song_no = [] }

[drum]
# input interval (if you are using taiko drum controller, it should be set to 0)
wait_period = 4
```

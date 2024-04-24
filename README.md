# TaikoArcadeLoader

This is a loader for Taiko no Tatsujin Nijiiro ver.

## Setup

Copy the extracted contents of dist.zip to the same directory as Taiko.exe

### config.toml

```toml
[amauth]
# connection server
server = "127.0.0.1"
# connection port
port = "54430"
# dongle serial
chassis_id = "284111080000"
# shop name
shop_id = "TAIKO ARCADE LOADER"
# game version
game_ver = "00.00"
# country code
country_code = "JPN"

[patches]
# patch version
# auto: hash detection (you need to use the original exe)
# JPN00: JPN 00.18
# JPN08: JPN 08.18
# JPN39: JPN 39.06
# CHN00: CHN 00.32 (based JPN 32.09)
version = "auto"
# window resolution
res = { x = 1920, y = 1080 }
# window mode
windowed = false
# vertical sync
vsync = false
# auto change to english ime mode
auto_ime = false
# unlock all songs
unlock_songs = true

  [patches.chn00]
  # sync test mode language to attract etc.
  fix_language = false
  # show demo movie
  demo_movie = true
  # enable one piece collab mode
  mode_collabo025 = false
  # enable ai soshina mode
  mode_collabo026 = false

[audio]
# wasapi shared mode
wasapi_shared = true
# use asio
asio = false
# asio driver name
asio_driver = ""

[card_reader]
# enable card reader emulation
enabled = true

[qr]
# enable qr emulation
enabled = true
# qr image path
image_path = ""

  # qr data used for other events (ex. gaiden, custom folder)
  [qr.data]
  # qr serial
  serial = ""
  # qr type
  # 0: default (serial only)
  # 5: custom folder
  type = 0
  # song noes used for custom folder
  song_no = []

[drum]
# input interval (if you are using taiko drum controller, it should be set to 0)
wait_period = 4

[controller]
# use analog input
analog = false
```

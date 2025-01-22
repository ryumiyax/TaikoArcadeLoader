# TaikoArcadeLoader

This is a loader for Taiko no Tatsujin Nijiiro ver.  
It currently supports the following versions:

* JPN 00.18
* JPN 08.18
* JPN 39.06
* CHN 00.32 (based on JPN 32.09)

**Attention**: As far as we know, game couldn't run on graphic card equals or lower than GTX 7xx series(GTX 7xx is not supported either), so if game crashes before it shows `avaliableRefreshRate`, that most likely means your graphic card is not supported!

## Setup

Copy the extracted contents of `dist.zip` to the same directory as Taiko.exe  
If your game hangs on a black screen at launch for more than a minute, Start Taiko.exe as Administrator.

### config.toml

```toml
[amauth]
server = "127.0.0.1"
port = "54430"
chassis_id = "284111080000"
shop_id = "TAIKO ARCADE LOADER"
game_ver = "00.00"
country_code = "JPN"


[patches]
version = "auto"            # Patch version
                            # | - auto: hash detection (you need to use the original exe otherwise it will not load).
                            # | - JPN00: For use with Taiko JPN 00.18
                            # | - JPN08: For use with Taiko JPN 08.18
                            # | - JPN39: For use with Taiko JPN 39.06
                            # | - CHN00: For use with Taiko CHN 00.32
<<<<<<< HEAD
unlock_songs = true         # Self-explanatory
local_files = true          # Only set this to false if you're using this on a Nijiiro Cabinet, running on BNA1
=======
unlock_songs = true
>>>>>>> 684ee61 (Several fix & optimize)

[patches.chn00]             # These patches are only available for version CHN00
fix_language = false        # Sync test mode language to attract etc
demo_movie = true           # Show demo movie
mode_collabo025 = false     # Enable one piece collab mode
mode_collabo026 = false     # Enable ai soshina mode


[emulation]
usio = true                 # Disable this if you want to use an original Namco USIO board. you need to place bnusio_original.dll (unmodified bnusio.dll) in the executable folder.
card_reader = true          # Disable this if you want to use an original Namco card reader
accept_invalid = false      # Enable this if you want to accept cards incompatible with the original readers
qr = true                   # Disable this if you want to use an original Namco QR code scanner


[graphics]
res = { x = 1920, y = 1080 }
windowed = false            # if set to false, will automatically go Fullscreen
cursor = true               # Fullscreen will automatically hide cursor
vsync = false               # Enable this if you are using a 120fps screen (and use "Let Application Decide" in Nvidia panel)
# fpslimit = 0              # Temporarily disabled, limit to 120 fps in Nvidia panel instead!!!
model_res_rate = 1.0        # Don-Model resolution rate (currently JPN39 only)
                            # | rate <= 0: default 1280x720
                            # | rate >  0: current resolution * rate


[audio]
real = true                 # Reduce audio latency when you are using "High Definition Audio" driver
wasapi_shared = true        # Wasapi shared mode, allows you to have multiple audio sources at once at a cost of having higher latency.
asio = false                # Use asio audio mode
asio_driver = "ASIO4ALL v2" # Asio driver name
                            # | If you're not using asio4all, open up regedit then navigate to HKEY_LOCAL_MACHINE\SOFTWARE\ASIO for your driver's name.
                            # | It is case-sensitive.



[qr]
image_path = ""             # Path to the image of the QR Code you want to use


[qr.data]                   # QR data used for other events (ex. gaiden, custom folder)
serial = ""                 # QR serial
type = 0                    # QR type
                            # | 0: default (serial only)
                            # | 5: custom folder
song_no = []                # Song noes used for custom folder


[controller]
wait_period = 0             # Input interval (if using taiko drum controller, should be set to 0)
analog_input = false        # Use analog input (you need a compatible controller, this allows playing small and big notes like on arcade cabinets)
global_keyboard = false     # Accept keyboard input even if Taiko.exe is not foreground



[keyboard]
auto_ime = true             # Automatically change to english ime mode upon game startup
jp_layout = false           # Use jp layout scan code (if using jp layout keyboard, must be set to true)


[layeredfs]
enabled = false             # Replace assets from the game using a layered file system.
                            # | For example if you want to edit the wordlist, add your edited version like so:
                            # | .\Data_mods\x64\datatable\wordlist.json
                            # | You can provide both unencrypted and encrypted files.


[logging]
log_level = "INFO"          # Log level, Can be either "NONE", "ERROR", "WARN", "INFO", "DEBUG" and "HOOKS"
                            # | Keep this as low as possible (Info is usually more than enough) as more logging will slow down your game
log_to_file = false         # Log to file, set this to true to save the logs from your last session to TaikoArcadeLoader.log
                            # |Again, if you do not have a use for this (debugging mods or whatnot), turn it off.
log_path = "logs.log"       # Log file path (Can be both relative and absolute).
```

### TestMode options (JPN39 only)

TaikoArcadeLoader offers several patches to select in TestMode  

The follow options are available in "MOD MANAGER" menu:

* FIX LANGUAGE (sync test mode language to attract etc)
* UNLOCK SONGS (show all of the songs)
* FREEZE TIMER (stop timer count down)
* KIMETSU MODE (enable collabo024, will show a blank title and crash when enter)
* ONE PIECE MODE (enable collabo025)
* AI SOSHINA MODE (enable collabo026)
* AOHARU MODE (enable aprilfool001)
* INSTANT RESULT (send result per song, experimental not stable)

Enhanced original option:

* Louder volume (Speaker Volume is now up to 300%, **WARNING: May damage your speakers**)
* Attract demo (Only available if FIX LANGUAGE is ON)

With chs-patch (Only unlock while resource correctly settled):

* Add Language: zh-cn
* Add Voice Language (Could switch between JPN and CHN)

### Running on BNA1

If you're planning to use the loader on real hardware, you'll also need to extract the two dlls contained in the [BNA1.zip](./BNA1.zip) file next to the game's executable.

## Building Manually

To compile TaikoArcadeLoader, you'll need to install [MSVC](https://aka.ms/vs/17/release/vs_BuildTools.exe).

Loading this project in CLion or VSCode with the cmake tools addon should then allow you to build the project.  
Do note that the .sln files created after you run the configure command CAN be opened using Visual Studio or Rider.  
If you want to build yourself, here are some instructions on how to do this from a cmd shell.  

Clone this repository, open *cmd* and run the following commands:

```bash
# Load the MSVC environment (Change this to your actual vcvarsall.bat path)
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

# Configure the build folder (this is only needed the first time)
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_VERSION="10.0.26100.0"

# Build TaikoArcadeLoader
cmake --build build --config Release --target bnusio
```

The compiled dll of TaikoArcadeLoader will be written in the `dist` folder.

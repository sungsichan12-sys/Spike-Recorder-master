# Installable games

The game library scans `game/<folder>/game.ini` every time it opens. The folder
name can be anything; each manifest needs a unique `id`, a display `title`, a
short `description`, a `type`, and a `module`.

```ini
id=my-game
title=My Game
description=One short line shown in the game list.
type=plugin
module=MyGame.dll
```

For `type=plugin`, put the DLL named by `module` in the same folder. It must
export `BYB_GetGamePlugin` and use the C ABI in `src/games/GamePlugin.h`. The
plugin receives live signal level, trigger level, and rising-edge events, and
draws through the host callbacks. This keeps game code and the recorder's C++
widget internals separate. See `night-pulse/` for a complete example.

The Windows build automatically compiles every `game/<folder>/*.cpp` file into
a sibling DLL. Sensor controls are peak-driven: each game acts on a rising
signal edge and retains the chosen state between peaks. Peak strength maps from
the trigger threshold to three times that threshold, letting one sensor provide
a discrete action and a simple direction or lane choice. Keyboard controls
remain available as a no-sensor fallback.
Games with keyboard fallback keep it available without a sensor. Open the game
library with `G` and use `[` or `]` there to tune peak sensitivity for the
connected sensor or microphone.

The bundled library is intentionally small. `dino/` jumps on one sensor or
microphone peak; a stronger peak makes a longer jump. Space or left click is a
normal jump, and Up or right click is a long jump when testing without input.
Its obstacle speed is tuned for younger players.

`dino/` renders a smoothly shaded sculpted dinosaur, animated legs and tail, layered
sandstone canyons, sunrise haze and contact shadows. Rendering uses a single
triangle batch and does not change jump timing or collision geometry.

`night-pulse/` is a first-person 3D drum kit with a 120 BPM backing track.
A sensor peak, microphone peak, Space, or click strikes a drum as the note
arrives at its marker. Twelve misses end the set. Press `M` on the ready or
result screen to toggle sound for the next set. Microphone mode defaults to
muted music and hit sounds to avoid speaker feedback; use headphones to enable
sound. ABI v3 adds per-vertex lighting and reciprocal-depth triangle drawing.
ABI v4 preserves that layout and supports batches up to 65,536 triangles for
dense models. ABI v5 appends the optional report_result callback for one completed
round, with a per-session run ID and score; v1/v2/v3/v4 modules remain supported.
Newly built v5 modules require
the updated host. The host requests 4x multisampling with a standard
OpenGL fallback for older drivers.

The outer run_spike_recorder.bat starts the cloud score sender. Dino and Night
Pulse queue completed results to server/outbox, and the sender uploads them to
the configured Google Apps Script API. No network wait occurs on the recording
thread. See the outer CLOUD_SETUP.md.

The EMG fighting game runs inside the same Recorder window:

```ini
id=emg-fight
title=근육 대결 EMG
description=2 players / two sensor channels / A S D and J K L.
type=plugin
module=EMGFight.dll
players=2
```

`players=2` asks for each player's name before opening the native module. ABI v6
appends two independent input-channel levels, thresholds, availability flags,
session names, an optional scaled text callback, and the current input sensitivity. The v1-v5 prefix is unchanged.
The first two enabled Recorder channels feed players 1 and 2. Enable both sensor
channels in Recorder for two-person EMG control; an absent channel is never
duplicated from the other player. Contraction / stronger contraction / double
contraction perform attack / strong attack / guard. A S D and J K L are the
keyboard alternatives. Enter starts a round, P pauses, and G returns to the
library. Losing an active input channel pauses the match automatically.
The original emg_fight.html is retained as a standalone reference; the game
library loads EMGFight.dll, and no browser or separate program is launched.

Copy a game's folder into the project's top-level game folder to install it;
remove that folder to remove it from the library. When running from a source
checkout, the project folder is canonical so an older staged copy cannot bring
back a removed game. Standalone deployments scan the game folder beside the
executable. build_windows.bat synchronizes the project game folders beside
the Windows executable for portable builds.

To build a Windows plugin from the project root, use the bundled MinGW compiler:

```bat
C:\msys64\mingw32\bin\g++.exe -shared -static -O2 -std=gnu++11 -Wall -I.\src -o .\game\my-game\MyGame.dll .\game\my-game\MyGame.cpp -static-libstdc++ -static-libgcc -lwinmm
```

The manifest `id` must match the plugin descriptor's `id`. Use
`game/night-pulse/` as the working example when creating another module.

All bundled games use `type=plugin`. Signal Dino is entirely in `game/dino/`: `DinoGame.cpp` builds `DinoGame.dll`; its rules, controls and models are sibling headers. Removing that folder removes Dino from the G library without changing or rebuilding the host.

## Set the input threshold before play

After entering player names, every game opens the native input setup screen:
relax and measure for 3 seconds, contract comfortably (or make the intended
sound for microphone control) and measure for 3 seconds, then release and confirm.
The graph shows the measured resting-noise band and the yellow action threshold.
Up / Down or + / - adjusts the threshold. Tab (or the channel label) changes the
input channel before measurement. Two-player games calibrate both channels
separately. K explicitly starts keyboard-only play with all sensor input disabled.

The game does not advance during setup. Weak or interrupted measurements must
be repeated. The host removes the measured noise floor, rejects isolated input
spikes, latches held contractions, and keeps the selected channel fixed for the
round. An input/filter/source change pauses the game for fresh setup; it does
not silently reuse another channel's threshold. The keyboard or mouse used to
confirm setup cannot trigger a game action on the same press.

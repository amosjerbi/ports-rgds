# Four Swords over wifi (two handhelds)

Plays *The Legend of Zelda: A Link to the Past & Four Swords* (GBA, USA) between two ROCKNIX handhelds on the same wifi,
the way two Game Boy Advances would play over a link cable. Each handheld runs its own copy of the game in RetroArch's
mGBA core; a small bridge passes the link packets between them.

## How it works
* `fourswords_netplay.ips` (369 bytes, my own bytes only, no ROM data) replaces the link library's poll and send steps.
  The game's own link manager then sees two real players: one is this console, the other arrives from the network.
  It also raises the game's input delay from 2 to 5 frames so wifi latency does not stall the game (about 83 ms of
  input lag, full speed up to ~4 frames of one-way latency).
* `fs-link.py` runs next to RetroArch on each handheld. It reads this console's outgoing packet and writes the other
  console's packets into the game through RetroArch's local UDP command port (`network_cmd_enable`).
  It only touches game memory when it sees the patched ROM's marker.
* `gba-launch` starts the bridge for the length of a "Four Swords Netplay" session and stops it afterwards.

## Setup (both handhelds)
1. Apply `fourswords_netplay.ips` to your own ROM: `python3 apply_ips.py "<rom>.zip" fourswords_netplay.ips "Legend of Zelda, The - Four Swords Netplay (USA).gba"` and put it in `/storage/roms/gba/` (copy the `.srm` save too if you want your progress).
2. Copy `fs-link.py` and `gba-launch` to `/storage/roms/ports/fs-link/` and write the other handheld's IP into `/storage/roms/ports/fs-link/peer`.
3. Point the GBA launch command in `es_systems.cfg` at `/storage/roms/ports/fs-link/gba-launch` (or merge the `Four Swords Netplay` branch into an existing wrapper).
4. Start the game on both. The handheld with the lower IP address is player 1 (green, "host").

## Playing
Pick *Four Swords* on both, press A through the story, and wait for "Linking with other systems..." on both. The host
presses A on the link screen and everything proceeds as on a real cable. Each player controls their own Link.

`/tmp/fs-link.log` shows the connection status, round-trip time and packet counts.

## Rebuilding
`python3 build_net.py 5 out.gba` builds the patched ROM from `fs.gba` (your unpatched ROM, next to the script); the argument is the input delay in frames (2..7).

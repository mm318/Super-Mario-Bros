# Super Mario Bros. — Battle Game

A simultaneous local two-player arena platformer using this repository's Mario,
Luigi, enemy, tile, mushroom, coin and star sprites, Press Start 2P font, music
and sound effects. Platforms use the underground brick with an illuminated top;
Boo hazards use castle-palette Lakitu sprites. Pipes use four overworld tiles.
POW, the arena background, HUD, menus and particles are drawn in code.

Race to collect **five shared coins** or eliminate the opponent to win a round.
The first player to win five rounds wins the match, which lasts five to nine
rounds. Stars in the HUD represent round wins in the current match and reset
when a new match starts. There are no saved victory totals or campaign stages.

Every round starts empty. After the ready countdown, enemies enter through
upper pipes every 2.8 seconds. Shared coins enter independently every 15
seconds, up to five uncollected coins in the arena. Defeating an enemy never
releases or awards a coin. Lakitu/Boo hazards begin entering after 14 seconds.

## Play

Choose **Start Battle** on the title screen. Two people play simultaneously on
one keyboard, or connect two SDL-compatible gamepads (first connected is Mario).

| Action | Mario | Luigi | Gamepad |
| --- | --- | --- | --- |
| Move | F / H | Left / Right | D-pad / left stick |
| Jump (hold for height) | S | . (period) | A / B |
| Dash (hold) | A | , (comma) | X / Y |
| Crouch when Super | G | Down | D-pad / stick down |

**Escape** is the only way to open the pause menu. Choose Resume Battle,
Restart Match, How to Battle, or Title Screen using Up/Down and Enter/Space,
or hover and click with the mouse. Gamepad D-pad and A also select options in
an already open menu. How to Battle returns to its parent menu using Back,
Enter, or Escape. The title screen also has a How to Battle option.

Enter/Space continues after a round or starts a new match after a victory.
**M** mutes and **F11** toggles fullscreen. Losing focus suspends play without
opening the pause menu; play resumes when focus returns.

The HUD is 28 logical pixels tall. Mario's name, five coin slots and five
round-win star slots align to the left; Luigi's align to the right, with the
round centered. Empty slots show silhouettes of the original coin/star sprites;
collected coins and round victories fill those slots, without numeric counts.
Platforms span more of the vertical space. The HUD does not stop jumps.

Jump into a platform beneath a grounded enemy to stun it. Jump onto platforms
and touch stunned enemies to kick them away. Platforms are 32 logical pixels
thick and solid on every side. Kicking enemies helps control the arena; coins
arrive separately and either player can collect them.

- Spinies overturn after one platform bump, then can be defeated by contact.
- Buzzy Beetles use their underground sprites. Stomping an active beetle is
  harmless; bump its platform to expose its upright shell. Side contact kicks
  it out, while stomping its shell restores the active beetle.
- Stomp a flying Koopa to remove its wings, then stomp again to retract it.
  The first platform bump and POW also retract Koopas. Stomping a shell launches it; side contact
  launches a stopped shell. A brief grace period protects only the kicker.
- Moving shells toss other enemies without killing them. Koopa shells recover
  only after nine uninterrupted seconds at rest. They re-emerge after one
  lower-pipe entry, then disappear into the pipe on their next entry. Re-kicking
  and POW do not reset that limit. Neither type of Koopa releases a coin.
- Bumping a stunned enemy or shell from below revives it angry. This includes
  Spiny, Buzzy Beetle, and both Koopa types; flying Koopas never regain their
  wings. Recovery timers and beetle-shell stomps also revive angry enemies.
  There are only normal and angry states: angry movement and kicked Koopa
  shells are exactly 50% faster, with no stacking. All angry enemies and shells use a red tint.
  Stunned foes and stopped Koopa shells have orbiting stars.
- Bump a platform beneath a grounded coin or any mushroom to bounce it upward.
  Its type, travel direction and collectible state remain unchanged.
- Stomping a rival stuns them for 0.5 seconds. Bumping the block beneath them
  stuns them for one second and pops them upward. Large stunned players crouch;
  Small stunned players use the game-over pose. Orbiting stars mark the stun.
  Stuns lock movement, dash and jump, while gravity continues normally.
- Both players start Super. Damage shrinks them; another hit while Small
  eliminates them and awards the opponent the round, regardless of coin counts.
- The translucent POW overturns Spinies and beetles, retracts Koopas, stops shells, clears Boos,
  and stuns the opponent for one second regardless of invincibility. It has
  three charges and returns after a random delay once exhausted.
- Mushroom appearance determines its effect, regardless of current sizes:

| Mushroom palette | Effect |
| --- | --- |
| General | Grow from Small to Super; no effect when already Super |
| Overworld | Exchange Mario's and Luigi's sizes |
| Underground | Exchange their positions and movement states, keeping their sizes |

Mushrooms emerge from pipes or hidden platform segments. Coins and enemies
recycle through the lower pipes; uncollected coins never expire.

| Round | Regular enemies |
| --- | --- |
| 1 | Spiny |
| 2 | Spiny, Buzzy Beetle |
| 3 | Buzzy Beetle |
| 4 | Spiny, Flying Koopa |
| 5 | Flying Koopa |
| 6 | Flying Koopa |
| 7 | Spiny, Buzzy Beetle, Flying Koopa |
| 8 | Buzzy Beetle, Flying Koopa |
| 9 | Spiny, Buzzy Beetle, Flying Koopa |

Each enemy spawn in every round has a 7% chance of being a walking green Koopa;
the other 93% uses the roster above. Walkers recur and share the five-enemy cap.
Lakitu/Boo hazards recur separately in every round.

## Build and run

Use **Zig 0.17.1**. SDL and its dependencies are built from source; pinned versions
and checksums are in [deps/README.md](deps/README.md). On Ubuntu:

```sh
sudo apt install libxext-dev libgles-dev libglx-dev libpulse-dev
zig build
zig build -Doptimize=safe run
```

To test the game with both players protected from elimination:

```sh
zig build -Doptimize=safe run -- --test
# Or launch the installed executable directly:
./zig-out/bin/super-mario-bros --test
```

`--test` allows enemies, shells and hazards to shrink Super Mario and Luigi as
usual. Once Small, they take no damage or damage knockback and cannot be
eliminated; a mushroom restores Super size and lets them shrink again. The mode
stays enabled across rounds, matches and restarts for that session. The footer
shows `TEST: NO ELIMINATION`. Player stomps, platform bumps and POW still stun
players, and coin victories still end rounds. Omit the flag to play normally.

The executable is `zig-out/bin/super-mario-bros` (`.exe` on Windows), retaining
the existing artifact name. The default window is 1600 × 960, with an 800 × 480
logical arena, so the larger window keeps the same physics and layout.
Its assets are installed in `zig-out/bin/res`;
keep them together. Assets are located relative to the executable, so it works
from another working directory. `debug`, `safe`, `fast`, and `small` are supported.

For the existing Emscripten browser target:

```sh
zig build install-emsdk                 # Pinned Emscripten 7.0.0, once
zig build -Dtarget=wasm32-emscripten -Doptimize=fast
zig build -Dtarget=wasm32-emscripten -Doptimize=fast run
# To serve without opening a browser, append: -- --no_browser
```

Serve all of `zig-out/web`, including `index.html`, `.js`, `.wasm`, `.data`, and
`coi-serviceworker.js`. Pthreads require cross-origin isolation; the service
worker supplies it on localhost or HTTPS. The loop uses requestAnimationFrame
without Asyncify; keyboard controls work when the game has focus.

## Rules and verification

```sh
zig build test
```

The SDL-independent simulation is in `include/battle/BattleGame.h` and
`src/battle/BattleGame.cpp`. `BattleScene` handles input, rendering, audio and menus.
`Core` owns the scene and application loop; the former
campaign scenes, ECS systems, commands and map code have been removed. Builds
package the fonts, sounds and sprites used by Battle Game. `res/` contains only
the active font, three sprite sheets, window icon, twelve sound effects, battle
music, and Windows icon/resource script. Campaign level data, screenshots,
menu artwork, tile mappings and unused audio have been removed.

Tests cover five-coin wins, five-to-nine-round matches and fresh-match resets, all nine
rosters, 7% walking-Koopa spawn frequency in every round, recurring walkers and the
shared enemy cap, empty round starts and delayed pipe entrances, enemy state
transitions, solid platform head/side collisions, jumping onto platforms in both
player sizes, unrestricted jumps above the HUD, item bounces, non-stacking 50% enemy anger,
jump/bump/kick behavior, 15-second coin cadence, independent contested coin spawns, no coins from defeated
enemies, damage, all three mushroom effects, POW exhaustion/regeneration, spawning,
Buzzy Beetle immunity/revival/finishing kicks, Koopa wing removal, POW effects,
side/stomp shell launches, per-player kick grace, one-recycle shell retirement,
recovery after continuous rest, player stomp/bump/POW stun durations, Super stun
crouching, test-mode shrinking and Small-player damage immunity for both players,
variable jumps, wrapping, simultaneous input and 162,000
stress simulation steps. CI builds native and web configurations and runs rules
tests. The rules tests can also run with AddressSanitizer/UBSan via a C++17 compiler.

This is a rules-based recreation using the supplied specification, with NES art,
not a frame-accurate SNES emulation. Unverified movement constants, enemy caps,
spawn/recovery/invulnerability timings and item/POW distributions are adjustable
in `battle::Rules`. Coin trajectories use platform gravity and continuous pipe
recycling. Player contact pushes horizontally without direct damage. Simultaneous
collisions resolve coins before damage; Mario takes an exact same-step tie for one
coin. Damage checks Mario first, so an exact simultaneous fatal collision awards
Luigi the round. Round state freezes immediately after the first award.

Original project: an MIT-licensed C++/SDL Super Mario Bros. recreation. See
[LICENSE](LICENSE) for the repository license.

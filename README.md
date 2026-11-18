# Super Mario Bros. — Battle Game

A simultaneous local two-player arena platformer, rebuilt from this repository's
C++/SDL game using its Mario/Luigi, Spiny, Koopa, tile, mushroom and coin sprites,
Press Start 2P font, music and sound effects. Platforms use the underground
thick-brick tile. Boo hazards use castle-palette Lakitu sprites. Pipes are built
from the four overworld pipe tiles; mushrooms use the overworld mushroom tile.
Flying Koopas use their winged, walking and shell sprites; Buzzy Beetles use
the underground-palette walking and shell sprites. POW, the arena
background, HUD, menus, status indicators and particles are drawn in code.

Defeat enemies to create **shared coins**, race to collect five, or eliminate the
other player. Win five rounds to earn a match star. There is one fixed, wrapping
arena, continuous enemy replenishment, and no campaign or bonus stages. Every
round starts empty: after the ready countdown, regular enemies enter through
upper pipes every 2.8 seconds, the arena Koopa arrives after 5.6 seconds, and
Lakitu/Boo hazards begin entering from the left upper pipe after 14 seconds.

## Play

Press **Enter** on the title screen. Two people play simultaneously on one keyboard,
or connect two SDL-compatible gamepads (first connected is Mario).

| Action | Mario | Luigi | Gamepad |
| --- | --- | --- | --- |
| Move | F / H | Left / Right | D-pad / left stick |
| Jump (hold for height) | S | . (period) | A / B |
| Dash (hold) | A | , (comma) | X / Y |
| Crouch when Super | G | Down | D-pad / stick down |

**Enter / Space** starts the game, continues after a round, and starts another
match after a victory. **Escape** pauses; **Tab** opens the rules and pauses
play; **M** mutes; **F11** toggles fullscreen. Gamepad Start pauses/continues and
Back opens the rules. In pause, **R** restarts the match and **Q** returns to the
title. Losing window focus pauses automatically. Stars survive restart/title.

Jump into the underside of a platform beneath a grounded enemy to bump its
segment; climb around its edge and touch the stunned enemy to kick it. A coin
then enters from an upper pipe. **Kicks don't award coins automatically.**
Either player can collect any coin. Platforms are 32 logical pixels thick and solid on every side. A head
strike bumps a segment and stops the jump. Use the gaps and edges to climb.

- Spinies are overturned by one platform bump, then defeated by contact.
- Permanently kicked-out enemies keep their sprite and fall through the arena
  and floor, drifting slightly in the kick direction. Their falling bodies have
  no collisions or rewards; the shared coin appears immediately as before.
- Underground Buzzy Beetles occupy the original Crab roster slots. Stomping an
  active beetle bounces the player without hurting either. Bump its platform
  from below to expose its shell: a side kick defeats it and generates a shared
  coin, while stomping its shell restores the active beetle.
- Stomp a flying Koopa to remove its wings; stomp again to retract it into a shell.
  Platform bumps and POW also retract Koopas. Stomping any Koopa shell launches
  it, as does side contact with a stopped shell. A short grace period protects
  only the player who kicked it; the rival can be hit immediately.
- Recovered enemies move faster. Bumping a stunned enemy turns it upright early
  and increases its speed. Spinies also change color.
- Stomping a rival forces them to crouch and stuns them for 0.5 seconds. Hitting
  the block beneath a rival stuns them for 1 second and bumps them upward.
  Movement, dash and jump input are locked while stunned; gravity still applies.
  These effects work on both player sizes, during damage invulnerability, and in
  test mode. A `STUN` label marks the affected player.
- Both players start every round Super. Damage makes them Small, with brief
  invulnerability; another hit eliminates them and immediately awards the round.
- The arena's original green Koopa follows the same shell rules but never awards
  coins. Moving shells toss other regular enemies without killing them. All
  Koopa shells recover only after remaining completely stopped on a platform or
  floor for nine seconds; movement restarts the timer.
- Moving Koopa shells re-emerge after their first lower-pipe entry, then disappear
  into a lower pipe on their next entry. Re-kicking or POW does not reset this
  limit. A regular flying Koopa generates a shared coin when it disappears;
  the arena Koopa stays gone until the next round and generates no coin.
- The central POW overturns Spinies and beetles, retracts all Koopas (including
  the arena Koopa and airborne ones), stops moving shells, and clears Boos.
  It has three charges and returns after a random delay once exhausted.
- Mushrooms restore Super size. Swap mushrooms exchange player positions when
  sizes match, or exchange their sizes when they differ. Both use the unmodified
  overworld mushroom sprite, without a ? overlay.
- Coins and enemies recycle through the lower pipes. Unclaimed coins don't expire.
- First to five round wins ends the match, after five to nine rounds. A player's
  sixth star resets their displayed stars to zero.

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

Every round also has the single arena Koopa and recurring Lakitu/Boo hazards.

Stars are stored in SDL's application preferences directory under
`SuperMarioBros/BattleGame/stars.txt` on desktop and in this browser origin's
`localStorage` on web. A new match resets rounds and coins while keeping stars.

## Build and run

Use **Zig 0.17.1**. SDL and its dependencies are built from source; pinned versions
and checksums are in [deps/README.md](deps/README.md). On Ubuntu:

```sh
sudo apt install libxext-dev libgles-dev libglx-dev libpulse-dev
zig build
zig build -Doptimize=safe run
```

To test the game with both players invincible:

```sh
zig build -Doptimize=safe run -- --test
# Or launch the installed executable directly:
./zig-out/bin/super-mario-bros --test
```

`--test` makes Mario and Luigi immune to enemy, shell and hazard damage in either
size. It stays enabled across rounds, matches and restarts for that session.
The footer shows `TEST: BOTH INVINCIBLE`. Player stomps and platform bumps still
stun players, and coin victories still end rounds. Omit the flag to play normally.

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
`src/battle/BattleGame.cpp`. `BattleScene` handles input, rendering, audio, menus,
and star persistence. `Core` owns the scene and application loop; the former
campaign scenes, ECS systems, commands and map code have been removed. Builds
package the fonts, sounds and sprites used by Battle Game. `res/` contains only
the active font, three sprite sheets, window icon, twelve sound effects, battle
music, and Windows icon/resource script. Campaign level data, screenshots,
menu artwork, tile mappings and unused audio have been removed.

Tests cover five-coin wins, five-to-nine-round matches, star rollover, all nine
rosters, empty round starts and delayed pipe entrances, enemy state transitions, solid platform head/side collisions and edge
climbing in both player sizes, jump/bump/kick behavior, contested
coins, damage, both ? Mushroom cases, POW exhaustion/regeneration, spawning,
Buzzy Beetle immunity/revival/finishing kicks, Koopa wing removal, POW effects,
side/stomp shell launches, per-player kick grace, one-recycle shell retirement,
recovery after continuous rest, player stomp/bump stun durations, forced crouching,
test-mode immunity for both players, variable jumps, wrapping, simultaneous input and 162,000
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

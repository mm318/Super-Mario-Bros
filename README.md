# Super Mario Bros. — Battle Game

A simultaneous local two-player arena platformer, rebuilt from this repository's
C++/SDL game using its Mario/Luigi, Spiny, Koopa, tile, mushroom and coin sprites,
Press Start 2P font, music and sound effects. Crab, Fighter Fly, Boo and POW art is
rendered as small pixel shapes to accompany the existing NES assets.

Defeat enemies to create **shared coins**, race to collect five, or eliminate the
other player. Win five rounds to earn a match star. There is one fixed, wrapping
arena, continuous enemy replenishment, and no campaign or bonus stages.

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

Jump through a platform beneath a grounded enemy to bump its segment; land on
that platform and touch the stunned enemy to kick it. A coin then enters from an
upper pipe. **Kicks don't award coins automatically.** Either player can collect
any coin. Platforms are passable upward and solid downward; a head crossing
triggers the bump without cancelling the jump.

- Spiny needs one bump; Crab needs two; Fighter Fly must be touching a platform.
- Stunned enemies recover faster. Bumping a stunned enemy turns it upright early,
  changes its color and increases its speed.
- Both players start every round Super. Damage makes them Small, with brief
  invulnerability; another hit eliminates them and immediately awards the round.
- Stomp Koopa and kick its shell at your opponent. It recovers, never awards coins,
  and its moving shell tosses regular enemies without killing them.
- The central POW overturns regular enemies anywhere, clears Boos and has three
  charges. It returns after a random delay once exhausted.
- Mushrooms restore Super size. A red ? Mushroom swaps player positions when
  sizes match, or swaps their sizes when they differ.
- Coins and enemies recycle through the lower pipes. Unclaimed coins don't expire.
- Round rosters follow the supplied nine-round table. First to five round wins
  ends the match, after five to nine rounds. A player's sixth star resets their
  displayed stars to zero.

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

The executable is `zig-out/bin/super-mario-bros` (`.exe` on Windows), retaining
the existing artifact name. Its assets are installed in `zig-out/bin/res`;
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
rosters, enemy state transitions, actual jump/bump/kick reachability, contested
coins, damage, both ? Mushroom cases, POW exhaustion/regeneration, spawning,
Koopa stomping/shells, variable jumps, wrapping, simultaneous input and 162,000
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

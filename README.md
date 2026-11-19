# Super Mario Bros. — Battle Game

A local two-player arena game starring Mario and Luigi. Collect **five coins**
or eliminate your rival to win a round. Win **five rounds** to take the match.

![Mario and Luigi battling in the arena](doc/screenshot.png)

## Build and play

Install [Zig 0.17.1](https://ziglang.org/download/), then run this command from
the game directory:

```sh
zig build -Doptimize=fast run
```

On Ubuntu, install these packages first:

```sh
sudo apt install libxext-dev libgles-dev libglx-dev libpulse-dev
```

Choose **Start Battle** on the title screen. Two people can share a keyboard
or use two gamepads; the first connected gamepad controls Mario.

## Controls

| Action | Mario | Luigi | Gamepad |
| --- | --- | --- | --- |
| Move | F / H | Left / Right | D-pad / left stick |
| Jump (hold for height) | S | . (period) | A / B |
| Dash (hold) | A | , (comma) | X / Y |
| Crouch when Super | G | Down | D-pad / stick down |

Press **Escape** to pause, **M** to mute, and **F11** for fullscreen. Use the
arrow keys and Enter/Space, a gamepad, or the mouse to choose menu options.
Enter/Space continues after a round. Play pauses automatically when the window
loses focus.

## How to win

- Bump platforms from below to stun enemies, then touch them to kick them away.
- Collect coins as they arrive; defeating enemies does not award coins.
- Both players start Super. One hit shrinks you; another while Small loses the round.
- Use mushrooms and the POW block to turn the tables on your rival.

Choose **How to Battle** from the title or pause menu for more help.
To practice without either player being eliminated:

```sh
zig build -Doptimize=fast run -- --test
```

## Play in a browser

Install the pinned Emscripten 7.0.0 SDK once, then build and launch:

```sh
zig build install-emsdk
zig build -Dtarget=wasm32-emscripten -Doptimize=fast run
```

Click the game to give it keyboard focus. To serve it without opening a browser,
append `-- --no_browser` to the run command.

Native builds are in `zig-out/bin`; keep the executable and its `res` folder
together when copying the game. Browser builds are in `zig-out/web`.

[MIT license](LICENSE).

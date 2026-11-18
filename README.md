# Super Mario Bros

A recreation of the Super Mario Bros game from the Nintendo NES using C++ and the SDL2 Library

Demo Video: https://www.youtube.com/watch?v=2BYdCmH1EEY

Play it online! https://gold872.github.io/games/super_mario_bros/

* *NOTE: The web build is lower quality and may contain some glitches*

## Compiling

Use **Zig 0.17.1**. The game and its SDL dependencies are built from source.
The dependency versions and source checksums are documented in
[deps/README.md](deps/README.md).

On Ubuntu, install the native system dependencies:

```sh
sudo apt install libxext-dev libgles-dev libglx-dev libpulse-dev
```

### Native desktop

```sh
zig build                   # Debug
zig build -Doptimize=safe   # Release with runtime safety checks
zig build -Doptimize=safe run
```

The executable is `zig-out/bin/super-mario-bros` (`super-mario-bros.exe` on Windows).
Its runtime assets are installed alongside it in `zig-out/bin/res`.
Keep that directory with the executable when distributing the native build.
The executable locates assets relative to its own directory.

The Windows icon is compiled from `res/super_mario_bros.rc` and `res/icon.ico`.

### Web browser

Install the pinned **Emscripten 7.0.0** SDK into `zig-pkg`:

```sh
zig build install-emsdk
```

Build or run the browser version:

```sh
zig build -Dtarget=wasm32-emscripten                        # Debug
zig build -Dtarget=wasm32-emscripten -Doptimize=fast        # Release
zig build -Dtarget=wasm32-emscripten -Doptimize=fast run
# Serve without opening a browser:
zig build -Dtarget=wasm32-emscripten -Doptimize=fast run -- --no_browser
```

The web build is installed to `zig-out/web/index.html`. Serve the entire
`zig-out/web` directory, including `index.js`, `index.wasm`, `index.data`, and
`coi-serviceworker.js`. The data file contains the game's levels, sprites,
fonts, and sounds. The service worker supplies cross-origin isolation for
pthreads on localhost or HTTPS.

The web build uses `requestAnimationFrame` without Asyncify. Frame pacing,
options key selection, and level loading return control to the browser without
blocking waits.

`debug`, `safe`, `fast`, and `small` are the optimization modes. Build outputs
and downloaded SDK files are ignored by Git.

## Controls

| Action | Default key |
| --- | --- |
| Left | F |
| Right | H |
| Jump | S |
| Duck | G |
| Sprint | A |
| Fireball | D |

Use the arrow keys and Enter in menus, and Escape to pause or go back.
Gameplay keys can be changed in the options menu.

## How it Works

### The Entities

- The Entities in this game are creating using an Entity Component System
    - Instead of having one Entity class that every Entity inherits, an Entity is a class that has nothing, but you are able to attatch components to it. Such components include:
        - Position Component
        - Texture Component
        - Moving Component

    - These Components get used by the Systems, where they find Entities with certain components, and update the Entities properly using the data from the Components.

- Entities don't only serve the purpose of displaying a texture on the screen, some are used to add properties to Entities (For example, classifying an entity as the player with the PlayerComponent), and some are also used to schedule an action (this is most commonly used in the sound and score system)

### The Commands

- Commands are used to perform actions that aren't focused on one entity. Commands are most commonly used in sequences, where multi-step processes are executed in order. Examples of this being used is the WarpCommand for warp-pipes and the VineCommand for climbing a vine.

### The Levels

- The Levels were created with the help of a program called [Tiled Map Editor](https://www.mapeditor.org/)

- After creating the Map in the Tiled Editor, they are exported as a CSV file, and then get read by the Map class, and using the IDs from the Map, the Entities get created with their needed components.

## Special Thanks
People that have been a huge help in developing this project with their amazing knowledge and skills
 - [Killme](https://github.com/killme)
 - [friedkeenan](https://github.com/friedkeenan/)

## Screenshots

### Main Menu
![Main Menu](res/screenshots/menu.png)

### Game
![Game](res/screenshots/game.png)

### Pause Screen
![Pause Screen](res/screenshots/pause.png)

### Options
![Options](res/screenshots/options.png)

### Underwater
![Underwater Level](res/screenshots/underwater.png)

### Gameover
![Game Over](res/screenshots/gameover.png)

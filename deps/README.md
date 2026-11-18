# Build dependencies

The Zig build uses these vendored libraries for Linux, Windows desktop, macOS,
and Emscripten:

| Library | Version | Build configuration |
| --- | --- | --- |
| [SDL](sdl/README.md) | 2.32.10 | Trimmed desktop and Emscripten source package |
| [SDL_image](sdl_image/VENDORED.md) | 2.8.12 | PNG loading through its bundled stb_image backend |
| [SDL_mixer](sdl_mixer/VENDORED.md) | 2.8.0 | WAV sound effects and music |
| [SDL_ttf](sdl_ttf/VENDORED.md) | 2.24.0 | FreeType font rendering without HarfBuzz |
| [FreeType](freetype/VENDORED.md) | 2.15.0 | Standard modules and bundled zlib |

Each library has its own Zig build package. Runtime libraries are linked
statically; Linux links the X11, OpenGL, and PulseAudio system libraries used by
SDL. Upstream license files are retained in each package.

The SDL Linux source list includes pthread condition variables for threaded
resource loading. FreeType's web build enables Emscripten's setjmp/longjmp
lowering to match the Emscripten linker configuration.

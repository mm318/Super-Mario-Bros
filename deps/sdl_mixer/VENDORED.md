# SDL_mixer 2.8.0

Source package: the vendored SDL_mixer in Bombermaaan.
Upstream: https://github.com/libsdl-org/SDL_mixer/releases/tag/release-2.8.0

Built with WAV music support for this game's WAV assets. No external codecs are linked.

Local patch: WAV metadata lengths are copied into an aligned `Uint32` before
endian conversion. Metadata fields can start at unaligned byte offsets.

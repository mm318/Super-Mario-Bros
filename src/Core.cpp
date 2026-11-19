#include "Core.h"

#include <iostream>

#include "SoundManager.h"
#include "TextureManager.h"

int Core::init() {
  if (TextureManager::Get().Init() != 0) {
    std::cerr << "Error Initializing Texture Manager" << std::endl;
    return -1;
  }

  if (SoundManager::Get().Init() != 0) {
    std::cerr << "Error Initializing Sound Manager" << std::endl;
    return -1;
  }

  scene = std::make_unique<BattleScene>(testMode);
  return 0;
}

void Core::run() {
  while (running) {
    mainLoop();
  }

  scene.reset();
  SoundManager::Get().Quit();
  TextureManager::Get().Quit();
}

void Core::mainLoop() {
#ifndef __EMSCRIPTEN__
  const Uint64 startTicks = SDL_GetTicks64();
#endif

  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      running = false;
    } else {
      scene->handleEvent(event);
    }
  }

  scene->update();

#ifndef __EMSCRIPTEN__
  constexpr Uint64 frameMillis = 1000 / 60;
  const Uint64     elapsed     = SDL_GetTicks64() - startTicks;
  if (elapsed < frameMillis) {
    SDL_Delay(frameMillis - elapsed);
  }
#endif
}

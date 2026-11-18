#include "Game.h"
#include "Core.h"
Game::Game() = default;
Game::Game(Core* core) : core(core) {}
void Game::init() { scene = std::make_unique<BattleScene>(); }
void Game::handleInput() {
   SDL_Event event;
   while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) core->setRunning(false);
      else scene->handleEvent(event);
   }
}
void Game::update() { scene->update(); }
void Game::shutdown() { scene.reset(); }
void Game::setCore(Core* value) { core = value; }

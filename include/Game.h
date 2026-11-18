#pragma once

#include "battle/BattleScene.h"

#include <SDL2/SDL.h>

#include <memory>

class Core;

class Game {
  public:
   Game();
   Game(Core* core);

   void init();

   void handleInput();

   void update();

   void shutdown();

   void setCore(Core* core);

  private:
   Core* core = nullptr;

   std::unique_ptr<BattleScene> scene;
};

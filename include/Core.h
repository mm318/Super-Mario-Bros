#pragma once

#include "battle/BattleScene.h"
#include <memory>

class Core {
public:
   int init();
   void run();
   void mainLoop();

private:
   std::unique_ptr<BattleScene> scene;
   bool running = true;
};

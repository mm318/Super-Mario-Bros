#pragma once

#include "battle/BattleScene.h"
#include <memory>

class Core {
public:
   explicit Core(bool testMode = false) : testMode(testMode) {}
   int init();
   void run();
   void mainLoop();

private:
   std::unique_ptr<BattleScene> scene;
   bool running = true;
   bool testMode;
};

#pragma once

#include <memory>

#include "battle/BattleScene.h"

class Core {
public:
  explicit Core(bool testMode = false) : testMode(testMode) {
  }

  int  init();
  void run();
  void mainLoop();

private:
  std::unique_ptr<BattleScene> scene;
  bool                         running = true;
  bool                         testMode;
};

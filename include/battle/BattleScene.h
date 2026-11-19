#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <memory>
#include <string>
#include <unordered_map>

#include "battle/BattleGame.h"

class BattleScene {
public:
  explicit BattleScene(bool testMode = false);
  ~BattleScene();

  void handleEvent(const SDL_Event & event);
  void update();

private:
  battle::BattleGame game;

  SDL_Renderer * renderer;

  std::shared_ptr<SDL_Texture>                       playerTexture, enemyTexture, angryEnemyTexture, blockTexture;
  std::array<std::shared_ptr<SDL_Texture>, 2>        hudSilhouettes;
  std::array<SDL_GameController *, 2>                controllers{};
  std::unordered_map<int, std::shared_ptr<TTF_Font>> fonts;

  struct Label {
    std::shared_ptr<SDL_Texture> texture;
    int                          w, h;
  };

  std::unordered_map<std::string, Label> labels;

  struct Particle {
    float     x, y, vx, vy, life;
    SDL_Color color;
  };

  std::vector<Particle> particles;

  bool        title = true, paused = false, help = false, muted = false, focused = true, fullscreen = false;
  double      previous = 0, accumulator = 0;
  float       animation = 0, resultDelay = 0, noticeTimer = 0;
  int         titleChoice = 0, pauseChoice = 0;
  std::string notice;

  void openController(int index);
  void createHudSilhouettes();
  void createAngryEnemyTexture();

  void setPaused(bool value);
  void startBattle();
  void selectMenuOption();

  void                         processEvents();
  std::array<battle::Input, 2> readInput() const;

  void render();

  void rect(int x, int y, int w, int h, SDL_Color color);
  void text(const std::string & value, int x, int y, int size, SDL_Color color, bool centered = false);
  void sprite(SDL_Texture * texture,
              SDL_Rect      source,
              SDL_Rect      dest,
              bool          flip           = false,
              bool          upsideDown     = false,
              bool          wrapHorizontal = true);

  void drawPlayer(const battle::Player & player, int index);
  void drawStunStars(float x, float headY, bool front);
  void drawEnemy(const battle::Enemy & enemy);
  void drawBoo(const battle::Boo & boo);
  void drawItem(const battle::Item & item);
  void drawPipe(int x, int y, bool right);

  void drawHud();
  void drawPanel(const std::string & heading, SDL_Color accent);
  void drawHelp();
  void drawMenuOption(const std::string & label, int y, bool selected);
};

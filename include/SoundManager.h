#pragma once

#include <SDL2/SDL_mixer.h>
#include <memory>
#include <unordered_map>

enum class SoundID {
  BLOCK_HIT,
  BOWSER_FALL,
  COIN,
  DEATH,
  JUMP,
  KICK,
  ONE_UP,
  PIPE,
  POWER_UP_COLLECT,
  SHRINK,
  STOMP,
  CASTLE_CLEAR
};

class SoundManager {
public:
  static SoundManager & Get() {
    return instance;
  }

  int Init();
  int Quit();

  void playSound(SoundID sound);
  void playMusic();

  void pauseMusic();
  void resumeMusic();
  void stopMusic();

private:
  SoundManager()                     = default;
  SoundManager(const SoundManager &) = delete;

  static SoundManager instance;

  void                       loadSounds();
  std::shared_ptr<Mix_Chunk> loadSound(const char * path);

  std::unordered_map<SoundID, std::shared_ptr<Mix_Chunk>> sounds;
  std::shared_ptr<Mix_Music>                              music;
};

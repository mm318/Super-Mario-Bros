#include "SoundManager.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include <iostream>

SoundManager SoundManager::instance;

int SoundManager::Init() {
   if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 4, 1024) != 0) {
      SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Failed to Open Audio: %s", SDL_GetError());
      std::cerr << "Failed to Open Audio: " << SDL_GetError() << std::endl;
      return -1;
   }

   loadSounds();
   music = std::shared_ptr<Mix_Music>(Mix_LoadMUS("res/sounds/music/underground.wav"), Mix_FreeMusic);

   return 0;
}

int SoundManager::Quit() {
   stopMusic();

   Mix_HaltChannel(-1);
   sounds.clear();
   music.reset();
   Mix_CloseAudio();
   Mix_Quit();

   return 0;
}

std::shared_ptr<Mix_Chunk> SoundManager::loadSound(const char* path) {
   return std::shared_ptr<Mix_Chunk>(Mix_LoadWAV(path), Mix_FreeChunk);
}

void SoundManager::playSound(SoundID sound) {
   auto* chunk = sounds.at(sound).get();
   Mix_PlayChannel(-1, chunk, 0);
}

void SoundManager::playMusic() {
   stopMusic();
   Mix_PlayMusic(music.get(), -1);
}

void SoundManager::pauseMusic() {
   Mix_PauseMusic();
}

void SoundManager::resumeMusic() {
   Mix_ResumeMusic();
}

void SoundManager::stopMusic() {
   Mix_HaltMusic();
}

void SoundManager::loadSounds() {
   sounds.insert({SoundID::BLOCK_HIT, loadSound("res/sounds/effects/blockhit.wav")});
   sounds.insert({SoundID::BOWSER_FALL, loadSound("res/sounds/effects/bowserfall.wav")});
   sounds.insert({SoundID::CASTLE_CLEAR, loadSound("res/sounds/effects/castleclear.wav")});
   sounds.insert({SoundID::COIN, loadSound("res/sounds/effects/coin.wav")});
   sounds.insert({SoundID::DEATH, loadSound("res/sounds/effects/death.wav")});
   sounds.insert({SoundID::JUMP, loadSound("res/sounds/effects/jump.wav")});
   sounds.insert({SoundID::KICK, loadSound("res/sounds/effects/kick.wav")});
   sounds.insert({SoundID::ONE_UP, loadSound("res/sounds/effects/oneup.wav")});
   sounds.insert({SoundID::PIPE, loadSound("res/sounds/effects/pipe.wav")});
   sounds.insert({SoundID::POWER_UP_COLLECT, loadSound("res/sounds/effects/powerupcollect.wav")});
   sounds.insert({SoundID::SHRINK, loadSound("res/sounds/effects/shrink.wav")});
   sounds.insert({SoundID::STOMP, loadSound("res/sounds/effects/stomp.wav")});
}

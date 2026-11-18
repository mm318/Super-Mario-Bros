#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <memory>

class TextureManager {
public:
   static TextureManager& Get() { return instance; }

   int Init();
   int Quit();
   std::shared_ptr<SDL_Texture> LoadSharedTexture(const char* path);
   std::shared_ptr<TTF_Font> LoadSharedFont(const char* path, int fontSize);
   SDL_Renderer* getRenderer() { return renderer; }

private:
   TextureManager() = default;
   TextureManager(const TextureManager&) = delete;
   static TextureManager instance;
   SDL_Window* window = nullptr;
   SDL_Renderer* renderer = nullptr;
};

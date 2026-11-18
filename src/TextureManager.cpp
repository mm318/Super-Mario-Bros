#include "TextureManager.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include <iostream>

namespace {
constexpr int screenWidth = 800, screenHeight = 480;
constexpr int defaultWindowScale = 2;
}

TextureManager TextureManager::instance;

int TextureManager::Init() {
   if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
      std::cerr << "Failed to Initialize SDL2: " << SDL_GetError() << std::endl;
      return -1;
   }
   if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0) {
      std::cerr << "Failed to Initialize SDL2_IMG: " << SDL_GetError() << std::endl;
      return -1;
   }
   if (TTF_Init() != 0) {
      std::cerr << "Failed to Initialize SDL2_TTF: " << SDL_GetError() << std::endl;
      return -1;
   }

   SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
   window = SDL_CreateWindow("Super Mario Bros. - Battle Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             screenWidth * defaultWindowScale, screenHeight * defaultWindowScale,
                             SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
   if (!window) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to Create Window: %s\n", SDL_GetError());
      std::cerr << "Failed to Create Window: " << SDL_GetError() << std::endl;
      return -1;
   }

   SDL_Surface* iconSurface = IMG_Load("res/sprites/icons/windowicon.png");

   SDL_SetWindowIcon(window, iconSurface);

   SDL_FreeSurface(iconSurface);

   renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
   if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
   if (!renderer) {
      SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to Create Renderer: %s", SDL_GetError());
      std::cerr << "Failed to Create Renderer: " << SDL_GetError() << std::endl;
      return -1;
   }

   SDL_RenderSetLogicalSize(renderer, screenWidth, screenHeight);

   return 0;
}

int TextureManager::Quit() {
   SDL_DestroyRenderer(renderer);
   SDL_DestroyWindow(window);

   TTF_Quit();
   IMG_Quit();
   SDL_Quit();

   return 0;
}

std::shared_ptr<SDL_Texture> TextureManager::LoadSharedTexture(const char* path) {
   SDL_Surface* surface = IMG_Load(path);
   SDL_SetColorKey(surface, SDL_TRUE, SDL_MapRGB(surface->format, 147, 187, 236));
   std::shared_ptr<SDL_Texture> texture(SDL_CreateTextureFromSurface(renderer, surface),
                                        SDL_DestroyTexture);
   SDL_FreeSurface(surface);
   return texture;
}

std::shared_ptr<TTF_Font> TextureManager::LoadSharedFont(const char* path, int fontSize) {
   return std::shared_ptr<TTF_Font>(TTF_OpenFont(path, fontSize), TTF_CloseFont);
}

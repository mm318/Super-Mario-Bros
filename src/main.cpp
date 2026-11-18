#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#define SDL_MAIN_HANDLED
#include "Core.h"

#ifndef __EMSCRIPTEN__
#include <filesystem>
#include <iostream>
#endif

#ifdef __EMSCRIPTEN__
void runLoop(void* data) {
   Core* core = (Core*)data;
   static bool initialized = false;
   if (!initialized) {
      // SDL configures loop timing when it creates the renderer.
      if (core->init() != 0) {
         emscripten_cancel_main_loop();
         return;
      }
      initialized = true;
      emscripten_set_main_loop_timing(EM_TIMING_RAF, 1);
   }
   core->mainLoop();
}
#endif

int main() {
   SDL_SetMainReady();
#ifndef __EMSCRIPTEN__
   char* base = SDL_GetBasePath();
   if (!base) {
      std::cerr << "Could not locate the application directory: " << SDL_GetError() << std::endl;
      return 1;
   }
   const std::filesystem::path resourceDirectory(base);
   SDL_free(base);
   std::error_code error;
   std::filesystem::current_path(resourceDirectory, error);
   if (error) {
      std::cerr << "Could not open the application directory: " << error.message() << std::endl;
      return 1;
   }
#endif
   Core core;

#ifdef __EMSCRIPTEN__
   emscripten_set_main_loop_arg(runLoop, &core, 0, 1);
#else
   if (core.init() != 0) {
      return -1;
   }
   core.run();
#endif

   return 0;
}

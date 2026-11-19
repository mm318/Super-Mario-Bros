#include "battle/BattleScene.h"
#include "SoundManager.h"
#include "TextureManager.h"
#include <SDL2/SDL_image.h>
#include <algorithm>
#include <cmath>
#include <ctime>

namespace {
constexpr SDL_Color ink{12,18,38,255}, white{244,242,224,255}, gold{255,210,70,255};
constexpr SDL_Color red{255,107,99,255}, green{108,225,143,255}, dim{143,161,187,255};
constexpr int panelTitleY=80, helpBackY=400, menuPaddingTop=10;
SDL_Rect enemySource(int id, int h = 16) { return {1+(id%35)*16, 1+(id/35)*16, 16, h}; }
SDL_Rect blockSource(int id) { return {1+(id%48)*17, 1+(id/48)*17, 16, 16}; }
const char* name(int i) { return i == 0 ? "MARIO" : "LUIGI"; }
}
BattleScene::BattleScene(bool testMode) : game(static_cast<uint32_t>(std::time(nullptr))) {
   game.rules.invinciblePlayers = testMode;
   auto& t = TextureManager::Get(); renderer = t.getRenderer();
   SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
   playerTexture = t.LoadSharedTexture("res/sprites/characters/PlayerSpriteSheet.png");
   enemyTexture = t.LoadSharedTexture("res/sprites/characters/EnemySpriteSheet.png");
   createAngryEnemyTexture();
   blockTexture = t.LoadSharedTexture("res/sprites/blocks/BlockTileSheet.png");
   createHudSilhouettes();
   for (int size : {8,10,12,16,24,32}) fonts[size] = t.LoadSharedFont("res/fonts/press-start-2p.ttf", size);
   for (int i=0; i<SDL_NumJoysticks(); ++i) openController(i);
   Mix_Volume(-1, MIX_MAX_VOLUME/2); Mix_VolumeMusic(MIX_MAX_VOLUME/3);
   previous = static_cast<double>(SDL_GetPerformanceCounter())/SDL_GetPerformanceFrequency();
}
BattleScene::~BattleScene() { for (auto* c : controllers) if (c) SDL_GameControllerClose(c); }
void BattleScene::createAngryEnemyTexture() {
   SDL_Surface* original=IMG_Load("res/sprites/characters/EnemySpriteSheet.png");
   if (!original) return;
   SDL_Surface* sheet=SDL_ConvertSurfaceFormat(original,SDL_PIXELFORMAT_RGBA32,0);
   SDL_FreeSurface(original);
   if (!sheet) return;
   for (int y=0;y<sheet->h;++y) {
      auto* pixels=reinterpret_cast<Uint32*>(static_cast<Uint8*>(sheet->pixels)+y*sheet->pitch);
      for (int x=0;x<sheet->w;++x) {
         Uint8 r,g,b,a; SDL_GetRGBA(pixels[x],sheet->format,&r,&g,&b,&a);
         if (!a || (r==147 && g==187 && b==236)) continue;
         // Raise red to the pixel's brightness so green/blue enemies turn red
         // too, while black outlines and transparency stay intact.
         r=std::max(r,std::max(g,b));
         pixels[x]=SDL_MapRGBA(sheet->format,r,g*140/255,b*130/255,a);
      }
   }
   SDL_SetColorKey(sheet,SDL_TRUE,SDL_MapRGB(sheet->format,147,187,236));
   angryEnemyTexture={SDL_CreateTextureFromSurface(renderer,sheet),SDL_DestroyTexture};
   SDL_FreeSurface(sheet);
}
void BattleScene::createHudSilhouettes() {
   // Use the original sprites' opaque pixels for the empty HUD slots.
   SDL_Surface* original=IMG_Load("res/sprites/blocks/BlockTileSheet.png");
   if (!original) return;
   SDL_Surface* sheet=SDL_ConvertSurfaceFormat(original,SDL_PIXELFORMAT_RGBA32,0);
   SDL_FreeSurface(original);
   if (!sheet) return;
   for (int icon=0;icon<2;++icon) {
      const auto source=blockSource(icon==0?656:96);
      SDL_Surface* mask=SDL_CreateRGBSurfaceWithFormat(0,16,16,32,SDL_PIXELFORMAT_RGBA32);
      if (!mask) continue;
      SDL_FillRect(mask,nullptr,SDL_MapRGBA(mask->format,0,0,0,0));
      for (int y=0;y<16;++y) {
         const auto* input=reinterpret_cast<const Uint32*>(static_cast<const Uint8*>(sheet->pixels)+(source.y+y)*sheet->pitch);
         auto* output=reinterpret_cast<Uint32*>(static_cast<Uint8*>(mask->pixels)+y*mask->pitch);
         for (int x=0;x<16;++x) {
            Uint8 r,g,b,a; SDL_GetRGBA(input[source.x+x],sheet->format,&r,&g,&b,&a);
            if (a && !(r==147 && g==187 && b==236)) output[x]=SDL_MapRGBA(mask->format,55,69,92,a);
         }
      }
      hudSilhouettes[icon]={SDL_CreateTextureFromSurface(renderer,mask),SDL_DestroyTexture};
      SDL_FreeSurface(mask);
   }
   SDL_FreeSurface(sheet);
}
void BattleScene::openController(int index) {
   if (!SDL_IsGameController(index)) return;
   const auto id = SDL_JoystickGetDeviceInstanceID(index);
   for (auto* c : controllers)
      if (c && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(c)) == id) return;
   for (auto*& c : controllers) if (!c) { c=SDL_GameControllerOpen(index); break; }
}
void BattleScene::setPaused(bool value) {
   if (value && !paused) pauseChoice=0;
   paused=value; accumulator=0;
   if (paused) SoundManager::Get().pauseMusic(); else SoundManager::Get().resumeMusic();
}
void BattleScene::startBattle() {
   title=help=false; game.startMatch(); particles.clear(); accumulator=0;
   resultDelay=noticeTimer=0; setPaused(false);
   SoundManager::Get().playMusic();
}
void BattleScene::selectMenuOption() {
   if (help) { help=false; return; }
   if (title) {
      if (titleChoice==0) startBattle();
      else help=true;
   } else if (paused) {
      switch (pauseChoice) {
         case 0: setPaused(false); break;
         case 1: startBattle(); break;
         case 2: help=true; break;
         case 3:
            title=true; titleChoice=0; setPaused(false); game.startMatch();
            particles.clear(); noticeTimer=0; SoundManager::Get().stopMusic(); break;
      }
   }
}
void BattleScene::handleEvent(const SDL_Event& e) {
   if (e.type==SDL_CONTROLLERDEVICEADDED) openController(e.cdevice.which);
   if (e.type==SDL_CONTROLLERDEVICEREMOVED) for (auto*& c : controllers)
      if (c && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(c))==e.cdevice.which) {
         SDL_GameControllerClose(c); c=nullptr;
      }
   if (e.type==SDL_WINDOWEVENT) {
      if (e.window.event==SDL_WINDOWEVENT_FOCUS_LOST) { focused=false; accumulator=0; SoundManager::Get().pauseMusic(); }
      if (e.window.event==SDL_WINDOWEVENT_FOCUS_GAINED) {
         focused=true; accumulator=0;
         if (!paused && !title) SoundManager::Get().resumeMusic();
      }
   }
   if ((e.type==SDL_MOUSEMOTION || e.type==SDL_MOUSEBUTTONDOWN) && (title || paused || help)) {
      // SDL already scales mouse events to the renderer's logical coordinates.
      const int x=e.type==SDL_MOUSEMOTION?e.motion.x:e.button.x;
      const int y=e.type==SDL_MOUSEMOTION?e.motion.y:e.button.y;
      const int first=help?helpBackY:title?334:174, spacing=title?42:52, count=help?1:title?2:4;
      for (int option=0;option<count;++option) {
         const int row=first+option*spacing;
         if (x>=220 && x<580 && y>=row-menuPaddingTop && y<row+28) {
            if (!help) { if (title) titleChoice=option; else pauseChoice=option; }
            if (e.type==SDL_MOUSEBUTTONDOWN && e.button.button==SDL_BUTTON_LEFT) selectMenuOption();
            break;
         }
      }
      return;
   }
   SDL_Keycode key=0;
   if (e.type==SDL_KEYDOWN && !e.key.repeat) key=e.key.keysym.sym;
   if (e.type==SDL_CONTROLLERBUTTONDOWN) {
      const bool menu=title || paused || help;
      if (menu && e.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_UP) key=SDLK_UP;
      if (menu && e.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_DOWN) key=SDLK_DOWN;
      if (menu && e.cbutton.button==SDL_CONTROLLER_BUTTON_A) key=SDLK_RETURN;
      if (e.cbutton.button==SDL_CONTROLLER_BUTTON_START &&
          (menu || game.phase==battle::Phase::RoundOver || game.phase==battle::Phase::MatchOver)) key=SDLK_RETURN;
   }
   if (!key) return;
   if (key==SDLK_m) { muted=!muted; Mix_Volume(-1,muted?0:MIX_MAX_VOLUME/2); Mix_VolumeMusic(muted?0:MIX_MAX_VOLUME/3); }
   if (key==SDLK_F11) {
      fullscreen=!fullscreen;
      SDL_SetWindowFullscreen(SDL_RenderGetWindow(renderer),fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0);
   }
   if (key==SDLK_ESCAPE) {
      if (help) help=false;
      else if (!title) setPaused(!paused);
      return;
   }
   if ((title || paused) && !help && (key==SDLK_UP || key==SDLK_DOWN)) {
      auto& choice=title?titleChoice:pauseChoice;
      const int count=title?2:4;
      choice=(choice+(key==SDLK_UP?count-1:1))%count;
      return;
   }
   if (key==SDLK_RETURN || key==SDLK_SPACE) {
      if (help || title || paused) selectMenuOption();
      else if (resultDelay>.6f && (game.phase==battle::Phase::RoundOver || game.phase==battle::Phase::MatchOver)) {
         game.nextRound(); particles.clear(); resultDelay=0; accumulator=0;
         SoundManager::Get().playMusic();
      }
   }
}
std::array<battle::Input,2> BattleScene::readInput() const {
   const Uint8* k=SDL_GetKeyboardState(nullptr);
   std::array<battle::Input,2> in{};
   if (!focused) return in;
   in[0]={bool(k[SDL_SCANCODE_F]),bool(k[SDL_SCANCODE_H]),bool(k[SDL_SCANCODE_S]),bool(k[SDL_SCANCODE_A]),bool(k[SDL_SCANCODE_G])};
   in[1]={bool(k[SDL_SCANCODE_LEFT]),bool(k[SDL_SCANCODE_RIGHT]),bool(k[SDL_SCANCODE_PERIOD]),bool(k[SDL_SCANCODE_COMMA]),bool(k[SDL_SCANCODE_DOWN])};
   for (int i=0; i<2; ++i) if (controllers[i]) {
      auto* c=controllers[i];
      const auto button=[c](SDL_GameControllerButton b) { return SDL_GameControllerGetButton(c,b)!=0; };
      const auto axis=SDL_GameControllerGetAxis(c,SDL_CONTROLLER_AXIS_LEFTX);
      in[i].left |= axis < -10000 || button(SDL_CONTROLLER_BUTTON_DPAD_LEFT);
      in[i].right |= axis > 10000 || button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
      in[i].jump |= button(SDL_CONTROLLER_BUTTON_A)||button(SDL_CONTROLLER_BUTTON_B);
      in[i].dash |= button(SDL_CONTROLLER_BUTTON_X)||button(SDL_CONTROLLER_BUTTON_Y);
      in[i].down |= SDL_GameControllerGetAxis(c,SDL_CONTROLLER_AXIS_LEFTY)>10000 || button(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
   }
   return in;
}
void BattleScene::processEvents() {
   using E=battle::EventType;
   auto& s=SoundManager::Get();
   for (const auto& e : game.events) {
      switch (e.type) {
         case E::Jump: s.playSound(SoundID::JUMP); break;
         case E::Bump: s.playSound(SoundID::BLOCK_HIT); break;
         case E::Stun: s.playSound(SoundID::STOMP); break;
         case E::Kick: s.playSound(SoundID::KICK); break;
         case E::Coin: s.playSound(SoundID::COIN); break;
         case E::Hurt: s.playSound(SoundID::SHRINK); break;
         case E::Death: s.playSound(SoundID::DEATH); break;
         case E::Pow: s.playSound(SoundID::BOWSER_FALL); notice="POW! ENEMIES FLIPPED / RIVAL STUNNED"; noticeTimer=2; break;
         case E::Mushroom: s.playSound(SoundID::POWER_UP_COLLECT); break;
         case E::SizeSwap: s.playSound(SoundID::PIPE); notice="SIZES SWAPPED"; noticeTimer=2; break;
         case E::PositionSwap: s.playSound(SoundID::PIPE); notice="POSITIONS SWAPPED"; noticeTimer=2; break;
         case E::RoundWin: s.stopMusic(); s.playSound(SoundID::CASTLE_CLEAR); resultDelay=0; break;
         case E::MatchWin: s.playSound(SoundID::ONE_UP); break;
      }
      if (e.type==E::Coin || e.type==E::Pow || e.type==E::SizeSwap || e.type==E::PositionSwap || e.type==E::Hurt) {
         for (int i=0; i<14; ++i) {
            const float a=i*6.283185f/14;
            particles.push_back({e.x,e.y-16,std::cos(a)*110,std::sin(a)*110-60,.65f,e.type==E::Hurt?red:gold});
         }
      }
   }
}
void BattleScene::update() {
   const double now=static_cast<double>(SDL_GetPerformanceCounter())/SDL_GetPerformanceFrequency();
   const float dt=static_cast<float>(std::min(now-previous,.1)); previous=now;
   if (focused && !paused && !help) animation+=dt;
   if (focused && !title && !paused && !help) {
      if (game.phase==battle::Phase::RoundOver || game.phase==battle::Phase::MatchOver) resultDelay+=dt;
      accumulator+=dt;
      const auto input=readInput();
      while (accumulator>=1.0/120) { game.step(1.f/120,input); processEvents(); accumulator-=1.0/120; }
      noticeTimer=std::max(0.f,noticeTimer-dt);
      for (auto& p : particles) { p.life-=dt; p.x+=p.vx*dt; p.y+=p.vy*dt; p.vy+=240*dt; }
      particles.erase(std::remove_if(particles.begin(),particles.end(),[](const Particle& p){return p.life<=0;}),particles.end());
   }
   render();
}
void BattleScene::rect(int x,int y,int w,int h,SDL_Color c) {
   SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,c.a); SDL_Rect r{x,y,w,h}; SDL_RenderFillRect(renderer,&r);
}
void BattleScene::text(const std::string& value,int x,int y,int size,SDL_Color c,bool centered) {
   const std::string key=std::to_string(size)+":"+std::to_string(c.r)+":"+std::to_string(c.g)+":"+std::to_string(c.b)+value;
   auto found=labels.find(key);
   if (found==labels.end()) {
      SDL_Surface* surface=TTF_RenderUTF8_Blended(fonts.at(size).get(),value.c_str(),c);
      if (!surface) return;
      Label label{std::shared_ptr<SDL_Texture>(SDL_CreateTextureFromSurface(renderer,surface),SDL_DestroyTexture),surface->w,surface->h};
      SDL_FreeSurface(surface); found=labels.emplace(key,std::move(label)).first;
   }
   const auto& label=found->second;
   SDL_Rect d{x-(centered?label.w/2:0),y,label.w,label.h}; SDL_RenderCopy(renderer,label.texture.get(),nullptr,&d);
}
void BattleScene::sprite(SDL_Texture* t,SDL_Rect s,SDL_Rect d,bool flip,bool upsideDown,bool wrapHorizontal) {
   const auto f=static_cast<SDL_RendererFlip>((flip?SDL_FLIP_HORIZONTAL:0)|(upsideDown?SDL_FLIP_VERTICAL:0));
   SDL_RenderCopyEx(renderer,t,&s,&d,0,nullptr,f);
   if (!wrapHorizontal) return;
   if (d.x<0) { d.x+=800; SDL_RenderCopyEx(renderer,t,&s,&d,0,nullptr,f); }
   else if (d.x+d.w>800) { d.x-=800; SDL_RenderCopyEx(renderer,t,&s,&d,0,nullptr,f); }
}
void BattleScene::drawPlayer(const battle::Player& p,int i) {
   if (p.eliminated) return;
   const bool stunned=p.stunTimer>0;
   const int h=static_cast<int>(p.height()), w=p.super&&!p.crouched?24:28;
   // Both rows use columns 3-5 for walking and column 7 for jumping.
   int frame=std::abs(p.vx)>20 ? 2+static_cast<int>(animation*(std::abs(p.vx)>190?15:9))%3 : 0;
   if (!p.grounded) frame=6;
   if (stunned || p.crouched) frame=1;
   SDL_Rect src{1+frame*16,9+(i*4+(p.super?1:0))*16,16,p.super?32:16};
   // The large crouch frame has ten blank rows above its 22-pixel pose.
   if (p.super && frame==1) { src.y+=10; src.h=22; }
   if (stunned) drawStunStars(p.x,p.y-h,false);
   if (!(p.invulnerable>0 && static_cast<int>(animation*14)%2))
      sprite(playerTexture.get(),src,{static_cast<int>(p.x)-w/2,static_cast<int>(p.y)-h,w,h},p.facing<0);
   if (stunned) drawStunStars(p.x,p.y-h,true);
}
void BattleScene::drawStunStars(float centerX,float headY,bool front) {
   for (int star=0;star<4;++star) {
      const float angle=animation*6+star*1.570796f, depth=std::sin(angle);
      if ((depth>=0)!=front) continue;
      const int size=front?8:6;
      const int x=static_cast<int>(centerX+std::cos(angle)*20)-size/2;
      const int y=static_cast<int>(headY-5+depth*5)-size/2;
      sprite(blockTexture.get(),blockSource(96),{x,y,size,size});
   }
}
void BattleScene::drawEnemy(const battle::Enemy& e) {
   const int x=static_cast<int>(e.x), y=static_cast<int>(e.y), frame=e.alive?static_cast<int>(animation*7)%2:0;
   auto* texture=e.angry && angryEnemyTexture?angryEnemyTexture.get():enemyTexture.get();
   const bool stunned=e.alive && (e.stunned || (e.isKoopa() && e.shell && std::abs(e.vx)<.01f));
   if (stunned) drawStunStars(e.x,e.y-28,false);
   if (e.type==battle::EnemyType::Spiny) {
      sprite(texture,enemySource(502+frame),{x-14,y-28,28,28},e.direction>0,e.stunned,e.alive);
   } else if (e.type==battle::EnemyType::BuzzyBeetle) {
      // Underground palette, walking frames 192/193 and shell 194.
      sprite(texture,enemySource(e.stunned?194:192+frame),{x-14,y-28,28,28},e.direction>0,false,e.alive);
   } else {
      const bool shell=e.stunned||e.shell;
      const int id=shell?77:(e.type==battle::EnemyType::FlyingKoopa && e.winged?40:38)+frame;
      const int height=shell?28:e.type==battle::EnemyType::WalkingKoopa?48:56;
      sprite(texture,enemySource(id,shell?16:32),{x-14,y-height,28,height},e.direction>0,e.stunned,e.alive);
   }
   if (stunned) drawStunStars(e.x,e.y-28,true);
}
void BattleScene::drawBoo(const battle::Boo& b) {
   // Castle palette, full Lakitu in its cloud (column 15, rows 10/11).
   sprite(enemyTexture.get(),enemySource(365,32),
          {static_cast<int>(b.x)-14,static_cast<int>(b.y)-56,28,56},b.vx>0);
}
void BattleScene::drawItem(const battle::Item& item) {
   int x=static_cast<int>(item.x),y=static_cast<int>(item.y);
   if (item.type==battle::ItemType::Coin) {
      sprite(blockTexture.get(),blockSource(656+static_cast<int>(animation*8)%3),{x-12,y-26,24,26});
   } else {
      const int tile=item.type==battle::ItemType::Mushroom?608:
                     item.type==battle::ItemType::SizeSwapMushroom?52:68;
      sprite(blockTexture.get(),blockSource(tile),{x-14,y-28,28,28});
   }
}
void BattleScene::drawPipe(int x,int y,bool right) {
   // Overworld horizontal pipe: four blocks at columns 4/5, rows 5/6.
   for (int row=0;row<2;++row) for (int column=0;column<2;++column) {
      const int sourceColumn=right?column:1-column;
      sprite(blockTexture.get(),blockSource(244+row*48+sourceColumn),
             {x+column*32,y+row*32,32,32},!right);
   }
}
void BattleScene::drawHud() {
   rect(0,0,800,battle::hudHeight,ink); rect(0,battle::hudHeight-1,800,1,{64,79,104,255});
   for (int i=0;i<2;++i) {
      text(name(i),i==0?16:724,8,12,i==0?red:green);
      for (int n=0;n<5;++n) {
         const SDL_Rect coin{i==0?102+n*14:686-n*14,7,12,14};
         const SDL_Rect star{i==0?188+n*17:598-n*17,7,14,14};
         sprite(hudSilhouettes[0].get(),{0,0,16,16},coin);
         sprite(hudSilhouettes[1].get(),{0,0,16,16},star);
         if (n<game.players[i].coins) sprite(blockTexture.get(),blockSource(656),coin);
         if (n<game.wins[i]) sprite(blockTexture.get(),blockSource(96),star);
      }
   }
   text("ROUND "+std::to_string(game.round),400,9,10,white,true);
}
void BattleScene::drawPanel(const std::string& heading,SDL_Color accent) {
   rect(0,battle::hudHeight,800,448-battle::hudHeight,{3,7,19,175});
   rect(68,56,664,376,ink); rect(68,56,664,4,accent);
   rect(68,428,664,4,{53,67,91,255}); text(heading,400,panelTitleY,24,accent,true);
}
void BattleScene::drawMenuOption(const std::string& label,int y,bool selected) {
   if (selected) { rect(220,y-menuPaddingTop,360,38,{38,47,66,255}); rect(320,y+25,160,2,gold); }
   text(label,400,y,16,selected?gold:white,true);
}
void BattleScene::drawHelp() {
   drawPanel("HOW TO BATTLE",gold);
   const int titleBottom=panelTitleY+TTF_FontHeight(fonts.at(24).get());
   const int blockHeight=212+TTF_FontHeight(fonts.at(10).get());
   const int top=titleBottom+(helpBackY-menuPaddingTop-titleBottom-blockHeight)/2;
   text("BUMP AND KICK",400,top,12,gold,true);
   text("Hit platforms from below to stun enemies.",400,top+26,10,white,true);
   text("Jump onto platforms and kick stunned foes.",400,top+44,10,white,true);
   text("COLLECT FIVE COINS",400,top+84,12,gold,true);
   text("Coins enter through the pipes. Either player can take them.",400,top+110,10,white,true);
   text("Collect five coins to win the round.",400,top+128,10,white,true);
   text("SURVIVE THE RIVALRY",400,top+168,12,gold,true);
   text("Eliminating the opponent also wins the round.",400,top+194,10,white,true);
   text("First to five round wins takes the match.",400,top+212,10,white,true);
   drawMenuOption("BACK",helpBackY,true);
}
void BattleScene::render() {
   SDL_Rect originalViewport;
   SDL_RenderGetViewport(renderer,&originalViewport);
   SDL_SetRenderDrawColor(renderer,7,12,28,255); SDL_RenderClear(renderer);
   const int palette=(game.round-1)%3;
   rect(0,battle::hudHeight,800,448-battle::hudHeight,palette==0?SDL_Color{19,29,51,255}:palette==1?SDL_Color{32,24,49,255}:SDL_Color{15,40,43,255});
   for (int y=battle::hudHeight+14;y<448;y+=32) for (int x=((y/32)%2)*32;x<800;x+=64) {
      rect(x,y,60,1,{40,48,65,255}); rect(x,y,1,28,{34,43,62,255});
   }
   SDL_Rect viewport=originalViewport;
   if (game.shake>0) viewport.y+=static_cast<int>(std::sin(animation*150)*4);
   SDL_RenderSetViewport(renderer,&viewport);
   for (const auto& s : game.segments) {
      const int y=static_cast<int>(s.y-std::sin(s.bump/.22f*3.14159f)*8);
      // Underground thick brick with the illuminated top (column 17, row 6).
      sprite(blockTexture.get(),blockSource(305),{static_cast<int>(s.x),y,32,static_cast<int>(battle::platformThickness)});
   }
   for (int x=0;x<800;x+=32) sprite(blockTexture.get(),blockSource(305),{x,448,32,32});
   if (game.powCharges) {
      rect(380,344,40,32,{8,10,22,56}); rect(383,346,34,27,{75,131,221,90}); text("POW",400,352,10,white,true);
      for (int i=0;i<game.powCharges;++i) rect(385+i*10,367,8,3,{244,242,224,170});
   }
   for (const auto& i : game.items) drawItem(i);
   for (const auto& e : game.enemies) drawEnemy(e);
   for (const auto& b : game.boos) drawBoo(b);
   drawPipe(0,static_cast<int>(battle::topPlatformY)-64,false); drawPipe(736,static_cast<int>(battle::topPlatformY)-64,true);
   drawPipe(0,384,false); drawPipe(736,384,true);
   for (int i=0;i<2;++i) drawPlayer(game.players[i],i);
   for (const auto& e : game.defeatedEnemies) drawEnemy(e);
   for (const auto& p : particles) rect(static_cast<int>(p.x),static_cast<int>(p.y),4,4,p.color);
   SDL_RenderSetViewport(renderer,&originalViewport); drawHud();
   rect(0,464,800,16,ink);
   text("ESC PAUSE   M "+std::string(muted?"UNMUTE":"MUTE")+"   F11 FULLSCREEN"+
        (game.rules.invinciblePlayers?"   TEST: NO ELIMINATION":""),400,468,8,dim,true);
   if (noticeTimer>0) text(notice,400,battle::hudHeight+8,10,gold,true);
   if (help) drawHelp();
   else if (title) {
      drawPanel("SUPER MARIO BROS.",gold); text("BATTLE GAME",400,126,32,white,true);
      text("5 COINS TO WIN A ROUND / 5 ROUNDS TO WIN THE MATCH",400,184,10,gold,true);
      rect(98,218,288,78,{39,33,48,255}); rect(414,218,288,78,{23,48,42,255});
      text("1  MARIO",118,231,12,red); text("2  LUIGI",434,231,12,green);
      text("F H MOVE / S JUMP / G DUCK",118,256,8,white); text("A TO DASH",118,275,8,dim);
      text("LEFT RIGHT MOVE / . JUMP",434,256,8,white); text(", TO DASH / DOWN TO DUCK",434,275,8,dim);
      drawMenuOption("START BATTLE",334,titleChoice==0);
      drawMenuOption("HOW TO BATTLE",376,titleChoice==1);
      text("TWO PLAYERS / KEYBOARD OR TWO GAMEPADS",400,412,8,dim,true);
   } else if (paused) {
      drawPanel("PAUSED",gold);
      const char* options[]={"RESUME BATTLE","RESTART MATCH","HOW TO BATTLE","TITLE SCREEN"};
      for (int option=0;option<4;++option) drawMenuOption(options[option],174+option*52,pauseChoice==option);
   } else if (game.phase==battle::Phase::Ready) {
      rect(215,176,370,112,ink); text("ROUND "+std::to_string(game.round),400,194,24,gold,true);
      text(game.readyTimer<.8f?"BATTLE!":"GET READY",400,239,16,white,true);
   } else if (game.phase==battle::Phase::RoundOver || game.phase==battle::Phase::MatchOver) {
      const bool match=game.phase==battle::Phase::MatchOver;
      drawPanel(std::string(name(game.winner))+(match?" WINS THE MATCH!":" WINS THE ROUND!"),game.winner==0?red:green);
      text(game.coinVictory?"FIVE COINS COLLECTED":"OPPONENT ELIMINATED",400,172,12,gold,true);
      text(std::to_string(game.wins[0])+"  -  "+std::to_string(game.wins[1]),400,221,32,white,true);
      text("MARIO               LUIGI",400,273,12,dim,true);
      text(match?"FIVE ROUND VICTORIES":"FIRST TO FIVE ROUND WINS",400,320,12,gold,true);
      text(match?"ENTER: PLAY ANOTHER MATCH":"ENTER: NEXT ROUND",400,374,12,green,true);
      text("ESC: PAUSE",400,402,8,dim,true);
   }
   SDL_RenderPresent(renderer);
}

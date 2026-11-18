#include "battle/BattleScene.h"
#include "SoundManager.h"
#include "TextureManager.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace {
constexpr SDL_Color ink{12,18,38,255}, white{244,242,224,255}, gold{255,210,70,255};
constexpr SDL_Color red{255,107,99,255}, green{108,225,143,255}, dim{143,161,187,255};
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
   blockTexture = t.LoadSharedTexture("res/sprites/blocks/BlockTileSheet.png");
   for (int size : {8,10,12,16,24,32}) fonts[size] = t.LoadSharedFont("res/fonts/press-start-2p.ttf", size);
   for (int i=0; i<SDL_NumJoysticks(); ++i) openController(i);
   loadStars();
   Mix_Volume(-1, MIX_MAX_VOLUME/2); Mix_VolumeMusic(MIX_MAX_VOLUME/3);
   previous = static_cast<double>(SDL_GetPerformanceCounter())/SDL_GetPerformanceFrequency();
}
BattleScene::~BattleScene() { for (auto* c : controllers) if (c) SDL_GameControllerClose(c); }
void BattleScene::openController(int index) {
   if (!SDL_IsGameController(index)) return;
   const auto id = SDL_JoystickGetDeviceInstanceID(index);
   for (auto* c : controllers)
      if (c && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(c)) == id) return;
   for (auto*& c : controllers) if (!c) { c=SDL_GameControllerOpen(index); break; }
}
void BattleScene::loadStars() {
#ifdef __EMSCRIPTEN__
   for (int i=0; i<2; ++i) game.stars[i]=EM_ASM_INT({
      try { return Number(globalThis['localStorage']['getItem']('mario-battle-stars-'+$0)) || 0; } catch (_) { return 0; }
   }, i);
#else
   char* pref=SDL_GetPrefPath("SuperMarioBros", "BattleGame");
   if (pref) { savePath=std::string(pref)+"stars.txt"; SDL_free(pref); }
   std::ifstream saved(savePath); saved >> game.stars[0] >> game.stars[1];
#endif
   for (auto& n : game.stars) if (n<0 || n>5) n=0;
}
void BattleScene::saveStars() {
#ifdef __EMSCRIPTEN__
   EM_ASM({ try {
      globalThis['localStorage']['setItem']('mario-battle-stars-0',$0); globalThis['localStorage']['setItem']('mario-battle-stars-1',$1);
   } catch (_) {} }, game.stars[0], game.stars[1]);
#else
   if (savePath.empty()) return;
   const std::string temporary=savePath+".tmp";
   std::ofstream out(temporary); out << game.stars[0] << ' ' << game.stars[1] << '\n'; out.close();
   if (out) std::rename(temporary.c_str(),savePath.c_str());
#endif
}
void BattleScene::setPaused(bool value) {
   paused=value; accumulator=0;
   if (paused) SoundManager::Get().pauseMusic(); else SoundManager::Get().resumeMusic();
}
void BattleScene::handleEvent(const SDL_Event& e) {
   if (e.type==SDL_CONTROLLERDEVICEADDED) openController(e.cdevice.which);
   if (e.type==SDL_CONTROLLERDEVICEREMOVED) for (auto*& c : controllers)
      if (c && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(c))==e.cdevice.which) {
         SDL_GameControllerClose(c); c=nullptr;
      }
   if (e.type==SDL_WINDOWEVENT) {
      if (e.window.event==SDL_WINDOWEVENT_FOCUS_LOST) { focused=false; if (!title) setPaused(true); }
      if (e.window.event==SDL_WINDOWEVENT_FOCUS_GAINED) focused=true;
   }
   SDL_Keycode key=0;
   if (e.type==SDL_KEYDOWN && !e.key.repeat) key=e.key.keysym.sym;
   if (e.type==SDL_CONTROLLERBUTTONDOWN) {
      if (e.cbutton.button==SDL_CONTROLLER_BUTTON_START)
         key=(title || game.phase==battle::Phase::RoundOver || game.phase==battle::Phase::MatchOver) ? SDLK_RETURN : SDLK_ESCAPE;
      if (e.cbutton.button==SDL_CONTROLLER_BUTTON_BACK) key=SDLK_TAB;
   }
   if (!key) return;
   if (key==SDLK_m) { muted=!muted; Mix_Volume(-1,muted?0:MIX_MAX_VOLUME/2); Mix_VolumeMusic(muted?0:MIX_MAX_VOLUME/3); }
   if (key==SDLK_F11) {
      fullscreen=!fullscreen;
      SDL_SetWindowFullscreen(SDL_RenderGetWindow(renderer),fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0);
   }
   if (key==SDLK_TAB) { help=!help; if (!title) setPaused(help); return; }
   if (key==SDLK_ESCAPE) {
      if (help) { help=false; if (!title) setPaused(false); }
      else if (!title) setPaused(!paused);
      return;
   }
   if (paused && key==SDLK_q) { title=true; help=false; setPaused(false); SoundManager::Get().stopMusic(); return; }
   if (paused && key==SDLK_r) {
      game.startMatch(); help=false; setPaused(false); particles.clear();
      SoundManager::Get().playMusic(); return;
   }
   if (key==SDLK_RETURN || key==SDLK_SPACE) {
      if (help) { help=false; if (!title) setPaused(false); return; }
      if (title) {
         title=false; game.startMatch(); particles.clear(); accumulator=0;
         SoundManager::Get().playMusic();
      } else if (paused) setPaused(false);
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
         case E::Pow: s.playSound(SoundID::BOWSER_FALL); notice="POW! ALL ENEMIES OVERTURNED"; noticeTimer=2; break;
         case E::Mushroom: s.playSound(SoundID::POWER_UP_COLLECT); break;
         case E::Swap: s.playSound(SoundID::PIPE); notice="? MUSHROOM: SWAP!"; noticeTimer=2; break;
         case E::RoundWin: s.stopMusic(); s.playSound(SoundID::CASTLE_CLEAR); resultDelay=0; break;
         case E::MatchWin: s.playSound(SoundID::ONE_UP); saveStars(); break;
      }
      if (e.type==E::Coin || e.type==E::Pow || e.type==E::Swap || e.type==E::Hurt) {
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
   if (!paused) animation+=dt;
   if (!title && !paused) {
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
   if (p.eliminated || (p.invulnerable>0 && static_cast<int>(animation*14)%2)) return;
   const int h=static_cast<int>(p.height()), w=p.super&&!p.crouched?24:28;
   int frame=std::abs(p.vx)>20 ? 1+static_cast<int>(animation*(std::abs(p.vx)>190?15:9))%3 : 0;
   if (!p.grounded) frame=5;
   if (p.crouched) frame=p.super?6:0;
   SDL_Rect src{1+frame*16,9+(i*4+(p.super?1:0))*16,16,p.super?32:16};
   sprite(playerTexture.get(),src,{static_cast<int>(p.x)-w/2,static_cast<int>(p.y)-h,w,h},p.facing<0);
   text(p.stunTimer>0?"STUN":i==0?"1":"2",static_cast<int>(p.x),static_cast<int>(p.y)-h-13,8,p.stunTimer>0?gold:i==0?red:green,true);
}
void BattleScene::drawEnemy(const battle::Enemy& e) {
   const int x=static_cast<int>(e.x), y=static_cast<int>(e.y), frame=e.alive?static_cast<int>(animation*7)%2:0;
   if (e.type==battle::EnemyType::Spiny) {
      SDL_SetTextureColorMod(enemyTexture.get(),255,e.speedLevel?140:255,e.speedLevel?130:255);
      sprite(enemyTexture.get(),enemySource(502+frame),{x-14,y-28,28,28},e.direction>0,e.stunned,e.alive);
      SDL_SetTextureColorMod(enemyTexture.get(),255,255,255);
   } else if (e.type==battle::EnemyType::BuzzyBeetle) {
      // Underground palette, walking frames 192/193 and shell 194.
      sprite(enemyTexture.get(),enemySource(e.stunned?194:192+frame),{x-14,y-28,28,28},e.direction>0,false,e.alive);
   } else {
      const bool shell=e.stunned||e.shell;
      const int id=shell?77:(e.winged?40:38)+frame;
      const int height=shell?28:56;
      sprite(enemyTexture.get(),enemySource(id,shell?16:32),{x-14,y-height,28,height},e.direction>0,e.stunned,e.alive);
   }
   if (e.alive && e.stunned) {
      text(e.recovery<2 && static_cast<int>(animation*8)%2?"!":"*",x,y-42,10,gold,true);
      rect(x-14,y+3,28,3,{45,51,66,255}); rect(x-14,y+3,static_cast<int>(28*e.recovery/(e.type==battle::EnemyType::FlyingKoopa?game.rules.shellRecoverySeconds:game.rules.stunSeconds)),3,gold);
   }
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
      sprite(blockTexture.get(),blockSource(52),{x-14,y-28,28,28});
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
   rect(0,0,800,72,ink); rect(0,70,800,2,{64,79,104,255});
   for (int i=0;i<2;++i) {
      const int x=i==0?24:552;
      text(name(i),x,10,16,i==0?red:green); text(game.players[i].super?"SUPER":"SMALL",x+104,14,8,white);
      text("COINS",x,36,8,dim);
      for (int n=0;n<5;++n) {
         const bool filled=n<game.players[i].coins;
         rect(x+56+n*17,34,11,13,filled?gold:SDL_Color{54,63,77,255});
         if (filled) rect(x+61+n*17,37,2,7,{179,110,38,255});
      }
      text("WINS "+std::to_string(game.wins[i])+"/5",x,56,8,white);
      text("STARS",x+112,56,8,gold);
      if (game.stars[i]==0) text("-",x+164,56,8,dim);
      for (int n=0;n<game.stars[i];++n) text("*",x+164+n*11,56,8,gold);
   }
   text("BATTLE GAME",400,12,12,gold,true); text("ROUND "+std::to_string(game.round)+" / 9",400,36,10,white,true);
   std::string roster;
   for (const auto t : battle::BattleGame::roster(game.round)) {
      if (!roster.empty()) roster+=" + ";
      roster+=t==battle::EnemyType::Spiny?"SPINY":t==battle::EnemyType::BuzzyBeetle?"BUZZY":"FLY KOOPA";
   }
   text(roster,400,56,8,dim,true);
}
void BattleScene::drawPanel(const std::string& heading,SDL_Color accent) {
   rect(0,72,800,376,{3,7,19,175}); rect(68,88,664,338,ink); rect(68,88,664,4,accent);
   rect(68,422,664,4,{53,67,91,255}); text(heading,400,112,24,accent,true);
}
void BattleScene::drawHelp() {
   drawPanel("HOW TO BATTLE",gold);
   text("1  BUMP FROM BELOW",100,161,12,gold);
   text("Hit solid bricks under a grounded enemy.",100,185,10,white);
   text("2  KICK, THEN RACE",100,214,12,gold);
   text("Climb around edges; touch stunned foes for a coin.",100,238,10,white);
   text("Either player can take it. Five coins wins!",100,256,10,white);
   text("3  SURVIVE THE RIVALRY",100,285,12,gold);
   text("Super > Small > out. Elimination also wins.",100,309,10,white);
   text("Beetles: bump to shell; stomp to revive.",100,329,10,dim);
   text("POW x3. Stomp Koopa shells to launch them.",100,349,10,dim);
   text("? Mushroom swaps positions, or sizes if different.",100,369,10,dim);
   text("ENTER / TAB TO RETURN",400,400,10,green,true);
}
void BattleScene::render() {
   SDL_Rect originalViewport;
   SDL_RenderGetViewport(renderer,&originalViewport);
   SDL_SetRenderDrawColor(renderer,7,12,28,255); SDL_RenderClear(renderer);
   const int palette=(game.round-1)%3;
   rect(0,72,800,376,palette==0?SDL_Color{19,29,51,255}:palette==1?SDL_Color{32,24,49,255}:SDL_Color{15,40,43,255});
   for (int y=86;y<448;y+=32) for (int x=((y/32)%2)*32;x<800;x+=64) {
      rect(x,y,60,1,{40,48,65,255}); rect(x,y,1,28,{34,43,62,255});
   }
   SDL_Rect viewport=originalViewport;
   if (game.shake>0) viewport.y+=static_cast<int>(std::sin(animation*150)*4);
   SDL_RenderSetViewport(renderer,&viewport);
   for (const auto& s : game.segments) {
      const int y=static_cast<int>(s.y-std::sin(s.bump/.22f*3.14159f)*8);
      // Underground palette, thick-brick tile (column 18, row 6).
      sprite(blockTexture.get(),blockSource(306),{static_cast<int>(s.x),y,32,static_cast<int>(battle::platformThickness)});
   }
   for (int x=0;x<800;x+=32) sprite(blockTexture.get(),blockSource(306),{x,448,32,32});
   if (game.powCharges) {
      rect(380,344,40,32,{8,10,22,255}); rect(383,346,34,27,{75,131,221,255}); text("POW",400,352,10,white,true);
      for (int i=0;i<game.powCharges;++i) rect(385+i*10,367,8,3,white);
   }
   for (const auto& i : game.items) drawItem(i);
   for (const auto& e : game.enemies) drawEnemy(e);
   if (game.koopa.alive) sprite(enemyTexture.get(),enemySource(game.koopa.shell?77:38+static_cast<int>(animation*7)%2,game.koopa.shell?16:32),
      {static_cast<int>(game.koopa.x)-14,static_cast<int>(game.koopa.y)-(game.koopa.shell?28:48),28,game.koopa.shell?28:48},game.koopa.direction>0);
   for (const auto& b : game.boos) drawBoo(b);
   drawPipe(0,80,false); drawPipe(736,80,true); drawPipe(0,384,false); drawPipe(736,384,true);
   for (int i=0;i<2;++i) drawPlayer(game.players[i],i);
   for (const auto& e : game.defeatedEnemies) drawEnemy(e);
   for (const auto& p : particles) rect(static_cast<int>(p.x),static_cast<int>(p.y),4,4,p.color);
   SDL_RenderSetViewport(renderer,&originalViewport); drawHud();
   rect(0,464,800,16,ink);
   text("ESC PAUSE   TAB RULES   M "+std::string(muted?"UNMUTE":"MUTE")+"   F11 FULLSCREEN"+
        (game.rules.invinciblePlayers?"   TEST: BOTH INVINCIBLE":""),400,468,8,dim,true);
   if (noticeTimer>0) text(notice,400,83,10,gold,true);
   if (title) {
      drawPanel("SUPER MARIO BROS.",gold); text("BATTLE GAME",400,155,32,white,true);
      text("TWO PLAYERS. ONE SHARED ARENA.",400,205,12,dim,true);
      text("5 COINS TO WIN A ROUND / 5 ROUNDS TO A STAR",400,236,10,gold,true);
      rect(98,266,288,78,{39,33,48,255}); rect(414,266,288,78,{23,48,42,255});
      text("1  MARIO",118,279,12,red); text("2  LUIGI",434,279,12,green);
      text("F H MOVE / S JUMP / G DUCK",118,304,8,white); text("A TO DASH",118,323,8,dim);
      text("LEFT RIGHT MOVE / . JUMP",434,304,8,white); text("DOWN DUCK / , DASH",434,323,8,dim);
      if (std::fmod(animation,1.4f)<1.1f) text("PRESS ENTER TO BATTLE",400,366,16,green,true);
      text("TAB: RULES   TWO GAMEPADS ALSO SUPPORTED",400,401,10,dim,true);
   } else if (paused) {
      drawPanel("PAUSED",gold);
      text("ENTER / ESC   RESUME BATTLE",400,194,12,white,true); text("TAB           HOW TO PLAY",400,236,12,dim,true);
      text("R             RESTART MATCH",400,278,12,dim,true); text("Q             TITLE SCREEN",400,320,12,dim,true);
      text("MATCH STARS ARE SAVED AUTOMATICALLY",400,385,10,gold,true);
   } else if (game.phase==battle::Phase::Ready) {
      rect(215,176,370,112,ink); text("ROUND "+std::to_string(game.round),400,194,24,gold,true);
      text(game.readyTimer<.8f?"BATTLE!":"GET READY",400,239,16,white,true);
   } else if (game.phase==battle::Phase::RoundOver || game.phase==battle::Phase::MatchOver) {
      const bool match=game.phase==battle::Phase::MatchOver;
      drawPanel(std::string(name(game.winner))+(match?" WINS THE MATCH!":" WINS THE ROUND!"),game.winner==0?red:green);
      text(game.coinVictory?"FIVE COINS COLLECTED":"OPPONENT ELIMINATED",400,172,12,gold,true);
      text(std::to_string(game.wins[0])+"  -  "+std::to_string(game.wins[1]),400,221,32,white,true);
      text("MARIO               LUIGI",400,273,12,dim,true);
      text(match?"ONE MATCH STAR AWARDED":"FIRST TO FIVE ROUND WINS",400,320,12,gold,true);
      text(match?"ENTER: PLAY ANOTHER MATCH":"ENTER: NEXT ROUND",400,374,12,green,true);
      text("ESC: PAUSE / TITLE",400,402,8,dim,true);
   }
   if (help) drawHelp();
   SDL_RenderPresent(renderer);
}

#include "battle/BattleGame.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
using namespace battle;
#define CHECK(x) do { if (!(x)) { std::cerr << __func__ << ':' << __LINE__ << ": " #x " failed\n"; std::exit(1); } } while (0)
namespace {
constexpr float dt=1.f/120;
void tick(BattleGame& g, float seconds, std::array<Input,2> in={}) {
   for (int i=0;i<static_cast<int>(seconds/dt);++i) g.step(dt,in);
}
BattleGame quiet() {
   Rules r; r.enemySpawnSeconds=r.booSpawnSeconds=r.itemSpawnSeconds=10000;
   BattleGame g(42,r); g.phase=Phase::Playing; g.enemies.clear();
   return g;
}
Enemy& addKoopa(BattleGame& g, EnemyType type=EnemyType::WalkingKoopa) {
   Enemy e; e.type=type; e.winged=false;
   g.enemies.push_back(e);
   return g.enemies.back();
}
unsigned tileAt(const BattleGame& g,float x,float y) {
   for (unsigned i=0;i<g.segments.size();++i) if (g.segments[i].x==x && g.segments[i].y==y) return i;
   std::exit(2);
}
void matchRules() {
   BattleGame g(1);
   CHECK(g.players[0].super && g.players[1].super);
   tick(g,3); CHECK(g.phase==Phase::Playing);
   for (int r=1;r<=9;++r) {
      CHECK(g.round==r);
      CHECK(g.players[0].coins==0 && g.players[1].coins==0);
      CHECK(g.players[0].super && g.players[1].super);
      g.phase=Phase::Playing;
      const int winner=(r%2==1)?0:1;
      for (int c=0;c<5;++c) { Item coin; g.collectItem(coin,winner); }
      CHECK(g.winner==winner && g.coinVictory);
      CHECK(g.phase==(r==9?Phase::MatchOver:Phase::RoundOver));
      // Award exactly once and freeze after a winner is known.
      Item extra; g.collectItem(extra,winner); CHECK(extra.alive);
      tick(g,.2f);
      if (r<9) g.nextRound();
   }
   CHECK(g.wins[0]==5 && g.wins[1]==4 && g.stars[0]==1);
   g.nextRound(); CHECK(g.round==1 && g.wins[0]==0 && g.stars[0]==1);
   g.stars[1]=5;
   for (int r=0;r<5;++r) {
      g.phase=Phase::Playing;
      g.players[0].super=false; g.hurtPlayer(0);
      if (r<4) g.nextRound();
   }
   CHECK(g.phase==Phase::MatchOver && g.stars[1]==0 && !g.coinVictory);
   CHECK(g.round==5);
}
void rosters() {
   using E=EnemyType;
   const std::vector<std::vector<E>> expected={{E::Spiny},{E::Spiny,E::BuzzyBeetle},{E::BuzzyBeetle},{E::Spiny,E::FlyingKoopa},{E::FlyingKoopa},{E::FlyingKoopa},{E::Spiny,E::BuzzyBeetle,E::FlyingKoopa},{E::BuzzyBeetle,E::FlyingKoopa},{E::Spiny,E::BuzzyBeetle,E::FlyingKoopa}};
   for (int r=1;r<=9;++r) {
      CHECK(BattleGame::roster(r)==expected[r-1]);
      auto g=quiet(); g.round=r;
      int walkers=0;
      for (int i=0;i<10000;++i) {
         g.enemies.clear();
         g.spawnEnemy();
         const auto t=g.enemies.back().type;
         if (t==E::WalkingKoopa) { ++walkers; CHECK(!g.enemies.back().winged); }
         else { bool valid=false; for (auto e:expected[r-1]) valid|=e==t; CHECK(valid); }
      }
      CHECK(walkers>600 && walkers<800); // Approximately 7% in every round.
      g.rules.walkingKoopaChance=0;
      for (int i=0;i<100;++i) { g.enemies.clear(); g.spawnEnemy(); CHECK(g.enemies.back().type!=E::WalkingKoopa); }
   }
}
void recurringWalkingKoopas() {
   Rules r; r.enemySpawnSeconds=.05f; r.walkingKoopaChance=1;
   r.booSpawnSeconds=r.itemSpawnSeconds=10000;
   for (int round=1;round<=9;++round) {
      BattleGame g(round,r); g.round=round; g.phase=Phase::Playing;
      for (int frame=0;frame<120 && static_cast<int>(g.enemies.size())<r.enemyLimit;++frame) g.step(dt,{});
      CHECK(static_cast<int>(g.enemies.size())==r.enemyLimit);
      for (const auto& e:g.enemies) CHECK(e.type==EnemyType::WalkingKoopa && !e.winged && !e.shell && e.grounded && e.y==144);
      // Retire a moving shell at its second pipe entry, then replenish the slot.
      auto& shell=g.enemies.front(); shell.shell=true; shell.shellRecycles=1;
      shell.x=742; shell.y=floorY; shell.vx=r.shellSpeed; shell.direction=1;
      g.step(dt,{});
      CHECK(static_cast<int>(g.enemies.size())==r.enemyLimit-1 && g.items.empty());
      tick(g,.1f);
      CHECK(static_cast<int>(g.enemies.size())==r.enemyLimit);
      CHECK(g.enemies.back().type==EnemyType::WalkingKoopa && !g.enemies.back().winged);
   }
}
void enemyStates() {
   auto g=quiet();
   for (auto type:{EnemyType::Spiny,EnemyType::BuzzyBeetle}) {
      Enemy e; e.type=type; e.grounded=true;
      g.hitEnemy(e); CHECK(e.stunned && e.recovery==g.rules.stunSeconds);
      CHECK(e.shell==(type==EnemyType::BuzzyBeetle));
      e.grounded=true; g.hitEnemy(e); CHECK(!e.stunned && !e.shell && e.speedLevel==1);
   }
   Enemy flying; flying.type=EnemyType::FlyingKoopa; flying.grounded=false;
   g.hitEnemy(flying); CHECK(!flying.shell);
   g.hitEnemy(flying,true); CHECK(flying.shell && !flying.winged && !flying.stunned);
   CHECK(flying.recovery==g.rules.shellRecoverySeconds && flying.vx==0);
   Enemy e; e.x=240; e.y=144; e.stunned=true; e.recovery=.05f;
   g.enemies.push_back(e); tick(g,.2f);
   CHECK(!g.enemies[0].stunned && g.enemies[0].speedLevel==1);
}
void bumpAndKick() {
   auto g=quiet();
   Enemy e; e.x=176; e.y=344; e.grounded=true; g.enemies.push_back(e);
   auto index=tileAt(g,160,344); g.segments[index].hiddenItem=false;
   g.bumpSegment(index,0); CHECK(g.enemies[0].stunned);
   CHECK(g.players[0].coins==0 && g.items.empty());
   g.players[0].x=176; g.players[0].y=344;
   g.step(dt,{});
   CHECK(g.enemies.empty() && g.items.size()==1 && g.items[0].type==ItemType::Coin);
   CHECK(g.players[0].coins==0); // Finishing a foe is never automatic coin credit.
   g.players[1].x=g.items[0].x; g.players[1].y=g.items[0].y;
   g.step(dt,{});
   CHECK(g.players[1].coins==1 && g.players[0].coins==0 && g.items.empty());
   // A head strike bumps and stuns, but cannot carry the player through bricks.
   g=quiet(); g.players[0].x=176;
   e.x=176; e.y=344; e.grounded=true; g.enemies.push_back(e);
   std::array<Input,2> input{}; input[0].jump=true;
   bool bumped=false, kicked=false;
   for(int i=0;i<130;++i) {
      g.step(dt,input);
      for (const auto& event:g.events) { bumped|=event.type==EventType::Bump; kicked|=event.type==EventType::Kick; }
      CHECK(g.players[0].y-g.players[0].height()>=344+platformThickness-.01f);
   }
   CHECK(bumped && !kicked && g.enemies.size()==1 && g.enemies[0].stunned);
}
void solidPlatforms() {
   // Both sizes and both players must bonk from below, climb around every tier,
   // land from above, and be stopped by the vertical faces of thick bricks.
   for (int player=0;player<2;++player) for (bool super : {false,true}) {
      auto g=quiet();
      auto& p=g.players[player]; p.super=super; p.x=176;
      std::array<Input,2> in{}; in[player].jump=true;
      const float underside=344+platformThickness;
      bool bumped=false;
      for (int frame=0;frame<110;++frame) {
         g.step(dt,in);
         CHECK(p.y-p.height()>=underside-.01f);
         for (const auto& event:g.events) bumped|=event.type==EventType::Bump;
      }
      CHECK(bumped && p.grounded && p.y==floorY);

      const float starts[]={floorY,344,240}, targets[]={344,240,144}, xs[]={344,140,344};
      for (int tier=0;tier<3;++tier) {
         p.x=xs[tier]; p.y=starts[tier]; p.vx=p.vy=0; p.grounded=true; p.jumpHeld=false;
         in={}; in[player].jump=true;
         in[player].left=tier!=1; in[player].right=tier==1;
         tick(g,.8f,in);
         CHECK(p.grounded && p.y==targets[tier]);
      }
      p.x=140; p.y=260; p.vx=180; p.vy=0; p.grounded=false;
      in={}; in[player].right=true; tick(g,.1f,in);
      CHECK(p.x<=149.01f);
   }
   // The central block also stops the head while activating exactly once.
   auto g=quiet(); g.players[0].x=400;
   std::array<Input,2> in{}; in[0].jump=true; tick(g,.6f,in);
   CHECK(g.powCharges==2 && g.players[0].grounded && g.players[0].y==floorY);
   // A Koopa bounce must not snap the player's head inside a low brick ceiling.
   g=quiet(); auto& koopa=addKoopa(g); koopa.x=200; koopa.y=floorY; koopa.grounded=true;
   g.players[0].x=200; g.players[0].y=424; g.players[0].vy=100;
   g.step(dt,{});
   CHECK(koopa.shell && g.players[0].y-g.players[0].height()>=344+platformThickness);
   // Rival contact during a jump cannot push someone through a brick side.
   g=quiet(); g.players[0].x=331; g.players[1].x=352;
   for (auto& p:g.players) { p.y=370; p.vy=-100; p.grounded=false; }
   g.step(dt,{});
   CHECK(g.players[0].x>=331 && g.players[1].x>352);
}
void damageAndItems() {
   auto g=quiet(); g.hurtPlayer(0); CHECK(!g.players[0].super && g.phase==Phase::Playing);
   g.hurtPlayer(0); CHECK(g.phase==Phase::Playing); // Damage grace period.
   Item mushroom; mushroom.type=ItemType::Mushroom;
   g.collectItem(mushroom,0); CHECK(g.players[0].super);
   g.players[0].invulnerable=0; g.hurtPlayer(0); g.players[0].invulnerable=0;
   g.hurtPlayer(0); CHECK(g.winner==1 && g.players[0].eliminated);
   g=quiet(); g.players[0].x=100; g.players[1].x=600;
   Item swap; swap.type=ItemType::SwapMushroom; g.collectItem(swap,0);
   CHECK(g.players[0].x==600 && g.players[1].x==100);
   g.players[1].super=false; swap.alive=true; g.collectItem(swap,0);
   CHECK(!g.players[0].super && g.players[1].super);
   CHECK(g.players[0].x==600 && g.players[1].x==100);
}
void powAndReplenishment() {
   auto g=quiet(); g.rules.powRespawnMin=g.rules.powRespawnMax=.1f;
   Enemy e; e.type=EnemyType::FlyingKoopa; e.x=200; e.y=144; g.enemies.push_back(e);
   g.boos.push_back({}); g.activatePow(0);
   CHECK(g.powCharges==2 && g.boos.empty() && g.enemies[0].shell && !g.enemies[0].winged);
   g.activatePow(1); g.activatePow(0); CHECK(g.powCharges==0);
   tick(g,.2f); CHECK(g.powCharges==3);
   g=quiet(); g.rules.enemySpawnSeconds=.1f; tick(g,.2f);
   // First timer was initialized before the override; restart applies settings.
   g.startMatch(); g.phase=Phase::Playing; tick(g,1);
   CHECK(static_cast<int>(g.enemies.size())==g.rules.enemyLimit);
   g.enemies.erase(g.enemies.begin()); tick(g,.2f);
   CHECK(static_cast<int>(g.enemies.size())==g.rules.enemyLimit);
}
void shellsAndMovement() {
   auto g=quiet();
   auto& koopa=addKoopa(g); koopa.x=200; koopa.y=344; koopa.grounded=true;
   g.players[0].x=200; g.players[0].y=301; g.players[0].vy=220;
   tick(g,.08f); CHECK(koopa.shell && g.players[0].super);
   g=quiet(); auto& shell=addKoopa(g); shell.shell=true; shell.recovery=8; shell.x=214; shell.y=448;
   g.players[0].x=190; g.step(dt,{}); CHECK(shell.vx>100 && g.items.empty());
   Enemy e; e.x=shell.x+20; e.y=shell.y; e.grounded=true; g.enemies.push_back(e);
   g.players[0].x=500; tick(g,.05f);
   CHECK(g.enemies.size()==2 && g.enemies[1].vy<0 && g.items.empty());
   g=quiet(); g.players[0].x=799; std::array<Input,2> input{}; input[0].right=true; input[1].left=true;
   tick(g,.3f,input); CHECK(g.players[0].x<100 && g.players[1].x<615);
   auto shortJump=quiet(), longJump=quiet(); input={}; input[0].jump=true;
   // Compare jump heights in the gap, away from the solid POW block.
   shortJump.players[0].x=longJump.players[0].x=352;
   shortJump.step(dt,input); input[0].jump=false; tick(shortJump,.2f,input);
   input[0].jump=true; tick(longJump,.2f,input); CHECK(longJump.players[0].y<shortJump.players[0].y);
}
void stoppedShellRecovery() {
   // Walking and de-winged flying Koopas share the rest-only recovery rule.
   for (auto type : {EnemyType::WalkingKoopa,EnemyType::FlyingKoopa}) {
      auto g=quiet(); g.rules.shellRecoverySeconds=.2f;
      for (auto& p:g.players) p.invulnerable=1000;
      const auto check=[&](auto& shell) {
         shell.x=400; shell.y=240; shell.grounded=true;
         shell.shell=true; shell.vx=350; shell.recovery=.01f;
         tick(g,.6f); CHECK(shell.shell && shell.recovery>.19f);
         shell.x=400; shell.y=240; shell.vx=shell.vy=0; shell.grounded=true;
         tick(g,.1f); CHECK(shell.shell && shell.recovery<.12f);
         shell.vx=350; tick(g,.1f); CHECK(shell.shell && shell.recovery>.19f);
         shell.x=400; shell.y=240; shell.vx=shell.vy=0; shell.grounded=true;
         tick(g,.1f); CHECK(shell.shell);
         tick(g,.14f); CHECK(!shell.shell && shell.speedLevel==1);
         // Falling or bouncing at zero horizontal speed is still movement.
         shell.shell=true; shell.x=400; shell.y=210; shell.vx=0; shell.vy=-10;
         shell.grounded=false; shell.recovery=.01f;
         tick(g,.1f); CHECK(shell.shell && shell.recovery>.19f);
      };
      check(addKoopa(g,type));
   }
}
void flyingKoopas() {
   auto g=quiet();
   Enemy e; e.type=EnemyType::FlyingKoopa; e.x=400; e.y=240; e.grounded=true;
   e.hop=100; g.enemies.push_back(e);
   const auto stomp=[&] {
      auto& enemy=g.enemies[0];
      g.players[0].x=enemy.x; g.players[0].y=enemy.y-43;
      g.players[0].vx=0; g.players[0].vy=220; g.players[0].grounded=false;
      enemy.touchGrace=0; tick(g,.08f);
   };
   stomp(); CHECK(!g.enemies[0].winged && !g.enemies[0].shell && g.players[0].super);
   stomp(); CHECK(g.enemies[0].shell && g.enemies[0].vx==0 && g.players[0].super);
   // A grounded shell can be kicked as a weapon, without awarding a coin.
   auto& enemy=g.enemies[0]; enemy.touchGrace=0;
   g.players[0].x=enemy.x-24; g.players[0].y=240; g.players[0].vy=0;
   g.players[0].grounded=true; g.step(dt,{});
   CHECK(enemy.shell && enemy.vx>100 && g.items.empty());
   // Stomping a moving shell kicks it again instead of stopping it.
   enemy.x=400; enemy.y=240; enemy.vx=350; enemy.vy=0; enemy.grounded=true; enemy.touchGrace=0;
   g.players[0].x=410; g.players[0].y=208; g.players[0].vy=220;
   g.players[0].vx=0; g.players[0].grounded=false;
   tick(g,.04f); CHECK(enemy.shell && std::abs(enemy.vx)==g.rules.shellSpeed && g.players[0].super);
   // POW also turns flying Koopas into kickable shells.
   g.hitEnemy(enemy,true); CHECK(enemy.shell && !enemy.stunned && !enemy.winged && enemy.vx==0);
   enemy.touchGrace=0; g.players[0].x=enemy.x-30; g.players[0].y=enemy.y; g.players[0].vy=0;
   g.step(dt,{}); CHECK(g.enemies.size()==1 && g.items.empty() && enemy.vx>0);
}
void buzzyBeetles() {
   auto g=quiet();
   Enemy e; e.type=EnemyType::BuzzyBeetle; e.x=400; e.y=240; e.grounded=true;
   g.enemies.push_back(e);
   const auto stomp=[&] {
      auto& beetle=g.enemies[0]; beetle.touchGrace=0;
      g.players[0].x=beetle.x; g.players[0].y=beetle.y-34;
      g.players[0].vx=0; g.players[0].vy=220; g.players[0].grounded=false;
      tick(g,.05f);
   };
   stomp(); CHECK(g.enemies[0].alive && !g.enemies[0].stunned && !g.enemies[0].shell);
   CHECK(g.players[0].super && g.enemies[0].speedLevel==0 && g.items.empty());
   g.enemies[0].x=400; g.enemies[0].y=240; g.enemies[0].grounded=true;
   g.bumpSegment(tileAt(g,384,240),0);
   CHECK(g.enemies[0].stunned && g.enemies[0].shell && g.enemies[0].vx==0);
   g.enemies[0].vy=0; g.enemies[0].grounded=true;
   stomp(); CHECK(!g.enemies[0].stunned && !g.enemies[0].shell && g.players[0].super);
   CHECK(g.items.empty());
   g.hitEnemy(g.enemies[0],true);
   g.players[0].x=g.enemies[0].x-24; g.players[0].y=g.enemies[0].y;
   g.players[0].vy=0; g.step(dt,{});
   CHECK(g.enemies.empty() && g.items.size()==1 && g.items[0].type==ItemType::Coin);
   CHECK(g.players[0].coins==0); // Kicking the exposed shell generates a shared coin.
}
void powHitsAllKoopas() {
   auto g=quiet();
   for (auto type : {EnemyType::WalkingKoopa,EnemyType::FlyingKoopa}) for (int state=0;state<3;++state) {
      Enemy e; e.type=type; e.x=200+state*100; e.y=144;
      e.winged=type==EnemyType::FlyingKoopa && state==0; e.shell=state==2; e.vx=state==2?350:0;
      e.shellRecycles=state==2?1:0; g.enemies.push_back(e);
   }
   g.activatePow(0);
   for(const auto& e:g.enemies) CHECK(e.shell && !e.winged && !e.stunned && e.vx==0 && e.vy<0);
   CHECK(g.enemies[2].shellRecycles==1 && g.enemies[5].shellRecycles==1 && g.items.empty());
   g.enemies[0].shell=false; g.activatePow(1);
   CHECK(g.enemies[0].shell && g.enemies[0].shellRecycles==0);
}
void sensitiveShellKicks() {
   for (auto type : {EnemyType::WalkingKoopa,EnemyType::FlyingKoopa}) {
      auto g=quiet();
      const auto check=[&](auto& shell) {
         shell.shell=true; shell.x=400; shell.y=240; shell.vx=shell.vy=0;
         shell.grounded=true; shell.recovery=9;
         g.players[0].x=368; g.players[0].y=240;
         g.players[1].x=426; g.players[1].y=240;
         g.step(dt,{}); CHECK(shell.vx==g.rules.shellSpeed);
         CHECK(shell.lastPlayer==0 && shell.touchGrace<=g.rules.shellKickGrace);
         g.step(dt,{}); CHECK(!g.players[1].super); // The rival gets no kick grace.
         shell.x=400; shell.y=240; shell.vx=shell.vy=0; shell.grounded=true;
         shell.touchGrace=0; shell.recovery=9;
         g.players[0].x=400; g.players[0].y=206; g.players[0].vy=220;
         g.players[0].vx=0; g.players[0].grounded=false;
         g.players[1].x=600; tick(g,.05f);
         CHECK(shell.shell && std::abs(shell.vx)==g.rules.shellSpeed && g.players[0].super);
      };
      check(addKoopa(g,type));
   }
}
void shellPipeLimit() {
   for (auto type : {EnemyType::WalkingKoopa,EnemyType::FlyingKoopa}) {
      auto g=quiet();
      addKoopa(g,type);
      const auto enter=[&](auto& shell) {
         shell.shell=true; shell.x=742; shell.y=floorY; shell.vx=350; shell.vy=0;
         shell.direction=1; shell.grounded=true; shell.recovery=9;
         shell.touchGrace=0; g.step(dt,{});
      };
      enter(g.enemies[0]);
      CHECK(g.enemies.size()==1 && g.enemies[0].shellRecycles==1 && g.enemies[0].y==144);
      // Kicking again doesn't restart the shell's pipe lifetime.
      const auto rekick=[&](auto& shell) {
         shell.x=400; shell.y=240; shell.vx=shell.vy=0; shell.grounded=true;
         g.players[0].x=368; g.players[0].y=240;
         g.step(dt,{}); CHECK(shell.shellRecycles==1 && shell.vx>0);
      };
      rekick(g.enemies[0]); enter(g.enemies[0]);
      CHECK(g.enemies.empty() && g.items.size()==(type==EnemyType::FlyingKoopa?1u:0u) && g.players[0].coins==0);
      g.activatePow(0); CHECK(g.enemies.empty());
      g.startMatch(); CHECK(g.enemies.empty());
   }
}
void defeatedEnemyFlight() {
   for (auto type : {EnemyType::Spiny,EnemyType::BuzzyBeetle}) {
      for (int player=0;player<2;++player) for (int direction : {-1,1}) {
         auto g=quiet();
         Enemy enemy; enemy.type=type; enemy.x=400; enemy.y=240;
         enemy.stunned=true; enemy.shell=type==EnemyType::BuzzyBeetle;
         enemy.grounded=true; enemy.recovery=7;
         g.enemies.push_back(enemy);
         auto& p=g.players[player]; p.x=400-direction*24; p.y=240;
         p.facing=-direction; // Kick position takes precedence over facing.
         g.step(dt,{});
         CHECK(g.enemies.empty() && g.defeatedEnemies.size()==1 && g.items.size()==1);
         const auto& falling=g.defeatedEnemies[0];
         CHECK(!falling.alive && !falling.grounded && falling.type==type);
         CHECK(falling.x==400 && falling.y==240 && falling.vy<0);
         CHECK(falling.vx==direction*g.rules.defeatedDriftSpeed);
         bool belowFloor=false;
         for (int frame=0;frame<180 && !g.defeatedEnemies.empty();++frame) {
            g.step(dt,{});
            if (!g.defeatedEnemies.empty()) {
               const auto& body=g.defeatedEnemies[0];
               CHECK((body.x-400)*direction>0 && !body.alive);
               belowFloor|=body.y>floorY;
            }
         }
         CHECK(belowFloor && g.defeatedEnemies.empty());
         CHECK(g.items.size()==1 && g.players[0].coins==0 && g.players[1].coins==0);
      }
   }
   // Defeated bodies drift past the screen edge instead of wrapping back in.
   auto g=quiet();
   Enemy enemy; enemy.x=4; enemy.y=144; enemy.stunned=true; enemy.recovery=7;
   enemy.grounded=true; g.enemies.push_back(enemy);
   g.players[0].x=28; g.players[0].y=144; g.step(dt,{}); tick(g,.1f);
   CHECK(g.defeatedEnemies.size()==1 && g.defeatedEnemies[0].x<0);
   g.startMatch(); CHECK(g.defeatedEnemies.empty());
}
void invinciblePlayersTestMode() {
   for (int player=0;player<2;++player) {
      auto normal=quiet(); normal.hurtPlayer(player); CHECK(!normal.players[player].super);
      normal.players[player].invulnerable=0; normal.hurtPlayer(player);
      CHECK(normal.players[player].eliminated && normal.winner==1-player);
      auto g=quiet(); g.rules.invinciblePlayers=true;
      auto& p=g.players[player]; g.hurtPlayer(player);
      CHECK(!p.super && !p.eliminated && p.invulnerable==g.rules.invulnerabilitySeconds && p.vy<0);
      p.invulnerable=0; const float vy=p.vy;
      for (int hit=0;hit<10;++hit) g.hurtPlayer(player);
      CHECK(!p.super && !p.eliminated && p.invulnerable==0 && p.vy==vy);
      CHECK(g.events.size()==1 && g.events[0].type==EventType::Hurt);
      Item mushroom; mushroom.type=ItemType::Mushroom; g.collectItem(mushroom,player);
      CHECK(p.super); g.hurtPlayer(player); CHECK(!p.super && !p.eliminated);

      // Contact with every active enemy, both moving shell types, and a Boo
      // shrinks a Super player, but cannot damage or knock back a Small player.
      for (bool super : {false,true}) for (int hazard=0;hazard<7;++hazard) {
         auto contact=quiet(); contact.rules.invinciblePlayers=true;
         auto& target=contact.players[player]; target.super=super; target.x=352;
         if (hazard<6) {
            const EnemyType types[]={EnemyType::Spiny,EnemyType::BuzzyBeetle,EnemyType::FlyingKoopa,EnemyType::WalkingKoopa,EnemyType::FlyingKoopa,EnemyType::WalkingKoopa};
            Enemy enemy; enemy.type=types[hazard]; enemy.x=target.x-4; enemy.y=floorY;
            enemy.grounded=true; enemy.hop=1000;
            if (hazard>=4) { enemy.shell=true; enemy.vx=350; enemy.recovery=9; }
            contact.enemies.push_back(enemy);
         } else {
            Boo boo; boo.x=target.x; boo.y=436; boo.diagonal=true;
            contact.boos.push_back(boo);
         }
         contact.step(dt,{});
         CHECK(!target.super && !target.eliminated && contact.phase==Phase::Playing);
         CHECK(super ? target.invulnerable>0 && target.vy<0 : target.invulnerable==0 && target.vy==0 && target.y==floorY);
         int hurts=0; for (const auto& e:contact.events) hurts+=e.type==EventType::Hurt;
         CHECK(hurts==(super?1:0));
      }
      for (int coin=0;coin<5;++coin) { Item item; g.collectItem(item,0); }
      CHECK(g.phase==Phase::RoundOver && g.winner==0);
      g.nextRound(); g.phase=Phase::Playing;
      for (int i=0;i<2;++i) { g.hurtPlayer(i); CHECK(!g.players[i].super && !g.players[i].eliminated); }
      for (int coin=0;coin<5;++coin) { Item item; g.collectItem(item,1); }
      CHECK(g.phase==Phase::RoundOver && g.winner==1);
      g.startMatch(); g.phase=Phase::Playing;
      for (int i=0;i<2;++i) { g.hurtPlayer(i); CHECK(!g.players[i].super && !g.players[i].eliminated); }
      CHECK(g.rules.invinciblePlayers);
   }
}
void playerStompsAndStuns() {
   for (bool testMode : {false,true}) for (bool super : {false,true})
   for (bool damageGrace : {false,true}) for (int attacker=0;attacker<2;++attacker) {
      auto g=quiet(); g.rules.invinciblePlayers=testMode;
      const int victim=1-attacker;
      auto& target=g.players[victim]; auto& stomper=g.players[attacker];
      target.x=352; target.y=floorY; target.super=super;
      target.invulnerable=damageGrace?9:0;
      stomper.x=352; stomper.y=target.y-target.height()-8;
      stomper.vy=220; stomper.grounded=false;
      for (int frame=0;frame<30 && target.stunTimer==0;++frame) g.step(dt,{});
      CHECK(target.stunTimer>=.49f);
      CHECK(target.crouched==super && target.height()==28 && target.super==super);
      CHECK(!target.eliminated && stomper.vy<0);
      stomper.x=100; stomper.y=floorY; stomper.vx=stomper.vy=0; stomper.grounded=true;
      std::array<Input,2> trying{};
      trying[victim].right=trying[victim].jump=trying[victim].dash=true;
      tick(g,.4f,trying);
      CHECK(target.x==352 && target.vx==0 && target.y==floorY && target.crouched==super);
      CHECK(target.stunTimer>0);
      tick(g,.15f);
      CHECK(target.stunTimer==0 && !target.crouched);
      g.step(dt,trying); CHECK(target.vx>0 && target.vy<0);
      g.startMatch(); CHECK(g.players[victim].stunTimer==0 && !g.players[victim].crouched);
   }
   // Side contact doesn't count as a stomp.
   auto g=quiet(); g.players[0].x=352; g.players[1].x=374;
   g.step(dt,{}); CHECK(g.players[0].stunTimer==0 && g.players[1].stunTimer==0);
}
void platformPlayerStuns() {
   for (bool testMode : {false,true}) for (bool super : {false,true}) for (int attacker=0;attacker<2;++attacker) {
      auto g=quiet(); g.rules.invinciblePlayers=testMode;
      const int victim=1-attacker;
      auto& target=g.players[victim]; auto& bumper=g.players[attacker];
      target.x=176; target.y=344; target.super=super; target.invulnerable=9;
      bumper.x=176; std::array<Input,2> in{}; in[attacker].jump=true;
      for (int frame=0;frame<60 && target.stunTimer==0;++frame) g.step(dt,in);
      CHECK(target.stunTimer>=.99f && target.vy<0);
      CHECK(target.super==super && target.crouched==super && !target.eliminated);
      bumper.x=700; bumper.y=floorY; bumper.vx=bumper.vy=0; bumper.grounded=true;
      in={}; in[victim].right=in[victim].jump=in[victim].dash=true;
      tick(g,.8f,in);
      CHECK(target.x==176 && target.vx==0 && target.y==344 && target.stunTimer>.15f);
      tick(g,.25f); CHECK(target.stunTimer==0);
      g.step(dt,in); CHECK(target.vx>0 && target.vy<0);
   }
   // A later stomp cannot shorten an existing platform stun or its crouch pose.
   auto g=quiet(); g.players[1].x=352; g.players[1].y=240;
   g.bumpSegment(tileAt(g,352,240),0);
   g.players[1].y=240; g.players[1].vy=0; g.players[1].grounded=true;
   g.players[0].x=352; g.players[0].y=240-g.players[1].height()-8;
   g.players[0].vy=220; g.players[0].grounded=false;
   bool stomped=false;
   for (int frame=0;frame<30 && !stomped;++frame) {
      g.step(dt,{});
      for (const auto& e:g.events) stomped|=e.type==EventType::Stun && e.player==1;
   }
   CHECK(stomped && g.players[1].stunTimer>.8f && g.players[1].crouched);
   g.players[0].x=100; g.players[0].y=floorY; g.players[0].vx=g.players[0].vy=0;
   tick(g,.6f);
   CHECK(g.players[1].stunTimer>0 && g.players[1].crouched);
}
void powStunsPlayers() {
   for (bool testMode : {false,true}) for (bool super : {false,true})
   for (bool damageGrace : {false,true}) for (int attacker=0;attacker<2;++attacker) {
      auto g=quiet(); g.rules.invinciblePlayers=testMode;
      auto& target=g.players[1-attacker]; target.super=super;
      target.invulnerable=damageGrace?9:0; target.vx=120;
      const float x=target.x;
      g.activatePow(attacker);
      CHECK(g.powCharges==2 && target.stunTimer==g.rules.powStunSeconds);
      CHECK(target.super==super && !target.eliminated && target.crouched==super && target.vx==0);
      CHECK(g.players[attacker].stunTimer==0);
      std::array<Input,2> in{}; in[1-attacker].right=in[1-attacker].jump=in[1-attacker].dash=true;
      tick(g,.8f,in);
      CHECK(target.stunTimer>.15f && target.x==x && target.y==floorY && target.vx==0);
      tick(g,.25f); CHECK(target.stunTimer==0 && !target.crouched);
      g.step(dt,in); CHECK(target.vx>0 && target.vy<0);
      // An exhausted POW cannot stun anyone.
      g.powCharges=0; target.stunTimer=0; g.activatePow(attacker);
      CHECK(target.stunTimer==0 && g.players[attacker].stunTimer==0);
   }
}
void emptyRoundsAndPipeSpawns() {
   Rules rules; rules.readySeconds=.05f; rules.enemySpawnSeconds=.1f;
   rules.booSpawnSeconds=.3f; rules.itemSpawnSeconds=10000;
   for (int round=1;round<=9;++round) {
      BattleGame g(round,rules); g.round=round;
      CHECK(g.enemies.empty() && g.boos.empty());
      tick(g,.08f); tick(g,.05f);
      CHECK(g.phase==Phase::Playing && g.enemies.empty() && g.boos.empty());
      for (int frame=0;frame<30 && g.enemies.empty();++frame) g.step(dt,{});
      CHECK(g.enemies.size()==1 && g.enemies[0].y==144);
      CHECK((g.enemies[0].x==64 && g.enemies[0].direction==1) ||
            (g.enemies[0].x==736 && g.enemies[0].direction==-1));
      for (int frame=0;frame<30 && g.enemies.size()<2;++frame) g.step(dt,{});
      CHECK(g.enemies.size()==2 && g.enemies[1].y==144 && (g.enemies[1].x==64 || g.enemies[1].x==736));
      CHECK(g.boos.empty());
      for (int frame=0;frame<30 && g.boos.empty();++frame) g.step(dt,{});
      CHECK(g.boos.size()==1 && g.boos[0].x==64 && g.boos[0].y==144);
      g.phase=Phase::RoundOver; g.nextRound();
      CHECK(g.enemies.empty() && g.boos.empty());
      g.startMatch(); CHECK(g.enemies.empty() && g.boos.empty());
   }
}
void tiesAndStress() {
   auto g=quiet(); g.players[0].coins=4; g.players[1].coins=4;
   g.players[1].x=g.players[0].x;
   Item coin; coin.x=g.players[0].x; coin.y=448; g.items.push_back(coin);
   g.step(dt,{}); CHECK(g.winner==0 && g.wins[0]==1 && g.wins[1]==0);
   // Exercise cycling, recovery, spawns, wrapping and all nine rosters over many
   // fixed steps, with sanitizers checking reference/vector lifetime hazards.
   std::mt19937 random(123);
   for (int round=1;round<=9;++round) {
      BattleGame s(round); s.round=round; s.phase=Phase::Playing;
      for (int frame=0;frame<18000;++frame) {
         std::array<Input,2> in{};
         for (auto& p:in) { auto n=random(); p.left=n&1; p.right=n&2; p.jump=n&4; p.dash=n&8; p.down=n&16; }
         for (auto& p:s.players) p.invulnerable=2;
         s.step(dt,in);
         for (const auto& p:s.players) {
            CHECK(std::isfinite(p.x) && std::isfinite(p.y) && p.x>=0 && p.x<800 && p.y<=448);
            for (const auto& tile:s.segments) for (float offset : {-width,0.f,width})
               CHECK(!(p.x+11>tile.x+offset+.01f && p.x-11<tile.x+offset+32-.01f &&
                       p.y>tile.y+.01f && p.y-p.height()<tile.y+platformThickness-.01f));
         }
         CHECK(static_cast<int>(s.enemies.size())<=s.rules.enemyLimit);
      }
   }
}
}
int main() {
   matchRules(); rosters(); recurringWalkingKoopas(); enemyStates(); bumpAndKick(); solidPlatforms(); damageAndItems();
   powAndReplenishment(); shellsAndMovement(); stoppedShellRecovery(); flyingKoopas(); buzzyBeetles();
   powHitsAllKoopas(); sensitiveShellKicks(); shellPipeLimit(); defeatedEnemyFlight();
   invinciblePlayersTestMode(); playerStompsAndStuns(); platformPlayerStuns(); powStunsPlayers();
   emptyRoundsAndPipeSpawns(); tiesAndStress();
   std::cout << "Battle rules: all checks passed (including 162,000 simulation steps).\n";
}

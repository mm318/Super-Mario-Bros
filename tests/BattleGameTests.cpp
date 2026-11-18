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
   g.koopa.x=750; g.koopa.y=144;
   return g;
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
   const std::vector<std::vector<E>> expected={{E::Spiny},{E::Spiny,E::Crab},{E::Crab},{E::Spiny,E::Fly},{E::Fly},{E::Fly},{E::Spiny,E::Crab,E::Fly},{E::Crab,E::Fly},{E::Spiny,E::Crab,E::Fly}};
   for (int r=1;r<=9;++r) {
      CHECK(BattleGame::roster(r)==expected[r-1]);
      auto g=quiet(); g.round=r;
      for (int i=0;i<25;++i) {
         g.spawnEnemy();
         const auto t=g.enemies.back().type;
         bool valid=false; for (auto e:expected[r-1]) valid|=e==t; CHECK(valid);
      }
   }
}
void enemyStates() {
   auto g=quiet();
   for (auto type:{EnemyType::Spiny,EnemyType::Fly,EnemyType::Crab}) {
      Enemy e; e.type=type; e.grounded=true;
      g.hitEnemy(e);
      if (type==EnemyType::Crab) { CHECK(e.angry && !e.stunned); e.grounded=true; g.hitEnemy(e); }
      CHECK(e.stunned && e.recovery==g.rules.stunSeconds);
      e.grounded=true; g.hitEnemy(e); CHECK(!e.stunned && e.speedLevel==1);
   }
   Enemy fly; fly.type=EnemyType::Fly; fly.grounded=false; g.hitEnemy(fly); CHECK(!fly.stunned);
   g.hitEnemy(fly,true); CHECK(fly.stunned);
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
   // The core combat is reachable with ordinary controls, including climbing.
   g=quiet(); g.players[0].x=176;
   e.x=176; e.y=344; e.grounded=true; g.enemies.push_back(e);
   std::array<Input,2> input{}; input[0].jump=true;
   bool bumped=false, climbed=false, kicked=false;
   for(int i=0;i<130;++i) {
      g.step(dt,input);
      for (const auto& event:g.events) { bumped|=event.type==EventType::Bump; kicked|=event.type==EventType::Kick; }
      climbed|=g.players[0].y<344;
   }
   CHECK(bumped && climbed && kicked);
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
   Enemy e; e.type=EnemyType::Crab; e.x=200; e.y=144; g.enemies.push_back(e);
   g.boos.push_back({}); g.activatePow(0);
   CHECK(g.powCharges==2 && g.boos.empty() && g.enemies[0].stunned);
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
   g.koopa.x=200; g.koopa.y=344; g.koopa.grounded=true;
   g.players[0].x=200; g.players[0].y=301; g.players[0].vy=220;
   tick(g,.08f); CHECK(g.koopa.shell && g.players[0].super);
   g=quiet(); g.koopa.shell=true; g.koopa.recovery=8; g.koopa.x=214; g.koopa.y=448;
   g.players[0].x=190; g.step(dt,{}); CHECK(g.koopa.vx>100 && g.items.empty());
   Enemy e; e.x=g.koopa.x+20; e.y=g.koopa.y; e.grounded=true; g.enemies.push_back(e);
   g.players[0].x=500; tick(g,.05f);
   CHECK(g.enemies.size()==1 && g.enemies[0].vy<0 && g.items.empty());
   g=quiet(); g.players[0].x=799; std::array<Input,2> input{}; input[0].right=true; input[1].left=true;
   tick(g,.3f,input); CHECK(g.players[0].x<100 && g.players[1].x<615);
   auto shortJump=quiet(), longJump=quiet(); input={}; input[0].jump=true;
   shortJump.step(dt,input); input[0].jump=false; tick(shortJump,.2f,input);
   input[0].jump=true; tick(longJump,.2f,input); CHECK(longJump.players[0].y<shortJump.players[0].y);
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
         for (const auto& p:s.players) CHECK(std::isfinite(p.x) && std::isfinite(p.y) && p.x>=0 && p.x<800 && p.y<=448);
         CHECK(static_cast<int>(s.enemies.size())<=s.rules.enemyLimit);
      }
   }
}
}
int main() {
   matchRules(); rosters(); enemyStates(); bumpAndKick(); damageAndItems();
   powAndReplenishment(); shellsAndMovement(); tiesAndStress();
   std::cout << "Battle rules: all checks passed (including 162,000 simulation steps).\n";
}

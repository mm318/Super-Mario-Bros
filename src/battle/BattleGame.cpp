#include "battle/BattleGame.h"

#include <algorithm>
#include <cmath>

namespace battle {
namespace {
bool overlap(const Body& a, float aw, float ah, const Body& b, float bw, float bh) {
   float dx = std::abs(a.x - b.x);
   dx = std::min(dx, std::abs(width - dx));
   return dx < (aw + bw) * .5f && a.y > b.y - bh && a.y - ah < b.y;
}
float approach(float value, float target, float amount) {
   return value < target ? std::min(value + amount, target) : std::max(value - amount, target);
}
void wrap(Body& body) {
   if (body.x < 0) body.x += width;
   if (body.x >= width) body.x -= width;
}
} // namespace

BattleGame::BattleGame(uint32_t seed, Rules settings) : rules(settings), random(seed) { startMatch(); }
float BattleGame::range(float low, float high) {
   return std::uniform_real_distribution<float>(low, high)(random);
}
std::vector<EnemyType> BattleGame::roster(int number) {
   using E = EnemyType;
   switch (number) {
      case 1: return {E::Spiny};
      case 2: return {E::Spiny, E::Crab};
      case 3: return {E::Crab};
      case 4: return {E::Spiny, E::Fly};
      case 5: case 6: return {E::Fly};
      case 8: return {E::Crab, E::Fly};
      default: return {E::Spiny, E::Crab, E::Fly};
   }
}
void BattleGame::startMatch() {
   wins = {};
   round = 1;
   resetRound();
}
void BattleGame::nextRound() {
   if (phase == Phase::MatchOver) startMatch();
   else if (phase == Phase::RoundOver) { round = wins[0] + wins[1] + 1; resetRound(); }
}
void BattleGame::resetRound() {
   players = {};
   players[0].x = 185; players[1].x = 615;
   for (auto& p : players) { p.y = floorY; p.grounded = true; }
   players[1].facing = -1;
   segments.clear(); enemies.clear(); items.clear(); boos.clear(); events.clear();
   // Three interleaved tiers. All tiles are passable upward and solid downward.
   for (int tile = 0; tile < 25; ++tile) {
      if (tile < 10 || tile >= 15) segments.push_back({tile * 32.f, 144});
      if (tile >= 5 && tile < 20) segments.push_back({tile * 32.f, 240});
      if (tile < 10 || tile >= 15) segments.push_back({tile * 32.f, 344});
   }
   segments[static_cast<unsigned>(range(0, static_cast<float>(segments.size())))].hiddenItem = true;
   koopa = {}; koopa.x = 400; koopa.y = 240; koopa.grounded = true;
   powCharges = 3; powTimer = 0;
   phase = Phase::Ready; winner = -1; coinVictory = false;
   readyTimer = rules.readySeconds; shake = 0;
   enemyTimer = rules.enemySpawnSeconds; booTimer = rules.booSpawnSeconds;
   itemTimer = rules.itemSpawnSeconds; nextPipeRight = false;
   nextBooDiagonal = false;
   spawnEnemy();
}
void BattleGame::emit(EventType type, float x, float y, int player) {
   events.push_back({type, x, y, player});
}
void BattleGame::spawnEnemy() {
   const auto choices = roster(round);
   Enemy e;
   e.type = choices[std::uniform_int_distribution<unsigned>(0, choices.size() - 1)(random)];
   e.x = nextPipeRight ? 704 : 96; e.y = 144; e.grounded = true;
   e.direction = nextPipeRight ? -1 : 1;
   nextPipeRight = !nextPipeRight;
   e.hop = range(.5f, 1.4f);
   enemies.push_back(e);
}
void BattleGame::spawnItem(ItemType type) {
   Item item;
   item.type = type;
   item.x = nextPipeRight ? 704 : 96; item.y = 144; item.grounded = true;
   item.direction = nextPipeRight ? -1 : 1;
   nextPipeRight = !nextPipeRight;
   items.push_back(item);
}
void BattleGame::moveBody(Body& b, float halfWidth, float dt) {
   const float previousY = b.y;
   b.x += b.vx * dt; wrap(b);
   b.vy = std::min(b.vy + rules.gravity * dt, 700.f);
   b.y += b.vy * dt;
   b.grounded = false;
   float landing = floorY;
   if (b.vy >= 0) {
      for (const auto& s : segments) {
         if (previousY <= s.y + .1f && b.y >= s.y && b.x + halfWidth > s.x && b.x - halfWidth < s.x + 32)
            landing = std::min(landing, s.y);
      }
      if (b.y >= landing) { b.y = landing; b.vy = 0; b.grounded = true; }
   }
}
void BattleGame::updatePlayer(int index, const Input& in, float dt) {
   auto& p = players[index];
   p.invulnerable = std::max(0.f, p.invulnerable - dt);
   p.crouched = in.down && p.super && p.grounded;
   const int direction = static_cast<int>(in.right) - static_cast<int>(in.left);
   const float target = (p.crouched ? 0 : direction) * (in.dash ? rules.dashSpeed : rules.walkSpeed);
   p.vx = approach(p.vx, target, (direction ? rules.acceleration : rules.friction) * dt);
   if (direction) p.facing = direction;
   if (in.jump && !p.jumpHeld && p.grounded) {
      p.vy = -rules.jumpSpeed; p.grounded = false; p.crouched = false;
      emit(EventType::Jump, p.x, p.y, index);
   }
   p.jumpHeld = in.jump;
   if (in.jump && p.vy < 0) p.vy -= rules.gravity * (1 - rules.heldJumpGravity) * dt;
   const float oldTop = p.y - p.height();
   const float oldFeet = p.y;
   moveBody(p, 11, dt);
   // A segment reacts once as the head crosses its underside. Continuing through
   // lets players land on it, as required by the supplied one-way platform rule.
   if (p.vy < 0) {
      for (unsigned i = 0; i < segments.size(); ++i) {
         const auto& s = segments[i];
         if (oldTop >= s.y + 16 && p.y - p.height() < s.y + 16 && p.x >= s.x && p.x < s.x + 32)
            bumpSegment(i, index);
      }
      if (powCharges && oldTop >= 376 && p.y - p.height() < 376 && std::abs(p.x - 400) < 31)
         activatePow(index);
   }
   // POW is a solid small block, so players can also stand on it.
   if (powCharges && p.vy >= 0 && oldFeet <= 344 && p.y >= 344 && std::abs(p.x - 400) < 28) {
      p.y = 344; p.vy = 0; p.grounded = true;
   }
   if (p.y - p.height() < 72) { p.y = 72 + p.height(); p.vy = std::max(0.f, p.vy); }
}
void BattleGame::hitEnemy(Enemy& e, bool pow) {
   if (!e.alive || (!pow && !e.grounded)) return;
   if (e.stunned && !pow) {
      e.stunned = false; e.angry = false; ++e.speedLevel;
      e.vy = -150; e.grounded = false;
   } else if (!pow && e.type == EnemyType::Crab && !e.angry) {
      e.angry = true; e.vy = -120; e.grounded = false;
   } else {
      e.stunned = true; e.recovery = rules.stunSeconds;
      e.vx = 0; e.vy = -160; e.grounded = false;
      emit(EventType::Stun, e.x, e.y);
   }
}
void BattleGame::bumpSegment(unsigned index, int player) {
   if (index >= segments.size()) return;
   auto& s = segments[index];
   if (s.bump > .10f) return;
   s.bump = .22f;
   emit(EventType::Bump, s.x + 16, s.y, player);
   for (auto& e : enemies)
      if (e.grounded && std::abs(e.y - s.y) < 2 && e.x + 13 > s.x && e.x - 13 < s.x + 32)
         hitEnemy(e);
   if (koopa.grounded && std::abs(koopa.y - s.y) < 2 && std::abs(koopa.x - s.x - 16) < 28) {
      koopa.shell = true; koopa.vx = 0; koopa.recovery = rules.shellRecoverySeconds;
      koopa.vy = -160; koopa.grounded = false;
   }
   const int other = 1 - player;
   if (players[other].grounded && std::abs(players[other].y - s.y) < 2 && std::abs(players[other].x - s.x - 16) < 27) {
      players[other].vy = -260; players[other].grounded = false;
   }
   if (s.hiddenItem) {
      s.hiddenItem = false;
      Item item; item.type = range(0, 1) < .5f ? ItemType::Mushroom : ItemType::SwapMushroom;
      item.x = s.x + 16; item.y = s.y - 3; item.vy = -180;
      item.direction = players[player].facing; items.push_back(item);
      emit(EventType::Mushroom, item.x, item.y);
   }
}
void BattleGame::activatePow(int player) {
   if (powCharges <= 0) return;
   --powCharges;
   for (auto& e : enemies) hitEnemy(e, true);
   boos.clear(); shake = .4f;
   emit(EventType::Pow, 400, 360, player);
   if (!powCharges) powTimer = range(rules.powRespawnMin, rules.powRespawnMax);
}
void BattleGame::updateEnemies(float dt) {
   for (auto& e : enemies) {
      if (!e.alive) continue;
      e.shellCooldown = std::max(0.f, e.shellCooldown - dt);
      if (e.stunned) {
         e.vx = 0; e.recovery -= dt;
         if (e.recovery <= 0) { e.stunned = false; e.angry = false; ++e.speedLevel; }
      }
      if (!e.stunned) {
         const float speed = e.type == EnemyType::Crab ? rules.crabSpeed : e.type == EnemyType::Fly ? rules.flySpeed : rules.spinySpeed;
         e.vx = e.direction * speed *
                (1 + rules.recoverySpeedGain * std::min(e.speedLevel, 6) + (e.angry ? rules.crabAngryBonus : 0));
         if (e.type == EnemyType::Fly && e.grounded) {
            e.hop -= dt;
            if (e.hop <= 0) { e.vy = -rules.flyHopSpeed; e.grounded = false; e.hop = range(rules.flyHopMin, rules.flyHopMax); }
         }
      }
      moveBody(e, 11, dt);
      if (e.grounded && e.y == floorY && (e.x < 58 || e.x > 742)) {
         e.x = e.direction > 0 ? 96 : 704; e.y = 144; e.vy = 0; e.grounded = true;
      }
   }
   koopa.touchGrace = std::max(0.f, koopa.touchGrace - dt);
   if (koopa.shell) {
      koopa.recovery -= dt;
      if (koopa.recovery <= 0) { koopa.shell = false; ++koopa.speedLevel; }
   }
   if (!koopa.shell) koopa.vx = koopa.direction * (rules.koopaSpeed + rules.koopaRecoverySpeedGain * std::min(koopa.speedLevel, 6));
   moveBody(koopa, 12, dt);
   if (koopa.grounded && koopa.y == floorY && (koopa.x < 58 || koopa.x > 742)) {
      koopa.x = koopa.direction > 0 ? 96 : 704; koopa.y = 144; koopa.vy = 0;
   }
   if (koopa.shell && std::abs(koopa.vx) > 100) {
      for (auto& e : enemies) {
         if (e.alive && e.shellCooldown == 0 && overlap(koopa, 28, 26, e, 28, 28)) {
            // A shell tosses ordinary enemies; it neither kills nor pays coins.
            e.vy = -300; e.grounded = false; e.shellCooldown = .5f;
         }
      }
   }
}
void BattleGame::updateItems(float dt) {
   for (auto& item : items) {
      if (!item.alive) continue;
      item.vx = item.direction * (item.type == ItemType::Coin ? rules.coinSpeed : rules.mushroomSpeed);
      moveBody(item, 9, dt);
      // Unclaimed coins keep cycling; they have no owner and never expire.
      if (item.grounded && item.y == floorY && (item.x < 58 || item.x > 742)) {
         item.x = item.direction > 0 ? 96 : 704; item.y = 144; item.vy = 0;
      }
   }
}
void BattleGame::updateBoos(float dt) {
   for (auto& boo : boos) {
      boo.age += dt; boo.x += boo.vx * dt;
      if (boo.diagonal) {
         wrap(boo); boo.y += boo.vy * dt;
         if (boo.y < 120) { boo.y = 120; boo.vy = std::abs(boo.vy); }
         if (boo.y > floorY - 12) { boo.y = floorY - 12; boo.vy = -std::abs(boo.vy); }
      } else boo.y = boo.baseY + std::sin(boo.age * rules.booWaveFrequency) * rules.booWaveAmplitude;
   }
   boos.erase(std::remove_if(boos.begin(), boos.end(), [this](const Boo& b) {
      return (!b.diagonal && b.x > width + 32) || b.age > rules.booLifetime;
   }), boos.end());
}
void BattleGame::hurtPlayer(int index) {
   if (phase != Phase::Playing) return;
   auto& p = players[index];
   if (p.invulnerable > 0 || p.eliminated) return;
   if (p.super) {
      p.super = false; p.crouched = false; p.invulnerable = rules.invulnerabilitySeconds;
      p.vy = -190; p.grounded = false;
      emit(EventType::Hurt, p.x, p.y, index);
   } else {
      p.eliminated = true; emit(EventType::Death, p.x, p.y, index);
      winRound(1 - index, false);
   }
}
void BattleGame::collectItem(Item& item, int index) {
   if (!item.alive || phase != Phase::Playing) return;
   auto& p = players[index]; item.alive = false;
   if (item.type == ItemType::Coin) {
      ++p.coins; emit(EventType::Coin, p.x, p.y - 32, index);
      if (p.coins >= Rules::coinsToWin) winRound(index, true);
   } else if (item.type == ItemType::Mushroom) {
      p.super = true; emit(EventType::Mushroom, p.x, p.y, index);
   } else {
      auto& a = players[0]; auto& b = players[1];
      if (a.super == b.super) {
         // Swap complete spatial state so grounded flags/velocity stay coherent.
         std::swap(static_cast<Body&>(a), static_cast<Body&>(b));
      } else { std::swap(a.super, b.super); a.crouched = b.crouched = false; }
      emit(EventType::Swap, p.x, p.y, index);
   }
}
void BattleGame::winRound(int index, bool coins) {
   if (phase != Phase::Playing) return;
   winner = index; coinVictory = coins; ++wins[index];
   phase = Phase::RoundOver;
   emit(EventType::RoundWin, players[index].x, players[index].y, index);
   if (wins[index] >= Rules::roundsToWin) {
      phase = Phase::MatchOver; stars[index] = (stars[index] + 1) % 6;
      emit(EventType::MatchWin, players[index].x, players[index].y, index);
   }
}
void BattleGame::collisions(float dt) {
   // Ties use a documented deterministic rule: coins before damage, Mario first
   // for simultaneous collection. Stop as soon as a round has been awarded.
   for (auto& item : items) {
      for (int i = 0; i < 2 && phase == Phase::Playing; ++i)
         if (item.alive && overlap(players[i], 24, players[i].height(), item, 24, 26)) collectItem(item, i);
   }
   for (int i = 0; i < 2 && phase == Phase::Playing; ++i) {
      auto& p = players[i];
      for (auto& e : enemies) {
         if (!e.alive || !overlap(p, 22, p.height(), e, 27, 27)) continue;
         if (e.stunned) {
            e.alive = false; spawnItem(ItemType::Coin); emit(EventType::Kick, e.x, e.y, i);
         } else hurtPlayer(i);
         if (phase != Phase::Playing) break;
      }
      if (phase != Phase::Playing) break;
      if (koopa.touchGrace == 0 && overlap(p, 24, p.height(), koopa, 28, koopa.shell ? 26 : 40)) {
         const float koopaTop = koopa.y - (koopa.shell ? 26 : 32);
         const bool stomp = p.vy > 0 && p.y - p.vy * dt <= koopaTop + 10;
         if (stomp) {
            koopa.shell = true; koopa.vx = 0; koopa.recovery = rules.shellRecoverySeconds;
            koopa.touchGrace = .22f; p.vy = -320; p.y = koopaTop; p.grounded = false;
            emit(EventType::Stun, koopa.x, koopa.y, i);
         } else if (koopa.shell && std::abs(koopa.vx) < 100) {
            koopa.direction = p.x < koopa.x ? 1 : -1; koopa.vx = koopa.direction * rules.shellSpeed;
            koopa.recovery = rules.shellRecoverySeconds; koopa.touchGrace = .25f;
            emit(EventType::Kick, koopa.x, koopa.y, i);
         } else hurtPlayer(i);
      }
      for (const auto& boo : boos)
         if (overlap(p, 20, p.height(), boo, 26, 28)) hurtPlayer(i);
   }
   if (phase != Phase::Playing) return;
   auto& a = players[0]; auto& b = players[1];
   if (overlap(a, 24, a.height(), b, 24, b.height())) {
      float dx = b.x - a.x;
      if (dx > width / 2) dx -= width;
      if (dx < -width / 2) dx += width;
      const float push = (dx >= 0 ? 1.f : -1.f) * (24 - std::abs(dx)) * .5f;
      a.x -= push; b.x += push; wrap(a); wrap(b);
      const float av = a.vx; a.vx = b.vx * .65f; b.vx = av * .65f;
   }
}
void BattleGame::step(float dt, const std::array<Input, 2>& inputs) {
   if (dt <= 0) return;
   events.clear();
   if (phase == Phase::Ready) {
      readyTimer -= dt;
      if (readyTimer <= 0) phase = Phase::Playing;
      return;
   }
   if (phase != Phase::Playing) return;
   shake = std::max(0.f, shake - dt);
   for (auto& s : segments) s.bump = std::max(0.f, s.bump - dt);
   for (int i = 0; i < 2; ++i) updatePlayer(i, inputs[i], dt);
   updateEnemies(dt); updateItems(dt); updateBoos(dt); collisions(dt);
   enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [](const Enemy& e) { return !e.alive; }), enemies.end());
   items.erase(std::remove_if(items.begin(), items.end(), [](const Item& i) { return !i.alive; }), items.end());
   if (phase != Phase::Playing) return;
   enemyTimer -= dt; booTimer -= dt; itemTimer -= dt;
   if (enemyTimer <= 0) {
      if (static_cast<int>(enemies.size()) < rules.enemyLimit) spawnEnemy();
      enemyTimer = rules.enemySpawnSeconds;
   }
   if (booTimer <= 0) {
      Boo boo; boo.x = 0; boo.baseY = range(180, 365); boo.y = boo.baseY;
      boo.diagonal = nextBooDiagonal; nextBooDiagonal = !nextBooDiagonal;
      boo.vx = boo.diagonal ? rules.booDiagonalSpeed : rules.booWaveSpeed;
      boo.vy = rules.booVerticalSpeed; boos.push_back(boo);
      booTimer = rules.booSpawnSeconds;
   }
   if (itemTimer <= 0) {
      if (std::count_if(items.begin(), items.end(), [](const Item& i) { return i.type != ItemType::Coin; }) < 3)
         spawnItem(range(0, 1) < rules.mushroomChance ? ItemType::Mushroom : ItemType::SwapMushroom);
      itemTimer = rules.itemSpawnSeconds;
   }
   if (!powCharges) {
      powTimer -= dt;
      if (powTimer <= 0) powCharges = 3;
   }
}
} // namespace battle

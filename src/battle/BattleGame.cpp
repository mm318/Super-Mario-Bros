#include "battle/BattleGame.h"

#include <algorithm>
#include <cmath>

namespace battle {
namespace {
  bool overlap(const Body & a, float aw, float ah, const Body & b, float bw, float bh) {
    float dx = std::abs(a.x - b.x);
    dx       = std::min(dx, std::abs(width - dx));

    return dx < (aw + bw) * .5f && a.y > b.y - bh && a.y - ah < b.y;
  }

  float approach(float value, float target, float amount) {
    return value < target ? std::min(value + amount, target) : std::max(value - amount, target);
  }

  void wrap(Body & body) {
    if (body.x < 0) {
      body.x += width;
    }

    if (body.x >= width) {
      body.x -= width;
    }
  }

  bool shellRecovered(const Body & shell, float & remaining, float dt, const Rules & rules) {
    // Recovery requires one uninterrupted period at rest, including vertical rest.
    if (!shell.grounded || std::abs(shell.vx) > .01f || std::abs(shell.vy) > .01f) {
      remaining = rules.shellRecoverySeconds;
    } else {
      remaining -= dt;
    }

    return remaining <= 0;
  }

  float enemySpeed(const Enemy & enemy, const Rules & rules) {
    const float normal = enemy.type == EnemyType::WalkingKoopa ? rules.koopaSpeed
                       : enemy.type == EnemyType::FlyingKoopa  ? rules.flyingKoopaSpeed
                       : enemy.type == EnemyType::BuzzyBeetle  ? rules.buzzyBeetleSpeed
                                                               : rules.spinySpeed;

    return normal * (enemy.angry ? rules.angrySpeedMultiplier : 1.f);
  }

  void reviveEnemy(Enemy & enemy, const Rules & rules) {
    enemy.stunned = enemy.shell = false;
    enemy.angry                 = true;

    if (enemy.isKoopa()) {
      enemy.winged = false;
    }

    enemy.recovery = enemy.touchGrace = 0;
    enemy.lastPlayer                  = -1;
    enemy.vx                          = enemy.direction * enemySpeed(enemy, rules);
  }

  void tuckKoopa(Enemy & koopa, const Rules & rules) {
    if (!koopa.shell) {
      koopa.shellRecycles = 0;
    }

    koopa.shell      = true;
    koopa.vx         = 0;
    koopa.vy         = -160;
    koopa.grounded   = false;
    koopa.recovery   = rules.shellRecoverySeconds;
    koopa.touchGrace = 0;
    koopa.lastPlayer = -1;
  }

  int kickDirection(const Body & body, const Player & player) {
    float dx = body.x - player.x;
    if (dx > width / 2) {
      dx -= width;
    }
    if (dx < -width / 2) {
      dx += width;
    }

    return std::abs(dx) < 6 ? player.facing : (dx > 0 ? 1 : -1);
  }

  void kickKoopa(Enemy & koopa, const Player & player, int index, const Rules & rules) {
    koopa.direction  = kickDirection(koopa, player);
    koopa.vx         = koopa.direction * rules.shellSpeed * (koopa.angry ? rules.angrySpeedMultiplier : 1.f);
    koopa.recovery   = rules.shellRecoverySeconds;
    koopa.touchGrace = rules.shellKickGrace;
    koopa.lastPlayer = index;
  }

  template<class T> void emergeFromPipe(T & body, bool right) {
    body.x         = right ? 736 : 64;
    body.y         = topPlatformY;
    body.direction = right ? -1 : 1;
    body.vy        = 0;
    body.grounded  = true;
  }
} // namespace

BattleGame::BattleGame(uint32_t seed, Rules settings) : rules(settings), random(seed) {
  startMatch();
}

float BattleGame::range(float low, float high) {
  return std::uniform_real_distribution<float>(low, high)(random);
}

ItemType BattleGame::randomMushroom() {
  if (range(0, 1) < rules.mushroomChance) {
    return ItemType::Mushroom;
  }

  return range(0, 1) < .5f ? ItemType::SizeSwapMushroom : ItemType::PositionSwapMushroom;
}

std::vector<EnemyType> BattleGame::roster(int number) {
  using E = EnemyType;

  switch (number) {
    case 1:
      return {E::BuzzyBeetle};
    case 2:
      return {E::BuzzyBeetle, E::Spiny};
    case 3:
      return {E::Spiny};
    case 4:
      return {E::BuzzyBeetle, E::FlyingKoopa};
    case 5:
    case 6:
      return {E::FlyingKoopa};
    case 8:
      return {E::Spiny, E::FlyingKoopa};
    default:
      return {E::BuzzyBeetle, E::Spiny, E::FlyingKoopa};
  }
}

void BattleGame::startMatch() {
  wins  = {};
  round = 1;
  resetRound();
}

void BattleGame::nextRound() {
  if (phase == Phase::MatchOver) {
    startMatch();
  } else if (phase == Phase::RoundOver) {
    round = wins[0] + wins[1] + 1;
    resetRound();
  }
}

void BattleGame::resetRound() {
  players      = {};
  players[0].x = 185;
  players[1].x = 615;
  for (auto & p : players) {
    p.y        = floorY;
    p.grounded = true;
  }
  players[1].facing = -1;

  segments.clear();
  enemies.clear();
  defeatedEnemies.clear();
  items.clear();
  boos.clear();
  events.clear();

  // Three interleaved tiers spread across the taller play area.
  for (int tile = 0; tile < 25; ++tile) {
    if (tile < 10 || tile >= 15) {
      segments.push_back({tile * 32.f, topPlatformY});
    }
    if (tile >= 5 && tile < 20) {
      segments.push_back({tile * 32.f, middlePlatformY});
    }
    if (tile < 10 || tile >= 15) {
      segments.push_back({tile * 32.f, lowerPlatformY});
    }
  }

  segments[static_cast<unsigned>(range(0, static_cast<float>(segments.size())))].hiddenItem = true;

  powCharges = 3;
  powTimer   = 0;

  phase       = Phase::Ready;
  winner      = -1;
  coinVictory = false;
  readyTimer  = rules.readySeconds;
  shake       = 0;

  enemyTimer = rules.enemySpawnSeconds;
  booTimer   = rules.booSpawnSeconds;
  itemTimer  = rules.itemSpawnSeconds;
  coinTimer  = rules.coinSpawnSeconds;

  nextPipeRight   = false;
  nextBooDiagonal = false;
}

void BattleGame::emit(EventType type, float x, float y, int player) {
  events.push_back({type, x, y, player});
}

void BattleGame::spawnEnemy() {
  const auto choices = roster(round);
  Enemy      e;
  e.type   = std::bernoulli_distribution(rules.walkingKoopaChance)(random)
             ? EnemyType::WalkingKoopa
             : choices[std::uniform_int_distribution<unsigned>(0, choices.size() - 1)(random)];
  e.winged = e.type == EnemyType::FlyingKoopa;

  emergeFromPipe(e, nextPipeRight);
  nextPipeRight = !nextPipeRight;
  e.hop         = range(.5f, 1.4f);
  enemies.push_back(e);
}

void BattleGame::spawnItem(ItemType type) {
  Item item;
  item.type = type;
  emergeFromPipe(item, nextPipeRight);
  nextPipeRight = !nextPipeRight;
  items.push_back(item);
}

void BattleGame::moveBody(Body & b, float halfWidth, float dt) {
  const float previousY = b.y;
  b.x += b.vx * dt;
  wrap(b);

  b.vy = std::min(b.vy + rules.gravity * dt, 700.f);
  b.y += b.vy * dt;
  b.grounded = false;

  float landing = floorY;
  if (b.vy >= 0) {
    for (const auto & s : segments) {
      if (previousY <= s.y + .1f && b.y >= s.y && b.x + halfWidth > s.x && b.x - halfWidth < s.x + 32) {
        landing = std::min(landing, s.y);
      }
    }

    if (b.y >= landing) {
      b.y        = landing;
      b.vy       = 0;
      b.grounded = true;
    }
  }
}

void BattleGame::updatePlayer(int index, const Input & in, float dt) {
  auto & p       = players[index];
  p.invulnerable = std::max(0.f, p.invulnerable - dt);

  const bool stunned = p.stunTimer > 0;
  p.crouched         = p.super && (stunned || (p.grounded && in.down));

  const int   direction = stunned ? 0 : static_cast<int>(in.right) - static_cast<int>(in.left);
  const float target    = (p.crouched ? 0 : direction) * (in.dash ? rules.dashSpeed : rules.walkSpeed);
  p.vx                  = approach(p.vx, target, (direction ? rules.acceleration : rules.friction) * dt);
  if (direction) {
    p.facing = direction;
  }

  if (!stunned && in.jump && !p.jumpHeld && p.grounded) {
    p.vy       = -rules.jumpSpeed;
    p.grounded = false;
    p.jumping  = true;
    p.crouched = false;
    emit(EventType::Jump, p.x, p.y, index);
  }
  p.jumpHeld = in.jump;
  if (!stunned && in.jump && p.vy < 0) {
    p.vy -= rules.gravity * (1 - rules.heldJumpGravity) * dt;
  }

  constexpr float halfWidth = 11;
  const float     height    = p.height();
  const float     oldX      = p.x;
  float           nextX     = p.x + p.vx * dt;

  // Resolve sides before vertical movement. Adjacent copies make edge tiles
  // solid even when a player straddles the horizontal wrap boundary.
  const auto blockSide = [&](float x, float y, float w) {
    if (p.y <= y || p.y - height >= y + platformThickness) {
      return;
    }
    for (float offset : {-width, 0.f, width}) {
      const float left = x + offset, right = left + w;
      if (p.vx > 0 && oldX + halfWidth <= left + .01f && nextX + halfWidth > left) {
        nextX = left - halfWidth;
        p.vx  = 0;
      } else if (p.vx < 0 && oldX - halfWidth >= right - .01f && nextX - halfWidth < right) {
        nextX = right + halfWidth;
        p.vx  = 0;
      }
    }
  };

  for (const auto & s : segments) {
    blockSide(s.x, s.y, 32);
  }
  if (powCharges) {
    blockSide(380, lowerPlatformY, 40);
  }

  p.x = nextX;
  wrap(p);

  const float oldFeet = p.y, oldTop = p.y - height;
  p.vy = std::min(p.vy + rules.gravity * dt, 700.f);
  p.y += p.vy * dt;
  p.grounded = false;

  const auto overlapsX = [&](float x, float w) {
    for (float offset : {-width, 0.f, width}) {
      if (p.x + halfWidth > x + offset && p.x - halfWidth < x + offset + w) {
        return true;
      }
    }
    return false;
  };

  if (p.vy < 0) {
    collidePlayerCeiling(index, oldTop);
  } else {
    float landing = floorY;
    for (const auto & s : segments) {
      if (oldFeet <= s.y + .01f && p.y >= s.y && overlapsX(s.x, 32)) {
        landing = std::min(landing, s.y);
      }
    }

    if (powCharges && oldFeet <= lowerPlatformY + .01f && p.y >= lowerPlatformY && overlapsX(380, 40)) {
      landing = std::min(landing, lowerPlatformY);
    }

    if (p.y >= landing) {
      p.y        = landing;
      p.vy       = 0;
      p.grounded = true;
      p.jumping  = false;
    }
  }
}

void BattleGame::hitEnemy(Enemy & e, bool pow) {
  if (!e.alive || (!pow && !e.grounded)) {
    return;
  }

  if (!pow && (e.stunned || e.shell)) {
    reviveEnemy(e, rules);
    e.vy       = -150;
    e.grounded = false;
    return;
  }

  if (e.isKoopa()) {
    e.winged = e.stunned = false;
    tuckKoopa(e, rules);
    emit(EventType::Stun, e.x, e.y);
    return;
  }

  e.stunned    = true;
  e.shell      = e.type == EnemyType::BuzzyBeetle;
  e.recovery   = rules.stunSeconds;
  e.touchGrace = 0;
  e.vx         = 0;
  e.vy         = -160;
  e.grounded   = false;
  emit(EventType::Stun, e.x, e.y);
}

void BattleGame::collidePlayerCeiling(int index, float oldTop) {
  auto &     p         = players[index];
  const auto overlapsX = [&](float x, float w) {
    for (float offset : {-width, 0.f, width}) {
      if (p.x + 11 > x + offset && p.x - 11 < x + offset + w) {
        return true;
      }
    }
    return false;
  };

  float ceiling = 0;
  int   bumped  = -1;
  bool  hitPow  = false;
  for (unsigned i = 0; i < segments.size(); ++i) {
    const auto & s      = segments[i];
    const float  bottom = s.y + platformThickness;
    if (oldTop >= bottom - .01f && p.y - p.height() < bottom && overlapsX(s.x, 32)) {
      if (
        bottom > ceiling
        || (bottom == ceiling && (bumped < 0 || std::abs(p.x - s.x - 16) < std::abs(p.x - segments[bumped].x - 16)))) {
        ceiling = bottom;
        bumped  = static_cast<int>(i);
      }
    }
  }

  const float powBottom = lowerPlatformY + platformThickness;
  if (powCharges && oldTop >= powBottom - .01f && p.y - p.height() < powBottom && overlapsX(380, 40)
      && powBottom > ceiling) {
    ceiling = powBottom;
    hitPow  = true;
  }

  if ((hitPow || bumped >= 0) && p.y - p.height() < ceiling) {
    p.y  = ceiling + p.height();
    p.vy = 0;
    if (hitPow) {
      activatePow(index);
    } else if (bumped >= 0) {
      bumpSegment(static_cast<unsigned>(bumped), index);
    }
  }
}

void BattleGame::bouncePlayer(int index, float top) {
  auto &      p      = players[index];
  const float oldTop = p.y - p.height();
  p.vy               = -320;
  p.y                = top;
  p.grounded         = false;
  collidePlayerCeiling(index, oldTop);
}

void BattleGame::bumpSegment(unsigned index, int player) {
  if (index >= segments.size()) {
    return;
  }

  auto & s = segments[index];
  if (s.bump > .10f) {
    return;
  }

  s.bump = .22f;
  emit(EventType::Bump, s.x + 16, s.y, player);

  const auto above = [&](const Body & body, float halfWidth) {
    float dx = std::abs(body.x - (s.x + 16));
    dx       = std::min(dx, width - dx);
    return body.grounded && std::abs(body.y - s.y) < 2 && dx < 16 + halfWidth;
  };

  for (auto & e : enemies) {
    if (above(e, 13)) {
      hitEnemy(e);
    }
  }

  for (auto & item : items) {
    if (item.alive && above(item, item.type == ItemType::Coin ? 12 : 14)) {
      item.vy       = -rules.itemBumpSpeed;
      item.grounded = false;
    }
  }

  const int other = 1 - player;
  if (above(players[other], 11)) {
    stunPlayer(other, rules.platformStunSeconds);
    players[other].vy       = -260;
    players[other].grounded = false;
  }

  if (s.hiddenItem && rules.spawning) {
    s.hiddenItem = false;
    Item item;
    item.type      = randomMushroom();
    item.x         = s.x + 16;
    item.y         = s.y - 3;
    item.vy        = -180;
    item.direction = players[player].facing;
    items.push_back(item);
    emit(EventType::Mushroom, item.x, item.y);
  }
}

void BattleGame::activatePow(int player) {
  if (powCharges <= 0) {
    return;
  }

  --powCharges;
  for (auto & e : enemies) {
    hitEnemy(e, true);
  }

  stunPlayer(1 - player, rules.powStunSeconds);
  boos.clear();
  shake = .4f;
  emit(EventType::Pow, 400, 360, player);

  if (!powCharges) {
    powTimer = range(rules.powRespawnMin, rules.powRespawnMax);
  }
}

void BattleGame::updateEnemies(float dt) {
  for (auto & e : enemies) {
    if (!e.alive) {
      continue;
    }

    e.shellCooldown = std::max(0.f, e.shellCooldown - dt);
    e.touchGrace    = std::max(0.f, e.touchGrace - dt);

    if (e.stunned) {
      e.vx = 0;
      if (!e.isKoopa()) {
        e.recovery -= dt;
        if (e.recovery <= 0) {
          reviveEnemy(e, rules);
        }
      }
    }

    if (!e.stunned && !e.shell) {
      e.vx = e.direction * enemySpeed(e, rules);
      if (e.type == EnemyType::FlyingKoopa && e.winged && e.grounded) {
        e.hop -= dt;
        if (e.hop <= 0) {
          e.vy       = -rules.flyingKoopaHopSpeed;
          e.grounded = false;
          e.hop      = range(rules.flyingKoopaHopMin, rules.flyingKoopaHopMax);
        }
      }
    }

    moveBody(e, e.type == EnemyType::WalkingKoopa ? 12 : 11, dt);
    if (e.isKoopa() && (e.stunned || e.shell) && shellRecovered(e, e.recovery, dt, rules)) {
      reviveEnemy(e, rules);
    }

    if (e.grounded && e.y == floorY && (e.x < 58 || e.x > 742)) {
      if (e.isKoopa() && e.shell && std::abs(e.vx) > .01f) {
        if (e.shellRecycles >= 1) {
          e.alive = false;
          continue;
        }
        ++e.shellRecycles;
      }
      emergeFromPipe(e, e.direction < 0);
    }
  }

  const auto tossEnemies = [&](const Body & shell, const Enemy * source) {
    for (auto & e : enemies) {
      if (&e != source && e.alive && e.shellCooldown == 0 && overlap(shell, 28, 26, e, 28, e.height())) {
        // A shell tosses ordinary enemies; it neither kills nor pays coins.
        e.vy            = -300;
        e.grounded      = false;
        e.shellCooldown = .5f;
      }
    }
  };

  for (const auto & e : enemies) {
    if (e.alive && e.isKoopa() && e.shell && std::abs(e.vx) > .01f) {
      tossEnemies(e, &e);
    }
  }
}

void BattleGame::updateItems(float dt) {
  for (auto & item : items) {
    if (!item.alive) {
      continue;
    }

    item.vx = item.direction * (item.type == ItemType::Coin ? rules.coinSpeed : rules.mushroomSpeed);
    moveBody(item, 9, dt);

    // Unclaimed coins keep cycling; they have no owner and never expire.
    if (item.grounded && item.y == floorY && (item.x < 58 || item.x > 742)) {
      emergeFromPipe(item, item.direction < 0);
    }
  }
}

void BattleGame::defeatEnemy(Enemy & enemy, int player) {
  enemy.alive       = false;
  Enemy falling     = enemy;
  falling.direction = kickDirection(enemy, players[player]);
  falling.vx        = falling.direction * rules.defeatedDriftSpeed;
  falling.vy        = -rules.defeatedPopSpeed;
  falling.grounded  = false;
  defeatedEnemies.push_back(falling);
  emit(EventType::Kick, enemy.x, enemy.y, player);
}

void BattleGame::updateDefeatedEnemies(float dt) {
  // Cosmetic bodies no longer collide, recover, wrap, or generate rewards.
  for (auto & enemy : defeatedEnemies) {
    enemy.vy += rules.gravity * dt;
    enemy.x += enemy.vx * dt;
    enemy.y += enemy.vy * dt;
  }

  defeatedEnemies.erase(std::remove_if(defeatedEnemies.begin(),
                                       defeatedEnemies.end(),
                                       [](const Enemy & enemy) {
                                         return enemy.y - enemy.height() > screenHeight;
                                       }),
                        defeatedEnemies.end());
}

void BattleGame::updateEliminatedPlayers(float dt) {
  for (auto & p : players) {
    if (p.eliminated && p.y - p.height() <= screenHeight) {
      p.vy += rules.gravity * dt;
      p.y += p.vy * dt;
    }
  }
}

bool BattleGame::dying() const {
  return std::any_of(players.begin(), players.end(), [](const Player & p) {
    return p.eliminated && p.y - p.height() <= screenHeight;
  });
}

void BattleGame::updateBoos(float dt) {
  for (auto & boo : boos) {
    boo.age += dt;
    boo.x += boo.vx * dt;

    if (boo.diagonal) {
      wrap(boo);
      boo.y += boo.vy * dt;
      if (boo.y < hudHeight + 48) {
        boo.y  = hudHeight + 48;
        boo.vy = std::abs(boo.vy);
      }
      if (boo.y > floorY - 12) {
        boo.y  = floorY - 12;
        boo.vy = -std::abs(boo.vy);
      }
    } else {
      const float entrance = std::min(boo.age / 2, 1.f);
      boo.y
        = topPlatformY
        + (boo.baseY - topPlatformY + std::sin(boo.age * rules.booWaveFrequency) * rules.booWaveAmplitude) * entrance;
    }
  }

  boos.erase(std::remove_if(boos.begin(),
                            boos.end(),
                            [this](const Boo & b) {
                              return (!b.diagonal && b.x > width + 32) || b.age > rules.booLifetime;
                            }),
             boos.end());
}

void BattleGame::hurtPlayer(int index) {
  if (phase != Phase::Playing) {
    return;
  }

  auto & p = players[index];
  if (p.invulnerable > 0 || p.eliminated || (rules.invinciblePlayers && !p.super)) {
    return;
  }

  if (p.super) {
    p.super = p.crouched = false;
    p.invulnerable       = rules.invulnerabilitySeconds;
    p.vy                 = -190;
    p.grounded           = false;
    emit(EventType::Hurt, p.x, p.y, index);
  } else {
    // Out of the round: hop, then drop through the arena and off the screen.
    p.eliminated   = true;
    p.grounded     = false;
    p.invulnerable = 0;
    p.stunTimer    = 0;
    p.vx           = 0;
    p.vy           = -rules.deathPopSpeed;
    emit(EventType::Death, p.x, p.y, index);
    winRound(1 - index, false);
  }
}

void BattleGame::fitPlayer(Player & p) {
  p.crouched = p.super && (p.crouched || p.stunTimer > 0);

  const auto separate = [&](float x, float y, float w) {
    for (float offset : {-width, 0.f, width}) {
      if (p.x + 11 > x + offset && p.x - 11 < x + offset + w && p.y > y && p.y - p.height() < y + platformThickness) {
        // Growing or swapping into a low ceiling must not embed a player
        // in a solid platform or trigger a head-bump attack.
        p.y        = y + platformThickness + p.height();
        p.vy       = std::max(0.f, p.vy);
        p.grounded = false;
      }
    }
  };

  for (const auto & s : segments) {
    separate(s.x, s.y, 32);
  }
  if (powCharges) {
    separate(380, lowerPlatformY, 40);
  }
}

void BattleGame::collectItem(Item & item, int index) {
  if (!item.alive || phase != Phase::Playing) {
    return;
  }

  auto & p   = players[index];
  item.alive = false;

  if (item.type == ItemType::Coin) {
    ++p.coins;
    emit(EventType::Coin, p.x, p.y - 32, index);
    if (p.coins >= Rules::coinsToWin) {
      winRound(index, true);
    }
  } else if (item.type == ItemType::Mushroom) {
    p.super = true;
    fitPlayer(p);
    emit(EventType::Mushroom, p.x, p.y, index);
  } else {
    auto & a = players[0];
    auto & b = players[1];

    if (item.type == ItemType::PositionSwapMushroom) {
      // Swap complete spatial state so grounded flags/velocity stay coherent.
      std::swap(static_cast<Body &>(a), static_cast<Body &>(b));
    } else {
      std::swap(a.super, b.super);
    }

    fitPlayer(a);
    fitPlayer(b);
    emit(item.type == ItemType::PositionSwapMushroom ? EventType::PositionSwap : EventType::SizeSwap, p.x, p.y, index);
  }
}

void BattleGame::winRound(int index, bool coins) {
  if (phase != Phase::Playing) {
    return;
  }

  winner      = index;
  coinVictory = coins;
  ++wins[index];
  phase = Phase::RoundOver;
  emit(EventType::RoundWin, players[index].x, players[index].y, index);

  if (wins[index] >= Rules::roundsToWin) {
    phase = Phase::MatchOver;
    emit(EventType::MatchWin, players[index].x, players[index].y, index);
  }
}

void BattleGame::collisions(float dt, const std::array<Player, 2> & previous) {
  // Ties use a documented deterministic rule: coins before damage, Mario first
  // for simultaneous collection. Stop as soon as a round has been awarded.
  for (auto & item : items) {
    for (int i = 0; i < 2 && phase == Phase::Playing; ++i) {
      if (item.alive && overlap(players[i], 24, players[i].height(), item, 24, 26)) {
        collectItem(item, i);
      }
    }
  }

  if (phase != Phase::Playing) {
    return;
  }

  collidePlayers(previous);

  for (int i = 0; i < 2 && phase == Phase::Playing; ++i) {
    auto & p = players[i];
    for (auto & e : enemies) {
      if (!e.alive || (e.touchGrace > 0 && e.lastPlayer == i)) {
        continue;
      }

      const bool koopaShell   = e.isKoopa() && e.shell;
      const bool stoppedShell = koopaShell && std::abs(e.vx) <= .01f;
      const bool walkingKoopa = e.type == EnemyType::WalkingKoopa;
      if (!overlap(p,
                   stoppedShell   ? 30
                   : walkingKoopa ? 24
                                  : 22,
                   p.height(),
                   e,
                   stoppedShell   ? 36
                   : walkingKoopa ? 28
                                  : 27,
                   e.height())) {
        continue;
      }

      const float top   = e.y - (e.isKoopa() && !e.shell ? 32 : 27);
      const bool  stomp = p.vy > 0 && p.y - p.vy * dt <= top + 10;
      if (e.type == EnemyType::BuzzyBeetle && stomp) {
        if (e.stunned) {
          reviveEnemy(e, rules);
        }
        e.touchGrace = rules.shellKickGrace;
        e.lastPlayer = i;
        bouncePlayer(i, top);
        emit(EventType::Stun, e.x, e.y, i);
      } else if (e.isKoopa()) {
        if (e.shell && (stomp || stoppedShell)) {
          kickKoopa(e, p, i, rules);
          if (stomp) {
            bouncePlayer(i, top);
          }
          emit(EventType::Kick, e.x, e.y, i);
        } else if (stomp) {
          if (e.type == EnemyType::FlyingKoopa && e.winged) {
            e.winged = false;
          } else {
            tuckKoopa(e, rules);
          }
          e.touchGrace = rules.shellKickGrace;
          e.lastPlayer = i;
          bouncePlayer(i, top);
          emit(EventType::Stun, e.x, e.y, i);
        } else {
          hurtPlayer(i);
        }
      } else if (e.stunned) {
        defeatEnemy(e, i);
      } else {
        hurtPlayer(i);
      }

      if (phase != Phase::Playing) {
        break;
      }
    }

    if (phase != Phase::Playing) {
      break;
    }

    for (const auto & boo : boos) {
      if (overlap(p, 20, p.height(), boo, 26, 28)) {
        hurtPlayer(i);
      }
    }
  }
}

void BattleGame::stunPlayer(int index, float seconds) {
  auto & p = players[index];
  if (p.stunTimer > 0) {
    return; // A stun runs its course: hits during it neither refresh nor extend it.
  }

  p.stunTimer = seconds;
  p.crouched  = p.super;
  p.vx        = 0;
  emit(EventType::Stun, p.x, p.y, index);
}

void BattleGame::collidePlayers(const std::array<Player, 2> & previous) {
  for (int attacker = 0; attacker < 2; ++attacker) {
    const int    victim      = 1 - attacker;
    auto &       a           = players[attacker];
    const auto & b           = players[victim];
    const float  top         = b.y - b.height();
    const float  previousTop = previous[victim].y - previous[victim].height();
    float        dx          = std::abs(a.x - b.x);
    dx                       = std::min(dx, width - dx);
    const bool descending    = a.vy > 0 && a.y - previous[attacker].y > top - previousTop;
    if (descending && dx < 24 && previous[attacker].y <= previousTop + 2 && a.y >= top) {
      // A stomp and a jump are both attacks: they cancel, leaving only the bounce.
      if (!b.jumping) {
        stunPlayer(victim, rules.stompStunSeconds);
      }
      bouncePlayer(attacker, top);
      return;
    }
  }

  auto & a = players[0];
  auto & b = players[1];
  if (overlap(a, 24, a.height(), b, 24, b.height())) {
    float dx = b.x - a.x;
    if (dx > width / 2) {
      dx -= width;
    }
    if (dx < -width / 2) {
      dx += width;
    }

    const float push     = (dx >= 0 ? 1.f : -1.f) * (24 - std::abs(dx)) * .5f;
    const auto  canShift = [&](const Player & p, float shift) {
      const auto blocked = [&](float x, float y, float w) {
        if (p.y <= y + .01f || p.y - p.height() >= y + platformThickness - .01f) {
          return false;
        }
        for (float offset : {-width, 0.f, width}) {
          if (p.x + shift + 11 > x + offset + .01f && p.x + shift - 11 < x + offset + w - .01f) {
            return true;
          }
        }
        return false;
      };

      for (const auto & s : segments) {
        if (blocked(s.x, s.y, 32)) {
          return false;
        }
      }
      return !powCharges || !blocked(380, lowerPlatformY, 40);
    };

    const bool moveA = canShift(a, -push), moveB = canShift(b, push);
    if (moveA && moveB) {
      a.x -= push;
      b.x += push;
    } else if (!moveA && canShift(b, push * 2)) {
      b.x += push * 2;
    } else if (!moveB && canShift(a, -push * 2)) {
      a.x -= push * 2;
    }

    wrap(a);
    wrap(b);
    const float av = a.vx;
    a.vx           = b.vx * .65f;
    b.vx           = av * .65f;
  }
}

void BattleGame::step(float dt, const std::array<Input, 2> & inputs) {
  if (dt <= 0) {
    return;
  }

  events.clear();
  updateDefeatedEnemies(dt);
  updateEliminatedPlayers(dt);

  if (phase == Phase::Ready) {
    readyTimer -= dt;
    if (readyTimer <= 0) {
      phase = Phase::Playing;
    }
    return;
  }

  if (phase != Phase::Playing) {
    return;
  }

  shake = std::max(0.f, shake - dt);
  for (auto & s : segments) {
    s.bump = std::max(0.f, s.bump - dt);
  }

  const auto previousPlayers = players;
  for (auto & p : players) {
    p.stunTimer = std::max(0.f, p.stunTimer - dt);
  }
  for (int i = 0; i < 2; ++i) {
    updatePlayer(i, inputs[i], dt);
  }

  updateEnemies(dt);
  updateItems(dt);
  updateBoos(dt);
  collisions(dt, previousPlayers);

  enemies.erase(std::remove_if(enemies.begin(),
                               enemies.end(),
                               [](const Enemy & e) {
                                 return !e.alive;
                               }),
                enemies.end());
  items.erase(std::remove_if(items.begin(),
                             items.end(),
                             [](const Item & i) {
                               return !i.alive;
                             }),
              items.end());

  if (phase != Phase::Playing || !rules.spawning) {
    return;
  }

  enemyTimer -= dt;
  booTimer -= dt;
  itemTimer -= dt;
  coinTimer -= dt;

  if (enemyTimer <= 0) {
    if (static_cast<int>(enemies.size()) < rules.enemyLimit) {
      spawnEnemy();
    }
    enemyTimer = rules.enemySpawnSeconds;
  }

  if (booTimer <= 0) {
    Boo boo;
    boo.x           = 64;
    boo.baseY       = range(topPlatformY + 36, 365);
    boo.y           = topPlatformY;
    boo.diagonal    = nextBooDiagonal;
    nextBooDiagonal = !nextBooDiagonal;
    boo.vx          = boo.diagonal ? rules.booDiagonalSpeed : rules.booWaveSpeed;
    boo.vy          = rules.booVerticalSpeed;
    boos.push_back(boo);
    booTimer = rules.booSpawnSeconds;
  }

  if (itemTimer <= 0) {
    if (std::count_if(items.begin(),
                      items.end(),
                      [](const Item & i) {
                        return i.type != ItemType::Coin;
                      })
        < 3) {
      spawnItem(randomMushroom());
    }
    itemTimer = rules.itemSpawnSeconds;
  }

  if (coinTimer <= 0) {
    if (std::count_if(items.begin(),
                      items.end(),
                      [](const Item & i) {
                        return i.type == ItemType::Coin;
                      })
        < rules.coinLimit) {
      spawnItem(ItemType::Coin);
    }
    coinTimer = rules.coinSpawnSeconds;
  }

  if (!powCharges) {
    powTimer -= dt;
    if (powTimer <= 0) {
      powCharges = 3;
    }
  }
}
} // namespace battle

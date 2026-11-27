#include <cmath>
#include <cstdlib>
#include <iostream>

#include "battle/BattleBot.h"

using namespace battle;

#define CHECK(x)                                                       \
  do {                                                                 \
    if (!(x)) {                                                        \
      std::cerr << __func__ << ':' << __LINE__ << ": " #x " failed\n"; \
      std::exit(1);                                                    \
    }                                                                  \
  } while (0)

namespace {
constexpr float dt = 1.f / 120;

long steps = 0;

// Advances the game with player 0 (and optionally player 1) under bot control.
template<class Stop> float play(BattleGame & g, float seconds, BattleBot & mario, BattleBot * luigi, Stop stop) {
  float t = 0;
  while (t < seconds && !stop()) {
    std::array<Input, 2> in{};
    in[0] = mario.update(g, dt);
    if (luigi) {
      in[1] = luigi->update(g, dt);
    }

    g.step(dt, in);
    ++steps;
    t += dt;
  }

  return t;
}

Rules quietRules() {
  Rules r;
  r.enemySpawnSeconds = r.booSpawnSeconds = r.itemSpawnSeconds = r.coinSpawnSeconds = 10000;
  return r;
}

BattleGame quiet(Rules r = quietRules()) {
  BattleGame g(42, r);
  g.phase = Phase::Playing;

  return g;
}

Item & addCoin(BattleGame & g, float x, float y) {
  Item coin;
  coin.x        = x;
  coin.y        = y;
  coin.grounded = true;
  g.items.push_back(coin);

  return g.items.back();
}

void staysIdleOutsidePlay() {
  BattleGame g(1);
  BattleBot  bot(0);
  CHECK(g.phase == Phase::Ready);

  const Input in = bot.update(g, dt);
  CHECK(!in.left && !in.right && !in.jump && !in.dash && !in.down);
  CHECK(bot.simulatedSteps == 0);
}

void routesFollowTheArena() {
  BattleGame g = quiet();
  BattleBot  bot(0);

  const auto time = [&](float fromX, float fromY, float toX, float toY) {
    return bot.travelTime(g, fromX, fromY, toX, toY);
  };

  CHECK(time(200, floorY, 200, floorY) == 0);

  // Every tier is reachable from every other, and higher tiers cost more.
  const float tiers[] = {floorY, lowerPlatformY, middlePlatformY, topPlatformY};
  for (float from : tiers) {
    for (float to : tiers) {
      CHECK(time(250, from, 250, to) < 8);
    }
  }
  CHECK(time(250, floorY, 250, lowerPlatformY) < time(250, floorY, 250, middlePlatformY));
  CHECK(time(250, floorY, 250, middlePlatformY) < time(250, floorY, 250, topPlatformY));

  // Walking wraps around the floor and around the joined outer platforms.
  CHECK(time(20, floorY, 780, floorY) < .5f);
  CHECK(time(20, topPlatformY, 780, topPlatformY) < .5f);
  CHECK(time(20, lowerPlatformY, 780, lowerPlatformY) < .5f);

  // The middle platform has open ends, so its far side is a long walk.
  CHECK(time(200, middlePlatformY, 600, middlePlatformY) > 1.2f);

  // The top gap can be jumped. The lower gap sits under the middle platform,
  // which cuts a jump short, so that crossing detours through the floor.
  CHECK(time(300, topPlatformY, 500, topPlatformY) < 1.5f);
  CHECK(time(300, lowerPlatformY, 500, lowerPlatformY) > time(300, topPlatformY, 500, topPlatformY));

  // Dropping is quicker than climbing.
  CHECK(time(250, topPlatformY, 250, floorY) < time(250, floorY, 250, topPlatformY));
}

void reachesEveryTier() {
  struct Spot {
    float x, y;
  };

  // Still coins force the bot to do all the travelling.
  Rules r     = quietRules();
  r.coinSpeed = 0;
  for (const Spot spot : {
         Spot{ 60,          floorY},
         Spot{100,  lowerPlatformY},
         Spot{700,  lowerPlatformY},
         Spot{300, middlePlatformY},
         Spot{100,    topPlatformY},
         Spot{700,    topPlatformY}
  }) {
    BattleGame g = quiet(r);
    BattleBot  bot(0);
    addCoin(g, spot.x, spot.y);

    const float estimate = bot.travelTime(g, g.players[0].x, g.players[0].y, spot.x, spot.y);
    const float taken    = play(g, 10, bot, nullptr, [&] {
      return g.players[0].coins == 1;
    });

    CHECK(g.players[0].coins == 1);
    CHECK(bot.goal().type != BattleBot::GoalType::Coin || taken < 10);

    // Estimates steer every decision, so they must stay close to reality.
    CHECK(taken < estimate * 1.5f + .75f);
  }
}

void interceptsCoinsFromThePipes() {
  BattleGame g = quiet();
  BattleBot  bot(0);

  // Coins leave alternating pipes and walk down through the arena.
  for (int coin = 1; coin <= 2; ++coin) {
    g.spawnItem(ItemType::Coin);
    play(g, 8, bot, nullptr, [&] {
      return g.players[0].coins == coin;
    });
    CHECK(g.players[0].coins == coin);
  }

  CHECK(g.players[0].super);
}

Enemy & addSpiny(BattleGame & g, float x, float y, int direction) {
  Enemy spiny;
  spiny.type      = EnemyType::Spiny;
  spiny.x         = x;
  spiny.y         = y;
  spiny.direction = direction;
  spiny.grounded  = true;
  g.enemies.push_back(spiny);

  return g.enemies.back();
}

void jumpsEnemiesInTheWay() {
  // A Spiny cannot be stomped, and it walks straight at the bot along the
  // short route to the coin. Nothing overhangs the top tier, so jump it.
  Rules r     = quietRules();
  r.coinSpeed = 0;

  BattleGame g = quiet(r);
  BattleBot  bot(0);
  g.players[0].x = 540;
  g.players[0].y = topPlatformY;
  addCoin(g, 760, topPlatformY);
  addSpiny(g, 640, topPlatformY, -1);

  const float taken = play(g, 6, bot, nullptr, [&] {
    return g.players[0].coins == 1;
  });

  CHECK(g.players[0].coins == 1 && taken < 2.5f);
  CHECK(g.players[0].super);
  CHECK(g.enemies.size() == 1 && !g.enemies[0].stunned);
}

void goesAroundUnderLowCeilings() {
  // The floor under a platform leaves no room to jump over anything, so the
  // bot wraps around the arena rather than run into the Spiny.
  Rules r     = quietRules();
  r.coinSpeed = 0;

  BattleGame g = quiet(r);
  BattleBot  bot(0);
  g.players[0].x = 150;
  g.players[1].x = 400;
  g.players[1].y = middlePlatformY;
  addCoin(g, 560, floorY);
  addSpiny(g, 300, floorY, -1);

  play(g, 6, bot, nullptr, [&] {
    return g.players[0].coins == 1;
  });
  CHECK(g.players[0].coins == 1 && g.players[0].super);

  // A rival standing in the lane is just as solid. Pushing them along would
  // hand them the coin, so after a moment the bot turns and laps the arena.
  BattleGame blocked = quiet(r);
  BattleBot  patient(0);
  blocked.players[0].x = 700;
  addCoin(blocked, 520, floorY);
  CHECK(blocked.players[1].x == 615 && blocked.players[1].y == floorY);

  play(blocked, 8, patient, nullptr, [&] {
    return blocked.phase != Phase::Playing || blocked.players[0].coins + blocked.players[1].coins > 0;
  });
  CHECK(blocked.players[0].coins == 1 && blocked.players[1].coins == 0);
}

void givesWayInALowLane() {
  // Under the lower platform nobody can hop over anybody. A rival walking
  // straight at the bot will not get past either, so instead of trading
  // shoves the bot turns and laps the arena the other way.
  Rules r     = quietRules();
  r.coinSpeed = 0;

  BattleGame g = quiet(r);
  BattleBot  luigi(1);
  g.players[0].x = 100;
  g.players[1].x = 250;
  addCoin(g, 20, floorY);

  int shoved = 0;
  for (int frame = 0; frame < 8 * 120 && g.players[1].coins == 0; ++frame) {
    std::array<Input, 2> in{};
    in[0].right = g.players[0].x > 60 && g.players[0].x < 400; // Marches toward the bot.
    in[1]       = luigi.update(g, dt);
    g.step(dt, in);
    ++steps;
    for (const auto & e : g.events) {
      shoved += e.type == EventType::Push && e.player == 1;
    }
  }
  CHECK(g.players[1].coins == 1 && g.players[0].coins == 0 && shoved <= 1);
}

void hopsOverInTheOpen() {
  // With nothing overhead an oncoming rival is no wall: the bot goes over
  // them rather than the long way round.
  Rules r     = quietRules();
  r.coinSpeed = 0;

  BattleGame g = quiet(r);
  BattleBot  luigi(1);
  g.players[0].x  = 210;
  g.players[0].vx = g.rules.walkSpeed;
  g.players[1].x  = 300;
  for (auto & p : g.players) {
    p.y = topPlatformY;
  }
  addCoin(g, 110, topPlatformY);

  float took = 0;
  for (; took < 4 && g.players[1].coins == 0; took += dt) {
    std::array<Input, 2> in{};
    in[0].right = g.players[0].x < 290; // Marches toward the bot.
    in[1]       = luigi.update(g, dt);
    g.step(dt, in);
    ++steps;
  }
  CHECK(g.players[1].coins == 1 && took < 1.5f);
}

void twoBotsPassInALowLane() {
  // Two bots meet head on under the lower platform, each bound for beyond
  // the other. Each gives way at once and goes round, instead of trading
  // shoves or waiting for the other to move.
  struct Crossing {
    float marioFrom, luigiFrom, marioTo, luigiTo;
  };
  for (const auto & c : {
         Crossing{790, 600, 560, 740},
         Crossing{300,  30,  60, 280},
         Crossing{ 30, 300, 300,  20}
  }) {
    BotConfig marioSpot, luigiSpot;
    marioSpot.waitX = c.marioTo;
    luigiSpot.waitX = c.luigiTo;
    marioSpot.waitY = luigiSpot.waitY = floorY;

    BattleGame g = quiet();
    BattleBot  mario(0, marioSpot), luigi(1, luigiSpot);
    g.players[0].x = c.marioFrom;
    g.players[1].x = c.luigiFrom;

    // Shoving the yielder along from behind is fine; trading head-on
    // shoves is what a stand-off looks like. Bots count a spot within a
    // tenth of a second's run as reached.
    int        clashes = 0;
    const auto at      = [&](int i, float x) {
      const float d = std::abs(g.players[i].x - x);
      return std::min(d, width - d) < 30;
    };
    for (float t = 0; t < 3.8f && !(at(0, c.marioTo) && at(1, c.luigiTo)); t += dt) {
      std::array<Input, 2> in{mario.update(g, dt), luigi.update(g, dt)};
      g.step(dt, in);
      ++steps;
      int shoved = 0;
      for (const auto & e : g.events) {
        shoved += e.type == EventType::Push;
      }
      clashes += shoved == 2;
    }
    CHECK(at(0, c.marioTo) && at(1, c.luigiTo) && clashes <= 3);
  }
}

void sharesAWaitingSpot() {
  // In a demo both bots play one level, so they wait at the same place. The
  // second one settles near the first instead of shoving them off it.
  for (const auto & config : {BotConfig::easy(), BotConfig(), BotConfig::hard()}) {
    BattleGame g = quiet();
    BattleBot  mario(0, config), luigi(1, config);

    int shoves = 0;
    for (int frame = 0; frame < 12 * 120; ++frame) {
      std::array<Input, 2> in{mario.update(g, dt), luigi.update(g, dt)};
      g.step(dt, in);
      ++steps;
      for (const auto & e : g.events) {
        shoves += frame > 4 * 120 && e.type == EventType::Push;
      }
    }
    CHECK(shoves <= 2);
    for (const auto & p : g.players) {
      const float d = std::abs(p.x - config.waitX);
      CHECK(std::abs(p.y - config.waitY) < 1 && std::min(d, width - d) < 120);
    }
  }
}

void hardBotsMoveOn() {
  // From the round's mirror-image start, two Hard bots reach the POW block
  // together. Each could stomp the other, but either can cancel that by
  // jumping, so neither should stay there trying.
  BattleGame g = quiet();
  BattleBot  mario(0, BotConfig::hard()), luigi(1, BotConfig::hard());
  for (int frame = 0; frame < 6 * 120; ++frame) {
    std::array<Input, 2> in{mario.update(g, dt), luigi.update(g, dt)};
    g.step(dt, in);
    ++steps;
  }
  for (const auto & p : g.players) {
    CHECK(p.y < middlePlatformY + 1);
  }
}

void takesTheMushroomWhenSmall() {
  Rules r         = quietRules();
  r.mushroomSpeed = 0;

  BattleGame g = quiet(r);
  BattleBot  bot(0);
  g.players[0].super = false;

  Item & mushroom = addCoin(g, 300, middlePlatformY);
  mushroom.type   = ItemType::Mushroom;

  play(g, 8, bot, nullptr, [&] {
    return g.players[0].super;
  });
  CHECK(g.players[0].super);

  // A swap that would shrink the bot is left alone.
  BattleGame swap = quiet(r);
  BattleBot  wary(0);
  swap.players[1].super = false;

  Item & trade = addCoin(swap, 240, floorY);
  trade.type   = ItemType::SizeSwapMushroom;

  play(swap, 4, wary, nullptr, [] {
    return false;
  });
  CHECK(swap.players[0].super && !swap.players[1].super);
  CHECK(swap.items.size() == 1);
}

void survivesAndScoresInARealRound() {
  // Full enemy traffic, with a rival that never moves. Test mode keeps the
  // idle rival from being eliminated before the bot has shown anything.
  Rules r;
  r.invinciblePlayers = true;
  r.coinSpawnSeconds  = 5;

  BattleGame g(7, r);
  BattleBot  bot(0);
  const long before = steps;
  play(g, 30, bot, nullptr, [&] {
    return g.phase == Phase::RoundOver;
  });

  CHECK(g.players[0].super);
  CHECK(g.players[0].coins >= 3 || g.winner == 0);

  // Looking ahead must stay cheap enough for a 120 Hz game loop.
  CHECK(bot.simulatedSteps / (steps - before) < 1200);
}

void sparesAStunnedRival() {
  // Land on the rival once, then let them up. Staying put would bounce on
  // their head and stun them again as soon as the first stun wore off.
  BattleGame g = quiet();
  BattleBot  bot(0);
  g.players[1].x        = 400;
  g.players[0].x        = 400;
  g.players[0].y        = floorY - 90;
  g.players[0].vy       = 250;
  g.players[0].grounded = false;

  int   stuns = 0;
  float last  = 0;
  play(g, 3, bot, nullptr, [&] {
    stuns += g.players[1].stunTimer > last;
    last = g.players[1].stunTimer;
    return false;
  });

  CHECK(stuns == 1);
  CHECK(g.players[1].stunTimer == 0);
}

void cannotSeeWhatIsComing() {
  // Two games that look the same but hold different futures: another seed
  // hides the mushroom block elsewhere, and one pipe is about to release an
  // enemy right in the bot's path. Until it appears, the bot must not react.
  Rules soon             = quietRules();
  soon.enemySpawnSeconds = .5f;
  BattleGame calm(42, quietRules()), busy(7, soon);
  for (auto * g : {&calm, &busy}) {
    g->phase        = Phase::Playing;
    g->players[0].x = 200;
    g->players[0].y = topPlatformY;
    g->players[1].x = 400;
  }

  BattleBot first(0, BotConfig::hard()), second(0, BotConfig::hard());
  int       ticks = 0;
  while (ticks < 240 && calm.enemies.size() == busy.enemies.size()) {
    const Input x = first.update(calm, dt), y = second.update(busy, dt);
    CHECK(x.left == y.left && x.right == y.right && x.jump == y.jump && x.dash == y.dash && x.down == y.down);
    calm.step(dt, {x, Input{}});
    busy.step(dt, {y, Input{}});
    steps += 2;
    ++ticks;
  }

  CHECK(ticks > 55 && ticks < 65 && busy.enemies.size() == 1);
}

void keepsClearOfPipeMouths() {
  // Pipes release enemies without warning, so the bot hops across the strip
  // in front of each mouth on the way to the hard level's spot between them.
  for (float start : {200.f, 600.f}) {
    BattleGame g = quiet();
    BattleBot  bot(0, BotConfig::hard());
    g.players[0].x = start;
    g.players[0].y = topPlatformY;
    play(g, 3, bot, nullptr, [&] {
      const auto & me = g.players[0];
      for (float mouth : {64.f, 736.f}) {
        CHECK(me.y < topPlatformY - 40 || std::abs(me.x - mouth) >= 32);
      }
      return false;
    });
    CHECK(std::abs(std::remainder(g.players[0].x, width)) < 30);
  }

  // Told to wait right in a mouth, it waits beside it instead.
  BattleGame g = quiet();
  BotConfig  config;
  config.waitX = 64;
  config.waitY = topPlatformY;
  BattleBot bot(0, config);
  g.players[0].x = 200;
  g.players[0].y = topPlatformY;
  play(g, 4, bot, nullptr, [] {
    return false;
  });
  CHECK(std::abs(bot.goal().x - 64) >= 32 && std::abs(g.players[0].x - bot.goal().x) < 30);
}

void holdsItsWaitingSpot() {
  // With nothing to do, each level settles on its waiting spot instead of
  // running past it and back.
  for (const BotConfig & config : {BotConfig::easy(), BotConfig(), BotConfig::hard()}) {
    BattleGame g = quiet();
    BattleBot  bot(0, config);
    g.players[1].y  = -3000;
    const auto away = [&] {
      g.players[1].y  = -3000;
      g.players[1].vy = 0;
      return false;
    };

    play(g, 6, bot, nullptr, away);
    play(g, 3, bot, nullptr, [&] {
      const float dx = std::abs(std::remainder(g.players[0].x - bot.goal().x, width));
      CHECK(bot.goal().type == BattleBot::GoalType::Wait && dx < 30 && g.players[0].y == bot.goal().y);
      return away();
    });
  }
}

void levelsDiffer() {
  const BotConfig normal;
  CHECK(BotConfig::easy().decisionTicks > normal.decisionTicks);
  CHECK(normal.decisionTicks > BotConfig::hard().decisionTicks);
  CHECK(BotConfig::easy().noticeSeconds > normal.noticeSeconds);
  CHECK(normal.noticeSeconds > BotConfig::hard().noticeSeconds);
  CHECK(BotConfig::easy().aggression < normal.aggression);
  CHECK(normal.aggression < BotConfig::hard().aggression);

  // The easy bot never dashes, and overlooks a coin that has only just appeared.
  BattleGame g = quiet();
  BattleBot  easy(0, BotConfig::easy());
  g.spawnItem(ItemType::Coin);

  bool  dashed = false;
  float t      = 0;
  while (t < 6 && g.players[0].coins == 0) {
    std::array<Input, 2> in{};
    in[0] = easy.update(g, dt);
    dashed |= in[0].dash;
    if (t < 1) {
      CHECK(easy.goal().type == BattleBot::GoalType::Wait);
    }

    g.step(dt, in);
    ++steps;
    t += dt;
  }

  CHECK(!dashed);
}

void deterministic() {
  const auto run = [](const BotConfig & config) {
    BattleGame g(11);
    BattleBot  mario(0, config), luigi(1, config);
    play(g, 8, mario, &luigi, [] {
      return false;
    });
    return g;
  };

  for (const BotConfig & config : {BotConfig(), BotConfig::hard()}) {
    const BattleGame a = run(config), b = run(config);
    for (int i = 0; i < 2; ++i) {
      CHECK(a.players[i].x == b.players[i].x && a.players[i].y == b.players[i].y);
      CHECK(a.players[i].coins == b.players[i].coins && a.players[i].super == b.players[i].super);
    }
    CHECK(a.enemies.size() == b.enemies.size() && a.phase == b.phase);
  }
}
} // namespace

int main() {
  staysIdleOutsidePlay();
  routesFollowTheArena();
  reachesEveryTier();
  interceptsCoinsFromThePipes();
  jumpsEnemiesInTheWay();
  goesAroundUnderLowCeilings();
  givesWayInALowLane();
  hopsOverInTheOpen();
  twoBotsPassInALowLane();
  sharesAWaitingSpot();
  hardBotsMoveOn();
  takesTheMushroomWhenSmall();
  survivesAndScoresInARealRound();
  sparesAStunnedRival();
  cannotSeeWhatIsComing();
  keepsClearOfPipeMouths();
  holdsItsWaitingSpot();
  levelsDiffer();
  deterministic();

  std::cout << "Computer player: all checks passed (" << steps << " game steps played).\n";
}

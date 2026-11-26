#include "battle/BattleBot.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace battle {
namespace {
  constexpr float far           = 1e6f;
  constexpr float bodyHalfWidth = 11, tile = 32;
  constexpr float powX         = 400;       // Centre of the POW block in BattleGame.cpp.
  constexpr float pipeMouths[] = {64, 736}; // Where BattleGame releases arrivals.
  constexpr float pipeReach    = 32;        // Half-width of the strip in front of each mouth.

  // Lookahead scoring. Coins and survival dominate; the rest breaks ties.
  constexpr float coinReward = 100, rivalCoinPenalty = 30, growReward = 80, hurtPenalty = 150;
  constexpr float roundReward = 1000, rivalHurtReward = 90;
  constexpr float stunnedPenalty = 35, rivalStunReward = 14;
  constexpr float powReward = 70, powWaste = 18, swapPenalty = 15;
  constexpr float kickReward = 45, kickWaste = 8, wrongKick = 25, defeatReward = 6;
  constexpr float secondsWeight = 22, enemyWeight = 26, booWeight = 30;
  constexpr float pipeWeight    = 40; // Per second spent where pipes release enemies and boos.
  constexpr float keepPlanBonus = 1, jumpCost = .5f;
  constexpr float patience = .5f;  // Seconds of delay worth one second closer to the goal.
  constexpr float settle   = .25f; // Weight on where a plan ends once the bot is at its goal.
  constexpr float arrived  = .1f;  // Seconds from the goal that count as being there.

  float wrapX(float x) {
    x = std::fmod(x, width);
    return x < 0 ? x + width : x;
  }

  // Shortest signed horizontal distance across the wrap.
  float offset(float from, float to) {
    float d = std::fmod(to - from, width);
    if (d > width / 2) {
      d -= width;
    }
    if (d < -width / 2) {
      d += width;
    }
    return d;
  }

  // Anything a pipe releases appears right here with no warning.
  bool atPipeMouth(const Player & p) {
    if (p.invulnerable > .5f || p.y <= topPlatformY - 40 || p.y - p.height() >= topPlatformY) {
      return false;
    }

    return std::any_of(std::begin(pipeMouths), std::end(pipeMouths), [&](float mouth) {
      return std::abs(offset(p.x, mouth)) < pipeReach;
    });
  }

  // Moves a top-tier spot to the nearer side of any pipe mouth it blocks.
  float clearOfPipes(float x, float y) {
    if (std::abs(y - topPlatformY) > 1) {
      return x;
    }

    for (float mouth : pipeMouths) {
      const float d = offset(mouth, x);
      if (std::abs(d) < pipeReach + bodyHalfWidth) {
        return wrapX(mouth + (d < 0 ? -1 : 1) * (pipeReach + bodyHalfWidth));
      }
    }

    return x;
  }

  bool stoppedShell(const Enemy & e) {
    return e.isKoopa() && e.shell && std::abs(e.vx) <= .01f;
  }

  // Stunned enemies and resting shells are safe to touch until they wake.
  bool harmful(const Enemy & e) {
    if (!e.alive) {
      return false;
    }

    const bool passive = e.isKoopa() ? stoppedShell(e) : e.stunned;
    return !passive || e.recovery < .4f;
  }

  // Seconds since an item left a pipe, or far when it is no longer obvious.
  float freshness(const Item & item, const Rules & rules) {
    if (!item.grounded || std::abs(item.y - topPlatformY) > 1) {
      return far;
    }

    const float travelled = (item.x - (item.direction > 0 ? 64 : 736)) * item.direction;
    if (travelled < 0 || travelled > 270) {
      return far;
    }
    return travelled / (item.type == ItemType::Coin ? rules.coinSpeed : rules.mushroomSpeed);
  }

  // Without seeing the rival's buttons, assume they keep doing the same thing,
  // except that a rival standing right under our platform is about to bump it.
  Input guessInput(const Player & rival, const Player & me, const Rules & rules) {
    Input in;
    if (rival.stunTimer > 0) {
      return in;
    }

    in.left  = rival.vx < -30;
    in.right = rival.vx > 30;
    in.dash  = std::abs(rival.vx) > rules.walkSpeed + 15;
    in.jump  = !rival.grounded && rival.vy < 0;

    const float below = rival.y - me.y;
    if (rival.grounded && me.grounded && below > 60 && below < 150
        && std::abs(offset(rival.x + rival.vx * .08f, me.x)) < 45) {
      in.jump = true;
    }
    return in;
  }
} // namespace

// Walks, reacts late, keeps to itself, and waits on the floor, where coins
// arrive last.
BotConfig BotConfig::easy() {
  BotConfig c;
  c.decisionTicks   = 16;
  c.horizon         = .5f;
  c.forecastSeconds = 0;
  c.noticeSeconds   = 2.5f;
  c.aggression      = 0;
  c.dash            = false;
  c.waitY           = floorY;
  return c;
}

// Reacts at once, hunts the rival, and waits between the two pipes, where
// coins appear first and fresh enemies walk away.
BotConfig BotConfig::hard() {
  BotConfig c;
  c.decisionTicks = 4;
  c.noticeSeconds = 0;
  c.aggression    = 1;
  c.harassSeconds = 2;
  c.waitX         = 0;
  c.waitY         = topPlatformY;
  return c;
}

BattleBot::BattleBot(int player, BotConfig settings) : config(settings), self(player) {
}

// --- Navigation ------------------------------------------------------------

float BattleBot::along(const Span & span, float x) const {
  const float s = wrapX(x - span.left);
  if (span.length >= width) {
    return s;
  }

  // Positions in the uncovered part belong to whichever end is nearer.
  return s > span.length + (width - span.length) / 2 ? s - width : s;
}

bool BattleBot::covers(const Span & span, float x, float halfWidth) const {
  if (span.length >= width) {
    return true;
  }

  const float s = along(span, x);
  return s > -halfWidth && s < span.length + halfWidth;
}

// The surface a body at (x, feet y) stands on or would fall onto.
int BattleBot::support(float x, float y) const {
  int best = static_cast<int>(spans.size()) - 1; // The floor.
  for (int i = 0; i < static_cast<int>(spans.size()); ++i) {
    if (spans[i].y >= y - 1 && spans[i].y < spans[best].y && covers(spans[i], x, bodyHalfWidth - .5f)) {
      best = i;
    }
  }
  return best;
}

BattleBot::Place BattleBot::locate(float x, float y) const {
  const int span = support(x, y);
  return {span, along(spans[span], x)};
}

float BattleBot::walk(int span, float a, float b) const {
  const float d = std::abs(a - b);
  return spans[span].length >= width ? std::min(d, width - d) : d;
}

void BattleBot::buildArena(const BattleGame & game) {
  const auto & rules = game.rules;
  arenaTiles         = game.segments.size();
  runSpeed           = (config.dash ? rules.dashSpeed : rules.walkSpeed) * .95f;
  spans.clear();
  nodes.clear();

  // Tiles sharing a surface merge into spans, joined across the wrap.
  std::vector<std::pair<float, float>> tiles;
  for (const auto & s : game.segments) {
    tiles.push_back({s.y, s.x});
  }
  std::sort(tiles.begin(), tiles.end());

  for (const auto & [y, x] : tiles) {
    if (!spans.empty() && spans.back().y == y && std::abs(spans.back().left + spans.back().length - x) < .5f) {
      spans.back().length += tile;
    } else {
      spans.push_back({x, tile, y});
    }
  }

  for (std::size_t i = 0; i < spans.size(); ++i) {
    for (std::size_t j = 0; j < spans.size(); ++j) {
      if (i != j && spans[i].y == spans[j].y && spans[j].left < .5f
          && std::abs(spans[i].left + spans[i].length - width) < .5f) {
        spans[i].length += spans[j].length;
        spans.erase(spans.begin() + static_cast<long>(j));
        i = static_cast<std::size_t>(-1);
        break;
      }
    }
  }
  spans.push_back({0, width, floorY});

  struct Link {
    int   from, to;
    float cost;
  };
  std::vector<Link> links;

  const auto node = [&](int span, float s) {
    nodes.push_back({span, s});
    return static_cast<int>(nodes.size()) - 1;
  };

  const float heldGravity = rules.gravity * rules.heldJumpGravity;
  const float fullJump    = rules.jumpSpeed * rules.jumpSpeed / (2 * heldGravity);
  const float airtime     = rules.jumpSpeed / heldGravity + std::sqrt(2 * fullJump / rules.gravity);
  const float topSpeed    = config.dash ? rules.dashSpeed : rules.walkSpeed;
  const float longestHop  = topSpeed * airtime * .85f - 2 * bodyHalfWidth;

  for (int i = 0; i + 1 < static_cast<int>(spans.size()); ++i) {
    const Span span = spans[i];
    if (span.length >= width) {
      continue;
    }

    for (int side : {-1, 1}) {
      const float edgeS = side < 0 ? 0 : span.length;
      const float edgeX = wrapX(span.left + edgeS);
      const float outX  = wrapX(edgeX + side * 30);
      const int   below = support(outX, span.y + 2);
      const float rise  = spans[below].y - span.y;

      // Walk off the end and fall to the surface below. Half the fall is
      // free because the body keeps travelling sideways in the air.
      links.push_back({node(i, edgeS + side * 13),
                       node(below, along(spans[below], wrapX(edgeX + side * 16))),
                       std::sqrt(2 * rise / rules.gravity) * .5f});

      // Jump from beside the end and steer onto it.
      if (rise < fullJump - 10) {
        const float climb
          = (rules.jumpSpeed - std::sqrt(rules.jumpSpeed * rules.jumpSpeed - 2 * heldGravity * rise)) / heldGravity;
        links.push_back({node(below, along(spans[below], outX)), node(i, edgeS - side * 8), climb + .1f});
      }

      // Leap a gap in the same tier when nothing overhead cuts the jump short.
      int   across = -1;
      float gap    = far;
      for (int j = 0; j + 1 < static_cast<int>(spans.size()); ++j) {
        if (spans[j].y != span.y) {
          continue;
        }

        const float facing = side > 0 ? spans[j].left : wrapX(spans[j].left + spans[j].length);
        const float d      = wrapX(side > 0 ? facing - edgeX : edgeX - facing);
        if (d > 1 && d < gap) {
          gap    = d;
          across = j;
        }
      }

      bool open = across >= 0 && gap <= longestHop;
      for (int c = 0; open && c + 1 < static_cast<int>(spans.size()); ++c) {
        if (spans[c].y >= span.y || span.y - spans[c].y > fullJump + 48 + platformThickness) {
          continue;
        }
        for (float d = 0; d <= gap; d += tile / 2) {
          open = open && !covers(spans[c], wrapX(edgeX + side * d), bodyHalfWidth);
        }
      }

      if (open) {
        links.push_back(
          {node(i, edgeS - side * 2), node(across, side > 0 ? 6 : spans[across].length - 6), gap / topSpeed + .35f});
      }
    }
  }

  const int count = static_cast<int>(nodes.size());
  nodesOn.assign(spans.size(), {});
  between.assign(count, std::vector<float>(count, far));
  for (int a = 0; a < count; ++a) {
    nodesOn[nodes[a].span].push_back(a);
    for (int b = 0; b < count; ++b) {
      if (nodes[a].span == nodes[b].span) {
        between[a][b] = walk(nodes[a].span, nodes[a].s, nodes[b].s) / runSpeed;
      }
    }
  }
  for (const auto & link : links) {
    between[link.from][link.to] = std::min(between[link.from][link.to], link.cost);
  }
  for (int k = 0; k < count; ++k) {
    for (int a = 0; a < count; ++a) {
      for (int b = 0; b < count; ++b) {
        between[a][b] = std::min(between[a][b], between[a][k] + between[k][b]);
      }
    }
  }

  toGoal.assign(count, far);
}

float BattleBot::travel(const Place & from, const Place & to) const {
  float best = from.span == to.span ? walk(from.span, from.s, to.s) / runSpeed : far;
  for (int a : nodesOn[from.span]) {
    const float start = walk(from.span, from.s, nodes[a].s) / runSpeed;
    if (start >= best) {
      continue;
    }
    for (int b : nodesOn[to.span]) {
      best = std::min(best, start + between[a][b] + walk(to.span, nodes[b].s, to.s) / runSpeed);
    }
  }
  return best;
}

// Walking distance that goes the other way around a blocking rival.
float BattleBot::detour(int span, float a, float b) const {
  const float d = walk(span, a, b);
  if (span != wall.span) {
    return d;
  }

  const bool loop    = spans[span].length >= width;
  const bool inside  = wall.s > std::min(a, b) && wall.s < std::max(a, b);
  const bool blocked = loop && std::abs(a - b) > width / 2 ? !inside : inside;
  return !blocked ? d : loop ? width - d : d + 3 * runSpeed;
}

float BattleBot::timeToGoal(const Place & from) const {
  float best = from.span == goalPlace.span ? detour(from.span, from.s, goalPlace.s) / runSpeed : far;
  for (int a : nodesOn[from.span]) {
    best = std::min(best, detour(from.span, from.s, nodes[a].s) / runSpeed + toGoal[a]);
  }
  return best;
}

float BattleBot::travelTime(const BattleGame & game, float fromX, float fromY, float toX, float toY) {
  if (spans.empty() || arenaTiles != game.segments.size()) {
    buildArena(game);
  }
  return travel(locate(fromX, fromY), locate(toX, toY));
}

// --- Strategy --------------------------------------------------------------

void BattleBot::setGoal(const Goal & goal) {
  current   = goal;
  goalPlace = locate(goal.x, goal.y);
  for (std::size_t a = 0; a < nodes.size(); ++a) {
    toGoal[a] = far;
    for (int b : nodesOn[goalPlace.span]) {
      toGoal[a] = std::min(toGoal[a], between[a][b] + walk(goalPlace.span, nodes[b].s, goalPlace.s) / runSpeed);
    }
  }
}

void BattleBot::watchProgress(const BattleGame & game) {
  const auto & me    = game.players[self];
  const auto & rival = game.players[1 - self];
  const Place  mine  = locate(me.x, me.y);
  const Place  block = locate(rival.x, rival.y);

  // The wall stands for as long as the rival does.
  if (wall.span >= 0 && (!rival.grounded || block.span != wall.span || walk(wall.span, block.s, wall.s) > 50)) {
    wall.span = -1;
  }

  const float left = timeToGoal(mine);
  if (left <= .2f || left < closest - .05f || left > closest + .5f) {
    closest = left;
    stalled = 0;
    return;
  }

  stalled += config.strategySeconds;
  if (stalled > .75f && rival.grounded && me.grounded && block.span == mine.span
      && walk(mine.span, mine.s, block.s) < 40) {
    wall    = block;
    closest = far;
  }
}

void BattleBot::chooseGoal(const BattleGame & game) {
  const auto & me     = game.players[self];
  const auto & rival  = game.players[1 - self];
  const Place  mine   = locate(me.x, me.y);
  const Place  theirs = locate(rival.x, rival.y);

  Goal best;
  best.x       = clearOfPipes(config.waitX, config.waitY);
  best.y       = config.waitY;
  best.utility = 1;

  const auto consider = [&](Goal goal) {
    // Prefer the goal already being chased so near-ties do not cause dithering.
    if (goal.type == current.type && std::abs(offset(goal.x, current.x)) < 90 && std::abs(goal.y - current.y) < 60) {
      goal.utility += 5;
    }
    if (goal.utility > best.utility) {
      best = goal;
    }
  };

  // Predict where every item will be, with both players out of the picture.
  constexpr float   sample = .25f, step = 1.f / 60;
  const std::size_t count   = game.items.size();
  const int         samples = count ? 1 + static_cast<int>(config.forecastSeconds / sample) : 0;
  std::vector<Body> path(count * static_cast<std::size_t>(samples));
  if (count) {
    BattleGame future     = game;
    future.rules.spawning = false;
    for (auto & p : future.players) {
      p.y        = -6000;
      p.vy       = 0;
      p.grounded = false;
    }

    for (int k = 0; k < samples; ++k) {
      for (std::size_t i = 0; i < count; ++i) {
        path[i * samples + k] = future.items[i];
      }
      for (int n = 0; k + 1 < samples && n < static_cast<int>(sample / step + .5f); ++n) {
        future.step(step, {});
        ++simulatedSteps;
      }
    }
  }

  float rivalCoinLead = far; // When the rival reaches a coin we cannot beat them to.
  for (std::size_t i = 0; i < count; ++i) {
    const auto & item = game.items[i];
    Goal         goal;
    switch (item.type) {
      case ItemType::Coin:
        goal.type    = GoalType::Coin;
        goal.utility = 100 + 5.f * rival.coins;
        break;
      case ItemType::Mushroom:
        goal.type    = GoalType::Mushroom;
        goal.utility = !me.super ? 95 : !rival.super ? 30 : 6;
        break;
      case ItemType::SizeSwapMushroom:
        goal.type    = GoalType::SizeSwap;
        goal.utility = !me.super && rival.super ? 105 : 0;
        break;
      case ItemType::PositionSwapMushroom:
        break;
    }
    if (goal.utility <= 0 || freshness(item, game.rules) < config.noticeSeconds) {
      continue;
    }

    // Meet the item at the earliest point of its path either player can reach.
    float mineAt = far, theirsAt = far;
    int   meet = 0;
    for (int k = 0; k < samples; ++k) {
      const auto & body  = path[i * samples + k];
      const Place  place = locate(body.x, body.y);
      const float  t     = k * sample;
      const float  eta   = std::max(t, travel(mine, place));
      if (eta < mineAt - .01f) {
        mineAt = eta;
        meet   = k;
      }
      theirsAt = std::min(theirsAt, std::max(t, travel(theirs, place)));
    }

    const bool losing = theirsAt + .3f < mineAt;
    if (losing && item.type == ItemType::Coin) {
      rivalCoinLead = std::min(rivalCoinLead, theirsAt);
    }

    goal.x       = path[i * samples + meet].x;
    goal.y       = path[i * samples + meet].y;
    goal.utility = std::max(2.f, (goal.utility - 9 * mineAt) * (losing ? .5f : 1.f));
    consider(goal);
  }

  if (config.aggression > 0 && harassCooldown <= 0) {
    // A resting shell level with the rival is a missile: kick it from behind.
    for (const auto & e : game.enemies) {
      if (!e.alive || !stoppedShell(e) || !e.grounded || !rival.grounded || std::abs(rival.y - e.y) > 4) {
        continue;
      }

      const float eta = travel(mine, locate(e.x, e.y));
      if (e.recovery < eta + .6f) {
        continue;
      }

      Goal goal;
      goal.type          = GoalType::KickShell;
      goal.kickDirection = offset(e.x, rival.x) >= 0 ? 1 : -1;
      const bool behind  = offset(me.x, e.x) * goal.kickDirection > 0;
      goal.x             = behind ? e.x : wrapX(e.x - goal.kickDirection * 44);
      goal.y             = e.y;
      goal.utility       = config.aggression * 60 - 9 * eta - (behind ? 0 : 6);
      consider(goal);
    }

    // Bumping the platform under the rival stuns and pops them up.
    if (rival.grounded && rival.y < floorY - 1 && rival.stunTimer <= 0) {
      const int   under = support(rival.x, rival.y + 2);
      const float eta   = travel(mine, {under, along(spans[under], rival.x)});
      if (spans[under].y - rival.y < 150 && eta < 1) {
        Goal goal;
        goal.type    = GoalType::BumpRival;
        goal.x       = rival.x;
        goal.y       = spans[under].y;
        goal.utility = config.aggression * (rival.super ? 28 : 45) - 10 * eta - 5;
        consider(goal);
      }
    }
  }

  if (game.powCharges > 0 && powCooldown <= 0) {
    // POW stuns every enemy, clears the ghosts, and freezes the rival.
    int threats = 0;
    for (const auto & e : game.enemies) {
      threats += harmful(e) && std::abs(offset(me.x, e.x)) < 200 && std::abs(e.y - me.y) < 130;
    }
    for (const auto & boo : game.boos) {
      threats += 2 * (std::abs(offset(me.x, boo.x)) < 180 && std::abs(boo.y - me.y) < 180);
    }

    const float eta      = travel(mine, locate(powX, floorY));
    const bool  cornered = !me.super && threats >= 3;
    const bool  lastCoin = rival.coins + 1 >= Rules::coinsToWin && eta + .2f < rivalCoinLead && rivalCoinLead < far;
    if (cornered || lastCoin) {
      Goal goal;
      goal.type    = GoalType::Pow;
      goal.x       = powX;
      goal.y       = floorY;
      goal.utility = (lastCoin ? 120 : 70) - 9 * eta;
      consider(goal);
    }
  }

  setGoal(best);
}

// --- Motor control ---------------------------------------------------------

Input BattleBot::drive(const Plan & plan, const Player & player, PlanState & state, int tick) {
  Input in;
  in.left  = plan.direction < 0;
  in.right = plan.direction > 0;
  in.dash  = plan.dash;
  if (!plan.jump) {
    return in;
  }

  // A jump already in flight belongs to the plan: holding only stretches it.
  if (tick == 0 && !player.grounded && !plan.delay) {
    state.launched = true;
  }

  if (!state.launched) {
    // The game wants a fresh press, so let go first if the button is down.
    if (tick >= plan.delay && player.grounded && !player.jumpHeld) {
      in.jump        = true;
      state.launched = true;
    }
    return in;
  }

  in.jump = plan.jump == 2 && player.vy < 0;
  return in;
}

float BattleBot::danger(const BattleGame & game) const {
  const auto & me = game.players[self];
  if (me.invulnerable > .5f) {
    return 0;
  }

  const float top   = me.y - me.height();
  float       total = 0;
  for (const auto & e : game.enemies) {
    if (!harmful(e)) {
      continue;
    }

    const float apart = std::max({0.f, e.y - e.height() - me.y, top - e.y});
    if (apart > 44) {
      continue;
    }

    // Fast movers such as kicked shells need a wider berth.
    const float dx      = offset(me.x, e.x);
    const float closing = std::max(0.f, (dx > 0 ? -1.f : 1.f) * (e.vx - me.vx));
    const float reach   = 46 + closing * .3f;
    if (std::abs(dx) < reach) {
      total += enemyWeight * (1 - std::abs(dx) / reach) * (apart < 8 ? 1.f : .5f);
    }
  }

  for (const auto & boo : game.boos) {
    const float d = std::hypot(offset(me.x, boo.x), boo.y - 14 - (me.y - me.height() / 2));
    if (d < 85) {
      total += booWeight * (1 - d / 85);
    }
  }

  return total * (me.super ? 1.f : 1.8f);
}

// Plays one candidate on a copy of the game and scores the outcome.
float BattleBot::rollout(const BattleGame & game, const Plan & candidate, float dt) {
  const int   rivalIndex = 1 - self;
  const Input rivalInput = guessInput(game.players[rivalIndex], game.players[self], game.rules);
  const int   horizon    = static_cast<int>(config.horizon / dt + .5f);
  const int   limit      = std::max(horizon, static_cast<int>(config.maxHorizon / dt + .5f));

  // Credit the best point along the way: the next decision can stop or turn
  // there, so running past a jump-off spot must not look like a setback.
  float      nearest = timeToGoal(locate(game.players[self].x, game.players[self].y));
  const bool there   = nearest < arrived;

  // Arrivals (pipes, boos, the mushroom block, POW refills) stay as unknown
  // to the bot as they are to a person until they actually appear.
  BattleGame sim     = game;
  sim.rules.spawning = false;

  PlanState state;
  float     score = 0;
  for (int tick = 0; tick < limit || (sim.players[self].stunTimer > 0 && tick < 2 * limit); ++tick) {
    const Player before = sim.players[self], rivalBefore = sim.players[rivalIndex];

    std::array<Input, 2> in;
    in[self]       = drive(candidate, before, state, tick);
    in[rivalIndex] = rivalInput;
    sim.step(dt, in);
    ++simulatedSteps;

    const auto & me    = sim.players[self];
    const auto & rival = sim.players[rivalIndex];
    const float  soon  = 1 - .125f * tick / limit; // Sooner rewards are worth more.

    score += coinReward * soon * (me.coins - before.coins);
    score -= atPipeMouth(me) ? pipeWeight * dt : 0;
    score -= rivalCoinPenalty * (rival.coins - rivalBefore.coins);
    if (!before.super && me.super) {
      score += growReward * soon;
    }
    if (rivalBefore.super && !rival.super) {
      score += rivalHurtReward * config.aggression;
    }
    if (me.stunTimer > before.stunTimer) {
      score -= stunnedPenalty;
    }
    if (rival.stunTimer > rivalBefore.stunTimer && harassCooldown <= 0) {
      score += rivalStunReward * config.aggression;
    }

    for (const auto & event : sim.events) {
      if (event.player != self) {
        continue;
      }

      if (event.type == EventType::Pow) {
        score += current.type == GoalType::Pow ? powReward : -powWaste;
      } else if (event.type == EventType::PositionSwap) {
        score -= swapPenalty;
      } else if (event.type == EventType::Kick) {
        // Either a stunned enemy was knocked away or a shell was launched.
        const Enemy * shell = nullptr;
        for (const auto & e : sim.enemies) {
          if (e.isKoopa() && e.shell && e.lastPlayer == self && e.touchGrace > game.rules.shellKickGrace - 1.5f * dt) {
            shell = &e;
          }
        }

        if (!shell) {
          score += defeatReward;
        } else if (current.type == GoalType::KickShell) {
          score += shell->direction == current.kickDirection ? kickReward * config.aggression : -wrongKick;
        } else {
          const bool atRival = std::abs(rival.y - shell->y) < 6 && offset(shell->x, rival.x) * shell->direction > 0;
          score += atRival ? kickReward * config.aggression : -kickWaste;
        }
      }
    }

    if (sim.phase != Phase::Playing) {
      return score + (sim.winner == self ? roundReward : -roundReward);
    }
    if (before.super && !me.super) {
      score -= hurtPenalty;
      break;
    }
    nearest = std::min(nearest, patience * (tick + 1) * dt + timeToGoal(locate(me.x, me.y)));
    // A stun is only as bad as what reaches us before it wears off.
    if (tick + 1 >= horizon && me.grounded && me.stunTimer <= 0) {
      break;
    }
  }

  // Already at the goal, every plan is equally close along the way, so where
  // it ends decides: stay at a waiting spot instead of running past it.
  const float end = there ? timeToGoal(locate(sim.players[self].x, sim.players[self].y)) : 0;
  return score - secondsWeight * (nearest + settle * end) - danger(sim);
}

void BattleBot::choosePlan(const BattleGame & game, float dt) {
  const auto &         me = game.players[self];
  std::array<Plan, 24> candidates;
  int                  count = 0;
  const auto           add   = [&](int direction, bool dash, int jump, int delay) {
    candidates[count++] = {direction, dash, jump, delay};
  };

  for (int direction : {0, -1, 1}) {
    for (int dash = 0; dash <= (direction && config.dash ? 1 : 0); ++dash) {
      add(direction, dash, 0, 0);
      if (me.grounded) {
        add(direction, dash, 1, 0);
        add(direction, dash, 2, 0);
      } else if (me.vy < 0) {
        add(direction, dash, 2, 0);
      }
    }
  }

  if (me.grounded) {
    // Keep running and jump a little later.
    for (int direction : {-1, 1}) {
      for (float delay : {.125f, .25f}) {
        add(direction, config.dash, 2, static_cast<int>(delay / dt + .5f));
      }
    }
  } else {
    // Jump again the moment we land.
    for (int direction : {0, -1, 1}) {
      add(direction, direction != 0 && config.dash, 2, 1);
    }
  }

  Plan  best      = plan;
  float bestScore = -far;
  for (int i = 0; i < count; ++i) {
    const auto & candidate = candidates[i];
    float        score     = rollout(game, candidate, dt);
    if (candidate.direction == plan.direction && candidate.dash == plan.dash) {
      score += keepPlanBonus;
    }
    if (candidate.jump) {
      score -= jumpCost;
    }

    if (score > bestScore) {
      bestScore = score;
      best      = candidate;
    }
  }

  plan      = best;
  planState = {};
  planTick  = 0;
}

Input BattleBot::update(const BattleGame & game, float dt) {
  if (game.phase != Phase::Playing || dt <= 0) {
    idle = true;
    return {};
  }

  if (idle || arenaTiles != game.segments.size()) {
    buildArena(game);
    idle          = false;
    plan          = {};
    planState     = {};
    planTick      = 0;
    untilDecision = 0;
    untilStrategy = powCooldown = harassCooldown = 0;
    current                                      = {};
    wall.span                                    = -1;
    closest = stalled = 0;
  }

  // Only the other player can stun a player, so a fresh stun on the rival is
  // ours. Give them room afterwards instead of hounding them.
  const float stun = game.players[1 - self].stunTimer;
  if (stun > rivalStun) {
    harassCooldown = config.harassSeconds;
  }
  rivalStun = stun;
  harassCooldown -= dt;

  powCooldown -= dt;
  for (const auto & event : game.events) {
    if (event.type == EventType::Pow) {
      powCooldown = 4;
    }
  }

  untilStrategy -= dt;
  if (untilStrategy <= 0) {
    chooseGoal(game);
    watchProgress(game);
    untilStrategy = config.strategySeconds;
  }

  // A stun with no more than this step left wears off before the step moves us.
  const auto & me = game.players[self];
  if (me.stunTimer > dt) {
    untilDecision = 0;
    return {};
  }

  if (--untilDecision < 0) {
    choosePlan(game, dt);
    untilDecision = config.decisionTicks - 1;
  }
  return drive(plan, me, planState, planTick++);
}
} // namespace battle

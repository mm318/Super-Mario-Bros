#pragma once

#include <array>
#include <vector>

#include "battle/BattleGame.h"

// A procedural computer player. It reads only public BattleGame state and
// answers with the same Input a keyboard or gamepad would provide.
//
//   1. Strategy picks one goal by utility: a coin, a mushroom, a stopped
//      shell to kick at the rival, the POW block, or a waiting spot.
//   2. Navigation turns the arena tiles into walkable spans joined by jump,
//      drop, and gap links, giving a travel time between any two places.
//   3. Motor control tries a handful of button combinations on copies of the
//      game, scores what happens, and plays the best one for a few steps.
//      The copies spawn nothing, so the bot never learns what arrives next.
namespace battle {
struct BotConfig {
  int   decisionTicks   = 8;    // Simulation steps between motor decisions.
  float horizon         = .85f; // Seconds each candidate action is simulated.
  float maxHorizon      = 1.3f; // Lookahead cap while a candidate is still airborne.
  float strategySeconds = .25f; // Seconds between goal re-evaluations.
  float forecastSeconds = 4;    // Item path prediction; zero chases current positions.
  float noticeSeconds   = 1;    // Items fresher than this out of a pipe go unnoticed.
  float aggression      = .35f; // Zero ignores the rival; one hunts for stuns and kicks.
  float harassSeconds   = 5;    // Rest between stuns so the rival gets to play too.
  bool  dash            = true; // Whether the bot ever holds the dash button.

  // Where to wait when nothing is worth chasing. Coins drop onto the middle
  // platform a few seconds after leaving a pipe.
  float waitX = width / 2, waitY = middlePlatformY;

  // The defaults above are the normal level.
  static BotConfig easy();
  static BotConfig hard();
};

class BattleBot {
public:
  enum class GoalType {
    Wait,
    Coin,
    Mushroom,
    SizeSwap,
    KickShell,
    BumpRival,
    Pow
  };

  struct Goal {
    GoalType type = GoalType::Wait;
    float    x = 0, y = 0, utility = 0;
    int      kickDirection = 0; // KickShell only: the way the shell should travel.
  };

  explicit BattleBot(int player, BotConfig config = {});

  // Call once per simulation step, before BattleGame::step, with the same dt.
  Input update(const BattleGame & game, float dt);

  const Goal & goal() const {
    return current;
  }

  // Estimated seconds for a player to run and jump between two places.
  float travelTime(const BattleGame & game, float fromX, float fromY, float toX, float toY);

  long simulatedSteps = 0; // Lookahead cost so far, for tests and profiling.

  BotConfig config;

private:
  struct Span {
    float left, length, y; // Walkable surface [left, left + length), wrapping.
  };

  struct Place {
    int   span;
    float s; // Distance along the span from its left end.
  };

  struct Plan {
    int  direction = 0;
    bool dash      = false;
    int  jump      = 0; // 0 none, 1 tap, 2 hold.
    int  delay     = 0; // Steps to wait on the ground before jumping.
  };

  struct PlanState {
    bool launched = false;
  };

  int self;

  std::vector<Span>               spans;
  std::vector<Place>              nodes;
  std::vector<std::vector<int>>   nodesOn;
  std::vector<std::vector<float>> between; // Seconds between link endpoints.
  std::vector<float>              toGoal;  // Seconds from each node to the goal.
  float                           runSpeed   = 250;
  std::size_t                     arenaTiles = 0;

  Goal      current;
  Place     goalPlace{0, 0};
  Plan      plan;
  PlanState planState;
  int       planTick = 0, untilDecision = 0;
  float     untilStrategy = 0, powCooldown = 0, harassCooldown = 0, rivalStun = 0;

  // A rival standing in the way under a low ceiling cannot be jumped over.
  // After a moment without progress, or at once when they come straight at
  // the bot, they count as a wall to route around, wherever shoves move
  // them along that span.
  int   wallSpan  = -1;
  float wallStart = 0; // Where along the span the rival stood when it rose.
  float closest = 0, stalled = 0;
  bool  idle = true;

  void  buildArena(const BattleGame & game);
  float along(const Span & span, float x) const;
  bool  covers(const Span & span, float x, float halfWidth) const;
  int   support(float x, float y) const;
  Place locate(float x, float y) const;
  float walk(int span, float a, float b) const;
  float detour(int span, float a, float b, const Place & block) const;
  float travel(const Place & from, const Place & to) const;
  float timeToGoal(const Place & from, const Place & block) const;
  Place blocker(const BattleGame & game) const;

  void  chooseGoal(const BattleGame & game);
  void  setGoal(const Goal & goal);
  void  watchProgress(const BattleGame & game);
  void  choosePlan(const BattleGame & game, float dt);
  float rollout(const BattleGame & game, const Plan & candidate, float dt);
  float danger(const BattleGame & game) const;

  static Input drive(const Plan & plan, const Player & player, PlanState & state, int tick);
};
} // namespace battle

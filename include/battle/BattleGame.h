#pragma once

#include <array>
#include <cstdint>
#include <random>
#include <vector>

// The supplied rules are authoritative. Unverified timing/physics live here,
// in seconds and logical pixels, rather than pretending to be SNES constants.
namespace battle {
struct Rules {
  bool invinciblePlayers = false; // Test mode prevents elimination after shrinking.
  bool spawning          = true;  // False pauses every arrival: pipes, boos, hidden blocks, POW refills.

  float gravity = 1600, jumpSpeed = 520, heldJumpGravity = .60f;
  float acceleration = 1500, friction = 1800, walkSpeed = 180, dashSpeed = 280;

  float stunSeconds = 7, invulnerabilitySeconds = 1.8f;
  float stompStunSeconds = .5f, platformStunSeconds = 1, powStunSeconds = 1;

  float enemySpawnSeconds = 2.8f, booSpawnSeconds = 14, itemSpawnSeconds = 17;
  float coinSpawnSeconds   = 15;
  float walkingKoopaChance = .07f;

  float powRespawnMin = 16, powRespawnMax = 28, shellRecoverySeconds = 9;
  float coinSpeed = 105, readySeconds = 2.4f;

  float spinySpeed = 66, buzzyBeetleSpeed = 56, flyingKoopaSpeed = 66;
  float angrySpeedMultiplier = 1.5f;
  float flyingKoopaHopSpeed = 260, flyingKoopaHopMin = .6f, flyingKoopaHopMax = 1.25f;
  float koopaSpeed = 65, shellSpeed = 350;
  float shellKickGrace     = .10f;
  float defeatedDriftSpeed = 80, defeatedPopSpeed = 100;
  float deathPopSpeed = 420; // A knocked-out player's hop before dropping off the screen.

  float mushroomSpeed = 78, mushroomChance = .6f, itemBumpSpeed = 240;

  float booWaveSpeed = 66, booDiagonalSpeed = 82, booVerticalSpeed = 74;
  float booWaveAmplitude = 55, booWaveFrequency = 2, booLifetime = 32;

  int                  enemyLimit = 5;
  int                  coinLimit  = 5;
  constexpr static int coinsToWin = 5, roundsToWin = 5;
};

constexpr float width = 800, floorY = 448, screenHeight = 480;
constexpr int   hudHeight    = 28;
constexpr float topPlatformY = 104, middlePlatformY = 224, lowerPlatformY = 344;
constexpr float platformThickness = 32;

struct Body {
  float x = 0, y = 0, vx = 0, vy = 0; // Center x, feet y.
  bool  grounded = false;
};

struct Input {
  bool left = false, right = false, jump = false, dash = false, down = false;
};

struct Player : Body {
  bool  super = true, crouched = false, eliminated = false, jumpHeld = false;
  bool  jumping      = false; // In the air from their own jump, which cancels a stomp.
  float invulnerable = 0;
  float stunTimer    = 0;
  int   coins = 0, facing = 1;

  float height() const {
    return super && !crouched ? 48.f : 28.f;
  }
};

enum class EnemyType {
  Spiny,
  BuzzyBeetle,
  FlyingKoopa,
  WalkingKoopa
};

struct Enemy : Body {
  EnemyType type    = EnemyType::Spiny;
  bool      stunned = false, alive = true, winged = true, shell = false, angry = false;
  int       direction = 1, shellRecycles = 0, lastPlayer = -1;
  float     recovery = 0, hop = .8f, shellCooldown = 0, touchGrace = 0;

  bool isKoopa() const {
    return type == EnemyType::FlyingKoopa || type == EnemyType::WalkingKoopa;
  }

  float height() const {
    return isKoopa() && !stunned && !shell ? 40.f : 27.f;
  }
};

enum class ItemType {
  Coin,
  Mushroom,
  SizeSwapMushroom,
  PositionSwapMushroom
};

struct Item : Body {
  ItemType type      = ItemType::Coin;
  bool     alive     = true;
  int      direction = 1;
};

struct Boo : Body {
  bool  diagonal = false;
  float age = 0, baseY = 200;
};

struct Segment {
  float x, y, bump = 0;
  bool  hiddenItem = false;
};

enum class Phase {
  Ready,
  Playing,
  RoundOver,
  MatchOver
};

enum class EventType {
  Jump,
  Bump,
  Stun,
  Kick,
  Coin,
  Hurt,
  Death,
  Pow,
  Mushroom,
  SizeSwap,
  PositionSwap,
  RoundWin,
  MatchWin
};

struct Event {
  EventType type;
  float     x, y;
  int       player = -1;
};

class BattleGame {
public:
  explicit BattleGame(uint32_t seed = 1, Rules rules = {});

  void startMatch();
  void nextRound();

  void step(float seconds, const std::array<Input, 2> & inputs);

  void bumpSegment(unsigned index, int player);
  void activatePow(int player);
  void hitEnemy(Enemy & enemy, bool pow = false);
  void hurtPlayer(int player);
  void collectItem(Item & item, int player);

  void spawnEnemy();
  void spawnItem(ItemType type);

  static std::vector<EnemyType> roster(int round);

  // True while a knocked-out player is still falling off the screen.
  bool dying() const;

  Rules rules;

  std::array<Player, 2> players;
  std::array<int, 2>    wins{};

  std::vector<Segment> segments;
  std::vector<Enemy>   enemies;
  std::vector<Enemy>   defeatedEnemies;
  std::vector<Item>    items;
  std::vector<Boo>     boos;

  std::vector<Event> events;

  Phase phase = Phase::Ready;
  int   round = 1, winner = -1, powCharges = 3;
  bool  coinVictory = false;
  float readyTimer = 0, shake = 0;

private:
  std::mt19937 random;
  float        enemyTimer = 0, booTimer = 0, itemTimer = 0, coinTimer = 0, powTimer = 0;
  bool         nextPipeRight = false, nextBooDiagonal = false;

  void     resetRound();
  float    range(float low, float high);
  ItemType randomMushroom();

  void moveBody(Body & body, float halfWidth, float dt);
  void updatePlayer(int index, const Input & input, float dt);
  void fitPlayer(Player & player);
  void collidePlayerCeiling(int index, float oldTop);
  void bouncePlayer(int index, float top);
  void stunPlayer(int index, float seconds);
  void collidePlayers(const std::array<Player, 2> & previous);

  void updateEnemies(float dt);
  void defeatEnemy(Enemy & enemy, int player);
  void updateDefeatedEnemies(float dt);
  void updateEliminatedPlayers(float dt);
  void updateItems(float dt);
  void updateBoos(float dt);

  void collisions(float dt, const std::array<Player, 2> & previous);

  void winRound(int player, bool coins);
  void emit(EventType type, float x, float y, int player = -1);
};
} // namespace battle

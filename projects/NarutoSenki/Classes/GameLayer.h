#pragma once
#include "GameOver.h"
#include "GearLayer.h"
#include "PauseLayer.h"
#include "Data/UnitData.h"
#include <memory>
#include <unordered_map>
#include <unordered_set>

#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
#include "glfw3.h"
#include <windows.h>
#define _isPressed(vk_code) (GetAsyncKeyState(vk_code) & 0x8000 ? 1 : 0)
#define isKeyDown(vk_code) (GetAsyncKeyState(vk_code) & 0x8000 ? 1 : 0)
#define getKeyUp(vk_code) (GetAsyncKeyState(vk_code) & 0x8000 ? 0 : 1)
#elif (CC_TARGET_PLATFORM == CC_PLATFORM_LINUX)
#if __has_include("glfw3.h")
#include "glfw3.h"
#elif __has_include(<GLFW/glfw3.h>)
#include <GLFW/glfw3.h>
#elif __has_include("glfw3/include/mac/glfw3.h")
#include "glfw3/include/mac/glfw3.h"
#else
#error "GLFW header not found. Check include paths."
#endif
#define _isPressed(__WINDOW__, __KEY__) glfwGetKey(__WINDOW__, __KEY__)
#elif (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)
#include "../../../cocos2dx/platform/android/jni/JniHelper.h"
#endif

class BGLayer;
class CharacterBase;
class Hero;
class Flog;
class Tower;
class GameLayer;
class HudLayer;
class BattleRuntimeSystem;
class SpawnSystem;
struct SessionState;

extern GameLayer* _gLayer;
extern bool _isFullScreen;
#if (CC_TARGET_PLATFORM == CC_PLATFORM_LINUX || CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
static GLFWwindow* _window = nullptr;
#endif

inline GameLayer* getGameLayer()
{
	return _gLayer;
}

class GameLayer : public Layer
{
	using OnHUDInitializedCallback = std::function<void()>;

	friend class LoadLayer;
	friend class BattleRuntimeSystem;

public:
	GameLayer();
	~GameLayer();

	TMXTiledMap* currentMap;
	CharacterBase* currentPlayer;
	// True if currentPlayer currently holds an extra retain() beyond its
	// normal scene-graph-owned reference count — see processPendingDeaths.
	// Only ever set when the player's last character dies and there's no
	// one left to switch to: GameOver's own result screen still needs to
	// read currentPlayer's actual live stats (kill count, group, etc, not
	// just its name) after it's been removed from the scene, so the
	// object itself is kept alive rather than nulling the pointer or
	// letting it be freed. Released exactly once in the destructor.
	bool _currentPlayerIsExtraRetained = false;

	vector<string> _allyRoster;
	vector<string> _enemyAllyRoster;
	void switchToAllySlot(int slotIndex);

	// AI-side mirror of the player's manual ally swap. Not a priority
	// action — it just waits for its cooldown and a clear moment, then
	// swaps like the player would via the HUD button.
	float _enemyAllySwitchCooldown = 15.0f; // seconds before AI's first swap attempt
	void switchEnemyAllySlot(int slotIndex);
	void updateEnemyAllySwitch(float dt);

	// Duel mode (ModeBoss): each side has 3 characters total (whoever's
	// currently active + the 2 in _allyRoster/_enemyAllyRoster). A death no
	// longer ends the match outright — it eliminates that one character and
	// force-switches to the next living roster member, the same way a
	// voluntary switch does. Only once every one of a side's 3 characters
	// has died does the match actually end. Eliminated names are locked out
	// of being switched back to for the rest of the match.
	unordered_set<string> _eliminatedAllies;
	unordered_set<string> _eliminatedEnemyAllies;
	bool isRosterNameEliminated(const string& name, bool isPlayerSide);

	// [0] = enemy side's shared chakra pool, [1] = player side's -- see
	// getDuelChakra/setDuelChakra/kDuelChakraBarSize above for details.
	uint32_t _duelChakra[2] = {0, 0};
	// Called from ModeBoss::onCharacterDead() instead of ending the match
	// directly. Queues the elimination/force-switch to run next frame
	// rather than doing it synchronously — onCharacterDead() fires as the
	// very first line of CharacterBase::dead(), before dead()'s own later
	// body (HP bar clearing, controller/possession cleanup, etc., all
	// operating on the dying hero) has run. Spawning the replacement and
	// updating the HUD synchronously right here would just get clobbered
	// a moment later when dead()'s tail runs and hides the HP bar / zeroes
	// the label for what would by then already be the wrong character.
	struct PendingDeathInfo
	{
		string name;
		Group group;
		Vec2 position;
		int charId;
		// The dying hero itself. Safe to hold across the one-frame defer —
		// enableReborn = false means Hero::reborn() no-ops entirely (see
		// despawnDeadHero), so nothing tears this object down or frees it
		// between forceSwitchOnDeath() queuing this and processPendingDeaths
		// picking it back up next frame.
		CharacterBase* hero;
		// True if this hero was mid-retirement (CharacterBase::_isRetiring)
		// at the moment it died — captured synchronously in
		// forceSwitchOnDeath()/eliminateInactiveHero() rather than
		// re-checked later, since checkRetireFinish() resets _isRetiring to
		// false as soon as it sees State::DEAD, before this queue is even
		// processed. Tells processPendingDeaths to just eliminate the name
		// and despawn the body, without force-switching or touching
		// currentPlayer/onGameOver — see eliminateInactiveHero's comment
		// for why a retiring hero's death can never legitimately trigger
		// either of those.
		bool wasRetiring;
	};
	vector<PendingDeathInfo> _pendingDeaths;
	void forceSwitchOnDeath(CharacterBase* deadHero);
	// Handles a *retiring* hero (see CharacterBase::_isRetiring) dying
	// before it finishes despawning on its own — e.g. switched away from,
	// left defenseless while it winds down, and killed by the enemy in
	// the meantime. This can never be "the last of that side" (the hero
	// that's currently active, plus whichever roster slot wasn't just
	// switched from, are structurally still alive whenever a retiring
	// hero exists at all), so unlike forceSwitchOnDeath this never
	// force-switches or ends the match — it just marks the name
	// permanently eliminated and despawns the body. Calling
	// forceSwitchOnDeath here instead would incorrectly treat this like
	// the active hero dying: it'd force-switch to the other bench slot
	// (hijacking control away from whoever's actually being played) and
	// write the same eliminated name into both roster slots at once
	// (the one this hero already occupied, plus the one it just got
	// force-switched into) — surfacing as both switch buttons showing
	// the dead hero's portrait and being locked.
	void eliminateInactiveHero(CharacterBase* deadHero);
	// Plays the same "smk" (smoke) poof effect used when a hero retires on
	// a voluntary switch, then immediately removes the body. Needed
	// because enableReborn = false (set on every duel-mode hero — see
	// spawnAllyReplacement/onCharacterInit) means Hero::reborn()'s usual
	// cleanup path — which normally removes a dead hero's body once its
	// reborn countdown finishes — never runs at all, leaving the corpse
	// stuck on-screen forever under a frozen skull overlay that never
	// resolves.
	void despawnDeadHero(CharacterBase* deadHero);
	void processPendingDeaths(float dt);

	// Per-character HP persistence across switches, keyed by character
	// name. Saved (as an absolute value, against that character's own 2x
	// MaxHP) the moment a character leaves the active slot, and restored
	// instead of inheriting whatever percentage the outgoing character
	// happened to be at. A name with no entry yet means "hasn't been
	// played this match" -> spawns at full HP.
	unordered_map<string, uint32_t> _allyHpByName;
	unordered_map<string, uint32_t> _enemyAllyHpByName;

	// Shared "spawn and configure a new active hero" logic used by both a
	// voluntary switch and a death-triggered force-switch. Restores
	// nextName's persisted HP (see _allyHpByName/_enemyAllyHpByName above)
	// instead of inheriting a percentage from whoever's being replaced.
	// Doesn't touch the outgoing character at all — callers handle that
	// themselves (retiring for a voluntary switch; already mid-death and
	// self-cleaning-up for a death).
	Hero* spawnAllyReplacement(const string& nextName, Role role, Group grp, Vec2 spawnPos, int charId, bool isPlayerSide);

	uint32_t _second;
	uint32_t _minute;
	int mapId;

	const char* kName;
	const char* aName;
	int kEXPBound;
	int aEXPBound;

	bool _isAttackButtonRelease;
	bool _hasSpawnedGuardian;
	bool _enableGuardian = true;
	// int _guardianNum;
	vector<Flog*> _KonohaFlogArray;
	vector<Flog*> _AkatsukiFlogArray;
	vector<Tower*> _TowerArray;
	vector<Hero*> _CharacterArray;

	bool _isShacking;

	int _playNum;
	void checkBackgroundMusic(float dt);

	PROP(HudLayer*, _hudLayer, HudLayer);
	// BGLayer is a sibling in the scene, not a child of GameLayer, so it
	// doesn't move automatically when updateViewPoint() recenters GameLayer
	// for the duel arena map -- this reference lets that centering also
	// keep the background in sync instead of only shifting the TMX map,
	// characters, and coded borders while the background stays put.
	PROP(BGLayer*, _bgLayer, BgLayer);
	void onHUDInitialized(const OnHUDInitializedCallback& callback);
	bool isHUDInit();
	void setTowerState(int charId);

	PROP_UInt(totalKills, TotalKills);
	PROP_UInt(totalTime, TotalTime);

	SpriteBatchNode* skillEffectBatch;
	SpriteBatchNode* damageEffectBatch;
	SpriteBatchNode* bulletBatch;
	SpriteBatchNode* shadowBatch;

	bool init();
	void initTileMap();
	void initHeros();
	void initFlogs();
	void initTower();
	void initGard();
	void initEffects();

	void updateViewPoint(float dt);
	void updateGameTime(float dt);

	Hero* addHero(const HeroData& data, int charId);
	Hero* addHero(const string& name, Role role, Group group, Vec2 spawnPoint, int charNo);
	void addFlog(float dt);

	void attackButtonClick(ABType type);
	void gearButtonClick(GearType type);
	void attackButtonRelease();

	void JoyStickRelease();
	void JoyStickUpdate(Vec2 direction);

	PROP(bool, _isSkillFinish, SkillFinish);
	void checkTower();

	void onPause();
	void resumeFromPause();
	void onGear();
	void playGameOpeningAnimation(float dt);
	void onGameStart(float dt);
	void onGameOver(bool isWin);

	void updateHudSkillButtons();
	void setHPLose(float percent);
	void setEnemyHPLose(float percent);
	void setCKRLose(bool isCRK2);

	// --- Duel-mode (Boss/Deathmatch) shared chakra pool ---
	//
	// Outside duel modes, CKR/CKR2 live per-character on CharacterBase
	// (see PROP_UInt(_ckr, CKR) / PROP_UInt(_ckr2, CKR2)) and get fully
	// refilled whenever an ally switch brings a new hero in. In Boss and
	// Deathmatch, chakra is shared across a whole side instead -- the
	// player and all player allies draw from (and fill) one pool, and
	// the enemy + its allies share a separate one -- so it has to live
	// here on GameLayer, which outlives any single spawned Hero and is
	// reachable before a bench ally has even spawned in for the first
	// time. Ally switches in duel mode don't touch this at all; the new
	// hero just inherits whatever their side's pool currently reads.
	//
	// The pool is denominated in "bars" of 15000 each, capped at 7 bars
	// (105000). Skill 4 costs 1 bar (15000, same as normal mode); skill
	// 5 costs 3 bars (45000, vs. 25000 normally) since it's now drawing
	// from the same pool skill 4 does rather than a separate, smaller
	// one. The HUD's EXP bar/label (repurposed for duel modes, since
	// duel rosters are always level 6/maxed and the real EXP bar is
	// otherwise unused there) shows progress within the *current* bar
	// (pool % kDuelChakraBarSize) and the count of full bars banked so
	// far (pool / kDuelChakraBarSize) -- e.g. "1 BAR", "2 BARS" -- the
	// same way an EXP bar fills and wraps on level-up.
	static constexpr uint32_t kDuelChakraBarSize = 15000;
	static constexpr uint32_t kDuelChakraMaxBars = 7;
	static constexpr uint32_t kDuelChakraMax = kDuelChakraBarSize * kDuelChakraMaxBars; // 105000
	static constexpr uint32_t kDuelSkill4Cost = kDuelChakraBarSize;     // 1 bar  (15000)
	static constexpr uint32_t kDuelSkill5Cost = kDuelChakraBarSize * 3; // 3 bars (45000)

	// Hold-to-charge rate (ramen button / AI idle-charge, see
	// ActionButton::updateChakraCharge() and CharacterBase::checkRetri()):
	// a linear ramp from kChargeR0 to a rate that reaches kDuelChakraMax
	// (105000, full 7 bars) at exactly kChargeMaxT seconds of continuous
	// holding. Shared here so the player's touch-driven charge and the
	// AI's own idle-charge behavior use identical math.
	static constexpr float kChargeR0 = 5000.f;   // units/s at start of hold
	static constexpr float kChargeA  = 11911.f;  // acceleration (units/s²)
	static constexpr float kChargeMaxT = 3.8f;   // seconds to fill from empty

	uint32_t getDuelChakra(bool isPlayerSide) const { return _duelChakra[isPlayerSide ? 1 : 0]; }
	void setDuelChakra(bool isPlayerSide, uint32_t value);
	void addDuelChakra(bool isPlayerSide, uint32_t value);
	// Resyncs a just-spawned duel-mode hero's _isCanOugis1/2 flags (used
	// by AI to decide whether it can throw skill4/5) against their
	// side's current pool. Needed because those flags default to false
	// on construction and would otherwise stay wrong until the next
	// chakra gain happens to land on this specific character.
	void syncDuelOugisFlags(class CharacterBase* hero, bool isPlayerSide);

	void setReport(const string& slayer, const string& dead, uint32_t killNum);
	void clearDoubleClick();
	void resetStatusBar();
	void setCoin(const char* value);
	void removeOugisMark(int type);
	void setOugis(CharacterBase* sender);
	void removeOugis();

	CharacterBase* ougisChar;
	CharacterBase* controlChar;
	Layer* blend;

	void onLeft();

	bool _isSurrender;

	bool _enableGear;
	bool _isOugis2Game;
	bool _isHardCoreGame;
	bool _isRandomChar;

	Group playerGroup;
	bool _isStarted;
	bool _isExiting;

	// Deathmatch "next stage" quick restart -- set by GameOver's start_btn
	// before triggering the normal _isExiting teardown, so onLeft() skips
	// the usual "back to menu" Lua handoff and launches straight into a
	// fresh Deathmatch run with the same character(s) instead.
	bool _quickRestartDeathmatch = false;
	string _retainedPlayerChar;
	vector<string> _retainedAllyRoster;

	const char* getGuardianGroup()
	{
		return playerGroup == Group::Konoha ? TowerEnum::AkatsukiCenter : TowerEnum::KonohaCenter;
	}

	bool _isGear;
	bool _isPause;
	GearLayer* _gearLayer;

	void clearAllFlogsMainTarget(CharacterBase* target);
	void clearAllUnitsMainTarget(CharacterBase* target);

	CREATE_FUNC(GameLayer);
	static bool checkHasAnyMovement();
	static int getMapCount();
	static int getDuelMapCount();

#if (CC_TARGET_PLATFORM == CC_PLATFORM_LINUX || CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
	static void keyEventHandle(GLFWwindow* window, int key, int scancode, int action, int modes);
#elif (CC_TARGET_PLATFORM == CC_PLATFORM_MAC)
	static void keyEventHandle(int key, int keyState);
#endif

private:
	void onEnter();
	void onExit();

	void setKeyEventHandler();
	void removeKeyEventHandler();

#if (CC_TARGET_PLATFORM == CC_PLATFORM_LINUX || CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
	int _lastPressedMovementKey;
#elif (CC_TARGET_PLATFORM == CC_PLATFORM_MAC)
	int _lastPressedMovementKey;
#endif
#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
	static void LPFN_ACCELEROMETER_KEYHOOK(UINT message, WPARAM wParam, LPARAM lParam);
#endif

	void invokeAllCallbacks();

	inline Vec2 getCustomSpawnPoint(HeroData& data);

	bool isHUDInitialized = false;
	bool is4V4Mode = false;
	vector<OnHUDInitializedCallback> callbackssList;

	std::unique_ptr<BattleRuntimeSystem> _battleRuntimeSystem;
	std::unique_ptr<SpawnSystem> _spawnSystem;
	std::unique_ptr<SessionState> _sessionState;
};

#define BIND(funcName) std::bind(&funcName, this)
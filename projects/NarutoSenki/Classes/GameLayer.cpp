#include "Defines.h"
#include "CharacterBase.h"
#include "GameLayer.h"
#include "BGLayer.h"
#include "HudLayer.h"
#include "StartMenu.h"
#include "Core/Provider.hpp"
#include "GameMode/GameModeImpl.h"
#include "Constants/UiFlowKeys.hpp"
#include "Systems/BattleRuntimeSystem.hpp"
#include "Systems/SpawnSystem.hpp"
#include "Systems/SessionState.hpp"
#include "Utils/Cocos2dxHelper.hpp"
#include <algorithm>

GameLayer* _gLayer = nullptr;
bool _isFullScreen = false;

void BattleRuntimeSystem::onGameStart(GameLayer* layer, bool skipInitFlogs, float flogSpawnDuration) const
{
	if (!layer)
		return;

	layer->_isStarted = true;
	layer->getHudLayer()->openingSprite->removeFromParent();
	layer->getHudLayer()->openingSprite = nullptr;
	layer->schedule(schedule_selector(GameLayer::updateGameTime), 1.0f);
	layer->schedule(schedule_selector(GameLayer::checkBackgroundMusic), 2.0f);
	if (!skipInitFlogs)
	{
		layer->schedule(schedule_selector(GameLayer::addFlog), flogSpawnDuration);
		layer->initFlogs();
		layer->addFlog(0);
	}

	layer->setKeyEventHandler();
	for (auto hero : layer->_CharacterArray)
	{
		hero->setWalkSpeed(hero->_originSpeed);
		if (hero->isCom())
			hero->doAI();
	}
}

void BattleRuntimeSystem::updateGameTime(GameLayer* layer) const
{
	if (!layer)
		return;

	layer->_second += 1;
	if (layer->_second == 60)
	{
		layer->_minute += 1;
		layer->_second = 0;
	}
	// Deathmatch (endless arcade) repurposes the clock label to show
	// "Stage X" instead — the handler sets that text itself, so don't
	// stomp it with the normal MM:SS format here.
	if (getGameMode() != GameMode::Deathmatch)
	{
		auto tempTime = format("{:02d}:{:02d}", layer->_minute, layer->_second);
		layer->getHudLayer()->gameClock->setString(tempTime.c_str());
	}
	layer->setTotalTime(layer->getTotalTime() + 1);

	layer->updateEnemyAllySwitch(1.0f);
}

void BattleRuntimeSystem::updateViewPoint(GameLayer* layer) const
{
	if (!layer || !layer->currentPlayer)
		return;

	Vec2 playerPoint;
	if (layer->ougisChar)
		playerPoint = layer->ougisChar->getPosition();
	else if (layer->controlChar)
		playerPoint = layer->controlChar->getPosition();
	else
		playerPoint = layer->currentPlayer->getPosition();

	if (isDuelMode())
	{
		float tileWidth = layer->currentMap->getTileSize().width;
		float tileHeight = layer->currentMap->getTileSize().height;

		float mapPixelWidth = layer->currentMap->getMapSize().width * tileWidth;
		float mapPixelHeight = layer->currentMap->getMapSize().height * tileHeight;

		float mapCenterX = mapPixelWidth / 2.0f;
		float mapCenterY = mapPixelHeight / 2.0f;
		float cameraX = winSize.width / 2.0f - mapCenterX;
		float cameraY = winSize.height / 2.0f - mapCenterY;

		layer->setPosition(Vec2(cameraX, cameraY));
	}
	else
	{
		int x = MAX(playerPoint.x, winSize.width / 2);
		int y = MAX(playerPoint.y, winSize.width / 2);
		x = MIN(x, (layer->currentMap->getMapSize().width * layer->currentMap->getTileSize().width) - winSize.width / 2);
		y = MIN(y, (layer->currentMap->getMapSize().height * layer->currentMap->getTileSize().height) - winSize.height / 2);
		layer->setPosition(Vec2(winSize.width / 2, y) - Vec2(x, y));
	}
}

void SpawnSystem::initMatchUnits(GameLayer* layer) const
{
	if (!layer)
		return;
	layer->initTileMap();
	if (!layer->currentMap)
		return;
	layer->initEffects();
}

GameLayer::GameLayer()
{
	_battleRuntimeSystem = std::make_unique<BattleRuntimeSystem>();
	_spawnSystem = std::make_unique<SpawnSystem>();
	_sessionState = std::make_unique<SessionState>();

	mapId = 0;

	_isAttackButtonRelease = true;
	_isSkillFinish = true;

	_second = 0;
	_minute = 0;
	_playNum = 2;

	kEXPBound = 25;
	aEXPBound = 25;

	_isShacking = false;
	_isSurrender = false;
	_hasSpawnedGuardian = false;

	_isStarted = false;
	_isExiting = false;

	ougisChar = nullptr;
	controlChar = nullptr;

	_enableGear = true;
	_isOugis2Game = false;
	_isHardCoreGame = false;
	_isRandomChar = false;

	currentPlayer = nullptr;

	_isGear = false;
	_isPause = false;

#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
	_lastPressedMovementKey = -100;
#elif (CC_TARGET_PLATFORM == CC_PLATFORM_LINUX)
	_lastPressedMovementKey = -100;
	_window = GLView::sharedOpenGLView()->m_window;
#elif (CC_TARGET_PLATFORM == CC_PLATFORM_MAC)
	_lastPressedMovementKey = -100;
#endif
}

GameLayer::~GameLayer()
{
	_gLayer = nullptr;
	removeKeyEventHandler();

	// See the comment on _currentPlayerIsExtraRetained's declaration.
	if (_currentPlayerIsExtraRetained && currentPlayer)
		currentPlayer->release();
}

bool GameLayer::init()
{
	Texture2D::setDefaultAlphaPixelFormat(kCCTexture2DPixelFormat_RGBA8888);
	setTouchEnabled(true);

	_gLayer = this;
	const auto& gd = getGameModeHandler()->gd;
	_enableGear = gd.enableGear;
	_isHardCoreGame = gd.isHardCore;
	_isRandomChar = gd.isRandomChar;
	is4V4Mode = gd.use4v4SpawnLayout;
	playerGroup = gd.playerGroup;

	return Layer::init();
}

void GameLayer::onEnter()
{
	if (_isExiting)
	{
		onLeft();
		return;
	}

	if (currentPlayer && !ougisChar)
	{
		if (currentPlayer->getState() == State::WALK)
		{
			currentPlayer->idle();
		}
	}

	Layer::onEnter();

	if (_isSurrender)
	{
		getGameModeHandler()->onSurrender();
		onGameOver(false);
	}
}

void GameLayer::onExit()
{
	Layer::onExit();

	if (_isExiting)
	{
		_isExiting = false;
	}
}

void GameLayer::onHUDInitialized(const OnHUDInitializedCallback& callback)
{
	callbackssList.push_back(callback);
}

bool GameLayer::isHUDInit()
{
	return isHUDInitialized;
}

void GameLayer::initTileMap()
{
	setRand();
	if (isDuelMode())
	{
		// Duel maps live in their own subfolder (Resources/Maps/Duels/),
		// numbered independently starting at 1 — adding more is just a
		// matter of dropping another {N}.tmx in there, getDuelMapCount()
		// picks it up automatically.
		int duelMapCount = getDuelMapCount();
		if (duelMapCount == 0)
		{
			CCMessageBox("Not found any duel map", "[Error] Not found any duel map");
			return;
		}
		mapId = random(duelMapCount) + 1;
		currentMap = TMXTiledMap::create(GetDuelMapPath(mapId));
	}
	else
	{
		int mapCount = getMapCount();
		if (mapCount == 0)
		{
			CCMessageBox("Not found any map", "[Error] Not found any map");
			return;
		}
		// The old duel map used to live here as 6.tmx, which the "roll == 6"
		// exclusion below existed to keep out of the regular rotation —
		// that map's moved to Maps/Duels/ now, so a 6.tmx here (if one ever
		// exists again) is just an ordinary map like any other.
		mapId = random(mapCount) + 1;
		currentMap = TMXTiledMap::create(GetMapPath(mapId));
	}
	addChild(currentMap, kMapOrder);
}

void GameLayer::initGard()
{
	if (!_enableGuardian)
		return;
	setRand();
	int index = random(2);
	auto guardianName = index == 0 ? GuardianEnum::Roshi : GuardianEnum::Han;
	auto guardianGroup = playerGroup == Group::Konoha ? Group::Akatsuki : Group::Konoha;
	auto guardian = Provider::create(guardianName, Role::Com, guardianGroup);

	if (playerGroup == Group::Konoha)
	{
		guardian->setPosition(Vec2(2800, 80));
		guardian->setSpawnPoint(Vec2(2800, 80));
	}
	else
	{
		guardian->setPosition(Vec2(272, 80));
		guardian->setSpawnPoint(Vec2(272, 80));
	}

	addChild(guardian, -guardian->getPositionY());
	guardian->setLV(6);
	guardian->setHPbar();
	guardian->setShadows();
	guardian->setCharId(_CharacterArray.size() + 1);

	guardian->idle();
	guardian->setSkillEffect("smk");

	guardian->doAI();

	_CharacterArray.push_back(guardian);
	_hudLayer->addMapIcon();

	_hasSpawnedGuardian = true;
}

void GameLayer::initHeros()
{
	_spawnSystem->initMatchUnits(this);
	if (currentMap == nullptr)
	{
		// initTileMap() failed (e.g. no map asset bundled). The error has
		// already been surfaced to the user via CCMessageBox; bail out here
		// instead of dereferencing the null tilemap below.
		return;
	}

	addSprites("UI/hpBar/hpBar.plist");

	auto handler = getGameModeHandler();
	auto herosDataVector = handler->getHerosArray();

	_isOugis2Game = true;

	TMXObjectGroup* group = currentMap->objectGroupNamed("object");
	if (group == nullptr)
	{
		CCMessageBox("Map is missing the 'object' layer", "[Error] Bad map");
		return;
	}
	CCArray* objectArray = group->getObjects();

	// 4v4 spawn layout
	if (is4V4Mode)
	{
		auto& hero1 = herosDataVector.at(0);
		auto& hero5 = herosDataVector.at(4);

		hero1.setSpawnPoint(getCustomSpawnPoint(hero1));
		addHero(hero1, 1);

		hero5.setSpawnPoint(getCustomSpawnPoint(hero5));
		addHero(hero5, 5);
	}

	int i = 0;
	for (auto& data : herosDataVector)
	{
		if (data.isInit)
			continue;

		int mapPos = i;
		if (data.group == Group::Akatsuki)
		{
			if (mapPos <= MapPosCount - 1)
				mapPos += MapPosCount;
		}
		else
		{
			if (mapPos > MapPosCount - 1)
				mapPos -= MapPosCount;
		}

		Ref* mapObject = objectArray->objectAtIndex(mapPos);
		auto mapdict = (CCDictionary*)mapObject;
		int x = ((CCString*)mapdict->objectForKey("x"))->intValue();
		int y = ((CCString*)mapdict->objectForKey("y"))->intValue();
		data.setSpawnPoint(Vec2(x, y));

		if (is4V4Mode)
		{
			int id = i + 2;
			if (id >= 5)
				id++;
			addHero(data, id);
		}
		else
		{
			addHero(data, i + 1);
		}
		i++;
	}

	// Duel modes: spawn the rest of both rosters now (benched -- invisible,
	// invulnerable, no AI), so switching later never has to spawn anything.
	if (isDuelMode())
		spawnBenchedAllies();

	// Tower HP bar color depends on currentPlayer group, so towers must be
	// initialized after at least one hero/player is created.
	if (!isDuelMode())
		initTower();

	schedule(schedule_selector(GameLayer::updateViewPoint), 0.00f);
	scheduleOnce(schedule_selector(GameLayer::playGameOpeningAnimation), 0.5f);
}

Hero* GameLayer::addHero(const HeroData& data, int charId)
{
	return addHero(data.name, data.role, data.group, data.spawnPoint, charId);
}

Hero* GameLayer::addHero(const string& name, Role role, Group group, Vec2 spawnPoint, int charId)
{
	auto hero = Provider::create(name, role, group);
	if (hero->isPlayer())
	{
		currentPlayer = hero;
	}
	hero->setPosition(spawnPoint);
	hero->setSpawnPoint(spawnPoint);
	// NOTE: Set all characters speed to zero. (Control movement before game real start)
	hero->setWalkSpeed(0);
	if (group == Group::Akatsuki)
	{
		hero->_isFlipped = true;
		hero->setFlipX(true);
	}
	hero->setHPbar();
	hero->setShadows();
	hero->idle();
	hero->setCharId(charId);
	if (!isDuelMode())
		hero->schedule(schedule_selector(CharacterBase::setRestore2), 1.0f);

	addChild(hero, -hero->getPositionY());
	_CharacterArray.push_back(hero);

	getGameModeHandler()->onCharacterInit(hero);
	return hero;
}

void GameLayer::playGameOpeningAnimation(float dt)
{
	getHudLayer()->playGameOpeningAnimation();

	setRand();
	auto path = random(2) == 0 ? "Audio/Menu/battle_start1.ogg" : "Audio/Menu/battle_start.ogg";
	SimpleAudioEngine::sharedEngine()->playEffect(path);

	scheduleOnce(schedule_selector(GameLayer::onGameStart), 0.75f);
}

void GameLayer::onGameStart(float dt)
{
	auto handler = getGameModeHandler();
	_battleRuntimeSystem->onGameStart(this, handler->skipInitFlogs, handler->flogSpawnDuration);

	getGameModeHandler()->onGameStart();
}

void GameLayer::initFlogs()
{
	addSprites("UI/hpBar/flogBar.plist");

	kName = FlogEnum::KotetsuFlog;
	aName = FlogEnum::FemalePainFlog;
}

void GameLayer::addFlog(float dt)
{
	auto KonohaFlogName = kName;
	auto AkatsukiFlogName = aName;

	int i;
	Flog* flog;
	float mainPosY;
	for (i = 0; i < kFlogCount; i++)
	{
		flog = Flog::create();
		flog->setID(KonohaFlogName, Role::Flog, Group::Konoha);
		if (i < kFlogCount / 2)
			mainPosY = (5.5 - i / 1.5) * 32;
		else
			mainPosY = (3.5 - i / 1.5) * 32;
		flog->_mainPosY = mainPosY;
		flog->setPosition(Vec2(13 * 32, flog->_mainPosY));
		flog->setHPbar();
		flog->idle();
		flog->doAI();
		_KonohaFlogArray.push_back(flog);
		addChild(flog, -int(flog->getPositionY()));
	}

	for (i = 0; i < kFlogCount; i++)
	{
		flog = Flog::create();
		flog->setID(AkatsukiFlogName, Role::Flog, Group::Akatsuki);
		if (i < kFlogCount / 2)
			mainPosY = (5.5 - i / 1.5) * 32;
		else
			mainPosY = (3.5 - i / 1.5) * 32;
		flog->_mainPosY = mainPosY;
		flog->setPosition(Vec2(83 * 32, flog->_mainPosY));
		flog->setHPbar();
		flog->idle();
		flog->doAI();
		_AkatsukiFlogArray.push_back(flog);
		addChild(flog, -flog->getPositionY());
	}
}

void GameLayer::initTower()
{
	addSprites(format("Unit/Tower/Tower{}.plist", mapId));

	TMXObjectGroup* metaGroup = currentMap->objectGroupNamed("meta");
	CCArray* metaArray = metaGroup->getObjects();
	Ref* pObject;
	int i = 0;

	CCARRAY_FOREACH(metaArray, pObject)
	{
		auto dict = (CCDictionary*)pObject;

		int metaX = ((CCString*)dict->objectForKey("x"))->intValue();
		int metaY = ((CCString*)dict->objectForKey("y"))->intValue();

		int metaWidth = ((CCString*)dict->objectForKey("width"))->intValue();
		int metaHeight = ((CCString*)dict->objectForKey("height"))->intValue();

		auto name = ((CCString*)dict->objectForKey("name"))->m_sString;

		Tower* tower = Tower::create();
		char towerName[7] = "abcdef";
		strncpy(towerName, name.c_str(), 6);
		if (is_same(towerName, kGroupKonoha))
		{
			tower->setID(name, Role::Tower, Group::Konoha);
		}
		else
		{
			tower->setID(name, Role::Tower, Group::Akatsuki);
			tower->setFlipX(true);
			tower->_isFlipped = true;
		}
		float posX = metaX + metaWidth / 2;
		float posY = metaY + metaHeight / 2;
		tower->setPosition(Vec2(posX, posY));
		tower->setSpawnPoint(Vec2(posX, posY));
		tower->setCharId(i + 1);

		if (i == 1 || i == 4)
		{
			if (is4V4Mode)
			{
				tower->setMaxHPValue(80000, false);
			}
			else
			{
				tower->setMaxHPValue(50000, false);
			}
			tower->setHPValue(tower->getMaxHP(), false);
		}
		tower->setHPbar();
		tower->_hpBar->setVisible(false);
		tower->idle();
		addChild(tower, -tower->getPositionY());

		_TowerArray.push_back(tower);
		i++;
	}
}

void GameLayer::initEffects()
{
	addSprites("Effects/SkillEffect.plist");
	skillEffectBatch = SpriteBatchNode::create("Effects/SkillEffect.png");
	addChild(skillEffectBatch, kSkillEffectOrder);

	addSprites("Effects/DamageEffect.plist");
	damageEffectBatch = SpriteBatchNode::create("Effects/DamageEffect.png");
	addChild(damageEffectBatch, kDamageEffectOrder);

	addSprites("Effects/Shadows.plist");
	shadowBatch = SpriteBatchNode::create("Effects/Shadows.png");
	addChild(shadowBatch, kShadowOrder);
}

void GameLayer::updateGameTime(float dt)
{
	_battleRuntimeSystem->updateGameTime(this);
}

void GameLayer::updateViewPoint(float dt)
{
	_battleRuntimeSystem->updateViewPoint(this);
}

void GameLayer::setTowerState(int charId)
{
	_hudLayer->setTowerState(charId);
}

void GameLayer::updateHudSkillButtons()
{
	_hudLayer->updateSkillButtons();
}

void GameLayer::setHPLose(float percent)
{
	_hudLayer->setHPLose(percent);
}

void GameLayer::setEnemyHPLose(float percent)
{
	_hudLayer->setEnemyHPLose(percent);
}

void GameLayer::setCKRLose(bool isCRK2)
{
	_hudLayer->setCKRLose(isCRK2);
}

void GameLayer::setDuelChakra(bool isPlayerSide, uint32_t value)
{
	_duelChakra[isPlayerSide ? 1 : 0] = (std::min)(value, kDuelChakraMax);

	if (_hudLayer)
	{
		_hudLayer->setDuelChakraBarUI(isPlayerSide);

		// Refresh the skill4/skill5 button dials too -- setCKRLose() is
		// their only refresh trigger (see ActionButton::updateProgressMark),
		// and every duel-chakra mutation (combat gains, AI/player casts,
		// the ramen hold-charge) funnels through this one function, so
		// hooking it here covers all of them without needing every call
		// site to separately remember to call setCKRLose(). Only the
		// player's own bank drives their own on-screen buttons.
		if (isPlayerSide)
		{
			_hudLayer->setCKRLose(false);
			_hudLayer->setCKRLose(true);
		}
	}
}

void GameLayer::addDuelChakra(bool isPlayerSide, uint32_t value)
{
	setDuelChakra(isPlayerSide, getDuelChakra(isPlayerSide) + value);
}

void GameLayer::syncDuelOugisFlags(CharacterBase* hero, bool isPlayerSide)
{
	if (!hero)
		return;

	uint32_t pool = getDuelChakra(isPlayerSide);
	hero->_isCanOugis1 = pool >= kDuelSkill4Cost;
	hero->_isCanOugis2 = pool >= kDuelSkill5Cost;
}

void GameLayer::setReport(const string& slayer, const string& dead, uint32_t killNum)
{
	_hudLayer->setReport(slayer, dead, killNum);
}

void GameLayer::resetStatusBar()
{
	_hudLayer->status_hpbar->setRotation(0);
}

void GameLayer::setCoin(const char* value)
{
	_hudLayer->setCoin(value);
}

void GameLayer::removeOugisMark(int type)
{
	if (type == 1)
	{
		if (_hudLayer->skill4Button)
		{
			if (_hudLayer->skill4Button->lockLabel1)
			{
				_hudLayer->skill4Button->lockLabel1->removeFromParent();
				_hudLayer->skill4Button->lockLabel1 = nullptr;
			}
		}
	}
	else
	{
		if (_hudLayer->skill5Button)
		{
			if (_hudLayer->skill5Button->lockLabel1)
			{
				_hudLayer->skill5Button->lockLabel1->removeFromParent();
				_hudLayer->skill5Button->lockLabel1 = nullptr;
			}
		}
	}
}

void GameLayer::checkTower()
{
	int konohaTowerCount = 0;
	int akatsukiTowerCount = 0;

	for (auto tower : _TowerArray)
	{
		if (tower->isKonohaGroup())
			konohaTowerCount++;
		else
			akatsukiTowerCount++;
	}

	if (konohaTowerCount == 2)
	{
		aName = FlogEnum::PainFlog;
		kEXPBound = 50;
	}
	else if (konohaTowerCount == 1)
	{
		aName = FlogEnum::ObitoFlog;
		kEXPBound = 100;
	}

	if (akatsukiTowerCount == 2)
	{
		kName = FlogEnum::IzumoFlog;
		aEXPBound = 50;
	}
	else if (akatsukiTowerCount == 1)
	{
		kName = FlogEnum::KakashiFlog;
		aEXPBound = 100;
	}

	for (auto hero : getGameLayer()->_CharacterArray)
	{
		if (hero->isNotCom())
			continue;

		if (hero->isKonohaGroup())
		{
			hero->battleCondiction = konohaTowerCount - akatsukiTowerCount;
			if (konohaTowerCount == 1)
			{
				hero->isBaseDanger = true;
			}
		}
		else
		{
			hero->battleCondiction = akatsukiTowerCount - konohaTowerCount;
			if (_isHardCoreGame)
			{
				if (akatsukiTowerCount == 1)
				{
					hero->isBaseDanger = true;
				}
			}
		}
	}

	if (konohaTowerCount == 0 || akatsukiTowerCount == 0)
	{
		if (playerGroup == Group::Konoha)
			onGameOver(konohaTowerCount != 0);
		else
			onGameOver(akatsukiTowerCount != 0);
	}
}

void GameLayer::clearDoubleClick()
{
	if (_hudLayer->skill1Button->getDoubleSkill() &&
		_hudLayer->skill1Button->_clickNum >= 1)
	{
		_hudLayer->skill1Button->setFreezeAction(nullptr);
		_hudLayer->skill1Button->beganAnimation();
	}
}

void GameLayer::JoyStickRelease()
{
	if (getHudLayer()->_isAllButtonLocked)  // add this guard
		return;
	if (currentPlayer->getState() == State::WALK)
		currentPlayer->idle();
}

void GameLayer::JoyStickUpdate(Vec2 direction)
{
	if (getHudLayer()->_isAllButtonLocked)  // add this guard
		return;
	if (!ougisChar)
		currentPlayer->walk(direction);
}

void GameLayer::attackButtonClick(ABType type)
{
	if (type == NAttack)
	{
		_isAttackButtonRelease = false;
	}

	if (type == AllySwitch1)
	{
		switchToAllySlot(0);
	}
	else if (type == AllySwitch2)
	{
		switchToAllySlot(1);
	}
	else if (type == Item1)
	{
		currentPlayer->setItem(type);
	}
	else
	{
		currentPlayer->attack(type);
	}

	// Each slot's own recharge (setCD(15000) at creation, ~15s) is already
	// independent per-button — allySwitch1Button/allySwitch2Button are
	// separate ActionButton instances with their own _cooldown/_timeCount.
	// This is a second, much shorter cooldown layered on top: switching
	// through EITHER slot puts BOTH buttons on a brief 0.5s lock, so you
	// can't immediately chain a second switch the instant the first one
	// registers.
	//
	// This used to poke setTimeCount() directly on the button that wasn't
	// clicked, which is what caused it to get stuck locked forever:
	// updateCDLabel (the only thing that ever counts _timeCount back down)
	// only ever gets scheduled from inside beganAnimation(), so a button
	// that never went through beganAnimation() had nothing left to bring
	// its _timeCount back to 0. applyMinCooldown sets the floor and makes
	// sure that schedule actually starts.
	if (type == AllySwitch1 || type == AllySwitch2)
	{
		auto hud = getHudLayer();
		if (hud->allySwitch1Button)
			hud->allySwitch1Button->applyMinCooldown(500);
		if (hud->allySwitch2Button)
			hud->allySwitch2Button->applyMinCooldown(500);
	}
}

// ---------------------------------------------------------------------------
// Duel-mode persistent roster -- see the comment on _allyHeroByKey in
// GameLayer.h. Every roster hero is spawned once at match start and never
// despawned by a switch; switching only toggles bench/active state.
// ---------------------------------------------------------------------------

string GameLayer::getRosterKey(Hero* hero, bool isPlayerSide)
{
	auto& heroMap = isPlayerSide ? _allyHeroByKey : _enemyHeroByKey;
	for (auto& kv : heroMap)
	{
		if (kv.second == hero)
			return kv.first;
	}
	return hero ? hero->getName() : string();
}

Hero* GameLayer::getActiveHero(bool isPlayerSide)
{
	auto& heroMap = isPlayerSide ? _allyHeroByKey : _enemyHeroByKey;
	for (auto& kv : heroMap)
	{
		Hero* hero = kv.second;
		if (hero && !hero->_isBenched && hero->getState() != State::DEAD)
			return hero;
	}
	return nullptr;
}

void GameLayer::initBenchedHeroStats(Hero* hero, bool isPlayerSide)
{
	// Same loadout the active heroes get in Boss/Deathmatch's
	// onHUDInitialized callback (level 6, 2x HP, no auto-reborn), minus the
	// chakra grant: chakra is one shared pool per side, so it's only added
	// once, by the active hero.
	hero->setCoin(3000);
	hero->setEXP(2500);
	for (int i = 1; i < 6; i++)
		hero->changeHPbar();

	hero->setCKR2(0);
	hero->_isCanOugis2 = false;

	uint32_t newMaxHP = hero->getMaxHP() * 2;
	hero->setMaxHPValue(newMaxHP, false);
	hero->setHPValue(newMaxHP, true);

	hero->enableReborn = false;

	syncDuelOugisFlags(hero, isPlayerSide);
}

void GameLayer::spawnBenchedAllies()
{
	if (!isDuelMode() || !currentPlayer)
		return;

	_allyHeroByKey.clear();
	_enemyHeroByKey.clear();
	_benchedHeroes.clear();

	Group playerGrp = currentPlayer->getGroup();
	Group enemyGrp = (playerGrp == Group::Konoha) ? Group::Akatsuki : Group::Konoha;

	// Register the two already-spawned active heroes.
	Hero* playerHero = nullptr;
	Hero* enemyHero = nullptr;
	for (auto hero : _CharacterArray)
	{
		if (hero == currentPlayer)
			playerHero = hero;
		else if (!enemyHero && hero->isCom() && hero->getGroup() == enemyGrp && hero->isNotGuardian())
			enemyHero = hero;
	}
	if (playerHero)
		_allyHeroByKey[playerHero->getName()] = playerHero;
	if (enemyHero)
		_enemyHeroByKey[enemyHero->getName()] = enemyHero;

	// Spawn every bench member, on the spot of their side's active hero (they
	// get teleported to wherever the hero they replace stands when they're
	// activated). Distinct charIds so the minimap can tell them apart.
	vector<pair<Hero*, bool>> spawned;
	int nextCharId = 100;

	auto spawnSide = [&](const vector<string>& roster, Hero* anchor, Group grp, bool isPlayerSide)
	{
		if (!anchor)
			return;
		for (const auto& name : roster)
		{
			if (name.empty())
				continue;
			Hero* benched = addHero(name, Role::Com, grp, anchor->getPosition(), nextCharId++);
			if (!benched)
				continue;
			benched->enableReborn = false;
			// Setup (level/HP) needs the HUD, so it's deferred below. Bench it
			// right away though so it's never visible/targetable/AI-driven,
			// not even for a frame.
			benched->enterBench();
			(isPlayerSide ? _allyHeroByKey : _enemyHeroByKey)[name] = benched;
			_benchedHeroes.push_back(benched);
			spawned.push_back({ benched, isPlayerSide });
		}
	};
	spawnSide(_allyRoster, playerHero, playerGrp, true);
	spawnSide(_enemyAllyRoster, enemyHero, enemyGrp, false);

	// Benched heroes stay in _CharacterArray until the HUD is up: its minimap
	// builds one icon per hero in that array (hidden for benched ones, see
	// HudLayer), and level/HP setup needs the HUD. Right after that they
	// leave the array for good, which is what makes every "loop over all
	// heroes" targeting/AoE/buff routine ignore them.
	onHUDInitialized(
		[this, spawned]()
		{
			for (auto& entry : spawned)
			{
				initBenchedHeroStats(entry.first, entry.second);
				std::erase(_CharacterArray, entry.first);
			}
		});
}

void GameLayer::benchHero(Hero* hero)
{
	if (!hero)
		return;
	hero->enterBench();
	std::erase(_CharacterArray, hero);
	_benchedHeroes.push_back(hero);
}

// Brings `hero` (currently benched) onto the field at `pos`, facing
// `flipped` -- i.e. where the hero it replaces was standing (a switched-out
// hero, or one that just died). Control (player side) or AI (enemy side) is
// handed over. Nothing is spawned or despawned here.
void GameLayer::activateHero(Hero* hero, const Vec2& pos, bool flipped, bool isPlayerSide)
{
	if (!hero)
		return;

	std::erase(_benchedHeroes, hero);
	hero->leaveBench(pos, flipped);
	if (std::find(_CharacterArray.begin(), _CharacterArray.end(), hero) == _CharacterArray.end())
		_CharacterArray.push_back(hero);

	// Walk speed is 0 until game start for every hero that was in the array
	// then; benched heroes were already out of it.
	hero->setWalkSpeed(hero->_originSpeed);
	hero->enableReborn = false;

	// Chakra is a shared pool, so just resync which ultimates this hero's
	// AI/UI considers available.
	syncDuelOugisFlags(hero, isPlayerSide);

	if (isPlayerSide)
	{
		hero->setRole(Role::Player);
		hero->_isAI = false;
		currentPlayer = hero;

		// The HP bar sprite was chosen by role when the hero spawned
		// (benched allies got the ally-AI one); rebuild it for the player.
		if (hero->_hpBar)
		{
			hero->_hpBar->removeFromParent();
			hero->_hpBar = nullptr;
		}
		hero->setHPbar();
		hero->setHPValue(hero->getHP(), true);
	}
	else
	{
		hero->setRole(Role::Com);
		hero->doAI();
	}
}

bool GameLayer::isRosterNameEliminated(const string& name, bool isPlayerSide)
{
	auto& eliminated = isPlayerSide ? _eliminatedAllies : _eliminatedEnemyAllies;
	return eliminated.find(name) != eliminated.end();
}

// Updates the player-side HUD for a newly controlled hero. Shared by a
// voluntary switch and a death-triggered force-switch.
static void refreshPlayerHudForHero(GameLayer* gl, Hero* newHero, const string& newKey)
{
	// The outgoing hero may have been mid-skill when it was benched; its
	// idle() (which normally flips this back) ran as a non-player, so make
	// sure the HUD isn't left waiting on a skill that will never "finish".
	gl->setSkillFinish(true);

	auto hud = gl->getHudLayer();
	hud->setEXPLose();
	hud->coinLabel->setString(to_cstr(newHero->getCoin()));
	if (!newHero->isEnableSkill04())
		hud->skill4Button->setLock();
	if (!newHero->isEnableSkill05())
		hud->skill5Button->setLock();

	hud->initGearButton(newKey);
	hud->updateSkillButtons();
	hud->resetSkillButtons();

	// newHero's HP lives on the hero itself -- reflect it on the HUD bar.
	gl->setHPLose(newHero->getHpPercent());
}

void GameLayer::switchToAllySlot(int slotIndex)
{
	if (slotIndex < 0 || slotIndex >= (int)_allyRoster.size() || !currentPlayer || currentPlayer->getState() == State::DEAD)
		return;

	string nextKey = _allyRoster[slotIndex];

	// Can't switch into an eliminated slot -- the HUD button should already
	// refuse this click (see HudLayer::updateAllySwitchButtons /
	// ActionButton::isCanClick), but guard the actual swap logic too.
	if (isRosterNameEliminated(nextKey, true))
		return;

	auto it = _allyHeroByKey.find(nextKey);
	if (it == _allyHeroByKey.end() || !it->second || it->second->getState() == State::DEAD)
		return;

	Hero* newHero = it->second;
	Hero* oldHero = (Hero*)currentPlayer;
	if (newHero == oldHero)
		return;

	string oldKey = getRosterKey(oldHero, true);

	oldHero->setSkillEffect("smk");

	// Nothing is despawned: the outgoing hero (and its summons/puppets) just
	// goes invisible, invulnerable, AI-less and frozen on the bench; the
	// incoming one is un-benched where the outgoing hero stood, and gets
	// control. HP, buffs, etc. simply stay on each hero object.
	//
	// Order matters: activate first (it reads oldHero's position/facing),
	// bench after.
	activateHero(newHero, oldHero->getPosition(), oldHero->_isFlipped, true);
	benchHero(oldHero);

	_allyRoster[slotIndex] = oldKey;

	refreshPlayerHudForHero(this, newHero, nextKey);

	ActionButton* swappedButton = (slotIndex == 0) ? _hudLayer->allySwitch1Button : _hudLayer->allySwitch2Button;
	if (swappedButton)
	{
		auto newFrame = getSpriteFrame("{}_rp.png", oldKey);
		if (newFrame)
			swappedButton->setDisplayFrame(newFrame);
		// Only the slot actually used gets the full ~15s recharge here --
		// the other slot's separate 0.5s "just switched" lock is applied in
		// attackButtonClick instead.
		swappedButton->beganAnimation();
	}
}


// AI-side mirror of switchToAllySlot above, operating on the enemy hero
// and _enemyAllyRoster instead of the player. Kept separate rather than
// unified with a parameter, since the two sides update different HUD
// elements (skill/gear buttons vs. the cosmetic enemy icons) and take
// control as different Roles (Player vs Com + AI).
void GameLayer::switchEnemyAllySlot(int slotIndex)
{
	if (slotIndex < 0 || slotIndex >= (int)_enemyAllyRoster.size())
		return;

	string nextKey = _enemyAllyRoster[slotIndex];

	// Can't switch into an eliminated slot -- mirrors the player-side guard.
	if (isRosterNameEliminated(nextKey, false))
		return;

	auto it = _enemyHeroByKey.find(nextKey);
	if (it == _enemyHeroByKey.end() || !it->second || it->second->getState() == State::DEAD)
		return;

	Hero* oldHero = getActiveHero(false);
	Hero* newHero = it->second;
	if (!oldHero || newHero == oldHero)
		return;

	string oldKey = getRosterKey(oldHero, false);

	oldHero->setSkillEffect("smk");

	activateHero(newHero, oldHero->getPosition(), oldHero->_isFlipped, false);
	benchHero(oldHero);

	_enemyAllyRoster[slotIndex] = oldKey;

	_hudLayer->refreshEnemyAvatar(nextKey);
	setEnemyHPLose(newHero->getHpPercent());

	Sprite* swappedIcon = (slotIndex == 0) ? _hudLayer->enemyAllyIcon1 : _hudLayer->enemyAllyIcon2;
	if (swappedIcon)
	{
		auto newFrame = getSpriteFrame("{}_rp.png", oldKey);
		if (newFrame)
			swappedIcon->setDisplayFrame(newFrame);
	}
}

// See the comment on the declaration in GameLayer.h for why this queues
// the actual work instead of doing it synchronously.
// See the comment on the declaration in GameLayer.h for why this exists.
void GameLayer::despawnDeadHero(CharacterBase* deadHero)
{
	if (!deadHero)
		return;

	deadHero->setSkillEffect("smk");

	// Dependent cleanup for a dying owner -- a
	// summon/puppet still alive when its owner dies (e.g. Chiyo dying with
	// Parents still up) would otherwise be left with a dangling master,
	// the same class of bug fixed for Sai's Ink Dragon and Sasuke's
	// Amaterasu traps, just triggered by death here instead of a switch.
	for (auto it = _CharacterArray.begin(); it != _CharacterArray.end();)
	{
		Hero* dependent = *it;
		if (dependent != deadHero && (dependent->getMaster() == deadHero || dependent->getSecMaster() == deadHero))
		{
			CCNotificationCenter::sharedNotificationCenter()->removeObserver(dependent, "acceptAttack");
			dependent->unscheduleAllSelectors();
			dependent->stopAllActions();
			if (dependent->_shadow)
				dependent->_shadow->removeFromParent();
			dependent->removeFromParent();
			it = _CharacterArray.erase(it);
		}
		else
		{
			++it;
		}
	}

	if (deadHero->hasMonsterArrayAny())
	{
		for (auto mo : deadHero->getMonsterArray())
		{
			CCNotificationCenter::sharedNotificationCenter()->removeObserver(mo, "acceptAttack");
			mo->unscheduleAllSelectors();
			mo->stopAllActions();
			if (mo->_shadow)
				mo->_shadow->removeFromParent();
			mo->removeFromParent();
		}
		deadHero->getMonsterArray().clear();
	}

	std::erase(_CharacterArray, (Hero*)deadHero);

	// unscheduleAllSelectors() here is what actually prevents the stuck
	// corpse: it cancels the scheduleOnce(Hero::reborn, ...) and
	// schedule(Hero::countDown, 1) that dealloc() already set up for this
	// hero (the skull-overlay/reborn-countdown machinery), which would
	// otherwise run down to 0 and then hit reborn()'s "if (!enableReborn)
	// return;" early-out — reborn() doing nothing at that point is exactly
	// what left the body stuck on-screen forever.
	CCNotificationCenter::sharedNotificationCenter()->removeObserver(deadHero, "acceptAttack");
	deadHero->unscheduleAllSelectors();
	deadHero->stopAllActions();
	if (deadHero->_shadow)
		deadHero->_shadow->removeFromParent();

	deadHero->removeFromParent();
}

void GameLayer::forceSwitchOnDeath(CharacterBase* deadHero)
{
	PendingDeathInfo info;
	info.name = deadHero->getName();
	info.group = deadHero->getGroup();
	info.position = deadHero->getPosition();
	info.charId = deadHero->getCharId();
	info.hero = deadHero;
	_pendingDeaths.push_back(info);

	scheduleOnce(schedule_selector(GameLayer::processPendingDeaths), 0.0f);
}

void GameLayer::processPendingDeaths(float dt)
{
	// Snapshot and clear up front -- onGameOver() below can trigger a
	// teardown that ends up touching this vector again, and a stray
	// double-death in the same frame (both duelists going down at once,
	// e.g. a mutual-kill hit) shouldn't get processed twice.
	auto pending = _pendingDeaths;
	_pendingDeaths.clear();

	for (auto& info : pending)
	{
		bool isPlayerSide = info.group == playerGroup;
		auto& eliminated = isPlayerSide ? _eliminatedAllies : _eliminatedEnemyAllies;
		auto& roster = isPlayerSide ? _allyRoster : _enemyAllyRoster;
		auto& heroMap = isPlayerSide ? _allyHeroByKey : _enemyHeroByKey;

		// Roster key of the hero that just died (stable even if it had
		// transformed -- getName() might be "SageNaruto" by now).
		string deadKey = getRosterKey((Hero*)info.hero, isPlayerSide);

		eliminated.insert(deadKey);
		// The dead hero is the one thing that does get despawned, so drop
		// it from the roster map before the pointer goes away.
		heroMap.erase(deadKey);

		int nextSlot = -1;
		for (int i = 0; i < (int)roster.size(); i++)
		{
			if (!isRosterNameEliminated(roster[i], isPlayerSide))
			{
				nextSlot = i;
				break;
			}
		}

		bool isFinalDeath = nextSlot < 0;
		bool isCurrentPlayer = info.hero == currentPlayer;

		// currentPlayer is a raw pointer, and despawnDeadHero() below
		// calls removeFromParent() on the dying hero -- which, if nothing
		// else is holding a reference, can actually deallocate the C++
		// object rather than just hide it (unlike a normal death, where
		// the object stays alive under the reborn/skull-overlay machinery
		// this whole system bypasses).
		//
		// In the normal force-switch case (isFinalDeath == false) that's
		// fine to just null out -- activateHero() below reassigns
		// currentPlayer to the newly active hero.
		//
		// On the player's actual final death though, nothing else ever
		// reassigns currentPlayer afterward, and GameOver's result screen
		// (GameOver::listResult(), scheduled ~0.2s after GameOver::init())
		// reads currentPlayer's *live* stats (kill count, group, etc, not
		// just its name) to build the screen -- a name-only fallback can't
		// cover that. So instead of nulling it, keep the object itself
		// alive with an extra retain() (released in ~GameLayer(), see
		// _currentPlayerIsExtraRetained) -- it's still fully removed from
		// the scene/inert either way, this only affects whether the
		// pointer stays valid for GameOver to read from afterward.
		bool deadFlipped = info.hero->_isFlipped; // read before despawn may free it
		bool keepAlive = isFinalDeath && isPlayerSide && isCurrentPlayer;
		if (keepAlive)
		{
			info.hero->retain();
			_currentPlayerIsExtraRetained = true;
		}
		else if (isCurrentPlayer)
		{
			currentPlayer = nullptr;
		}

		despawnDeadHero(info.hero);

		if (isFinalDeath)
		{
			// Every one of this side's 3 characters is down now -- the
			// match actually ends. Give the mode handler a chance to react
			// to specifically *this* (e.g. Deathmatch persisting/resetting
			// its streak here rather than at each individual death) before
			// tearing down into the GameOver scene. Win if it was the
			// enemy side that just ran out.
			getGameModeHandler()->onSideEliminated(isPlayerSide);
			onGameOver(!isPlayerSide);
			// onGameOver() just pushed a whole new Scene (the GameOver
			// screen) via Director::pushScene(). If a second entry in this
			// same batch also eliminated its side -- a mutual kill landing
			// on both duelists' last characters in the same frame -- calling
			// onGameOver() again here would run its scene-snapshot logic
			// against the GameOver scene instead of the game scene it
			// expects, and the unconditional cast of that scene's first
			// child to BGLayer* would be garbage. Stop processing the rest
			// of this batch entirely rather than risk that.
			return;
		}

		string nextKey = roster[nextSlot];
		// Leave the dead character's key sitting in this slot rather than
		// clearing it -- doesn't change what's switchable (that's gated on
		// the elimination set, not roster slot contents), just keeps the
		// slot as a visible record of who died there instead of going
		// blank.
		roster[nextSlot] = deadKey;

		auto nextIt = heroMap.find(nextKey);
		Hero* newHero = (nextIt != heroMap.end()) ? nextIt->second : nullptr;
		if (!newHero)
			continue;

		// The already-spawned bench member takes the dead hero's place --
		// nothing new is spawned.
		activateHero(newHero, info.position, deadFlipped, isPlayerSide);

		if (isPlayerSide)
		{
			refreshPlayerHudForHero(this, newHero, nextKey);

			ActionButton* swappedButton = (nextSlot == 0) ? _hudLayer->allySwitch1Button : _hudLayer->allySwitch2Button;
			if (swappedButton)
			{
				auto newFrame = getSpriteFrame("{}_rp.png", deadKey);
				if (newFrame)
					swappedButton->setDisplayFrame(newFrame);
				// Deliberately no beganAnimation() here -- this slot is
				// eliminated now, it should stay locked for the rest of
				// the match regardless of cooldown state (see
				// HudLayer::updateAllySwitchButtons / ActionButton's
				// isCanClick, both of which check isRosterNameEliminated
				// directly rather than relying on the button's own CD).
			}
		}
		else
		{
			_hudLayer->refreshEnemyAvatar(nextKey);
			setEnemyHPLose(newHero->getHpPercent());

			Sprite* swappedIcon = (nextSlot == 0) ? _hudLayer->enemyAllyIcon1 : _hudLayer->enemyAllyIcon2;
			if (swappedIcon)
			{
				auto newFrame = getSpriteFrame("{}_rp.png", deadKey);
				if (newFrame)
					swappedIcon->setDisplayFrame(newFrame);
			}
		}
	}
}

// Ticked once a second from BattleRuntimeSystem::updateGameTime(). Purely
// opportunistic — never interrupts anything, never forces a swap, just
// waits for its cooldown and a clear moment.
void GameLayer::updateEnemyAllySwitch(float dt)
{
	if (!isDuelMode() || _enemyAllyRoster.empty())
		return;

	_enemyAllySwitchCooldown -= dt;
	if (_enemyAllySwitchCooldown > 0.0f)
		return;

	Hero* enemyHero = getActiveHero(false);
	if (!enemyHero)
		return;

	// Switching never interrupts or gets blocked by whatever enemyHero is
	// doing -- it just goes to the bench (see switchEnemyAllySlot).
	vector<int> availableSlots;
	for (int i = 0; i < (int)_enemyAllyRoster.size(); i++)
	{
		if (!isRosterNameEliminated(_enemyAllyRoster[i], false))
			availableSlots.push_back(i);
	}
	if (availableSlots.empty())
		return;

	setRand();
	int slotIndex = availableSlots[random((int)availableSlots.size())];
	switchEnemyAllySlot(slotIndex);

	// Wait at least 20-40s (randomized) before considering another swap.
	setRand();
	_enemyAllySwitchCooldown = 20.0f + random(21);
}

void GameLayer::gearButtonClick(GearType type)
{
	currentPlayer->useGear(type);
}

void GameLayer::attackButtonRelease()
{
	_isAttackButtonRelease = true;
}

void GameLayer::onPause()
{
	if (_isPause)
		return;

	_isPause = true;
	RenderTexture* snapshoot = RenderTexture::create(winSize.width, winSize.height);
	Scene* f = Director::sharedDirector()->getRunningScene();
	Ref* pObject = f->getChildren()->objectAtIndex(0);
	BGLayer* bg = (BGLayer*)pObject;
	snapshoot->begin();
	bg->visit();

	visit();
	snapshoot->end();

	Scene* pscene = Scene::create();
	PauseLayer* layer = PauseLayer::create(snapshoot);
	pscene->addChild(layer);
	Director::sharedDirector()->pushScene(pscene);
}

void GameLayer::resumeFromPause()
{
	if (!_isPause)
		return;

	if (UserDefault::sharedUserDefault()->getBoolForKey("isBGM"))
	{
		SimpleAudioEngine::sharedEngine()->resumeBackgroundMusic();
	}
	if (UserDefault::sharedUserDefault()->getBoolForKey("isVoice"))
	{
		SimpleAudioEngine::sharedEngine()->resumeAllEffects();
	}

	Director::sharedDirector()->popScene();
	_isPause = false;
}

void GameLayer::onGear()
{
	if (!_enableGear)
		return;
	if (_isGear)
		return;
	_isGear = true;

	RenderTexture* snapshoot = RenderTexture::create(winSize.width, winSize.height);
	Scene* f = Director::sharedDirector()->getRunningScene();
	Ref* pObject = f->getChildren()->objectAtIndex(0);
	BGLayer* bg = (BGLayer*)pObject;
	snapshoot->begin();
	bg->visit();

	visit();
	snapshoot->end();

	Scene* pscene = Scene::create();
	GearLayer* layer = GearLayer::create(snapshoot);
	_gearLayer = layer;
	layer->updatePlayerGear();
	pscene->addChild(layer);
	Director::sharedDirector()->pushScene(pscene);
}

void GameLayer::onGameOver(bool isWin)
{
	removeKeyEventHandler();

	if (_isPause)
	{
		_isPause = false;
		Director::sharedDirector()->popScene();
	}
	if (_isGear)
	{
		_isGear = false;
		Director::sharedDirector()->popScene();
	}

	RenderTexture* snapshoot = RenderTexture::create(winSize.width, winSize.height);
	Scene* f = Director::sharedDirector()->getRunningScene();
	Ref* pObject = f->getChildren()->objectAtIndex(0);
	BGLayer* bg = (BGLayer*)pObject;
	snapshoot->begin();
	bg->visit();
	visit();
	snapshoot->end();

	getGameModeHandler()->Internal_GameOver();

	Scene* pscene = Scene::create();
	GameOver* layer = GameOver::create(snapshoot);
	layer->setWin(isWin);
	pscene->addChild(layer);
	Director::sharedDirector()->pushScene(pscene);
}

void GameLayer::onLeft()
{
	CCNotificationCenter::sharedNotificationCenter()->purgeNotificationCenter();

	CCArray* childArray = getChildren();
	Ref* pObject;
	CCARRAY_FOREACH(childArray, pObject)
	{
		auto ac = (Node*)pObject;
		ac->unscheduleUpdate();
		ac->unscheduleAllSelectors();
	}

	// Deathmatch "next stage" quick restart launches straight into a new
	// match that needs the exact same assets the next LoadLayer is about
	// to preload again anyway -- skip unloading them instead of unloading
	// then immediately reloading. This matters most for UI.plist/Map.plist
	// specifically: those are only ever loaded from Lua (StartMenu.lua /
	// SelectLayer.lua), and this shortcut never passes back through either
	// of those screens to reload them.
	if (!_quickRestartDeathmatch)
	{
		// Benched duel heroes aren't in _CharacterArray -- unload their
		// images too.
		vector<Hero*> allHeroes = _CharacterArray;
		allHeroes.insert(allHeroes.end(), _benchedHeroes.begin(), _benchedHeroes.end());
		LoadLayer::unloadAllCharsIMG(allHeroes);
		// AFTER
		if (!isDuelMode())
			removeSprites(format("Unit/Tower/Tower{}.plist", mapId));

		if (_isHardCoreGame)
		{
			removeSprites(kGuardian_Han);
			removeSprites(kGuardian_Roshi);
			KTools::prepareFileOGG(GuardianEnum::Han, true);
			KTools::prepareFileOGG(GuardianEnum::Roshi, true);
		}

		KTools::prepareFileOGG("Effect", true);
		KTools::prepareFileOGG("Ougis", true);
	}

	_CharacterArray.clear();
	_benchedHeroes.clear();
	_allyHeroByKey.clear();
	_enemyHeroByKey.clear();
	_TowerArray.clear();
	_KonohaFlogArray.clear();
	_AkatsukiFlogArray.clear();

	if (!_quickRestartDeathmatch)
	{
		removeSprites("UI.plist");
		removeSprites("Map.plist");
	}

	SimpleAudioEngine::sharedEngine()->end();

	if (_quickRestartDeathmatch)
	{
		_quickRestartDeathmatch = false;

		// Same handoff SelectLayer::onGameStart() does, just fed from the
		// retained character/roster instead of a fresh selection.
		auto tempSelect = SelectLayer::create();
		tempSelect->setSelectHero(_retainedPlayerChar.c_str());
		if (_retainedAllyRoster.size() >= 1)
			tempSelect->setCom1Select(_retainedAllyRoster[0].c_str());
		if (_retainedAllyRoster.size() >= 2)
			tempSelect->setCom2Select(_retainedAllyRoster[1].c_str());

		auto handler = getGameModeHandler();
		handler->selectLayer = tempSelect;
		handler->init();
		handler->onInitHeros();

		auto loadScene = Scene::create();
		auto loadLayer = LoadLayer::create();
		loadLayer->preloadAudio();
		loadScene->addChild(loadLayer);

		Director::sharedDirector()->replaceScene(TransitionFade::create(1.0f, loadScene));
		return;
	}

	lua_call_func(UiFlowKeys::kOnGameOver);
}

void GameLayer::checkBackgroundMusic(float dt)
{
	if (UserDefault::sharedUserDefault()->getBoolForKey("isBGM"))
	{
		if (!SimpleAudioEngine::sharedEngine()->isBackgroundMusicPlaying())
		{
			if (!_isHardCoreGame)
			{
				SimpleAudioEngine::sharedEngine()->playBackgroundMusic(BATTLE_MUSIC);
			}
			else
			{
				int id = (mapId - 1) > 4 ? 4 : (mapId - 1);
				if (_playNum == 0)
				{
					SimpleAudioEngine::sharedEngine()->playBackgroundMusic(format("Audio/Music/Battle{}.ogg", 2 + id * 3).c_str(), false);
					_playNum++;
				}
				else if (_playNum == 1)
				{
					SimpleAudioEngine::sharedEngine()->playBackgroundMusic(format("Audio/Music/Battle{}.ogg", 3 + id * 3).c_str(), false);
					_playNum++;
				}
				else if (_playNum == 2)
				{
					SimpleAudioEngine::sharedEngine()->playBackgroundMusic(format("Audio/Music/Battle{}.ogg", 1 + id * 3).c_str(), false);
					_playNum = 0;
				}
			}
		}
	}
}

void GameLayer::setOugis(CharacterBase* sender)
{
	if (!_hudLayer->ougisLayer)
	{
		ougisChar = sender;

		CCArray* childArray = getChildren();
		Ref* pObject;
		CCARRAY_FOREACH(childArray, pObject)
		{
			auto object = (Node*)pObject;
			object->pauseSchedulerAndActions();
		}
		pauseSchedulerAndActions();

		updateViewPoint(0.01f);

		blend = LayerColor::create(ccc4(0, 0, 0, 200), winSize.width, winSize.height);
		blend->setPosition(Vec2(-getPositionX(), 0));
		addChild(blend, 1000);
		sender->setZOrder(2000);

		if (UserDefault::sharedUserDefault()->getBoolForKey("isVoice"))
		{
			SimpleAudioEngine::sharedEngine()->playEffect(format("Audio/Ougis/{}_ougis.ogg", ougisChar->getName()).c_str());
		}

		_hudLayer->setOugis(ougisChar->getName(), ougisChar->getGroup());
	}
}

void GameLayer::removeOugis()
{
	ougisChar->setZOrder(-ougisChar->getPositionY());
	CCArray* childArray = getChildren();
	Ref* pObject;
	CCARRAY_FOREACH(childArray, pObject)
	{
		auto object = (Node*)pObject;
		object->resumeSchedulerAndActions();
	}
	resumeSchedulerAndActions();

	blend->removeFromParent();
	ougisChar = nullptr;
}

#if (CC_TARGET_PLATFORM == CC_PLATFORM_MAC)
extern "C" void MacKeyboard_register();
extern "C" void MacKeyboard_unregister();
#endif

void GameLayer::setKeyEventHandler()
{
#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
	Director::sharedDirector()->getOpenGLView()->setAccelerometerKeyHook((GLView::LPFN_ACCELEROMETER_KEYHOOK)(&GameLayer::LPFN_ACCELEROMETER_KEYHOOK));
#elif (CC_TARGET_PLATFORM == CC_PLATFORM_LINUX)
	glfwSetKeyCallback(_window, keyEventHandle);
#elif (CC_TARGET_PLATFORM == CC_PLATFORM_MAC)
	MacKeyboard_register();
#endif
}

void GameLayer::removeKeyEventHandler()
{
#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
	Director::sharedDirector()->getOpenGLView()->setAccelerometerKeyHook(nullptr);
#elif (CC_TARGET_PLATFORM == CC_PLATFORM_LINUX)
	glfwSetKeyCallback(_window, nullptr);
#elif (CC_TARGET_PLATFORM == CC_PLATFORM_MAC)
	MacKeyboard_unregister();
#endif
}

int GameLayer::getMapCount()
{
	int index = 1;
	int mapCount = 0;
	auto fileUtils = FileUtils::sharedFileUtils();
	while (fileUtils->isFileExist(format("Maps/{}.tmx", index++).c_str()))
		mapCount++;
	CCLOG("===== Found %d maps =====", mapCount);
	return mapCount;
}

int GameLayer::getDuelMapCount()
{
	int index = 1;
	int mapCount = 0;
	auto fileUtils = FileUtils::sharedFileUtils();
	while (fileUtils->isFileExist(format("Maps/Duels/{}.tmx", index++).c_str()))
		mapCount++;
	CCLOG("===== Found %d duel maps =====", mapCount);
	return mapCount;
}

void GameLayer::invokeAllCallbacks()
{
	isHUDInitialized = true;
	if (callbackssList.size() > 0)
	{
		for (auto& callback : callbackssList)
			callback();
		callbackssList.clear();
	}
}

Vec2 GameLayer::getCustomSpawnPoint(HeroData& data)
{
	data.isInit = true;
	return data.group == Group::Konoha ? Vec2(432, 80) : Vec2(2608, 80);
}

void GameLayer::clearAllFlogsMainTarget(CharacterBase* target)
{
	UnitEx::clearMainTarget(target, _KonohaFlogArray);
	UnitEx::clearMainTarget(target, _AkatsukiFlogArray);
}

void GameLayer::clearAllUnitsMainTarget(CharacterBase* target)
{
	clearAllFlogsMainTarget(target);
	UnitEx::clearMainTarget(target, _AkatsukiFlogArray);
}

#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32 || CC_TARGET_PLATFORM == CC_PLATFORM_LINUX)

#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
#define isPressed(__KEY__) _isPressed(__KEY__)
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
#define isPressed(__KEY__) glfwGetKey(_window, __KEY__)
#endif

/**
 * Use __W __S __D __A control when to force move
 */
#define MOVE(__W, __S, __D, __A, name, keyState)                                    \
	{                                                                               \
		if (keyState)                                                               \
			_gLayer->_lastPressedMovementKey = name;                                \
		else if (_gLayer->_lastPressedMovementKey == name)                          \
			_gLayer->_lastPressedMovementKey = -100;                                \
		int horizontal;                                                             \
		int vertical;                                                               \
		if (__W)                                                                    \
		{                                                                           \
			vertical = (isPressed(KEY_W) ? 1 : -1);                                 \
		}                                                                           \
		else if (__S)                                                               \
		{                                                                           \
			vertical = (isPressed(KEY_S) ? -1 : 1);                                 \
		}                                                                           \
		else                                                                        \
		{                                                                           \
			vertical = (isPressed(KEY_W) ? 1 : -1) + (isPressed(KEY_S) ? -1 : 1);   \
			vertical = abs(vertical) > 1 ? vertical / 2 : vertical;                 \
		}                                                                           \
		if (__D)                                                                    \
		{                                                                           \
			horizontal = (isPressed(KEY_D) ? 1 : -1);                               \
		}                                                                           \
		else if (__A)                                                               \
		{                                                                           \
			horizontal = (isPressed(KEY_A) ? -1 : 1);                               \
		}                                                                           \
		else                                                                        \
		{                                                                           \
			horizontal = (isPressed(KEY_D) ? 1 : -1) + (isPressed(KEY_A) ? -1 : 1); \
			horizontal = abs(horizontal) > 1 ? horizontal / 2 : horizontal;         \
		}                                                                           \
		if (horizontal != 0 || vertical != 0)                                       \
		{                                                                           \
			if (!_gLayer->ougisChar)                                                \
				_gLayer->currentPlayer->walk(Vec2(horizontal, vertical));           \
		}                                                                           \
		else if (_gLayer->currentPlayer->getState() == State::WALK)                 \
		{                                                                           \
			_gLayer->_lastPressedMovementKey = -100;                                \
			_gLayer->currentPlayer->idle();                                         \
		}                                                                           \
		break;                                                                      \
	}

#define ON_GEAR_BY(__ID__, __KEY_STATE__)                                     \
	if (_gLayer->_isGear && __KEY_STATE__)                                    \
	{                                                                         \
		auto &gearBtns = _gLayer->_gearLayer->_screwLayer->getGearBtnArray(); \
		if (gearBtns.size() >= __ID__ - 1)                                    \
		{                                                                     \
			auto gear_btn = gearBtns.at(__ID__ - 1);                          \
			if (gear_btn)                                                     \
				gear_btn->click();                                            \
		}                                                                     \
	}                                                                         \
	break;

bool GameLayer::checkHasAnyMovement()
{
	if (_gLayer)
	{
		if (_gLayer->_lastPressedMovementKey != -100)
		{
			keyEventHandle(_window, _gLayer->_lastPressedMovementKey, 0, 1, 0);
			return true;
		}
	}
	return false;
}

/** NOTE: Impl key listener */
void GameLayer::keyEventHandle(GLFWwindow* window, int key, int scancode, int keyState, int mods)
{
	// NOTE: only attack button can hold
	//  Other keys is only click
	if (keyState == 2 && key != KEY_J)
		return;
	switch (key)
	{
	case KEY_W:
		// case KEY_UP:
		MOVE(keyState, 0, 0, 0, KEY_W, keyState);
	case KEY_S:
		// case KEY_DOWN:
		MOVE(0, keyState, 0, 0, KEY_S, keyState);
	case KEY_A:
		// case KEY_LEFT:
		MOVE(0, 0, keyState, 0, KEY_A, keyState);
	case KEY_D:
		// case KEY_RIGHT:
		MOVE(0, 0, 0, keyState, KEY_D, keyState);
	case KEY_J:
		if (keyState)
			_gLayer->_hudLayer->nAttackButton->click();
		else
			_gLayer->_isAttackButtonRelease = true;
		break;
	case KEY_L: // Ramen button -- duel modes hold-to-charge instead of a
	            // single-shot heal click (see ActionButton::ccTouchBegan/
	            // ccTouchEnded for the touch equivalent). Outside duel
	            // mode this keeps the old single-click heal behavior.
		if (isDuelMode())
		{
			if (keyState)
				_gLayer->_hudLayer->item1Button->startChakraCharge();
			else
				_gLayer->_hudLayer->item1Button->stopChakraCharge();
		}
		else if (keyState)
		{
			_gLayer->_hudLayer->item1Button->click();
		}
		break;
	case KEY_H: // Ougis 2 buttons
		if (keyState)
			_gLayer->_hudLayer->skill5Button->click();
		break;
	case KEY_K: // Ougis 1 buttons
		if (keyState)
			_gLayer->_hudLayer->skill4Button->click();
		break;
	case KEY_U: // skill 1
		if (keyState)
			_gLayer->_hudLayer->skill1Button->click();
		break;
	case KEY_I: // skill 2
		if (keyState)
			_gLayer->_hudLayer->skill2Button->click();
		break;
		// AFTER
	case KEY_O: // skill 3
		if (keyState)
			_gLayer->_hudLayer->skill3Button->click();
		break;
	case KEY_T: // swap to ally slot 1
		if (keyState && _gLayer->_hudLayer->allySwitch1Button)
			_gLayer->_hudLayer->allySwitch1Button->click();
		break;
	case KEY_Y: // swap to ally slot 2
		if (keyState && _gLayer->_hudLayer->allySwitch2Button)
			_gLayer->_hudLayer->allySwitch2Button->click();
		break;
		// Gear buttons
	case KEY_1:
	case KEY_KP_1:
		ON_GEAR_BY(1, keyState);
	case KEY_2:
	case KEY_KP_2:
		ON_GEAR_BY(2, keyState);
	case KEY_3:
	case KEY_KP_3:
		ON_GEAR_BY(3, keyState);
	case KEY_4:
	case KEY_KP_4:
		ON_GEAR_BY(4, keyState);
	case KEY_5:
	case KEY_KP_5:
		ON_GEAR_BY(5, keyState);
	case KEY_6:
	case KEY_KP_6:
		ON_GEAR_BY(6, keyState);
	case KEY_7:
	case KEY_KP_7:
		ON_GEAR_BY(7, keyState);
	case KEY_8:
	case KEY_KP_8:
		ON_GEAR_BY(8, keyState);
	case KEY_9:
	case KEY_KP_9:
		ON_GEAR_BY(9, keyState);
	case KEY_0:
	case KEY_KP_0:
		break;
		/* Item buttons */
		// Item 1 & Purchase
	case KEY_B:
		if (keyState)
		{
			if (_gLayer->_isGear)
				_gLayer->_gearLayer->confirmPurchase();
			else
				_gLayer->_hudLayer->getItem3Button()->click();
		}
		break;
		// Item 2
	case KEY_N:
		if (keyState)
			_gLayer->_hudLayer->getItem4Button()->click();
		break;
		// Item 3
	case KEY_M:
		if (keyState)
			_gLayer->_hudLayer->getItem2Button()->click();
		break;
	case KEY_ESCAPE:
	case KEY_ENTER:
		if (keyState && _gLayer->_isStarted == true)
		{
			if (_gLayer->_isPause)
			{
				_gLayer->resumeFromPause();
			}
			else if (_gLayer->_isGear)
			{
				Director::sharedDirector()->popScene();
				_gLayer->_isGear = false;
			}
			else
			{
				_gLayer->onPause(); // enter pause menu
			}
		}
		break;
	case KEY_SPACE:
		if (_gLayer->_enableGear && _gLayer->_isStarted && keyState && !_gLayer->_isPause)
		{
			if (_gLayer->_isGear)
			{
				Director::sharedDirector()->popScene();
				_gLayer->_isGear = false;
			}
			else
			{
				_gLayer->onGear(); // enter gear shop
			}
		}
		break;
	case KEY_F11:
		// if (keyState)
		// {
		// 	// SET_FULL_SCREEN_MODE(_window, _isFullScreen);
		// 	GLFWmonitor *monitor = glfwGetPrimaryMonitor();
		// 	if (nullptr == monitor)
		// 	{
		// 		return;
		// 	}
		// 	const GLFWvidmode *videoMode = glfwGetVideoMode(monitor);
		// 	int width, height;
		// 	glfwGetWindowSize(_window, &width, &height);
		// 	if (_isFullScreen)
		// 	{
		// 		glfwSetWindowMonitor(_window, nullptr, videoMode->width / 2, videoMode->height / 2, WIDTH, HEIGHT, videoMode->refreshRate);
		// 		glfwSetWindowPos(_window,
		// 						 (videoMode->width - WIDTH) / 2,
		// 						 (videoMode->height - HEIGHT) * 0.35f);
		// 	}
		// 	else
		// 	{
		// 		glfwSetWindowMonitor(_window, nullptr, 0, 0, videoMode->width, videoMode->height, videoMode->refreshRate);
		// 		Director::sharedDirector()->getOpenGLView()->updateFrameSize(videoMode->width,videoMode->height);
		// 	}
		// 	_isFullScreen = !_isFullScreen;
		// }
		break;
	}
}

#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)

void GameLayer::LPFN_ACCELEROMETER_KEYHOOK(UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:
		keyEventHandle(nullptr, wParam, 0, 1, 0);
		break;
	case WM_SYSKEYUP:
	case WM_KEYUP:
		keyEventHandle(nullptr, wParam, 0, 0, 0);
		break;
	}
}

#endif

#endif

#if (CC_TARGET_PLATFORM == CC_PLATFORM_MAC)

static bool s_macKeyState[256] = {};

#define isPressed(__KEY__) ((__KEY__) < 256 && s_macKeyState[__KEY__])

#define MOVE_MAC(__W, __S, __D, __A, name, keyState)                                    \
	{                                                                               \
		if (keyState)                                                               \
			_gLayer->_lastPressedMovementKey = name;                                \
		else if (_gLayer->_lastPressedMovementKey == name)                          \
			_gLayer->_lastPressedMovementKey = -100;                                \
		int horizontal;                                                             \
		int vertical;                                                               \
		if (__W)                                                                    \
		{                                                                           \
			vertical = (isPressed(KEY_W) ? 1 : -1);                                \
		}                                                                           \
		else if (__S)                                                               \
		{                                                                           \
			vertical = (isPressed(KEY_S) ? -1 : 1);                                \
		}                                                                           \
		else                                                                        \
		{                                                                           \
			vertical = (isPressed(KEY_W) ? 1 : -1) + (isPressed(KEY_S) ? -1 : 1);  \
			vertical = abs(vertical) > 1 ? vertical / 2 : vertical;                \
		}                                                                           \
		if (__D)                                                                    \
		{                                                                           \
			horizontal = (isPressed(KEY_D) ? 1 : -1);                              \
		}                                                                           \
		else if (__A)                                                               \
		{                                                                           \
			horizontal = (isPressed(KEY_A) ? -1 : 1);                              \
		}                                                                           \
		else                                                                        \
		{                                                                           \
			horizontal = (isPressed(KEY_D) ? 1 : -1) + (isPressed(KEY_A) ? -1 : 1); \
			horizontal = abs(horizontal) > 1 ? horizontal / 2 : horizontal;        \
		}                                                                           \
		if (horizontal != 0 || vertical != 0)                                       \
		{                                                                           \
			if (!_gLayer->ougisChar)                                                \
				_gLayer->currentPlayer->walk(Vec2(horizontal, vertical));           \
		}                                                                           \
		else if (_gLayer->currentPlayer->getState() == State::WALK)                 \
		{                                                                           \
			_gLayer->_lastPressedMovementKey = -100;                                \
			_gLayer->currentPlayer->idle();                                         \
		}                                                                           \
		break;                                                                      \
	}

void GameLayer::keyEventHandle(int key, int keyState)
{
	if (!_gLayer || !_gLayer->currentPlayer)
		return;

	if (key >= 0 && key < 256)
		s_macKeyState[key] = (keyState != 0);

	switch (key)
	{
	case KEY_W:
		MOVE_MAC(keyState, 0, 0, 0, KEY_W, keyState);
	case KEY_S:
		MOVE_MAC(0, keyState, 0, 0, KEY_S, keyState);
	case KEY_A:
		MOVE_MAC(0, 0, keyState, 0, KEY_A, keyState);
	case KEY_D:
		MOVE_MAC(0, 0, 0, keyState, KEY_D, keyState);
	case KEY_J:
		if (keyState)
			_gLayer->_hudLayer->nAttackButton->click();
		else
			_gLayer->_isAttackButtonRelease = true;
		break;
	case KEY_L: // Ramen button -- see the GLFW keyEventHandle() overload
	            // above for why duel modes hold-to-charge instead of a
	            // single click.
		if (isDuelMode())
		{
			if (keyState)
				_gLayer->_hudLayer->item1Button->startChakraCharge();
			else
				_gLayer->_hudLayer->item1Button->stopChakraCharge();
		}
		else if (keyState)
		{
			_gLayer->_hudLayer->item1Button->click();
		}
		break;
	case KEY_H:
		if (keyState)
			_gLayer->_hudLayer->skill5Button->click();
		break;
	case KEY_K:
		if (keyState)
			_gLayer->_hudLayer->skill4Button->click();
		break;
	case KEY_U:
		if (keyState)
			_gLayer->_hudLayer->skill1Button->click();
		break;
	case KEY_I:
		if (keyState)
			_gLayer->_hudLayer->skill2Button->click();
		break;
	case KEY_O:
		if (keyState)
			_gLayer->_hudLayer->skill3Button->click();
		break;
	case KEY_T:
		if (keyState && _gLayer->_hudLayer->allySwitch1Button)
			_gLayer->_hudLayer->allySwitch1Button->click();
		break;
	case KEY_Y:
		if (keyState && _gLayer->_hudLayer->allySwitch2Button)
			_gLayer->_hudLayer->allySwitch2Button->click();
		break;
	case KEY_1: case KEY_KP_1:
		if (_gLayer->_isGear && keyState) { auto& gb = _gLayer->_gearLayer->_screwLayer->getGearBtnArray(); if (gb.size() >= 1 && gb.at(0)) gb.at(0)->click(); } break;
	case KEY_2: case KEY_KP_2:
		if (_gLayer->_isGear && keyState) { auto& gb = _gLayer->_gearLayer->_screwLayer->getGearBtnArray(); if (gb.size() >= 2 && gb.at(1)) gb.at(1)->click(); } break;
	case KEY_3: case KEY_KP_3:
		if (_gLayer->_isGear && keyState) { auto& gb = _gLayer->_gearLayer->_screwLayer->getGearBtnArray(); if (gb.size() >= 3 && gb.at(2)) gb.at(2)->click(); } break;
	case KEY_B:
		if (keyState) { if (_gLayer->_isGear) _gLayer->_gearLayer->confirmPurchase(); else _gLayer->_hudLayer->getItem3Button()->click(); } break;
	case KEY_N:
		if (keyState) _gLayer->_hudLayer->getItem4Button()->click(); break;
	case KEY_M:
		if (keyState) _gLayer->_hudLayer->getItem2Button()->click(); break;
	case KEY_SPACE:
		if (_gLayer->_enableGear && _gLayer->_isStarted && keyState && !_gLayer->_isPause)
		{
			if (_gLayer->_isGear)
			{
				Director::sharedDirector()->popScene();
				_gLayer->_isGear = false;
			}
			else
			{
				_gLayer->onGear();
			}
		}
		break;
	case KEY_ESCAPE: case KEY_ENTER:
		if (keyState && _gLayer->_isStarted)
		{
			if (_gLayer->_isPause) { _gLayer->resumeFromPause(); }
			else if (_gLayer->_isGear) { Director::sharedDirector()->popScene(); _gLayer->_isGear = false; }
			else { _gLayer->onPause(); }
		}
		break;
	}
}

bool GameLayer::checkHasAnyMovement()
{
	if (_gLayer && _gLayer->_lastPressedMovementKey != -100)
	{
		keyEventHandle(_gLayer->_lastPressedMovementKey, 1);
		return true;
	}
	return false;
}

#elif !(CC_TARGET_PLATFORM == CC_PLATFORM_WIN32 || CC_TARGET_PLATFORM == CC_PLATFORM_LINUX)
bool GameLayer::checkHasAnyMovement()
{
	return false;
}
#endif

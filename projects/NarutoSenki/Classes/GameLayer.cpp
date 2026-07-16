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

// Shared "spawn and configure a new active hero" logic — see the comment
// on the declaration in GameLayer.h.
Hero* GameLayer::spawnAllyReplacement(const string& nextName, Role role, Group grp, Vec2 spawnPos, int charId, bool isPlayerSide)
{
	auto& hpMap = isPlayerSide ? _allyHpByName : _enemyAllyHpByName;

	Hero* newHero = addHero(nextName, role, grp, spawnPos, charId);
	newHero->setWalkSpeed(newHero->_originSpeed);

	newHero->setCoin(3000);
	newHero->setEXP(2500);
	for (int i = 1; i < 6; i++)
		newHero->changeHPbar();
	uint32_t newMaxHP = newHero->getMaxHP() * 2;
	newHero->setMaxHPValue(newMaxHP, false);

	// Restore this character's own persisted HP if they've been active
	// earlier this match (i.e. switched away from before); otherwise this
	// is their first time in and they start at full HP. Clamped to their
	// current MaxHP in case some buff/level difference makes the saved
	// value no longer valid.
	auto savedHpIt = hpMap.find(nextName);
	uint32_t restoredHp = (savedHpIt != hpMap.end()) ? (std::min)(savedHpIt->second, newMaxHP) : newMaxHP;
	newHero->setHPValue(restoredHp, true);

	newHero->increaseAllCkrs(25000);
	newHero->enableReborn = false;

	return newHero;
}

bool GameLayer::isRosterNameEliminated(const string& name, bool isPlayerSide)
{
	auto& eliminated = isPlayerSide ? _eliminatedAllies : _eliminatedEnemyAllies;
	return eliminated.find(name) != eliminated.end();
}

void GameLayer::switchToAllySlot(int slotIndex)
{
	if (slotIndex < 0 || slotIndex >= (int)_allyRoster.size() || !currentPlayer || currentPlayer->getState() == State::DEAD)
		return;

	string nextName = _allyRoster[slotIndex];

	// Can't switch into an eliminated slot — same defense-in-depth as the
	// DEAD check above; the HUD button should already refuse this click
	// (see HudLayer::updateAllySwitchButtons / ActionButton::isCanClick),
	// but guard the actual swap logic too.
	if (isRosterNameEliminated(nextName, true))
		return;

	Hero* oldHero = (Hero*)currentPlayer;
	Vec2 swapPos = oldHero->getPosition();
	Group grp = oldHero->getGroup();
	int oldCharId = oldHero->getCharId();

	oldHero->setSkillEffect("smk");

	string oldName = oldHero->getName();

	// Save the outgoing hero's current HP against their own name, so it's
	// restored (rather than some other character inheriting it as a
	// percentage, or it resetting to full) if/when they get switched back
	// to later in the match.
	_allyHpByName[oldName] = oldHero->getHP();

	// oldHero isn't torn down here anymore — control moves to newHero
	// immediately below (addHero sets currentPlayer as soon as it spawns),
	// but oldHero itself is handed to AI and left on the field to finish
	// whatever it was doing (idle, walking, or mid-skill/attack) before it
	// despawns on its own. This is what lets Chiyo keep her Parents puppet
	// alive and Lee finish a combo instead of having both cut off mid-
	// action, and it's why the mid-attack switch guard that used to be here
	// is gone — switching no longer interrupts anything.
	oldHero->retireAndDespawnWhenIdle();

	_allyRoster[slotIndex] = oldName;

	Hero* newHero = spawnAllyReplacement(nextName, Role::Player, grp, swapPos, oldCharId, true);

	_hudLayer->setEXPLose();
	_hudLayer->coinLabel->setString(to_cstr(newHero->getCoin()));
	if (!newHero->isEnableSkill04())
		_hudLayer->skill4Button->setLock();
	if (!newHero->isEnableSkill05())
		_hudLayer->skill5Button->setLock();

	_hudLayer->initGearButton(nextName);
	_hudLayer->updateSkillButtons();
	_hudLayer->resetSkillButtons();

	ActionButton* swappedButton = (slotIndex == 0) ? _hudLayer->allySwitch1Button : _hudLayer->allySwitch2Button;
	if (swappedButton)
	{
		auto newFrame = getSpriteFrame("{}_rp.png", oldName);
		if (newFrame)
			swappedButton->setDisplayFrame(newFrame);
		// Only the slot actually used gets the full ~15s recharge here —
		// the other slot's separate 0.5s "just switched" lock is applied
		// in attackButtonClick instead (see the comment there). This used
		// to call beganAnimation() on BOTH buttons unconditionally, which
		// is what put both slots on the full 15s cooldown together no
		// matter which one was clicked.
		swappedButton->beganAnimation();
	}

	// newHero's HP is now independent of oldHero's — reflect newHero's own
	// (restored) percentage on the HUD bar, not oldHero's.
	setHPLose(newHero->getHpPercent());
}


// AI-side mirror of switchToAllySlot above, operating on the enemy hero
// and _enemyAllyRoster instead of the player. Kept separate rather than
// unified with a parameter, since the two sides update different HUD
// elements (skill/gear buttons vs. the cosmetic enemy icons) and swap in
// as different Roles (Player vs Com).
void GameLayer::switchEnemyAllySlot(int slotIndex)
{
	if (slotIndex < 0 || slotIndex >= (int)_enemyAllyRoster.size())
		return;

	// Can't switch into an eliminated slot — mirrors the player-side guard
	// in switchToAllySlot.
	if (isRosterNameEliminated(_enemyAllyRoster[slotIndex], false))
		return;

	Group enemyGrp = (playerGroup == Group::Konoha) ? Group::Akatsuki : Group::Konoha;
	Hero* oldHero = nullptr;
	for (auto hero : _CharacterArray)
	{
		// Same _isRetiring skip as updateEnemyAllySwitch — don't pick an
		// already-retiring hero as the thing to switch out again.
		if (hero->isCom() && hero->getGroup() == enemyGrp && hero->getState() != State::DEAD && !hero->_isRetiring)
		{
			oldHero = hero;
			break;
		}
	}
	if (!oldHero)
		return;

	Vec2 swapPos = oldHero->getPosition();
	Group grp = oldHero->getGroup();
	int oldCharId = oldHero->getCharId();

	oldHero->setSkillEffect("smk");
	string oldName = oldHero->getName();
	string nextName = _enemyAllyRoster[slotIndex];

	// Same per-name HP persistence as switchToAllySlot — see the comments
	// there.
	_enemyAllyHpByName[oldName] = oldHero->getHP();

	// Same retire-instead-of-despawn handoff as switchToAllySlot — see the
	// comments there. oldHero stays on the field under AI (it's already
	// Role::Com so no role change is needed here) and despawns itself once
	// its current action finishes.
	oldHero->retireAndDespawnWhenIdle();

	_enemyAllyRoster[slotIndex] = oldName;

	Hero* newHero = spawnAllyReplacement(nextName, Role::Com, grp, swapPos, oldCharId, false);
	newHero->doAI();

	_hudLayer->refreshEnemyAvatar(nextName);

	Sprite* swappedIcon = (slotIndex == 0) ? _hudLayer->enemyAllyIcon1 : _hudLayer->enemyAllyIcon2;
	if (swappedIcon)
	{
		auto newFrame = getSpriteFrame("{}_rp.png", oldName);
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

	// Same dependent cleanup as CharacterBase::despawnRetiredHero() — a
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
	info.wasRetiring = false;
	_pendingDeaths.push_back(info);

	scheduleOnce(schedule_selector(GameLayer::processPendingDeaths), 0.0f);
}

void GameLayer::eliminateInactiveHero(CharacterBase* deadHero)
{
	PendingDeathInfo info;
	info.name = deadHero->getName();
	info.group = deadHero->getGroup();
	info.position = deadHero->getPosition();
	info.charId = deadHero->getCharId();
	info.hero = deadHero;
	info.wasRetiring = true;
	_pendingDeaths.push_back(info);

	// Same one-frame defer as forceSwitchOnDeath, same reasoning — this
	// runs from onCharacterDead(), the very first line of dead(), before
	// dead()'s own later body (HP bar clearing etc, operating on `this`)
	// has run. despawnDeadHero() below would otherwise get clobbered a
	// moment later by that tail.
	scheduleOnce(schedule_selector(GameLayer::processPendingDeaths), 0.0f);
}

void GameLayer::processPendingDeaths(float dt)
{
	// Snapshot and clear up front — onGameOver() below can trigger a
	// teardown that ends up touching this vector again, and a stray
	// double-death in the same frame (both duelists going down at once,
	// e.g. a mutual-kill hit) shouldn't get processed twice.
	auto pending = _pendingDeaths;
	_pendingDeaths.clear();

	for (auto& info : pending)
	{
		// A retiring hero dying is never "the last of that side" and
		// never touches currentPlayer/onGameOver — see the comment on
		// eliminateInactiveHero's declaration for why. Just mark it
		// eliminated and despawn it, skip the force-switch machinery
		// entirely.
		if (info.wasRetiring)
		{
			bool isPlayerSideRetiring = info.group == playerGroup;
			auto& eliminatedRetiring = isPlayerSideRetiring ? _eliminatedAllies : _eliminatedEnemyAllies;
			auto& hpMapRetiring = isPlayerSideRetiring ? _allyHpByName : _enemyAllyHpByName;

			eliminatedRetiring.insert(info.name);
			hpMapRetiring.erase(info.name);

			despawnDeadHero(info.hero);
			continue;
		}

		bool isPlayerSide = info.group == playerGroup;
		auto& eliminated = isPlayerSide ? _eliminatedAllies : _eliminatedEnemyAllies;
		auto& roster = isPlayerSide ? _allyRoster : _enemyAllyRoster;
		auto& hpMap = isPlayerSide ? _allyHpByName : _enemyAllyHpByName;

		eliminated.insert(info.name);
		// No longer relevant -- this name can never be switched to again
		// this match (see isRosterNameEliminated), so there's nothing left
		// to restore HP for.
		hpMap.erase(info.name);

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
		// fine to just null out -- spawnAllyReplacement()'s addHero() call
		// reassigns currentPlayer to the new hero moments later anyway.
		//
		// On the player's actual final death though, nothing else ever
		// reassigns currentPlayer afterward, and GameOver's result screen
		// (GameOver::listResult(), scheduled ~0.2s after GameOver::init())
		// reads currentPlayer's *live* stats (kill count, group, etc, not
		// just its name) to build the screen -- a name-only fallback can't
		// cover that. So instead of nulling it, keep the object itself
		// alive with an extra retain() (released in ~GameLayer(), see
		// _currentPlayerIsExtraRetained) — it's still fully removed from
		// the scene/inert either way, this only affects whether the
		// pointer stays valid for GameOver to read from afterward.
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
			// same batch also eliminated its side — a mutual kill landing
			// on both duelists' last characters in the same frame — calling
			// onGameOver() again here would run its scene-snapshot logic
			// against the GameOver scene instead of the game scene it
			// expects, and the unconditional cast of that scene's first
			// child to BGLayer* would be garbage. Stop processing the rest
			// of this batch entirely rather than risk that.
			return;
		}

		string nextName = roster[nextSlot];
		// Leave the dead character's name sitting in this slot rather than
		// clearing it -- doesn't change what's switchable (that's gated on
		// the elimination set, not roster slot contents), just keeps the
		// slot as a visible record of who died there instead of going
		// blank.
		roster[nextSlot] = info.name;

		if (isPlayerSide)
		{
			Hero* newHero = spawnAllyReplacement(nextName, Role::Player, info.group, info.position, info.charId, true);

			_hudLayer->setEXPLose();
			_hudLayer->coinLabel->setString(to_cstr(newHero->getCoin()));
			if (!newHero->isEnableSkill04())
				_hudLayer->skill4Button->setLock();
			if (!newHero->isEnableSkill05())
				_hudLayer->skill5Button->setLock();

			_hudLayer->initGearButton(nextName);
			_hudLayer->updateSkillButtons();
			_hudLayer->resetSkillButtons();

			ActionButton* swappedButton = (nextSlot == 0) ? _hudLayer->allySwitch1Button : _hudLayer->allySwitch2Button;
			if (swappedButton)
			{
				auto newFrame = getSpriteFrame("{}_rp.png", info.name);
				if (newFrame)
					swappedButton->setDisplayFrame(newFrame);
				// Deliberately no beganAnimation() here — this slot is
				// eliminated now, it should stay locked for the rest of
				// the match regardless of cooldown state (see
				// HudLayer::updateAllySwitchButtons / ActionButton's
				// isCanClick, both of which check isRosterNameEliminated
				// directly rather than relying on the button's own CD).
			}

			setHPLose(newHero->getHpPercent());
		}
		else
		{
			Hero* newHero = spawnAllyReplacement(nextName, Role::Com, info.group, info.position, info.charId, false);
			newHero->doAI();

			_hudLayer->refreshEnemyAvatar(nextName);

			Sprite* swappedIcon = (nextSlot == 0) ? _hudLayer->enemyAllyIcon1 : _hudLayer->enemyAllyIcon2;
			if (swappedIcon)
			{
				auto newFrame = getSpriteFrame("{}_rp.png", info.name);
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

	Group enemyGrp = (playerGroup == Group::Konoha) ? Group::Akatsuki : Group::Konoha;
	Hero* enemyHero = nullptr;
	for (auto hero : _CharacterArray)
	{
		// Skip _isRetiring heroes — they're still Role::Com and still in
		// _CharacterArray until their despawn poll finishes, but they're not
		// the "real" active enemy hero anymore and shouldn't be picked as
		// the target of a fresh switch decision.
		if (hero->isCom() && hero->getGroup() == enemyGrp && hero->getState() != State::DEAD && !hero->_isRetiring)
		{
			enemyHero = hero;
			break;
		}
	}
	if (!enemyHero)
		return;

	// No more mid-attack/cBuff gating here — switching no longer interrupts
	// or gets blocked by whatever enemyHero is doing (see
	// CharacterBase::retireAndDespawnWhenIdle / switchEnemyAllySlot). It
	// just hands enemyHero to AI and lets it finish naturally.
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
		LoadLayer::unloadAllCharsIMG(_CharacterArray);
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
	case KEY_L: // Ramen button
		if (keyState)
			_gLayer->_hudLayer->item1Button->click();
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
	case KEY_L:
		if (keyState)
			_gLayer->_hudLayer->item1Button->click();
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

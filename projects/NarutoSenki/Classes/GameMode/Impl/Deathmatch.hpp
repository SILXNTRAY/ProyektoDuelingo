#pragma once
#include "GameMode/IGameModeHandler.hpp"
#include "MyUtils/KTools.h"

// Deathmatch — endless arcade mode. Mechanically this is just Boss (1v1
// Duel Arena) mode: same setup (EXP/coin/CKR/level-up, 3x HP multiplier,
// no gear, ally bench/swap), same arena rules (isDuelMode() covers both),
// and the same win condition — kill the enemy once and the match ends.
// The "endless" part comes from GameOver's start_btn (Deathmatch-only),
// which quick-restarts into the next stage with the same character
// instead of returning to the menu. The stage streak is persisted to
// SQLite on every win and reset to 0 on a loss or forfeit.
class ModeDeathmatch : public IGameModeHandler
{
private:
	bool isAddCallback;
	int _stage = 1;

	// Same handler-local resolve/transfer pattern as Boss.hpp — see the
	// comments there for why these are staged here before GameLayer exists.
	vector<string> _pendingAllyRoster;
	vector<string> _pendingEnemyAllyRoster;

public:
	void init()
	{
		CCLOG("Enter Deathmatch mode.");

		isAddCallback = false;

		gd.isHardCore = true;
		gd.enableGear = false;
		setSkipFlogInit(true);

		_stage = KTools::readDeathmatchStreak() + 1;
	}

	void onInitHeros()
	{
		initHeros(1, 1, nullptr, Group::Konoha);

		_pendingAllyRoster.clear();
		_pendingEnemyAllyRoster.clear();

		if (selectLayer->_com1Select)
			_pendingAllyRoster.push_back(selectLayer->_com1Select);
		if (selectLayer->_com2Select)
			_pendingAllyRoster.push_back(selectLayer->_com2Select);

		vector<string> used = _pendingAllyRoster;
		for (const auto &h : getHerosArray())
			used.push_back(h.name);

		for (size_t i = 0; i < _pendingAllyRoster.size(); i++)
		{
			auto pick = getRandomHeroExceptAll(used);
			_pendingEnemyAllyRoster.push_back(pick);
			used.push_back(pick);
		}

		// Persist the current team so the mode select screen can offer to
		// resume it later instead of picking fresh — see GameModeLayer's
		// Deathmatch continue/new-team prompt.
		KTools::saveDeathmatchTeam(
			selectLayer->_playerSelect ? selectLayer->_playerSelect : "",
			_pendingAllyRoster.size() >= 1 ? _pendingAllyRoster[0] : "",
			_pendingAllyRoster.size() >= 2 ? _pendingAllyRoster[1] : "");
	}

	void onGameStart()
	{
		updateStageLabel();
	}

	void onGameOver()
	{
	}

	void onCharacterInit(CharacterBase *c)
	{
		if (!isAddCallback && !getGameLayer()->isHUDInit())
		{
			isAddCallback = true;
			auto layer = getGameLayer();

			layer->_allyRoster = _pendingAllyRoster;
			layer->_enemyAllyRoster = _pendingEnemyAllyRoster;

			layer->onHUDInitialized(
				[layer]()
				{
					if (!layer)
						return;
					for (auto hero : layer->_CharacterArray)
					{
						hero->setCoin(3000);
						hero->setEXP(2500);
						for (int i = 1; i < 6; i++)
							hero->changeHPbar();

						uint32_t newMaxHP = hero->getMaxHP() * 3;
						hero->setMaxHPValue(newMaxHP, false);
						hero->setHPValue(newMaxHP, true);

						hero->increaseAllCkrs(25000);
						hero->enableReborn = false;

						if (hero->isPlayer())
						{
							layer->getHudLayer()->setEXPLose();
							layer->getHudLayer()->coinLabel->setString(to_cstr(hero->getCoin()));
							if (!hero->isEnableSkill04())
								layer->getHudLayer()->skill4Button->setLock();
							if (!hero->isEnableSkill05())
								layer->getHudLayer()->skill5Button->setLock();
						}
					}
				});
		}
	}

	void onCharacterDead(CharacterBase *c)
	{
		if (!c->isPlayerOrCom())
			return;

		if (c->getGroup() == playerGroup)
		{
			// Player died — run's over, reset the persisted streak.
			KTools::saveDeathmatchStreak(0);
			getGameLayer()->onGameOver(false);
			return;
		}

		// Enemy died — that's the win condition, same as Boss mode. Persist
		// the stage just cleared as the new streak and end the match; the
		// GameOver screen's start_btn (Deathmatch-only) is what actually
		// continues to the next stage, with the same retained character.
		KTools::saveDeathmatchStreak(_stage);
		getGameLayer()->onGameOver(true);
	}

	void onCharacterReborn(CharacterBase *c)
	{
	}

	void onSurrender() override
	{
		// Forfeiting via the pause menu skips onCharacterDead() entirely
		// (no character actually died in combat), so it needs its own
		// hook to reset the streak the same way a real loss does.
		KTools::saveDeathmatchStreak(0);
	}

	vector<string> getExtraPreloadChars() override
	{
		vector<string> all = _pendingAllyRoster;
		all.insert(all.end(), _pendingEnemyAllyRoster.begin(), _pendingEnemyAllyRoster.end());
		return all;
	}

private:
	void updateStageLabel()
	{
		auto layer = getGameLayer();
		if (!layer || !layer->getHudLayer() || !layer->getHudLayer()->gameClock)
			return;

		layer->getHudLayer()->gameClock->setString(format("Stage {}", _stage).c_str());
	}
};

#pragma once
#include "GameMode/IGameModeHandler.hpp"

// Duel (1 VS 1 Arena) mode — reuses the Boss slot.
// Setup mirrors Mode1v1 (EXP/coin/CKR/level-up via changeHPbar x5),
// then applies an explicit 3x HP multiplier on top of the levelled MaxHP.
// No reborn: first death ends the match instantly.
class ModeBoss : public IGameModeHandler
{
private:
	bool isAddCallback;

	// Resolved in onInitHeros() (handler-local, GameLayer doesn't exist
	// yet at that point) then transferred into GameLayer::_allyRoster
	// inside onCharacterInit(), which is the first point it's safe to
	// touch getGameLayer().
	vector<string> _pendingAllyRoster;

	// Enemy AI's benched roster — same size as the player's, so neither
	// side has an informational advantage. Display only for now; not
	// wired into AI swap behavior yet.
	vector<string> _pendingEnemyAllyRoster;

public:
	void init()
	{
		CCLOG("Enter Duel (1 VS 1 Arena) mode.");

		isAddCallback = false;

		gd.isHardCore = true;
		gd.enableGear = false;
		setSkipFlogInit(true);
	}

	void onInitHeros()
	{
		initHeros(1, 1, nullptr, Group::Konoha);

		// These are handler-local and this handler instance persists across
		// matches (it isn't recreated each game like GameLayer is), so clear
		// out whatever's left from the previous match before rebuilding.
		_pendingAllyRoster.clear();
		_pendingEnemyAllyRoster.clear();

		// Resolve the player's benched ally roster (they don't spawn —
		// only the main pick is on the field until swapped in via the
		// ramen button). GameLayer doesn't exist yet here, so everything
		// below only touches handler-local / selectLayer data.
		if (selectLayer->_com1Select)
			_pendingAllyRoster.push_back(selectLayer->_com1Select);
		if (selectLayer->_com2Select)
			_pendingAllyRoster.push_back(selectLayer->_com2Select);

		// Give the enemy AI a random bench the same size as the player's —
		// symmetry only, no swap logic wired up for it yet. Exclude both
		// duelists and anyone already on the player's bench so the icons
		// don't visually clash.
		vector<string> used = _pendingAllyRoster;
		for (const auto &h : getHerosArray())
			used.push_back(h.name);

		for (size_t i = 0; i < _pendingAllyRoster.size(); i++)
		{
			auto pick = getRandomHeroExceptAll(used);
			_pendingEnemyAllyRoster.push_back(pick);
			used.push_back(pick);
		}
	}

	void onGameStart()
	{
	}

	void onGameOver()
	{
	}

	void onCharacterInit(CharacterBase* c)
	{
		if (!isAddCallback && !getGameLayer()->isHUDInit())
		{
			isAddCallback = true;
			auto layer = getGameLayer();

			// Safe to touch GameLayer here — this only ever runs from
			// inside GameLayer::addHero(), so layer is guaranteed valid.
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

						// Explicit 3x HP multiplier on top of the levelled MaxHP.
						uint32_t newMaxHP = hero->getMaxHP() * 3;
						hero->setMaxHPValue(newMaxHP, false);
						hero->setHPValue(newMaxHP, true);

						hero->increaseAllCkrs(25000);

						// No respawn — first death ends the duel.
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

	void onCharacterDead(CharacterBase* c)
	{
		if (!c->isPlayerOrCom())
			return;

		// Win if the dead character was the enemy; lose if it was the player.
		getGameLayer()->onGameOver(c->getGroup() != playerGroup);
	}

	void onCharacterReborn(CharacterBase* c)
	{
	}

	vector<string> getExtraPreloadChars() override
	{
		vector<string> all = _pendingAllyRoster;
		all.insert(all.end(), _pendingEnemyAllyRoster.begin(), _pendingEnemyAllyRoster.end());
		return all;
	}
};

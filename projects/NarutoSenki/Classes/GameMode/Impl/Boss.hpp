#pragma once
#include "GameMode/IGameModeHandler.hpp"

/* Duel(1 VS 1 Arena) mode — reuses the Boss slot.
 Setup mirrors Mode1v1 (EXP/coin/CKR/level-up via changeHPbar x5),
 then applies an explicit 2x HP multiplier on top of the levelled MaxHP.
 Each side has 3 characters total (the initial pick + 2 companions in
_allyRoster/_enemyAllyRoster). A death eliminates that one character and
 force-switches to the next living roster member (see
 GameLayer::forceSwitchOnDeath) instead of ending the match outright —
 the match only actually ends once a side has lost all 3.*/
class ModeBoss : public IGameModeHandler
{
private:
	bool isAddCallback;

	/* Resolved in onInitHeros() (handler - local, GameLayer doesn't exist
	 yet at that point) then transferred into GameLayer::_allyRoster
	 inside onCharacterInit(), which is the first point it's safe to
	 touch getGameLayer().*/
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
		for (const auto& h : getHerosArray())
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

						// changeHPbar() above levels straight to 6, which
						// (via its own level-2 and level-4 thresholds)
						// grants a full CKR (15000, unlocks SKILL4/Ougis1)
						// AND a full CKR2 (25000, unlocks SKILL5/Ougis2)
						// immediately -- letting both sides open the match
						// already able to spam ultimates. Duel mode should
						// start with SKILL4 available but SKILL5 empty,
						// earned only through actually landing/taking hits
						// (see increaseAllCkrs()'s damage-dealt/damage-taken
						// gain, which stays fully intact and untouched).
						hero->setCKR2(0);
						hero->_isCanOugis2 = false;

						// Explicit 2x HP multiplier on top of the levelled MaxHP.
						uint32_t newMaxHP = hero->getMaxHP() * 2;
						hero->setMaxHPValue(newMaxHP, false);
						hero->setHPValue(newMaxHP, true);

						// Re-grant CKR (SKILL4) only -- changeHPbar()'s level-2
						// threshold already did this once, but this keeps
						// it topped up/consistent with the pre-existing
						// behavior. CKR2 (SKILL5) is deliberately excluded
						// per the setCKR2(0) reset above.
						hero->increaseAllCkrs(25000, true, false);

						// No auto-reborn — a death is instead handled by
						// forceSwitchOnDeath below, which eliminates this
						// character and swaps to the next living roster
						// member, or ends the match if that was the last one.
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

		// A hero mid-retirement (see CharacterBase::_isRetiring — switched
		// away from, finishing its current action under AI before it
		// despawns on its own) still passes isPlayerOrCom() even after
		// setRole(Role::Com), so it needs to be told apart from the
		// actually-active hero dying: it's not the character being played
		// right now, and forceSwitchOnDeath would incorrectly treat this
		// like an active-hero death (hijacking control via an unwanted
		// force-switch and corrupting the roster — see
		// eliminateInactiveHero's comment for the full explanation).
		if (c->_isRetiring)
		{
			getGameLayer()->eliminateInactiveHero(c);
			return;
		}

		// Used to end the match immediately here (getGameLayer()->
		// onGameOver(...)) -- now a death only eliminates that one
		// character and force-switches to the next living roster member.
		// forceSwitchOnDeath itself calls onGameOver() for real once a
		// side has lost all 3.
		getGameLayer()->forceSwitchOnDeath(c);
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
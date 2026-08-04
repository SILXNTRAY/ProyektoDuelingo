#pragma once
#include "GameMode/IGameModeHandler.hpp"
#include "MyUtils/KTools.h"
#include "MyUtils/UnlockRequirements.hpp"

// Deathmatch — endless arcade mode. Mechanically this is just Boss (1v1
// Duel Arena) mode: same setup (EXP/coin/CKR/level-up, 2x HP multiplier,
// no gear, ally bench/swap), same arena rules (isDuelMode() covers both),
// and the same eliminate/force-switch duel model (see
// GameLayer::forceSwitchOnDeath) — a single death eliminates that one
// character and swaps to the next living roster member; a stage only
// actually clears/ends once one side has lost all 3.
// The "endless" part comes from GameOver's start_btn (Deathmatch-only),
// which quick-restarts into the next stage with the same character
// instead of returning to the menu. The stage streak is persisted to
// SQLite once a side is fully eliminated (see onSideEliminated below) --
// win increments and saves it, loss resets it to 0.
//
// Every 10th stage (see isBossRound()) is a boss round: the enemy is
// forced to be one of getBossList()'s reserved characters, gets no
// allies at all, and has 5x HP instead of the usual 2x. Those reserved
// names are excluded from ever showing up as a regular enemy or enemy
// ally on non-boss stages, so the boss encounter stays a surprise.
class ModeDeathmatch : public IGameModeHandler
{
private:
	bool isAddCallback;
	int _stage = 1;

	// Same handler-local resolve/transfer pattern as Boss.hpp — see the
	// comments there for why these are staged here before GameLayer exists.
	vector<string> _pendingAllyRoster;
	vector<string> _pendingEnemyAllyRoster;

	// Editable list of "boss enemy" characters — edit this to add/remove
	// who's eligible to show up as the milestone boss (see isBossRound()
	// below). These names are reserved: outside of an actual boss round,
	// they're excluded from ever being picked as a regular enemy or enemy
	// ally, so the boss encounter stays a surprise rather than something
	// you might've already fought as a normal opponent.
	static const vector<string>& getBossList()
	{
		static const vector<string> bossList = { "Orochimaru", "Itachi", "Pain" };
		return bossList;
	}

	// Every 10th stage is a boss round.
	bool isBossRound() const
	{
		return _stage % 10 == 0;
	}

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

		// initHeros() just picked the enemy (heroDataVector index 1 — index
		// 0 is always the player in this 1-player/1-enemy setup) from the
		// full random pool, which has no way to know about the boss list
		// reservation. Fix that up here via overrideHeroName().
		auto& bossList = getBossList();
		if (isBossRound())
		{
			// Boss round: force the enemy to be a random pick FROM the
			// boss list, and give it no allies at all (see onCharacterInit
			// below for the 5x HP that goes with this).
			setRand();
			string boss = bossList[random((int)bossList.size())];
			overrideHeroName(1, boss);
		}
		else
		{
			// Not a boss round — if the normal random pool happened to
			// land on a reserved boss name anyway, re-roll it excluding
			// the boss list (and the player's own picks, same reasoning
			// as the exclusion set built for the enemy ally roster below).
			string currentPick = getHerosArray().size() > 1 ? getHerosArray()[1].name : "";
			bool pickedReservedBoss = std::find(bossList.begin(), bossList.end(), currentPick) != bossList.end();
			if (pickedReservedBoss)
			{
				vector<string> excludeForReroll = bossList;
				excludeForReroll.push_back(currentPick);
				string rerolled = getRandomHeroExceptAll(excludeForReroll);
				overrideHeroName(1, rerolled);
			}
		}

		// Give the enemy AI a random bench the same size as the player's —
		// symmetry only, no swap logic wired up for it yet. Exclude both
		// duelists, anyone already on the player's bench, and the reserved
		// boss list (see getBossList() above) so those never show up as a
		// regular enemy ally either. Boss rounds skip this entirely — a
		// boss enemy can't have allies.
		if (!isBossRound())
		{
			vector<string> used = _pendingAllyRoster;
			for (const auto& h : getHerosArray())
				used.push_back(h.name);
			used.insert(used.end(), bossList.begin(), bossList.end());

			for (size_t i = 0; i < _pendingAllyRoster.size(); i++)
			{
				auto pick = getRandomHeroExceptAll(used);
				_pendingEnemyAllyRoster.push_back(pick);
				used.push_back(pick);
			}
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

	void onCharacterInit(CharacterBase* c)
	{
		if (!isAddCallback && !getGameLayer()->isHUDInit())
		{
			isAddCallback = true;
			auto layer = getGameLayer();

			layer->_allyRoster = _pendingAllyRoster;
			layer->_enemyAllyRoster = _pendingEnemyAllyRoster;

			// Captured by value rather than calling isBossRound() from
			// inside the lambda -- _stage (and therefore this) doesn't
			// change mid-match, but this keeps the lambda's dependencies
			// explicit and avoids capturing `this` unnecessarily.
			bool isBoss = isBossRound();

			layer->onHUDInitialized(
				[layer, isBoss]()
				{
					if (!layer)
						return;
					for (auto hero : layer->_CharacterArray)
					{
						hero->setCoin(3000);
						hero->setEXP(2500);
						for (int i = 1; i < 6; i++)
							hero->changeHPbar();

						// See the matching comment in Boss.hpp -- the level-up
						// loop above grants a full CKR2 (SKILL5/Ougis2) for
						// free, which combined with the flat increaseAllCkrs
						// grant below let both sides open every stage already
						// able to spam ultimates. Reset CKR2 to empty; it's
						// earned back only through the existing damage-dealt/
						// damage-taken gain in increaseAllCkrs() (unchanged).
						hero->setCKR2(0);
						hero->_isCanOugis2 = false;

						// On a boss round, the enemy (the only non-player
						// character present at this point -- boss rounds
						// give the enemy no allies, see onInitHeros above)
						// gets 5x instead of the usual 2x.
						uint32_t multiplier = (isBoss && !hero->isPlayer()) ? 5 : 2;
						uint32_t newMaxHP = hero->getMaxHP() * multiplier;
						hero->setMaxHPValue(newMaxHP, false);
						hero->setHPValue(newMaxHP, true);

						// CKR (SKILL4) only -- CKR2 (SKILL5) deliberately
						// excluded per the setCKR2(0) reset above.
						hero->increaseAllCkrs(25000, true, false);

						// No auto-reborn — a death is instead handled by
						// forceSwitchOnDeath below, which eliminates this
						// character and swaps to the next living roster
						// member, or ends the stage if that was the last one.
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

		// Same distinction as Boss.hpp — a hero mid-retirement still
		// passes isPlayerOrCom() but isn't the character actually being
		// played right now; see eliminateInactiveHero's comment for why
		// forceSwitchOnDeath would handle this incorrectly.
		if (c->_isRetiring)
		{
			getGameLayer()->eliminateInactiveHero(c);
			return;
		}

		// Used to end the stage immediately here (a single kill/death) --
		// now a death only eliminates that one character and force-switches
		// to the next living roster member, same as Boss mode.
		// forceSwitchOnDeath itself calls onSideEliminated() below (and
		// then onGameOver()) once a side has lost all 3.
		getGameLayer()->forceSwitchOnDeath(c);
	}

	void onCharacterReborn(CharacterBase* c)
	{
	}

	// currentPlayer can be a transformed form (SageNaruto, RikudoNaruto,
	// etc.) if the match ended mid-transformation -- normalize back to the
	// base hero, same mapping GameOver.cpp uses for resultChar, so the
	// record is credited to the hero the player actually picked.
	static string baseHeroName(const string& name)
	{
		if (name == HeroEnum::SageNaruto || name == HeroEnum::RikudoNaruto)
			return HeroEnum::Naruto;
		if (name == HeroEnum::SageJiraiya)
			return HeroEnum::Jiraiya;
		if (name == HeroEnum::ImmortalSasuke)
			return HeroEnum::Sasuke;
		if (name == HeroEnum::RockLee)
			return HeroEnum::Lee;
		if (name == HeroEnum::Nagato)
			return HeroEnum::Pain;
		return name;
	}

	// Fires once a side is fully eliminated (all 3 characters down) --
	// the actual "stage over" moment now, instead of a single kill/death.
	// See GameLayer::processPendingDeaths, which calls this right before
	// onGameOver().
	void onSideEliminated(bool isPlayerSide) override
	{
		if (isPlayerSide)
		{
			// Player's side is out — run's over, reset the persisted
			// *current* streak (used to resume/seed the next run's
			// _stage). The per-hero ArcadeRecordRound best is untouched
			// here: it's a permanent high-water mark, not a live counter,
			// so a loss never resets it.
			KTools::saveDeathmatchStreak(0);
			return;
		}

		// Enemy's side is out — that's the win condition for this stage.
		// Persist the stage just cleared as the new streak; the GameOver
		// screen's start_btn (Deathmatch-only) is what actually continues
		// to the next stage, with the same retained character.
		KTools::saveDeathmatchStreak(_stage);

		// Also update the per-hero best-round record (ArcadeRecordRound)
		// for every character on the player's side this run -- the
		// currently active hero plus both bench allies -- but only if
		// this stage actually beats each hero's own previous best.
		// saveArcadeRecordRoundIfBetter() already no-ops when it isn't.
		auto layer = getGameLayer();
		if (layer && layer->currentPlayer)
		{
			string activeName = baseHeroName(layer->currentPlayer->getName());
			KTools::saveArcadeRecordRoundIfBetter(activeName.c_str(), _stage);

			for (const auto& allyName : layer->_allyRoster)
			{
				if (allyName.empty() || allyName == activeName)
					continue;
				KTools::saveArcadeRecordRoundIfBetter(allyName.c_str(), _stage);
			}
		}

		// A boss round just cleared -- see if that unlocks whichever hero
		// is keyed to this specific boss (e.g. beating Madara unlocks
		// Madara). getHerosArray()[1] is the enemy slot in this 1v1 setup
		// (see onInitHeros() above, which is also where overrideHeroName
		// forces it to a boss list pick on boss rounds).
		if (isBossRound())
		{
			auto& heroes = getHerosArray();
			if (heroes.size() > 1)
				UnlockRequirements::tryUnlockViaArcadeBoss(heroes[1].name);
		}

		// Pick up any ArcadeStageRecord unlock rules that just became
		// satisfied by the ArcadeRecordRound updates above (e.g. NarutoSR
		// unlocking once Naruto's record hits stage 20).
		UnlockRequirements::tryUnlockViaProgress();
	}

	void onSurrender() override
	{
		// Forfeiting via the pause menu no longer resets the persisted
		// streak -- a surrender is treated as "leaving the run as-is"
		// rather than a loss, so _stage stays intact for the next time
		// this run is resumed.
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

		if (isBossRound())
			layer->getHudLayer()->gameClock->setString(format("Stage {} - BOSS", _stage).c_str());
		else
			layer->getHudLayer()->gameClock->setString(format("Stage {}", _stage).c_str());
	}
};
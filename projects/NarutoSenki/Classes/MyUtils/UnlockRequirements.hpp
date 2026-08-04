#pragma once
#include <vector>
#include <functional>
#include <string>
#include "Enums/HeroEnum.h"
#include "MyUtils/KTools.h"

// Data-driven unlock rules for characters that don't start unlocked.
// Everyone NOT listed in kUnlockRules is unlocked by default -- only the
// select few below start locked and need one of these conditions met.
//
// Each hero has exactly one rule *type*, but a rule can require several
// sub-conditions that must ALL be satisfied (e.g. SageKabuto needing both
// Orochimaru AND Kabuto at a stage/win threshold). This matches every
// example given; if a future hero needs multiple alternative unlock paths
// (any-of instead of all-of), this'll need a small extension, but nothing
// here currently requires that.
//
// NOTE: Madara, NarutoSR, Kurenai, Kabuto, and SageKabuto below are either
// placeholders or characters that don't exist in this codebase yet --
// they're wired up here so the unlock system is ready the moment those
// heroes are added; referencing a HeroEnum::X that doesn't exist yet will
// simply fail to compile, which is the signal to add it for real.

namespace UnlockRequirements
{
	enum class RuleType
	{
		// Beat this hero at least once as the final boss encounter in
		// Deathmatch/Arcade mode.
		ArcadeBoss,

		// Reach at least the given ArcadeRecordRound (ADD: stage number)
		// with each of the listed heroes.
		ArcadeStageRecord,

		// Spend a flat amount of coins (GameRecord.coin) to unlock.
		Purchase,

		// Reach at least the given regular win count (CharRecord.column1,
		// i.e. "bonds" -- 1v1/3v3/4v4/etc, NOT duel modes) with each of
		// the listed heroes.
		RegularWins,
	};

	// One (hero, threshold) requirement within a rule. Unused for
	// RuleType::ArcadeBoss and RuleType::Purchase.
	struct HeroThreshold
	{
		const char* heroName;
		int threshold;
	};

	struct UnlockRule
	{
		const char* heroName; // the locked hero this rule is for
		RuleType type;

		// Only meaningful for ArcadeBoss: which hero must be beaten as a
		// boss. Defaults to heroName itself (the common case: beat X to
		// unlock X), but kept separate in case a boss unlocks someone else.
		const char* bossHeroName = nullptr;

		// Only meaningful for Purchase.
		int coinCost = 0;

		// Only meaningful for ArcadeStageRecord / RegularWins. ALL entries
		// must be satisfied. Use an initializer_list-friendly fixed array.
		std::vector<HeroThreshold> requirements;
	};

	// The actual rule table. Add a new locked hero here; anyone not listed
	// is unlocked by default (see isUnlocked() below).
	//
	// UNLOCK_RULES_HAVE_MADARA_AND_NARUTOSR guards the two entries below
	// since this repo doesn't have HeroEnum::Madara / HeroEnum::NarutoSR
	// yet. Define it (e.g. in your local build config, or just delete the
	// #if/#endif once those heroes are merged in) to enable them.
#ifndef UNLOCK_RULES_HAVE_MADARA_AND_NARUTOSR
#define UNLOCK_RULES_HAVE_MADARA_AND_NARUTOSR 1
#endif

	inline const std::vector<UnlockRule>& getUnlockRules()
	{
		static const std::vector<UnlockRule> rules = {
#if UNLOCK_RULES_HAVE_MADARA_AND_NARUTOSR
			// Beat Madara as the Arcade boss once to unlock him.
			{ HeroEnum::Madara, RuleType::ArcadeBoss, HeroEnum::Madara },

			// Reach ArcadeRecordRound >= 20 with Naruto to unlock NarutoSR.
			{ HeroEnum::NarutoSR, RuleType::ArcadeStageRecord, nullptr, 0,
				{ { HeroEnum::Naruto, 20 } } },
#endif

				// -- Placeholder examples (heroes below don't exist yet) --

				// Reach ArcadeRecordRound >= 10 with BOTH Orochimaru and Kabuto.
				// { HeroEnum::SageKabuto, RuleType::ArcadeStageRecord, nullptr, 0,
				//     { { HeroEnum::Orochimaru, 10 }, { HeroEnum::Kabuto, 10 } } },

				// Spend 10000 coins to unlock Kurenai.
				// { HeroEnum::Kurenai, RuleType::Purchase, nullptr, 10000 },

				// 20 regular (non-duel) wins with Orochimaru unlocks Kabuto.
				// { HeroEnum::Kabuto, RuleType::RegularWins, nullptr, 0,
				//     { { HeroEnum::Orochimaru, 20 } } },

				// 10 regular wins each with Naruto, Sakura, Sai, and Kakashi
				// unlocks Yamato.
				// { HeroEnum::Yamato, RuleType::RegularWins, nullptr, 0,
				//     { { HeroEnum::Naruto, 10 }, { HeroEnum::Sakura, 10 },
				//       { HeroEnum::Sai, 10 }, { HeroEnum::Kakashi, 10 } } },
		};
		return rules;
	}

	// Find the rule for a hero, or nullptr if that hero isn't gated at all
	// (i.e. unlocked by default).
	inline const UnlockRule* findRule(const string& heroName)
	{
		for (const auto& rule : getUnlockRules())
		{
			if (heroName == rule.heroName)
				return &rule;
		}
		return nullptr;
	}

	// Whether every (hero, threshold) pair in a requirements list is
	// currently satisfied. Empty list is trivially true.
	inline bool allThresholdsMet(const std::vector<HeroThreshold>& requirements,
		std::function<int(const char*)> readValue)
	{
		for (const auto& req : requirements)
		{
			if (readValue(req.heroName) < req.threshold)
				return false;
		}
		return true;
	}

	// True if the hero is already flagged unlocked in the UnlockedChar
	// table (i.e. was unlocked previously, e.g. via purchase), OR the hero
	// isn't gated by any rule at all (default-unlocked roster).
	//
	// This does NOT check whether the rule's conditions are met right
	// now -- use isEligible() for that. isUnlocked() reflects only what's
	// actually been persisted/granted.
	//
	// KTools::isCharacterUnlocked() treats a hero with no UnlockedChar row
	// as unlocked (since most heroes are never gated and never get a
	// row) -- so for a hero that DOES have a rule here, an absent row
	// means "never explicitly locked yet", which we correct by writing an
	// explicit locked ("0") row the first time this is checked, exactly
	// like KTools::unlockCharacter() lazily creates a row when unlocking.
	inline bool isUnlocked(const string& heroName)
	{
		const UnlockRule* rule = findRule(heroName);
		if (rule == nullptr)
			return true; // not gated -- unlocked by default

		bool unlocked = KTools::isCharacterUnlocked(heroName.c_str());
		if (!unlocked)
			return false;

		// isCharacterUnlocked() returned true, but that could mean either
		// "explicitly unlocked" or "no row exists yet" for this gated
		// hero -- disambiguate via a raw column read.
		auto raw = KTools::readSQLite("UnlockedChar", "name", heroName.c_str(), "unlocked");
		if (!raw.empty())
			return true; // explicitly unlocked already

		// No row yet for a gated hero: lock it now so it stops reading
		// as unlocked from here on, then report locked. This mirrors
		// KTools::unlockCharacter()'s own insert-if-missing handling --
		// saveSQLite() is an UPDATE and is a no-op against a row that
		// doesn't exist yet, so we need KTools::lockCharacter() (a tiny
		// sibling of unlockCharacter that inserts with unlocked="0").
		KTools::lockCharacter(heroName.c_str());
		return false;
	}

	// Whether the hero currently qualifies for unlock based on live
	// progress (ArcadeRecordRound / column1 win counts) -- does NOT check
	// coins for Purchase (that's a deliberate action, not passive
	// progress) and does NOT persist anything. Used to show "ready to
	// unlock!" UI state before the player actually claims it.
	inline bool isEligible(const string& heroName)
	{
		const UnlockRule* rule = findRule(heroName);
		if (rule == nullptr)
			return true;

		if (isUnlocked(heroName))
			return true;

		switch (rule->type)
		{
		case RuleType::ArcadeBoss:
			// No separate "bosses beaten" ledger exists yet -- callers
			// that detect a boss-clear should call
			// tryUnlockViaArcadeBoss() directly at that moment instead
			// of polling this. Treat as not-yet-eligible here.
			return false;

		case RuleType::ArcadeStageRecord:
			return allThresholdsMet(rule->requirements, [](const char* hero) {
				return KTools::readArcadeRecordRound(hero);
				});

		case RuleType::RegularWins:
			return allThresholdsMet(rule->requirements, [](const char* hero) {
				return KTools::readWinNumFromSQL(hero);
				});

		case RuleType::Purchase:
			// Purchase is never "passively" eligible -- it always
			// requires the explicit tryUnlockViaPurchase() action.
			return false;
		}
		return false;
	}

	// Call this the moment a hero is defeated as the Arcade/Deathmatch
	// boss. Unlocks whichever hero (if any) has an ArcadeBoss rule keyed
	// to that boss. No-ops if already unlocked or no such rule exists.
	inline void tryUnlockViaArcadeBoss(const string& bossHeroDefeated)
	{
		for (const auto& rule : getUnlockRules())
		{
			if (rule.type != RuleType::ArcadeBoss)
				continue;

			const char* keyedBoss = rule.bossHeroName ? rule.bossHeroName : rule.heroName;
			if (bossHeroDefeated != keyedBoss)
				continue;

			// See the comment in tryUnlockViaProgress() -- isUnlocked(),
			// not the raw KTools::isCharacterUnlocked(), so a hero that's
			// gated but has no UnlockedChar row yet is correctly treated
			// as locked rather than silently skipped.
			if (!isUnlocked(rule.heroName))
				KTools::unlockCharacter(rule.heroName);
		}
	}

	// Call this after a stage-clear / win-count update (i.e. right after
	// KTools::saveArcadeRecordRoundIfBetter or the column1 win save) to
	// pick up any newly-satisfied ArcadeStageRecord / RegularWins rules.
	// Cheap no-op scan if nothing new qualifies.
	inline void tryUnlockViaProgress()
	{
		for (const auto& rule : getUnlockRules())
		{
			if (rule.type != RuleType::ArcadeStageRecord && rule.type != RuleType::RegularWins)
				continue;

			// NOTE: must use isUnlocked() here, not the raw
			// KTools::isCharacterUnlocked() -- the raw call treats a
			// missing UnlockedChar row as unlocked (correct default for
			// the vast majority of non-gated heroes), but for a hero that
			// IS gated and simply hasn't had a row created yet (e.g.
			// isUnlocked() was never called on it before), that would
			// incorrectly read as "already unlocked" and skip the
			// eligibility check below forever.
			if (isUnlocked(rule.heroName))
				continue;

			if (isEligible(rule.heroName))
				KTools::unlockCharacter(rule.heroName);
		}
	}

	// Attempt to purchase a hero with a Purchase rule. Returns true and
	// deducts coins on success; false (no-op) if the rule doesn't exist,
	// isn't a Purchase rule, is already unlocked, or the player can't
	// afford it.
	inline bool tryUnlockViaPurchase(const string& heroName)
	{
		const UnlockRule* rule = findRule(heroName);
		if (rule == nullptr || rule->type != RuleType::Purchase)
			return false;

		if (KTools::isCharacterUnlocked(heroName.c_str()))
			return false;

		int coins = KTools::readCoinFromSQL();
		if (coins < rule->coinCost)
			return false;

		KTools::saveToSQLite("GameRecord", "coin", std::to_string(coins - rule->coinCost).c_str(), false);
		KTools::unlockCharacter(heroName.c_str());
		return true;
	}
}
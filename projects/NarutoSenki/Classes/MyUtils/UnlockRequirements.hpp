#pragma once
#include <vector>
#include <functional>
#include <string>
#include "Enums/HeroEnum.h"
#include "MyUtils/KTools.h"

namespace UnlockRequirements
{
	enum class RuleType
	{
		ArcadeBoss,
		ArcadeStageRecord,
		Purchase,
		RegularWins,
	};

	struct HeroThreshold
	{
		const char* heroName;
		int threshold;
	};

	struct UnlockRule
	{
		const char* heroName;
		RuleType type;

		const char* bossHeroName = nullptr;
		int coinCost = 0;

		std::vector<HeroThreshold> requirements;
	};

#ifndef UNLOCK_RULES_HAVE_MADARA_AND_NARUTOSR
#define UNLOCK_RULES_HAVE_MADARA_AND_NARUTOSR 1
#endif

	inline const std::vector<UnlockRule>& getUnlockRules()
	{
		static const std::vector<UnlockRule> rules = {
#if UNLOCK_RULES_HAVE_MADARA_AND_NARUTOSR
			{ HeroEnum::Madara, RuleType::ArcadeBoss, HeroEnum::Madara },
			{ HeroEnum::NarutoSR, RuleType::ArcadeStageRecord, nullptr, 0,
				{ { HeroEnum::Naruto, 20 } } },
#endif
		};
		return rules;
	}

	/* Hero names newly unlocked during this session awaiting notification display. */
	inline std::vector<std::string>& pendingUnlockNotifications()
	{
		static std::vector<std::string> pending;
		return pending;
	}

	/* Pops and clears all queued unlock notifications. */
	inline std::vector<std::string> takePendingUnlockNotifications()
	{
		std::vector<std::string> result;
		result.swap(pendingUnlockNotifications());
		return result;
	}

	inline const UnlockRule* findRule(const string& heroName)
	{
		for (const auto& rule : getUnlockRules())
		{
			if (heroName == rule.heroName)
				return &rule;
		}
		return nullptr;
	}

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

	/* Checks if character is unlocked. Lazily creates explicit locked row if missing for a gated hero. */
	inline bool isUnlocked(const string& heroName)
	{
		const UnlockRule* rule = findRule(heroName);
		if (rule == nullptr)
			return true;

		bool unlocked = KTools::isCharacterUnlocked(heroName.c_str());
		if (!unlocked)
			return false;

		auto raw = KTools::readSQLite("UnlockedChar", "name", heroName.c_str(), "unlocked");
		if (!raw.empty())
			return true;

		KTools::lockCharacter(heroName.c_str());
		return false;
	}

	/* Evaluates live progression requirements without persisting state. */
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
			return false;
		}
		return false;
	}

	/* Unlocks character gated behind boss defeat if applicable. */
	inline void tryUnlockViaArcadeBoss(const string& bossHeroDefeated)
	{
		for (const auto& rule : getUnlockRules())
		{
			if (rule.type != RuleType::ArcadeBoss)
				continue;

			const char* keyedBoss = rule.bossHeroName ? rule.bossHeroName : rule.heroName;
			if (bossHeroDefeated != keyedBoss)
				continue;

			if (!isUnlocked(rule.heroName))
			{
				KTools::unlockCharacter(rule.heroName);
				pendingUnlockNotifications().push_back(rule.heroName);
			}
		}
	}

	/* Evaluates and unlocks any progression-based rules satisfied by recent progress. */
	inline void tryUnlockViaProgress()
	{
		for (const auto& rule : getUnlockRules())
		{
			if (rule.type != RuleType::ArcadeStageRecord && rule.type != RuleType::RegularWins)
				continue;

			if (isUnlocked(rule.heroName))
				continue;

			if (isEligible(rule.heroName))
			{
				KTools::unlockCharacter(rule.heroName);
				pendingUnlockNotifications().push_back(rule.heroName);
			}
		}
	}

	/* Deducts coins and unlocks character if affordable. */
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

	inline string formatRequirementLine(RuleType type, const HeroThreshold& req)
	{
		if (type == RuleType::ArcadeStageRecord)
			return format("Beat Round {} in Arcade with {} on your team", req.threshold, req.heroName);
		else
			return format("Get {} wins as {}", req.threshold, req.heroName);
	}

	/* Returns formatted string description lines for UI unlock requirements popup. */
	inline std::vector<string> getUnlockConditionLines(const string& heroName)
	{
		const UnlockRule* rule = findRule(heroName);
		if (rule == nullptr)
			return {};

		if (rule->type == RuleType::ArcadeBoss)
		{
			const char* boss = rule->bossHeroName ? rule->bossHeroName : rule->heroName;
			return { format("Beat {} in Arcade", boss) };
		}

		if (rule->type == RuleType::Purchase)
		{
			return { format("Spend {} coins to unlock?", rule->coinCost) };
		}

		if (rule->requirements.empty())
			return {};

		if (rule->requirements.size() == 1)
			return { formatRequirementLine(rule->type, rule->requirements[0]) };

		auto readValue = [&](const char* hero) {
			return rule->type == RuleType::ArcadeStageRecord
				? KTools::readArcadeRecordRound(hero)
				: KTools::readWinNumFromSQL(hero);
			};

		int completed = 0;
		for (const auto& req : rule->requirements)
		{
			if (readValue(req.heroName) >= req.threshold)
				completed++;
		}

		std::vector<string> lines;
		lines.push_back(format("{}/{} completed", completed, (int)rule->requirements.size()));
		for (const auto& req : rule->requirements)
			lines.push_back(formatRequirementLine(rule->type, req));
		return lines;
	}

	/* Newline-joined string format for Lua binding compatibility. */
	inline string getUnlockConditionLinesJoined(const string& heroName)
	{
		auto lines = getUnlockConditionLines(heroName);
		string joined;
		for (size_t i = 0; i < lines.size(); i++)
		{
			if (i > 0)
				joined += "\n";
			joined += lines[i];
		}
		return joined;
	}
}
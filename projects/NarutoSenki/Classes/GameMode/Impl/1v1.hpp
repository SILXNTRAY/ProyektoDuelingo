#pragma once
#include "GameMode/IGameModeHandler.hpp"

class Mode1v1 : public IGameModeHandler
{
private:
	bool isAddCallback;

public:
	void init()
	{
		CCLOG("Enter 1 VS 1 mode.");

		isAddCallback = false;

		gd.isHardCore = true;
	}

	void onInitHeros()
	{
		clearHeroArray();
		setRand();

		// Randomly assign player group
		Group playerGrp = random(2) > 0 ? Group::Akatsuki : Group::Konoha;
		Group enemyGrp = playerGrp == Group::Konoha ? Group::Akatsuki : Group::Konoha;
		this->playerGroup = playerGrp;
		this->gd.playerGroup = playerGrp;

		// Resolve player character
		string playerChar;
		if (selectLayer->_playerSelect)
		{
			playerChar = selectLayer->_playerSelect;
		}
		else
		{
			gd.isRandomChar = true;
			do {
				setRand();
				playerChar = kHeroList[random((int)kHeroNum)];
			} while (playerChar == "None");
			selectLayer->_playerSelect = playerChar.c_str();
		}

		// Resolve opponent — com1Select is the chosen enemy
		string opponentChar;
		if (selectLayer->_com1Select)
		{
			opponentChar = selectLayer->_com1Select;
		}
		else
		{
			do {
				setRand();
				opponentChar = kHeroList[random((int)kHeroNum)];
			} while (opponentChar == playerChar || opponentChar == "None");
		}

		addHero(playerChar.c_str(), Role::Player, playerGrp);
		addHero(opponentChar.c_str(), Role::Com, enemyGrp);
	}

	void onGameStart()
	{
		getGameLayer()->_enableGuardian = false;
	}

	void onGameOver()
	{
	}

	void onCharacterInit(CharacterBase *c)
	{
		if (!isAddCallback && !getGameLayer()->isHUDInit())
		{
			// NOTE: Following code won't get correct value
			// Because this function `onCharacterInit` is not in current stack
			// So this value always nullptr or wrong value
			//
			// getGameLayer()->onHUDInitialized(
			// 	[&c]()
			// 	{
			// 		c->setEXP(2500);
			// 		for (int i = 1; i < 6; i++)
			// 			c->changeHPbar();
			// 		c->increaseAllCkrs(50000);
			// 	});
			isAddCallback = true;
			auto layer = getGameLayer();
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
						hero->setHPValue(hero->getMaxHP());
						hero->increaseAllCkrs(25000);
						hero->setRebornTime(10);

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
	}

	void onCharacterReborn(CharacterBase *c)
	{
	}
};

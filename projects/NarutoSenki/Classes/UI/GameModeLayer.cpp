#include "StartMenu.h"
#include "UI/GameModeLayer.h"
#include "UI/ModeMenuButton.hpp"
#include "Constants/UiFlowKeys.hpp"
#include "MyUtils/KTools.h"

extern const GameData kDefaultGameData;

// const GameModeData &GameModeData::from(const char *path)
// {
//     GameModeData data = {}
//     return data;
// }

bool GameModeLayer::init()
{
	auto bgSprite = Sprite::create("red_bg.png");
	FULL_SCREEN_SPRITE(bgSprite);
	bgSprite->setAnchorPoint(Vec2(0, 0));
	bgSprite->setPosition(Vec2(0, 0));
	addChild(bgSprite, -100);

	// menu bars
	auto menu_bar_b = Sprite::create("menu_bar2.png");
	menu_bar_b->setAnchorPoint(Vec2(0, 0));
	FULL_SCREEN_SPRITE(menu_bar_b);
	addChild(menu_bar_b, 2);

	auto menu_bar_t = Sprite::create("menu_bar3.png");
	menu_bar_t->setAnchorPoint(Vec2(0, 0));
	menu_bar_t->setPosition(Vec2(0, winSize.height - menu_bar_t->getContentSize().height));
	FULL_SCREEN_SPRITE(menu_bar_t);
	addChild(menu_bar_t, 2);

	auto modemenu_title = Sprite::createWithSpriteFrameName("startmenu_title.png");
	modemenu_title->setAnchorPoint(Vec2(0, 0));
	modemenu_title->setPosition(Vec2(2, winSize.height - modemenu_title->getContentSize().height - 2));
	addChild(modemenu_title, 3);

	initModeData();

	// init menus
	// const int padding = -10;
	// const int width = 100;
	const int offset = (winSize.width - 460) / 2 + 100 / 2;
	const float posY = (winSize.height / 2) + 30;
	for (int i = 0; i < 3; i++)
	{
		auto mode_btn = ModeMenuButton::create(format("GameMode/{}.png", i + 1));
		mode_btn->mode = (GameMode)i;
		mode_btn->setDelegate(this);
		mode_btn->setPositionX(offset);
		mode_btn->setPositionY((posY + 55 + 7.5f) - i * (55 + 7.5f));
		menuButtons[i] = mode_btn;
		addChild(mode_btn);
	}
	for (int i = 3; i < 6; i++)
	{
		auto mode_btn = ModeMenuButton::create(format("GameMode/{}.png", i + 1));
		mode_btn->mode = (GameMode)i;
		mode_btn->setDelegate(this);
		mode_btn->setPositionX(offset + 10 + (i - 2) * (80 + 5));
		mode_btn->setPositionY(posY);
		menuButtons[i] = mode_btn;
		addChild(mode_btn);
		// init animation
		// auto delay = DelayTime::create(i * 0.3f);
		// auto move = MoveTo::create(0.5f, Vec2((i - 1) * (width + padding) + offset, posY + 30));
		// auto action = Sequence::createWithTwoActions(delay, move);
		// mode_btn->runAction(action);
	}
	for (size_t i = 6; i < GameMode::__Internal_Max_Length; i++)
	{
		auto mode_btn = ModeMenuButton::create(format("GameMode/{}.png", i + 1));
		mode_btn->mode = (GameMode)i;
		mode_btn->setDelegate(this);
		mode_btn->setPositionX(offset + 20 + (80 + 5) * 4);
		mode_btn->setPositionY((posY + 47) - (i - 6) * (86 + 8.0f));
		menuButtons[i] = mode_btn;
		addChild(mode_btn);
	}

	for (size_t i = 0; i < menuButtons.size(); i++)
	{
		if (modes[i].isLocked)
		{
			menuButtons.at(i)->useMask2 = modes.at(i).useMask2;
			menuButtons.at(i)->lock();
		}
	}

	menuLabel = CCLabelTTF::create();
	menuLabel->setAnchorPoint(Vec2(0, 0));
	menuLabel->setPosition(Vec2(10, 2));
	addChild(menuLabel, 5);

	// init return button
	auto return_img = MenuItemSprite::create(Sprite::create("UI/return_btn.png"), nullptr, nullptr, this, menu_selector(GameModeLayer::backToMenu));
	Menu *return_btn = Menu::create(return_img, nullptr);
	return_btn->setAnchorPoint(Vec2(1, 0.5f));
	return_btn->setPosition(winSize.width - 38, 65);
	addChild(return_btn, 5);

	return Layer::init();
}

void GameModeLayer::backToMenu(Ref *sender)
{
	SimpleAudioEngine::sharedEngine()->playEffect("Audio/Menu/cancel.ogg");

	auto menuScene = Scene::create();
	auto menuLayer = StartMenu::create();
	menuScene->addChild(menuLayer);
	Director::sharedDirector()->replaceScene(TransitionFade::create(1.0f, menuScene));
}

void GameModeLayer::initModeData()
{
	// init mode text data
	auto lang = Application::sharedApplication()->getCurrentLanguage();
	if (lang == LanguageType::kLanguageChinese)
	{
		modes[GameMode::OneVsOne] = {"1 VS 1", ""};
		modes[GameMode::Classic] = {"3 VS 3", "经典模式"};
		modes[GameMode::FourVsFour] = {"4 VS 4", ""};
		modes[GameMode::HardCore_4Vs4] = {"硬核模式 (4 VS 4)", "禁用装备"};
		modes[GameMode::Boss] = {"决斗模式 (1 VS 1)", ""};
		modes[GameMode::Clone] = {"克隆模式 (3 VS 3)", ""};
		modes[GameMode::Deathmatch] = {"死亡竞赛 (无尽 1 VS 1)", ""};
		modes[GameMode::RandomDeathmatch] = {"随机死亡竞赛 (3 VS 3)", ""};
	}
	else // English
	{
		modes[GameMode::OneVsOne] = {"1 VS 1", ""};
		modes[GameMode::Classic] = {"3 VS 3", "Classic Mode"};
		modes[GameMode::FourVsFour] = {"4 VS 4", ""};
		modes[GameMode::HardCore_4Vs4] = {"HardCore (4 VS 4)", "Disabled gear"};
		modes[GameMode::Boss] = {"Duel (1 VS 1 Arena)", ""};
		modes[GameMode::Clone] = {"Clone (3 VS 3)", ""};
		modes[GameMode::Deathmatch] = {"Deathmatch (Endless 1 VS 1)", ""};
		modes[GameMode::RandomDeathmatch] = {"Random Deathmatch (3 VS 3)", ""};
	}

	// init mode handlers
	for (size_t i = 0; i < GameMode::__Internal_Max_Length; i++)
	{
		auto &data = modes.at(i);
		if (data.isLocked)
			data.description += " (In developtment)";
		// data.handler = s_ModeHandlers[i];
	}
}

bool GameModeLayer::pushMode(const GameModeData &data)
{
	return true;
}

void GameModeLayer::removeMode(const GameModeData &data)
{
}

void GameModeLayer::selectMode(GameMode mode)
{
	auto data = modes[(size_t)mode];
	string label = data.title;
	if (data.description.size() > 0)
	{
		label += " | ";
		label += data.description;
	}
	menuLabel->setString(label.c_str());

	if (setSelect(mode))
	{
		for (size_t i = 0; i < modes.size(); i++)
			modes.at(i).isLocked = true;
		CCLOG("Selected %s mode", data.title.c_str());

		if (mode == GameMode::Deathmatch)
		{
			string savedPlayer, savedAlly1, savedAlly2;
			if (KTools::readDeathmatchStreak() > 0 && KTools::readDeathmatchTeam(savedPlayer, savedAlly1, savedAlly2))
			{
				showDeathmatchContinuePrompt(savedPlayer, savedAlly1, savedAlly2);
				return;
			}
		}

		enterMode(mode);
	}
}

void GameModeLayer::enterMode(GameMode mode)
{
	s_GameMode = mode;
	bool enableCustomSelect = false;
	if (Cheats < kMaxCheats && (mode == GameMode::FourVsFour || mode == GameMode::HardCore_4Vs4))
	{
		enableCustomSelect = false;
	}
	else if (mode != GameMode::RandomDeathmatch && mode != GameMode::Clone)
	{
		enableCustomSelect = Cheats >= kMaxCheats;
	}

	// call lua global function StartMenu.enterSelectLayer
	auto pStack = get_luastack;
	auto state = pStack->getLuaState();
	lua_getglobal(state, UiFlowKeys::kEnterSelectLayer);
	pStack->pushInt(mode);
	pStack->pushBoolean(enableCustomSelect);
	pStack->executeFunction(2);

	auto handler = getGameModeHandler();
	handler->setOldCheats(Cheats);
	handler->init();
}

void GameModeLayer::showDeathmatchContinuePrompt(const string &playerChar, const string &ally1, const string &ally2)
{
	_dmSavedPlayer = playerChar;
	_dmSavedAlly1 = ally1;
	_dmSavedAlly2 = ally2;

	if (dmPromptLayer)
		return;

	SimpleAudioEngine::sharedEngine()->playEffect("Audio/Menu/select.ogg");
	dmPromptLayer = Layer::create();

	// Reusing GameOver's "back to menu" confirm dialog assets as-is for
	// now, just repurposed for a different question.
	auto bg = Sprite::createWithSpriteFrameName("confirm_bg.png");
	bg->setPosition(Vec2(winSize.width / 2, winSize.height / 2));

	auto title = Sprite::createWithSpriteFrameName("confirm_title.png");
	title->setPosition(Vec2(winSize.width / 2, winSize.height / 2 + 38));

	auto text = Sprite::createWithSpriteFrameName("btm_text.png");
	text->setPosition(Vec2(winSize.width / 2, winSize.height / 2 + 8));

	MenuItem *yes_btn = MenuItemSprite::create(Sprite::createWithSpriteFrameName("yes_btn1.png"), Sprite::createWithSpriteFrameName("yes_btn2.png"), this, menu_selector(GameModeLayer::onDeathmatchContinueYes));
	MenuItem *no_btn = MenuItemSprite::create(Sprite::createWithSpriteFrameName("no_btn1.png"), Sprite::createWithSpriteFrameName("no_btn2.png"), this, menu_selector(GameModeLayer::onDeathmatchContinueNo));

	Menu *confirm_menu = Menu::create(yes_btn, no_btn, nullptr);
	confirm_menu->alignItemsHorizontallyWithPadding(24);
	confirm_menu->setPosition(Vec2(winSize.width / 2, winSize.height / 2 - 30));

	dmPromptLayer->addChild(bg, 1);
	dmPromptLayer->addChild(confirm_menu, 2);
	dmPromptLayer->addChild(title, 2);
	dmPromptLayer->addChild(text, 2);
	addChild(dmPromptLayer, 500);
}

void GameModeLayer::onDeathmatchContinueYes(Ref *sender)
{
	SimpleAudioEngine::sharedEngine()->playEffect("Audio/Menu/confirm.ogg");
	dmPromptLayer->removeFromParent();
	dmPromptLayer = nullptr;

	s_GameMode = GameMode::Deathmatch;
	auto handler = getGameModeHandler();
	handler->setOldCheats(Cheats);
	handler->init();

	// We're skipping SelectLayer's interactive screen entirely, but it's
	// still what's responsible for loading these -- load the same set
	// directly so nothing further down the line (LoadLayer, HUD, etc.)
	// reaches for a sprite frame that was never brought in.
	addSprites("Record.plist");
	addSprites("Record2.plist");
	addSprites("Select.plist");
	addSprites("UI.plist");
	addSprites("Report.plist");
	addSprites("Ougis.plist");
	addSprites("Ougis2.plist");
	addSprites("Map.plist");
	addSprites("Gears.plist");

	auto tempSelect = SelectLayer::create();
	tempSelect->setSelectHero(_dmSavedPlayer.c_str());
	if (!_dmSavedAlly1.empty())
		tempSelect->setCom1Select(_dmSavedAlly1.c_str());
	if (!_dmSavedAlly2.empty())
		tempSelect->setCom2Select(_dmSavedAlly2.c_str());

	handler->selectLayer = tempSelect;
	handler->onInitHeros();

	auto loadScene = Scene::create();
	auto loadLayer = LoadLayer::create();
	loadLayer->preloadAudio();
	loadScene->addChild(loadLayer);

	Director::sharedDirector()->replaceScene(TransitionFade::create(1.0f, loadScene));
}

void GameModeLayer::onDeathmatchContinueNo(Ref *sender)
{
	SimpleAudioEngine::sharedEngine()->playEffect("Audio/Menu/cancel.ogg");
	dmPromptLayer->removeFromParent();
	dmPromptLayer = nullptr;

	KTools::saveDeathmatchStreak(0);

	enterMode(GameMode::Deathmatch);
}

bool GameModeLayer::setSelect(GameMode mode)
{
	auto &data = modes.at(mode);
	if (!data.isLocked && data.hasSelected)
		return true;

	for (size_t i = 0; i < modes.size(); i++)
		modes.at(i).hasSelected = false;
	data.hasSelected = true;
	return false;
}

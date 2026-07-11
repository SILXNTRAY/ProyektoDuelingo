#pragma once
#include "GameMode/GameModeImpl.h"

class ModeMenuButton;

class GameModeLayer : public Layer
{
public:
	static const int kMenuCount = 5;

	bool init();
	void backToMenu(Ref *sender);

	void initModeData();
	bool pushMode(const GameModeData &data);
	void removeMode(const GameModeData &data);
	void selectMode(GameMode mode);

	CREATE_FUNC(GameModeLayer);

private:
	inline bool setSelect(GameMode mode);
	void enterMode(GameMode mode);

	// Deathmatch continue/new-team prompt -- reuses the same confirm
	// dialog visuals as GameOver's "back to menu" confirm, for now.
	void showDeathmatchContinuePrompt(const string &playerChar, const string &ally1, const string &ally2);
	void onDeathmatchContinueYes(Ref *sender);
	void onDeathmatchContinueNo(Ref *sender);

	Layer *dmPromptLayer = nullptr;
	string _dmSavedPlayer;
	string _dmSavedAlly1;
	string _dmSavedAlly2;

	CCLabelTTF *menuLabel = nullptr;

	vector<ModeMenuButton *> menuButtons = vector<ModeMenuButton *>(static_cast<size_t>(GameMode::__Internal_Max_Length));
	vector<GameModeData> modes = vector<GameModeData>(static_cast<size_t>(GameMode::__Internal_Max_Length));
};

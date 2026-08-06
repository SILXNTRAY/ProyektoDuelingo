#pragma once
#include "Defines.h"
#if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)
#include "../../../cocos2dx/platform/android/jni/JniHelper.h"
#endif

class GameOver : public Layer
{
public:
	GameOver();
	~GameOver();

	bool init(RenderTexture *snapshoot);

	PROP(bool, _isWin, Win);

	Layer *exitLayer = nullptr;
	Layer *cheatLayer = nullptr;
	Layer *unlockedLayer = nullptr;
	Sprite *result_bg = nullptr;

	Sprite *refreshBtn = nullptr;
	MenuItem *upload_btn = nullptr;
	MenuItem *start_btn = nullptr;

	string detailRecord;
	float finnalScore;

	static GameOver *create(RenderTexture *snapshoot);

private:
	// FIXED
	string resultChar;

	// Heroes newly unlocked this GameOver screen (drained from
	// UnlockRequirements' pending-notification queue in listResult()),
	// still waiting to have their "X has been unlocked!" popup shown.
	// Queued rather than shown all at once so multiple simultaneous
	// unlocks each get their own popup, one after another.
	std::vector<string> _unlockPopupQueue;

	void onBackToMenu(Ref *sender);
	void listResult();
	void onCancel(Ref *sender);
	void onLeft(Ref *sender);
	void onUPloadBtn(Ref *sender);
	void onStartNextStage(Ref *sender);

	void showNextUnlockedPopup();
	void onUnlockedPopupOK(Ref *sender);
};

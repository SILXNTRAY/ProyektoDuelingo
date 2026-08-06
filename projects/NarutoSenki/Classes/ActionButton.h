#pragma once
#include "Defines.h"

class HudLayer;

class ActionButton : public Sprite, public CCTouchDelegate
{
public:
	ActionButton();

	int _clickNum;
	float _clickTime;
	ProgressTimer *markSprite;
	Sprite *ougismarkSprite;
	Sprite *proressmarkSprite;
	Sprite *proressblinkSprite;
	Sprite *progressPointSprite;
	Sprite *proressblinkMask;
	const char *_cost;
	ClippingNode *clipper;
	bool _isLock;

	bool _isColdChanged;

	bool init(const string &szImage);
	bool isCanClick();
	CCRect getRect();

	void click();
	void setMarkSprite(const char *mark);
	void setOugisMark();
	void setProgressMark();
	void updateProgressMark();
	void reset();

	Sprite *gearSign;
	CCLabelBMFont *cdLabel;

	CCLabelBMFont *lockLabel1;
	CCLabelBMFont *lockLabel2;

	bool _isMarkVisable;

	PROP(ABType, _abType, ABType);
	GearType _gearType;

	PROP_UInt(_cooldown, CD);
	PROP(bool, _isDoubleSkill, DoubleSkill);
	PROP_UInt(_timeCount, TimeCount);

	void setLock(bool showX = false);
	void unLock();
	void clearOugisMark();
	void setGearType(GearType type);

	// Duel-mode hold-to-charge: while the player holds item1Button down,
	// this accumulates time and drives the shared chakra pool via a
	// quadratic ramp (slow start → fast end). Only active in duel modes
	// (Boss/Deathmatch) and only for the Item1 button. See
	// ActionButton.cpp's updateChakraCharge() for the math.
	float _chakraChargeHeld = 0.f; // seconds held so far this press
	bool  _isChargingChakra = false;
	void  startChakraCharge();
	void  stopChakraCharge();
	void  updateChakraCharge(float dt);

	PROP_PTR(Action, _freezeAction, FreezeAction);
	PROP(HudLayer *, _delegate, Delegate);
	void beganAnimation(bool isLock = false);
	void updateCDLabel(float dt);
	// Bumps _timeCount up to at least minMs (never down — won't shorten a
	// longer cooldown already in progress) and makes sure it'll actually
	// decay back to 0 on its own. Unlike poking setTimeCount() directly,
	// this is safe to call on a button that wasn't otherwise just clicked:
	// updateCDLabel is the only thing that ever counts _timeCount back
	// down, and it's only ever scheduled from inside beganAnimation(), so
	// setting a nonzero value without going through here would leave it
	// stuck forever with nothing left to bring it back to 0.
	void applyMinCooldown(uint32_t minMs);

	static ActionButton *create(const string &szImage);

protected:
	void onEnter();
	void onExit();
	bool ccTouchBegan(Touch *touch, Event *event);
	void ccTouchEnded(Touch *touch, Event *event);
	void ccTouchCancelled(Touch *touch, Event *event);

	void createFreezeAnimation();
	void clearClick();

	bool containsTouchLocation(Touch *touch);
};

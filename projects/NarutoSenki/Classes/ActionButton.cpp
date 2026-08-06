#include "ActionButton.h"
#include "CharacterBase.h"
#include "HudLayer.h"

ActionButton::ActionButton()
{
	_isDoubleSkill = false;
	_freezeAction = nullptr;
	markSprite = nullptr;
	ougismarkSprite = nullptr;
	_clickTime = 0;
	_clickNum = 0;
	_isMarkVisable = true;
	_isLock = false;
	_isColdChanged = false;
	clipper = nullptr;
	_cost = nullptr;
	_gearType = GearType::None;
	cdLabel = nullptr;
	lockLabel1 = nullptr;
	lockLabel2 = nullptr;

	proressblinkSprite = nullptr;
	proressmarkSprite = nullptr;
	proressblinkSprite = nullptr;
	progressPointSprite = nullptr;
	proressblinkMask = nullptr;
	gearSign = nullptr;
}

bool ActionButton::init(const string &szImage)
{
	RETURN_FALSE_IF(!Sprite::init());

	initWithSpriteFrameName(szImage.c_str());
	setAnchorPoint(Vec2(0, 0));

	return true;
}

void ActionButton::onEnter()
{
	Sprite::onEnter();
	Director::sharedDirector()->getTouchDispatcher()->addTargetedDelegate(this, -50, true);
}

void ActionButton::onExit()
{
	Sprite::onExit();
	Director::sharedDirector()->getTouchDispatcher()->removeDelegate(this);
}

CCRect ActionButton::getRect()
{
	CCSize size = getContentSize();
	return CCRect(0, 0, size.width, size.height);
}

bool ActionButton::containsTouchLocation(Touch *touch)
{
	return getRect().containsPoint(convertTouchToNodeSpace(touch));
}

bool ActionButton::ccTouchBegan(Touch *touch, Event *event)
{
	if (!containsTouchLocation(touch) || _delegate->_isAllButtonLocked)
	{
		return false;
	}

	// Duel-mode ramen button: no CD, hold to charge chakra instead of
	// triggering the normal click/heal flow. The press is consumed here
	// entirely -- ccTouchEnded() stops the charge and the heal never fires.
	if (_abType == Item1 && _delegate->isDuelMode())
	{
		startChakraCharge();
		return true;
	}

	click();
	return true;
}

void ActionButton::ccTouchEnded(Touch *touch, Event *event)
{
	if (_abType == NAttack)
		_delegate->attackButtonRelease();

	if (_abType == Item1 && _isChargingChakra)
		stopChakraCharge();
}

void ActionButton::ccTouchCancelled(Touch *touch, Event *event)
{
	// A held charge needs to stop even if the touch never reaches
	// ccTouchEnded (e.g. interrupted by a system alert or another touch
	// taking over) -- otherwise updateChakraCharge() would keep running
	// against a finger that's no longer down.
	if (_abType == Item1 && _isChargingChakra)
		stopChakraCharge();
}

// ---------------------------------------------------------------------------
// Duel-mode hold-to-charge chakra (Item1 / ramen button)
// ---------------------------------------------------------------------------
//
// Design target: holding from empty (0) to full (7 bars = 105000 units)
// takes exactly 3.8 seconds. The charge rate follows a linear ramp so it
// starts slow (feels fair -- you can't instantly jam skill5) and ends fast
// (rewarding sustained holds). Given:
//
//   pool(t) = r0·t + ½·a·t²              (quadratic integration of linear rate)
//   rate(t) = r0 + a·t                   (linear ramp)
//   pool(T) = 105000,  T = 3.8 s
//   r0      = 5000 units/s               (initial, slow-start feel)
//
// Solving: 1.9·(r1 - r0) = 105000 - 19000  =>  r1 ≈ 50263  =>  a ≈ 11911
//
// Each scheduler tick: Δpool = rate(t_held) · dt  (clamped to kDuelChakraMax)
//
// The button: has no cooldown in duel mode (the CD is never started; the
// ramen heal never fires). The charge starts on touch-down, stops on
// touch-up. A short "freeze" animation plays on touch-down as visual feedback.

// Rate constants (kChargeR0/kChargeA/kChargeMaxT) now live on GameLayer
// alongside the other kDuelChakra* constants -- see GameLayer.h -- so the
// AI's own idle-charge behavior (CharacterBase::checkRetri()) can share
// the exact same math instead of risking drift from a second copy.

void ActionButton::startChakraCharge()
{
	if (_isChargingChakra)
		return;

	// Can only start a charge while standing idle -- mid-attack, mid-hurt,
	// walking, etc. all block it. (updateChakraCharge() below re-checks
	// this every tick too, so a charge that starts idle still gets cut
	// off the moment idle is interrupted, e.g. by getting hit.)
	auto* player = getGameLayer()->currentPlayer;
	if (!player || player->getState() != State::IDLE)
		return;

	_isChargingChakra = true;
	_chakraChargeHeld = 0.f;

	// Press-feedback visual only (matches other buttons' "began" look)
	// without going through beganAnimation(), since that unconditionally
	// starts the CD timer -- this button has none in duel mode.
	if (markSprite && _abType != OUGIS1 && _abType != OUGIS2)
	{
		if (!_freezeAction || _isColdChanged)
		{
			_isColdChanged = false;
			createFreezeAnimation();
		}
		markSprite->stopAllActions();
		markSprite->runAction(_freezeAction);
	}

	schedule(schedule_selector(ActionButton::updateChakraCharge), 0.016f); // ~60 Hz
}

void ActionButton::stopChakraCharge()
{
	if (!_isChargingChakra)
		return;

	_isChargingChakra = false;
	_chakraChargeHeld = 0.f;
	unschedule(schedule_selector(ActionButton::updateChakraCharge));
}

void ActionButton::updateChakraCharge(float dt)
{
	if (!_delegate->isDuelMode() || !_isChargingChakra)
	{
		stopChakraCharge();
		return;
	}

	auto* player = getGameLayer()->currentPlayer;
	// Idle is required to keep charging, not just to start -- anything
	// that breaks idle (getting hit into HURT/KNOCKDOWN/AIRHURT, using a
	// skill, walking, etc.) cuts the charge off immediately, same as if
	// the touch had been released.
	if (!player || player->getState() != State::IDLE)
	{
		stopChakraCharge();
		return;
	}

	// Clamp held time so rate never exceeds what it was at the 3.8s mark.
	float t = (std::min)(_chakraChargeHeld, GameLayer::kChargeMaxT);
	float rate = GameLayer::kChargeR0 + GameLayer::kChargeA * t;
	float delta = rate * dt;

	_chakraChargeHeld += dt;

	uint32_t gain = static_cast<uint32_t>(delta + 0.5f); // round to nearest
	if (gain == 0) gain = 1;

	getGameLayer()->addDuelChakra(true, gain); // true = player side; also
	                                            // refreshes the bar/label
	                                            // and skill4/5 button dials

	// Sync the player's own ougis-castable flags in case a bar just tipped over.
	getGameLayer()->syncDuelOugisFlags(player, true);
}

// ---------------------------------------------------------------------------

void ActionButton::click()
{
	if (_delegate && isCanClick())
	{
		if (!_isDoubleSkill)
		{
			beganAnimation();
		}

		if (!_delegate->_isAllButtonLocked)
		{
			if (_abType == GearItem)
			{
				_delegate->gearButtonClick(_gearType);
			}
			else
			{
				_delegate->attackButtonClick(_abType);
			}
		}
	}
}

bool ActionButton::isCanClick()
{
	// recorde current time sec format;

	// cc_timeval timeVal;
	// CCTime::gettimeofdayCocos2d(&timeVal, 0);
	// float currTime = timeVal.tv_sec + timeVal.tv_usec / 1000;

	if (_abType != NAttack)
	{
		if (_isDoubleSkill)
		{
			// double click solution
			if (_clickNum == 0 && _delegate->getSkillFinish() && getTimeCount() == 0 && !_delegate->ougisLayer)
			{
				return true;
			}
			else if (_clickNum == 1 && _delegate->getSkillFinish() && !_delegate->ougisLayer)
			{
				return true;
			}
		}
		else
		{
			// isSkillFinish consider the AttackAction is done or not to prevent the skill invalid release
			if (_abType == Item1)
			{
				if (!_delegate->ougisLayer && getTimeCount() == 0 && !_isLock && getGameLayer()->currentPlayer->getState() != State::DEAD)
				{
					if (_delegate->offCoin(_cost))
					{
						return true;
					}
				}
			}
			else if (_abType == GearBtn)
			{
				if (!_isLock)
				{
					getGameLayer()->onGear();
				}
				return false;
			}
			else if (_abType == GearItem)
			{
				if (!_delegate->ougisLayer && getTimeCount() == 0 && !_isLock)
				{
					if (_gearType == GearType::Gear06 && getGameLayer()->currentPlayer->getState() != State::DEAD)
					{
						return true;
					}
					else if (_gearType == GearType::Gear00 && _delegate->getSkillFinish())
					{
						return true;
					}
					else if (_gearType == GearType::Gear03)
					{
						return true;
					}
				}
			}
			// ougis click solution
			else if (_abType == OUGIS1)
			{
				if (_delegate->getSkillFinish() && _delegate->getOugisEnable(false) && !_isLock && !_delegate->ougisLayer)
				{
					_delegate->costCKR(15000, false);
					return true;
				}
			}
			else if (_abType == OUGIS2)
			{
				if (_delegate->getSkillFinish() && _delegate->getOugisEnable(true) && !_isLock && !_delegate->ougisLayer)
				{
					// Duel modes (Boss/Deathmatch) share one chakra pool
					// per side, so skill5 draws 3 bars (45000) from it
					// instead of the normal mode's separate 25000 CKR2
					// pool -- see GameLayer::kDuelSkill5Cost.
					_delegate->costCKR(_delegate->isDuelMode() ? GameLayer::kDuelSkill5Cost : 25000, true);
					return true;
				}
			}
			else if (_abType == AllySwitch1 || _abType == AllySwitch2)
			{
				auto gl = getGameLayer();
				auto player = gl->currentPlayer;
				State s = player->getState();
				// No longer blocked by attack/skill state or cBuff — see
				// CharacterBase::retireAndDespawnWhenIdle. The outgoing hero
				// gets handed to AI and finishes on its own instead of the
				// click being refused.
				//
				// Elimination is checked here too, not just in the visual
				// lock (HudLayer::updateAllySwitchButtons) — this is the
				// actual click gate, same reasoning as everywhere else in
				// this file that the visual lock alone isn't the real
				// enforcement point.
				int slotIndex = (_abType == AllySwitch1) ? 0 : 1;
				bool eliminated = (int)gl->_allyRoster.size() > slotIndex &&
					gl->isRosterNameEliminated(gl->_allyRoster[slotIndex], true);
				if (getTimeCount() == 0 && s != State::DEAD && !eliminated)
				{
					return true;
				}
			}
			else
			{
				if (getTimeCount() == 0 && _delegate->getSkillFinish() && !_isLock && !_delegate->ougisLayer)
				{
					return true;
				}
			}
		}

		return false;
	}
	else
	{
		return true;
	}
}

void ActionButton::beganAnimation(bool isLock)
{
	// record the click time
	cc_timeval timeVal;
	CCTime::gettimeofdayCocos2d(&timeVal, 0);
	_clickTime = timeVal.tv_sec + timeVal.tv_usec / 1000;

	if (markSprite && _abType != OUGIS1 && _abType != OUGIS2)
	{
		if (!_freezeAction || _isColdChanged)
		{
			if (_isColdChanged)
			{
				_isColdChanged = false;
			}
			createFreezeAnimation();
		}

		setTimeCount(getCD());
		if (cdLabel)
		{
			cdLabel->removeFromParent();
			unschedule(schedule_selector(ActionButton::updateCDLabel));
		}
		if (_abType != Item1)
		{
			cdLabel = CCLabelBMFont::create(to_cstr(getCD() / 1000), Fonts::Default);

#if (CC_TARGET_PLATFORM == CC_PLATFORM_LINUX || CC_TARGET_PLATFORM == CC_PLATFORM_WIN32 || CC_TARGET_PLATFORM == CC_PLATFORM_MAC)
			cdLabel->setScale(0.3f);
			cdLabel->setPosition(Vec2(getPositionX() + getContentSize().width * getScale() / 2,
									  getPositionY() + getContentSize().height * getScale() / 2));
#else
			cdLabel->setScale(0.4f);
			cdLabel->setPosition(Vec2(getPositionX() + getContentSize().width / 2,
									  getPositionY() + getContentSize().height / 2));
#endif

			_delegate->addChild(cdLabel, 200);
		}
		schedule(schedule_selector(ActionButton::updateCDLabel), 1.0f);

		markSprite->stopAllActions();
		markSprite->runAction(_freezeAction);
	}
}

// Bumps _timeCount up to at least minMs (via std::max at the call site, so
// this never shortens a longer cooldown already running) and makes sure
// it'll actually count back down to 0 on its own. Poking setTimeCount()
// directly instead of going through here is what caused the "other slot
// stays locked forever" bug: updateCDLabel is the only thing that ever
// decrements _timeCount, and it's only ever scheduled from inside
// beganAnimation() -- so a button that was never clicked (only had its
// _timeCount bumped by the other slot's switch) would sit at that value
// forever, with nothing left running to bring it back down.
void ActionButton::applyMinCooldown(uint32_t minMs)
{
	if (getTimeCount() >= minMs)
		return;

	setTimeCount(minMs);

	// No cdLabel for this one on purpose -- minMs here is well under 1000,
	// and updateCDLabel's display (getTimeCount() / 1000) would show "0"
	// for the entire brief window, which reads as broken rather than
	// informative. The real ~15s cooldown from beganAnimation() still gets
	// its label as normal; this is just a short, silent lock.
	schedule(schedule_selector(ActionButton::updateCDLabel), 1.0f);
}

void ActionButton::setGearType(GearType type)
{
	auto gearIcon = Sprite::createWithSpriteFrameName(format("gear_{:02d}.png", (int)type).c_str());
	gearIcon->setScale(0.85f);
	gearIcon->setPosition(Vec2(18, 18));
	addChild(gearIcon);
	_gearType = type;

	if (gearSign)
	{
		gearSign->removeFromParent();
		gearSign = nullptr;
	}
}

void ActionButton::updateCDLabel(float dt)
{
	if (!_delegate->ougisLayer)
	{
		if (getTimeCount() > 1000)
		{
			int tempCount = getTimeCount() - 1000;
			setTimeCount(tempCount);
			if (cdLabel)
			{
				cdLabel->setString(to_cstr(tempCount / 1000));
			}
		}
		else
		{
			unschedule(schedule_selector(ActionButton::updateCDLabel));
			setTimeCount(0);
			if (cdLabel)
			{
				cdLabel->removeFromParent();
				cdLabel = nullptr;
			}
		}
	}
}

void ActionButton::setMarkSprite(const char *mark)
{
	auto tmpSprite = Sprite::createWithSpriteFrameName(mark);
	markSprite = ProgressTimer::create(tmpSprite);
	markSprite->setType(kCCProgressTimerTypeRadial);

	markSprite->setReverseDirection(true);
	markSprite->setPosition(getPosition());
	markSprite->setAnchorPoint(Vec2(0, 0));
#if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID || CC_TARGET_PLATFORM == CC_PLATFORM_IOS)
#else
	markSprite->setScale(DESKTOP_UI_MASK_SCALE);
#endif
	_delegate->addChild(markSprite, 500);

	if (_abType == GearBtn)
	{
		if (getGameLayer()->_enableGear)
		{
			gearSign = Sprite::createWithSpriteFrameName("gearsign.png");
			gearSign->setPosition(Vec2(getPositionX() + 17, getPositionY() + 17));
			_delegate->addChild(gearSign, 500);
		}
	}
}

void ActionButton::setOugisMark()
{
	ougismarkSprite = Sprite::createWithSpriteFrameName("skill_freeze.png");
	ougismarkSprite->setPosition(getPosition());
	ougismarkSprite->setAnchorPoint(Vec2(0, 0));
	_delegate->addChild(ougismarkSprite, 500);
	if (_abType == OUGIS1)
	{
		lockLabel1 = CCLabelBMFont::create("LV2", Fonts::Default);
	}
	else
	{
		lockLabel1 = CCLabelBMFont::create("LV4", Fonts::Default);
	}

#if (CC_TARGET_PLATFORM == CC_PLATFORM_LINUX || CC_TARGET_PLATFORM == CC_PLATFORM_WIN32 || CC_TARGET_PLATFORM == CC_PLATFORM_MAC)
	ougismarkSprite->setScale(DESKTOP_UI_SCALE);

	lockLabel1->setScale(0.3f);
	lockLabel1->setPosition(Vec2(getPositionX() + getContentSize().width * getScale() / 2,
								 getPositionY() + getContentSize().height * getScale() / 2));
#else
	lockLabel1->setScale(0.4f);
	lockLabel1->setPosition(Vec2(getPositionX() + getContentSize().width / 2,
								 getPositionY() + getContentSize().height / 2));
#endif
	_delegate->addChild(lockLabel1, 200);
}

void ActionButton::setProgressMark()
{
	clipper = ClippingNode::create();
	auto stencil = Sprite::createWithSpriteFrameName("icon_bg1.png");
	stencil->setAnchorPoint(Vec2(0, 0));
	clipper->setStencil(stencil);

	proressmarkSprite = Sprite::createWithSpriteFrameName("icon_bg2.png");

	clipper->setPosition(getPosition());
	clipper->addChild(proressmarkSprite);

	proressmarkSprite->setPosition(Vec2(proressmarkSprite->getContentSize().width / 2, proressmarkSprite->getContentSize().height / 2));
	// 50,120,180

	_delegate->addChild(clipper, -50);

	proressblinkSprite = Sprite::createWithSpriteFrameName("icon_bg3.png");
	proressblinkSprite->setPosition(getPosition());
	proressblinkSprite->setPosition(Vec2(proressmarkSprite->getContentSize().width / 2, proressmarkSprite->getContentSize().height / 2));

	auto fd = FadeOut::create(0.2f);
	proressblinkSprite->runAction(RepeatForever::create(fd));
	clipper->addChild(proressblinkSprite, 50);

#if (CC_TARGET_PLATFORM == CC_PLATFORM_LINUX || CC_TARGET_PLATFORM == CC_PLATFORM_WIN32 || CC_TARGET_PLATFORM == CC_PLATFORM_MAC)
	clipper->setScale(0.8f);
#endif

	if (_abType == OUGIS1)
	{
		progressPointSprite = Sprite::createWithSpriteFrameName("icon_bg4.png");
		proressmarkSprite->setRotation(-50);
		proressblinkSprite->setRotation(-50);
		progressPointSprite->setPosition(getPosition());
	}
	else
	{
		progressPointSprite = Sprite::createWithSpriteFrameName("icon_bg5.png");
		proressmarkSprite->setRotation(-85);
		proressblinkSprite->setRotation(-85);
		progressPointSprite->setPosition(Vec2(getPositionX() + 1, getPositionY()));
	}

	progressPointSprite->setAnchorPoint(Vec2(0, 0));
#if (CC_TARGET_PLATFORM == CC_PLATFORM_LINUX || CC_TARGET_PLATFORM == CC_PLATFORM_WIN32 || CC_TARGET_PLATFORM == CC_PLATFORM_MAC)
	progressPointSprite->setScale(0.8f);
#endif
	_delegate->addChild(progressPointSprite, -25);

	auto fd2 = FadeOut::create(0.5f);
	proressblinkMask = Sprite::createWithSpriteFrameName("icon_bg6.png");
	proressblinkMask->setPosition(getPosition());
	proressblinkMask->setAnchorPoint(Vec2(0, 0));
#if (CC_TARGET_PLATFORM == CC_PLATFORM_LINUX || CC_TARGET_PLATFORM == CC_PLATFORM_WIN32 || CC_TARGET_PLATFORM == CC_PLATFORM_MAC)
	proressblinkMask->setScale(0.8f);
#endif
	proressblinkMask->runAction(RepeatForever::create(fd2));

	_delegate->addChild(proressblinkMask, 200);

	if (_abType == OUGIS2)
	{
		if (_delegate->skill4Button)
		{
			if (_delegate->skill4Button->proressblinkMask && !_delegate->skill4Button->_isLock)
			{
				_delegate->skill4Button->proressblinkMask->stopAllActions();
				auto fd3 = FadeOut::create(0.5f);
				_delegate->skill4Button->proressblinkMask->runAction(RepeatForever::create(fd3));
			}
		}
	}
}

void ActionButton::updateProgressMark()
{
	int ckr;
	int ckr2;

	if (_delegate->isDuelMode())
	{
		uint32_t pool = getGameLayer()->getDuelChakra(true); // player's own shared pool drives their own button dials

		// Skill4 (OUGIS1, 1 bar/15000): the pool is already denominated
		// in the same 15000-sized units the math below expects, so it
		// feeds straight in unchanged. Past 30000 the code below just
		// shows "fully charged", which still reads correctly here too
		// (skill4's been castable since 15000 regardless of the pool
		// going higher for skill5's sake).
		ckr = (int)pool;

		// Skill5 (OUGIS2, 3 bars/45000 in duel mode vs. 25000 normally):
		// rescale the pool's 0..45000 charging range onto the 0..25000
		// domain the math below expects, so the dial reads empty at
		// pool==0 and full/castable right at pool==45000. The old
		// 25000..50000 "overcharge" stage has no duel-mode equivalent
		// (there's no skill6 for it to be hinting at) so it's skipped --
		// once castable the dial just holds full via the same >=25000
		// branch below.
		ckr2 = (int)((std::min)(pool, GameLayer::kDuelSkill5Cost) * 25000 / GameLayer::kDuelSkill5Cost);
	}
	else
	{
		// NOTE: Using uint32_t will get wrong results.
		// uint32_t ckr = 100;
		// float result = -1    * ckr; //=> 4294967196
		// float result = -1.0f * ckr; //=> -100
		ckr = getGameLayer()->currentPlayer->getCKR();
		ckr2 = getGameLayer()->currentPlayer->getCKR2();
	}

	if (_abType == OUGIS1)
	{
		if (ckr < 15000)
		{
			if (proressblinkSprite)
				proressblinkSprite->setRotation(0);

			if (proressmarkSprite)
				proressmarkSprite->setRotation(-1.0f * ((ckr / 15000.0f) * 50));

			if (proressblinkMask)
				proressblinkMask->setVisible(false);
		}
		else if (ckr < 30000)
		{
			if (proressblinkSprite)
				proressblinkSprite->setRotation(-50);

			if (proressmarkSprite)
				proressmarkSprite->setRotation(-1.0f * (((ckr - 15000) / 15000.0f) * 70 + 50));

			if (proressblinkMask && !_isLock)
			{
				proressblinkMask->setVisible(true);
				proressblinkMask->stopAllActions();
				auto fd = FadeOut::create(0.5f);
				proressblinkMask->runAction(RepeatForever::create(fd));

				if (_delegate->skill5Button)
				{
					if (_delegate->skill5Button->proressblinkMask && !_delegate->skill5Button->_isLock)
					{
						_delegate->skill5Button->proressblinkMask->stopAllActions();
						auto fd2 = FadeOut::create(0.5f);
						_delegate->skill5Button->proressblinkMask->runAction(RepeatForever::create(fd2));
					}
				}
			}
		}
		else if (ckr < 45000)
		{
			if (proressblinkSprite)
				proressblinkSprite->setRotation(-120);

			if (proressmarkSprite)
				proressmarkSprite->setRotation(-1.0f * (((ckr - 30000) / 15000.0f) * 60 + 120));
		}
		else
		{
			if (proressblinkSprite)
				proressmarkSprite->setRotation(-180);

			if (proressblinkSprite)
				proressblinkSprite->setRotation(-180);
		}
	}
	else
	{
		if (ckr2 < 25000)
		{
			if (proressblinkSprite)
				proressblinkSprite->setRotation(0);

			if (proressmarkSprite)
				proressmarkSprite->setRotation(-1.0f * ((ckr2 / 25000.0f) * 85));

			if (proressblinkMask)
				proressblinkMask->setVisible(false);
		}
		else if (ckr2 < 50000)
		{
			if (proressblinkSprite)
				proressblinkSprite->setRotation(-85);

			if (proressmarkSprite)
				proressmarkSprite->setRotation(-1.0f * (((ckr2 - 25000) / 25000.0f) * 90 + 90));

			if (proressblinkMask && !_isLock)
			{
				proressblinkMask->setVisible(true);
				proressblinkMask->stopAllActions();
				auto fd = FadeOut::create(0.5f);
				proressblinkMask->runAction(RepeatForever::create(fd));

				if (_delegate->skill4Button)
				{
					if (_delegate->skill4Button->proressblinkMask && !_delegate->skill4Button->_isLock)
					{
						_delegate->skill4Button->proressblinkMask->stopAllActions();
						auto fd2 = FadeOut::create(0.5f);
						_delegate->skill4Button->proressblinkMask->runAction(RepeatForever::create(fd2));
					}
				}
			}
		}
		else
		{
			if (proressmarkSprite)
				proressmarkSprite->setRotation(-180);

			if (proressblinkSprite)
				proressblinkSprite->setRotation(-180);
		}
	}
}

void ActionButton::reset()
{
	clearClick();

	if (!_isLock && markSprite)
	{
		markSprite->stopAllActions();
		markSprite->setPercentage(0);
	}

	if (!_delegate->ougisLayer)
	{
		unschedule(schedule_selector(ActionButton::updateCDLabel));
		setTimeCount(0);
		if (cdLabel)
		{
			cdLabel->removeFromParent();
			cdLabel = nullptr;
		}
	}
}

void ActionButton::setLock(bool showX)
{
	if (markSprite)
	{
		markSprite->stopAllActions();
		markSprite->setPercentage(100);
	}
	else
	{
		setMarkSprite("skill_freeze.png");
	}

	if (proressblinkMask)
	{
		proressblinkMask->setVisible(false);
	}
	_isLock = true;

	if (showX)
	{
		if (cdLabel)
		{
			cdLabel->removeFromParent();
			unschedule(schedule_selector(ActionButton::updateCDLabel));
		}
		cdLabel = CCLabelBMFont::create("X", Fonts::Default);
#if (CC_TARGET_PLATFORM == CC_PLATFORM_LINUX || CC_TARGET_PLATFORM == CC_PLATFORM_WIN32 || CC_TARGET_PLATFORM == CC_PLATFORM_MAC)
		cdLabel->setScale(0.3f);
#else
		cdLabel->setScale(0.4f);
#endif
		cdLabel->setPosition(Vec2(getPositionX() + getContentSize().width * getScale() / 2,
			getPositionY() + getContentSize().height * getScale() / 2));
		_delegate->addChild(cdLabel, 200);
	}
}

// AFTER
void ActionButton::unLock()
{
	if (markSprite)
	{
		markSprite->stopAllActions();
		markSprite->setPercentage(0);
	}

	_isLock = false;

	if (cdLabel && getTimeCount() == 0)
	{
		cdLabel->removeFromParent();
		cdLabel = nullptr;
	}

	uint32_t ckr = getGameLayer()->currentPlayer->getCKR();
	uint32_t ckr2 = getGameLayer()->currentPlayer->getCKR2();

	if (_abType == OUGIS1)
	{
		if (ckr >= 15000)
		{
			if (proressblinkMask)
			{
				proressblinkMask->setVisible(true);
			}
		}
	}
	else if (_abType == OUGIS2)
	{
		if (ckr2 >= 25000)
		{
			if (proressblinkMask)
			{
				proressblinkMask->setVisible(false);
			}
		}
	}

	_isLock = false;
}

void ActionButton::createFreezeAnimation()
{
	auto to = ProgressTo::create(0, 99.999f);

	int delay = getCD() / 1000;
	auto to1 = ProgressTo::create(delay, 0);

	Action *freezeAction;
	if (_isDoubleSkill)
	{
		auto callback = CallFunc::create(std::bind(&ActionButton::clearClick, this));
		freezeAction = newSequence(to, to1, callback);
	}
	else
	{
		freezeAction = newSequence(to, to1);
	}

	setFreezeAction(freezeAction);
}

void ActionButton::clearClick()
{
	_clickNum = 0;
}

void ActionButton::clearOugisMark()
{
	if (ougismarkSprite)
	{
		ougismarkSprite->removeAllChildrenWithCleanup(true);
	}

	if (clipper)
	{
		clipper->removeFromParent();
	}
	if (progressPointSprite)
	{
		progressPointSprite->removeFromParent();
	}
	if (proressblinkMask)
	{
		proressblinkMask->removeFromParent();
	}
}

ActionButton *ActionButton::create(const string &szImage)
{
	ActionButton *ab = new ActionButton();
	if (ab && ab->init(szImage))
	{
		ab->autorelease();
		return ab;
	}
	else
	{
		delete ab;
		return nullptr;
	}
}

#pragma once
#include "Hero.hpp"
#include "Shinobi/Bunshin/NarutoSRClone.hpp"

class NarutoSR : public Hero
{
	void setID(const string& name, Role role, Group group) override
	{
		Hero::setID(name, role, group);
		setAIHandler(NarutoSR::perform);
	}

	// Override dead() to guarantee cBuff removal when dying
	void dead() override
	{
		if (_isCBuffActive)
		{
			// Revert base stats, actions, and HUD elements
			resumeAction(0.0f);
		}
		Hero::dead();
	}

	void changeAction() override
	{
		if (_skillChangeBuffValue != 17)
			return;

		if (_isCBuffActive)
			return;

		_isCBuffActive = true;

		// --- Cache base-form actions & stats ---
		_baseIdleAction = getIdleAction();
		_baseWalkAction = getWalkAction();
		_baseNAttackAction = getNAttackAction();
		_baseSkill1Action = getSkill1Action();
		_baseSkill2Action = getSkill2Action();
		_baseSkill3Action = getSkill3Action();

		_baseNAttackType = _nAttackType;
		_baseNAttackRangeX = _nAttackRangeX;
		_baseNAttackRangeY = _nAttackRangeY;
		setTempAttackValue1(getNAttackValue());

		_baseSAttackType1 = getSAttackType1();
		_baseSAttackValue1 = getSAttackValue1();
		_baseSAttackRangeX1 = _sAttackRangeX1;
		_baseSAttackRangeY1 = _sAttackRangeY1;

		_baseSAttackType2 = getSAttackType2();
		_baseSAttackValue2 = getSAttackValue2();
		_baseSAttackRangeX2 = _sAttackRangeX2;
		_baseSAttackRangeY2 = _sAttackRangeY2;

		_baseSAttackType3 = getSAttackType3();
		_baseSAttackValue3 = getSAttackValue3();
		_baseSAttackRangeX3 = _sAttackRangeX3;
		_baseSAttackRangeY3 = _sAttackRangeY3;

		// Idle / Walk -> skill10 / skill11 (SPC1 / SPC2)
		setIdleAction(createAnimation(skillSPC1Array, 5, true, false));
		setWalkAction(createAnimation(skillSPC2Array, 10, true, false));

		// nAttack -> aAttack (skill12 / SPC3)
		setNAttackAction(createAnimation(skillSPC3Array, 10, false, true));
		setNAttackValue(getSpcAttackValue3());
		_nAttackType = getSpcAttack3Type();
		_nAttackRangeX = _spcAttackRangeX3;
		_nAttackRangeY = _spcAttackRangeY3;

		// Skill1 -> skill13 (SPC6)
		setSkill1Action(createAnimation(skillSPC6Array, 10, false, true));
		setSAttackType1(getSpcAttack6Type());
		setSAttackValue1(getSpcAttackValue6());
		_sAttackRangeX1 = _spcAttackRangeX6;
		_sAttackRangeY1 = _spcAttackRangeY6;

		// Skill2 -> skill14 (SPC7)
		setSkill2Action(createAnimation(skillSPC7Array, 10, false, true));
		setSAttackType2(getSpcAttack7Type());
		setSAttackValue2(getSpcAttackValue7());
		_sAttackRangeX2 = _spcAttackRangeX7;
		_sAttackRangeY2 = _spcAttackRangeY7;

		// Skill3 -> skill15 (SPC8)
		setSkill3Action(createAnimation(skillSPC8Array, 10, false, true));
		setSAttackType3(getSpcAttack8Type());
		setSAttackValue3(getSpcAttackValue8());
		_sAttackRangeX3 = _spcAttackRangeX8;
		_sAttackRangeY3 = _spcAttackRangeY8;

		// Update HUD Icons
		if (isPlayer())
		{
			auto frame1 = getSpriteFrame("NarutoSR_skill1_1.png");
			auto frame2 = getSpriteFrame("NarutoSR_skill2_1.png");
			auto frame3 = getSpriteFrame("NarutoSR_skill3_1.png");

			if (frame1)
				getGameLayer()->getHudLayer()->skill1Button->setDisplayFrame(frame1);
			if (frame2)
				getGameLayer()->getHudLayer()->skill2Button->setDisplayFrame(frame2);
			if (frame3)
				getGameLayer()->getHudLayer()->skill3Button->setDisplayFrame(frame3);
		}

		lockOugisButtons();

		_defense += 400;
		_isArmored = true;
	}

	void resumeAction(float dt) override
	{
		if (_isCBuffActive)
		{
			_isCBuffActive = false;

			setIdleAction(_baseIdleAction);
			setWalkAction(_baseWalkAction);
			setNAttackAction(_baseNAttackAction);
			setSkill1Action(_baseSkill1Action);
			setSkill2Action(_baseSkill2Action);
			setSkill3Action(_baseSkill3Action);

			if (hasTempAttackValue1())
			{
				setNAttackValue(getTempAttackValue1());
				setTempAttackValue1(0);
			}
			_nAttackType = _baseNAttackType;
			_nAttackRangeX = _baseNAttackRangeX;
			_nAttackRangeY = _baseNAttackRangeY;

			setSAttackType1(_baseSAttackType1);
			setSAttackValue1(_baseSAttackValue1);
			_sAttackRangeX1 = _baseSAttackRangeX1;
			_sAttackRangeY1 = _baseSAttackRangeY1;

			setSAttackType2(_baseSAttackType2);
			setSAttackValue2(_baseSAttackValue2);
			_sAttackRangeX2 = _baseSAttackRangeX2;
			_sAttackRangeY2 = _baseSAttackRangeY2;

			setSAttackType3(_baseSAttackType3);
			setSAttackValue3(_baseSAttackValue3);
			_sAttackRangeX3 = _baseSAttackRangeX3;
			_sAttackRangeY3 = _baseSAttackRangeY3;

			if (isPlayer())
			{
				auto frame1 = getSpriteFrame("NarutoSR_skill1.png");
				auto frame2 = getSpriteFrame("NarutoSR_skill2.png");
				auto frame3 = getSpriteFrame("NarutoSR_skill3.png");

				if (frame1)
					getGameLayer()->getHudLayer()->skill1Button->setDisplayFrame(frame1);
				if (frame2)
					getGameLayer()->getHudLayer()->skill2Button->setDisplayFrame(frame2);
				if (frame3)
					getGameLayer()->getHudLayer()->skill3Button->setDisplayFrame(frame3);
			}

			unlockOugisButtons();

			resetDefenseValue(400);
			_isArmored = false;

			if (_state != State::DEAD)
			{
				_state = State::WALK;
				setKnockDownAction(createAnimation(skillSPC4Array, 10, false, true));
				knockDown();
				setKnockDownAction(createAnimation(knockDownArray, 10, false, true));
			}
		}

		CharacterBase::resumeAction(dt);
	}

	Hero* createClone(int cloneTime) override
	{
		auto clone = createCloneHero<NarutoSRClone>(getName());
		return clone;
	}

	void perform() override
	{
		_mainTarget = nullptr;
		findHeroHalf();

		tryBuyGear(GearType::Gear03, GearType::Gear07, GearType::Gear02);

		if (needBackToTowerToRestoreHP() || needBackToDefendTower())
			return;

		if (_mainTarget && _mainTarget->isNotFlog())
		{
			Vec2 moveDirection;
			Vec2 sp = getDistanceToTarget();

			if (isFreeState())
			{
				if (_isCanOugis2 && !_isControlled && getGameLayer()->_isOugis2Game && !_isArmored)
				{
					changeSide(sp);
					attack(OUGIS2);
					return;
				}
				else if (_isCanSkill2 && !_isArmored)
				{
					changeSide(sp);
					attack(SKILL2);
					return;
				}
				else if (enemyCombatPoint > friendCombatPoint && abs(enemyCombatPoint - friendCombatPoint) > 3000 && !_isHealing && !_isArmored && !_isControlled)
				{
					if (abs(sp.x) < 160)
						stepBack2();
					else
						idle();
					return;
				}
				else if (abs(sp.x) < 128)
				{
					if (abs(sp.x) > 96 || abs(sp.y) > 32)
					{
						moveDirection = sp.getNormalized();
						walk(moveDirection);
						return;
					}
					else if (abs(sp.x) < 32 && (_isCanSkill1 || _isCanOugis1 || _isCanSkill3))
					{
						stepBack();
						return;
					}

					if (_isCanOugis1 && !_isControlled && _mainTarget->getDEF() < 5000 && !_isArmored)
					{
						changeSide(sp);
						attack(OUGIS1);
					}
					else if (_isCanGear03)
					{
						useGear(GearType::Gear03);
					}
					else if (_isCanSkill3 && !_isArmored && _mainTarget->getDEF() < 5000)
					{
						changeSide(sp);
						attack(SKILL3);
					}
					else if (_isCanSkill1 && !_isArmored && _mainTarget->getDEF() < 5000)
					{
						changeSide(sp);
						attack(SKILL1);
					}
					else
					{
						changeSide(sp);
						attack(NAttack);
					}

					return;
				}
			}
		}

		_mainTarget = nullptr;
		if (notFindFlogHalf())
			findTowerHalf();

		if (_mainTarget)
		{
			Vec2 moveDirection;
			Vec2 sp = getDistanceToTarget();

			if (abs(sp.x) > 32 || abs(sp.y) > 32)
			{
				moveDirection = sp.getNormalized();
				walk(moveDirection);
				return;
			}

			if (isFreeState())
			{
				if (_isCanSkill2 && !_isArmored)
				{
					changeSide(sp);
					attack(SKILL2);
					return;
				}
				else
				{
					changeSide(sp);
					attack(NAttack);
				}
			}

			return;
		}

		checkHealingState();
	}

private:
	bool _isCBuffActive = false;

	RefPtr<FiniteTimeAction> _baseIdleAction;
	RefPtr<FiniteTimeAction> _baseWalkAction;
	RefPtr<FiniteTimeAction> _baseNAttackAction;
	RefPtr<FiniteTimeAction> _baseSkill1Action;
	RefPtr<FiniteTimeAction> _baseSkill2Action;
	RefPtr<FiniteTimeAction> _baseSkill3Action;

	string _baseNAttackType;
	int    _baseNAttackRangeX = 0;
	int    _baseNAttackRangeY = 0;

	string   _baseSAttackType1;
	uint32_t _baseSAttackValue1 = 0;
	int      _baseSAttackRangeX1 = 0;
	int      _baseSAttackRangeY1 = 0;
	float    _baseSAttackCD1 = 0.0f;

	string   _baseSAttackType2;
	uint32_t _baseSAttackValue2 = 0;
	int      _baseSAttackRangeX2 = 0;
	int      _baseSAttackRangeY2 = 0;
	float    _baseSAttackCD2 = 0.0f;

	string   _baseSAttackType3;
	uint32_t _baseSAttackValue3 = 0;
	int      _baseSAttackRangeX3 = 0;
	int      _baseSAttackRangeY3 = 0;
	float    _baseSAttackCD3 = 0.0f;
};
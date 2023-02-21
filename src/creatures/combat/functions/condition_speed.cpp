/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019-2022 OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.org/
 */

#include "creatures/combat/functions/condition_speed.hpp"
#include "creatures/combat/condition.h"
#include "creatures/creature.h"
#include "creatures/players/player.h"
#include "game/game.h"

void ConditionSpeed::setFormulaVars(float NewMina, float NewMinb, float NewMaxa, float NewMaxb) {
	this->mina = NewMina;
	this->minb = NewMinb;
	this->maxa = NewMaxa;
	this->maxb = NewMaxb;
}

void ConditionSpeed::getFormulaValues(int32_t var, int32_t &min, int32_t &max) const {
	int32_t difference = var - 40;
	min = mina * difference + minb;
	max = maxa * difference + maxb;
}

bool ConditionSpeed::setParam(ConditionParam_t param, int32_t value) {
	Condition::setParam(param, value);
	if (param != CONDITION_PARAM_SPEED) {
		return false;
	}

	speedDelta = value;

	if (value > 0) {
		conditionType = CONDITION_HASTE;
	} else {
		conditionType = CONDITION_PARALYZE;
	}
	return true;
}

bool ConditionSpeed::unserializeProp(ConditionAttr_t attr, PropStream &propStream) {
	if (attr == CONDITIONATTR_SPEEDDELTA) {
		return propStream.read<int32_t>(speedDelta);
	} else if (attr == CONDITIONATTR_FORMULA_MINA) {
		return propStream.read<float>(mina);
	} else if (attr == CONDITIONATTR_FORMULA_MINB) {
		return propStream.read<float>(minb);
	} else if (attr == CONDITIONATTR_FORMULA_MAXA) {
		return propStream.read<float>(maxa);
	} else if (attr == CONDITIONATTR_FORMULA_MAXB) {
		return propStream.read<float>(maxb);
	}
	return Condition::unserializeProp(attr, propStream);
}

void ConditionSpeed::serialize(PropWriteStream &propWriteStream) {
	Condition::serialize(propWriteStream);

	propWriteStream.write<uint8_t>(CONDITIONATTR_SPEEDDELTA);
	propWriteStream.write<int32_t>(speedDelta);

	propWriteStream.write<uint8_t>(CONDITIONATTR_FORMULA_MINA);
	propWriteStream.write<float>(mina);

	propWriteStream.write<uint8_t>(CONDITIONATTR_FORMULA_MINB);
	propWriteStream.write<float>(minb);

	propWriteStream.write<uint8_t>(CONDITIONATTR_FORMULA_MAXA);
	propWriteStream.write<float>(maxa);

	propWriteStream.write<uint8_t>(CONDITIONATTR_FORMULA_MAXB);
	propWriteStream.write<float>(maxb);
}

bool ConditionSpeed::startCondition(Creature* creature) {
	if (!Condition::startCondition(creature)) {
		return false;
	}

	if (speedDelta == 0) {
		int32_t min;
		int32_t max;
		auto baseSpeed = creature->getBaseSpeed();
		getFormulaValues(baseSpeed, min, max);
		speedDelta = uniform_random(min, max);

		if (conditionType == CONDITION_HASTE) {
			speedDelta = speedDelta - baseSpeed;
		}

		if (speedDelta < 40 - baseSpeed) {
			speedDelta = 40 - baseSpeed;
		}
	}

	g_game().changeSpeed(creature, speedDelta);
	return true;
}

bool ConditionSpeed::executeCondition(Creature* creature, int32_t interval) {
	return Condition::executeCondition(creature, interval);
}

void ConditionSpeed::endCondition(Creature* creature) {
	g_game().changeSpeed(creature, -speedDelta);
}

void ConditionSpeed::addCondition(Creature* creature, const Condition* addCondition) {
	if (conditionType != addCondition->getType()) {
		return;
	}

	if (ticks == -1 && addCondition->getTicks() > 0) {
		return;
	}

	setTicks(addCondition->getTicks());

	const ConditionSpeed &conditionSpeed = static_cast<const ConditionSpeed &>(*addCondition);
	int32_t oldSpeedDelta = speedDelta;
	speedDelta = conditionSpeed.speedDelta;
	mina = conditionSpeed.mina;
	maxa = conditionSpeed.maxa;
	minb = conditionSpeed.minb;
	maxb = conditionSpeed.maxb;

	if (speedDelta == 0) {
		int32_t min;
		int32_t max;
		auto baseSpeed = creature->getBaseSpeed();
		getFormulaValues(baseSpeed, min, max);
		speedDelta = uniform_random(min, max);

		if (conditionType == CONDITION_HASTE) {
			speedDelta = speedDelta - baseSpeed;
		}

		if (speedDelta < baseSpeed - 40) {
			speedDelta = 40 - baseSpeed;
		}
	}

	int32_t newSpeedChange = (speedDelta - oldSpeedDelta);
	if (newSpeedChange != 0) {
		g_game().changeSpeed(creature, newSpeedChange);
	}
}

uint32_t ConditionSpeed::getIcons() const {
	uint32_t icons = Condition::getIcons();
	switch (conditionType) {
		case CONDITION_HASTE:
			icons |= ICON_HASTE;
			break;

		case CONDITION_PARALYZE:
			icons |= ICON_PARALYZE;
			break;

		default:
			break;
	}
	return icons;
}

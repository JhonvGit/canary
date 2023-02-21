/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019-2022 OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.org/
 */

#include "pch.hpp"
#include "creatures/combat/condition.h"

class ConditionSpeed final : public Condition {
	public:
		ConditionSpeed(ConditionId_t initId, ConditionType_t initType, int32_t initTicks, bool initBuff, uint32_t initSubId, int32_t initChangeSpeed) :
			Condition(initId, initType, initTicks, initBuff, initSubId), speedDelta(initChangeSpeed) { }

		bool startCondition(Creature* creature) override;
		bool executeCondition(Creature* creature, int32_t interval) override;
		void endCondition(Creature* creature) override;
		void addCondition(Creature* creature, const Condition* condition) override;
		uint32_t getIcons() const override;

		ConditionSpeed* clone() const override {
			return new ConditionSpeed(*this);
		}

		bool setParam(ConditionParam_t param, int32_t value) override;

		void setFormulaVars(float mina, float minb, float maxa, float maxb);

		// serialization
		void serialize(PropWriteStream &propWriteStream) override;
		bool unserializeProp(ConditionAttr_t attr, PropStream &propStream) override;

	private:
		void getFormulaValues(int32_t var, int32_t &min, int32_t &max) const;

		int32_t speedDelta;

		// formula variables
		float mina = 0.0f;
		float minb = 0.0f;
		float maxa = 0.0f;
		float maxb = 0.0f;
};
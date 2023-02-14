/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019-2022 OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.org/
 */

#include "pch.hpp"

#include "items/bed.h"
#include "game/game.h"
#include "io/iologindata.h"
#include "game/scheduling/scheduler.h"

BedItem::BedItem(uint16_t id) :
	Item(id) {
	internalRemoveSleeper();
}

Attr_ReadValue BedItem::readAttr(AttrTypes_t attr, PropStream& propStream) {
	switch (attr) {
		case ATTR_SLEEPERGUID: {
			uint32_t guid;
			if (!propStream.read<uint32_t>(guid)) {
				return ATTR_READ_ERROR;
			}

			if (guid != 0) {
				std::string name = IOLoginData::getNameByGuid(guid);
				if (!name.empty()) {
					setAttribute(ItemAttribute_t::DESCRIPTION, name + " is sleeping there.");
					g_game().setBedSleeper(this, guid);
					sleeperGUID = guid;
				}
			}
			return ATTR_READ_CONTINUE;
		}

		case ATTR_SLEEPSTART: {
			uint32_t sleep_start;
			if (!propStream.read<uint32_t>(sleep_start)) {
				return ATTR_READ_ERROR;
			}

			sleepStart = static_cast<uint64_t>(sleep_start);
			return ATTR_READ_CONTINUE;
		}

		default:
			break;
	}
	return Item::readAttr(attr, propStream);
}

void BedItem::serializeAttr(PropWriteStream& propWriteStream) const {
	if (sleeperGUID != 0) {
		propWriteStream.write<uint8_t>(ATTR_SLEEPERGUID);
		propWriteStream.write<uint32_t>(sleeperGUID);
	}

	if (sleepStart != 0) {
		propWriteStream.write<uint8_t>(ATTR_SLEEPSTART);
		// FIXME: should be stored as 64-bit, but we need to retain backwards compatibility
		propWriteStream.write<uint32_t>(static_cast<uint32_t>(sleepStart));
	}
}

BedItem* BedItem::getNextBedItem() const {
	Direction dir = Item::items[id].bedPartnerDir;
	Position targetPos = getNextPosition(dir, getPosition());

	const Tile* tile = g_game().map.getTile(targetPos);
	if (tile == nullptr) {
		return nullptr;
	}
	return tile->getBedItem();
}

bool BedItem::canUse(Player* player) {
	if ((player == nullptr) || (house == nullptr) || !player->isPremium()) {
		return false;
	}

	if (sleeperGUID == 0) {
		return true;
	}

	if (house->getHouseAccessLevel(player) == HOUSE_OWNER) {
		return true;
	}

	Player sleeper(nullptr);
	if (!IOLoginData::loadPlayerById(&sleeper, sleeperGUID)) {
		return false;
	}

	if (house->getHouseAccessLevel(&sleeper) > house->getHouseAccessLevel(player)) {
		return false;
	}
	return true;
}

bool BedItem::trySleep(Player* player) {
	if (!house || player->isRemoved()) {
		return false;
	}

	if (sleeperGUID != 0) {
		if (Item::items[id].transformToFree != 0 && house->getOwner() == player->getGUID()) {
			wakeUp(nullptr);
		}

		g_game().addMagicEffect(player->getPosition(), CONST_ME_POFF);
		return false;
	}
	return true;
}

bool BedItem::sleep(Player* player) {
	if (house == nullptr) {
		return false;
	}

	if (sleeperGUID != 0) {
		return false;
	}

	BedItem* nextBedItem = getNextBedItem();

	internalSetSleeper(player);

	if (nextBedItem != nullptr) {
		nextBedItem->internalSetSleeper(player);
	}

	// update the bedSleepersMap
	g_game().setBedSleeper(this, player->getGUID());

	// make the player walk onto the bed
	g_game().map.moveCreature(*player, *getTile());

	// display 'Zzzz'/sleep effect
	g_game().addMagicEffect(player->getPosition(), CONST_ME_SLEEP);

	// logout player after he sees himself walk onto the bed and it change id
	g_scheduler().addEvent(createSchedulerTask(SCHEDULER_MINTICKS, std::bind(&ProtocolGame::logout, player->client, false, false)));

	// change self and partner's appearance
	updateAppearance(player);

	if (nextBedItem != nullptr) {
		nextBedItem->updateAppearance(player);
	}

	return true;
}

void BedItem::wakeUp(Player* player) {
	if (house == nullptr) {
		return;
	}

	if (sleeperGUID != 0) {
		if (player == nullptr) {
			Player regenPlayer(nullptr);
			if (IOLoginData::loadPlayerById(&regenPlayer, sleeperGUID)) {
				regeneratePlayer(&regenPlayer);
				IOLoginData::savePlayer(&regenPlayer);
			}
		} else {
			regeneratePlayer(player);
			g_game().addCreatureHealth(player);
		}
	}

	// update the bedSleepersMap
	g_game().removeBedSleeper(sleeperGUID);

	BedItem* nextBedItem = getNextBedItem();

	// unset sleep info
	internalRemoveSleeper();

	if (nextBedItem != nullptr) {
		nextBedItem->internalRemoveSleeper();
	}

	// change self and partner's appearance
	updateAppearance(nullptr);

	if (nextBedItem != nullptr) {
		nextBedItem->updateAppearance(nullptr);
	}
}

void BedItem::regeneratePlayer(Player* player) const {
	const uint32_t sleptTime = time(nullptr) - sleepStart;

	Condition* condition = player->getCondition(CONDITION_REGENERATION, CONDITIONID_DEFAULT);
	if (condition != nullptr) {
		uint32_t regen;
		if (condition->getTicks() != -1) {
			regen = std::min<int32_t>((condition->getTicks() / 1000), sleptTime) / 30; // RATE_HEALTH_REGEN_SPEED and RATE_MANA_REGEN_SPEED?
			const int32_t newRegenTicks = condition->getTicks() - (regen * 30000);
			if (newRegenTicks <= 0) {
				player->removeCondition(condition);
			} else {
				condition->setTicks(newRegenTicks);
			}
		} else {
			regen = sleptTime / 30;
		}

		player->changeHealth(regen * g_configManager().getFloat(RATE_HEALTH_REGEN), false);
		player->changeMana(regen * g_configManager().getFloat(RATE_MANA_REGEN));
	}

	const int32_t soulRegen = sleptTime / (60 * 15); // RATE_SOUL_REGEN_SPEED?
	player->changeSoul(soulRegen);
}

void BedItem::updateAppearance(const Player* player) {
	const ItemType& it = Item::items[id];
	if (it.type == ITEM_TYPE_BED) {
		if ((player != nullptr) && it.transformToOnUse[player->getSex()] != 0) {
			const ItemType& newType = Item::items[it.transformToOnUse[player->getSex()]];
			if (newType.type == ITEM_TYPE_BED) {
				g_game().transformItem(this, it.transformToOnUse[player->getSex()]);
			}
		} else if (it.transformToFree != 0) {
			const ItemType& newType = Item::items[it.transformToFree];
			if (newType.type == ITEM_TYPE_BED) {
				g_game().transformItem(this, it.transformToFree);
			}
		}
	}
}

void BedItem::internalSetSleeper(const Player* player) {
	std::string desc_str = player->getName() + " is sleeping there.";

	sleeperGUID = player->getGUID();
	sleepStart = time(nullptr);
	setAttribute(ItemAttribute_t::DESCRIPTION, desc_str);
}

void BedItem::internalRemoveSleeper() {
	sleeperGUID = 0;
	sleepStart = 0;
	setAttribute(ItemAttribute_t::DESCRIPTION, "Nobody is sleeping there.");
}

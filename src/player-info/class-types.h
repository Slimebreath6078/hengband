﻿#pragma once

enum class PlayerClassType : short {
	WARRIOR = 0,
    MAGE = 1,
    PRIEST = 2,
    ROGUE = 3,
    RANGER = 4,
    PALADIN = 5,
    WARRIOR_MAGE = 6,
    CHAOS_WARRIOR = 7,
    MONK = 8,
    MINDCRAFTER = 9,
    HIGH_MAGE = 10,
    TOURIST = 11,
    IMITATOR = 12,
    BEASTMASTER = 13,
    SORCERER = 14,
    ARCHER = 15,
    MAGIC_EATER = 16,
    BARD = 17,
    RED_MAGE = 18,
    SAMURAI = 19,
    FORCETRAINER = 20,
    BLUE_MAGE = 21,
    CAVALRY = 22,
    BERSERKER = 23,
    SMITH = 24,
    MIRROR_MASTER = 25,
    NINJA = 26,
    SNIPER = 27,
    ELEMENTALIST = 28,
    MAX,
};

constexpr auto PLAYER_CLASS_TYPE_MAX = static_cast<short>(PlayerClassType::MAX);

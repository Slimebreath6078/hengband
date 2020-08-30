﻿#include "info-reader/kind-info-tokens-table.h"

/*!
 * オブジェクト基本特性トークンの定義 /
 * Object flags
 */
concptr k_info_flags[NUM_K_FLAGS] = {
    "STR",
    "INT",
    "WIS",
    "DEX",
    "CON",
    "CHR",
    "MAGIC_MASTERY",
    "FORCE_WEAPON",
    "STEALTH",
    "SEARCH",
    "INFRA",
    "TUNNEL",
    "SPEED",
    "BLOWS",
    "CHAOTIC",
    "VAMPIRIC",
    "SLAY_ANIMAL",
    "SLAY_EVIL",
    "SLAY_UNDEAD",
    "SLAY_DEMON",
    "SLAY_ORC",
    "SLAY_TROLL",
    "SLAY_GIANT",
    "SLAY_DRAGON",
    "KILL_DRAGON",
    "VORPAL",
    "IMPACT",
    "BRAND_POIS",
    "BRAND_ACID",
    "BRAND_ELEC",
    "BRAND_FIRE",
    "BRAND_COLD",

    "SUST_STR",
    "SUST_INT",
    "SUST_WIS",
    "SUST_DEX",
    "SUST_CON",
    "SUST_CHR",
    "RIDING",
    "EASY_SPELL",
    "IM_ACID",
    "IM_ELEC",
    "IM_FIRE",
    "IM_COLD",
    "THROW",
    "REFLECT",
    "FREE_ACT",
    "HOLD_EXP",
    "RES_ACID",
    "RES_ELEC",
    "RES_FIRE",
    "RES_COLD",
    "RES_POIS",
    "RES_FEAR",
    "RES_LITE",
    "RES_DARK",
    "RES_BLIND",
    "RES_CONF",
    "RES_SOUND",
    "RES_SHARDS",
    "RES_NETHER",
    "RES_NEXUS",
    "RES_CHAOS",
    "RES_DISEN",

    "SH_FIRE",
    "SH_ELEC",
    "SLAY_HUMAN",
    "SH_COLD",
    "NO_TELE",
    "NO_MAGIC",
    "DEC_MANA",
    "TY_CURSE",
    "WARNING",
    "HIDE_TYPE",
    "SHOW_MODS",
    "SLAY_GOOD",
    "LEVITATION",
    "LITE",
    "SEE_INVIS",
    "TELEPATHY",
    "SLOW_DIGEST",
    "REGEN",
    "XTRA_MIGHT",
    "XTRA_SHOTS",
    "IGNORE_ACID",
    "IGNORE_ELEC",
    "IGNORE_FIRE",
    "IGNORE_COLD",
    "ACTIVATE",
    "DRAIN_EXP",
    "TELEPORT",
    "AGGRAVATE",
    "BLESSED",
    "XXX3", /* Fake flag for Smith */
    "XXX4", /* Fake flag for Smith */
    "KILL_GOOD",

    "KILL_ANIMAL",
    "KILL_EVIL",
    "KILL_UNDEAD",
    "KILL_DEMON",
    "KILL_ORC",
    "KILL_TROLL",
    "KILL_GIANT",
    "KILL_HUMAN",
    "ESP_ANIMAL",
    "ESP_UNDEAD",
    "ESP_DEMON",
    "ESP_ORC",
    "ESP_TROLL",
    "ESP_GIANT",
    "ESP_DRAGON",
    "ESP_HUMAN",
    "ESP_EVIL",
    "ESP_GOOD",
    "ESP_NONLIVING",
    "ESP_UNIQUE",
    "FULL_NAME",
    "FIXED_FLAVOR",
    "ADD_L_CURSE",
    "ADD_H_CURSE",
    "DRAIN_HP",
    "DRAIN_MANA",

    "LITE_2",
    "LITE_3",
    "LITE_M1",
    "LITE_M2",
    "LITE_M3",
    "LITE_FUEL",

    "CALL_ANIMAL",
    "CALL_DEMON",
    "CALL_DRAGON",
    "CALL_UNDEAD",
    "COWARDICE",
    "LOW_MELEE",
    "LOW_AC",
    "LOW_MAGIC",
    "FAST_DIGEST",
    "SLOW_REGEN",
    "MIGHTY_THROW",
    "EASY2_WEAPON",
    "DOWN_SAVING",
    "NO_AC",
    "HEAVY_SPELL",
    "RES_TIME",
 };

/*!
 * オブジェクト生成特性トークンの定義 /
 * Object flags
 */
concptr k_info_gen_flags[NUM_K_GENERATION_FLAGS] = {
    "INSTA_ART",
    "QUESTITEM",
    "XTRA_POWER",
    "ONE_SUSTAIN",
    "XTRA_RES_OR_POWER",
    "XTRA_H_RES",
    "XTRA_E_RES",
    "XTRA_L_RES",
    "XTRA_D_RES",
    "XTRA_RES",
    "CURSED",
    "HEAVY_CURSE",
    "PERMA_CURSE",
    "RANDOM_CURSE0",
    "RANDOM_CURSE1",
    "RANDOM_CURSE2",
    "XTRA_DICE",
    "POWERFUL",
    "XXX",
    "XXX",
    "XXX",
    "XXX",
    "XXX",
    "XXX",
    "XXX",
    "XXX",
    "XXX",
    "XXX",
    "XXX",
    "XXX",
    "XXX",
    "XXX",
};

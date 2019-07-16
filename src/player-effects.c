﻿/*!
 * @file effects.c
 * @brief プレイヤーのステータス管理 / effects of various "objects"
 * @date 2014/01/01
 * @author
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke\n
 *\n
 * This software may be copied and distributed for educational, research,\n
 * and not for profit purposes provided that this copyright and statement\n
 * are included in all such copies.  Other copyrights may also apply.\n
 *\n
 * 2013 Deskull rearranged comment for Doxygen.\n
 */

#include "angband.h"
#include "term.h"
#include "core.h"
#include "util.h"

#include "creature.h"

#include "artifact.h"
#include "cmd-dump.h"
#include "floor.h"
#include "bldg.h"
#include "birth.h"
#include "grid.h"
#include "mutation.h"
#include "quest.h"
#include "avatar.h"
#include "spells-status.h"
#include "realm-song.h"
#include "realm-hex.h"
#include "object-ego.h"
#include "object-hook.h"
#include "wild.h"
#include "spells-floor.h"
#include "player-status.h"
#include "player-class.h"
#include "player-move.h"
#include "player-effects.h"
#include "player-race.h"
#include "player-class.h"
#include "player-personality.h"
#include "player-sex.h"
#include "player-damage.h"
#include "monster-status.h"
#include "snipe.h"
#include "files.h"
#include "monster-spell.h"
#include "world.h"
#include "objectkind.h"
#include "autopick.h"
#include "save.h"
#include "report.h"

#include "view-mainwindow.h"

 /*!
  * @brief 修行僧の構え能力テーブル
  */
const kamae kamae_shurui[MAX_KAMAE] =
{
#ifdef JP
	{"玄武", 25, ""},
	{"白虎", 30, ""},
	{"青竜", 35, ""},
	{"朱雀", 40, ""},
#else
	{"Genbu", 25, "(Black Tortoise) "},
	{"Byakko", 30, "(White Tiger) "},
	{"Seiryuu", 35, "(Blue Dragon) "},
	{"Suzaku", 40, "(Red Phoenix) "},
#endif
};

/*!
 * @brief 剣術家の構え能力テーブル
 */
const kamae kata_shurui[MAX_KATA] =
{
#ifdef JP
	{"居合", 25, ""},
	{"風塵", 30, ""},
	{"降鬼", 35, ""},
	{"無想", 40, ""},
#else
	{"Iai", 25, ""},
	{"Huujin", 30, ""},
	{"Kouki", 35, ""},
	{"Musou", 40, ""},
#endif
};


/*!
 * @brief プレイヤーの継続行動を設定する。
 * @param typ 継続行動のID\n
 * #ACTION_NONE / #ACTION_SEARCH / #ACTION_REST / #ACTION_LEARN / #ACTION_FISH / #ACTION_KAMAE / #ACTION_KATA / #ACTION_SING / #ACTION_HAYAGAKE / #ACTION_SPELL から選択。
 * @return なし
 */
void set_action(player_type *creature_ptr, ACTION_IDX typ)
{
	int prev_typ = creature_ptr->action;

	if (typ == prev_typ)
	{
		return;
	}
	else
	{
		switch (prev_typ)
		{
			case ACTION_SEARCH:
			{
				msg_print(_("探索をやめた。", "You no longer walk carefully."));
				creature_ptr->redraw |= (PR_SPEED);
				break;
			}
			case ACTION_REST:
			{
				creature_ptr->resting = 0;
				break;
			}
			case ACTION_LEARN:
			{
				msg_print(_("学習をやめた。", "You stop Learning"));
				creature_ptr->new_mane = FALSE;
				break;
			}
			case ACTION_KAMAE:
			{
				msg_print(_("構えをといた。", "You stop assuming the posture."));
				creature_ptr->special_defense &= ~(KAMAE_MASK);
				break;
			}
			case ACTION_KATA:
			{
				msg_print(_("型を崩した。", "You stop assuming the posture."));
				creature_ptr->special_defense &= ~(KATA_MASK);
				creature_ptr->update |= (PU_MONSTERS);
				creature_ptr->redraw |= (PR_STATUS);
				break;
			}
			case ACTION_SING:
			{
				msg_print(_("歌うのをやめた。", "You stop singing."));
				break;
			}
			case ACTION_HAYAGAKE:
			{
				msg_print(_("足が重くなった。", "You are no longer walking extremely fast."));
				take_turn(creature_ptr, 100);
				break;
			}
			case ACTION_SPELL:
			{
				msg_print(_("呪文の詠唱を中断した。", "You stopped spelling all spells."));
				break;
			}
		}
	}

	creature_ptr->action = typ;

	/* If we are requested other action, stop singing */
	if (prev_typ == ACTION_SING) stop_singing(creature_ptr);
	if (prev_typ == ACTION_SPELL) stop_hex_spell();

	switch (creature_ptr->action)
	{
		case ACTION_SEARCH:
		{
			msg_print(_("注意深く歩き始めた。", "You begin to walk carefully."));
			creature_ptr->redraw |= (PR_SPEED);
			break;
		}
		case ACTION_LEARN:
		{
			msg_print(_("学習を始めた。", "You begin Learning"));
			break;
		}
		case ACTION_FISH:
		{
			msg_print(_("水面に糸を垂らした．．．", "You begin fishing..."));
			break;
		}
		case ACTION_HAYAGAKE:
		{
			msg_print(_("足が羽のように軽くなった。", "You begin to walk extremely fast."));
			break;
		}
		default:
		{
			break;
		}
	}
	creature_ptr->update |= (PU_BONUS);
	creature_ptr->redraw |= (PR_STATE);
}

/*!
 * @brief プレイヤーの全ての時限効果をリセットする。 / reset timed flags
 * @return なし
 */
void reset_tim_flags(player_type *creature_ptr)
{
	creature_ptr->fast = 0;            /* Timed -- Fast */
	creature_ptr->lightspeed = 0;
	creature_ptr->slow = 0;            /* Timed -- Slow */
	creature_ptr->blind = 0;           /* Timed -- Blindness */
	creature_ptr->paralyzed = 0;       /* Timed -- Paralysis */
	creature_ptr->confused = 0;        /* Timed -- Confusion */
	creature_ptr->afraid = 0;          /* Timed -- Fear */
	creature_ptr->image = 0;           /* Timed -- Hallucination */
	creature_ptr->poisoned = 0;        /* Timed -- Poisoned */
	creature_ptr->cut = 0;             /* Timed -- Cut */
	creature_ptr->stun = 0;            /* Timed -- Stun */

	creature_ptr->protevil = 0;        /* Timed -- Protection */
	creature_ptr->invuln = 0;          /* Timed -- Invulnerable */
	creature_ptr->ult_res = 0;
	creature_ptr->hero = 0;            /* Timed -- Heroism */
	creature_ptr->shero = 0;           /* Timed -- Super Heroism */
	creature_ptr->shield = 0;          /* Timed -- Shield Spell */
	creature_ptr->blessed = 0;         /* Timed -- Blessed */
	creature_ptr->tim_invis = 0;       /* Timed -- Invisibility */
	creature_ptr->tim_infra = 0;       /* Timed -- Infra Vision */
	creature_ptr->tim_regen = 0;       /* Timed -- Regeneration */
	creature_ptr->tim_stealth = 0;     /* Timed -- Stealth */
	creature_ptr->tim_esp = 0;
	creature_ptr->wraith_form = 0;     /* Timed -- Wraith Form */
	creature_ptr->tim_levitation = 0;
	creature_ptr->tim_sh_touki = 0;
	creature_ptr->tim_sh_fire = 0;
	creature_ptr->tim_sh_holy = 0;
	creature_ptr->tim_eyeeye = 0;
	creature_ptr->magicdef = 0;
	creature_ptr->resist_magic = 0;
	creature_ptr->tsuyoshi = 0;
	creature_ptr->kabenuke = 0;
	creature_ptr->tim_res_nether = 0;
	creature_ptr->tim_res_time = 0;
	creature_ptr->tim_mimic = 0;
	creature_ptr->mimic_form = 0;
	creature_ptr->tim_reflect = 0;
	creature_ptr->multishadow = 0;
	creature_ptr->dustrobe = 0;
	creature_ptr->action = ACTION_NONE;

	creature_ptr->oppose_acid = 0;     /* Timed -- oppose acid */
	creature_ptr->oppose_elec = 0;     /* Timed -- oppose lightning */
	creature_ptr->oppose_fire = 0;     /* Timed -- oppose heat */
	creature_ptr->oppose_cold = 0;     /* Timed -- oppose cold */
	creature_ptr->oppose_pois = 0;     /* Timed -- oppose poison */

	creature_ptr->word_recall = 0;
	creature_ptr->alter_reality = 0;
	creature_ptr->sutemi = FALSE;
	creature_ptr->counter = FALSE;
	creature_ptr->ele_attack = 0;
	creature_ptr->ele_immune = 0;
	creature_ptr->special_attack = 0L;
	creature_ptr->special_defense = 0L;

	while(creature_ptr->energy_need < 0) creature_ptr->energy_need += ENERGY_NEED();
	creature_ptr->timewalk = FALSE;

	if (PRACE_IS_(creature_ptr, RACE_DEMON) && (creature_ptr->lev > 44)) creature_ptr->oppose_fire = 1;
	if ((creature_ptr->pclass == CLASS_NINJA) && (creature_ptr->lev > 44)) creature_ptr->oppose_pois = 1;
	if (creature_ptr->pclass == CLASS_BERSERKER) creature_ptr->shero = 1;

	if (creature_ptr->riding)
	{
		(void)set_monster_fast(creature_ptr->riding, 0);
		(void)set_monster_slow(creature_ptr->riding, 0);
		(void)set_monster_invulner(creature_ptr->riding, 0, FALSE);
	}

	if (creature_ptr->pclass == CLASS_BARD)
	{
		SINGING_SONG_EFFECT(creature_ptr) = 0;
		SINGING_SONG_ID(creature_ptr) = 0;
	}
}

/*!
 * @brief プレイヤーに魔力消去効果を与える。
 * @return なし
 */
void dispel_player(player_type *creature_ptr)
{
	(void)set_fast(creature_ptr, 0, TRUE);
	(void)set_lightspeed(creature_ptr, 0, TRUE);
	(void)set_slow(creature_ptr, 0, TRUE);
	(void)set_shield(creature_ptr, 0, TRUE);
	(void)set_blessed(creature_ptr, 0, TRUE);
	(void)set_tsuyoshi(creature_ptr, 0, TRUE);
	(void)set_hero(creature_ptr, 0, TRUE);
	(void)set_shero(creature_ptr, 0, TRUE);
	(void)set_protevil(creature_ptr, 0, TRUE);
	(void)set_invuln(creature_ptr, 0, TRUE);
	(void)set_wraith_form(creature_ptr, 0, TRUE);
	(void)set_kabenuke(creature_ptr, 0, TRUE);
	(void)set_tim_res_nether(creature_ptr, 0, TRUE);
	(void)set_tim_res_time(creature_ptr, 0, TRUE);
	/* by henkma */
	(void)set_tim_reflect(creature_ptr, 0,TRUE);
	(void)set_multishadow(creature_ptr, 0,TRUE);
	(void)set_dustrobe(creature_ptr, 0,TRUE);

	(void)set_tim_invis(creature_ptr, 0, TRUE);
	(void)set_tim_infra(creature_ptr, 0, TRUE);
	(void)set_tim_esp(creature_ptr, 0, TRUE);
	(void)set_tim_regen(creature_ptr, 0, TRUE);
	(void)set_tim_stealth(creature_ptr, 0, TRUE);
	(void)set_tim_levitation(creature_ptr, 0, TRUE);
	(void)set_tim_sh_touki(creature_ptr, 0, TRUE);
	(void)set_tim_sh_fire(creature_ptr, 0, TRUE);
	(void)set_tim_sh_holy(creature_ptr, 0, TRUE);
	(void)set_tim_eyeeye(creature_ptr, 0, TRUE);
	(void)set_magicdef(creature_ptr, 0, TRUE);
	(void)set_resist_magic(creature_ptr, 0, TRUE);
	(void)set_oppose_acid(creature_ptr, 0, TRUE);
	(void)set_oppose_elec(creature_ptr, 0, TRUE);
	(void)set_oppose_fire(creature_ptr, 0, TRUE);
	(void)set_oppose_cold(creature_ptr, 0, TRUE);
	(void)set_oppose_pois(creature_ptr, 0, TRUE);
	(void)set_ultimate_res(creature_ptr, 0, TRUE);
	(void)set_mimic(creature_ptr, 0, 0, TRUE);
	(void)set_ele_attack(creature_ptr, 0, 0);
	(void)set_ele_immune(creature_ptr, 0, 0);

	/* Cancel glowing hands */
	if (creature_ptr->special_attack & ATTACK_CONFUSE)
	{
		creature_ptr->special_attack &= ~(ATTACK_CONFUSE);
		msg_print(_("手の輝きがなくなった。", "Your hands stop glowing."));
	}

	if (music_singing_any(p_ptr) || hex_spelling_any(p_ptr))
	{
		concptr str = (music_singing_any(p_ptr)) ? _("歌", "singing") : _("呪文", "spelling");
		INTERUPTING_SONG_EFFECT(creature_ptr) = SINGING_SONG_EFFECT(creature_ptr);
		SINGING_SONG_EFFECT(creature_ptr) = MUSIC_NONE;
		msg_format(_("%sが途切れた。", "Your %s is interrupted."), str);

		creature_ptr->action = ACTION_NONE;
		creature_ptr->update |= (PU_BONUS | PU_HP | PU_MONSTERS);
		creature_ptr->redraw |= (PR_MAP | PR_STATUS | PR_STATE);
		creature_ptr->window |= (PW_OVERHEAD | PW_DUNGEON);
		creature_ptr->energy_need += ENERGY_NEED();
	}
}


/*!
 * @brief 変身効果の継続時間と変身先をセットする / Set "tim_mimic", and "mimic_form", notice observable changes
 * @param v 継続時間
 * @param p 変身内容
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_mimic(player_type *creature_ptr, TIME_EFFECT v, MIMIC_RACE_IDX p, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->tim_mimic && (creature_ptr->mimic_form == p) && !do_dec)
		{
			if (creature_ptr->tim_mimic > v) return FALSE;
		}
		else if ((!creature_ptr->tim_mimic) || (creature_ptr->mimic_form != p))
		{
			msg_print(_("自分の体が変わってゆくのを感じた。", "You feel that your body changes."));
			creature_ptr->mimic_form = p;
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->tim_mimic)
		{
			msg_print(_("変身が解けた。", "You are no longer transformed."));
			if (creature_ptr->mimic_form == MIMIC_DEMON) set_oppose_fire(creature_ptr, 0, TRUE);
			creature_ptr->mimic_form=0;
			notice = TRUE;
			p = 0;
		}
	}

	/* Use the value */
	creature_ptr->tim_mimic = v;

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, TRUE);

	creature_ptr->redraw |= (PR_BASIC | PR_STATUS);
	creature_ptr->update |= (PU_BONUS | PU_HP);

	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 盲目の継続時間をセットする / Set "blind", notice observable changes
 * @param v 継続時間
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 * @details
 * Note the use of "PU_UN_LITE" and "PU_UN_VIEW", which is needed to\n
 * memorize any terrain features which suddenly become "visible".\n
 * Note that blindness is currently the only thing which can affect\n
 * "player_can_see_bold()".\n
 */
bool set_blind(player_type *creature_ptr, TIME_EFFECT v)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (!creature_ptr->blind)
		{
			if (creature_ptr->prace == RACE_ANDROID)
			{
				msg_print(_("センサーをやられた！", "You are blind!"));
			}
			else
			{
				msg_print(_("目が見えなくなってしまった！", "You are blind!"));
			}

			notice = TRUE;
			chg_virtue(creature_ptr, V_ENLIGHTEN, -1);
		}
	}

	else
	{
		if (creature_ptr->blind)
		{
			if (creature_ptr->prace == RACE_ANDROID)
			{
				msg_print(_("センサーが復旧した。", "You can see again."));
			}
			else
			{
				msg_print(_("やっと目が見えるようになった。", "You can see again."));
			}

			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->blind = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);
	if (disturb_state) disturb(FALSE, FALSE);

	/* Fully update the visuals */
	creature_ptr->update |= (PU_UN_VIEW | PU_UN_LITE | PU_VIEW | PU_LITE | PU_MONSTERS | PU_MON_LITE);
	creature_ptr->redraw |= (PR_MAP);
	creature_ptr->window |= (PW_OVERHEAD | PW_DUNGEON);
	handle_stuff();
	return (TRUE);
}


/*!
 * @brief 混乱の継続時間をセットする / Set "confused", notice observable changes
 * @param v 継続時間
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_confused(player_type *creature_ptr, TIME_EFFECT v)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (!creature_ptr->confused)
		{
			msg_print(_("あなたは混乱した！", "You are confused!"));

			if (creature_ptr->action == ACTION_LEARN)
			{
				msg_print(_("学習が続けられない！", "You cannot continue Learning!"));
				creature_ptr->new_mane = FALSE;

				creature_ptr->redraw |= (PR_STATE);
				creature_ptr->action = ACTION_NONE;
			}
			if (creature_ptr->action == ACTION_KAMAE)
			{
				msg_print(_("構えがとけた。", "Your posture gets loose."));
				creature_ptr->special_defense &= ~(KAMAE_MASK);
				creature_ptr->update |= (PU_BONUS);
				creature_ptr->redraw |= (PR_STATE);
				creature_ptr->action = ACTION_NONE;
			}
			else if (creature_ptr->action == ACTION_KATA)
			{
				msg_print(_("型が崩れた。", "Your posture gets loose."));
				creature_ptr->special_defense &= ~(KATA_MASK);
				creature_ptr->update |= (PU_BONUS);
				creature_ptr->update |= (PU_MONSTERS);
				creature_ptr->redraw |= (PR_STATE);
				creature_ptr->redraw |= (PR_STATUS);
				creature_ptr->action = ACTION_NONE;
			}

			/* Sniper */
			if (creature_ptr->concent) reset_concentration(TRUE);

			/* Hex */
			if (hex_spelling_any(p_ptr)) stop_hex_spell_all();

			notice = TRUE;
			creature_ptr->counter = FALSE;
			chg_virtue(creature_ptr, V_HARMONY, -1);
		}
	}

	else
	{
		if (creature_ptr->confused)
		{
			msg_print(_("やっと混乱がおさまった。", "You feel less confused now."));
			creature_ptr->special_attack &= ~(ATTACK_SUIKEN);
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->confused = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	handle_stuff();
	return (TRUE);
}


/*!
 * @brief 毒の継続時間をセットする / Set "poisoned", notice observable changes
 * @param v 継続時間
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_poisoned(player_type *creature_ptr, TIME_EFFECT v)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (!creature_ptr->poisoned)
		{
			msg_print(_("毒に侵されてしまった！", "You are poisoned!"));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->poisoned)
		{
			msg_print(_("やっと毒の痛みがなくなった。", "You are no longer poisoned."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->poisoned = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	handle_stuff();
	return (TRUE);
}


/*!
 * @brief 恐怖の継続時間をセットする / Set "afraid", notice observable changes
 * @param v 継続時間
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_afraid(player_type *creature_ptr, TIME_EFFECT v)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (!creature_ptr->afraid)
		{
			msg_print(_("何もかも恐くなってきた！", "You are terrified!"));

			if (creature_ptr->special_defense & KATA_MASK)
			{
				msg_print(_("型が崩れた。", "Your posture gets loose."));
				creature_ptr->special_defense &= ~(KATA_MASK);
				creature_ptr->update |= (PU_BONUS);
				creature_ptr->update |= (PU_MONSTERS);
				creature_ptr->redraw |= (PR_STATE);
				creature_ptr->redraw |= (PR_STATUS);
				creature_ptr->action = ACTION_NONE;
			}

			notice = TRUE;
			creature_ptr->counter = FALSE;
			chg_virtue(creature_ptr, V_VALOUR, -1);
		}
	}

	else
	{
		if (creature_ptr->afraid)
		{
			msg_print(_("やっと恐怖を振り払った。", "You feel bolder now."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->afraid = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 麻痺の継続時間をセットする / Set "paralyzed", notice observable changes
 * @param v 継続時間
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_paralyzed(player_type *creature_ptr, TIME_EFFECT v)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (!creature_ptr->paralyzed)
		{
			msg_print(_("体が麻痺してしまった！", "You are paralyzed!"));
			/* Sniper */
			if (creature_ptr->concent) reset_concentration(TRUE);

			/* Hex */
			if (hex_spelling_any(p_ptr)) stop_hex_spell_all();

			creature_ptr->counter = FALSE;
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->paralyzed)
		{
			msg_print(_("やっと動けるようになった。", "You can move again."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->paralyzed = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->redraw |= (PR_STATE);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 幻覚の継続時間をセットする / Set "image", notice observable changes
 * @param v 継続時間
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 * @details Note that we must redraw the map when hallucination changes.
 */
bool set_image(player_type *creature_ptr, TIME_EFFECT v)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;
	if (creature_ptr->pseikaku == SEIKAKU_CHARGEMAN) v = 0;

	if (v)
	{
		set_tsuyoshi(creature_ptr, 0, TRUE);
		if (!creature_ptr->image)
		{
			msg_print(_("ワーオ！何もかも虹色に見える！", "Oh, wow! Everything looks so cosmic now!"));

			/* Sniper */
			if (creature_ptr->concent) reset_concentration(TRUE);

			creature_ptr->counter = FALSE;
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->image)
		{
			msg_print(_("やっとはっきりと物が見えるようになった。", "You can see clearly again."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->image = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, TRUE);

	creature_ptr->redraw |= (PR_MAP | PR_HEALTH | PR_UHEALTH);
	creature_ptr->update |= (PU_MONSTERS);
	creature_ptr->window |= (PW_OVERHEAD | PW_DUNGEON);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 加速の継続時間をセットする / Set "fast", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_fast(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->fast && !do_dec)
		{
			if (creature_ptr->fast > v) return FALSE;
		}
		else if (!IS_FAST() && !creature_ptr->lightspeed)
		{
			msg_print(_("素早く動けるようになった！", "You feel yourself moving much faster!"));
			notice = TRUE;
			chg_virtue(creature_ptr, V_PATIENCE, -1);
			chg_virtue(creature_ptr, V_DILIGENCE, 1);
		}
	}

	else
	{
		if (creature_ptr->fast && !creature_ptr->lightspeed && !music_singing(p_ptr, MUSIC_SPEED) && !music_singing(p_ptr, MUSIC_SHERO))
		{
			msg_print(_("動きの素早さがなくなったようだ。", "You feel yourself slow down."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->fast = v;

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 光速移動の継続時間をセットする / Set "lightspeed", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_lightspeed(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (creature_ptr->wild_mode) v = 0;

	if (v)
	{
		if (creature_ptr->lightspeed && !do_dec)
		{
			if (creature_ptr->lightspeed > v) return FALSE;
		}
		else if (!creature_ptr->lightspeed)
		{
			msg_print(_("非常に素早く動けるようになった！", "You feel yourself moving extremely faster!"));
			notice = TRUE;
			chg_virtue(creature_ptr, V_PATIENCE, -1);
			chg_virtue(creature_ptr, V_DILIGENCE, 1);
		}
	}

	else
	{
		if (creature_ptr->lightspeed)
		{
			msg_print(_("動きの素早さがなくなったようだ。", "You feel yourself slow down."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->lightspeed = v;

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 減速の継続時間をセットする / Set "slow", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_slow(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->slow && !do_dec)
		{
			if (creature_ptr->slow > v) return FALSE;
		}
		else if (!creature_ptr->slow)
		{
			msg_print(_("体の動きが遅くなってしまった！", "You feel yourself moving slower!"));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->slow)
		{
			msg_print(_("動きの遅さがなくなったようだ。", "You feel yourself speed up."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->slow = v;

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);
	handle_stuff();
	return (TRUE);
}


/*!
 * @brief 肌石化の継続時間をセットする / Set "shield", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_shield(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->shield && !do_dec)
		{
			if (creature_ptr->shield > v) return FALSE;
		}
		else if (!creature_ptr->shield)
		{
			msg_print(_("肌が石になった。", "Your skin turns to stone."));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->shield)
		{
			msg_print(_("肌が元に戻った。", "Your skin returns to normal."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->shield = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);
	handle_stuff();
	return (TRUE);
}


/*!
 * @brief つぶれるの継続時間をセットする / Set "tsubureru", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_tsubureru(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->tsubureru && !do_dec)
		{
			if (creature_ptr->tsubureru > v) return FALSE;
		}
		else if (!creature_ptr->tsubureru)
		{
			msg_print(_("横に伸びた。", "Your body expands horizontally."));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->tsubureru)
		{
			msg_print(_("もう横に伸びていない。", "Your body returns to normal."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->tsubureru = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);
	handle_stuff();
	return (TRUE);
}


/*!
 * @brief 魔法の鎧の継続時間をセットする / Set "magicdef", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_magicdef(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->magicdef && !do_dec)
		{
			if (creature_ptr->magicdef > v) return FALSE;
		}
		else if (!creature_ptr->magicdef)
		{
			msg_print(_("魔法の防御力が増したような気がする。", "You feel more resistant to magic."));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->magicdef)
		{
			msg_print(_("魔法の防御力が元に戻った。", "You feel less resistant to magic."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->magicdef = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 祝福の継続時間をセットする / Set "blessed", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_blessed(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->blessed && !do_dec)
		{
			if (creature_ptr->blessed > v) return FALSE;
		}
		else if (!IS_BLESSED())
		{
			msg_print(_("高潔な気分になった！", "You feel righteous!"));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->blessed && !music_singing(p_ptr, MUSIC_BLESS))
		{
			msg_print(_("高潔な気分が消え失せた。", "The prayer has expired."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->blessed = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);
	handle_stuff();
	return (TRUE);
}


/*!
 * @brief 士気高揚の継続時間をセットする / Set "hero", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_hero(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->hero && !do_dec)
		{
			if (creature_ptr->hero > v) return FALSE;
		}
		else if (!IS_HERO())
		{
			msg_print(_("ヒーローになった気がする！", "You feel like a hero!"));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->hero && !music_singing(p_ptr, MUSIC_HERO) && !music_singing(p_ptr, MUSIC_SHERO))
		{
			msg_print(_("ヒーローの気分が消え失せた。", "The heroism wears off."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->hero = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);

	/* Recalculate hitpoints */
	creature_ptr->update |= (PU_HP);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 狂戦士化の継続時間をセットする / Set "shero", notice observable changes
 * @param v 継続時間/ 0ならば無条件にリセット
 * @param do_dec FALSEの場合現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_shero(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (creature_ptr->pclass == CLASS_BERSERKER) v = 1;
	if (v)
	{
		if (creature_ptr->shero && !do_dec)
		{
			if (creature_ptr->shero > v) return FALSE;
		}
		else if (!creature_ptr->shero)
		{
			msg_print(_("殺戮マシーンになった気がする！", "You feel like a killing machine!"));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->shero)
		{
			msg_print(_("野蛮な気持ちが消え失せた。", "You feel less Berserk."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->shero = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);

	/* Recalculate hitpoints */
	creature_ptr->update |= (PU_HP);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 対邪悪結界の継続時間をセットする / Set "protevil", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_protevil(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->protevil && !do_dec)
		{
			if (creature_ptr->protevil > v) return FALSE;
		}
		else if (!creature_ptr->protevil)
		{
			msg_print(_("邪悪なる存在から守られているような感じがする！", "You feel safe from evil!"));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->protevil)
		{
			msg_print(_("邪悪なる存在から守られている感じがなくなった。", "You no longer feel safe from evil."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->protevil = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 幽体化の継続時間をセットする / Set "wraith_form", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_wraith_form(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->wraith_form && !do_dec)
		{
			if (creature_ptr->wraith_form > v) return FALSE;
		}
		else if (!creature_ptr->wraith_form)
		{
			msg_print(_("物質界を離れて幽鬼のような存在になった！", "You leave the physical world and current_world_ptr->game_turn into a wraith-being!"));
			notice = TRUE;
			chg_virtue(creature_ptr, V_UNLIFE, 3);
			chg_virtue(creature_ptr, V_HONOUR, -2);
			chg_virtue(creature_ptr, V_SACRIFICE, -2);
			chg_virtue(creature_ptr, V_VALOUR, -5);

			creature_ptr->redraw |= (PR_MAP);
			creature_ptr->update |= (PU_MONSTERS);

			creature_ptr->window |= (PW_OVERHEAD | PW_DUNGEON);
		}
	}

	else
	{
		if (creature_ptr->wraith_form)
		{
			msg_print(_("不透明になった感じがする。", "You feel opaque."));
			notice = TRUE;

			creature_ptr->redraw |= (PR_MAP);
			creature_ptr->update |= (PU_MONSTERS);

			creature_ptr->window |= (PW_OVERHEAD | PW_DUNGEON);
		}
	}

	/* Use the value */
	creature_ptr->wraith_form = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);
	handle_stuff();
	return (TRUE);

}

/*!
 * @brief 無傷球の継続時間をセットする / Set "invuln", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_invuln(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->invuln && !do_dec)
		{
			if (creature_ptr->invuln > v) return FALSE;
		}
		else if (!IS_INVULN())
		{
			msg_print(_("無敵だ！", "Invulnerability!"));
			notice = TRUE;

			chg_virtue(creature_ptr, V_UNLIFE, -2);
			chg_virtue(creature_ptr, V_HONOUR, -2);
			chg_virtue(creature_ptr, V_SACRIFICE, -3);
			chg_virtue(creature_ptr, V_VALOUR, -5);

			creature_ptr->redraw |= (PR_MAP);
			creature_ptr->update |= (PU_MONSTERS);

			creature_ptr->window |= (PW_OVERHEAD | PW_DUNGEON);
		}
	}

	else
	{
		if (creature_ptr->invuln && !music_singing(p_ptr, MUSIC_INVULN))
		{
			msg_print(_("無敵ではなくなった。", "The invulnerability wears off."));
			notice = TRUE;

			creature_ptr->redraw |= (PR_MAP);
			creature_ptr->update |= (PU_MONSTERS);

			creature_ptr->window |= (PW_OVERHEAD | PW_DUNGEON);

			creature_ptr->energy_need += ENERGY_NEED();
		}
	}

	/* Use the value */
	creature_ptr->invuln = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 時限ESPの継続時間をセットする / Set "tim_esp", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_tim_esp(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->tim_esp && !do_dec)
		{
			if (creature_ptr->tim_esp > v) return FALSE;
		}
		else if (!IS_TIM_ESP())
		{
			msg_print(_("意識が広がった気がする！", "You feel your consciousness expand!"));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->tim_esp && !music_singing(p_ptr, MUSIC_MIND))
		{
			msg_print(_("意識は元に戻った。", "Your consciousness contracts again."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->tim_esp = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);
	creature_ptr->update |= (PU_MONSTERS);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 時限透明視の継続時間をセットする / Set "tim_invis", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_tim_invis(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->tim_invis && !do_dec)
		{
			if (creature_ptr->tim_invis > v) return FALSE;
		}
		else if (!creature_ptr->tim_invis)
		{
			msg_print(_("目が非常に敏感になった気がする！", "Your eyes feel very sensitive!"));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->tim_invis)
		{
			msg_print(_("目の敏感さがなくなったようだ。", "Your eyes feel less sensitive."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->tim_invis = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);

	/* Update the monsters */
	creature_ptr->update |= (PU_MONSTERS);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 時限赤外線視力の継続時間をセットする / Set "tim_infra", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_tim_infra(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->tim_infra && !do_dec)
		{
			if (creature_ptr->tim_infra > v) return FALSE;
		}
		else if (!creature_ptr->tim_infra)
		{
			msg_print(_("目がランランと輝き始めた！", "Your eyes begin to tingle!"));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->tim_infra)
		{
			msg_print(_("目の輝きがなくなった。", "Your eyes stop tingling."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->tim_infra = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);

	/* Update the monsters */
	creature_ptr->update |= (PU_MONSTERS);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 時限急回復の継続時間をセットする / Set "tim_regen", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_tim_regen(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->tim_regen && !do_dec)
		{
			if (creature_ptr->tim_regen > v) return FALSE;
		}
		else if (!creature_ptr->tim_regen)
		{
			msg_print(_("回復力が上がった！", "You feel yourself regenerating quickly!"));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->tim_regen)
		{
			msg_print(_("素早く回復する感じがなくなった。", "You feel yourself regenerating slowly."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->tim_regen = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 隠密の歌の継続時間をセットする / Set "tim_stealth", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_tim_stealth(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->tim_stealth && !do_dec)
		{
			if (creature_ptr->tim_stealth > v) return FALSE;
		}
		else if (!IS_TIM_STEALTH())
		{
			msg_print(_("足音が小さくなった！", "You begin to walk silently!"));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->tim_stealth && !music_singing(p_ptr, MUSIC_STEALTH))
		{
			msg_print(_("足音が大きくなった。", "You no longer walk silently."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->tim_stealth = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 超隠密状態をセットする
 * @param set TRUEならば超隠密状態になる。
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_superstealth(player_type *creature_ptr, bool set)
{
	bool notice = FALSE;

	if (creature_ptr->is_dead) return FALSE;

	if (set)
	{
		if (!(creature_ptr->special_defense & NINJA_S_STEALTH))
		{
			if (current_floor_ptr->grid_array[creature_ptr->y][creature_ptr->x].info & CAVE_MNLT)
			{
				msg_print(_("敵の目から薄い影の中に覆い隠された。", "You are mantled in weak shadow from ordinary eyes."));
				creature_ptr->monlite = creature_ptr->old_monlite = TRUE;
			}
			else
			{
				msg_print(_("敵の目から影の中に覆い隠された！", "You are mantled in shadow from ordinary eyes!"));
				creature_ptr->monlite = creature_ptr->old_monlite = FALSE;
			}

			notice = TRUE;

			/* Use the value */
			creature_ptr->special_defense |= NINJA_S_STEALTH;
		}
	}

	else
	{
		if (creature_ptr->special_defense & NINJA_S_STEALTH)
		{
			msg_print(_("再び敵の目にさらされるようになった。", "You are exposed to common sight once more."));
			notice = TRUE;

			/* Use the value */
			creature_ptr->special_defense &= ~(NINJA_S_STEALTH);
		}
	}

	/* Nothing to notice */
	if (!notice) return (FALSE);
	creature_ptr->redraw |= (PR_STATUS);

	if (disturb_state) disturb(FALSE, FALSE);
	return (TRUE);
}

/*!
 * @brief 一時的浮遊の継続時間をセットする / Set "tim_levitation", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_tim_levitation(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->tim_levitation && !do_dec)
		{
			if (creature_ptr->tim_levitation > v) return FALSE;
		}
		else if (!creature_ptr->tim_levitation)
		{
			msg_print(_("体が宙に浮き始めた。", "You begin to fly!"));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->tim_levitation)
		{
			msg_print(_("もう宙に浮かべなくなった。", "You stop flying."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->tim_levitation = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 一時的闘気のオーラの継続時間をセットする / Set "tim_sh_touki", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_tim_sh_touki(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->tim_sh_touki && !do_dec)
		{
			if (creature_ptr->tim_sh_touki > v) return FALSE;
		}
		else if (!creature_ptr->tim_sh_touki)
		{
			msg_print(_("体が闘気のオーラで覆われた。", "You have enveloped by the aura of the Force!"));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->tim_sh_touki)
		{
			msg_print(_("闘気が消えた。", "Aura of the Force disappeared."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->tim_sh_touki = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 一時的火炎のオーラの継続時間をセットする / Set "tim_sh_fire", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_tim_sh_fire(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->tim_sh_fire && !do_dec)
		{
			if (creature_ptr->tim_sh_fire > v) return FALSE;
		}
		else if (!creature_ptr->tim_sh_fire)
		{
			msg_print(_("体が炎のオーラで覆われた。", "You have enveloped by fiery aura!"));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->tim_sh_fire)
		{
			msg_print(_("炎のオーラが消えた。", "Fiery aura disappeared."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->tim_sh_fire = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 一時的聖なるのオーラの継続時間をセットする / Set "tim_sh_holy", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_tim_sh_holy(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->tim_sh_holy && !do_dec)
		{
			if (creature_ptr->tim_sh_holy > v) return FALSE;
		}
		else if (!creature_ptr->tim_sh_holy)
		{
			msg_print(_("体が聖なるオーラで覆われた。", "You have enveloped by holy aura!"));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->tim_sh_holy)
		{
			msg_print(_("聖なるオーラが消えた。", "Holy aura disappeared."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->tim_sh_holy = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 目には目をの残り時間をセットする / Set "tim_eyeeye", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_tim_eyeeye(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->tim_eyeeye && !do_dec)
		{
			if (creature_ptr->tim_eyeeye > v) return FALSE;
		}
		else if (!creature_ptr->tim_eyeeye)
		{
			msg_print(_("法の守り手になった気がした！", "You feel like a keeper of commandments!"));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->tim_eyeeye)
		{
			msg_print(_("懲罰を執行することができなくなった。", "You no longer feel like a keeper."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->tim_eyeeye = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);
	handle_stuff();
	return (TRUE);
}


/*!
 * @brief 一時的魔法防御の継続時間をセットする / Set "resist_magic", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_resist_magic(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->resist_magic && !do_dec)
		{
			if (creature_ptr->resist_magic > v) return FALSE;
		}
		else if (!creature_ptr->resist_magic)
		{
			msg_print(_("魔法への耐性がついた。", "You have been protected from magic!"));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->resist_magic)
		{
			msg_print(_("魔法に弱くなった。", "You are no longer protected from magic."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->resist_magic = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 一時的反射の継続時間をセットする / Set "tim_reflect", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_tim_reflect(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->tim_reflect && !do_dec)
		{
			if (creature_ptr->tim_reflect > v) return FALSE;
		}
		else if (!creature_ptr->tim_reflect)
		{
			msg_print(_("体の表面が滑かになった気がする。", "Your body becames smooth."));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->tim_reflect)
		{
			msg_print(_("体の表面が滑かでなくなった。", "Your body is no longer smooth."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->tim_reflect = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);
	handle_stuff();
	return (TRUE);
}


/*
 * Set "multishadow", notice observable changes
 */
bool set_multishadow(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->multishadow && !do_dec)
		{
			if (creature_ptr->multishadow > v) return FALSE;
		}
		else if (!creature_ptr->multishadow)
		{
			msg_print(_("あなたの周りに幻影が生まれた。", "Your Shadow enveloped you."));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->multishadow)
		{
			msg_print(_("幻影が消えた。", "Your Shadow disappears."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->multishadow = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 一時的破片のオーラの継続時間をセットする / Set "dustrobe", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_dustrobe(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->dustrobe && !do_dec)
		{
			if (creature_ptr->dustrobe > v) return FALSE;
		}
		else if (!creature_ptr->dustrobe)
		{
			msg_print(_("体が鏡のオーラで覆われた。", "You were enveloped by mirror shards."));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->dustrobe)
		{
			msg_print(_("鏡のオーラが消えた。", "The mirror shards disappear."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->dustrobe = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 一時的壁抜けの継続時間をセットする / Set "kabenuke", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_kabenuke(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->kabenuke && !do_dec)
		{
			if (creature_ptr->kabenuke > v) return FALSE;
		}
		else if (!creature_ptr->kabenuke)
		{
			msg_print(_("体が半物質の状態になった。", "You became ethereal form."));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->kabenuke)
		{
			msg_print(_("体が物質化した。", "You are no longer in an ethereal form."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->kabenuke = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief オクレ兄さんの継続時間をセットする / Set "tsuyoshi", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_tsuyoshi(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->tsuyoshi && !do_dec)
		{
			if (creature_ptr->tsuyoshi > v) return FALSE;
		}
		else if (!creature_ptr->tsuyoshi)
		{
			msg_print(_("「オクレ兄さん！」", "Brother OKURE!"));
			notice = TRUE;
			chg_virtue(creature_ptr, V_VITALITY, 2);
		}
	}

	else
	{
		if (creature_ptr->tsuyoshi)
		{
			msg_print(_("肉体が急速にしぼんでいった。", "Your body had quickly shriveled."));

			(void)dec_stat(creature_ptr, A_CON, 20, TRUE);
			(void)dec_stat(creature_ptr, A_STR, 20, TRUE);

			notice = TRUE;
			chg_virtue(creature_ptr, V_VITALITY, -3);
		}
	}

	/* Use the value */
	creature_ptr->tsuyoshi = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);

	/* Recalculate hitpoints */
	creature_ptr->update |= (PU_HP);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 一時的元素スレイの継続時間をセットする / Set a temporary elemental brand. Clear all other brands. Print status messages. -LM-
 * @param attack_type スレイのタイプID
 * @param v 継続時間
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_ele_attack(player_type *creature_ptr, u32b attack_type, TIME_EFFECT v)
{
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	/* Clear all elemental attacks (only one is allowed at a time). */
	if ((creature_ptr->special_attack & (ATTACK_ACID)) && (attack_type != ATTACK_ACID))
	{
		creature_ptr->special_attack &= ~(ATTACK_ACID);
		msg_print(_("酸で攻撃できなくなった。", "Your temporary acidic brand fades away."));
	}
	if ((creature_ptr->special_attack & (ATTACK_ELEC)) && (attack_type != ATTACK_ELEC))
	{
		creature_ptr->special_attack &= ~(ATTACK_ELEC);
		msg_print(_("電撃で攻撃できなくなった。", "Your temporary electrical brand fades away."));
	}
	if ((creature_ptr->special_attack & (ATTACK_FIRE)) && (attack_type != ATTACK_FIRE))
	{
		creature_ptr->special_attack &= ~(ATTACK_FIRE);
		msg_print(_("火炎で攻撃できなくなった。", "Your temporary fiery brand fades away."));
	}
	if ((creature_ptr->special_attack & (ATTACK_COLD)) && (attack_type != ATTACK_COLD))
	{
		creature_ptr->special_attack &= ~(ATTACK_COLD);
		msg_print(_("冷気で攻撃できなくなった。", "Your temporary frost brand fades away."));
	}
	if ((creature_ptr->special_attack & (ATTACK_POIS)) && (attack_type != ATTACK_POIS))
	{
		creature_ptr->special_attack &= ~(ATTACK_POIS);
		msg_print(_("毒で攻撃できなくなった。", "Your temporary poison brand fades away."));
	}

	if ((v) && (attack_type))
	{
		/* Set attack type. */
		creature_ptr->special_attack |= (attack_type);

		/* Set duration. */
		creature_ptr->ele_attack = v;

#ifdef JP
		msg_format("%sで攻撃できるようになった！",
			     ((attack_type == ATTACK_ACID) ? "酸" :
			      ((attack_type == ATTACK_ELEC) ? "電撃" :
			       ((attack_type == ATTACK_FIRE) ? "火炎" : 
				((attack_type == ATTACK_COLD) ? "冷気" : 
				 ((attack_type == ATTACK_POIS) ? "毒" : 
					"(なし)"))))));
#else
		msg_format("For a while, the blows you deal will %s",
			     ((attack_type == ATTACK_ACID) ? "melt with acid!" :
			      ((attack_type == ATTACK_ELEC) ? "shock your foes!" :
			       ((attack_type == ATTACK_FIRE) ? "burn with fire!" : 
				((attack_type == ATTACK_COLD) ? "chill to the bone!" : 
				 ((attack_type == ATTACK_POIS) ? "poison your enemies!" : 
					"do nothing special."))))));
#endif
	}

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->redraw |= (PR_STATUS);

	creature_ptr->update |= (PU_BONUS);
	handle_stuff();

	return (TRUE);
}

/*!
 * @brief 一時的元素免疫の継続時間をセットする / Set a temporary elemental brand.  Clear all other brands.  Print status messages. -LM-
 * @param immune_type 免疫のタイプID
 * @param v 継続時間
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_ele_immune(player_type *creature_ptr, u32b immune_type, TIME_EFFECT v)
{
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	/* Clear all elemental attacks (only one is allowed at a time). */
	if ((creature_ptr->special_defense & (DEFENSE_ACID)) && (immune_type != DEFENSE_ACID))
	{
		creature_ptr->special_defense &= ~(DEFENSE_ACID);
		msg_print(_("酸の攻撃で傷つけられるようになった。。", "You are no longer immune to acid."));
	}
	if ((creature_ptr->special_defense & (DEFENSE_ELEC)) && (immune_type != DEFENSE_ELEC))
	{
		creature_ptr->special_defense &= ~(DEFENSE_ELEC);
		msg_print(_("電撃の攻撃で傷つけられるようになった。。", "You are no longer immune to electricity."));
	}
	if ((creature_ptr->special_defense & (DEFENSE_FIRE)) && (immune_type != DEFENSE_FIRE))
	{
		creature_ptr->special_defense &= ~(DEFENSE_FIRE);
		msg_print(_("火炎の攻撃で傷つけられるようになった。。", "You are no longer immune to fire."));
	}
	if ((creature_ptr->special_defense & (DEFENSE_COLD)) && (immune_type != DEFENSE_COLD))
	{
		creature_ptr->special_defense &= ~(DEFENSE_COLD);
		msg_print(_("冷気の攻撃で傷つけられるようになった。。", "You are no longer immune to cold."));
	}
	if ((creature_ptr->special_defense & (DEFENSE_POIS)) && (immune_type != DEFENSE_POIS))
	{
		creature_ptr->special_defense &= ~(DEFENSE_POIS);
		msg_print(_("毒の攻撃で傷つけられるようになった。。", "You are no longer immune to poison."));
	}

	if ((v) && (immune_type))
	{
		/* Set attack type. */
		creature_ptr->special_defense |= (immune_type);

		/* Set duration. */
		creature_ptr->ele_immune = v;

		msg_format(_("%sの攻撃を受けつけなくなった！", "For a while, You are immune to %s"),
			     ((immune_type == DEFENSE_ACID) ? _("酸", "acid!") :
			      ((immune_type == DEFENSE_ELEC) ? _("電撃", "electricity!") :
			       ((immune_type == DEFENSE_FIRE) ? _("火炎", "fire!") : 
				((immune_type == DEFENSE_COLD) ? _("冷気", "cold!") : 
				 ((immune_type == DEFENSE_POIS) ? _("毒", "poison!") : 
					_("(なし)", "do nothing special.")))))));
	}

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->redraw |= (PR_STATUS);
	creature_ptr->update |= (PU_BONUS);
	handle_stuff();

	return (TRUE);
}

/*!
 * @brief 一時的酸耐性の継続時間をセットする / Set "oppose_acid", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_oppose_acid(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->oppose_acid && !do_dec)
		{
			if (creature_ptr->oppose_acid > v) return FALSE;
		}
		else if (!IS_OPPOSE_ACID())
		{
			msg_print(_("酸への耐性がついた気がする！", "You feel resistant to acid!"));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->oppose_acid && !music_singing(p_ptr, MUSIC_RESIST) && !(creature_ptr->special_defense & KATA_MUSOU))
		{
			msg_print(_("酸への耐性が薄れた気がする。", "You feel less resistant to acid."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->oppose_acid = v;

	/* Nothing to notice */
	if (!notice) return (FALSE);
	creature_ptr->redraw |= (PR_STATUS);

	if (disturb_state) disturb(FALSE, FALSE);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 一時的電撃耐性の継続時間をセットする / Set "oppose_elec", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_oppose_elec(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->oppose_elec && !do_dec)
		{
			if (creature_ptr->oppose_elec > v) return FALSE;
		}
		else if (!IS_OPPOSE_ELEC())
		{
			msg_print(_("電撃への耐性がついた気がする！", "You feel resistant to electricity!"));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->oppose_elec && !music_singing(p_ptr, MUSIC_RESIST) && !(creature_ptr->special_defense & KATA_MUSOU))
		{
			msg_print(_("電撃への耐性が薄れた気がする。", "You feel less resistant to electricity."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->oppose_elec = v;

	/* Nothing to notice */
	if (!notice) return (FALSE);
	creature_ptr->redraw |= (PR_STATUS);

	if (disturb_state) disturb(FALSE, FALSE);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 一時的火炎耐性の継続時間をセットする / Set "oppose_fire", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_oppose_fire(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if ((PRACE_IS_(creature_ptr, RACE_DEMON) && (creature_ptr->lev > 44)) || (creature_ptr->mimic_form == MIMIC_DEMON)) v = 1;
	if (v)
	{
		if (creature_ptr->oppose_fire && !do_dec)
		{
			if (creature_ptr->oppose_fire > v) return FALSE;
		}
		else if (!IS_OPPOSE_FIRE())
		{
			msg_print(_("火への耐性がついた気がする！", "You feel resistant to fire!"));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->oppose_fire && !music_singing(p_ptr, MUSIC_RESIST) && !(creature_ptr->special_defense & KATA_MUSOU))
		{
			msg_print(_("火への耐性が薄れた気がする。", "You feel less resistant to fire."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->oppose_fire = v;

	/* Nothing to notice */
	if (!notice) return (FALSE);
	creature_ptr->redraw |= (PR_STATUS);

	if (disturb_state) disturb(FALSE, FALSE);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 一時的冷気耐性の継続時間をセットする / Set "oppose_cold", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_oppose_cold(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->oppose_cold && !do_dec)
		{
			if (creature_ptr->oppose_cold > v) return FALSE;
		}
		else if (!IS_OPPOSE_COLD())
		{
			msg_print(_("冷気への耐性がついた気がする！", "You feel resistant to cold!"));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->oppose_cold && !music_singing(p_ptr, MUSIC_RESIST) && !(creature_ptr->special_defense & KATA_MUSOU))
		{
			msg_print(_("冷気への耐性が薄れた気がする。", "You feel less resistant to cold."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->oppose_cold = v;

	/* Nothing to notice */
	if (!notice) return (FALSE);
	creature_ptr->redraw |= (PR_STATUS);

	if (disturb_state) disturb(FALSE, FALSE);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 一時的毒耐性の継続時間をセットする / Set "oppose_pois", notice observable changes
 * @param v 継続時間
 * @param do_dec 現在の継続時間より長い値のみ上書きする
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 */
bool set_oppose_pois(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if ((creature_ptr->pclass == CLASS_NINJA) && (creature_ptr->lev > 44)) v = 1;
	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->oppose_pois && !do_dec)
		{
			if (creature_ptr->oppose_pois > v) return FALSE;
		}
		else if (!IS_OPPOSE_POIS())
		{
			msg_print(_("毒への耐性がついた気がする！", "You feel resistant to poison!"));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->oppose_pois && !music_singing(p_ptr, MUSIC_RESIST) && !(creature_ptr->special_defense & KATA_MUSOU))
		{
			msg_print(_("毒への耐性が薄れた気がする。", "You feel less resistant to poison."));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->oppose_pois = v;

	/* Nothing to notice */
	if (!notice) return (FALSE);
	creature_ptr->redraw |= (PR_STATUS);

	if (disturb_state) disturb(FALSE, FALSE);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 朦朧の継続時間をセットする / Set "stun", notice observable changes
 * @param v 継続時間
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 * @details
 * Note the special code to only notice "range" changes.
 */
bool set_stun(player_type *creature_ptr, TIME_EFFECT v)
{
	int old_aux, new_aux;
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;
	if (PRACE_IS_(creature_ptr, RACE_GOLEM) || ((creature_ptr->pclass == CLASS_BERSERKER) && (creature_ptr->lev > 34))) v = 0;

	/* Knocked out */
	if (creature_ptr->stun > 100)
	{
		old_aux = 3;
	}

	/* Heavy stun */
	else if (creature_ptr->stun > 50)
	{
		old_aux = 2;
	}

	/* Stun */
	else if (creature_ptr->stun > 0)
	{
		old_aux = 1;
	}

	/* None */
	else
	{
		old_aux = 0;
	}

	/* Knocked out */
	if (v > 100)
	{
		new_aux = 3;
	}

	/* Heavy stun */
	else if (v > 50)
	{
		new_aux = 2;
	}

	/* Stun */
	else if (v > 0)
	{
		new_aux = 1;
	}

	/* None */
	else
	{
		new_aux = 0;
	}

	/* Increase cut */
	if (new_aux > old_aux)
	{
		/* Describe the state */
		switch (new_aux)
		{
			/* Stun */
			case 1: msg_print(_("意識がもうろうとしてきた。", "You have been stunned.")); break;

			/* Heavy stun */
			case 2: msg_print(_("意識がひどくもうろうとしてきた。", "You have been heavily stunned.")); break;

			/* Knocked out */
			case 3: msg_print(_("頭がクラクラして意識が遠のいてきた。", "You have been knocked out.")); break;
		}

		if (randint1(1000) < v || one_in_(16))
		{
			msg_print(_("割れるような頭痛がする。", "A vicious blow hits your head."));

			if (one_in_(3))
			{
				if (!creature_ptr->sustain_int) (void)do_dec_stat(creature_ptr, A_INT);
				if (!creature_ptr->sustain_wis) (void)do_dec_stat(creature_ptr, A_WIS);
			}
			else if (one_in_(2))
			{
				if (!creature_ptr->sustain_int) (void)do_dec_stat(creature_ptr, A_INT);
			}
			else
			{
				if (!creature_ptr->sustain_wis) (void)do_dec_stat(creature_ptr, A_WIS);
			}
		}
		if (creature_ptr->special_defense & KATA_MASK)
		{
			msg_print(_("型が崩れた。", "Your posture gets loose."));
			creature_ptr->special_defense &= ~(KATA_MASK);
			creature_ptr->update |= (PU_BONUS);
			creature_ptr->update |= (PU_MONSTERS);
			creature_ptr->redraw |= (PR_STATE);
			creature_ptr->redraw |= (PR_STATUS);
			creature_ptr->action = ACTION_NONE;
		}

		/* Sniper */
		if (creature_ptr->concent) reset_concentration(TRUE);

		/* Hex */
		if (hex_spelling_any(p_ptr)) stop_hex_spell_all();

		notice = TRUE;
	}

	/* Decrease cut */
	else if (new_aux < old_aux)
	{
		/* Describe the state */
		switch (new_aux)
		{
			/* None */
		case 0:
			msg_print(_("やっと朦朧状態から回復した。", "You are no longer stunned."));

			if (disturb_state) disturb(FALSE, FALSE);
			break;
		}

		notice = TRUE;
	}

	/* Use the value */
	creature_ptr->stun = v;

	/* No change */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);

	/* Redraw the "stun" */
	creature_ptr->redraw |= (PR_STUN);
	handle_stuff();
	return (TRUE);
}


/*!
 * @brief 出血の継続時間をセットする / Set "cut", notice observable changes
 * @param v 継続時間
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 * @details
 * Note the special code to only notice "range" changes.
 */
bool set_cut(player_type *creature_ptr, TIME_EFFECT v)
{
	int old_aux, new_aux;
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if ((creature_ptr->prace == RACE_GOLEM ||
	    creature_ptr->prace == RACE_SKELETON ||
	    creature_ptr->prace == RACE_SPECTRE ||
		(creature_ptr->prace == RACE_ZOMBIE && creature_ptr->lev > 11)) &&
	    !creature_ptr->mimic_form)
		v = 0;

	/* Mortal wound */
	if (creature_ptr->cut > 1000)
	{
		old_aux = 7;
	}

	/* Deep gash */
	else if (creature_ptr->cut > 200)
	{
		old_aux = 6;
	}

	/* Severe cut */
	else if (creature_ptr->cut > 100)
	{
		old_aux = 5;
	}

	/* Nasty cut */
	else if (creature_ptr->cut > 50)
	{
		old_aux = 4;
	}

	/* Bad cut */
	else if (creature_ptr->cut > 25)
	{
		old_aux = 3;
	}

	/* Light cut */
	else if (creature_ptr->cut > 10)
	{
		old_aux = 2;
	}

	/* Graze */
	else if (creature_ptr->cut > 0)
	{
		old_aux = 1;
	}

	/* None */
	else
	{
		old_aux = 0;
	}

	/* Mortal wound */
	if (v > 1000)
	{
		new_aux = 7;
	}

	/* Deep gash */
	else if (v > 200)
	{
		new_aux = 6;
	}

	/* Severe cut */
	else if (v > 100)
	{
		new_aux = 5;
	}

	/* Nasty cut */
	else if (v > 50)
	{
		new_aux = 4;
	}

	/* Bad cut */
	else if (v > 25)
	{
		new_aux = 3;
	}

	/* Light cut */
	else if (v > 10)
	{
		new_aux = 2;
	}

	/* Graze */
	else if (v > 0)
	{
		new_aux = 1;
	}

	/* None */
	else
	{
		new_aux = 0;
	}

	/* Increase cut */
	if (new_aux > old_aux)
	{
		/* Describe the state */
		switch (new_aux)
		{
			/* Graze */
			case 1: msg_print(_("かすり傷を負ってしまった。", "You have been given a graze.")); break;

			/* Light cut */
			case 2: msg_print(_("軽い傷を負ってしまった。", "You have been given a light cut.")); break;

			/* Bad cut */
			case 3: msg_print(_("ひどい傷を負ってしまった。", "You have been given a bad cut.")); break;

			/* Nasty cut */
			case 4: msg_print(_("大変な傷を負ってしまった。", "You have been given a nasty cut.")); break;

			/* Severe cut */
			case 5: msg_print(_("重大な傷を負ってしまった。", "You have been given a severe cut.")); break;

			/* Deep gash */
			case 6: msg_print(_("ひどい深手を負ってしまった。", "You have been given a deep gash.")); break;

			/* Mortal wound */
			case 7: msg_print(_("致命的な傷を負ってしまった。", "You have been given a mortal wound.")); break;
		}

		notice = TRUE;

		if (randint1(1000) < v || one_in_(16))
		{
			if (!creature_ptr->sustain_chr)
			{
				msg_print(_("ひどい傷跡が残ってしまった。", "You have been horribly scarred."));
				do_dec_stat(creature_ptr, A_CHR);
			}
		}
	}

	/* Decrease cut */
	else if (new_aux < old_aux)
	{
		/* Describe the state */
		switch (new_aux)
		{
			/* None */
			case 0:
			msg_format(_("やっと%s。", "You are no longer bleeding."), creature_ptr->prace == RACE_ANDROID ? "怪我が直った" : "出血が止まった");

			if (disturb_state) disturb(FALSE, FALSE);
			break;
		}

		notice = TRUE;
	}

	/* Use the value */
	creature_ptr->cut = v;

	/* No change */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);

	/* Redraw the "cut" */
	creature_ptr->redraw |= (PR_CUT);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief 空腹状態をセットする / Set "food", notice observable changes
 * @param v 継続時間
 * @return ステータスに影響を及ぼす変化があった場合TRUEを返す。
 * @details
 * Set "", notice observable changes\n
 *\n
 * The "food" variable can get as large as 20000, allowing the
 * addition of the most "filling" item, Elvish Waybread, which adds
 * 7500 food units, without overflowing the 32767 maximum limit.\n
 *\n
 * Perhaps we should disturb the player with various messages,
 * especially messages about hunger status changes.  \n
 *\n
 * Digestion of food is handled in "dungeon.c", in which, normally,
 * the player digests about 20 food units per 100 game turns, more
 * when "fast", more when "regenerating", less with "slow digestion",
 * but when the player is "gorged", he digests 100 food units per 10
 * game turns, or a full 1000 food units per 100 game turns.\n
 *\n
 * Note that the player's speed is reduced by 10 units while gorged,
 * so if the player eats a single food ration (5000 food units) when
 * full (15000 food units), he will be gorged for (5000/100)*10 = 500
 * game turns, or 500/(100/5) = 25 player turns (if nothing else is
 * affecting the player speed).\n
 */
bool set_food(player_type *creature_ptr, TIME_EFFECT v)
{
	int old_aux, new_aux;

	bool notice = FALSE;
	v = (v > 20000) ? 20000 : (v < 0) ? 0 : v;

	/* Fainting / Starving */
	if (creature_ptr->food < PY_FOOD_FAINT)
	{
		old_aux = 0;
	}

	/* Weak */
	else if (creature_ptr->food < PY_FOOD_WEAK)
	{
		old_aux = 1;
	}

	/* Hungry */
	else if (creature_ptr->food < PY_FOOD_ALERT)
	{
		old_aux = 2;
	}

	/* Normal */
	else if (creature_ptr->food < PY_FOOD_FULL)
	{
		old_aux = 3;
	}

	/* Full */
	else if (creature_ptr->food < PY_FOOD_MAX)
	{
		old_aux = 4;
	}

	/* Gorged */
	else
	{
		old_aux = 5;
	}

	/* Fainting / Starving */
	if (v < PY_FOOD_FAINT)
	{
		new_aux = 0;
	}

	/* Weak */
	else if (v < PY_FOOD_WEAK)
	{
		new_aux = 1;
	}

	/* Hungry */
	else if (v < PY_FOOD_ALERT)
	{
		new_aux = 2;
	}

	/* Normal */
	else if (v < PY_FOOD_FULL)
	{
		new_aux = 3;
	}

	/* Full */
	else if (v < PY_FOOD_MAX)
	{
		new_aux = 4;
	}

	/* Gorged */
	else
	{
		new_aux = 5;
	}

	if (old_aux < 1 && new_aux > 0)
		chg_virtue(creature_ptr, V_PATIENCE, 2);
	else if (old_aux < 3 && (old_aux != new_aux))
		chg_virtue(creature_ptr, V_PATIENCE, 1);
	if (old_aux == 2)
		chg_virtue(creature_ptr, V_TEMPERANCE, 1);
	if (old_aux == 0)
		chg_virtue(creature_ptr, V_TEMPERANCE, -1);

	/* Food increase */
	if (new_aux > old_aux)
	{
		/* Describe the state */
		switch (new_aux)
		{
			/* Weak */
			case 1: msg_print(_("まだ空腹で倒れそうだ。", "You are still weak.")); break;

			/* Hungry */
			case 2: msg_print(_("まだ空腹だ。", "You are still hungry.")); break;

			/* Normal */
			case 3: msg_print(_("空腹感がおさまった。", "You are no longer hungry.")); break;

			/* Full */
			case 4: msg_print(_("満腹だ！", "You are full!")); break;

			/* Bloated */
			case 5:
			msg_print(_("食べ過ぎだ！", "You have gorged yourself!"));
			chg_virtue(creature_ptr, V_HARMONY, -1);
			chg_virtue(creature_ptr, V_PATIENCE, -1);
			chg_virtue(creature_ptr, V_TEMPERANCE, -2);

			break;
		}

		/* Change */
		notice = TRUE;
	}

	/* Food decrease */
	else if (new_aux < old_aux)
	{
		/* Describe the state */
		switch (new_aux)
		{
			/* Fainting / Starving */
			case 0: msg_print(_("あまりにも空腹で気を失ってしまった！", "You are getting faint from hunger!")); break;

			/* Weak */
			case 1: msg_print(_("お腹が空いて倒れそうだ。", "You are getting weak from hunger!")); break;

			/* Hungry */
			case 2: msg_print(_("お腹が空いてきた。", "You are getting hungry.")); break;

			/* Normal */
			case 3: msg_print(_("満腹感がなくなった。", "You are no longer full.")); break;

			/* Full */
			case 4: msg_print(_("やっとお腹がきつくなくなった。", "You are no longer gorged.")); break;
		}

		if (creature_ptr->wild_mode && (new_aux < 2))
		{
			change_wild_mode(FALSE);
		}

		/* Change */
		notice = TRUE;
	}

	/* Use the value */
	creature_ptr->food = v;

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);

	/* Redraw hunger */
	creature_ptr->redraw |= (PR_HUNGER);
	handle_stuff();
	return (TRUE);
}

/*!
 * @brief プレイヤーの基本能力値を増加させる / Increases a stat by one randomized level -RAK-
 * @param stat 上昇させるステータスID
 * @return 実際に上昇した場合TRUEを返す。
 * @details
 * Note that this function (used by stat potions) now restores\n
 * the stat BEFORE increasing it.\n
 */
bool inc_stat(player_type *creature_ptr, int stat)
{
	BASE_STATUS value, gain;

	/* Then augment the current/max stat */
	value = creature_ptr->stat_cur[stat];

	/* Cannot go above 18/100 */
	if (value < creature_ptr->stat_max_max[stat])
	{
		/* Gain one (sometimes two) points */
		if (value < 18)
		{
			gain = ((randint0(100) < 75) ? 1 : 2);
			value += gain;
		}

		/* Gain 1/6 to 1/3 of distance to 18/100 */
		else if (value < (creature_ptr->stat_max_max[stat]-2))
		{
			/* Approximate gain value */
			gain = (((creature_ptr->stat_max_max[stat]) - value) / 2 + 3) / 2;
			if (gain < 1) gain = 1;

			/* Apply the bonus */
			value += randint1(gain) + gain / 2;

			/* Maximal value */
			if (value > (creature_ptr->stat_max_max[stat]-1)) value = creature_ptr->stat_max_max[stat]-1;
		}

		/* Gain one point at a time */
		else
		{
			value++;
		}

		/* Save the new value */
		creature_ptr->stat_cur[stat] = value;

		/* Bring up the maximum too */
		if (value > creature_ptr->stat_max[stat])
		{
			creature_ptr->stat_max[stat] = value;
		}
		creature_ptr->update |= (PU_BONUS);

		/* Success */
		return (TRUE);
	}

	/* Nothing to gain */
	return (FALSE);
}

/*!
 * @brief プレイヤーの基本能力値を減少させる / Decreases a stat by an amount indended to vary from 0 to 100 percent.
 * @param stat 減少させるステータスID
 * @param amount 減少させる基本量
 * @param permanent TRUEならば現在の最大値を減少させる
 * @return 実際に減少した場合TRUEを返す。
 * @details
 *\n
 * Amount could be a little higher in extreme cases to mangle very high\n
 * stats from massive assaults.  -CWS\n
 *\n
 * Note that "permanent" means that the *given* amount is permanent,\n
 * not that the new value becomes permanent.  This may not work exactly\n
 * as expected, due to "weirdness" in the algorithm, but in general,\n
 * if your stat is already drained, the "max" value will not drop all\n
 * the way down to the "cur" value.\n
 */
bool dec_stat(player_type *creature_ptr, int stat, int amount, int permanent)
{
	BASE_STATUS cur, max;
	int loss, same;
	bool res = FALSE;

	/* Acquire current value */
	cur = creature_ptr->stat_cur[stat];
	max = creature_ptr->stat_max[stat];

	/* Note when the values are identical */
	same = (cur == max);

	/* Damage "current" value */
	if (cur > 3)
	{
		/* Handle "low" values */
		if (cur <= 18)
		{
			if (amount > 90) cur--;
			if (amount > 50) cur--;
			if (amount > 20) cur--;
			cur--;
		}

		/* Handle "high" values */
		else
		{
			/* Hack -- Decrement by a random amount between one-quarter */
			/* and one-half of the stat bonus times the percentage, with a */
			/* minimum damage of half the percentage. -CWS */
			loss = (((cur-18) / 2 + 1) / 2 + 1);
			if (loss < 1) loss = 1;

			/* Randomize the loss */
			loss = ((randint1(loss) + loss) * amount) / 100;

			/* Maximal loss */
			if (loss < amount/2) loss = amount/2;

			/* Lose some points */
			cur = cur - loss;

			/* Hack -- Only reduce stat to 17 sometimes */
			if (cur < 18) cur = (amount <= 20) ? 18 : 17;
		}

		/* Prevent illegal values */
		if (cur < 3) cur = 3;

		/* Something happened */
		if (cur != creature_ptr->stat_cur[stat]) res = TRUE;
	}

	/* Damage "max" value */
	if (permanent && (max > 3))
	{
		chg_virtue(creature_ptr, V_SACRIFICE, 1);
		if (stat == A_WIS || stat == A_INT)
			chg_virtue(creature_ptr, V_ENLIGHTEN, -2);

		/* Handle "low" values */
		if (max <= 18)
		{
			if (amount > 90) max--;
			if (amount > 50) max--;
			if (amount > 20) max--;
			max--;
		}

		/* Handle "high" values */
		else
		{
			/* Hack -- Decrement by a random amount between one-quarter */
			/* and one-half of the stat bonus times the percentage, with a */
			/* minimum damage of half the percentage. -CWS */
			loss = (((max-18) / 2 + 1) / 2 + 1);
			loss = ((randint1(loss) + loss) * amount) / 100;
			if (loss < amount/2) loss = amount/2;

			/* Lose some points */
			max = max - loss;

			/* Hack -- Only reduce stat to 17 sometimes */
			if (max < 18) max = (amount <= 20) ? 18 : 17;
		}

		/* Hack -- keep it clean */
		if (same || (max < cur)) max = cur;

		/* Something happened */
		if (max != creature_ptr->stat_max[stat]) res = TRUE;
	}

	if (res)
	{
		/* Actually set the stat to its new value. */
		creature_ptr->stat_cur[stat] = cur;
		creature_ptr->stat_max[stat] = max;

		creature_ptr->redraw |= (PR_STATS);
		creature_ptr->update |= (PU_BONUS);
	}

	return (res);
}


/*!
 * @brief プレイヤーの基本能力値を回復させる / Restore a stat.  Return TRUE only if this actually makes a difference.
 * @param stat 回復ステータスID
 * @return 実際に回復した場合TRUEを返す。
 */
bool res_stat(player_type *creature_ptr, int stat)
{
	/* Restore if needed */
	if (creature_ptr->stat_cur[stat] != creature_ptr->stat_max[stat])
	{
		creature_ptr->stat_cur[stat] = creature_ptr->stat_max[stat];
		creature_ptr->update |= (PU_BONUS);
		creature_ptr->redraw |= (PR_STATS);

		/* Success */
		return (TRUE);
	}

	/* Nothing to restore */
	return (FALSE);
}


/*
 * Increase players hit points, notice effects
 */
bool hp_player(player_type *creature_ptr, int num)
{
	int vir;
	vir = virtue_number(creature_ptr, V_VITALITY);

	if(num <= 0) return (FALSE);

	if(vir)
	{
		num = num * (creature_ptr->virtues[vir - 1] + 1250) / 1250;
	}
	/* Healing needed */
	if (creature_ptr->chp < creature_ptr->mhp)
	{
		if ((num > 0) && (creature_ptr->chp < (creature_ptr->mhp/3)))
			chg_virtue(creature_ptr, V_TEMPERANCE, 1);
		/* Gain hitpoints */
		creature_ptr->chp += num;

		/* Enforce maximum */
		if (creature_ptr->chp >= creature_ptr->mhp)
		{
			creature_ptr->chp = creature_ptr->mhp;
			creature_ptr->chp_frac = 0;
		}

		creature_ptr->redraw |= (PR_HP);

		creature_ptr->window |= (PW_PLAYER);

		/* Heal 0-4 */
		if (num < 5)
		{
			msg_print(_("少し気分が良くなった。", "You feel a little better."));
		}

		/* Heal 5-14 */
		else if (num < 15)
		{
			msg_print(_("気分が良くなった。", "You feel better."));
		}

		/* Heal 15-34 */
		else if (num < 35)
		{
			msg_print(_("とても気分が良くなった。", "You feel much better."));
		}

		/* Heal 35+ */
		else
		{
			msg_print(_("ひじょうに気分が良くなった。", "You feel very good."));
		}

		return (TRUE);
	}

	/* Ignore */
	return (FALSE);
}


/*
 * Array of stat "descriptions"
 */
static concptr desc_stat_pos[] =
{
	_("強く", "strong"),
	_("知的に", "smart"),
	_("賢く", "wise"),
	_("器用に", "dextrous"),
	_("健康に", "healthy"),
	_("美しく", "cute")
};


/*
 * Array of stat "descriptions"
 */
static concptr desc_stat_neg[] =
{
	_("弱く", "weak"),
	_("無知に", "stupid"),
	_("愚かに", "naive"),
	_("不器用に", "clumsy"),
	_("不健康に", "sickly"),
	_("醜く", "ugly")
};


/*
 * Lose a "point"
 */
bool do_dec_stat(player_type *creature_ptr, int stat)
{
	bool sust = FALSE;

	/* Access the "sustain" */
	switch (stat)
	{
		case A_STR: if (creature_ptr->sustain_str) sust = TRUE; break;
		case A_INT: if (creature_ptr->sustain_int) sust = TRUE; break;
		case A_WIS: if (creature_ptr->sustain_wis) sust = TRUE; break;
		case A_DEX: if (creature_ptr->sustain_dex) sust = TRUE; break;
		case A_CON: if (creature_ptr->sustain_con) sust = TRUE; break;
		case A_CHR: if (creature_ptr->sustain_chr) sust = TRUE; break;
	}

	/* Sustain */
	if (sust && (!ironman_nightmare || randint0(13)))
	{
		msg_format(_("%sなった気がしたが、すぐに元に戻った。", "You feel %s for a moment, but the feeling passes."),
					desc_stat_neg[stat]);

		/* Notice effect */
		return (TRUE);
	}

	/* Attempt to reduce the stat */
	if (dec_stat(creature_ptr, stat, 10, (ironman_nightmare && !randint0(13))))
	{
		msg_format(_("ひどく%sなった気がする。", "You feel very %s."), desc_stat_neg[stat]);

		/* Notice effect */
		return (TRUE);
	}

	/* Nothing obvious */
	return (FALSE);
}


/*
 * Restore lost "points" in a stat
 */
bool do_res_stat(player_type *creature_ptr, int stat)
{
	/* Attempt to increase */
	if (res_stat(creature_ptr, stat))
	{
		msg_format(_("元通りに%sなった気がする。", "You feel less %s."), desc_stat_pos[stat]);
		return (TRUE);
	}

	/* Nothing obvious */
	return (FALSE);
}


/*
 * Gain a "point" in a stat
 */
bool do_inc_stat(player_type *creature_ptr, int stat)
{
	bool res;

	/* Restore strength */
	res = res_stat(creature_ptr, stat);

	/* Attempt to increase */
	if (inc_stat(creature_ptr, stat))
	{
		if (stat == A_WIS)
		{
			chg_virtue(creature_ptr, V_ENLIGHTEN, 1);
			chg_virtue(creature_ptr, V_FAITH, 1);
		}
		else if (stat == A_INT)
		{
			chg_virtue(creature_ptr, V_KNOWLEDGE, 1);
			chg_virtue(creature_ptr, V_ENLIGHTEN, 1);
		}
		else if (stat == A_CON)
			chg_virtue(creature_ptr, V_VITALITY, 1);

		msg_format(_("ワーオ！とても%sなった！", "Wow!  You feel very %s!"), desc_stat_pos[stat]);

		return (TRUE);
	}

	/* Restoration worked */
	if (res)
	{
		msg_format(_("元通りに%sなった気がする。", "You feel less %s."), desc_stat_pos[stat]);

		return (TRUE);
	}

	/* Nothing obvious */
	return (FALSE);
}


/*
 * Restores any drained experience
 */
bool restore_level(player_type *creature_ptr)
{
	/* Restore experience */
	if (creature_ptr->exp < creature_ptr->max_exp)
	{
		msg_print(_("経験値が戻ってきた気がする。", "You feel your experience returning."));

		/* Restore the experience */
		creature_ptr->exp = creature_ptr->max_exp;

		/* Check the experience */
		check_experience(creature_ptr);

		/* Did something */
		return (TRUE);
	}

	/* No effect */
	return (FALSE);
}

/*
 * Forget everything
 */
bool lose_all_info(player_type *creature_ptr)
{
	int i;

	chg_virtue(creature_ptr, V_KNOWLEDGE, -5);
	chg_virtue(creature_ptr, V_ENLIGHTEN, -5);

	/* Forget info about objects */
	for (i = 0; i < INVEN_TOTAL; i++)
	{
		object_type *o_ptr = &creature_ptr->inventory_list[i];
		if (!o_ptr->k_idx) continue;

		/* Allow "protection" by the MENTAL flag */
		if (o_ptr->ident & (IDENT_MENTAL)) continue;

		/* Remove "default inscriptions" */
		o_ptr->feeling = FEEL_NONE;

		/* Hack -- Clear the "empty" flag */
		o_ptr->ident &= ~(IDENT_EMPTY);

		/* Hack -- Clear the "known" flag */
		o_ptr->ident &= ~(IDENT_KNOWN);

		/* Hack -- Clear the "felt" flag */
		o_ptr->ident &= ~(IDENT_SENSE);
	}
	creature_ptr->update |= (PU_BONUS);
	creature_ptr->update |= (PU_COMBINE | PU_REORDER);

	creature_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER);

	/* Mega-Hack -- Forget the map */
	wiz_dark();

	/* It worked */
	return (TRUE);
}


void do_poly_wounds(player_type *creature_ptr)
{
	/* Changed to always provide at least _some_ healing */
	s16b wounds = creature_ptr->cut;
	s16b hit_p = (creature_ptr->mhp - creature_ptr->chp);
	s16b change = damroll(creature_ptr->lev, 5);
	bool Nasty_effect = one_in_(5);

	if (!(wounds || hit_p || Nasty_effect)) return;

	msg_print(_("傷がより軽いものに変化した。", "Your wounds are polymorphed into less serious ones."));
	hp_player(creature_ptr, change);
	if (Nasty_effect)
	{
		msg_print(_("新たな傷ができた！", "A new wound was created!"));
		take_hit(creature_ptr, DAMAGE_LOSELIFE, change / 2, _("変化した傷", "a polymorphed wound"), -1);
		set_cut(creature_ptr,change);
	}
	else
	{
		set_cut(creature_ptr,creature_ptr->cut - (change / 2));
	}
}


/*
 * Change player race
 */
void change_race(player_type *creature_ptr, CHARACTER_IDX new_race, concptr effect_msg)
{
	concptr title = race_info[new_race].title;
	int  old_race = creature_ptr->prace;

#ifdef JP
	msg_format("あなたは%s%sに変化した！", effect_msg, title);
#else
	msg_format("You current_world_ptr->game_turn into %s %s%s!", (!effect_msg[0] && is_a_vowel(title[0]) ? "an" : "a"), effect_msg, title);
#endif

	chg_virtue(creature_ptr, V_CHANCE, 2);

	if (creature_ptr->prace < 32)
	{
		creature_ptr->old_race1 |= 1L << creature_ptr->prace;
	}
	else
	{
		creature_ptr->old_race2 |= 1L << (creature_ptr->prace - 32);
	}
	creature_ptr->prace = new_race;
	rp_ptr = &race_info[creature_ptr->prace];

	/* Experience factor */
	creature_ptr->expfact = rp_ptr->r_exp + cp_ptr->c_exp;

	/*
	 * The speed bonus of Klackons and Sprites are disabled
	 * and the experience penalty is decreased.
	 */
	if (((creature_ptr->pclass == CLASS_MONK) || (creature_ptr->pclass == CLASS_FORCETRAINER) || (creature_ptr->pclass == CLASS_NINJA)) && ((creature_ptr->prace == RACE_KLACKON) || (creature_ptr->prace == RACE_SPRITE)))
		creature_ptr->expfact -= 15;

	/* Get character's height and weight */
	get_height_weight(creature_ptr);

	/* Hitdice */
	if (creature_ptr->pclass == CLASS_SORCERER)
		creature_ptr->hitdie = rp_ptr->r_mhp/2 + cp_ptr->c_mhp + ap_ptr->a_mhp;
	else
		creature_ptr->hitdie = rp_ptr->r_mhp + cp_ptr->c_mhp + ap_ptr->a_mhp;

	roll_hitdice(creature_ptr, 0L);

	/* The experience level may be modified */
	check_experience(creature_ptr);

	creature_ptr->redraw |= (PR_BASIC);

	creature_ptr->update |= (PU_BONUS);

	handle_stuff();

	/* Load an autopick preference file */
	if (old_race != creature_ptr->prace) autopick_load_pref(FALSE);

	/* Player's graphic tile may change */
	lite_spot(creature_ptr->y, creature_ptr->x);
}


void do_poly_self(player_type *creature_ptr)
{
	int power = creature_ptr->lev;

	msg_print(_("あなたは変化の訪れを感じた...", "You feel a change coming over you..."));
	chg_virtue(creature_ptr, V_CHANCE, 1);

	if ((power > randint0(20)) && one_in_(3) && (creature_ptr->prace != RACE_ANDROID))
	{
		char effect_msg[80] = "";
		CHARACTER_IDX new_race;

		/* Some form of racial polymorph... */
		power -= 10;

		if ((power > randint0(5)) && one_in_(4))
		{
			/* sex change */
			power -= 2;

			if (creature_ptr->psex == SEX_MALE)
			{
				creature_ptr->psex = SEX_FEMALE;
				sp_ptr = &sex_info[creature_ptr->psex];
				sprintf(effect_msg, _("女性の", "female "));
			}
			else
			{
				creature_ptr->psex = SEX_MALE;
				sp_ptr = &sex_info[creature_ptr->psex];
				sprintf(effect_msg, _("男性の", "male "));
			}
		}

		if ((power > randint0(30)) && one_in_(5))
		{
			int tmp = 0;

			/* Harmful deformity */
			power -= 15;

			while (tmp < A_MAX)
			{
				if (one_in_(2))
				{
					(void)dec_stat(creature_ptr, tmp, randint1(6) + 6, one_in_(3));
					power -= 1;
				}
				tmp++;
			}

			/* Deformities are discriminated against! */
			(void)dec_stat(creature_ptr, A_CHR, randint1(6), TRUE);

			if (effect_msg[0])
			{
				char tmp_msg[10];
				sprintf(tmp_msg,_("%s", "%s "),effect_msg);
				sprintf(effect_msg,_("奇形の%s", "deformed %s "),tmp_msg);
			}
			else
			{
				sprintf(effect_msg,_("奇形の", "deformed "));
			}
		}

		while ((power > randint0(20)) && one_in_(10))
		{
			/* Polymorph into a less mutated form */
			power -= 10;

			if (!lose_mutation(creature_ptr, 0))
			msg_print(_("奇妙なくらい普通になった気がする。", "You feel oddly normal."));
		}

		do
		{
			new_race = (CHARACTER_IDX)randint0(MAX_RACES);
		}
		while ((new_race == creature_ptr->prace) || (new_race == RACE_ANDROID));

		change_race(creature_ptr, new_race, effect_msg);
	}

	if ((power > randint0(30)) && one_in_(6))
	{
		int tmp = 0;

		/* Abomination! */
		power -= 20;
		msg_format(_("%sの構成が変化した！", "Your internal organs are rearranged!"), creature_ptr->prace == RACE_ANDROID ? "機械" : "内臓");

		while (tmp < A_MAX)
		{
			(void)dec_stat(creature_ptr, tmp, randint1(6) + 6, one_in_(3));
			tmp++;
		}
		if (one_in_(6))
		{
			msg_print(_("現在の姿で生きていくのは困難なようだ！", "You find living difficult in your present form!"));
			take_hit(creature_ptr, DAMAGE_LOSELIFE, damroll(randint1(10), creature_ptr->lev), _("致命的な突然変異", "a lethal mutation"), -1);

			power -= 10;
		}
	}

	if ((power > randint0(20)) && one_in_(4))
	{
		power -= 10;

		get_max_stats(creature_ptr);
		roll_hitdice(creature_ptr, 0L);
	}

	while ((power > randint0(15)) && one_in_(3))
	{
		power -= 7;
		(void)gain_mutation(creature_ptr, 0);
	}

	if (power > randint0(5))
	{
		power -= 5;
		do_poly_wounds(creature_ptr);
	}

	/* Note: earlier deductions may have left power < 0 already. */
	while (power > 0)
	{
		status_shuffle();
		power--;
	}
}

/*
 * Gain experience
 */
void gain_exp_64(player_type *creature_ptr, s32b amount, u32b amount_frac)
{
	if (creature_ptr->is_dead) return;

	if (creature_ptr->prace == RACE_ANDROID) return;

	/* Gain some experience */
	s64b_add(&(creature_ptr->exp), &(creature_ptr->exp_frac), amount, amount_frac);

	/* Slowly recover from experience drainage */
	if (creature_ptr->exp < creature_ptr->max_exp)
	{
		/* Gain max experience (20%) (was 10%) */
		creature_ptr->max_exp += amount / 5;
	}

	check_experience(creature_ptr);
}


/*
 * Gain experience
 */
void gain_exp(player_type *creature_ptr, s32b amount)
{
	gain_exp_64(creature_ptr, amount, 0L);
}


void calc_android_exp(player_type *creature_ptr)
{
	int i;
	u32b total_exp = 0;
	if (creature_ptr->is_dead) return;

	if (creature_ptr->prace != RACE_ANDROID) return;

	for (i = INVEN_RARM; i < INVEN_TOTAL; i++)
	{
		object_type *o_ptr = &creature_ptr->inventory_list[i];
		object_type forge;
		object_type *q_ptr = &forge;
		u32b value, exp;
		DEPTH level = MAX(k_info[o_ptr->k_idx].level - 8, 1);

		if ((i == INVEN_RIGHT) || (i == INVEN_LEFT) || (i == INVEN_NECK) || (i == INVEN_LITE)) continue;
		if (!o_ptr->k_idx) continue;
		object_wipe(q_ptr);

		object_copy(q_ptr, o_ptr);
		q_ptr->discount = 0;
		q_ptr->curse_flags = 0L;

		if (object_is_fixed_artifact(o_ptr))
		{
			level = (level + MAX(a_info[o_ptr->name1].level - 8, 5)) / 2;
			level += MIN(20, a_info[o_ptr->name1].rarity/(a_info[o_ptr->name1].gen_flags & TRG_INSTA_ART ? 10 : 3));
		}
		else if (object_is_ego(o_ptr))
		{
			level += MAX(3, (e_info[o_ptr->name2].rating - 5)/2);
		}
		else if (o_ptr->art_name)
		{
			s32b total_flags = flag_cost(o_ptr, o_ptr->pval);
			int fake_level;

			if (!object_is_weapon_ammo(o_ptr))
			{
				/* For armors */
				if (total_flags < 15000) fake_level = 10;
				else if (total_flags < 35000) fake_level = 25;
				else fake_level = 40;
			}
			else
			{
				/* For weapons */
				if (total_flags < 20000) fake_level = 10;
				else if (total_flags < 45000) fake_level = 25;
				else fake_level = 40;
			}

			level = MAX(level, (level + MAX(fake_level - 8, 5)) / 2 + 3);
		}

		value = object_value_real(q_ptr);

		if (value <= 0) continue;
		if ((o_ptr->tval == TV_SOFT_ARMOR) && (o_ptr->sval == SV_ABUNAI_MIZUGI) && (creature_ptr->pseikaku != SEIKAKU_SEXY)) value /= 32;
		if (value > 5000000L) value = 5000000L;
		if ((o_ptr->tval == TV_DRAG_ARMOR) || (o_ptr->tval == TV_CARD)) level /= 2;

		if (object_is_artifact(o_ptr) || object_is_ego(o_ptr) ||
		    (o_ptr->tval == TV_DRAG_ARMOR) ||
		    ((o_ptr->tval == TV_HELM) && (o_ptr->sval == SV_DRAGON_HELM)) ||
		    ((o_ptr->tval == TV_SHIELD) && (o_ptr->sval == SV_DRAGON_SHIELD)) ||
		    ((o_ptr->tval == TV_GLOVES) && (o_ptr->sval == SV_SET_OF_DRAGON_GLOVES)) ||
		    ((o_ptr->tval == TV_BOOTS) && (o_ptr->sval == SV_PAIR_OF_DRAGON_GREAVE)) ||
		    ((o_ptr->tval == TV_SWORD) && (o_ptr->sval == SV_DIAMOND_EDGE)))
		{
			if (level > 65) level = 35 + (level - 65) / 5;
			else if (level > 35) level = 25 + (level - 35) / 3;
			else if (level > 15) level = 15 + (level - 15) / 2;
			exp = MIN(100000L, value) / 2 * level * level;
			if (value > 100000L)
				exp += (value - 100000L) / 8 * level * level;
		}
		else
		{
			exp = MIN(100000L, value) * level;
			if (value > 100000L)
				exp += (value - 100000L) / 4  * level;
		}
		if ((((i == INVEN_RARM) || (i == INVEN_LARM)) && (has_melee_weapon(creature_ptr, i))) || (i == INVEN_BOW)) total_exp += exp / 48;
		else total_exp += exp / 16;
		if (i == INVEN_BODY) total_exp += exp / 32;
	}
	creature_ptr->exp = creature_ptr->max_exp = total_exp;
	check_experience(creature_ptr);
}


/*
 * Lose experience
 */
void lose_exp(player_type *creature_ptr, s32b amount)
{
	if (creature_ptr->prace == RACE_ANDROID) return;
	if (amount > creature_ptr->exp) amount = creature_ptr->exp;

	creature_ptr->exp -= amount;

	check_experience(creature_ptr);
}


/*
 * Drain experience
 * If resisted to draining, return FALSE
 */
bool drain_exp(player_type *creature_ptr, s32b drain, s32b slip, int hold_exp_prob)
{
	/* Androids and their mimics are never drained */
	if (creature_ptr->prace == RACE_ANDROID) return FALSE;

	if (creature_ptr->hold_exp && (randint0(100) < hold_exp_prob))
	{
		/* Hold experience */
		msg_print(_("しかし自己の経験値を守りきった！", "You keep hold of your experience!"));
		return FALSE;
	}

	/* Hold experience failed */
	if (creature_ptr->hold_exp)
	{
		msg_print(_("経験値を少し吸い取られた気がする！", "You feel your experience slipping away!"));
		lose_exp(creature_ptr, slip);
	}
	else
	{
		msg_print(_("経験値が体から吸い取られた気がする！", "You feel your experience draining away!"));
		lose_exp(creature_ptr, drain);
	}

	return TRUE;
}


bool set_ultimate_res(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->ult_res && !do_dec)
		{
			if (creature_ptr->ult_res > v) return FALSE;
		}
		else if (!creature_ptr->ult_res)
		{
			msg_print(_("あらゆることに対して耐性がついた気がする！", "You feel resistant!"));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->ult_res)
		{
			msg_print(_("あらゆることに対する耐性が薄れた気がする。", "You feel less resistant"));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->ult_res = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);
	handle_stuff();
	return (TRUE);
}

bool set_tim_res_nether(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->tim_res_nether && !do_dec)
		{
			if (creature_ptr->tim_res_nether > v) return FALSE;
		}
		else if (!creature_ptr->tim_res_nether)
		{
			msg_print(_("地獄の力に対して耐性がついた気がする！", "You feel nether resistant!"));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->tim_res_nether)
		{
			msg_print(_("地獄の力に対する耐性が薄れた気がする。", "You feel less nether resistant"));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->tim_res_nether = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);
	handle_stuff();
	return (TRUE);
}

bool set_tim_res_time(player_type *creature_ptr, TIME_EFFECT v, bool do_dec)
{
	bool notice = FALSE;
	v = (v > 10000) ? 10000 : (v < 0) ? 0 : v;

	if (creature_ptr->is_dead) return FALSE;

	if (v)
	{
		if (creature_ptr->tim_res_time && !do_dec)
		{
			if (creature_ptr->tim_res_time > v) return FALSE;
		}
		else if (!creature_ptr->tim_res_time)
		{
			msg_print(_("時間逆転の力に対して耐性がついた気がする！", "You feel time resistant!"));
			notice = TRUE;
		}
	}

	else
	{
		if (creature_ptr->tim_res_time)
		{
			msg_print(_("時間逆転の力に対する耐性が薄れた気がする。", "You feel less time resistant"));
			notice = TRUE;
		}
	}

	/* Use the value */
	creature_ptr->tim_res_time = v;
	creature_ptr->redraw |= (PR_STATUS);

	/* Nothing to notice */
	if (!notice) return (FALSE);

	if (disturb_state) disturb(FALSE, FALSE);
	creature_ptr->update |= (PU_BONUS);
	handle_stuff();
	return (TRUE);
}


/*
 * Choose a warrior-mage elemental attack. -LM-
 */
bool choose_ele_attack(player_type *creature_ptr)
{
	int num;

	char choice;

	if (!has_melee_weapon(creature_ptr, INVEN_RARM) && !has_melee_weapon(creature_ptr, INVEN_LARM))
	{
		msg_format(_("武器を持たないと魔法剣は使えない。", "You cannot use temporary branding with no weapon."));
		return FALSE;
	}
	screen_save();

	num = (creature_ptr->lev - 20) / 5;
	c_prt(TERM_RED,    _("        a) 焼棄", "        a) Fire Brand"), 2, 14);

	if (num >= 2) 
		c_prt(TERM_L_WHITE,_("        b) 凍結", "        b) Cold Brand"), 3, 14);
	else 
		prt("", 3, 14);
	
	if (num >= 3) 
		c_prt(TERM_GREEN,  _("        c) 毒殺", "        c) Poison Brand"), 4, 14);
	else 
		prt("", 4, 14);

	if (num >= 4) 
		c_prt(TERM_L_DARK, _("        d) 溶解", "        d) Acid Brand"), 5, 14);
	else 
		prt("", 5, 14);

	if (num >= 5) 
		c_prt(TERM_BLUE,   _("        e) 電撃", "        e) Elec Brand"), 6, 14);
	else 
		prt("", 6, 14);

	prt("", 7, 14);
	prt("", 8, 14);
	prt("", 9, 14);

	prt("", 1, 0);
	prt(_("        どの元素攻撃をしますか？", "        Choose a temporary elemental brand "), 1, 14);

	choice = inkey();

	if ((choice == 'a') || (choice == 'A')) 
		set_ele_attack(creature_ptr, ATTACK_FIRE, creature_ptr->lev/2 + randint1(creature_ptr->lev/2));
	else if (((choice == 'b') || (choice == 'B')) && (num >= 2))
		set_ele_attack(creature_ptr, ATTACK_COLD, creature_ptr->lev/2 + randint1(creature_ptr->lev/2));
	else if (((choice == 'c') || (choice == 'C')) && (num >= 3))
		set_ele_attack(creature_ptr, ATTACK_POIS, creature_ptr->lev/2 + randint1(creature_ptr->lev/2));
	else if (((choice == 'd') || (choice == 'D')) && (num >= 4))
		set_ele_attack(creature_ptr, ATTACK_ACID, creature_ptr->lev/2 + randint1(creature_ptr->lev/2));
	else if (((choice == 'e') || (choice == 'E')) && (num >= 5))
		set_ele_attack(creature_ptr, ATTACK_ELEC, creature_ptr->lev/2 + randint1(creature_ptr->lev/2));
	else
	{
		msg_print(_("魔法剣を使うのをやめた。", "You cancel the temporary branding."));
		screen_load();
		return FALSE;
	}
	screen_load();
	return TRUE;
}


/*
 * Choose a elemental immune. -LM-
 */
bool choose_ele_immune(player_type *creature_ptr, TIME_EFFECT immune_turn)
{
	char choice;
	screen_save();

	c_prt(TERM_RED,    _("        a) 火炎", "        a) Immune Fire"), 2, 14);
	c_prt(TERM_L_WHITE,_("        b) 冷気", "        b) Immune Cold"), 3, 14);
	c_prt(TERM_L_DARK, _("        c) 酸", "        c) Immune Acid"), 4, 14);
	c_prt(TERM_BLUE,   _("        d) 電撃", "        d) Immune Elec"), 5, 14);

	prt("", 6, 14);
	prt("", 7, 14);
	prt("", 8, 14);
	prt("", 9, 14);

	prt("", 1, 0);
	prt(_("        どの元素の免疫をつけますか？", "        Choose a temporary elemental immune "), 1, 14);

	choice = inkey();

	if ((choice == 'a') || (choice == 'A')) 
		set_ele_immune(creature_ptr, DEFENSE_FIRE, immune_turn);
	else if ((choice == 'b') || (choice == 'B'))
		set_ele_immune(creature_ptr, DEFENSE_COLD, immune_turn);
	else if ((choice == 'c') || (choice == 'C'))
		set_ele_immune(creature_ptr, DEFENSE_ACID, immune_turn);
	else if ((choice == 'd') || (choice == 'D'))
		set_ele_immune(creature_ptr, DEFENSE_ELEC, immune_turn);
	else
	{
		msg_print(_("免疫を付けるのをやめた。", "You cancel the temporary immune."));
		screen_load();
		return FALSE;
	}
	screen_load();
	return TRUE;
}


﻿/*!
 * @brief プレーヤーからモンスターへの打撃処理
 * @date 2020/05/22
 * @author Hourier
 */

#include "system/angband.h"
#include "combat/player-attack.h"
#include "player/player-skill.h"
#include "player/avatar.h"
#include "object/artifact.h"
#include "main/sound-definitions-table.h"
#include "player/player-damage.h"
#include "realm/realm-hex.h"
#include "combat/martial-arts-table.h"
#include "world/world.h"
#include "object/object-flavor.h"
#include "object/object-hook.h"
#include "monster/monster-status.h"
#include "combat/attack-accuracy.h"
#include "combat/attack-criticality.h"
#include "monster/monsterrace-hook.h"
#include "combat/slaying.h"
#include "io/files-util.h"
#include "player/player-effects.h"
#include "spell/spells3.h"
#include "spell/spells-floor.h"
#include "combat/player-attack-util.h"
#include "mind/racial-samurai.h"

static player_attack_type *initialize_player_attack_type(player_attack_type *pa_ptr, s16b hand, combat_options mode, monster_type *m_ptr)
{
    pa_ptr->hand = hand;
    pa_ptr->mode = mode;
    pa_ptr->m_ptr = m_ptr;
    pa_ptr->backstab = FALSE;
    pa_ptr->suprise_attack = FALSE;
    pa_ptr->stab_fleeing = FALSE;
    pa_ptr->monk_attack = FALSE;
    pa_ptr->num_blow = 0;
    pa_ptr->attack_damage = 0;
    pa_ptr->can_drain = FALSE;
    pa_ptr->ma_ptr = &ma_blows[0];
    return pa_ptr;
}

/*!
 * @brief 盗賊と忍者における不意打ち
 * @param attacker_ptr プレーヤーへの参照ポインタ
 * @param pa_ptr 直接攻撃構造体への参照ポインタ
 * @return なし
 */
static void process_suprise_attack(player_type *attacker_ptr, player_attack_type *pa_ptr)
{
    monster_race *r_ptr = &r_info[pa_ptr->m_ptr->r_idx];
    if (!has_melee_weapon(attacker_ptr, INVEN_RARM + pa_ptr->hand) || attacker_ptr->icky_wield[pa_ptr->hand])
        return;

    int tmp = attacker_ptr->lev * 6 + (attacker_ptr->skill_stl + 10) * 4;
    if (attacker_ptr->monlite && (pa_ptr->mode != HISSATSU_NYUSIN))
        tmp /= 3;
    if (attacker_ptr->cursed & TRC_AGGRAVATE)
        tmp /= 2;
    if (r_ptr->level > (attacker_ptr->lev * attacker_ptr->lev / 20 + 10))
        tmp /= 3;
    if (MON_CSLEEP(pa_ptr->m_ptr) && pa_ptr->m_ptr->ml) {
        /* Can't backstab creatures that we can't see, right? */
        pa_ptr->backstab = TRUE;
    } else if ((attacker_ptr->special_defense & NINJA_S_STEALTH) && (randint0(tmp) > (r_ptr->level + 20)) && pa_ptr->m_ptr->ml && !(r_ptr->flagsr & RFR_RES_ALL)) {
        pa_ptr->suprise_attack = TRUE;
    } else if (MON_MONFEAR(pa_ptr->m_ptr) && pa_ptr->m_ptr->ml) {
        pa_ptr->stab_fleeing = TRUE;
    }
}

/*!
 * @brief 一部職業で攻撃に倍率がかかったりすることの処理
 * @param attacker_ptr プレーヤーへの参照ポインタ
 * @param pa_ptr 直接攻撃構造体への参照ポインタ
 * @return なし
 */
static void attack_classify(player_type *attacker_ptr, player_attack_type *pa_ptr)
{
    switch (attacker_ptr->pclass) {
    case CLASS_ROGUE:
    case CLASS_NINJA:
        process_suprise_attack(attacker_ptr, pa_ptr);
        return;
    case CLASS_MONK:
    case CLASS_FORCETRAINER:
    case CLASS_BERSERKER:
        if ((empty_hands(attacker_ptr, TRUE) & EMPTY_HAND_RARM) && !attacker_ptr->riding)
            pa_ptr->monk_attack = TRUE;
        return;
    default:
        return;
    }
}

/*!
 * @brief マーシャルアーツの技能値を増加させる
 * @param attacker_ptr プレーヤーへの参照ポインタ
 * @param pa_ptr 直接攻撃構造体への参照ポインタ
 * @return なし
 */
static void get_bare_knuckle_exp(player_type *attacker_ptr, player_attack_type *pa_ptr)
{
    monster_race *r_ptr = &r_info[pa_ptr->m_ptr->r_idx];
    if ((r_ptr->level + 10) <= attacker_ptr->lev ||
        (attacker_ptr->skill_exp[GINOU_SUDE] >= s_info[attacker_ptr->pclass].s_max[GINOU_SUDE]))
        return;

    if (attacker_ptr->skill_exp[GINOU_SUDE] < WEAPON_EXP_BEGINNER)
        attacker_ptr->skill_exp[GINOU_SUDE] += 40;
    else if ((attacker_ptr->skill_exp[GINOU_SUDE] < WEAPON_EXP_SKILLED))
        attacker_ptr->skill_exp[GINOU_SUDE] += 5;
    else if ((attacker_ptr->skill_exp[GINOU_SUDE] < WEAPON_EXP_EXPERT) && (attacker_ptr->lev > 19))
        attacker_ptr->skill_exp[GINOU_SUDE] += 1;
    else if ((attacker_ptr->lev > 34))
        if (one_in_(3))
            attacker_ptr->skill_exp[GINOU_SUDE] += 1;

    attacker_ptr->update |= (PU_BONUS);
}

/*!
 * @brief 装備している武器の技能値を増加させる
 * @param attacker_ptr プレーヤーへの参照ポインタ
 * @param pa_ptr 直接攻撃構造体への参照ポインタ
 * @return なし
 */
static void get_weapon_exp(player_type *attacker_ptr, player_attack_type *pa_ptr)
{
    OBJECT_TYPE_VALUE tval = attacker_ptr->inventory_list[INVEN_RARM + pa_ptr->hand].tval - TV_WEAPON_BEGIN;
    OBJECT_SUBTYPE_VALUE sval = attacker_ptr->inventory_list[INVEN_RARM + pa_ptr->hand].sval;
    int now_exp = attacker_ptr->weapon_exp[tval][sval];
    if (now_exp >= s_info[attacker_ptr->pclass].w_max[tval][sval])
        return;

    SUB_EXP amount = 0;
    if (now_exp < WEAPON_EXP_BEGINNER)
        amount = 80;
    else if (now_exp < WEAPON_EXP_SKILLED)
        amount = 10;
    else if ((now_exp < WEAPON_EXP_EXPERT) && (attacker_ptr->lev > 19))
        amount = 1;
    else if ((attacker_ptr->lev > 34) && one_in_(2))
        amount = 1;

    attacker_ptr->weapon_exp[tval][sval] += amount;
    attacker_ptr->update |= (PU_BONUS);
}

/*!
 * @brief 直接攻撃に伴う技能値の上昇処理
 * @param attacker_ptr プレーヤーへの参照ポインタ
 * @param pa_ptr 直接攻撃構造体への参照ポインタ
 * @return なし
 */
static void get_attack_exp(player_type *attacker_ptr, player_attack_type *pa_ptr)
{
    monster_race *r_ptr = &r_info[pa_ptr->m_ptr->r_idx];
    object_type *o_ptr = &attacker_ptr->inventory_list[INVEN_RARM + pa_ptr->hand];
    if (!o_ptr->k_idx == 0)
    {
        get_bare_knuckle_exp(attacker_ptr, pa_ptr);
        return;
    }

    if (!object_is_melee_weapon(o_ptr) || ((r_ptr->level + 10) <= attacker_ptr->lev))
        return;

    get_weapon_exp(attacker_ptr, pa_ptr);
}

/*!
 * @brief 攻撃回数を決定する
 * @param attacker_ptr プレーヤーへの参照ポインタ
 * @param pa_ptr 直接攻撃構造体への参照ポインタ
 * @return なし
 * @details 毒針は確定で1回
 */
static void calc_num_blow(player_type *attacker_ptr, player_attack_type *pa_ptr)
{
    if ((pa_ptr->mode == HISSATSU_KYUSHO) || (pa_ptr->mode == HISSATSU_MINEUCHI) || (pa_ptr->mode == HISSATSU_3DAN) || (pa_ptr->mode == HISSATSU_IAI))
        pa_ptr->num_blow = 1;
    else if (pa_ptr->mode == HISSATSU_COLD)
        pa_ptr->num_blow = attacker_ptr->num_blow[pa_ptr->hand] + 2;
    else
        pa_ptr->num_blow = attacker_ptr->num_blow[pa_ptr->hand];

    object_type *o_ptr = &attacker_ptr->inventory_list[INVEN_RARM + pa_ptr->hand];
    if ((o_ptr->tval == TV_SWORD) && (o_ptr->sval == SV_POISON_NEEDLE))
        pa_ptr->num_blow = 1;
}

static void print_suprise_attack(player_attack_type *pa_ptr)
{
    if (pa_ptr->backstab)
        msg_format(_("あなたは冷酷にも眠っている無力な%sを突き刺した！", "You cruelly stab the helpless, sleeping %s!"), pa_ptr->m_name);
    else if (pa_ptr->suprise_attack)
        msg_format(_("不意を突いて%sに強烈な一撃を喰らわせた！", "You make surprise attack, and hit %s with a powerful blow!"), pa_ptr->m_name);
    else if (pa_ptr->stab_fleeing)
        msg_format(_("逃げる%sを背中から突き刺した！", "You backstab the fleeing %s!"), pa_ptr->m_name);
    else if (!pa_ptr->monk_attack)
        msg_format(_("%sを攻撃した。", "You hit %s."), pa_ptr->m_name);
}

/*!
 * todo 実質enumなので後で型を変更する
 * @brief 混沌属性の武器におけるカオス効果を決定する
 * @param attacker_ptr プレーヤーへの参照ポインタ
 * @param pa_ptr 直接攻撃構造体への参照ポインタ
 * @return カオス効果
 * @details
 * 吸血20%、地震0.12%、混乱26.892%、テレポート・アウェイ1.494%、変身1.494% /
 * Vampiric 20%, Quake 0.12%, Confusion 26.892%, Teleport away 1.494% and Polymorph 1.494%
 */
static chaotic_effect select_chaotic_effect(player_type *attacker_ptr, player_attack_type *pa_ptr)
{
    if (!(have_flag(pa_ptr->flags, TR_CHAOTIC)) || one_in_(2))
        return CE_NONE;

    if (one_in_(10))
        chg_virtue(attacker_ptr, V_CHANCE, 1);

    if (randint1(5) < 3)
        return CE_VAMPIRIC;
    
    if (one_in_(250))
        return CE_CONFUSION;

    if (!one_in_(10))
        return CE_QUAKE;

    return one_in_(2) ? CE_TELE_AWAY : CE_POLYMORPH;
}

/*!
 * @brief 生命のあるモンスターから吸血できるか判定する
 * @param attacker_ptr プレーヤーへの参照ポインタ
 * @param pa_ptr 直接攻撃構造体への参照ポインタ
 * @なし
 */
static void decide_blood_sucking(player_type *attacker_ptr, player_attack_type *pa_ptr)
{
    bool is_blood_sucker = have_flag(pa_ptr->flags, TR_VAMPIRIC);
    is_blood_sucker |= pa_ptr->chaos_effect == CE_VAMPIRIC;
    is_blood_sucker |= pa_ptr->mode == HISSATSU_DRAIN;
    is_blood_sucker |= hex_spelling(attacker_ptr, HEX_VAMP_BLADE);
    if (!is_blood_sucker)
        return;

    pa_ptr->can_drain = monster_living(pa_ptr->m_ptr->r_idx);
}

/*!
 * @brief 朦朧への抵抗値を計算する
 * @param pa_ptr 直接攻撃構造体への参照ポインタ
 * @return 朦朧への抵抗値
 */
static int calc_stun_resistance(player_attack_type *pa_ptr)
{
    monster_race *r_ptr = &r_info[pa_ptr->m_ptr->r_idx];
    int resist_stun = 0;
    if (r_ptr->flags1 & RF1_UNIQUE)
        resist_stun += 88;

    if (r_ptr->flags3 & RF3_NO_STUN)
        resist_stun += 66;

    if (r_ptr->flags3 & RF3_NO_CONF)
        resist_stun += 33;

    if (r_ptr->flags3 & RF3_NO_SLEEP)
        resist_stun += 33;

    if ((r_ptr->flags3 & RF3_UNDEAD) || (r_ptr->flags3 & RF3_NONLIVING))
        resist_stun += 66;

    return resist_stun;
}

/*!
 * @brief 技のランダム選択回数を決定する
 * @param attacker_ptr プレーヤーへの参照ポインタ
 * @return 技のランダム選択回数
 * @details ランダム選択は一番強い技が最終的に選択されるので、回数が多いほど有利
 */
static int calc_max_blow_selection_times(player_type *attacker_ptr)
{
    if (attacker_ptr->special_defense & KAMAE_BYAKKO)
        return (attacker_ptr->lev < 3 ? 1 : attacker_ptr->lev / 3);
    
    if (attacker_ptr->special_defense & KAMAE_SUZAKU)
        return 1;
    
    if (attacker_ptr->special_defense & KAMAE_GENBU)
        return 1;
    
    return attacker_ptr->lev < 7 ? 1 : attacker_ptr->lev / 7;
}

/*!
 * @brief プレーヤーのレベルと技の難度を加味しつつ、確率で一番強い技を選ぶ
 * @param attacker_ptr プレーヤーへの参照ポインタ
 * @return 技のランダム選択回数
 * @return 技の行使に必要な最低レベル
 */
static int select_blow(player_type *attacker_ptr, player_attack_type *pa_ptr, int max_blow_selection_times)
{
    int min_level = 1;
    martial_arts *old_ptr = &ma_blows[0];
    for (int times = 0; times < max_blow_selection_times; times++) {
        do {
            pa_ptr->ma_ptr = &ma_blows[randint0(MAX_MA)];
            if ((attacker_ptr->pclass == CLASS_FORCETRAINER) && (pa_ptr->ma_ptr->min_level > 1))
                min_level = pa_ptr->ma_ptr->min_level + 3;
            else
                min_level = pa_ptr->ma_ptr->min_level;
        } while ((min_level > attacker_ptr->lev) || (randint1(attacker_ptr->lev) < pa_ptr->ma_ptr->chance));

        if ((pa_ptr->ma_ptr->min_level > old_ptr->min_level) && !attacker_ptr->stun && !attacker_ptr->confused) {
            old_ptr = pa_ptr->ma_ptr;

            if (current_world_ptr->wizard && cheat_xtra) {
                msg_print(_("攻撃を再選択しました。", "Attack re-selected."));
            }
        } else {
            pa_ptr->ma_ptr = old_ptr;
        }
    }

    if (attacker_ptr->pclass == CLASS_FORCETRAINER)
        min_level = MAX(1, pa_ptr->ma_ptr->min_level - 3);
    else
        min_level = pa_ptr->ma_ptr->min_level;
}

/*!
 * @brief プレイヤーの打撃処理サブルーチン /
 * Player attacks a (poor, defenseless) creature        -RAK-
 * @param y 攻撃目標のY座標
 * @param x 攻撃目標のX座標
 * @param fear 攻撃を受けたモンスターが恐慌状態に陥ったかを返す参照ポインタ
 * @param mdeath 攻撃を受けたモンスターが死亡したかを返す参照ポインタ
 * @param hand 攻撃を行うための武器を持つ手
 * @param mode 発動中の剣術ID
 * @return なし
 * @details
 * If no "weapon" is available, then "punch" the monster one time.
 */
void exe_player_attack_to_monster(player_type *attacker_ptr, POSITION y, POSITION x, bool *fear, bool *mdeath, s16b hand, combat_options mode)
{
    bool vorpal_cut = FALSE;
    bool do_quake = FALSE;
    bool weak = FALSE;
    bool drain_msg = TRUE;
    int drain_result = 0;
    int drain_heal = 0;
    int drain_left = MAX_VAMPIRIC_DRAIN;

    floor_type *floor_ptr = attacker_ptr->current_floor_ptr;
    grid_type *g_ptr = &floor_ptr->grid_array[y][x];
    monster_type *m_ptr = &floor_ptr->m_list[g_ptr->m_idx];
    player_attack_type tmp_attack;
    player_attack_type *pa_ptr = initialize_player_attack_type(&tmp_attack, hand, mode, m_ptr);
    monster_race *r_ptr = &r_info[pa_ptr->m_ptr->r_idx];
    bool is_human = (r_ptr->d_char == 'p');
    bool is_lowlevel = (r_ptr->level < (attacker_ptr->lev - 15));

    attack_classify(attacker_ptr, pa_ptr);
    get_attack_exp(attacker_ptr, pa_ptr);

    /* Disturb the monster */
    (void)set_monster_csleep(attacker_ptr, g_ptr->m_idx, 0);
    monster_desc(attacker_ptr, pa_ptr->m_name, m_ptr, 0);

    int chance = calc_attack_quality(attacker_ptr, pa_ptr);
    object_type *o_ptr = &attacker_ptr->inventory_list[INVEN_RARM + pa_ptr->hand];
    bool is_zantetsu_nullified = ((o_ptr->name1 == ART_ZANTETSU) && (r_ptr->d_char == 'j'));
    bool is_ej_nullified = ((o_ptr->name1 == ART_EXCALIBUR_J) && (r_ptr->d_char == 'S'));
    calc_num_blow(attacker_ptr, pa_ptr);

    /* Attack once for each legal blow */
    int num = 0;
    while ((num++ < pa_ptr->num_blow) && !attacker_ptr->is_dead) {
        if (!process_attack_hit(attacker_ptr, pa_ptr, chance))
            continue;

        int vorpal_chance = ((o_ptr->name1 == ART_VORPAL_BLADE) || (o_ptr->name1 == ART_CHAINSWORD)) ? 2 : 4;

        sound(SOUND_HIT);
        print_suprise_attack(pa_ptr);

        object_flags(o_ptr, pa_ptr->flags);
        pa_ptr->chaos_effect = select_chaotic_effect(attacker_ptr, pa_ptr);
        decide_blood_sucking(attacker_ptr, pa_ptr);

        if ((have_flag(pa_ptr->flags, TR_VORPAL) || hex_spelling(attacker_ptr, HEX_RUNESWORD)) && (randint1(vorpal_chance * 3 / 2) == 1) && !is_zantetsu_nullified)
            vorpal_cut = TRUE;
        else
            vorpal_cut = FALSE;

        // ダメージ計算を開始、取り敢えず素手と仮定し1とする.
        pa_ptr->attack_damage = 1;
        if (pa_ptr->monk_attack) {
            int special_effect = 0;
            int stun_effect = 0;
            WEIGHT weight = 8;
            int resist_stun = calc_stun_resistance(pa_ptr);
            int max_blow_selection_times = calc_max_blow_selection_times(attacker_ptr);
            int min_level = select_blow(attacker_ptr, pa_ptr, max_blow_selection_times);

            pa_ptr->attack_damage = damroll(pa_ptr->ma_ptr->dd + attacker_ptr->to_dd[hand], pa_ptr->ma_ptr->ds + attacker_ptr->to_ds[hand]);
            if (attacker_ptr->special_attack & ATTACK_SUIKEN)
                pa_ptr->attack_damage *= 2;

            if (pa_ptr->ma_ptr->effect == MA_KNEE) {
                if (r_ptr->flags1 & RF1_MALE) {
                    msg_format(_("%sに金的膝蹴りをくらわした！", "You hit %s in the groin with your knee!"), pa_ptr->m_name);
                    sound(SOUND_PAIN);
                    special_effect = MA_KNEE;
                } else
                    msg_format(pa_ptr->ma_ptr->desc, pa_ptr->m_name);
            }

            else if (pa_ptr->ma_ptr->effect == MA_SLOW) {
                if (!((r_ptr->flags1 & RF1_NEVER_MOVE) || my_strchr("~#{}.UjmeEv$,DdsbBFIJQSXclnw!=?", r_ptr->d_char))) {
                    msg_format(_("%sの足首に関節蹴りをくらわした！", "You kick %s in the ankle."), pa_ptr->m_name);
                    special_effect = MA_SLOW;
                } else
                    msg_format(pa_ptr->ma_ptr->desc, pa_ptr->m_name);
            } else {
                if (pa_ptr->ma_ptr->effect) {
                    stun_effect = (pa_ptr->ma_ptr->effect / 2) + randint1(pa_ptr->ma_ptr->effect / 2);
                }

                msg_format(pa_ptr->ma_ptr->desc, pa_ptr->m_name);
            }

            if (attacker_ptr->special_defense & KAMAE_SUZAKU)
                weight = 4;
            if ((attacker_ptr->pclass == CLASS_FORCETRAINER) && P_PTR_KI) {
                weight += (P_PTR_KI / 30);
                if (weight > 20)
                    weight = 20;
            }

            pa_ptr->attack_damage = critical_norm(attacker_ptr, attacker_ptr->lev * weight, min_level, pa_ptr->attack_damage, attacker_ptr->to_h[0], 0);

            if ((special_effect == MA_KNEE) && ((pa_ptr->attack_damage + attacker_ptr->to_d[hand]) < m_ptr->hp)) {
                msg_format(_("%^sは苦痛にうめいている！", "%^s moans in agony!"), pa_ptr->m_name);
                stun_effect = 7 + randint1(13);
                resist_stun /= 3;
            }

            else if ((special_effect == MA_SLOW) && ((pa_ptr->attack_damage + attacker_ptr->to_d[hand]) < m_ptr->hp)) {
                if (!(r_ptr->flags1 & RF1_UNIQUE) && (randint1(attacker_ptr->lev) > r_ptr->level) && m_ptr->mspeed > 60) {
                    msg_format(_("%^sは足をひきずり始めた。", "%^s starts limping slower."), pa_ptr->m_name);
                    m_ptr->mspeed -= 10;
                }
            }

            if (stun_effect && ((pa_ptr->attack_damage + attacker_ptr->to_d[hand]) < m_ptr->hp)) {
                if (attacker_ptr->lev > randint1(r_ptr->level + resist_stun + 10)) {
                    if (set_monster_stunned(attacker_ptr, g_ptr->m_idx, stun_effect + MON_STUNNED(m_ptr))) {
                        msg_format(_("%^sはフラフラになった。", "%^s is stunned."), pa_ptr->m_name);
                    } else {
                        msg_format(_("%^sはさらにフラフラになった。", "%^s is more stunned."), pa_ptr->m_name);
                    }
                }
            }
        }

        /* Handle normal weapon */
        else if (o_ptr->k_idx) {
            pa_ptr->attack_damage = damroll(o_ptr->dd + attacker_ptr->to_dd[hand], o_ptr->ds + attacker_ptr->to_ds[hand]);
            pa_ptr->attack_damage = calc_attack_damage_with_slay(attacker_ptr, o_ptr, pa_ptr->attack_damage, m_ptr, mode, FALSE);

            if (pa_ptr->backstab) {
                pa_ptr->attack_damage *= (3 + (attacker_ptr->lev / 20));
            } else if (pa_ptr->suprise_attack) {
                pa_ptr->attack_damage = pa_ptr->attack_damage * (5 + (attacker_ptr->lev * 2 / 25)) / 2;
            } else if (pa_ptr->stab_fleeing) {
                pa_ptr->attack_damage = (3 * pa_ptr->attack_damage) / 2;
            }

            if ((attacker_ptr->impact[hand] && ((pa_ptr->attack_damage > 50) || one_in_(7))) || (pa_ptr->chaos_effect == CE_QUAKE) || (mode == HISSATSU_QUAKE)) {
                do_quake = TRUE;
            }

            if ((!(o_ptr->tval == TV_SWORD) || !(o_ptr->sval == SV_POISON_NEEDLE)) && !(mode == HISSATSU_KYUSHO))
                pa_ptr->attack_damage = critical_norm(attacker_ptr, o_ptr->weight, o_ptr->to_h, pa_ptr->attack_damage, attacker_ptr->to_h[hand], mode);

            drain_result = pa_ptr->attack_damage;

            if (vorpal_cut) {
                int mult = 2;

                if ((o_ptr->name1 == ART_CHAINSWORD) && !one_in_(2)) {
                    char chainsword_noise[1024];
                    if (!get_rnd_line(_("chainswd_j.txt", "chainswd.txt"), 0, chainsword_noise)) {
                        msg_print(chainsword_noise);
                    }
                }

                if (o_ptr->name1 == ART_VORPAL_BLADE) {
                    msg_print(_("目にも止まらぬヴォーパルブレード、手錬の早業！", "Your Vorpal Blade goes snicker-snack!"));
                } else {
                    msg_format(_("%sをグッサリ切り裂いた！", "Your weapon cuts deep into %s!"), pa_ptr->m_name);
                }

                /* Try to increase the damage */
                while (one_in_(vorpal_chance)) {
                    mult++;
                }

                pa_ptr->attack_damage *= (HIT_POINT)mult;

                /* Ouch! */
                if (((r_ptr->flagsr & RFR_RES_ALL) ? pa_ptr->attack_damage / 100 : pa_ptr->attack_damage) > m_ptr->hp) {
                    msg_format(_("%sを真っ二つにした！", "You cut %s in half!"), pa_ptr->m_name);
                } else {
                    switch (mult) {
                    case 2:
                        msg_format(_("%sを斬った！", "You gouge %s!"), pa_ptr->m_name);
                        break;
                    case 3:
                        msg_format(_("%sをぶった斬った！", "You maim %s!"), pa_ptr->m_name);
                        break;
                    case 4:
                        msg_format(_("%sをメッタ斬りにした！", "You carve %s!"), pa_ptr->m_name);
                        break;
                    case 5:
                        msg_format(_("%sをメッタメタに斬った！", "You cleave %s!"), pa_ptr->m_name);
                        break;
                    case 6:
                        msg_format(_("%sを刺身にした！", "You smite %s!"), pa_ptr->m_name);
                        break;
                    case 7:
                        msg_format(_("%sを斬って斬って斬りまくった！", "You eviscerate %s!"), pa_ptr->m_name);
                        break;
                    default:
                        msg_format(_("%sを細切れにした！", "You shred %s!"), pa_ptr->m_name);
                        break;
                    }
                }
                drain_result = drain_result * 3 / 2;
            }

            pa_ptr->attack_damage += o_ptr->to_d;
            drain_result += o_ptr->to_d;
        }

        /* Apply the player damage bonuses */
        pa_ptr->attack_damage += attacker_ptr->to_d[hand];
        drain_result += attacker_ptr->to_d[hand];

        if ((mode == HISSATSU_SUTEMI) || (mode == HISSATSU_3DAN))
            pa_ptr->attack_damage *= 2;
        if ((mode == HISSATSU_SEKIRYUKA) && !monster_living(m_ptr->r_idx))
            pa_ptr->attack_damage = 0;
        if ((mode == HISSATSU_SEKIRYUKA) && !attacker_ptr->cut)
            pa_ptr->attack_damage /= 2;

        /* No negative damage */
        if (pa_ptr->attack_damage < 0)
            pa_ptr->attack_damage = 0;

        if ((mode == HISSATSU_ZANMA) && !(!monster_living(m_ptr->r_idx) && (r_ptr->flags3 & RF3_EVIL))) {
            pa_ptr->attack_damage = 0;
        }

        if (is_zantetsu_nullified) {
            msg_print(_("こんな軟らかいものは切れん！", "You cannot cut such a elastic thing!"));
            pa_ptr->attack_damage = 0;
        }

        if (is_ej_nullified) {
            msg_print(_("蜘蛛は苦手だ！", "Spiders are difficult for you to deal with!"));
            pa_ptr->attack_damage /= 2;
        }

        if (mode == HISSATSU_MINEUCHI) {
            int tmp = (10 + randint1(15) + attacker_ptr->lev / 5);

            pa_ptr->attack_damage = 0;
            anger_monster(attacker_ptr, m_ptr);

            if (!(r_ptr->flags3 & (RF3_NO_STUN))) {
                /* Get stunned */
                if (MON_STUNNED(m_ptr)) {
                    msg_format(_("%sはひどくもうろうとした。", "%s is more dazed."), pa_ptr->m_name);
                    tmp /= 2;
                } else {
                    msg_format(_("%s はもうろうとした。", "%s is dazed."), pa_ptr->m_name);
                }

                /* Apply stun */
                (void)set_monster_stunned(attacker_ptr, g_ptr->m_idx, MON_STUNNED(m_ptr) + tmp);
            } else {
                msg_format(_("%s には効果がなかった。", "%s is not effected."), pa_ptr->m_name);
            }
        }

        /* Modify the damage */
        pa_ptr->attack_damage = mon_damage_mod(attacker_ptr, m_ptr, pa_ptr->attack_damage, (bool)(((o_ptr->tval == TV_POLEARM) && (o_ptr->sval == SV_DEATH_SCYTHE)) || ((attacker_ptr->pclass == CLASS_BERSERKER) && one_in_(2))));
        if (((o_ptr->tval == TV_SWORD) && (o_ptr->sval == SV_POISON_NEEDLE)) || (mode == HISSATSU_KYUSHO)) {
            if ((randint1(randint1(r_ptr->level / 7) + 5) == 1) && !(r_ptr->flags1 & RF1_UNIQUE) && !(r_ptr->flags7 & RF7_UNIQUE2)) {
                pa_ptr->attack_damage = m_ptr->hp + 1;
                msg_format(_("%sの急所を突き刺した！", "You hit %s on a fatal spot!"), pa_ptr->m_name);
            } else
                pa_ptr->attack_damage = 1;
        } else if ((attacker_ptr->pclass == CLASS_NINJA) && has_melee_weapon(attacker_ptr, INVEN_RARM + hand) && !attacker_ptr->icky_wield[hand] && ((attacker_ptr->cur_lite <= 0) || one_in_(7))) {
            int maxhp = maxroll(r_ptr->hdice, r_ptr->hside);
            if (one_in_(pa_ptr->backstab ? 13 : (pa_ptr->stab_fleeing || pa_ptr->suprise_attack) ? 15 : 27)) {
                pa_ptr->attack_damage *= 5;
                drain_result *= 2;
                msg_format(_("刃が%sに深々と突き刺さった！", "You critically injured %s!"), pa_ptr->m_name);
            } else if (((m_ptr->hp < maxhp / 2) && one_in_((attacker_ptr->num_blow[0] + attacker_ptr->num_blow[1] + 1) * 10)) || ((one_in_(666) || ((pa_ptr->backstab || pa_ptr->suprise_attack) && one_in_(11))) && !(r_ptr->flags1 & RF1_UNIQUE) && !(r_ptr->flags7 & RF7_UNIQUE2))) {
                if ((r_ptr->flags1 & RF1_UNIQUE) || (r_ptr->flags7 & RF7_UNIQUE2) || (m_ptr->hp >= maxhp / 2)) {
                    pa_ptr->attack_damage = MAX(pa_ptr->attack_damage * 5, m_ptr->hp / 2);
                    drain_result *= 2;
                    msg_format(_("%sに致命傷を負わせた！", "You fatally injured %s!"), pa_ptr->m_name);
                } else {
                    pa_ptr->attack_damage = m_ptr->hp + 1;
                    msg_format(_("刃が%sの急所を貫いた！", "You hit %s on a fatal spot!"), pa_ptr->m_name);
                }
            }
        }

        msg_format_wizard(CHEAT_MONSTER,
            _("%dのダメージを与えた。(残りHP %d/%d(%d))", "You do %d damage. (left HP %d/%d(%d))"), pa_ptr->attack_damage,
            m_ptr->hp - pa_ptr->attack_damage, m_ptr->maxhp, m_ptr->max_maxhp);

        if (pa_ptr->attack_damage <= 0)
            pa_ptr->can_drain = FALSE;

        if (drain_result > m_ptr->hp)
            drain_result = m_ptr->hp;

        /* Damage, check for fear and death */
        if (mon_take_hit(attacker_ptr, g_ptr->m_idx, pa_ptr->attack_damage, fear, NULL)) {
            *mdeath = TRUE;
            if ((attacker_ptr->pclass == CLASS_BERSERKER) && attacker_ptr->energy_use) {
                if (attacker_ptr->migite && attacker_ptr->hidarite) {
                    if (hand)
                        attacker_ptr->energy_use = attacker_ptr->energy_use * 3 / 5 + attacker_ptr->energy_use * num * 2 / (attacker_ptr->num_blow[hand] * 5);
                    else
                        attacker_ptr->energy_use = attacker_ptr->energy_use * num * 3 / (attacker_ptr->num_blow[hand] * 5);
                } else {
                    attacker_ptr->energy_use = attacker_ptr->energy_use * num / attacker_ptr->num_blow[hand];
                }
            }
            if ((o_ptr->name1 == ART_ZANTETSU) && is_lowlevel)
                msg_print(_("またつまらぬものを斬ってしまった．．．", "Sigh... Another trifling thing I've cut...."));
            break;
        }

        /* Anger the monster */
        if (pa_ptr->attack_damage > 0)
            anger_monster(attacker_ptr, m_ptr);

        touch_zap_player(m_ptr, attacker_ptr);

        /* Are we draining it?  A little note: If the monster is
		dead, the drain does not work... */

        if (pa_ptr->can_drain && (drain_result > 0)) {
            if (o_ptr->name1 == ART_MURAMASA) {
                if (is_human) {
                    HIT_PROB to_h = o_ptr->to_h;
                    HIT_POINT to_d = o_ptr->to_d;
                    int i, flag;

                    flag = 1;
                    for (i = 0; i < to_h + 3; i++)
                        if (one_in_(4))
                            flag = 0;
                    if (flag)
                        to_h++;

                    flag = 1;
                    for (i = 0; i < to_d + 3; i++)
                        if (one_in_(4))
                            flag = 0;
                    if (flag)
                        to_d++;

                    if (o_ptr->to_h != to_h || o_ptr->to_d != to_d) {
                        msg_print(_("妖刀は血を吸って強くなった！", "Muramasa sucked blood, and became more powerful!"));
                        o_ptr->to_h = to_h;
                        o_ptr->to_d = to_d;
                    }
                }
            } else {
                if (drain_result > 5) /* Did we really hurt it? */
                {
                    drain_heal = damroll(2, drain_result / 6);

                    if (hex_spelling(attacker_ptr, HEX_VAMP_BLADE))
                        drain_heal *= 2;

                    if (cheat_xtra) {
                        msg_format(_("Draining left: %d", "Draining left: %d"), drain_left);
                    }

                    if (drain_left) {
                        if (drain_heal < drain_left) {
                            drain_left -= drain_heal;
                        } else {
                            drain_heal = drain_left;
                            drain_left = 0;
                        }

                        if (drain_msg) {
                            msg_format(_("刃が%sから生命力を吸い取った！", "Your weapon drains life from %s!"), pa_ptr->m_name);
                            drain_msg = FALSE;
                        }

                        drain_heal = (drain_heal * attacker_ptr->mutant_regenerate_mod) / 100;

                        hp_player(attacker_ptr, drain_heal);
                        /* We get to keep some of it! */
                    }
                }
            }

            m_ptr->maxhp -= (pa_ptr->attack_damage + 7) / 8;
            if (m_ptr->hp > m_ptr->maxhp)
                m_ptr->hp = m_ptr->maxhp;
            if (m_ptr->maxhp < 1)
                m_ptr->maxhp = 1;
            weak = TRUE;
        }

        pa_ptr->can_drain = FALSE;
        drain_result = 0;

        /* Confusion attack */
        if ((attacker_ptr->special_attack & ATTACK_CONFUSE) || (pa_ptr->chaos_effect == CE_CONFUSION) || (mode == HISSATSU_CONF) || hex_spelling(attacker_ptr, HEX_CONFUSION)) {
            /* Cancel glowing hands */
            if (attacker_ptr->special_attack & ATTACK_CONFUSE) {
                attacker_ptr->special_attack &= ~(ATTACK_CONFUSE);
                msg_print(_("手の輝きがなくなった。", "Your hands stop glowing."));
                attacker_ptr->redraw |= (PR_STATUS);
            }

            /* Confuse the monster */
            if (r_ptr->flags3 & RF3_NO_CONF) {
                if (is_original_ap_and_seen(attacker_ptr, m_ptr))
                    r_ptr->r_flags3 |= RF3_NO_CONF;
                msg_format(_("%^sには効果がなかった。", "%^s is unaffected."), pa_ptr->m_name);

            } else if (randint0(100) < r_ptr->level) {
                msg_format(_("%^sには効果がなかった。", "%^s is unaffected."), pa_ptr->m_name);
            } else {
                msg_format(_("%^sは混乱したようだ。", "%^s appears confused."), pa_ptr->m_name);
                (void)set_monster_confused(attacker_ptr, g_ptr->m_idx, MON_CONFUSED(m_ptr) + 10 + randint0(attacker_ptr->lev) / 5);
            }
        }

        else if (pa_ptr->chaos_effect == CE_TELE_AWAY) {
            bool resists_tele = FALSE;

            if (r_ptr->flagsr & RFR_RES_TELE) {
                if (r_ptr->flags1 & RF1_UNIQUE) {
                    if (is_original_ap_and_seen(attacker_ptr, m_ptr))
                        r_ptr->r_flagsr |= RFR_RES_TELE;
                    msg_format(_("%^sには効果がなかった。", "%^s is unaffected!"), pa_ptr->m_name);
                    resists_tele = TRUE;
                } else if (r_ptr->level > randint1(100)) {
                    if (is_original_ap_and_seen(attacker_ptr, m_ptr))
                        r_ptr->r_flagsr |= RFR_RES_TELE;
                    msg_format(_("%^sは抵抗力を持っている！", "%^s resists!"), pa_ptr->m_name);
                    resists_tele = TRUE;
                }
            }

            if (!resists_tele) {
                msg_format(_("%^sは消えた！", "%^s disappears!"), pa_ptr->m_name);
                teleport_away(attacker_ptr, g_ptr->m_idx, 50, TELEPORT_PASSIVE);
                num = pa_ptr->num_blow + 1; /* Can't hit it anymore! */
                *mdeath = TRUE;
            }
        }

        else if ((pa_ptr->chaos_effect == CE_POLYMORPH) && (randint1(90) > r_ptr->level)) {
            if (!(r_ptr->flags1 & (RF1_UNIQUE | RF1_QUESTOR)) && !(r_ptr->flagsr & RFR_EFF_RES_CHAO_MASK)) {
                if (polymorph_monster(attacker_ptr, y, x)) {
                    msg_format(_("%^sは変化した！", "%^s changes!"), pa_ptr->m_name);
                    *fear = FALSE;
                    weak = FALSE;
                } else {
                    msg_format(_("%^sには効果がなかった。", "%^s is unaffected."), pa_ptr->m_name);
                }

                /* Hack -- Get new monster */
                m_ptr = &floor_ptr->m_list[g_ptr->m_idx];

                /* Oops, we need a different name... */
                monster_desc(attacker_ptr, pa_ptr->m_name, m_ptr, 0);

                /* Hack -- Get new race */
                r_ptr = &r_info[m_ptr->r_idx];
            }
        } else if (o_ptr->name1 == ART_G_HAMMER) {
            monster_type *target_ptr = &floor_ptr->m_list[g_ptr->m_idx];

            if (target_ptr->hold_o_idx) {
                object_type *q_ptr = &floor_ptr->o_list[target_ptr->hold_o_idx];
                GAME_TEXT o_name[MAX_NLEN];

                object_desc(attacker_ptr, o_name, q_ptr, OD_NAME_ONLY);
                q_ptr->held_m_idx = 0;
                q_ptr->marked = OM_TOUCHED;
                target_ptr->hold_o_idx = q_ptr->next_o_idx;
                q_ptr->next_o_idx = 0;
                msg_format(_("%sを奪った。", "You snatched %s."), o_name);
                inven_carry(attacker_ptr, q_ptr);
            }
        }

        pa_ptr->backstab = FALSE;
        pa_ptr->suprise_attack = FALSE;
    }

    if (weak && !(*mdeath)) {
        msg_format(_("%sは弱くなったようだ。", "%^s seems weakened."), pa_ptr->m_name);
    }

    if ((drain_left != MAX_VAMPIRIC_DRAIN) && one_in_(4)) {
        chg_virtue(attacker_ptr, V_UNLIFE, 1);
    }

    /* Mega-Hac
	attack_damage -- apply earthquake brand */
    if (do_quake) {
        earthquake(attacker_ptr, attacker_ptr->y, attacker_ptr->x, 10, 0);
        if (!floor_ptr->grid_array[y][x].m_idx)
            *mdeath = TRUE;
    }
}
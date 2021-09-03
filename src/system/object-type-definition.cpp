﻿/*
 * @file object-type-definition.h
 * @brief アイテム定義の構造体とエンティティ処理実装
 * @author Hourier
 * @date 2021/05/02
 */

#include "system/object-type-definition.h"
#include "object-enchant/object-curse.h"
#include "object-enchant/special-object-flags.h"
#include "object-enchant/trc-types.h"
#include "object-enchant/trg-types.h"
#include "object/object-kind.h"
#include "sv-definition/sv-armor-types.h"
#include "sv-definition/sv-other-types.h"
#include "sv-definition/sv-protector-types.h"
#include "sv-definition/sv-weapon-types.h"
#include "system/player-type-definition.h"

/*!
 * @brief オブジェクトを初期化する
 * Wipe an object clean.
 */
void object_type::wipe()
{
    (void)WIPE(this, object_type);
}

/*!
 * @brief オブジェクトを複製する
 * Wipe an object clean.
 * @param j_ptr 複製元のオブジェクトの構造体参照ポインタ
 */
void object_type::copy_from(object_type *j_ptr)
{
    (void)COPY(this, j_ptr, object_type);
}

/*!
 * @brief オブジェクト構造体にベースアイテムを作成する
 * Prepare an object based on an object kind.
 * @param player_ptr プレーヤーへの参照ポインタ
 * @param k_idx 新たに作成したいベースアイテム情報のID
 */
void object_type::prep(KIND_OBJECT_IDX ko_idx)
{
    object_kind *k_ptr = &k_info[ko_idx];
    auto old_stack_idx = this->stack_idx;
    wipe();
    this->stack_idx = old_stack_idx;
    this->k_idx = ko_idx;
    this->tval = k_ptr->tval;
    this->sval = k_ptr->sval;
    this->pval = k_ptr->pval;
    this->number = 1;
    this->weight = k_ptr->weight;
    this->to_h = k_ptr->to_h;
    this->to_d = k_ptr->to_d;
    this->to_a = k_ptr->to_a;
    this->ac = k_ptr->ac;
    this->dd = k_ptr->dd;
    this->ds = k_ptr->ds;

    if (k_ptr->act_idx > 0)
        this->xtra2 = (XTRA8)k_ptr->act_idx;
    if (k_info[this->k_idx].cost <= 0)
        this->ident |= (IDENT_BROKEN);

    if (k_ptr->gen_flags.has(TRG::CURSED))
        this->curse_flags.set(TRC::CURSED);
    if (k_ptr->gen_flags.has(TRG::HEAVY_CURSE))
        this->curse_flags.set(TRC::HEAVY_CURSE);
    if (k_ptr->gen_flags.has(TRG::PERMA_CURSE))
        this->curse_flags.set(TRC::PERMA_CURSE);
    if (k_ptr->gen_flags.has(TRG::RANDOM_CURSE0))
        this->curse_flags.set(get_curse(0, this));
    if (k_ptr->gen_flags.has(TRG::RANDOM_CURSE1))
        this->curse_flags.set(get_curse(1, this));
    if (k_ptr->gen_flags.has(TRG::RANDOM_CURSE2))
        this->curse_flags.set(get_curse(2, this));
}

/*!
 * @brief オブジェクトが武器として装備できるかどうかを返す / Check if an object is weapon (including bows)
 * @return 武器として使えるならばtrueを返す
 */
bool object_type::is_weapon() const
{
    return (TV_WEAPON_BEGIN <= this->tval) && (this->tval <= TV_WEAPON_END);
}

/*!
 * @brief オブジェクトが武器や矢弾として使用できるかを返す / Check if an object is weapon (including bows and ammo)
 * Rare weapons/aromors including Blade of Chaos, Dragon armors, etc.
 * @return 武器や矢弾として使えるならばtrueを返す
 */
bool object_type::is_weapon_ammo() const
{
    return (TV_MISSILE_BEGIN <= this->tval) && (this->tval <= TV_WEAPON_END);
}

/*!
 * @brief オブジェクトが武器、防具、矢弾として使用できるかを返す / Check if an object is weapon, armour or ammo
 * @return 武器、防具、矢弾として使えるならばtrueを返す
 */
bool object_type::is_weapon_armour_ammo() const
{
    return this->is_weapon_ammo() || this->is_armour();
}

/*!
 * @brief オブジェクトが近接武器として装備できるかを返す / Melee weapons
 * @return 近接武器として使えるならばtrueを返す
 */
bool object_type::is_melee_weapon() const
{
    return (TV_DIGGING <= this->tval) && (this->tval <= TV_SWORD);
}

/*!
 * @brief エッセンスの付加可能な武器や矢弾かを返す
 * @return エッセンスの付加可能な武器か矢弾ならばtrueを返す。
 */
bool object_type::is_melee_ammo() const
{
    switch (this->tval) {
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_DIGGING:
    case TV_BOLT:
    case TV_ARROW:
    case TV_SHOT: {
        return true;
    }
    case TV_SWORD: {
        if (this->sval != SV_POISON_NEEDLE)
            return true;
    }

    default:
        break;
    }

    return false;
}

/*!
 * @brief オブジェクトが装備可能であるかを返す / Wearable including all weapon, all armour, bow, light source, amulet, and ring
 * @return 装備可能ならばTRUEを返す
 */
bool object_type::is_wearable() const
{
    return (TV_WEARABLE_BEGIN <= this->tval) && (this->tval <= TV_WEARABLE_END);
}

/*!
 * @brief オブジェクトが装備品であるかを返す(object_type::is_wearableに矢弾を含む) / Equipment including all wearable objects and ammo
 * @return 装備品ならばtrueを返す
 */
bool object_type::is_equipment() const
{
    return (TV_EQUIP_BEGIN <= this->tval) && (this->tval <= TV_EQUIP_END);
}

/*!
 * @brief 武器匠の「武器」鑑定対象になるかを判定する。/ Hook to specify "weapon"
 * @return 対象になるならtrueを返す。
 */
bool object_type::is_orthodox_melee_weapons() const
{
    switch (this->tval) {
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_DIGGING: {
        return true;
    }
    case TV_SWORD: {
        if (this->sval != SV_POISON_NEEDLE)
            return true;
    }

    default:
        break;
    }

    return false;
}

/*!
 * @brief 修復対象となる壊れた武器かを判定する。 / Hook to specify "broken weapon"
 * @return 修復対象になるならTRUEを返す。
 */
bool object_type::is_broken_weapon() const
{
    if (this->tval != TV_SWORD)
        return false;

    switch (this->sval) {
    case SV_BROKEN_DAGGER:
    case SV_BROKEN_SWORD:
        return true;
    }

    return false;
}

/*!
 * @brief オブジェクトが投射可能な武器かどうかを返す。
 * @return 投射可能な武器ならばtrue
 */
bool object_type::is_throwable() const
{
    return (this->tval == TV_DIGGING) || (this->tval == TV_SWORD) || (this->tval == TV_POLEARM) || (this->tval == TV_HAFTED);
}

/*!
 * @brief オブジェクトがどちらの手にも装備できる武器かどうかの判定
 * @return 左右両方の手で装備できるならばtrueを返す。
 */
bool object_type::is_mochikae() const
{
    /* Check for a usable slot */
    return ((TV_DIGGING <= this->tval) && (this->tval <= TV_SWORD)) || (this->tval == TV_SHIELD) || (this->tval == TV_CAPTURE) || (this->tval == TV_CARD);
}

/*!
 * @brief オブジェクトが強化不能武器であるかを返す / Poison needle can not be enchanted
 * @return 強化不能ならばtrueを返す
 */
bool object_type::refuse_enchant_weapon() const
{
    return (this->tval == TV_SWORD) && (this->sval == SV_POISON_NEEDLE);
}

/*!
 * @brief オブジェクトが強化可能武器であるかを返す /
 * Check if an object is weapon (including bows and ammo) and allows enchantment
 * @return 強化可能ならばtrueを返す
 */
bool object_type::allow_enchant_weapon() const
{
    return this->is_weapon_ammo() && !this->refuse_enchant_weapon();
}

/*!
 * @brief オブジェクトが強化可能な近接武器であるかを返す /
 * Check if an object is melee weapon and allows enchantment
 * @return 強化可能な近接武器ならばtrueを返す
 */
bool object_type::allow_enchant_melee_weapon() const
{
    return this->is_melee_weapon() && !this->refuse_enchant_weapon();
}

/*!
 * @brief オブジェクトが両手持ち可能な武器かを返す /
 * Check if an object is melee weapon and allows wielding with two-hands
 * @return 両手持ち可能ならばTRUEを返す
 */
bool object_type::allow_two_hands_wielding() const
{
    return this->is_melee_weapon() && ((this->weight > 99) || (this->tval == TV_POLEARM));
}

/*!
 * @brief オブジェクトが矢弾として使用できるかどうかを返す / Check if an object is ammo
 * @return 矢弾として使えるならばtrueを返す
 */
bool object_type::is_ammo() const
{
    return (TV_MISSILE_BEGIN <= this->tval) && (this->tval <= TV_MISSILE_END);
}

/*!
 * @brief 対象のアイテムが矢やクロスボウの矢の材料になるかを返す。/
 * Hook to determine if an object is contertible in an arrow/bolt
 * @return 材料にできるならtrueを返す
 */
bool object_type::is_convertible() const
{
    if ((this->tval == TV_JUNK) || (this->tval == TV_SKELETON))
        return true;
    if ((this->tval == TV_CORPSE) && (this->sval == SV_SKELETON))
        return true;
    return false;
}

bool object_type::is_lance() const
{
    auto is_lance = this->tval == TV_POLEARM;
    is_lance &= (this->sval == SV_LANCE) || (this->sval == SV_HEAVY_LANCE);
    return is_lance;
}

/*!
 * @brief オブジェクトが防具として装備できるかどうかを返す / Check if an object is armour
 * @return 防具として装備できるならばtrueを返す
 */
bool object_type::is_armour() const
{
    return (TV_ARMOR_BEGIN <= this->tval) && (this->tval <= TV_ARMOR_END);
}

/*!
 * @brief オブジェクトがレアアイテムかどうかを返す /
 * Rare weapons/aromors including Blade of Chaos, Dragon armors, etc.
 * @return レアアイテムならばTRUEを返す
 */
bool object_type::is_rare() const
{
    switch (this->tval) {
    case TV_HAFTED:
        if (this->sval == SV_MACE_OF_DISRUPTION || this->sval == SV_WIZSTAFF)
            return true;
        break;

    case TV_POLEARM:
        if (this->sval == SV_SCYTHE_OF_SLICING || this->sval == SV_DEATH_SCYTHE)
            return true;
        break;

    case TV_SWORD:
        if (this->sval == SV_BLADE_OF_CHAOS || this->sval == SV_DIAMOND_EDGE || this->sval == SV_POISON_NEEDLE || this->sval == SV_HAYABUSA)
            return true;
        break;

    case TV_SHIELD:
        if (this->sval == SV_DRAGON_SHIELD || this->sval == SV_MIRROR_SHIELD)
            return true;
        break;

    case TV_HELM:
        if (this->sval == SV_DRAGON_HELM)
            return true;
        break;

    case TV_BOOTS:
        if (this->sval == SV_PAIR_OF_DRAGON_GREAVE)
            return true;
        break;

    case TV_CLOAK:
        if (this->sval == SV_ELVEN_CLOAK || this->sval == SV_ETHEREAL_CLOAK || this->sval == SV_SHADOW_CLOAK || this->sval == SV_MAGIC_RESISTANCE_CLOAK)
            return true;
        break;

    case TV_GLOVES:
        if (this->sval == SV_SET_OF_DRAGON_GLOVES)
            return true;
        break;

    case TV_SOFT_ARMOR:
        if (this->sval == SV_KUROSHOUZOKU || this->sval == SV_ABUNAI_MIZUGI)
            return true;
        break;

    case TV_HARD_ARMOR:
        if (this->sval == SV_MITHRIL_CHAIN_MAIL || this->sval == SV_MITHRIL_PLATE_MAIL || this->sval == SV_ADAMANTITE_PLATE_MAIL)
            return true;
        break;

    case TV_DRAG_ARMOR:
        return true;

    default:
        break;
    }

    /* Any others are not "rare" objects. */
    return false;
}

/*!
 * @brief オブジェクトがエゴアイテムかどうかを返す
 * @return エゴアイテムならばtrueを返す
 */
bool object_type::is_ego() const
{
    return this->name2 != 0;
}

/*!
 * @brief オブジェクトが鍛冶師のエッセンス付加済みかを返す /
 * Check if an object is made by a smith's special ability
 * @return エッセンス付加済みならばTRUEを返す
 */
bool object_type::is_smith() const
{
    if (this->is_weapon_armour_ammo() && this->xtra3)
        return true;

    return false;
}

/*!
 * @brief オブジェクトがアーティファクトかを返す /
 * Check if an object is artifact
 * @return アーティファクトならばtrueを返す
 */
bool object_type::is_artifact() const
{
    return this->is_fixed_artifact() || (this->art_name != 0);
}

/*!
 * @brief オブジェクトが固定アーティファクトかを返す /
 * Check if an object is fixed artifact
 * @return 固定アーティファクトならばtrueを返す
 */
bool object_type::is_fixed_artifact() const
{
    return this->name1 != 0;
}

/*!
 * @brief オブジェクトがランダムアーティファクトかを返す /
 * Check if an object is random artifact
 * @return ランダムアーティファクトならばtrueを返す
 */
bool object_type::is_random_artifact() const
{
    return this->is_artifact() && !this->is_fixed_artifact();
}

/*!
 * @brief オブジェクトが通常のアイテム(アーティファクト、エゴ、鍛冶師エッセンス付加いずれでもない)かを返す /
 * Check if an object is neither artifact, ego, nor 'smith' object
 * @return 通常のアイテムならばtrueを返す
 */
bool object_type::is_nameless() const
{
    return !this->is_artifact() && !this->is_ego() && !this->is_smith();
}

bool object_type::is_valid() const
{
    return this->k_idx != 0;
}

bool object_type::is_broken() const
{
    return (this->ident & IDENT_BROKEN) != 0;
}

bool object_type::is_cursed() const
{
    return this->curse_flags.any();
}

bool object_type::is_held_by_monster() const
{
    return this->held_m_idx != 0;
}

/*
 * Determine if a given inventory item is "known"
 * Test One -- Check for special "known" tag
 * Test Two -- Check for "Easy Know" + "Aware"
 */
bool object_type::is_known() const
{
    return ((this->ident & IDENT_KNOWN) != 0) || (k_info[this->k_idx].easy_know && k_info[this->k_idx].aware);
}

bool object_type::is_fully_known() const
{
    return (this->ident & IDENT_FULL_KNOWN) != 0;
}

/*!
 * @brief 与えられたオブジェクトのベースアイテムが鑑定済かを返す / Determine if a given inventory item is "aware"
 * @return 鑑定済ならtrue
 */
bool object_type::is_aware() const
{
    return k_info[this->k_idx].aware;
}

/*
 * Determine if a given inventory item is "tried"
 */
bool object_type::is_tried() const
{
    return k_info[this->k_idx].tried;
}

/*!
 * @brief オブジェクトが薬であるかを返す
 * @return オブジェクトが薬ならばtrueを返す
 */
bool object_type::is_potion() const
{
    return k_info[this->k_idx].tval == TV_POTION;
}

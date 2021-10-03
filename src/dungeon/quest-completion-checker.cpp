﻿#include "dungeon/quest-completion-checker.h"
#include "core/player-update-types.h"
#include "dungeon/quest.h"
#include "effect/effect-characteristics.h"
#include "floor/cave.h"
#include "floor/floor-object.h"
#include "floor/floor-util.h"
#include "grid/feature-flag-types.h"
#include "grid/feature.h"
#include "grid/grid.h"
#include "monster/monster-info.h"
#include "object-enchant/item-apply-magic.h"
#include "system/floor-type-definition.h"
#include "system/grid-type-definition.h"
#include "system/monster-type-definition.h"
#include "system/object-type-definition.h"
#include "system/player-type-definition.h"
#include "view/display-messages.h"

QuestCompletionChecker::QuestCompletionChecker(player_type *player_ptr, monster_type *m_ptr)
    : player_ptr(player_ptr)
    , m_ptr(m_ptr)
{
}

/*!
 * @brief 特定の敵を倒した際にクエスト達成処理 /
 * Check for "Quest" completion when a quest monster is killed or charmed.
 * @param player_ptr プレイヤーへの参照ポインタ
 * @param m_ptr 撃破したモンスターの構造体参照ポインタ
 */
void QuestCompletionChecker::complete()
{
    auto *floor_ptr = this->player_ptr->current_floor_ptr;
    auto quest_num = floor_ptr->inside_quest;
    if (!quest_num) {
        short i;
        for (i = max_q_idx - 1; i > 0; i--) {
            auto *const q_ptr = &quest[i];
            if (q_ptr->status != QuestStatusType::TAKEN) {
                continue;
            }

            if (q_ptr->flags & QUEST_FLAG_PRESET) {
                continue;
            }

            if ((q_ptr->level != floor_ptr->dun_level) && (q_ptr->type != QuestKindType::KILL_ANY_LEVEL)) {
                continue;
            }

            if ((q_ptr->type == QuestKindType::FIND_ARTIFACT) || (q_ptr->type == QuestKindType::FIND_EXIT)) {
                continue;
            }

            if ((q_ptr->type == QuestKindType::KILL_NUMBER) || (q_ptr->type == QuestKindType::TOWER) || (q_ptr->type == QuestKindType::KILL_ALL)) {
                break;
            }

            if (((q_ptr->type == QuestKindType::KILL_LEVEL) || (q_ptr->type == QuestKindType::KILL_ANY_LEVEL) || (q_ptr->type == QuestKindType::RANDOM)) && (q_ptr->r_idx == this->m_ptr->r_idx)) {
                break;
            }
        }

        quest_num = i;
    }

    auto create_stairs = false;
    auto reward = false;
    if ((quest_num > 0) && (quest[quest_num].status == QuestStatusType::TAKEN)) {
        auto *const q_ptr = &quest[quest_num];
        switch (q_ptr->type) {
        case QuestKindType::KILL_NUMBER:
            q_ptr->cur_num++;
            if (q_ptr->cur_num >= q_ptr->num_mon) {
                complete_quest(this->player_ptr, quest_num);
                q_ptr->cur_num = 0;
            }

            break;
        case QuestKindType::KILL_ALL:
            if (!is_hostile(this->m_ptr) || (this->count_all_hostile_monsters() != 1)) {
                break;
            }

            if (q_ptr->flags & QUEST_FLAG_SILENT) {
                q_ptr->status = QuestStatusType::FINISHED;
            } else {
                complete_quest(this->player_ptr, quest_num);
            }

            break;
        case QuestKindType::KILL_LEVEL:
        case QuestKindType::RANDOM:
            if (q_ptr->r_idx != this->m_ptr->r_idx) {
                break;
            }

            q_ptr->cur_num++;
            if (q_ptr->cur_num < q_ptr->max_num) {
                break;
            }

            complete_quest(this->player_ptr, quest_num);
            if (!(q_ptr->flags & QUEST_FLAG_PRESET)) {
                create_stairs = true;
                floor_ptr->inside_quest = 0;
            }

            if ((quest_num == QUEST_OBERON) || (quest_num == QUEST_SERPENT)) {
                q_ptr->status = QuestStatusType::FINISHED;
            }

            if (q_ptr->type == QuestKindType::RANDOM) {
                reward = true;
                q_ptr->status = QuestStatusType::FINISHED;
            }

            break;
        case QuestKindType::KILL_ANY_LEVEL:
            q_ptr->cur_num++;
            if (q_ptr->cur_num >= q_ptr->max_num) {
                complete_quest(this->player_ptr, quest_num);
                q_ptr->cur_num = 0;
            }

            break;
        case QuestKindType::TOWER:
            if (!is_hostile(this->m_ptr)) {
                break;
            }

            if (this->count_all_hostile_monsters() == 1) {
                q_ptr->status = QuestStatusType::STAGE_COMPLETED;
                if ((quest[QUEST_TOWER1].status == QuestStatusType::STAGE_COMPLETED) && (quest[QUEST_TOWER2].status == QuestStatusType::STAGE_COMPLETED)
                    && (quest[QUEST_TOWER3].status == QuestStatusType::STAGE_COMPLETED)) {
                    complete_quest(this->player_ptr, QUEST_TOWER1);
                }
            }

            break;
        default:
            break;
        }
    }

    auto y = this->m_ptr->fy;
    auto x = this->m_ptr->fx;
    if (create_stairs) {
        auto *g_ptr = &floor_ptr->grid_array[y][x];
        while (cave_has_flag_bold(floor_ptr, y, x, FF::PERMANENT) || !g_ptr->o_idx_list.empty() || g_ptr->is_object()) {
            int ny;
            int nx;
            scatter(this->player_ptr, &ny, &nx, y, x, 1, PROJECT_NONE);
            y = ny;
            x = nx;
            g_ptr = &floor_ptr->grid_array[y][x];
        }

        msg_print(_("魔法の階段が現れた...", "A magical staircase appears..."));
        cave_set_feat(this->player_ptr, y, x, feat_down_stair);
        this->player_ptr->update |= PU_FLOW;
    }

    if (!reward) {
        return;
    }

    object_type forge;
    auto *o_ptr = &forge;
    for (auto i = 0; i < (floor_ptr->dun_level / 15) + 1; i++) {
        o_ptr->wipe();
        make_object(this->player_ptr, o_ptr, AM_GOOD | AM_GREAT);
        (void)drop_near(this->player_ptr, o_ptr, -1, y, x);
    }
}

/*!
 * @brief 現在フロアに残っている敵モンスターの数を返す /
 * @return 現在の敵モンスターの数
 */
int QuestCompletionChecker::count_all_hostile_monsters()
{
    auto *floor_ptr = this->player_ptr->current_floor_ptr;
    auto number_mon = 0;
    for (auto x = 0; x < floor_ptr->width; ++x) {
        for (auto y = 0; y < floor_ptr->height; ++y) {
            auto m_idx = floor_ptr->grid_array[y][x].m_idx;
            if ((m_idx > 0) && is_hostile(&floor_ptr->m_list[m_idx])) {
                ++number_mon;
            }
        }
    }

    return number_mon;
}

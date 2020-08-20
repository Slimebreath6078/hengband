﻿#pragma once

#include "system/angband.h"

extern floor_type floor_info;

void update_smell(floor_type *floor_ptr, player_type *subject_ptr);
void forget_flow(floor_type *floor_ptr);
void wipe_o_list(floor_type *floor_ptr);
void set_floor(player_type *player_ptr, POSITION x, POSITION y);
void compact_objects(player_type *owner_ptr, int size);
void scatter(player_type *player_ptr, POSITION *yp, POSITION *xp, POSITION y, POSITION x, POSITION d, BIT_FLAGS mode);

﻿#pragma once

#include "system/angband.h"

enum class SamuraiKata : uint8_t {
    NONE = 0,
    IAI = 1, //!< 居合の型
    FUUJIN = 2, //!< 風塵の型
    KOUKIJIN = 3, //!< 降鬼陣の型
    MUSOU = 4, //!< 無想の型
};

struct samurai_data_type {
    SamuraiKata kata{};
};
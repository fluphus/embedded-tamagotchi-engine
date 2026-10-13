// src/display/display_assets.cpp
// 资源数据定义 - 当前全部为空占位
// 用户放入真实图片资源后, 替换此文件中的 nullptr 为实际数据指针

#include "display_assets.h"

// ============================================================================
//  宠物形态精灵 (占位: 全部为空)
// ============================================================================

const SpriteAsset& formSprite(Form) {static const SpriteAsset empty{nullptr,0,0};return empty;}
const SpriteAsset SPRITE_FOOD[FOOD_COUNT] = {};
const SpriteAsset SPRITE_SPECIAL_FOOD[SFOOD_COUNT] = {};

const AnimFrameSet ANIM_POKE_FRAMES      = { nullptr, 0, 100 };
const AnimFrameSet ANIM_EVOLUTION_FRAMES  = { nullptr, 0, 150 };
const AnimFrameSet ANIM_DESTROY_FRAMES    = { nullptr, 0, 100 };
const AnimFrameSet ANIM_RHONGO_FRAMES     = { nullptr, 0, 200 };
const AnimFrameSet ANIM_MAPO_FRAMES       = { nullptr, 0, 100 };
const AnimFrameSet ANIM_COMBO_FRAMES      = { nullptr, 0, 100 };

const AnimFrameSet ANIM_NOBU_FRAMES       = { nullptr, 0, 100 };

#pragma once
#include "presentation_types.h"
#include "../engine/timeline.h"
#include "../engine/diagnostics.h"
struct AnimTraits {
    static bool wait(const AnimNode& n){return n.type==NODE_WAIT_INPUT||n.type==NODE_UI_FEED_CARDS||n.type==NODE_UI_COMBO_SELECT;}
    static uint16_t duration(const AnimNode& n){return n.durationMs?n.durationMs:wait(n)?0:n.type==NODE_CALLBACK?1:500;}
    static void start(const AnimNode& n){if(n.onStart)n.onStart();}
    static void complete(const AnimNode& n){if(n.onComplete)n.onComplete();}
};
class AnimationQueue : public engine::Timeline<AnimNode,AnimTraits,ANIM_QUEUE_CAPACITY> {
    static uint32_t clock(){const auto& p=engine::support::ports;return p.monotonic?p.monotonic(p.context).value:0;}
public:
    AnimationQueue():engine::Timeline<AnimNode,AnimTraits,ANIM_QUEUE_CAPACITY>(clock){}
};
extern AnimationQueue animQueue;

#pragma once
#include "definition.h"
#if defined(CONTENT_SPROUT) && CONTENT_SPROUT
#include "sprout/policy.h"
namespace selected {using Policy=sprout::Policy;using sprout::capture;using sprout::project;using sprout::validate;}
#else
#include "artoria/policy.h"
namespace selected {using Policy=artoria::Policy;using artoria::capture;using artoria::project;using artoria::validate;}
#endif

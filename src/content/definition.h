#pragma once
#include "../engine/contract.h"
#include "artoria/legacy_view.h"
namespace content {
struct FormDefinition { engine::Id id; const char* name; uint8_t asset; };
#if defined(CONTENT_SPROUT) && CONTENT_SPROUT
#include "sprout/definition.inc"
#else
#include "artoria/definition.inc"
#endif
inline constexpr size_t FormCount=sizeof(Forms)/sizeof(Forms[0]);
static_assert(FormCount<=engine::GalleryCapacity,"gallery capacity exceeded");
inline int formIndex(engine::Id id) {for(size_t i=0;i<FormCount;++i) if(Forms[i].id==id)return int(i);return -1;}
inline bool knownForm(engine::Id id) {return formIndex(id)>=0;}
inline const char* formName(engine::Id id) {int i=formIndex(id);return i<0?"Unknown":Forms[i].name;}
inline Form formAt(size_t index) {return static_cast<Form>(index<FormCount?Forms[index].id:Forms[0].id);}
inline uint8_t assetForm(engine::Id id) {int i=formIndex(id);return i<0?255:Forms[i].asset;}
}

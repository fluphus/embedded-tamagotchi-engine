#pragma once
#include "display_model.h"
#include "../pet/gallery.h"
namespace display {
// Both backends consume this same stable-ID/name/unlock/cursor projection.
inline void bindGallery(DisplayModel& model,const GallerySystem& gallery,uint8_t page,uint8_t selected) {
    model.galleryPage=page;model.gallerySelectedIndex=selected;
    model.galleryTotalForms=TOTAL_GALLERY_FORMS;model.galleryItemsThisPage=gallery.getItemCountOnPage(page);
}
struct GalleryCell {Form form;const char* name;bool unlocked;bool selected;};
inline GalleryCell galleryCell(const DisplayModel& model,const GalleryData& gallery,uint8_t index) {
    const Form form=content::formAt(size_t(model.galleryPage)*GALLERY_ITEMS_PER_PAGE+index);
    return {form,content::formName(form),gallery.isUnlocked(form),index==model.gallerySelectedIndex};
}
}

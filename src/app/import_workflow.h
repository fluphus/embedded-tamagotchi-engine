#pragma once
#include "../core/save_manager.h"
namespace app {
SaveResult resetPetChecked(SaveManager& manager, PetState& pet, DeviceState& device,
                           GalleryData& gallery, uint32_t now, uint8_t day);
SaveResult persistDeviceChecked(SaveManager& manager,const DeviceState& device);
// Shared by console reset, button destroy and host restart/fault tests.
SaveResult restoreVisitOwner(SaveManager& manager, PetState& pet, DeviceState& device,
                            uint32_t now, uint8_t day);
// Called by console COMMIT and host fault-injection tests. RAM commits only on success.
SaveResult commitImport(SaveManager& manager, PetState& pet, DeviceState& device,
                        GalleryData& gallery, uint8_t target, const uint8_t* bytes,
                        size_t size, uint32_t now, uint8_t day);
}

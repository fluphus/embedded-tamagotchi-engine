#pragma once
#include "../core/save_manager.h"
namespace app {
inline void adoptSnapshotClock(const SaveManager& manager,DeviceState& device) {
    // Atomic snapshot owns its settlement anchor. A stale/newer sidecar cannot
    // cause repeated compensation or advance a fallback older pet silently.
    if(manager.getLastSaveTime())device.device_clock_epoch=manager.getLastSaveTime();
}
enum class Startup {NewGame,Loaded,Recovery};
inline Startup loadStartup(SaveManager& manager,PetState& pet) {
    if(!manager.initialized())return Startup::Recovery;
    if(!manager.hasSave())return Startup::NewGame;
    return manager.load(pet)==SAVE_OK?Startup::Loaded:Startup::Recovery;
}
}

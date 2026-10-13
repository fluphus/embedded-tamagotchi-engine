#include "import_workflow.h"
#include "runtime.h"
#include "time_rules.h"
#include "save_format.h"
#include "diagnostics.h"
namespace app {
SaveResult resetPetChecked(SaveManager& manager,PetState& pet,DeviceState& device,
                           GalleryData& gallery,uint32_t now,uint8_t day) {
    if(device.is_visiting || manager.recoveryRequired() || manager.importPending() || device.rounds==UINT16_MAX)
        return SAVE_ERR_CORRUPTED;
    uint8_t retained=0xff;uint32_t sequence=0;
    for(auto slot:{manager.getActiveSlotA(),manager.getActiveSlotB()}) {
        SaveHeader header;SaveResult status;
        if(manager.getSlotStatus(slot,header,status) && (retained==0xff || header.sequence>sequence)) {
            retained=slot;sequence=header.sequence;
        }
    }
    auto fail=[&](const char* stage,SaveResult result) {
        manager.requireRecovery();
        tracef("[Reset] FAILED stage=%s error=%u; recovery required; old pet retained (SAVE_EXPORT_OWNER), automatic writes blocked\n",stage,unsigned(result));
        return result;
    };
    if(retained==0xff)return fail("old_pet",SAVE_ERR_NO_DATA);
    auto result=manager.beginOwnerRestore(retained);
    if(result!=SAVE_OK)return fail("guard_backup",result);
    PetState next=pet;DeviceState nextDevice=device;GalleryData nextGallery=gallery;
    resetPet(next,now,device.rounds);next.daily_feed.reset(day);
    ++nextDevice.rounds;nextDevice.device_clock_epoch=now;nextGallery.unlock(next.form);
    result=manager.saveGallery(nextGallery);
    if(result!=SAVE_OK)return fail("gallery",result);
    result=persistDeviceChecked(manager,nextDevice);
    if(result!=SAVE_OK)return fail("device_v10",result);
    result=manager.save(next,now); // retained old slot is excluded until completion
    if(result!=SAVE_OK)return fail("pet",result);
    if(!manager.verifyLatestSave(now,next))return fail("verify",SAVE_ERR_VERIFY);
    result=manager.finishImport();
    if(result!=SAVE_OK)return fail("complete",result);
    pet=next;device=nextDevice;gallery=nextGallery;manager.markSaved(now);
    trace("[Reset] Pet reset committed.");return SAVE_OK;
}
SaveResult restoreVisitOwner(SaveManager& manager,PetState& pet,DeviceState& device,
                            uint32_t now,uint8_t day) {
    if(!device.is_visiting||device.owner_frozen_slot>=SAVE_SLOT_COUNT||
       manager.recoveryRequired()||manager.importPending()) {
        trace("[Visit] Recovery required: cannot restore owner; visit and saved records preserved.");
        return SAVE_ERR_CORRUPTED;
    }
    auto fail=[&](const char* stage,SaveResult result) {
        manager.requireRecovery();
        tracef("[Visit] FAILED stage=%s error=%u; recovery required; visit and owner records preserved, automatic writes blocked\n",stage,unsigned(result));
        return result;
    };
    const auto owner=device.owner_frozen_slot;
    auto result=manager.beginOwnerRestore(owner);
    if(result!=SAVE_OK)return fail("owner_backup",result);
    PetState nextPet{};
    result=manager.loadFromSlot(owner,nextPet);
    if(result!=SAVE_OK)return fail("owner_load",result);
    const auto duration=frozenDuration(device.visit_start_epoch,now);
    realignOwnerTimestamps(nextPet,duration);
    nextPet.daily_feed.reset(day);
    result=manager.setActivePairAfterVisitEnd(owner);
    if(result!=SAVE_OK)return fail("slot_cfg",result);
    auto nextDevice=device;
    nextDevice.is_visiting=false;nextDevice.owner_frozen_slot=0xff;
    nextDevice.visit_start_epoch=0;nextDevice.device_clock_epoch=now;
    result=persistDeviceChecked(manager,nextDevice);
    if(result!=SAVE_OK)return fail("device_v10",result);
    result=manager.saveRestoredOwner(owner,nextPet,now);
    if(result!=SAVE_OK)return fail("owner_save",result);
    result=manager.finishImport();
    if(result!=SAVE_OK)return fail("guard_complete",result);
    pet=nextPet;device=nextDevice;
    manager.markSaved(now);
    tracef("[Visit] Owner restored. Frozen duration: %lu sec\n",static_cast<unsigned long>(duration));
    return SAVE_OK;
}
SaveResult persistDeviceChecked(SaveManager& manager,const DeviceState& device) {
    const auto result=manager.saveDeviceState(device);
    if(result!=SAVE_OK){
        manager.requireRecovery();
        tracef("[Device] FAILED to persist device state (%u); automatic writes blocked\n",unsigned(result));
    }
    return result;
}
SaveResult commitImport(SaveManager& manager, PetState& pet, DeviceState& device,
                        GalleryData& gallery, uint8_t target, const uint8_t* bytes,
                        size_t size, uint32_t now, uint8_t day) {
    SaveHeader header{};PetState donor{};GalleryData donorGallery{};bool full=false;
    const auto decoded=decodeSave(bytes,size,donor,header,&donorGallery,&full);
    if(decoded!=engine::DecodeError::Ok){
        tracef("[SaveImport] REJECTED: format error %u\n",unsigned(decoded));
        return SAVE_ERR_CORRUPTED;
    }
    if(device.is_visiting||manager.importPending()||target>=SAVE_SLOT_COUNT||header.save_time==0){
        trace("[SaveImport] REJECTED: visit/recovery active or invalid import metadata");
        return SAVE_ERR_CORRUPTED;
    }
    uint8_t owner=0xff;
    if(!full){
        uint32_t sequence=0;
        for(const auto slot:{manager.getActiveSlotA(),manager.getActiveSlotB()}){
            SaveHeader saved;SaveResult status;
            if(manager.getSlotStatus(slot,saved,status) &&
               (owner==0xff||saved.sequence>sequence||(saved.sequence==sequence&&slot<owner))){
                sequence=saved.sequence;owner=slot;
            }
        }
        if(owner==0xff||owner==target){
            trace("[SaveImport] REJECTED: no valid owner or target is frozen owner");
            return SAVE_ERR_CORRUPTED;
        }
    }
    auto fail=[&](const char* stage,SaveResult result){
        manager.requireRecovery();
        tracef("[SaveImport] FAILED stage=%s error=%u; recovery required; partial records retained, automatic writes blocked\n",stage,unsigned(result));
        return result;
    };
    auto result=manager.beginImport(owner);
    if(result!=SAVE_OK)return fail("guard",result);
    result=manager.importSlotRaw(target,header,donor,true);
    if(result!=SAVE_OK)return fail("pet",result);
    GalleryData nextGallery=full?donorGallery:gallery;
    if(!full)nextGallery.unlock(donor.form);
    result=manager.saveGallery(nextGallery);
    if(result!=SAVE_OK)return fail("gallery",result);
    result=full?manager.setActivePairForImportedBaseSlot(target):manager.setActivePairForVisit(owner);
    if(result!=SAVE_OK)return fail("slot_cfg",result);
    result=manager.syncGlobalGalleryToSlot(target);
    if(result!=SAVE_OK)return fail("slot_gallery",result);
    PetState nextPet{};
    result=manager.loadFromSlot(target,nextPet);
    if(result!=SAVE_OK)return fail("load",result);
    realignImportedTimestamps(nextPet,header.save_time,now,day);
    DeviceState nextDevice=device;
    ++nextDevice.rounds;
    nextDevice.device_clock_epoch=now;
    nextDevice.is_visiting=!full;
    nextDevice.owner_frozen_slot=full?0xff:owner;
    nextDevice.visit_start_epoch=full?0:now;
    result=persistDeviceChecked(manager,nextDevice);
    if(result!=SAVE_OK)return fail("device_v10",result);
    result=manager.save(nextPet,now);
    if(result!=SAVE_OK)return fail("final_save",result);
    result=manager.finishImport(full?0xff:owner);
    if(result!=SAVE_OK)return fail("guard_complete",result);
    pet=nextPet;device=nextDevice;gallery=nextGallery;
    manager.markSaved(now);
    if(full)tracef("[SaveImport] Full backup restored to slot=%u, rounds=%u\n",target,device.rounds);
    else tracef("[SaveImport] Visit started: guest in slot=%u, owner frozen in slot=%u, rounds=%u\n",target,owner,device.rounds);
    return SAVE_OK;
}
}

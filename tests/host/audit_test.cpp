#include "app/import_workflow.h"
#include "app/save_protocol.h"
#include "app/startup.h"
#include "display/gallery_view.h"
#include "app/day_rules.h"
#include "app/save_format.h"
#include "core/save_manager.h"
#include "engine/storage.h"
#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <fstream>
#include <iterator>
namespace engine { KeyValue keyValue; }
namespace {
int checks=0;
std::string foreignFixture;
#define CHECK(x) do { ++checks; if(!(x))throw std::runtime_error(std::string(__func__)+":"+std::to_string(__LINE__)+" " #x); } while(false)
struct PowerLoss {};
struct Store {
    std::map<std::string,std::vector<uint8_t>> values;
    std::string fail;
    int failNth=1, matched=0;
    bool openFails=false,eraseFails=false;
    std::string readFails,readMismatch;size_t maxBytes=SIZE_MAX;
    int cutAfter=0,writes=0;
    std::vector<std::string> written;
    engine::KeyValue port(){return {this,
        [](void*p,const char*,bool){return !static_cast<Store*>(p)->openFails;},
        [](void*p,const char*k){return static_cast<Store*>(p)->values.count(k)!=0;},
        [](void*p,const char*k)->size_t{auto&m=static_cast<Store*>(p)->values;auto i=m.find(k);return i==m.end()?0:i->second.size();},
        [](void*p,const char*k,void*b,size_t n)->size_t{auto&s=*static_cast<Store*>(p);auto&m=s.values;auto i=m.find(k);if(s.readFails==k||i==m.end()||i->second.size()>n)return 0;std::copy(i->second.begin(),i->second.end(),static_cast<uint8_t*>(b));if(s.readMismatch==k&&i->second.size())static_cast<uint8_t*>(b)[0]^=1;return i->second.size();},
        [](void*p,const char*k,const void*b,size_t n)->size_t{auto&s=*static_cast<Store*>(p);if(s.fail==k&&++s.matched==s.failNth)return 0;size_t used=0;for(const auto& item:s.values)if(item.first!=k)used+=item.second.size();if(used>s.maxBytes||n>s.maxBytes-used)return 0;auto*data=static_cast<const uint8_t*>(b);s.values[k]={data,data+n};s.written.push_back(k);if(++s.writes==s.cutAfter)throw PowerLoss{};return n;},
        [](void*p){auto&s=*static_cast<Store*>(p);if(s.eraseFails)return false;s.values.clear();return true;}};}
};
PetState fresh(){PetState p{};app::initializePet(p,1700000000);return p;}
void attach(Store&s){engine::keyValue=s.port();}
void frozenOwner(){
    for(bool damagedThird:{false,true}){
        Store s;attach(s);SaveManager m;CHECK(m.init()==SAVE_OK);auto p=fresh();
        CHECK(m.save(p,1700000000)==SAVE_OK);CHECK(m.save(p,1700000300)==SAVE_OK);
        s.values["s0_v10"][22]^=1;
        if(damagedThird)s.values["s2_v10"]={1,2,3};
        DeviceState d;d.init();d.is_visiting=true;d.owner_frozen_slot=0;d.visit_start_epoch=1700000000;
        CHECK(m.saveDeviceState(d)==SAVE_OK);m.setActivePairForVisit(0);
        const auto owner=s.values["s0_v10"];
        SaveManager restart;CHECK(restart.init()==SAVE_OK);CHECK(restart.load(p)==SAVE_OK);
        CHECK(restart.getActiveSlotA()!=0&&restart.getActiveSlotB()!=0);
        for(int i=0;i<6;++i){p.health=60+i;CHECK(restart.save(p,1700000600+300*i)==SAVE_OK);CHECK(s.values["s0_v10"]==owner);}
    }
}
void sidecars(){
    for(const std::string key:{"gallery","dev_state","gallery_v10","device_v10"})
      for(int kind=0;kind<4;++kind){
        Store s;attach(s);SaveManager seed;CHECK(seed.init()==SAVE_OK);
        GalleryData g;g.init();g.unlock(content::formAt(0));DeviceState d;d.init();d.rounds=7;
        if(key=="gallery_v10")CHECK(seed.saveGallery(g)==SAVE_OK);
        else if(key=="device_v10")CHECK(seed.saveDeviceState(d)==SAVE_OK);
        else {std::ifstream file(std::string("tests/fixtures/v9/")+(key=="gallery"?"gallery.bin":"device.bin"),std::ios::binary);s.values[key]={std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};}
        if(kind==1)s.values[key].resize(2);
        if(kind==2 && key=="gallery_v10")s.values[key][0]^=1;
        if(kind==2 && key=="device_v10")s.values[key][1]^=1;
        if(kind==3 && key=="gallery_v10")s.values[key][4]=99;
        if(kind==3 && key=="device_v10")s.values[key][0]=99;
        if(kind==2 && key=="gallery")s.values[key][0]=2;
        if(kind==2 && key=="dev_state")s.values[key][6]=2;
        if(kind==3 && (key=="gallery"||key=="dev_state"))s.values[key].push_back(0);
        if(kind>=2 && (key=="gallery_v10"||key=="device_v10")){
            auto& bytes=s.values[key];const auto sum=engine::checksum(bytes.data(),bytes.size()-2);
            bytes[bytes.size()-2]=uint8_t(sum);bytes.back()=uint8_t(sum>>8);
        }
        const auto original=s.values[key];
        SaveManager boot;CHECK(boot.init()==SAVE_OK);CHECK(s.values[key]==original);
        CHECK(boot.hasSave()); // setup must choose existing-data branch, not New game
        auto p=fresh();CHECK(app::loadStartup(boot,p)==app::Startup::Recovery);CHECK(boot.recoveryRequired());
        const auto before=s.values;
        CHECK(!boot.shouldAutoSave(1700000000));CHECK(boot.save(p,1700000000)!=SAVE_OK);
        CHECK(boot.saveGallery(g)!=SAVE_OK);CHECK(boot.saveDeviceState(d)!=SAVE_OK);CHECK(s.values==before);
      }
}
void unknownSlots(){
    for(bool otherPack:{false,true})for(uint8_t unknownSlot:{uint8_t(0),uint8_t(1)}){
        Store s;attach(s);SaveManager seed;CHECK(seed.init()==SAVE_OK);auto p=fresh();
        CHECK(seed.save(p,1700000000)==SAVE_OK);CHECK(seed.save(p,1700000300)==SAVE_OK);
        const std::string key=unknownSlot==0?"s0_v10":"s1_v10";
        auto &raw=s.values[key];
        if(otherPack){
            std::ifstream file(foreignFixture,std::ios::binary);
            raw={std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
            CHECK(raw.size()==engine::FullBytes);
        }else raw[4]=99;
        const auto crc=engine::recordChecksum(raw.data(),raw.size());raw[22]=uint8_t(crc);raw[23]=uint8_t(crc>>8);
        const auto unknown=raw;
        app::runtime.discardEvents();
        app::configure({nullptr,nullptr,[](void*)->engine::MonotonicMillis{return {600000};},nullptr,nullptr});
        SaveManager boot;CHECK(boot.init()==SAVE_OK);CHECK(boot.load(p)==SAVE_OK);
        for(int i=0;i<6;++i){CHECK(boot.shouldAutoSave(1700000600+i*300));CHECK(boot.save(p,1700000600+i*300)==SAVE_OK);CHECK(s.values[key]==unknown);}
        SaveManager reboot;CHECK(reboot.init()==SAVE_OK&&app::loadStartup(reboot,p)==app::Startup::Loaded);CHECK(s.values[key]==unknown);
        app::configure({});
    }
}
void galleryRendering(){
    GallerySystem gallery;gallery.init();gallery.open();
    gallery.unlockForm(content::formAt(0));DisplayModel model{};
    for(size_t index=0;index<content::FormCount;++index){
        const auto& browse=gallery.getBrowseState();
        display::bindGallery(model,gallery,browse.current_page,browse.selected_index);
        const auto selected=display::galleryCell(model,gallery.getData(),model.gallerySelectedIndex);
        CHECK(selected.form==gallery.getSelectedForm());CHECK(selected.form==content::formAt(index));
        CHECK(selected.selected);CHECK(std::string(selected.name)==content::formName(content::formAt(index)));
        CHECK(selected.unlocked==(index==0));
        for(uint8_t i=0;i<model.galleryItemsThisPage;++i){
            const auto cell=display::galleryCell(model,gallery.getData(),i);
            CHECK(cell.form==content::formAt(model.galleryPage*GALLERY_ITEMS_PER_PAGE+i));
            CHECK(cell.selected==(i==model.gallerySelectedIndex));
            CHECK(cell.unlocked==gallery.isFormUnlocked(cell.form));
        }
#if defined(CONTENT_SPROUT) && CONTENT_SPROUT
        CHECK(selected.form==(index==0?40:47));CHECK(std::string(selected.name)==(index==0?"Seed":"Bloom"));
#endif
#if !defined(CONTENT_SPROUT) || !CONTENT_SPROUT
        static const char* baselineNames[]={"Lily","White Saber","Black Saber","White Lancer","Black Lancer","White Archer","Black Rider","White Ruler","Rhongomyniad","Black Rhongomyniad","nobu","Oda Nobunaga"};
        CHECK(selected.form==index);CHECK(std::string(selected.name)==baselineNames[index]);
#endif
        gallery.navigateRight();
    }
    CHECK(gallery.getSelectedForm()==content::formAt(0));
    gallery.unlockAll();display::bindGallery(model,gallery,0,1);
    CHECK(display::galleryCell(model,gallery.getData(),1).unlocked);
    CHECK(display::galleryCell(model,gallery.getData(),1).selected);
}
std::vector<uint8_t> encodeRam(const PetState& pet,const GalleryData& gallery){
    uint8_t bytes[engine::MaxSaveBytes];size_t size=0;SaveHeader header{};
    CHECK(app::encodeSave(pet,header,&gallery,bytes,sizeof(bytes),size));return {bytes,bytes+size};
}
void imports(){
    for(bool full:{true,false})for(const std::string stage:{"success","gallery","slot_cfg","device_v10","final_save","guard_complete"}){
        Store s;attach(s);SaveManager manager;CHECK(manager.init()==SAVE_OK);
        auto pet=fresh();GalleryData gallery{};gallery.init();gallery.unlock(pet.form);
        DeviceState device;device.init();device.rounds=4;device.device_clock_epoch=1700000000;
        CHECK(manager.saveGallery(gallery)==SAVE_OK);CHECK(manager.saveDeviceState(device)==SAVE_OK);
        CHECK(manager.save(pet,1700000000)==SAVE_OK);
        const auto owner=s.values["s0_v10"];const auto originalRam=encodeRam(pet,gallery);const auto originalDevice=device;
        auto donor=fresh();donor.health=81;donor.birth_timestamp=1699960000;
        GalleryData donorGallery{};donorGallery.init();donorGallery.unlockAll();
        uint8_t raw[engine::MaxSaveBytes];size_t size=0;SaveHeader header{};header.save_time=1700000000;header.sequence=8;
        CHECK(app::encodeSave(donor,header,full?&donorGallery:nullptr,raw,sizeof(raw),size));
        if(stage=="gallery")s.fail="gallery_v10";
        if(stage=="slot_cfg")s.fail="slot_cfg";
        if(stage=="device_v10")s.fail="device_v10";
        if(stage=="final_save")s.fail="s2_v10";
        if(stage=="guard_complete"){s.fail="import_guard";s.failNth=2;}
        std::string output;
        app::configure({&output,nullptr,nullptr,nullptr,[](void*p,const char*line){*static_cast<std::string*>(p)+=line;}});
        const auto result=app::commitImport(manager,pet,device,gallery,1,raw,size,1700003600,15);
        if(stage=="success"){
            CHECK(result==SAVE_OK);CHECK(!manager.recoveryRequired());CHECK(!manager.importPending());
            CHECK(pet.health==81&&pet.birth_timestamp==1699963600);CHECK(device.rounds==5&&device.is_visiting==!full);
            CHECK(output.find(full?"Full backup restored":"Visit started")!=std::string::npos);
            SaveManager boot;CHECK(boot.init()==SAVE_OK);PetState loaded{};CHECK(app::loadStartup(boot,loaded)==app::Startup::Loaded);
            CHECK(loaded.health==81&&loaded.birth_timestamp==1699963600);
            if(!full)for(int i=0;i<4;++i)CHECK(boot.save(loaded,1700003900+i*300)==SAVE_OK&&s.values["s0_v10"]==owner);
        }else{
            CHECK(result!=SAVE_OK);CHECK(manager.recoveryRequired()&&manager.importPending());
            CHECK(output.find("FAILED stage="+stage)!=std::string::npos);
            CHECK(output.find("Full backup restored")==std::string::npos&&output.find("Visit started")==std::string::npos);
            CHECK(encodeRam(pet,gallery)==originalRam);
            CHECK(device.rounds==originalDevice.rounds&&device.is_visiting==originalDevice.is_visiting&&device.owner_frozen_slot==originalDevice.owner_frozen_slot&&device.device_clock_epoch==originalDevice.device_clock_epoch&&device.visit_start_epoch==originalDevice.visit_start_epoch);
            const auto retained=s.values;
            CHECK(!manager.shouldAutoSave(1700003600));CHECK(manager.save(pet,1700003600)!=SAVE_OK);
            CHECK(manager.saveGallery(gallery)!=SAVE_OK);CHECK(manager.saveDeviceState(device)!=SAVE_OK);CHECK(s.values==retained);
            s.fail.clear();SaveManager boot;CHECK(boot.init()==SAVE_OK);PetState loaded{};
            CHECK(app::loadStartup(boot,loaded)==app::Startup::Recovery);CHECK(boot.recoveryRequired());
            CHECK(!boot.shouldAutoSave(1700003600));CHECK(boot.save(pet,1700003600)!=SAVE_OK);CHECK(boot.saveGallery(gallery)!=SAVE_OK);CHECK(boot.saveDeviceState(device)!=SAVE_OK);CHECK(s.values==retained);
            CHECK(boot.loadFromSlot(0,loaded)!=SAVE_OK); // no implicit bypass of pending-import guard
            SaveHeader exported{};CHECK(boot.exportSlotRaw(0,exported,loaded)==SAVE_OK); // backups remain accessible
            if(!full)CHECK(s.values["s0_v10"]==owner);
            std::cout<<"OBSERVED "<<(full?"full":"visit")<<' '<<stage<<" serial="<<output.substr(output.find("[SaveImport] FAILED"),output.find('\n',output.find("[SaveImport] FAILED"))-output.find("[SaveImport] FAILED"))<<'\n';
            std::cout<<"RAM health="<<pet.health<<" rounds="<<device.rounds<<" visiting="<<device.is_visiting<<" pair="<<unsigned(manager.getActiveSlotA())<<','<<unsigned(manager.getActiveSlotB())<<" restart=Recovery NVS:";
            for(const auto& item:s.values)std::cout<<' '<<item.first<<'='<<item.second.size()<<'/'<<engine::checksum(item.second.data(),item.second.size());
            std::cout<<'\n';
            GalleryData recoveryGallery{};bool bound=false;
            CHECK(boot.exportSlotRawWithGallery(0,exported,loaded,recoveryGallery,bound)==SAVE_OK);
            uint8_t recoveryRaw[engine::MaxSaveBytes];size_t recoverySize=0;
            CHECK(app::encodeSave(loaded,exported,&recoveryGallery,recoveryRaw,sizeof(recoveryRaw),recoverySize));
            CHECK(boot.erase()==SAVE_OK);CHECK(!boot.hasSave());CHECK(!boot.recoveryRequired()); // explicit deletion only
            auto recoveryPet=fresh();DeviceState recoveryDevice;recoveryDevice.init();GalleryData emptyGallery{};emptyGallery.init();
            CHECK(app::commitImport(boot,recoveryPet,recoveryDevice,emptyGallery,0,recoveryRaw,recoverySize,1700007200,15)==SAVE_OK);
            CHECK(recoveryPet.health==50&&!recoveryDevice.is_visiting&&!boot.recoveryRequired());
            SaveManager recoveredBoot;CHECK(recoveredBoot.init()==SAVE_OK);
            CHECK(app::loadStartup(recoveredBoot,loaded)==app::Startup::Loaded&&loaded.health==50);
        }
        app::configure({});
    }
}

// Same production operation is called by doReset and onDestroyExecuted.
void restoreOwner(){
    const uint32_t start=1700000000,now=start+3600;
    auto seed=[&](Store& s,SaveManager& manager,PetState& pet,DeviceState& device,GalleryData& gallery){
        attach(s);CHECK(manager.init()==SAVE_OK);pet=fresh();pet.health=50;
        gallery.init();gallery.unlock(pet.form);CHECK(manager.saveGallery(gallery)==SAVE_OK);
        CHECK(manager.save(pet,start)==SAVE_OK); // owner 0, seq 1
        pet.health=71;CHECK(manager.save(pet,start+300)==SAVE_OK); // visitor 1, seq 2
        CHECK(manager.setActivePair(1,2)==SAVE_OK);
        pet.health=82;CHECK(manager.save(pet,start+600)==SAVE_OK); // visitor 2, seq 3
        device.init();device.rounds=4;device.is_visiting=true;
        device.owner_frozen_slot=0;device.visit_start_epoch=start;device.device_clock_epoch=start+600;
        CHECK(manager.saveDeviceState(device)==SAVE_OK);CHECK(manager.setActivePairForVisit(0)==SAVE_OK);
        CHECK(app::loadStartup(manager,pet)==app::Startup::Loaded&&pet.health==82);
        s.writes=0;s.written.clear();
    };
    auto blocked=[&](Store& s,SaveManager& manager,PetState& pet,DeviceState& device,GalleryData& gallery){
        CHECK(manager.recoveryRequired()&&manager.importPending());
        const auto retained=s.values;
        CHECK(!manager.shouldAutoSave(now));CHECK(manager.save(pet,now)!=SAVE_OK);
        CHECK(manager.saveGallery(gallery)!=SAVE_OK);CHECK(manager.saveDeviceState(device)!=SAVE_OK);
        CHECK(manager.loadFromSlot(1,pet)!=SAVE_OK);CHECK(s.values==retained);
    };
    for(const std::string stage:{"slot_cfg","device_v10","owner_save","owner_backup","owner_stage","guard_complete","unreadable","success"}){
        Store s;SaveManager manager;PetState pet;DeviceState device;GalleryData gallery;
        seed(s,manager,pet,device,gallery);
        if(stage=="unreadable")s.values["s0_v10"][22]^=1;
        const auto original=s.values["s0_v10"];const auto ram=encodeRam(pet,gallery);
        const auto originalDevice=device;
        if(stage=="slot_cfg"||stage=="device_v10")s.fail=stage;
        if(stage=="owner_save")s.fail="s0_v10";
        if(stage=="owner_backup")s.fail="owner_restore";
        if(stage=="owner_stage")s.fail="s0_stage";
        if(stage=="guard_complete"){s.fail="import_guard";s.failNth=2;}
        std::string output;
        app::configure({&output,nullptr,nullptr,nullptr,[](void*p,const char*line){*static_cast<std::string*>(p)+=line;}});
        const auto result=app::restoreVisitOwner(manager,pet,device,now,15);
        if(stage=="success"){
            CHECK(result==SAVE_OK&&pet.health==50&&!device.is_visiting);
            CHECK(pet.birth_timestamp==start+3600&&device.rounds==4);
            CHECK(output.find("Owner restored.")!=std::string::npos);
            CHECK(!manager.importPending()&&!manager.recoveryRequired());
            CHECK(s.values["owner_restore"]==original);
            for(int cycle=0;cycle<4;++cycle){
                CHECK(manager.save(pet,now+300*(cycle+1))==SAVE_OK);
                SaveManager boot;CHECK(boot.init()==SAVE_OK);
                CHECK(app::loadStartup(boot,pet)==app::Startup::Loaded&&pet.health==50);
                CHECK(boot.loadDeviceState(device)==SAVE_OK&&!device.is_visiting);
            }
        }else{
            CHECK(result!=SAVE_OK);CHECK(output.find("FAILED stage=")!=std::string::npos);
            CHECK(output.find("Owner restored.")==std::string::npos);
            CHECK(output.find("Force-ending")==std::string::npos);
            CHECK(encodeRam(pet,gallery)==ram);
            CHECK(device.is_visiting&&device.owner_frozen_slot==0&&device.rounds==originalDevice.rounds&&device.visit_start_epoch==originalDevice.visit_start_epoch&&device.device_clock_epoch==originalDevice.device_clock_epoch);
            if(stage!="guard_complete")CHECK(s.values["s0_v10"]==original);
            if(stage!="owner_backup"&&stage!="unreadable")CHECK(s.values["owner_restore"]==original);
            blocked(s,manager,pet,device,gallery);
            s.fail.clear();SaveManager boot;CHECK(boot.init()==SAVE_OK);
            PetState loaded=fresh();CHECK(app::loadStartup(boot,loaded)==app::Startup::Recovery);
            blocked(s,boot,pet,device,gallery);
            const auto retained=s.values;
            CHECK(app::restoreVisitOwner(boot,pet,device,now,15)!=SAVE_OK&&s.values==retained);
            if(stage!="unreadable"){
                SaveHeader header;CHECK(boot.exportSlotRaw(0,header,loaded)==SAVE_OK&&loaded.health==50);
            }
        }
        DeviceState persisted;CHECK(manager.loadDeviceState(persisted)==SAVE_OK);
        std::cout<<"RESTORE "<<stage<<" result="<<unsigned(result)<<" RAM_HP="<<pet.health
                 <<" RAM_visit="<<device.is_visiting<<" NVS_visit="<<persisted.is_visiting
                 <<" restart="<<(stage=="success"?"Owner":"Recovery")<<'\n';
        if(stage!="success")std::cout<<output.substr(output.find("[Visit] FAILED"))<<'\n';
        app::configure({});
    }
    // A retained owner backup alone must not be mistaken for an empty device.
    {
        Store s;SaveManager manager;PetState pet;DeviceState device;GalleryData gallery;
        seed(s,manager,pet,device,gallery);
        CHECK(app::restoreVisitOwner(manager,pet,device,now,15)==SAVE_OK);
        const auto backup=s.values["owner_restore"];
        s.values.clear();s.values["owner_restore"]=backup;
        SaveManager boot;CHECK(boot.init()==SAVE_OK&&boot.hasSave());
        CHECK(app::loadStartup(boot,pet)==app::Startup::Recovery);
        CHECK(!boot.shouldAutoSave(now));CHECK(boot.save(pet,now)!=SAVE_OK);
        CHECK(boot.saveGallery(gallery)!=SAVE_OK&&boot.saveDeviceState(device)!=SAVE_OK);
        CHECK(s.values["owner_restore"]==backup);
    }
    const std::vector<std::string> steps={"import_guard","owner_restore","slot_cfg","device_v10","s0_stage","s0_v10","import_guard"};
    for(size_t cut=1;cut<=steps.size();++cut){
        Store s;SaveManager manager;PetState pet;DeviceState device;GalleryData gallery;
        seed(s,manager,pet,device,gallery);
        const auto original=s.values["s0_v10"],ram=encodeRam(pet,gallery);
        s.cutAfter=int(cut);bool interrupted=false;std::string output;
        app::configure({&output,nullptr,nullptr,nullptr,[](void*p,const char*line){*static_cast<std::string*>(p)+=line;}});
        try{app::restoreVisitOwner(manager,pet,device,now,15);}catch(const PowerLoss&){interrupted=true;}
        CHECK(interrupted&&s.writes==int(cut));CHECK(s.written.back()==steps[cut-1]);
        CHECK(encodeRam(pet,gallery)==ram&&device.is_visiting);
        CHECK(output.find("Owner restored.")==std::string::npos);
        if(cut<6)CHECK(s.values["s0_v10"]==original);
        if(cut>=2)CHECK(s.values["owner_restore"]==original);
        s.cutAfter=0;SaveManager boot;CHECK(boot.init()==SAVE_OK);PetState loaded=fresh();
        if(cut<steps.size()){
            CHECK(app::loadStartup(boot,loaded)==app::Startup::Recovery);
            blocked(s,boot,pet,device,gallery);
        }else{
            CHECK(app::loadStartup(boot,loaded)==app::Startup::Loaded&&loaded.health==50);
            CHECK(boot.loadDeviceState(device)==SAVE_OK&&!device.is_visiting);
            CHECK(boot.save(loaded,now+300)==SAVE_OK);
            SaveManager again;CHECK(again.init()==SAVE_OK);
            CHECK(app::loadStartup(again,loaded)==app::Startup::Loaded&&loaded.health==50);
        }
        SaveHeader header;CHECK(boot.exportSlotRaw(0,header,loaded)==SAVE_OK&&loaded.health==50);
        std::cout<<"POWER_CUT after="<<cut<<':'<<steps[cut-1]<<" restart="
                 <<(cut<steps.size()?"Recovery":"Owner")<<" original_owner="<<(cut>=2?"backup retained":"slot retained")<<'\n';
        app::configure({});
    }
}

void permutations(){
    for(uint8_t owner=0;owner<3;++owner)for(bool reverse:{false,true}){
        Store s;attach(s);SaveManager m;CHECK(m.init()==SAVE_OK);auto pet=fresh();
        GalleryData gallery;gallery.init();DeviceState device;device.init();
        CHECK(m.setActivePair(owner,(owner+1)%3)==SAVE_OK);
        pet.health=50;CHECK(m.save(pet,1700000000)==SAVE_OK);
        // Rotation resolves an empty-pair tie by physical slot number, not by
        // active-pair argument order. Explicitly place the newest owner record.
        SaveHeader seedHeader{};seedHeader.save_time=1700000000;
        uint8_t ownerBytes[engine::MaxSaveBytes];size_t ownerSize=0;PetState ownerPet;
        CHECK(app::encodeSave(pet,seedHeader,nullptr,ownerBytes,sizeof(ownerBytes),ownerSize));
        CHECK(app::decodeSave(ownerBytes,ownerSize,ownerPet,seedHeader)==engine::DecodeError::Ok);
        CHECK(m.importSlotRaw(owner,seedHeader,ownerPet,true)==SAVE_OK);
        const auto original=s.values["s"+std::to_string(owner)+"_v10"];
        auto donor=fresh();donor.health=71;SaveHeader h{};h.sequence=9;h.save_time=1700000000;
        uint8_t raw[engine::MaxSaveBytes];size_t size=0;
        CHECK(app::encodeSave(donor,h,nullptr,raw,sizeof(raw),size));
        const uint8_t target=(owner+(reverse?2:1))%3;
        CHECK(app::commitImport(m,pet,device,gallery,target,raw,size,1700003600,15)==SAVE_OK);
        for(int cycle=0;cycle<6;++cycle){
            SaveManager boot;CHECK(boot.init()==SAVE_OK);
            CHECK(app::loadStartup(boot,pet)==app::Startup::Loaded&&pet.health==71+cycle);
            CHECK(boot.loadDeviceState(device)==SAVE_OK&&device.owner_frozen_slot==owner);
            CHECK(s.values["s"+std::to_string(owner)+"_v10"]==original);
            pet.health=72+cycle;CHECK(boot.save(pet,1700003900+cycle*300)==SAVE_OK);
        }
        SaveManager end;CHECK(end.init()==SAVE_OK&&end.load(pet)==SAVE_OK);
        CHECK(app::restoreVisitOwner(end,pet,device,1700007200,15)==SAVE_OK&&pet.health==50);
        for(int cycle=0;cycle<3;++cycle){
            CHECK(end.save(pet,1700007500+cycle*300)==SAVE_OK);
            SaveManager boot;CHECK(boot.init()==SAVE_OK&&app::loadStartup(boot,pet)==app::Startup::Loaded&&pet.health==50);
        }
        std::cout<<"PERMUTATION owner="<<unsigned(owner)<<" guest_first="<<unsigned(target)<<" PASS\n";
    }
}
void importInterruptions(){
    for(bool full:{false,true}){
        Store base;attach(base);SaveManager seed;CHECK(seed.init()==SAVE_OK);auto pet=fresh();
        GalleryData gallery;gallery.init();DeviceState device;device.init();
        CHECK(seed.save(pet,1700000000)==SAVE_OK);
        auto donor=fresh();donor.health=81;SaveHeader h{};h.sequence=9;h.save_time=1700000000;
        uint8_t raw[engine::MaxSaveBytes];size_t size=0;
        CHECK(app::encodeSave(donor,h,full?&gallery:nullptr,raw,sizeof(raw),size));
        Store complete=base;attach(complete);SaveManager m;CHECK(m.init()==SAVE_OK);
        complete.writes=0;complete.written.clear();
        CHECK(app::commitImport(m,pet,device,gallery,1,raw,size,1700003600,15)==SAVE_OK);
        const auto steps=complete.written;
        for(size_t cut=1;cut<=steps.size();++cut){
            Store s=base;attach(s);SaveManager manager;CHECK(manager.init()==SAVE_OK);
            auto before=fresh();DeviceState d;d.init();GalleryData g;g.init();
            s.writes=0;s.written.clear();s.cutAfter=int(cut);bool interrupted=false;
            try{app::commitImport(manager,before,d,g,1,raw,size,1700003600,15);}catch(const PowerLoss&){interrupted=true;}
            CHECK(interrupted&&s.written.back()==steps[cut-1]&&before.health==50&&!d.is_visiting);
            s.cutAfter=0;SaveManager boot;CHECK(boot.init()==SAVE_OK);PetState loaded=fresh();
            if(cut<steps.size()){
                CHECK(app::loadStartup(boot,loaded)==app::Startup::Recovery);
                const auto disk=s.values;CHECK(!boot.shouldAutoSave(1700007200));
                CHECK(boot.save(loaded,1700007200)!=SAVE_OK&&boot.saveGallery(g)!=SAVE_OK);
                CHECK(s.values==disk);
            }else{
                CHECK(app::loadStartup(boot,loaded)==app::Startup::Loaded&&loaded.health==81);
                CHECK(boot.loadDeviceState(d)==SAVE_OK&&d.is_visiting==!full);
            }
            CHECK(s.values["s0_v10"]==base.values["s0_v10"]);
            std::cout<<"IMPORT_CUT full="<<full<<" after="<<cut<<':'<<steps[cut-1]<<" PASS\n";
        }
    }
}
void serviceFaults(){
    std::string output;app::configure({&output,nullptr,nullptr,nullptr,[](void*p,const char* text){*static_cast<std::string*>(p)+=text;}});
    Store base;attach(base);SaveManager seed;CHECK(seed.init()==SAVE_OK);auto pet=fresh();
    CHECK(seed.save(pet,1700000000)==SAVE_OK);
    for(const std::string fault:{"open","read","quota","guard","backup","erase"}){
        output.clear();Store s=base;attach(s);SaveManager m;
        if(fault=="open")s.openFails=true;
        if(fault=="read")s.readFails="s0_v10";
        const auto init=m.init();
        if(fault=="open"||fault=="read"){
            CHECK((init==SAVE_OK)==(fault!="open"));
            CHECK(app::loadStartup(m,pet)==app::Startup::Recovery);
            const auto disk=s.values;CHECK(m.save(pet,1700000300)!=SAVE_OK&&s.values==disk);
        }else{
            CHECK(init==SAVE_OK&&m.load(pet)==SAVE_OK);
            DeviceState d;d.init();GalleryData g;g.init();
            if(fault=="quota"||fault=="guard"){
                auto donor=fresh();SaveHeader h{};h.save_time=1700000000;
                uint8_t raw[engine::MaxSaveBytes];size_t size=0;
                CHECK(app::encodeSave(donor,h,&g,raw,sizeof(raw),size));
                if(fault=="guard")s.fail="import_guard";
                else{size_t used=0;for(const auto& item:s.values)used+=item.second.size();s.maxBytes=used+6;}
                CHECK(app::commitImport(m,pet,d,g,1,raw,size,1700003600,15)!=SAVE_OK);
                CHECK(m.recoveryRequired()&&!m.shouldAutoSave(1700007200));
            }else if(fault=="backup"){
                d.is_visiting=true;d.owner_frozen_slot=0;d.visit_start_epoch=1700000000;
                CHECK(m.saveDeviceState(d)==SAVE_OK);s.readMismatch="owner_restore";
                CHECK(app::restoreVisitOwner(m,pet,d,1700003600,15)==SAVE_ERR_VERIFY);
                CHECK(m.recoveryRequired()&&m.importPending()&&d.is_visiting);
            }else{
                CHECK(m.beginImport(0)==SAVE_OK);m.requireRecovery();s.eraseFails=true;const auto disk=s.values;
                CHECK(m.erase()!=SAVE_OK&&m.recoveryRequired()&&s.values==disk);
            }
            CHECK(s.values["s0_v10"]==base.values["s0_v10"]);
            const auto disk=s.values;CHECK(m.save(pet,1700007200)!=SAVE_OK&&s.values==disk);
        }
        CHECK(output.find("Full backup restored")==std::string::npos&&output.find("Visit started")==std::string::npos&&output.find("Owner restored.")==std::string::npos&&output.find("All save data erased")==std::string::npos);
        const bool pending=m.importPending();const auto retained=s.values;
        s.openFails=false;s.readFails.clear();s.readMismatch.clear();s.fail.clear();s.maxBytes=SIZE_MAX;
        SaveManager reboot;CHECK(reboot.init()==SAVE_OK);
        const auto startup=app::loadStartup(reboot,pet);
        CHECK(startup==(pending?app::Startup::Recovery:app::Startup::Loaded));
        if(pending){CHECK(reboot.save(pet,1700007200)!=SAVE_OK&&s.values==retained);}
        else CHECK(pet.health==50); // guard never written => no partial operation happened
        std::cout<<"SERVICE "<<fault<<" reboot="<<(pending?"Recovery":"original owner")<<" PASS\n";
    }
    app::configure({});
}
void sequenceBoundary(){
    Store s;attach(s);SaveManager m;CHECK(m.init()==SAVE_OK);auto pet=fresh();
    SaveHeader h{};h.sequence=UINT32_MAX-1;h.save_time=1700000000;
    GalleryData g;g.init();uint8_t bytes[engine::MaxSaveBytes];size_t size=0;
    CHECK(app::encodeSave(pet,h,&g,bytes,sizeof(bytes),size));
    s.values["s0_v10"]={bytes,bytes+size};
    SaveManager boot;CHECK(boot.init()==SAVE_OK&&boot.load(pet)==SAVE_OK);
    pet.health=81;CHECK(boot.save(pet,1700000300)==SAVE_OK);
    const auto disk=s.values;
    CHECK(boot.save(pet,1700000600)!=SAVE_OK&&s.values==disk);
    SaveManager again;CHECK(again.init()==SAVE_OK&&again.load(pet)==SAVE_OK&&pet.health==81);
    CHECK(again.save(pet,1700000900)!=SAVE_OK&&s.values==disk);
}

void formatBoundary(){
    Store s;attach(s);SaveManager m;CHECK(m.init()==SAVE_OK);auto pet=fresh();
    CHECK(m.save(pet,1700000000)==SAVE_OK);DeviceState d;d.init();GalleryData g;g.init();
    uint8_t bytes[engine::MaxSaveBytes];size_t size=0;SaveHeader h{};h.save_time=1700000000;
    CHECK(app::encodeSave(pet,h,&g,bytes,sizeof(bytes),size));
    const auto original=s.values;const auto ram=encodeRam(pet,g);
    for(size_t length:{size_t(0),size_t(159),size_t(161),size_t(229),SIZE_MAX}){
        CHECK(app::commitImport(m,pet,d,g,1,bytes,length,1700003600,15)!=SAVE_OK);
        CHECK(s.values==original&&encodeRam(pet,g)==ram&&!m.importPending());
    }
    for(const auto position:{24,25,53,160})for(const auto value:{uint8_t(0),uint8_t(32),uint8_t(64),uint8_t(65),uint8_t(254),uint8_t(255)}){
        std::vector<uint8_t> changed(bytes,bytes+size);changed[position]=value;
        const auto sum=engine::recordChecksum(changed.data(),size);changed[22]=uint8_t(sum);changed[23]=uint8_t(sum>>8);
        PetState temp;SaveHeader header;GalleryData gallery;
        if(app::decodeSave(changed.data(),size,temp,header,&gallery)==engine::DecodeError::Ok)continue;
        CHECK(app::commitImport(m,pet,d,g,1,changed.data(),size,1700003600,15)!=SAVE_OK);
        CHECK(s.values==original&&encodeRam(pet,g)==ram&&!m.importPending());
    }
}

void protocolBridge(){
    Store s;attach(s);SaveManager manager;CHECK(manager.init()==SAVE_OK);auto pet=fresh();
    CHECK(manager.save(pet,1700000000)==SAVE_OK);
    DeviceState device;device.init();GalleryData gallery;gallery.init();
    app::SlotImportSession session;app::CommandLine line;uint32_t mono=0;
    app::configure({nullptr,nullptr,nullptr,nullptr,[](void*,const char* text){std::cout<<text;if(*text&&text[std::strlen(text)-1]!='\n')std::cout<<'\n';}});
    std::string input;
    while(std::getline(std::cin,input)){
        if(input.rfind("TICK ",0)==0){mono+=uint32_t(std::stoul(input.substr(5)));session.expire(mono);}
        else if(input=="FAULT gallery"){s.fail="gallery_v10";s.matched=0;}
        else if(input=="FAULT owner"){s.fail="s"+std::to_string(device.owner_frozen_slot)+"_v10";s.matched=0;}
        else if(input=="CLEAR_FAULT"){s.fail.clear();s.eraseFails=false;}
        else if(input=="FAULT erase")s.eraseFails=true;
        else if(input=="RESTART"){
            manager=SaveManager{};CHECK(manager.init()==SAVE_OK);auto state=app::loadStartup(manager,pet);
            std::cout<<"BOOT "<<(state==app::Startup::Recovery?"Recovery":state==app::Startup::Loaded?"Loaded":"NewGame")<<" HP="<<pet.health<<'\n';
            if(state==app::Startup::Loaded)manager.loadDeviceState(device);
            session.abort();line=app::CommandLine{};
        }else if(input=="RESTORE")app::restoreVisitOwner(manager,pet,device,1700007200,15);
        else if(input=="BACKUP_ONLY"){
            const auto backup=s.values["owner_restore"];s.values.clear();s.values["owner_restore"]=backup;
        }else if(input=="STATE"){
            std::cout<<"STATE hp="<<pet.health<<" recovery="<<manager.recoveryRequired()<<" pending="<<manager.importPending()<<" visiting="<<device.is_visiting<<" writes="<<s.writes<<'\n';
        }else{
            // Bridge framing is separate from the production serial command bytes.
            bool fragment=input.rfind("PART ",0)==0;
            if(fragment)input=input.substr(5);else input+='\n';
            for(char c:input)line.receive(c,mono,[&](const char* cmd){
                const auto result=app::saveProtocol(session,cmd,manager,pet,device,gallery,1700003600,15,mono);
                if(result==app::ProtocolResult::Erased){device.init();pet=fresh();gallery.init();}
            });
        }
        std::cout<<"BRIDGE_END"<<std::endl;
    }
}

}
int main(int argc,char**argv){
    try{
        std::string which=argc>1?argv[1]:"";
        if(which=="protocol"){protocolBridge();return 0;}
        if(which=="emit"){
            auto pet=fresh();GalleryData gallery{};gallery.init();gallery.unlock(pet.form);
            SaveHeader header{};header.save_time=1700000000;header.sequence=9;
            uint8_t bytes[engine::MaxSaveBytes];size_t size=0;
            CHECK(app::encodeSave(pet,header,&gallery,bytes,sizeof(bytes),size));
            std::ofstream file(argv[2],std::ios::binary);file.write(reinterpret_cast<const char*>(bytes),size);CHECK(file.good());return 0;
        }
        if(argc>2)foreignFixture=argv[2];
        if(which=="1")galleryRendering();else if(which=="4")imports();else if(which=="2")frozenOwner();else if(which=="3")sidecars();else if(which=="5")unknownSlots();else if(which=="6")restoreOwner();else if(which=="7")permutations();else if(which=="8")importInterruptions();else if(which=="9")serviceFaults();else if(which=="10")sequenceBoundary();else if(which=="11")formatBoundary();else throw std::runtime_error("unknown audit");
        std::cout<<"PASS audit "<<which<<" "<<content::Title<<": "<<checks<<" checks\n";
    }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}

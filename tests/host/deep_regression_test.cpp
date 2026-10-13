// Actual menu/UI/console/lifecycle services. Only external storage/peripherals are fake.
#define main original_integration_main
#define printStatus integration_printStatus
#include "tests/host/integration_test.cpp"
#undef printStatus
#undef main
#include "app/context.h"
char cmdBuf[64];uint8_t cmdLen=0;SlotImportSession g_slotImport;
uint64_t rtc_us_at_sleep=0;uint32_t epoch_at_sleep=0;int sleepCalls=0,timeEdits=0;
void PowerManager::setBrightness(uint8_t){} void PowerManager::setDimBrightness(uint8_t){}
void PowerManager::setDimTimeout(uint32_t){} void PowerManager::setOffTimeout(uint32_t){}
void PowerManager::saveConfig(){}
struct PowerCut{};
int cutAfter=0,writes=0;std::string readFault;bool persistentFailure=false;
engine::KeyValue originalPort;
std::vector<std::string> writeKeys;
void instrument(){
    originalPort=engine::keyValue;cutAfter=writes=0;writeKeys.clear();readFault.clear();persistentFailure=false;
    engine::keyValue.read=[](void*c,const char*k,void*b,size_t n)->size_t {
        if(readFault==k)return 0;
        return originalPort.read(c,k,b,n);
    };
    engine::keyValue.write=[](void*c,const char*k,const void*b,size_t n)->size_t {
        if(persistentFailure && writeFault==k)return 0;
        auto size=originalPort.write(c,k,b,n);
        if(size==n){writeKeys.push_back(k);if(++writes==cutAfter)throw PowerCut{};}
        return size;
    };
}
void ready(){
    animQueue.clear();hostMillis=0;setup();waitingForTimeSet=true;reinitPetAfterInitialTimeConfirm=true;
    CHECK(gameCallbacks.onInitialTimeConfirm(2024,1,1,8,0)==TimeConfirmResult::Accepted);
    drain();tick(5000);menuController.switchContext(UI_IDLE);DisplayManager::switchPage(PAGE_IDLE);
    Serial.output.clear();instrument();
}
app::Startup reboot(){
    writeFault.clear();readFault.clear();persistentFailure=false;cutAfter=0;
    saveManager=SaveManager{};CHECK(saveManager.init()==SAVE_OK);
    const auto result=app::loadStartup(saveManager,pet);
    if(result==app::Startup::Loaded){CHECK(saveManager.loadDeviceState(deviceState)==SAVE_OK);restoreLoadedClock();}
    return result;
}
void destroyButtons(){
    for(auto pin:BTN_PIN_MAP)hostPins[pin]=LOW;frame(31);frame(BTN_DESTROY_HOLD_MS+1);
    for(auto pin:BTN_PIN_MAP)hostPins[pin]=HIGH;frame(31);
    CHECK(menuController.getCurrentContext()==UI_DESTROY_CONFIRM);press(BTN_L);press(BTN_M);
}
uint8_t newest(){
    uint8_t found=0xff;uint32_t seq=0;
    for(uint8_t i=0;i<3;++i){SaveHeader h;SaveResult status;if(saveManager.getSlotStatus(i,h,status)&&(found==0xff||h.sequence>seq)){found=i;seq=h.sequence;}}
    CHECK(found!=0xff);return found;
}
void makeVisit(){
    auto donor=pet;donor.health=71;SaveHeader h{};h.save_time=timeManager.now();
    uint8_t bytes[engine::MaxSaveBytes];size_t size=0;CHECK(app::encodeSave(donor,h,nullptr,bytes,sizeof(bytes),size));
    CHECK(app::commitImport(saveManager,pet,deviceState,gallerySystem.getData(),uint8_t((newest()+1)%3),bytes,size,timeManager.now(),timeManager.getDay())==SAVE_OK);
    drain();tick(5000);menuController.switchContext(UI_IDLE);DisplayManager::switchPage(PAGE_IDLE);
}
std::vector<uint8_t> exportCommand(const std::string& command) {
    Serial.output.clear();processCommand(command.c_str());
    CHECK(Serial.output.find("SAVE_EXPORT_END")!=std::string::npos);
    std::vector<uint8_t> bytes;size_t start=0;
    while((start=Serial.output.find("SAVE_EXPORT_DATA ",start))!=std::string::npos){
        start+=17;const auto end=Serial.output.find('\n',start);
        auto hex=Serial.output.substr(start,end-start);if(!hex.empty()&&hex.back()=='\r')hex.pop_back();
        uint8_t data[engine::MaxSaveBytes];size_t size=0;
        CHECK(decodeHexToBytes(hex.c_str(),data,sizeof(data),&size));bytes.insert(bytes.end(),data,data+size);
        start=end;
    }
    CHECK(bytes.size()==engine::FullBytes);return bytes;
}
void importCommand(const std::vector<uint8_t>& bytes) {
    processCommand("SAVE_IMPORT_BEGIN 0");
    for(size_t start=0;start<bytes.size();start+=16){
        std::string command="SAVE_IMPORT_DATA ";char hex[3];
        for(size_t i=start;i<bytes.size()&&i<start+16;++i){std::snprintf(hex,sizeof(hex),"%02X",bytes[i]);command+=hex;}
        processCommand(command.c_str());
    }
    processCommand("SAVE_IMPORT_COMMIT");
}
void clockProgress(){
    for(bool visiting:{false,true}) {
        ready();if(visiting)makeVisit();
        const auto oldClock=deviceState.device_clock_epoch;
        std::vector<uint8_t> frozen;if(visiting)frozen=disk["s"+std::to_string(deviceState.owner_frozen_slot)+"_v10"];
        for(int i=0;i<720;++i){
            hostMillis+=120000;menuController.injectButton(BTN_R,BTN_EVENT_PRESS);
            processElapsedTime();drain();tick(5000);
            CHECK(!saveManager.shouldAutoSave(timeManager.now()));
        }
        const auto age=pet.age_days;const auto checkpoint=saveManager.getLastSaveTime();
        CHECK(age>=1&&checkpoint>oldClock+86400);
        DeviceState stored;CHECK(saveManager.loadDeviceState(stored)==SAVE_OK && stored.device_clock_epoch==oldClock);
        Serial.output.clear();processCommand("load");
        CHECK(timeManager.now()==checkpoint&&pet.age_days==age&&Serial.output.find("[Load] OK")!=std::string::npos);
        CHECK(reboot()==app::Startup::Loaded);CHECK(timeManager.now()==checkpoint&&pet.age_days==age);
        const auto target=timeManager.epochToTimeInfo(checkpoint+60);
        waitingForTimeSet=true;reinitPetAfterInitialTimeConfirm=false;
        CHECK(gameCallbacks.onInitialTimeConfirm(target.year,target.month,target.day,target.hour,target.minute)==TimeConfirmResult::Accepted);
        CHECK(pet.age_days==age&&timeManager.now()>=checkpoint);
        // Repeat a power-loss confirmation at the same wall clock: no second day.
        CHECK(reboot()==app::Startup::Loaded);waitingForTimeSet=true;
        CHECK(gameCallbacks.onInitialTimeConfirm(target.year,target.month,target.day,target.hour,target.minute)==TimeConfirmResult::Accepted);
        CHECK(pet.age_days==age);
        if(visiting)CHECK(disk["s"+std::to_string(deviceState.owner_frozen_slot)+"_v10"]==frozen);
    }
    // Every actual auto-save durable step: stage, committed pet, device sidecar.
    for(bool visiting:{false,true})for(int cut=1;cut<=3;++cut){
        ready();if(visiting)makeVisit();const auto oldEpoch=saveManager.getLastSaveTime();
        const auto age=pet.age_days;timeManager.advanceSeconds(120);const auto newEpoch=timeManager.now();
        writes=0;cutAfter=cut;bool interrupted=false;
        try{autoSaveCurrentPet();}catch(PowerCut&){interrupted=true;}
        CHECK(interrupted);CHECK(reboot()==app::Startup::Loaded);
        CHECK(timeManager.now()==(cut==1?oldEpoch:newEpoch)&&pet.age_days==age&&deviceState.is_visiting==visiting);
    }
    // If latest snapshot is unreadable, its newer sidecar cannot advance older rules.
    ready();timeManager.advanceSeconds(120);CHECK(autoSaveCurrentPet()==SAVE_OK);
    const auto latest=newest();disk["s"+std::to_string(latest)+"_v10"][22]^=1;
    CHECK(reboot()==app::Startup::Loaded);CHECK(timeManager.now()==saveManager.getLastSaveTime());
    std::cout<<"PASS D1 normal/visitor 27h interactions, normal/power restart, save cuts, fallback checkpoint\n";
}
void timeOperationCuts(){
    for(bool visiting:{false,true}){
        ready();if(visiting)makeVisit();
        waitingForTimeSet=true;reinitPetAfterInitialTimeConfirm=false;
        writes=0;CHECK(gameCallbacks.onInitialTimeConfirm(2024,1,2,8,1)==TimeConfirmResult::Accepted);
        const auto total=writes;CHECK(total>=8);
        for(int cut=1;cut<=total;++cut){
            ready();if(visiting)makeVisit();
            waitingForTimeSet=true;reinitPetAfterInitialTimeConfirm=false;writes=0;cutAfter=cut;bool interrupted=false;
            try{gameCallbacks.onInitialTimeConfirm(2024,1,2,8,1);}catch(PowerCut&){interrupted=true;}
            CHECK(interrupted);const auto result=reboot();
            CHECK(result==(cut==total?app::Startup::Loaded:app::Startup::Recovery));
            if(cut==total){const auto age=pet.age_days;CHECK(age==1&&deviceState.is_visiting==visiting);
                waitingForTimeSet=true;CHECK(gameCallbacks.onInitialTimeConfirm(2024,1,2,8,1)==TimeConfirmResult::Accepted&&pet.age_days==age);}
        }
    }
    for(const std::string command:{"d","SET_TIME 1704182407"}){
        ready();writes=0;processCommand(command.c_str());const auto total=writes;CHECK(total>=5);
        for(int cut=1;cut<=total;++cut){
            ready();writes=0;cutAfter=cut;bool interrupted=false;
            try{processCommand(command.c_str());}catch(PowerCut&){interrupted=true;}
            CHECK(interrupted);CHECK(reboot()==(cut==total?app::Startup::Loaded:app::Startup::Recovery));
        }
    }
    std::cout<<"PASS D1 offline normal/visitor, d, explicit running set-time: every durable cut protected\n";
}
void assertRetained(uint8_t slot,const std::vector<uint8_t>& bytes){
    CHECK(disk["s"+std::to_string(slot)+"_v10"]==bytes);
    SaveHeader h;PetState p;CHECK(saveManager.exportSlotRaw(slot,h,p)==SAVE_OK&&p.health==79);
}
void resetSeed(){ready();pet.health=79;CHECK(saveManager.save(pet,timeManager.now())==SAVE_OK);}
void resetFaults(){
    const std::vector<std::string> faults={"import_guard","owner_restore","gallery_v10","device_v10","s0_stage","s0_v10","complete"};
    for(bool buttons:{false,true})for(const auto& fault:faults){
        resetSeed();const auto oldRounds=deviceState.rounds;const auto slot=newest();const auto bytes=disk["s"+std::to_string(slot)+"_v10"];
        const auto oldPet=selected::capture(pet);writeFault=fault=="complete"?"import_guard":fault;writeFaultNth=fault=="complete"?2:1;writeHits=0;
        if(buttons)destroyButtons();else processCommand("reset");
        CHECK(saveManager.recoveryRequired()&&pet.health==79&&deviceState.rounds==oldRounds);
        CHECK(selected::capture(pet).age_days==oldPet.age_days);
        CHECK(Serial.output.find("*** DESTROYED ***")==std::string::npos&&Serial.output.find("Pet reset committed")==std::string::npos);
        assertRetained(slot,bytes);
        const auto diskBefore=disk;menuController.injectButton(BTN_R,BTN_EVENT_PRESS);processCommand("save");CHECK(disk==diskBefore);
        const auto start=reboot();CHECK(start==(fault=="import_guard"?app::Startup::Loaded:app::Startup::Recovery));
        assertRetained(slot,bytes);
        if(start==app::Startup::Recovery){
            // User-facing command, not a raw NVS-only proof of recoverability.
            const auto backup=exportCommand(fault=="owner_restore"?"SAVE_EXPORT "+std::to_string(slot):"SAVE_EXPORT_OWNER");
            PetState recovered;SaveHeader header;GalleryData history;bool full=false;
            CHECK(app::decodeSave(backup.data(),backup.size(),recovered,header,&history,&full)==engine::DecodeError::Ok&&recovered.health==79&&full);
            const auto before=disk;processCommand("save");CHECK(disk==before);
            // Explicit user recovery workflow can restore the retained pet, not just expose bytes.
            processCommand("SAVE_RECOVERY_ERASE CONFIRM");importCommand(backup);
            CHECK(!saveManager.recoveryRequired()&&!waitingForTimeSet&&pet.health==79);
            CHECK(reboot()==app::Startup::Loaded&&pet.health==79);
        }else CHECK(deviceState.rounds==oldRounds&&pet.health==79);
    }
    // Enumerate all completed writes in the real service, then cut both real entries at each one.
    resetSeed();writes=0;processCommand("reset");const auto total=writes;CHECK(total==7); // guard,backup,gallery,device,stage,pet,complete
    for(bool buttons:{false,true})for(int cut=1;cut<=total;++cut){
        resetSeed();const auto rounds=deviceState.rounds;const auto slot=newest();const auto bytes=disk["s"+std::to_string(slot)+"_v10"];
        writes=0;cutAfter=cut;bool interrupted=false;
        try{if(buttons)destroyButtons();else processCommand("reset");}catch(PowerCut&){interrupted=true;}
        CHECK(interrupted);const auto start=reboot();
        CHECK(start==(cut==total?app::Startup::Loaded:app::Startup::Recovery));assertRetained(slot,bytes);
        if(cut==total)CHECK(pet.health==50&&deviceState.rounds==rounds+1);
        else CHECK(saveManager.recoveryRequired());
    }
    for(bool buttons:{false,true}){
        resetSeed();const auto rounds=deviceState.rounds;
        if(buttons)destroyButtons();else processCommand("reset");
        CHECK(!saveManager.recoveryRequired()&&pet.health==50&&deviceState.rounds==rounds+1);
        CHECK(saveManager.save(pet,timeManager.now())==SAVE_OK);CHECK(reboot()==app::Startup::Loaded&&pet.health==50&&deviceState.rounds==rounds+1);
    }
    std::cout<<"PASS D2 both reset entries, 7 faults + every durable cut, RAM/success/exports/restart\n";
}
void galleryRetry(){
    for(bool retry:{false,true}){
        ready();const auto unlocked=content::formAt(1);pet.form=unlocked;writeFault="gallery_v10";writeHits=0;
        persistCurrentFormUnlock();CHECK(saveManager.galleryPending());
        CHECK(Serial.output.find("unlock pending")!=std::string::npos);
        GalleryData g;CHECK(saveManager.loadGallery(g)==SAVE_OK&&!g.isUnlocked(unlocked));
        // Repeated pet save while gallery is failing may not write an incomplete snapshot.
        writeHits=0;persistentFailure=true;const auto before=disk;
        CHECK(saveManager.save(pet,timeManager.now())!=SAVE_OK&&disk==before);
        persistentFailure=false;writeFault.clear();
        if(retry){persistCurrentFormUnlock();CHECK(!saveManager.galleryPending());}
        CHECK(saveManager.save(pet,timeManager.now())==SAVE_OK&&!saveManager.galleryPending());
        SaveHeader h;PetState snap;GalleryData bound;bool full=false;
        CHECK(saveManager.exportSlotRawWithGallery(newest(),h,snap,bound,full)==SAVE_OK&&full&&bound.isUnlocked(unlocked));
        hostRandom=500;processCommand("reset");CHECK(!saveManager.recoveryRequired());
        CHECK(reboot()==app::Startup::Loaded);gallerySystem.init();CHECK(saveManager.loadGallery(gallerySystem.getData())==SAVE_OK&&gallerySystem.isFormUnlocked(unlocked));
        const auto exported=exportCommand("SAVE_EXPORT 0");
        CHECK(app::decodeSave(exported.data(),exported.size(),snap,h,&bound,&full)==engine::DecodeError::Ok&&full&&bound.isUnlocked(unlocked));
    }
    std::cout<<"PASS D5 transient gallery retry, pet commit gate, reset/reboot/full export\n";
}
void setupButtons(){
    ready();waitingForTimeSet=true;reinitPetAfterInitialTimeConfirm=false;
    UICallbacks observed=gameCallbacks;timeEdits=0;
    observed.onInitialTimeEdit=[](uint16_t y,uint8_t m,uint8_t d,uint8_t h,uint8_t minute,uint8_t field,bool confirm){++timeEdits;gameCallbacks.onInitialTimeEdit(y,m,d,h,minute,field,confirm);};
    menuController.init(&pet,&deviceState,&observed);
    menuController.startInitialTimeSetup(timeManager.timeInfoToEpoch(2026,1,1,8,0,0));
    for(int i=0;i<6;++i)press(BTN_M);
    CHECK(waitingForTimeSet&&!saveManager.recoveryRequired());const auto edits=timeEdits;
    CHECK(Serial.output.find("awaiting supported time")!=std::string::npos);
    for(int i=0;i<4;++i)press(BTN_R); // back to year
    for(int i=0;i<98;++i)press(BTN_L); // 2026 -> 2024, permitted 2000..2099 wrap
    CHECK(timeEdits>edits);
    for(int i=0;i<6;++i)press(BTN_M);
    CHECK(!waitingForTimeSet&&menuController.getCurrentContext()==UI_IDLE&&timeManager.getTimeInfo().year==2024);
    // Storage error yields explicit Recovery rather than a false completed setup.
    waitingForTimeSet=true;menuController.startInitialTimeSetup(timeManager.now());
    writeFault="device_v10";writeHits=0;Serial.output.clear();
    for(int i=0;i<6;++i)press(BTN_M);
    CHECK(waitingForTimeSet&&saveManager.recoveryRequired());CHECK(Serial.output.find("recovery required")!=std::string::npos);
    CHECK(Serial.output.find("Initial time confirmed")==std::string::npos);
    std::cout<<"PASS D4 button rejection -> edit -> accepted, storage failure -> Recovery\n";
}
void advanceContract(){
    for(unsigned n:{1440u,1441u,10080u,10081u}){
        ready();pet.seriousness=0;pet.idle_paused_until=0;const auto before=timeManager.now();const auto age=pet.age_days;
        const auto original=pet;
        processCommand(("t "+std::to_string(n)).c_str());
        if(n<=10080){
            CHECK(timeManager.now()==before+n*60u&&!app::runtime.commandRejected());CHECK(pet.age_days==age);
            // Direct production minute operations are the expected minute-only contract.
            auto expected=original;for(unsigned i=1;i<=n;++i)seriousnessSystem.onIdleTick(expected,before+i*60u);
            CHECK(pet.seriousness==expected.seriousness&&pet.form==expected.form&&pet.idle_minute_remainder==expected.idle_minute_remainder);
            CHECK(Serial.output.find("minute batch exceeds")==std::string::npos);
        }else CHECK(timeManager.now()==before&&pet.seriousness==0&&pet.age_days==age&&Serial.output.find("REJECTED")!=std::string::npos);
    }
    std::cout<<"PASS D6 actual t command 1440/1441/10080/10081; rule/clock matched, no new day rules\n";
}
void sleepGate(){
    for(const std::string fault:{"s0_stage","s0_v10","stage_read","record_read","device_v10","device_read","same_time_record_read","none"}){
        ready();sleepCalls=0;epoch_at_sleep=0;rtc_us_at_sleep=0;
        // ready has slot0, next write is slot1
        if(fault=="s0_stage")writeFault="s1_stage";
        if(fault=="s0_v10")writeFault="s1_v10";
        if(fault=="device_v10")writeFault=fault;
        if(fault=="stage_read")readFault="s1_stage";
        if(fault=="record_read")readFault="s1_v10";
        if(fault=="device_read")readFault="device_v10";
        if(fault=="same_time_record_read"){
            // Old snapshot has exactly the same pet/time; fallback must not masquerade as readback.
            const auto t=timeManager.epochToTimeInfo(saveManager.getLastSaveTime());
            timeManager.setSimulatedTime(t.year,t.month,t.day,t.hour,t.minute);readFault="s1_v10";
        }
        persistentFailure=true;Serial.output.clear();enterDeepSleep();
        std::cout<<"SLEEP fault="<<fault<<" calls="<<sleepCalls<<"\n";
        if(fault=="none")CHECK(sleepCalls==1&&epoch_at_sleep==timeManager.now()&&rtc_us_at_sleep!=0);
        else {
            CHECK(sleepCalls==0&&epoch_at_sleep==0&&rtc_us_at_sleep==0&&Serial.output.find("Entering deep sleep")==std::string::npos);
            if(fault=="record_read" || fault=="same_time_record_read" || fault=="device_v10" || fault=="device_read"){
                CHECK(saveManager.recoveryRequired()&&!saveManager.shouldAutoSave(timeManager.now()));
                const auto before=disk;CHECK(saveManager.save(pet,timeManager.now())!=SAVE_OK&&disk==before);
            }
        }
    }
    std::cout<<"PASS D3 actual lifecycle save/stage/record/device/readback failures abort, success sleeps once\n";
}
int main(){try{clockProgress();timeOperationCuts();resetFaults();galleryRetry();setupButtons();advanceContract();sleepGate();
    std::cout<<"PASS deep regression "<<content::Title<<" checks="<<checks<<'\n';
}catch(const std::exception&e){std::cerr<<"FAIL deep regression "<<e.what()<<'\n';return 1;}}

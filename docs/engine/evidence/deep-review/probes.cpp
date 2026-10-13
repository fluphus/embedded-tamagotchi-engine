// Defect reproductions: exit 0 means the documented defects were reproduced.
// Production rules/menu/storage/console/lifecycle are linked unchanged.
#define main original_integration_main
#define printStatus integration_printStatus
#include "tests/host/integration_test.cpp"
#undef printStatus
#undef main
#include "app/context.h"
char cmdBuf[64]; uint8_t cmdLen=0;
SlotImportSession g_slotImport;
uint64_t rtc_us_at_sleep=0; uint32_t epoch_at_sleep=0;
int sleepCalls=0;
int timeEdits=0;
void PowerManager::setBrightness(uint8_t){}
void PowerManager::setDimBrightness(uint8_t){}
void PowerManager::setDimTimeout(uint32_t){}
void PowerManager::setOffTimeout(uint32_t){}
void PowerManager::saveConfig(){}
void ready(){
    animQueue.clear();hostMillis=0;setup();waitingForTimeSet=true;reinitPetAfterInitialTimeConfirm=true;
    gameCallbacks.onInitialTimeConfirm(2024,1,1,8,0);drain();tick(5000);
    menuController.switchContext(UI_IDLE);DisplayManager::switchPage(PAGE_IDLE);
}
void restart(){writeFault.clear();saveManager=SaveManager{};CHECK(saveManager.init()==SAVE_OK);CHECK(app::loadStartup(saveManager,pet)==app::Startup::Loaded);CHECK(saveManager.loadDeviceState(deviceState)==SAVE_OK);}
void destroyButtons(){
    for(auto pin:BTN_PIN_MAP)hostPins[pin]=LOW;frame(31);frame(BTN_DESTROY_HOLD_MS+1);
    for(auto pin:BTN_PIN_MAP)hostPins[pin]=HIGH;frame(31);
    CHECK(menuController.getCurrentContext()==UI_DESTROY_CONFIRM);press(BTN_L);press(BTN_M);
}
void probeClock(){
    ready();const auto anchor=deviceState.device_clock_epoch;
    for(int i=0;i<720;++i){
        hostMillis+=120000;menuController.injectButton(BTN_R,BTN_EVENT_PRESS);
        if(timeManager.checkNewMinute())seriousnessSystem.onIdleTick(pet,timeManager.now());
        if(timeManager.checkNewDay())doDayEnd();
        drain();tick(5000);CHECK(!saveManager.shouldAutoSave(timeManager.now()));
    }
    SaveHeader h;PetState saved;uint8_t a=saveManager.getActiveSlotA(),b=saveManager.getActiveSlotB();
    CHECK(saveManager.exportSlotRaw(a,h,saved)==SAVE_OK);uint32_t newest=h.save_time;
    if(saveManager.exportSlotRaw(b,h,saved)==SAVE_OK)newest=std::max(newest,h.save_time);
    DeviceState ds;CHECK(saveManager.loadDeviceState(ds)==SAVE_OK);
    CHECK(ds.device_clock_epoch==anchor&&newest>anchor+600);
    std::cout<<"REPRO clock: latest_pet_minus_device="<<newest-ds.device_clock_epoch<<" seconds; interaction saves keep autosave deferred\n";
    auto current=timeManager.getTimeInfo();const auto beforeAge=pet.age_days;
    restart();waitingForTimeSet=true;reinitPetAfterInitialTimeConfirm=false;
    gameCallbacks.onInitialTimeConfirm(current.year,current.month,current.day,current.hour,current.minute);
    CHECK(pet.age_days>beforeAge);
    std::cout<<"REPRO clock reboot at same wall time: age "<<beforeAge<<" -> "<<pet.age_days<<" (already settled day applied again)\n";
}
void probeReset(){
    for(bool button:{false,true})for(bool deviceFailure:{false,true}){
        ready();pet.health=79;CHECK(saveManager.save(pet,timeManager.now())==SAVE_OK);
        const auto rounds=deviceState.rounds;
        writeFault=deviceFailure?"device_v10":"s0_v10";writeHits=0;
        if(button)destroyButtons();else doReset();
        const auto ramRounds=deviceState.rounds;const auto ramHp=pet.health;const bool blocked=saveManager.recoveryRequired();
        restart();
        if(deviceFailure&&button)CHECK(deviceState.rounds==rounds&&pet.health==ramHp&&!blocked);
        if(!deviceFailure)CHECK(deviceState.rounds==ramRounds&&pet.health==79);
        std::cout<<"REPRO reset: button="<<button<<" device_failure="<<deviceFailure<<" RAM rounds="<<ramRounds<<" HP="<<ramHp<<" restart rounds="<<deviceState.rounds<<" HP="<<pet.health<<" runtime_recovery="<<blocked<<" reboot=Loaded\n";
    }
}
void probeGallery(){
    ready();const auto form=content::formAt(1);CHECK(!gallerySystem.isFormUnlocked(form));
    pet.form=form;writeFault="gallery_v10";writeHits=0;persistCurrentFormUnlock();writeFault.clear();
    persistCurrentFormUnlock();CHECK(gallerySystem.isFormUnlocked(form));
    GalleryData persisted;CHECK(saveManager.loadGallery(persisted)==SAVE_OK&&!persisted.isUnlocked(form));
    CHECK(saveManager.save(pet,timeManager.now())==SAVE_OK);
    SaveHeader h;PetState p;GalleryData bound;bool full;
    for(uint8_t i=0;i<2;++i)if(saveManager.exportSlotRawWithGallery(i,h,p,bound,full)==SAVE_OK)CHECK(!bound.isUnlocked(form));
    hostRandom=500;doReset();restart();gallerySystem.init();CHECK(saveManager.loadGallery(gallerySystem.getData())==SAVE_OK);gallerySystem.unlockForm(pet.form);
    CHECK(!gallerySystem.isFormUnlocked(form));
    std::cout<<"REPRO gallery: RAM unlock retry skipped; normal reset/reboot permanently loses historical unlock\n";
}
void probeTimeUI(){
    ready();waitingForTimeSet=true;reinitPetAfterInitialTimeConfirm=false;
    UICallbacks observed=gameCallbacks;timeEdits=0;
    observed.onInitialTimeEdit=[](uint16_t y,uint8_t m,uint8_t d,uint8_t h,uint8_t minute,uint8_t field,bool confirm){++timeEdits;gameCallbacks.onInitialTimeEdit(y,m,d,h,minute,field,confirm);};
    menuController.init(&pet,&deviceState,&observed);
    auto future=timeManager.timeInfoToEpoch(2026,1,1,8,0,0);
    menuController.startInitialTimeSetup(future);
    for(int i=0;i<6;++i)press(BTN_M);
    CHECK(waitingForTimeSet&&!saveManager.recoveryRequired());
    CHECK(Serial.output.find("awaiting supported time")!=std::string::npos);
    const int edits=timeEdits;Serial.output.clear();for(int i=0;i<3;++i){press(BTN_R);press(BTN_L);press(BTN_M);}
    CHECK(timeEdits==edits&&menuController.getCurrentContext()==UI_TIME_SETUP);
    std::cout<<"REPRO time UI: capacity rejected; all subsequent edit/navigation buttons ignored\n";
}
void probeAdvance(){
    ready();pet.seriousness=0;pet.idle_paused_until=0;auto before=timeManager.now();
    Serial.output.clear();processCommand("t 1441");
    CHECK(timeManager.now()==before+1441*60&&pet.seriousness==0);
    CHECK(Serial.output.find("minute batch exceeds 1440")!=std::string::npos);
    std::cout<<"REPRO t: clock advanced 1441 minutes while engine rejected batch; SR unchanged\n";
}
void probeSleep(){
    ready();tick(120000);writeFault="device_v10";writeHits=0;sleepCalls=0;
    enterDeepSleep();CHECK(saveManager.recoveryRequired()&&sleepCalls==1);
    std::cout<<"REPRO sleep: device persistence failed, recovery set, deep_sleep_start still called\n";
}
int main(){try{probeClock();probeReset();probeGallery();probeTimeUI();probeAdvance();probeSleep();
    std::cout<<"REPRODUCED "<<content::Title<<" checks="<<checks<<" (defects present, not acceptance PASS)\n";
}catch(const std::exception& e){std::cerr<<"Probe failed: "<<e.what()<<'\n';return 1;}}

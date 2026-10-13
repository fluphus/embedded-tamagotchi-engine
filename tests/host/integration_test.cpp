#include "core/power_manager.h"
#include "app/workflow_services.h"
#include "app/startup.h"
#include "app/import_workflow.h"
#include "engine/storage.h"
#include "presentation/animation_director.h"
#include <map>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <chrono>
uint32_t hostMillis=0,hostRandom=0;int hostPins[64];HostSerial Serial;
namespace engine {KeyValue keyValue;}
PetState pet;DeviceState deviceState;
bool waitingForTimeSet=true,reinitPetAfterInitialTimeConfirm=true;
void printStatus(){} // Serial status output is unrelated to tested state transitions.
// No sleep/power hardware is used by these tests.
PowerManager powerManager;
void PowerManager::onUserActivity(){}
int checks=0;
#define CHECK(x) do{++checks;if(!(x))throw std::runtime_error(std::to_string(__LINE__)+" " #x);}while(false)
std::map<std::string,std::vector<uint8_t>> disk;
std::string writeFault;int writeFaultNth=1,writeHits=0;bool openFault=false;
void setup(){
    writeFault.clear();writeFaultNth=1;writeHits=0;openFault=false;
    for(auto& pin:hostPins)pin=HIGH;
    engine::keyValue={nullptr,[](void*,const char*,bool){return !openFault;},
        [](void*,const char*k){return disk.count(k)!=0;},
        [](void*,const char*k)->size_t{return disk.count(k)?disk[k].size():0;},
        [](void*,const char*k,void* b,size_t n)->size_t{if(!disk.count(k)||disk[k].size()>n)return 0;std::memcpy(b,disk[k].data(),disk[k].size());return disk[k].size();},
        [](void*,const char*k,const void* b,size_t n)->size_t{if(writeFault==k&&++writeHits==writeFaultNth)return 0;auto*p=static_cast<const uint8_t*>(b);disk[k]={p,p+n};return n;},
        [](void*){disk.clear();return true;}};
    app::configure({nullptr,[](void*)->engine::EpochSeconds{return {timeManager.now()};},[](void*)->engine::MonotonicMillis{return {hostMillis};},[](void*){return hostRandom;},[](void*,const char* line){Serial.println(line);}});
    disk.clear();saveManager=SaveManager{};CHECK(saveManager.init()==SAVE_OK);
    timeManager.init();gallerySystem.init();deviceState.init();
    app::initializePet(pet,1700000000);
    menuController.init(&pet,&deviceState,&gameCallbacks);DisplayManager::init();
}
void tick(uint32_t delta){hostMillis+=delta;DisplayManager::update(hostMillis);DisplayManager::renderIfDirty();}
void drain(){for(int i=0;i<200&&animQueue.isPlaying()&&!animQueue.isWaitingInput();++i)tick(10000);}
void calendar(){
    const uint16_t dates[][6]={{2024,1,31,2024,2,1},{2023,12,31,2024,1,1},{2024,2,28,2024,2,29},{2024,2,29,2024,3,1},{2023,2,28,2023,3,1}};
    for(const auto& d:dates){
        timeManager.setSimulatedTime(d[0],d[1],d[2],23,59);auto before=pet.age_days;
        timeManager.advanceSeconds(60);CHECK(timeManager.checkNewDay());doDayEnd();
        const auto t=timeManager.getTimeInfo();CHECK(t.year==d[3]&&t.month==d[4]&&t.day==d[5]);
        CHECK(pet.age_days==before+1&&!timeManager.checkNewDay());
    }
    // Actual initial-time callback and normal reboot/offline callback.
    setup();gameCallbacks.onInitialTimeConfirm(2024,2,28,23,59);
    CHECK(!waitingForTimeSet&&!reinitPetAfterInitialTimeConfirm&&pet.birth_timestamp==timeManager.now());
    GalleryData initialGallery;CHECK(saveManager.loadGallery(initialGallery)==SAVE_OK&&initialGallery.isUnlocked(pet.form));
    PetState initialPet;SaveHeader initialHeader;GalleryData boundGallery;bool bound=false;
    CHECK(saveManager.exportSlotRawWithGallery(0,initialHeader,initialPet,boundGallery,bound)==SAVE_OK&&bound&&boundGallery.isUnlocked(pet.form));
    CHECK(saveManager.save(pet,timeManager.now())==SAVE_OK);
    CHECK(persistDeviceState()==SAVE_OK);
    SaveManager boot;CHECK(boot.init()==SAVE_OK&&app::loadStartup(boot,pet)==app::Startup::Loaded);
    CHECK(boot.loadDeviceState(deviceState)==SAVE_OK);const auto age=pet.age_days;
    waitingForTimeSet=true;gameCallbacks.onInitialTimeConfirm(2024,3,1,0,0);
    CHECK(pet.age_days==age+1&&!waitingForTimeSet);
    // Existing hour-step minute detector semantics are intentionally unchanged.
    timeManager.setSimulatedTime(2024,1,1,0,0);timeManager.advanceMinutes(60);
    CHECK(!timeManager.checkNewMinute());
}
void rollover(){
    hostMillis=UINT32_MAX-2000;timeManager.setSimulatedTime(2024,1,1,0,0);const auto start=timeManager.now();
    pet.seriousness=50;
    auto poke=seriousnessSystem.onInteract(pet,INTERACT_POKE,start);CHECK(poke.seriousness_after<poke.seriousness_before);
    app::runtime.markSaved();AnimNode node;node.durationMs=4000;animQueue.clear();CHECK(animQueue.enqueue(node));
    tick(3999);CHECK(timeManager.now()==start+3&&animQueue.isPlaying()&&!saveManager.shouldAutoSave(start));
    tick(1);CHECK(timeManager.now()==start+4&&!animQueue.isPlaying());
    poke=seriousnessSystem.onInteract(pet,INTERACT_POKE,timeManager.now());CHECK(poke.seriousness_after==poke.seriousness_before);
    tick(296000);CHECK(saveManager.shouldAutoSave(start+300));

    // Advance repeatedly across a full 32-bit period; no anchored clock reversal.
    uint32_t previous=timeManager.now();
    for(int i=0;i<50;++i){hostMillis+=100000000;const auto now=timeManager.now();CHECK(now>=previous);previous=now;}
    // Deadline near wrap must not expire before the wrap.
    hostMillis=UINT32_MAX-1000;FeedOutcome outcome{};DisplayManager::showFeedResult(outcome,0);
    tick(500);CHECK(DisplayManager::getCurrentPage()==PAGE_FEED_RESULT);
    tick(PAGE_DURATION_FEED_RESULT-500);CHECK(DisplayManager::getCurrentPage()==PAGE_IDLE);
}
void offlineCapacity(){
    setup();timeManager.setSimulatedTime(2024,1,1,0,0);
    const auto begin=std::chrono::steady_clock::now();skipTime(app::MaxOfflineSeconds);
    const auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count();
    CHECK(pet.age_days==366);const auto state=selected::capture(pet);const auto before=disk;
    skipTime(app::MaxOfflineSeconds+1);CHECK(selected::capture(pet).age_days==state.age_days&&disk==before);
    waitingForTimeSet=true;reinitPetAfterInitialTimeConfirm=false;deviceState.device_clock_epoch=timeManager.now();
    const auto clock=timeManager.now();const auto diskBefore=disk;
    auto future=timeManager.epochToTimeInfo(clock+app::MaxOfflineSeconds+86400);
    gameCallbacks.onInitialTimeConfirm(future.year,future.month,future.day,future.hour,future.minute);
    CHECK(waitingForTimeSet&&timeManager.now()==clock&&disk==diskBefore);
    engine::Command c;c.kind=engine::CommandKind::AdvanceMinutes;c.minutes=UINT32_MAX;c.time={timeManager.now()};
    app::execute(pet,c);CHECK(app::runtime.commandRejected()&&selected::capture(pet).age_days==state.age_days);
    CHECK(!app::offlineSupported(86400,UINT16_MAX,1700000000));
    CHECK(!app::offlineSupported(86400,1,UINT32_MAX-100));
    pet.age_days=UINT16_MAX;const auto diskAtMax=disk;const auto hpAtMax=pet.health;
    doDayEnd();CHECK(pet.age_days==UINT16_MAX&&pet.health==hpAtMax&&disk==diskAtMax);
    app::growDay(pet);CHECK(pet.age_days==UINT16_MAX);
    app::settleDay(pet,timeManager.now(),timeManager.getDay());CHECK(pet.age_days==UINT16_MAX&&pet.health==hpAtMax);
    std::cout<<"MAX_OFFLINE seconds="<<app::MaxOfflineSeconds<<" days="<<state.age_days<<" elapsed_ms="<<ms<<" (host only)\n";
}
void timeSetupFaults(){
    for(const std::string fault:{"open","recovery","guard_first","device","gallery","final_save","guard_complete"}){
        setup();waitingForTimeSet=true;reinitPetAfterInitialTimeConfirm=true;Serial.output.clear();
        if(fault=="open"){openFault=true;saveManager=SaveManager{};CHECK(saveManager.init()==SAVE_ERR_NVS_INIT);}
        if(fault=="recovery")saveManager.requireRecovery();
        if(fault=="guard_first"||fault=="guard_complete"){writeFault="import_guard";writeFaultNth=fault=="guard_first"?1:2;}
        if(fault=="device")writeFault="device_v10";
        if(fault=="gallery")writeFault="gallery_v10";
        if(fault=="final_save")writeFault="s0_v10";
        gameCallbacks.onInitialTimeConfirm(2024,1,1,8,0);
        CHECK(waitingForTimeSet&&saveManager.recoveryRequired());
        CHECK(Serial.output.find("Initial time confirmed, entering normal operation.")==std::string::npos);
        const auto retained=disk;CHECK(saveManager.save(pet,1700007200)!=SAVE_OK&&disk==retained);
        openFault=false;writeFault.clear();SaveManager reboot;CHECK(reboot.init()==SAVE_OK);
        auto loaded=pet;const auto startup=app::loadStartup(reboot,loaded);
        const bool wroteGuard=fault=="device"||fault=="gallery"||fault=="final_save"||fault=="guard_complete";
        CHECK(startup==(wroteGuard?app::Startup::Recovery:app::Startup::NewGame));
        std::cout<<"TIME_SETUP_FAULT "<<fault<<" no_success=1 writes_blocked=1 restart="<<(wroteGuard?"Recovery":"original empty device")<<'\n';
    }
}

void visitTime(){
    for(bool callback:{false,true}){
        setup();timeManager.setSimulatedTime(2024,1,1,8,0);const auto start=timeManager.now();
        app::initializePet(pet,start);pet.health=50;CHECK(saveManager.save(pet,start)==SAVE_OK);
        auto donor=pet;donor.health=71;SaveHeader h{};h.save_time=start;
        uint8_t raw[engine::MaxSaveBytes];size_t size=0;
        CHECK(app::encodeSave(donor,h,nullptr,raw,sizeof(raw),size));
        CHECK(app::commitImport(saveManager,pet,deviceState,gallerySystem.getData(),1,raw,size,start,1)==SAVE_OK);
        const auto owner=disk["s0_v10"];
        waitingForTimeSet=true;reinitPetAfterInitialTimeConfirm=false;
        gameCallbacks.onInitialTimeConfirm(2024,1,2,8,0);
        CHECK(deviceState.is_visiting&&disk["s0_v10"]==owner&&pet.age_days==1);
        if(callback)gameCallbacks.onDestroyExecuted(pet.form);else doReset();
        CHECK(!deviceState.is_visiting&&pet.health==50&&pet.birth_timestamp==start+86400);
        SaveManager boot;CHECK(boot.init()==SAVE_OK&&app::loadStartup(boot,pet)==app::Startup::Loaded&&pet.health==50);
    }
}

void frame(uint32_t ms){hostMillis+=ms;menuController.update();DisplayManager::update(hostMillis);DisplayManager::renderIfDirty();}
void press(ButtonId button){hostPins[BTN_PIN_MAP[button]]=LOW;frame(31);hostPins[BTN_PIN_MAP[button]]=HIGH;frame(31);}
void inputs(){
    setup();timeManager.setSimulatedTime(2024,1,1,8,0);tick(5000);
#if !defined(CONTENT_SPROUT) || !CONTENT_SPROUT
    // Select an injected random word whose real draw contains three healthy foods.
    bool found=false;
    for(hostRandom=0;hostRandom<10000;++hostRandom){auto draw=feedingSystem.drawFood();int count=0;for(auto id:draw.food_ids)count+=FOOD_TABLE[id].is_healthy;if(count>=3){found=true;break;}}
    CHECK(found);
#endif
    press(BTN_L);CHECK(menuController.getCurrentContext()==UI_FEED_DRAW);
    for(int i=0;i<4;++i){press(BTN_M);}
    CHECK(pet.daily_feed.feed_count==0);
    tick(5000);CHECK(menuController.getCurrentContext()==UI_FEED_PICK);
    const auto draw=menuController.getFeedState().draw;
    std::vector<uint8_t> wanted;
    for(uint8_t i=0;i<4&&wanted.size()<3;++i){
#if defined(CONTENT_SPROUT) && CONTENT_SPROUT
        if(FOOD_TABLE[draw.food_ids[i]].is_healthy)wanted.push_back(i);
#else
        if(FOOD_TABLE[draw.food_ids[i]].is_healthy)wanted.push_back(i);
#endif
    }
    CHECK(wanted.size()==3);
    for(auto index:wanted){while(menuController.getFeedState().cursor!=index)press(BTN_R);press(BTN_M);}
    CHECK(pet.daily_feed.feed_count==1);auto hp=pet.health;
    const auto blockedContext=menuController.getCurrentContext();
    CHECK(DisplayManager::isPageBlockingInput());
    for(int i=0;i<3;++i)menuController.injectButton(BTN_M,BTN_EVENT_PRESS);
    CHECK(menuController.getCurrentContext()==blockedContext&&pet.health==hp&&pet.daily_feed.feed_count==1);
    for(int i=0;i<4;++i)press(BTN_L);
    CHECK(pet.daily_feed.feed_count==1&&pet.health==hp);
    drain();
    CHECK(animQueue.isWaitingInput()&&menuController.getCurrentContext()==UI_SPECIAL_FOOD);
    CHECK(!DisplayManager::isPageBlockingInput());
    press(BTN_M);CHECK(!animQueue.isWaitingInput());hp=pet.health;
    const auto feedSaved=disk;drain();drain();CHECK(pet.health==hp&&disk==feedSaved&&pet.daily_feed.feed_count==1);

    // Growth and gallery through actual daily application + callbacks + renderer.
    pet.age_days=2;
#if defined(CONTENT_SPROUT) && CONTENT_SPROUT
    pet.age_days=0;
#endif
    doDayEnd();drain();tick(10000);CHECK(pet.stage==STAGE_ADULT);
    CHECK(gallerySystem.isFormUnlocked(pet.form));
    menuController.switchContext(UI_IDLE);DisplayManager::switchPage(PAGE_IDLE);
    hostPins[PIN_BTN_M]=LOW;frame(31);frame(900);hostPins[PIN_BTN_M]=HIGH;frame(31);
    CHECK(menuController.getCurrentContext()==UI_GALLERY);const auto count=pet.daily_feed.feed_count;
    press(BTN_R);press(BTN_M);CHECK(menuController.getCurrentContext()==UI_IDLE&&pet.daily_feed.feed_count==count);
    // Real 3-key hold confirmation: first cancel, then confirm exactly once.
    auto destroy=[&](){for(auto pin:BTN_PIN_MAP)hostPins[pin]=LOW;frame(31);frame(BTN_DESTROY_HOLD_MS+1);for(auto pin:BTN_PIN_MAP)hostPins[pin]=HIGH;frame(31);};
    destroy();CHECK(menuController.getCurrentContext()==UI_DESTROY_CONFIRM);
    const auto rounds=deviceState.rounds;press(BTN_M);CHECK(deviceState.rounds==rounds);
    destroy();CHECK(menuController.getCurrentContext()==UI_DESTROY_CONFIRM);
    const auto destroyedName=std::string(content::formName(pet.form));Serial.output.clear();
    press(BTN_L);press(BTN_M);CHECK(deviceState.rounds==rounds+1);
    CHECK(Serial.output.find("Destroyed: "+destroyedName)!=std::string::npos);
    const auto saved=disk;drain();tick(10000);CHECK(deviceState.rounds==rounds+1&&disk==saved);
    menuController.switchContext(UI_IDLE);DisplayManager::switchPage(PAGE_IDLE);
    AnimNode node;node.durationMs=500;
    for(int i=0;i<ANIM_QUEUE_CAPACITY;++i)CHECK(animQueue.enqueue(node));
    CHECK(!animQueue.enqueue(node)&&animQueue.overflowed());const auto state=selected::capture(pet);
    press(BTN_L);CHECK(selected::capture(pet).health==state.health);
    animQueue.clear();tick(1);CHECK(!DisplayManager::isPageBlockingInput());
}

// These scenarios use real GPIO debounce, menu, rules, callbacks, rendering,
// animation queue and SaveManager. Only peripheral storage/GPIO are substituted.
#if defined(CONTENT_SPROUT) && CONTENT_SPROUT
void sproutFoodInputs(){
    CHECK(FOOD_COUNT==4&&SFOOD_COUNT==2);
    CHECK(FOOD_TABLE[2].id==42&&FOOD_TABLE[2].is_healthy&&FOOD_TABLE[2].health_delta==2);
    CHECK(SPECIAL_FOOD_TABLE[0].id==101&&SPECIAL_FOOD_TABLE[1].id==107);
    // Both healthy special choices, followed by every distinct Pebble combination.
    const uint8_t combinations[][3]={{17,23,42},{17,23,42},{17,23,90},{17,42,90},{23,42,90}};
    const int health[]={68,68,59,54,57},water[]={7,7,5,3,4};
    for(int scenario=0;scenario<5;++scenario){
        setup();Serial.output.clear();hostRandom=uint32_t(scenario);
        timeManager.setSimulatedTime(2024,1,1,8,0);tick(5000);
        press(BTN_L);CHECK(menuController.getCurrentContext()==UI_FEED_DRAW);
        for(int i=0;i<4;++i)press(BTN_M);
        CHECK(pet.daily_feed.feed_count==0);
        tick(5000);CHECK(menuController.getCurrentContext()==UI_FEED_PICK);
        const auto draw=menuController.getFeedState().draw;
        for(auto id:combinations[scenario]){
            uint8_t index=0;while(index<4&&FOOD_TABLE[draw.food_ids[index]].id!=id)++index;
            CHECK(index<4);
            for(int n=0;menuController.getFeedState().cursor!=index&&n<4;++n)press(BTN_R);
            CHECK(menuController.getFeedState().cursor==index);press(BTN_M);
        }
        CHECK(pet.daily_feed.feed_count==1);
        const auto feedHealth=pet.health;const auto feedDisk=disk;
        for(int i=0;i<4;++i)press(BTN_M);
        CHECK(pet.health==feedHealth&&pet.daily_feed.feed_count==1&&disk==feedDisk);
        drain();
        if(scenario<2){
            CHECK(animQueue.isWaitingInput()&&menuController.getCurrentContext()==UI_SPECIAL_FOOD);
            CHECK(!DisplayManager::isPageBlockingInput()&&pet.health==63);
            CHECK(sprout::read(selected::capture(pet)).water==6);
            CHECK(Serial.output.find("Special food selection (2 items)")!=std::string::npos);
            if(scenario==1)press(BTN_R);
            press(BTN_M);
            CHECK(!animQueue.isWaitingInput()&&pet.health==68);
            const auto name=scenario==0?"Special food selected: 0 (Spring Rain)":"Special food selected: 1 (Starlight)";
            const auto selectionLog=Serial.output.find(name);
            CHECK(selectionLog!=std::string::npos);
            const auto saved=disk;
            for(int i=0;i<4;++i)press(BTN_M);
            CHECK(pet.health==68&&disk==saved&&pet.daily_feed.feed_count==1);
            drain();drain();tick(10000);CHECK(disk==saved);
            CHECK(Serial.output.find(name,selectionLog+1)==std::string::npos);
        }else{
            CHECK(!animQueue.isWaitingInput());tick(10000);
            CHECK(Serial.output.find("Special food selection")==std::string::npos);
        }
        CHECK(!animQueue.isPlaying()&&!DisplayManager::isPageBlockingInput());
        CHECK(menuController.getCurrentContext()==UI_IDLE&&DisplayManager::getCurrentPage()==PAGE_IDLE);
        CHECK(pet.health==health[scenario]&&pet.form==40&&pet.stage==STAGE_CHILD);
        CHECK(sprout::read(selected::capture(pet)).water==water[scenario]);
        SaveManager reboot;PetState restored;
        CHECK(reboot.init()==SAVE_OK&&app::loadStartup(reboot,restored)==app::Startup::Loaded);
        CHECK(restored.health==health[scenario]&&restored.daily_feed.feed_count==1);
        CHECK(sprout::read(selected::capture(restored)).water==water[scenario]);
        CHECK(restored.form==40&&restored.daily_feed.last_feed_time==pet.daily_feed.last_feed_time);
        CHECK(reboot.save(restored,timeManager.now())==SAVE_OK);
        SaveManager rebootAgain;CHECK(rebootAgain.init()==SAVE_OK);
        CHECK(app::loadStartup(rebootAgain,restored)==app::Startup::Loaded&&restored.health==health[scenario]);
        CHECK(sprout::read(selected::capture(restored)).water==water[scenario]);
    }
}
#endif
void importPresentation(){
    for(bool combo:{false,true}){
        setup();gameCallbacks.onInitialTimeConfirm(2024,1,1,8,0);tick(5000);
        SaveHeader h{};h.save_time=timeManager.now();h.sequence=1;
        uint8_t raw[engine::FullBytes];size_t size=0;
        CHECK(app::encodeSave(pet,h,&gallerySystem.getData(),raw,sizeof(raw),size));
        bool found=false;
        for(hostRandom=0;hostRandom<10000;++hostRandom){auto draw=feedingSystem.drawFood();int count=0;for(auto id:draw.food_ids)count+=FOOD_TABLE[id].is_healthy;if(count>=3){found=true;break;}}
        CHECK(found);press(BTN_L);tick(5000);CHECK(menuController.getCurrentContext()==UI_FEED_PICK);
        if(combo){
            const auto draw=menuController.getFeedState().draw;int chosen=0;
            for(uint8_t i=0;i<4&&chosen<3;++i)if(FOOD_TABLE[draw.food_ids[i]].is_healthy){while(menuController.getFeedState().cursor!=i)press(BTN_R);press(BTN_M);++chosen;}
            drain();CHECK(animQueue.isWaitingInput()&&menuController.getCurrentContext()==UI_SPECIAL_FOOD);
        }else press(BTN_M);
        CHECK(app::commitImport(saveManager,pet,deviceState,gallerySystem.getData(),0,raw,size,timeManager.now(),timeManager.getDay())==SAVE_OK);
        menuController.onPetImported(); // Same successful-COMMIT presentation hook as console.
        CHECK(!animQueue.isPlaying()&&!animQueue.isWaitingInput()&&menuController.getCurrentContext()==UI_IDLE);
        CHECK(!menuController.getFeedState().active&&menuController.getFeedState().selected_count==0);
        tick(5000);const auto saved=disk;press(BTN_M);
        CHECK(menuController.getCurrentContext()==UI_STATUS&&pet.health==50&&pet.daily_feed.feed_count==0&&disk==saved);
    }
}
int main(int argc,char** argv){try{
    bool inputsOnly=false;
    for(int i=1;i<argc;++i){
        const std::string arg=argv[i];
        if(arg=="--require-all")continue; // All input assertions below are mandatory in both modes.
        if(arg=="--case"&&i+1<argc){const std::string value=argv[++i];CHECK(value=="all"||value=="inputs");inputsOnly=value=="inputs";}
        else throw std::runtime_error("unknown test argument: "+arg);
    }
    if(!inputsOnly){setup();calendar();setup();rollover();offlineCapacity();timeSetupFaults();visitTime();}
    inputs();importPresentation();
#if defined(CONTENT_SPROUT) && CONTENT_SPROUT
    sproutFoodInputs();
#endif
    std::cout<<"PASS integration "<<content::Title<<" "<<checks<<" checks\n";
return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

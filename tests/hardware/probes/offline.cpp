// Test firmware only. Link wrapper executes the real loop and real time UI callback.
// It measures the production offline path on the MCU, not a copied benchmark loop.
#include "../../../src/app/workflow_services.h"
#include <esp_timer.h>
extern "C" void __real__Z4loopv();
extern "C" void __wrap__Z4loopv(){
    __real__Z4loopv();
    static bool done=false;if(done)return;done=true;
    if(saveManager.hasSave()||saveManager.recoveryRequired()){
        Serial.println("[HW Offline] SKIPPED existing data; no automatic rewrite");return;
    }
    gameCallbacks.onInitialTimeConfirm(2024,1,1,0,0);
    if(waitingForTimeSet){Serial.println("[HW Offline] FAIL initialization");return;}
    const auto start=timeManager.now();const auto target=start+app::MaxOfflineSeconds;
    const auto t=timeManager.epochToTimeInfo(target);
    waitingForTimeSet=true;reinitPetAfterInitialTimeConfirm=false;
    const auto begin=esp_timer_get_time();
    gameCallbacks.onInitialTimeConfirm(t.year,t.month,t.day,t.hour,t.minute);
    const auto elapsed=esp_timer_get_time()-begin;
    PetState restored;const auto loaded=saveManager.load(restored);
    const bool ok=!waitingForTimeSet&&!saveManager.recoveryRequired()&&pet.age_days==366&&loaded==SAVE_OK&&restored.age_days==366;
    app::tracef("[HW Offline] %s seconds=%lu elapsed_us=%llu age=%u restored_age=%u heap=%u min_heap=%u stack_bytes=%u\n",
        ok?"PASS":"FAIL",(unsigned long)app::MaxOfflineSeconds,(unsigned long long)elapsed,
        pet.age_days,loaded==SAVE_OK?restored.age_days:0,ESP.getFreeHeap(),ESP.getMinFreeHeap(),(unsigned)uxTaskGetStackHighWaterMark(nullptr));
}

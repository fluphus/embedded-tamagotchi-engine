#pragma once
#include "import_workflow.h"
#include "save_format.h"
#include "serial_codec.h"
#include "diagnostics.h"
#include <cstdlib>
#include <cstring>
namespace app {
struct SlotImportSession {
    bool active=false;uint8_t slot=0;size_t received=0;uint8_t invalidCount=0;
    uint32_t lastActivity=0;uint8_t raw[engine::MaxSaveBytes]{};
    void abort(){active=false;received=0;invalidCount=0;}
    void expire(uint32_t now){if(active&&uint32_t(now-lastActivity)>=30000){abort();trace("[SaveImport] ABORTED: session timeout");}}
};
enum class ProtocolResult {Unhandled,Handled,Imported,Erased};
inline bool protocolSlot(const char* arg,uint8_t& slot){
    char* end=nullptr;const auto value=std::strtol(arg,&end,10);
    if(end==arg||*end||value<0||value>=SAVE_SLOT_COUNT)return false;
    slot=uint8_t(value);return true;
}
inline ProtocolResult saveProtocol(SlotImportSession& session,const char* cmd,
        SaveManager& manager,PetState& pet,DeviceState& device,GalleryData& gallery,
        uint32_t epoch,uint8_t day,uint32_t monotonic){
    session.expire(monotonic);
    if(std::strncmp(cmd,"SAVE_EXPORT",11)==0){
        uint8_t bytes[engine::MaxSaveBytes];size_t size=0;SaveResult result;int exportedSlot=-1;
        if(std::strcmp(cmd,"SAVE_EXPORT_OWNER")==0){result=manager.exportOwnerBackup(bytes,sizeof(bytes),size);}
        else{
            const bool petOnly=std::strncmp(cmd,"SAVE_EXPORT_PET ",16)==0;
            uint8_t slot=0;
            if(!protocolSlot(cmd+(petOnly?16:std::strncmp(cmd,"SAVE_EXPORT ",12)==0?12:std::strlen(cmd)),slot)){
                trace("[SaveExport] FAILED: invalid command/slot");return ProtocolResult::Handled;
            }
            exportedSlot=slot;
            SaveHeader header;PetState state;GalleryData bound;bool hasBound=false;
            result=petOnly?manager.exportSlotRaw(slot,header,state):manager.exportSlotRawWithGallery(slot,header,state,bound,hasBound);
            if(result==SAVE_OK&&!encodeSave(state,header,petOnly?nullptr:&bound,bytes,sizeof(bytes),size))result=SAVE_ERR_CORRUPTED;
        }
        if(result!=SAVE_OK){tracef("[SaveExport] FAILED (%u)\n",unsigned(result));return ProtocolResult::Handled;}
        if(exportedSlot>=0)tracef("SAVE_EXPORT_BEGIN slot=%u bytes=%u\n",unsigned(exportedSlot),unsigned(size));
        else tracef("SAVE_EXPORT_BEGIN owner_restore bytes=%u\n",unsigned(size));
        for(size_t offset=0;offset<size;offset+=16){
            char hex[33];size_t take=size-offset<16?size-offset:16;
            for(size_t i=0;i<take;++i)std::snprintf(hex+i*2,3,"%02X",bytes[offset+i]);
            tracef("SAVE_EXPORT_DATA %s\n",hex);
        }
        if(exportedSlot>=0)tracef("SAVE_EXPORT_END slot=%u bytes=%u\n",unsigned(exportedSlot),unsigned(size));
        else tracef("SAVE_EXPORT_END owner_restore bytes=%u\n",unsigned(size));
        return ProtocolResult::Handled;
    }
    if(std::strcmp(cmd,"SAVE_RECOVERY_ERASE CONFIRM")==0){
        if(!manager.recoveryRequired()){trace("[SaveRecovery] REJECTED: no recovery lock");return ProtocolResult::Handled;}
        if(manager.erase()!=SAVE_OK){trace("[SaveRecovery] FAILED");return ProtocolResult::Handled;}
        session.abort();trace("[SaveRecovery] Explicit erase completed; please set time.");return ProtocolResult::Erased;
    }
    if(std::strncmp(cmd,"SAVE_IMPORT_BEGIN ",18)==0){
        uint8_t slot=0;session.abort();
        if(!protocolSlot(cmd+18,slot)||device.is_visiting||manager.importPending()){
            trace("[SaveImport] REJECTED: invalid slot or visit/recovery active");return ProtocolResult::Handled;
        }
        session.active=true;session.slot=slot;session.lastActivity=monotonic;
        tracef("[SaveImport] READY slot=%u target_bytes=%u (legacy=%u) v9_pet=72 v9_full=84 offset=1\n",slot,unsigned(engine::FullBytes),unsigned(engine::PetBytes));
        return ProtocolResult::Handled;
    }
    if(std::strcmp(cmd,"SAVE_IMPORT_ABORT")==0){session.abort();trace("[SaveImport] ABORTED");return ProtocolResult::Handled;}
    if(std::strncmp(cmd,"SAVE_IMPORT_DATA ",17)==0){
        if(!session.active){trace("[SaveImport] No active session.");return ProtocolResult::Handled;}
        const char* hex=cmd+17;size_t offset=session.received;
        if(*hex=='@'){
            char* end=nullptr;unsigned long value=std::strtoul(hex+1,&end,10);
            if(end==hex+1||*end!=' '||value>sizeof(session.raw)){
                session.abort();trace("[SaveImport] Invalid hex payload or offset; ABORTED");return ProtocolResult::Handled;
            }
            offset=size_t(value);hex=end+1;
        }
        uint8_t decoded[engine::MaxSaveBytes];size_t count=0;
        if(!decodeHexToBytes(hex,decoded,sizeof(decoded),&count)||offset>session.received||count>sizeof(session.raw)-offset||
           (offset<session.received&&(count>session.received-offset||std::memcmp(decoded,session.raw+offset,count)))){
            session.abort();trace("[SaveImport] Invalid hex payload or offset; ABORTED");return ProtocolResult::Handled;
        }
        if(offset==session.received){std::memcpy(session.raw+offset,decoded,count);session.received+=count;}
        session.lastActivity=monotonic;
        tracef("[SaveImport] RECV %u/%u\n",unsigned(session.received),unsigned(engine::FullBytes));return ProtocolResult::Handled;
    }
    if(std::strcmp(cmd,"SAVE_IMPORT_COMMIT")==0){
        if(!session.active){trace("[SaveImport] No active session.");return ProtocolResult::Handled;}
        const auto count=session.received;session.active=false;
        if(count!=engine::PetBytes&&count!=engine::FullBytes&&count!=72&&count!=84){
            session.abort();trace("[SaveImport] Incomplete payload; ABORTED");return ProtocolResult::Handled;
        }
        const auto result=commitImport(manager,pet,device,gallery,session.slot,session.raw,count,epoch,day);
        session.abort();return result==SAVE_OK?ProtocolResult::Imported:ProtocolResult::Handled;
    }
    return ProtocolResult::Unhandled;
}
// Complete lines only. Overlong lines are discarded in their entirety, including
// their prefix; timeout discards a disconnected sender's partial line.
struct CommandLine {
    char bytes[64]{};size_t length=0;bool overflow=false;uint32_t last=0;
    template<class Handler>void receive(char c,uint32_t now,Handler handle){
        if(length&&uint32_t(now-last)>=30000){length=0;overflow=false;}
        last=now;
        if(c=='\r'||c=='\n'){
            if(overflow)trace("[Cmd] REJECTED: overlong command");
            else if(length){bytes[length]=0;handle(bytes);}
            length=0;overflow=false;
        }else if(length<sizeof(bytes)-1&&!overflow)bytes[length++]=c;
        else overflow=true;
    }
};
}

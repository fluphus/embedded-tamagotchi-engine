// Compare a real device UART export after production startup migration with
// the production v9 migrator's canonical payload (not host-struct serialization).
#include "content/artoria/v9.h"
#include <fstream>
#include <iterator>
#include <iostream>
#include <vector>
int main(int argc,char** argv){
 if(argc!=3)return 2;
 auto read=[](const char* path){std::ifstream f(path,std::ios::binary);return std::vector<uint8_t>(std::istreambuf_iterator<char>(f),{});};
 const auto old=read(argv[1]),live=read(argv[2]);engine::Document a,b;
 const engine::PackageContract pack{0x41525452,[](engine::Id id){return id<12;},artoria::validate};
 if(artoria::migrateV9(old.data(),old.size(),a)!=engine::DecodeError::Ok||engine::decode(live.data(),live.size(),pack,b)!=engine::DecodeError::Ok)return 3;
 uint8_t canonical[engine::FullBytes];size_t n=0;
 if(engine::encode(a,pack,canonical,sizeof(canonical),n)!=engine::DecodeError::Ok||n!=live.size())return 4;
 for(size_t i=24;i<n;++i)if(canonical[i]!=live[i]){std::cerr<<"payload mismatch at "<<i<<"\n";return 5;}
 std::cout<<"PASS real v9 -> on-device startup -> UART v10: all state, extension and gallery payload bytes equal; header sequence/time may advance\n";
}

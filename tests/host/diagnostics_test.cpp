#include "platform/arduino/diagnostics.h"
#include <cstdlib>
#include <iostream>
unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<'\n';return EXIT_FAILURE;}}while(false)
int main(){
    for(size_t length:{0u,1u,31u,32u,33u,64u,88u,191u}){
        std::string line(length,'X');if(length>4)line[length/2]='\n';
        Serial={};Serial.println(line.c_str());const auto original=Serial.output;
        Serial={};platform::diagnosticLine(line.c_str());
        CHECK(Serial.output==original);
#if ARDUINO_USB_CDC_ON_BOOT && ARDUINO_USB_MODE == 1
        CHECK(Serial.largestWrite<=32);
        CHECK(Serial.flushes>0);
#else
        CHECK(Serial.flushes==0);
#endif
    }
#if ARDUINO_USB_CDC_ON_BOOT && ARDUINO_USB_MODE == 1
    for(size_t failure:{1u,2u})for(size_t partial:{0u,7u}){
        Serial={};Serial.failAt=failure;Serial.partialBytes=partial;platform::diagnosticLine(std::string(120,'X').c_str());
        CHECK(Serial.writes==failure);
        CHECK(Serial.output.find('\n')==std::string::npos);
        CHECK(Serial.output.size()<120);
    }
#endif
    std::cout<<"PASS diagnostic adapter usb="<<ARDUINO_USB_CDC_ON_BOOT<<" mode="<<ARDUINO_USB_MODE<<" checks="<<checks<<'\n';
}

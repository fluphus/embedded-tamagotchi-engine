#pragma once
// Only the Serial peripheral used by the production output adapter.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
struct TestSerial {
    std::string output;
    size_t largestWrite=0, writes=0, flushes=0, failAt=0, partialBytes=0;
    size_t write(const uint8_t* data,size_t size) {
        ++writes;if(size>largestWrite)largestWrite=size;
        if(writes==failAt){output.append(reinterpret_cast<const char*>(data),partialBytes);return partialBytes;}
        output.append(reinterpret_cast<const char*>(data),size);return size;
    }
    void flush(){++flushes;}
    void println(const char* text="") {
        write(reinterpret_cast<const uint8_t*>(text),std::strlen(text));
        write(reinterpret_cast<const uint8_t*>("\r\n"),2);
    }
};
inline TestSerial Serial;

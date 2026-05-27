#include <windows.h>
#include <cstdio>
#include <vector>
#include <cstdint>
#include <cstring>

#include "UltraHDREncoder.h"
#include "common/ImageBuffer.h"

int wmain() {
    // Create a minimal HDR image: 64x64 RGBA F16, all white
    ImageBuffer buf(64, 64);
    // Fill with white (1.0 in half-float = 0x3C00)
    uint16_t white = 0x3C00;
    // Fill with semi-HDR white (10.0 in half-float = 0x4900)
    uint16_t hdr = 0x4900;  
    for (auto& v : buf.data) {
        v = hdr;  // RGB = bright HDR, A = 1.0
    }
    
    wprintf(L"Buffer: %dx%d, stride=%zu, data size=%zu\n",
        buf.width, buf.height, buf.stride(), buf.data.size() * sizeof(uint16_t));
    
    UltraHDREncoder encoder;
    auto result = encoder.Encode(buf, L"C:\\Users\\20506\\Desktop\\test_xmp_output.jpg", 95, 90, 1);
    
    if (result.ok()) {
        wprintf(L"SUCCESS: Encoded to test_xmp_output.jpg\n");
        return 0;
    } else {
        wprintf(L"FAILED: %s\n", result.message.c_str());
        return 1;
    }
}
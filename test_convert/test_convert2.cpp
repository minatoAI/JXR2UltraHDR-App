// Minimal UltraHDR conversion test
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <string>
#include <memory>

// UltraHDR API
#include <ultrahdr_api.h>

// Our pipeline API
#include "pipeline/JXRDecoder.h"
#include "pipeline/UltraHDREncoder.h"

int wmain(int argc, wchar_t* argv[]) {
    if (argc < 3) {
        wprintf(L"Usage: %s input.jxr output.jpg\n", argc > 0 ? argv[0] : L"test");
        return 1;
    }

    std::wstring inputPath = argv[1];
    std::wstring outputPath = argv[2];

    // Decode JXR
    JXRDecoder decoder;
    ImageBuffer buf;
    auto result = decoder.Decode(inputPath, buf);
    if (!result.ok()) {
        wprintf(L"Decode failed: %s\n", result.message.c_str());
        return 1;
    }
    wprintf(L"Decoded: %dx%d\n", buf.width, buf.height);

    // Encode UltraHDR
    UltraHDREncoder encoder;
    result = encoder.Encode(buf, outputPath, 90, 70, 1);
    if (!result.ok()) {
        wprintf(L"Encode failed: %s\n", result.message.c_str());
        return 1;
    }

    wprintf(L"Success! Output: %s\n", outputPath.c_str());
    return 0;
}

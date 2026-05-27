#include <windows.h>
#include <string>
#include <vector>

// Minimal converter API declarations matching our library
struct ImageBuffer {
    int width = 0, height = 0, channels = 4;
    std::vector<uint16_t> data;
    ImageBuffer() = default;
    ImageBuffer(int w, int h) : width(w), height(h) {
        data.resize(static_cast<size_t>(w) * h * 4);
    }
    bool empty() const { return data.empty(); }
    size_t stride() const { return static_cast<size_t>(width) * 8; }
};

enum class ConverterErrorCode {
    kSuccess = 0,
    kJXRDecodeFailed,
    kUltraHDREncodingFailed,
    kUnsupportedPixelFormat,
    kFileOpenFailed,
    kFileWriteFailed,
    kMemoryAllocationFailed
};

struct ConverterResult {
    bool success;
    ConverterErrorCode errorCode;
    std::wstring errorMessage;
    static ConverterResult Success() { return {true, ConverterErrorCode::kSuccess, L""}; }
    static ConverterResult Fail(ConverterErrorCode code, const std::wstring& msg) {
        return {false, code, msg};
    }
};

// JXRDecoder
class JXRDecoder {
public:
    JXRDecoder();
    ~JXRDecoder();
    ConverterResult Decode(const std::wstring& filePath, ImageBuffer& outBuffer);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// UltraHDREncoder
class UltraHDREncoder {
public:
    UltraHDREncoder();
    ~UltraHDREncoder();
    ConverterResult Encode(const ImageBuffer& hdrBuffer,
                           const std::wstring& outputPath,
                           int sdrQuality = 90,
                           int gainMapQuality = 70,
                           int gainMapScaleFactor = 1);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

int wmain(int argc, wchar_t* argv[]) {
    if (argc < 3) {
        MessageBoxW(nullptr, L"Usage: test_convert.exe input.jxr output.jpg", L"Test", MB_OK);
        return 1;
    }
    
    std::wstring inputPath = argv[1];
    std::wstring outputPath = argv[2];
    
    // Decode JXR
    JXRDecoder decoder;
    ImageBuffer buf;
    auto result = decoder.Decode(inputPath, buf);
    if (!result.success) {
        std::wstring msg = L"Decode failed: " + result.errorMessage;
        MessageBoxW(nullptr, msg.c_str(), L"Test", MB_OK);
        return 1;
    }
    
    // Encode UltraHDR
    UltraHDREncoder encoder;
    result = encoder.Encode(buf, outputPath, 90, 70, 1);
    if (!result.success) {
        std::wstring msg = L"Encode failed: " + result.errorMessage;
        MessageBoxW(nullptr, msg.c_str(), L"Test", MB_OK);
        return 1;
    }
    
    MessageBoxW(nullptr, L"Success!", L"Test", MB_OK);
    return 0;
}

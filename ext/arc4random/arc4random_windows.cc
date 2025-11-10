/*
 * Windows implementation of arc4random functions
 * Provides cryptographically secure random number generation using Windows APIs
 */
#include <cstddef>
#include <cinttypes>
#include <random>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>

extern "C" {

void arc4random_buf(void* buf, size_t nbytes) {
    HCRYPTPROV hCryptProv = 0;
    if (CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        CryptGenRandom(hCryptProv, static_cast<DWORD>(nbytes), static_cast<BYTE*>(buf));
        CryptReleaseContext(hCryptProv, 0);
    } else {
        // Fallback to std::random_device if CryptGenRandom fails
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<uint8_t> dis;
        uint8_t* bytes = static_cast<uint8_t*>(buf);
        for (size_t i = 0; i < nbytes; ++i) {
            bytes[i] = dis(gen);
        }
    }
}

uint32_t arc4random(void) {
    uint32_t result;
    arc4random_buf(&result, sizeof(result));
    return result;
}

uint32_t arc4random_uniform(uint32_t upper_bound) {
    if (upper_bound == 0) return 0;
    
    // Avoid modulo bias by rejecting values >= (2^32 - upper_bound) % upper_bound
    uint32_t min = (0xffffffff - upper_bound + 1) % upper_bound;
    uint32_t r;
    do {
        r = arc4random();
    } while (r < min);
    
    return r % upper_bound;
}

void explicit_bzero(void* ptr, size_t len) {
    SecureZeroMemory(ptr, len);
}

} // extern "C"

#endif // _WIN32

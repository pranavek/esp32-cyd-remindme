// Minimal 24-bit BMP loader. Adapted from Bodmer's TFT_eSPI examples.
// Streams scanlines from LittleFS, converts BGR888 to RGB565 on the fly,
// pushes one row at a time. Bottom-up storage handled by walking y backwards.

#include "Bmp.h"

#include <FS.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>

namespace Bmp {

static uint16_t read16(fs::File& f) {
    uint16_t r;
    ((uint8_t*)&r)[0] = f.read();
    ((uint8_t*)&r)[1] = f.read();
    return r;
}

static uint32_t read32(fs::File& f) {
    uint32_t r;
    ((uint8_t*)&r)[0] = f.read();
    ((uint8_t*)&r)[1] = f.read();
    ((uint8_t*)&r)[2] = f.read();
    ((uint8_t*)&r)[3] = f.read();
    return r;
}

bool draw(TFT_eSPI& tft, const char* path, int x, int y) {
    if (x >= tft.width() || y >= tft.height()) return false;
    if (!LittleFS.exists(path)) return false;

    fs::File f = LittleFS.open(path, "r");
    if (!f) return false;

    if (read16(f) != 0x4D42) { f.close(); return false; }   // 'BM'
    read32(f);
    read32(f);
    uint32_t pixOffset = read32(f);
    read32(f);
    int32_t  w = (int32_t)read32(f);
    int32_t  h = (int32_t)read32(f);
    if (read16(f) != 1)  { f.close(); return false; }
    if (read16(f) != 24) { f.close(); return false; }
    if (read32(f) != 0)  { f.close(); return false; }

    bool flip = (h > 0);
    if (h < 0) h = -h;

    f.seek(pixOffset);

    const int padding = (4 - (w * 3) & 3) & 3;
    const size_t rowBytes = (size_t)(w * 3 + padding);
    uint8_t lineBuf[rowBytes];

    bool oldSwap = tft.getSwapBytes();
    tft.setSwapBytes(true);

    for (int row = 0; row < h; ++row) {
        int targetY = flip ? (y + h - 1 - row) : (y + row);
        if (targetY < 0 || targetY >= tft.height()) {
            f.read(lineBuf, rowBytes);
            continue;
        }
        f.read(lineBuf, rowBytes);

        uint8_t*  bptr = lineBuf;
        uint16_t* tptr = (uint16_t*)lineBuf;
        for (int col = 0; col < w; ++col) {
            uint8_t b = *bptr++;
            uint8_t g = *bptr++;
            uint8_t r = *bptr++;
            *tptr++ = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        }
        tft.pushImage(x, targetY, w, 1, (uint16_t*)lineBuf);
    }

    tft.setSwapBytes(oldSwap);
    f.close();
    return true;
}

}  // namespace Bmp

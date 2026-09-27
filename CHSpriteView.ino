
#include <CHGfx.h>
#include "src/CHSpriteView.h"

enum : uint8_t {
    BLACK = 0, DARKGREY, GREY, LIGHTGREY, WHITE,
    RED, ORANGE, YELLOW, GREEN, DARKGREEN,
    CYAN, BLUE, NAVY, MAGENTA, PURPLE, PINK
};

static const uint16_t palette[16] = {
    0x0000, 0x18E3, 0x4208, 0xC618, 0xFFFF,
    0xF800, 0xFD20, 0xFFE0, 0x07E0, 0x0400,
    0x07FF, 0x001F, 0x0010, 0xF81F, 0x8010, 0xFC9F
};

// CHGame chGame;

void setup()
{
    Gfx.begin();                    // 24 MHz SPI, 16 bpp output
    Gfx.setPalette(palette, 16);
    Gfx.clear(NAVY);
    Gfx.display();                  // one DMA burst, ~11 ms
}

void loop()
{
    int returnCode = drawSpriteFile("SPRITE.BIN", 12, 12, 0);
    Serial.println(returnCode);
    Gfx.display();                  // one DMA burst, ~11 ms
}
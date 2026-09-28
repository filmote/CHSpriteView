
#include "src/CHGame.h"
#include "src/CHSpriteView.h"

enum Colors { Transparent, Blue, White, Red, Navy,
              BLANK_2, BLANK_3, BLANK_4, BLANK_5, BLANK_6,
              BLANK_7, BLANK_8, BLANK_9, BLANK_10, BLANK_11, Black };

static const uint16_t palette[16] = {
    0x0000, 0x0001, 0x0020, 0xF949, 0xF128,
    0xFD00, 0xFF45, 0xF129, 0xF4E0, 0xF4E1,
    0xF724, 0xF725, 0x0000, 0x0000, 0x0000,
    0x0000
};

static const uint8_t Sprite_InMem_X = 16;
static const uint8_t Sprite_InMem_Y = 16;
static const uint8_t Sprite_InMem[] PROGMEM
{
    0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
    0x01, 0x00, 0x10, 0x01, 0x10, 0x01, 0x00, 0x10,
    0x01, 0x00, 0x10, 0x01, 0x10, 0x01, 0x00, 0x10,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
    0x01, 0x00, 0x01, 0x00, 0x00, 0x10, 0x00, 0x10,
    0x01, 0x10, 0x00, 0x00, 0x00, 0x00, 0x01, 0x10,
    0x01, 0x01, 0x01, 0x00, 0x00, 0x10, 0x10, 0x10,
    0x01, 0x00, 0x10, 0x00, 0x00, 0x01, 0x00, 0x10,
    0x01, 0x00, 0x00, 0x11, 0x11, 0x00, 0x00, 0x10,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
    0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
};

CHGame chGame;
uint8_t fire_Idx = 0;
uint32_t prevTime;

uint8_t gBuf[SPRITE_BUF_SIZE];

void setup()
{
    Gfx.begin();                    // 24 MHz SPI, 16 bpp output

    if (!SD.begin(24000000UL, PIN_SD_CS)) SD.begin(PIN_SD_CS);
    spiClaimForLcd();

    Gfx.setPalette(palette, 16);

    chGame.setFrameRate(60);
    chGame.boot();

    Gfx.display();                  // one DMA burst, ~11 ms
    prevTime = millis();
}

#define _TEST_NF_1
#define TEST_NF_2
#define _TEST_NF_3
#define _TEST_NF_4
#define _TEST_EF_1
#define _TEST_EF_2

void loop()
{
    // if (!chGame.nextFrame()) return;
    uint32_t currTime = millis();
    Serial.println(currTime - prevTime);
    prevTime = currTime;

    // Gfx.wait();
    Gfx.clear(Colors::Black);
    chGame.pollButtons();

    // Opens a file based on file name, determines W and H from file ..
    // Approx 113 - 129 milliseconds / 7.75 to 8.80 fps
    #if defined(TEST_NF_1)
        //                 012345678901
        char fileName[] = "WHEI/FIRE_00.BIN";
        fileName[10] = 48 + fire_Idx / 10;
        fileName[11] = 48 + fire_Idx % 10;

        drawSpriteFile(fileName, 0, 64, Colors::Transparent);

    // Opens a file based on file name, W and H passed explicilty ..
    // Approx 105 - 120 milliseconds / 8.33 9.52 tfps
    #elif defined(TEST_NF_2)
        //                 012345678901
        char fileName[] = "FIRE/FIRE_00.BIN";
        fileName[10] = 48 + fire_Idx / 10;
        fileName[11] = 48 + fire_Idx % 10;

        drawSpriteFile(fileName, 0, 64, 128, 64, Colors::Transparent);

    // Opens a file based on file name, determines W and H from file, uses a global buffer for reading SD data ..
    // Approx 114 - 130 milliseconds / 7.69 to 8.80 fps
    #elif defined(TEST_NF_3)
        //                 012345678901
        char fileName[] = "WHEI/FIRE_00.BIN";
        fileName[10] = 48 + fire_Idx / 10;
        fileName[11] = 48 + fire_Idx % 10;

        drawSpriteFile_WithBuff(fileName, 0, 64, Colors::Transparent, gBuf);

    // Opens a file based on file name, W and H passed explicilty, uses a global buffer for reading SD data ..
    // Approx 103 - 119 milliseconds / 9.53 to 8.33 fps
    #elif defined(TEST_NF_4)
        //                 012345678901
        char fileName[] = "FIRE/FIRE_00.BIN";
        fileName[10] = 48 + fire_Idx / 10;
        fileName[11] = 48 + fire_Idx % 10;

        drawSpriteFile_WithBuff(fileName, 0, 64, 128, 64, Colors::Transparent, gBuf);

    // Uses globally openned file and seeks the starting position of file, W and H passed explicilty ..
    // Approx 127 - 135 milliseconds / 7.41 to 7.87 fps
    #elif defined(TEST_EF_1)

        sdBegin();
        File f = SD.open("/FIRE/FIRE.BIN");
        sdEnd();

        drawSpriteFile(&f, 0, 64, 128, 64, fire_Idx, Colors::Transparent);

    // Uses globally openned file and seeks the starting position of file, W and H passed explicilty, uses a global buffer for reading SD data ..
    // Approx 127 - 135 milliseconds / 7.41 to 7.87 fps
    #elif defined(TEST_EF_2)

        sdBegin();
        File f = SD.open("/FIRE/FIRE.BIN");
        sdEnd();

        drawSpriteFile_WithBuff(&f, 0, 64, 128, 64, fire_Idx, Colors::Transparent, gBuf);

    #endif

    // Gfx.drawSprite(Sprite_InMem, 0, 0, 16, 16, Colors::Transparent);
    Gfx.display();                

    fire_Idx++;
    if (fire_Idx > 15) fire_Idx = 0;
}
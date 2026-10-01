#include "src/CHGame.h"
#include "src/CHSpriteView.h"
#include "src/Enums.h"

#define FIRE_FRAMES 16
#define FIRE_X      0
#define FIRE_Y      64

CHGame chGame;

static SpriteFile spriteFiles[Images::Count];   /* Stores the starting block and other details of each image */
static uint8_t    fireIdx   = 0;
File f;

#define _DEBUG_SERIAL
#define TEST_1
#define _TEST_2
#define _TEST_3
#define _TEST_4

void setup() {
 
    Gfx.begin(GFX_DIV2, GFX_16BPP);
    Gfx.setPalette(palette, 16);

    chGame.setFrameRate(60);
    chGame.boot();

    #if defined(DEBUG_SERIAL) 
        Serial.waitForPC(1500);
    #endif

    #if defined(TEST_1) || defined(TEST_2) || defined(TEST_3)

        if (!SD.begin(24000000UL, PIN_SD_CS)) SD.begin(PIN_SD_CS);
        spiClaimForLcd();
    
    #endif

    #if defined(TEST_3)

        sdBegin();
        f = SD.open("/FIRE/FIRE.BIN");
        sdEnd();

        #if defined(DEBUG_SERIAL) 
            if (!f) { Serial.println(" Test_3: open FAILED. "); }
        #endif

    #endif

    #if defined(TEST_4)

        // --------------------------------------------------------------------------------------------
        // TEST_4
        //
        // Load all the sprite block starting addresses into an array of SpriteFiles for fast streaming
        // later.  The process will fail if any of the files are fragmented as this kills the read
        // performance .. to fix this, format the SD card and write the files back onto it.
        //
        while (!loadAll(spriteFiles)) {
            /* No card or no frames: wait for START, then try again. */
            do { chGame.pollButtons(); delay(10); } while (!chGame.justPressed(START_BUTTON));
            // drawStaticScreen();
        }

    #endif

    // drawStaticScreen();
}

void loop() {

    if (!chGame.nextFrame()) return;
    chGame.pollButtons();

    Gfx.wait();
    Gfx.clear(Colors::Black);

    // --------------------------------------------------------------------------------------------
    // TEST_1
    // 
    // Opens a file based on file name, determines W and H from file itself. This is the slowest 
    // but most convenient method. Unlike TEST_4, this method is not affected due to fragmentation
    // of the files.
    //
    // These images can be generated from https://www.bloggingadeadhorse.com/cart/CHGfxConverter.html 
    // using the Download.bin / Download.zip function and ensuring the 'Include width / height header 
    // in bin' option is *selected*.  The data format is [Width][Height]<data> where the data is encoded 
    // with two horizontal pixels per byte (16 colours) and even pixels in the low nibble of each byte. 
    // This coincides with the screen buffers format allowing easy blitting using the standard CHGfx 
    // library.
    //
    #if defined(TEST_1)

        char fileName[] = "FIRE/FIRE_00.BIN";
        fileName[10] = 48 + fireIdx / 10;
        fileName[11] = 48 + fireIdx % 10;

        #if defined(DEBUG_SERIAL) 

            Serial.print("TEST_1 ");
            Serial.println(fileName);

        #endif

        uint8_t ret = drawSpriteFile(fileName, 0, 64, Colors::Transparent);
      

    // --------------------------------------------------------------------------------------------
    // TEST_2
    // 
    // Opens a file based on file name with the W and H passed explicitly.  This is the fastest method
    // available when using file names. Unlike TEST_4, this method is not affected due to fragmentation
    // of the files.
    //
    // These images can be generated from https://www.bloggingadeadhorse.com/cart/CHGfxConverter.html 
    // using the Download.bin / Download.zip function and ensuring the 'Include width / height header 
    // in bin' option is *deselected*.  The data format is simply <data> where the data is encoded with
    // two horizontal pixels per byte (16 colours) and even pixels in the low nibble of each byte. 
    // This coincides with the screen buffers format allowing easy blitting using the standard CHGfx 
    // library.
    //
    #elif defined(TEST_2)

        char fileName[] = "FIRE/NOWH.BIN";

        #if defined(DEBUG_SERIAL) 

            Serial.print("TEST_2 ");
            Serial.println(fileName);

        #endif

        uint8_t ret = drawSpriteFile(fileName, 0, 64, 128, 64, Colors::Transparent);

   

    // --------------------------------------------------------------------------------------------
    // TEST_3
    // 
    // Opens a file in the initial setup and uses this open file handle for the remainder of the sketch.
    // All images are packed 'end to end' in a single file with no Width or Height values encoded in the
    // file. This is the fastest method available when using a potentially fragmented file.
    // Each draw seeks to frame fireIdx (byte offset W * H * fireIdx / 2) in the open file and then
    // reads W * H / 2 bytes from there; the file is never closed.
    //
    // These images can be generated from https://www.bloggingadeadhorse.com/cart/CHGfxConverter.html 
    // using the Download.zip function and ensuring the 'Spritesheet as a single .BIN, frames packed 
    // end-to-end (no width/height)' option is *selected*. 
    //
    #elif defined(TEST_3)

        uint8_t ret = drawSpriteFile(&f, 0, 64, 128, 64, fireIdx, Colors::Transparent);


    // --------------------------------------------------------------------------------------------
    // TEST_4
    // 
    // Uses a global lookup table to quickly position and read the file contents - populated in the
    // setup() (see above). The Width and Height are read from each file's header by loadAll() and thus it 
    // uses the same file structure as TEST_1.  Note though that the file contents cannot be fragmented.
    //
    // These images can be generated from https://www.bloggingadeadhorse.com/cart/CHGfxConverter.html 
    // using the Download.bin / Download.zip function and ensuring the 'Include width / height header 
    // in bin' option is *selected*.  The data format is [Width][Height]<data> where the data is encoded 
    // with two horizontal pixels per byte (16 colours) and even pixels in the low nibble of each byte. 
    // This coincides with the screen buffers format allowing easy blitting using the standard CHGfx 
    // library.
    //
    #elif defined(TEST_4)

        #if defined(DEBUG_SERIAL) 
            Serial.print("TEST_3 IDX:");
            Serial.println(fireIdx);
        #endif

        spriteDraw(spriteFiles[fireIdx], FIRE_X, FIRE_Y, -1);
        spriteDraw(spriteFiles[Images::Skull], 44, 50, 0);
        gfx_flushRectAsync(0, 0, 128, 128);  



    #endif
        
    if (chGame.isFrameCount(8)) {
        if (++fireIdx >= FIRE_FRAMES) fireIdx = 0;
    }

    Gfx.display();                

}
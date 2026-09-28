/*
 * spriteview.cpp - reads a packed-4bpp sprite file off the SD card and
 * blits it into CHGfx's framebuffer, in buffered chunks of rows. The file
 * either carries a 2-byte (width, height) header, or the caller passes
 * the size in and the file is read as raw, headerless rows -- see
 * drawSpriteFile()'s doc comment in the header.
 *
 * Reading a row at a time (instead of the whole sprite into RAM) means a
 * full 128x128 sprite costs a 64-byte stack buffer, not an 8 KB one --
 * this chip doesn't have 8 KB to spare on top of CHGfx's own framebuffer.
 * Every SD access stays bracketed in sdBegin()/sdEnd(), same as chgame.h
 * asks, so the shared SPI bus hands back to the LCD cleanly between reads.
 */
#include "CHSpriteView.h"

/* --------------------------------------------------------------------- */
/* SPI1 arbitration                                                       */
/* --------------------------------------------------------------------- */
#define SPI_MSTR (1u << 2)
#define SPI_SPE  (1u << 6)
#define SPI_SSI  (1u << 8)
#define SPI_SSM  (1u << 9)

/* Master, mode 0, software NSS held high, MSB first, 8-bit frames, BR = 000
 * (HCLK/2 = 24 MHz, the chip's ceiling and what gfx_begin(GFX_DIV2) asks for).
 * CTLR2 is cleared so the SD side cannot leave TXDMAEN or SSOE asserted;
 * CHGfx sets TXDMAEN itself per transfer and flips DFF as it needs it. */
void spiClaimForLcd(void)
{
    SPI1->CTLR1  = 0;
    SPI1->CTLR2  = 0;
    SPI1->CTLR1  = SPI_MSTR | SPI_SSM | SPI_SSI;
    SPI1->CTLR1 |= SPI_SPE;
}

void sdBegin(void) { gfx_wait(); }
void sdEnd(void)   { spiClaimForLcd(); }

static bool fileOpen(File &f, const char *path)
{
    sdBegin();
    f = SD.open(path);
    sdEnd();
    return (bool)f;
}

static void fileClose(File &f)
{
    sdBegin();
    f.close();
    sdEnd();
}

static bool fileRead(File &f, uint8_t *dst, size_t n)
{
    sdBegin();
    const bool ok = ((size_t)f.read(dst, n) == n);
    sdEnd();
    return ok;
}

int drawSpriteFile(const char *path, int x, int y, int transparentIndex) {

    return drawSpriteFile(path, x, y, -1, -1, transparentIndex);

}

int drawSpriteFile(const char *path, int x, int y, int w, int h, int transparentIndex)
{
    File f;
    if (!fileOpen(f, path)) {
        return SPRITE_ERR_FILE_OPEN;
    }

    // Retrieve the width and height from images?
    
    if (w <= 0 || h <= 0) {
        /* No size given -- the file must carry the 2-byte header. */
        uint8_t header[2];
        if (!fileRead(f, header, 2)) {
            fileClose(f);
            return SPRITE_ERR_HEADER_READ;
        }
        w = header[0];
        h = header[1];
    }
    /* else: size was passed in, so the file is treated as raw rows with
     * no header at all -- nothing has been read from it yet. */

    if (w <= 0 || h <= 0) {
        fileClose(f);
        return SPRITE_ERR_BAD_DIMENSIONS;
    }

    const size_t rowBytes = (size_t)(w + 1) / 2;
    if (rowBytes > SPRITE_BUF_SIZE) {
        fileClose(f);
        return SPRITE_ERR_TOO_WIDE;
    }

    /* gfx_blit()'s signature always takes transparent as an int:
     *   void gfx_blit(const uint8_t *spr, int x, int y, int w, int h, int transparent);
     * -1 is assumed to mean "opaque, no transparent colour" -- worth
     * confirming against CHGfx.h if sprites come out wrong. */

    uint8_t buf[SPRITE_BUF_SIZE];
    const int rowsPerRead = SPRITE_BUF_SIZE / rowBytes; /* >= 1, checked above */

    int row = 0;
    while (row < h) {
        const int rowsThisRead = (rowsPerRead < h - row) ? rowsPerRead : (h - row);
        const size_t bytesThisRead = rowsThisRead * rowBytes;

        if (!fileRead(f, buf, bytesThisRead)) {
            fileClose(f);
            return SPRITE_ERR_TRUNCATED;
        }
        for (int i = 0; i < rowsThisRead; i++) {
            gfx_blit(buf + (size_t)i * rowBytes, x, y + row + i, w, 1, transparentIndex);
        }
        row += rowsThisRead;
    }

    fileClose(f);
    return SPRITE_OK;
}
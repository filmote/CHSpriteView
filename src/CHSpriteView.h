#pragma once

/*
 * CHSpriteView - draw packed 4 bpp sprites from microSD into CHGfx's
 * framebuffer, fast enough to animate from the card every frame.
 */
#include <CHGfx.h>
#include "SD/SD.h"
#include "Enums.h"

/* ------------------------------------------------------------------------ */
/* Tuning                                                                    */
/* ------------------------------------------------------------------------ */
/* Header size of the sprite file format. */
#define SPRITE_HEADER_BYTES 2

/* Tail cache, per loaded sprite. A sprite's pixel data rarely ends exactly
 * on a 512-byte block boundary. If only a few bytes spill into its last
 * block, spriteLoad() keeps those bytes in RAM so spriteDraw() can stop the
 * card stream on a block boundary instead of reading (and discarding) a
 * whole extra block every frame. The fire frames are 2 + 4096 bytes: the
 * last 2 pixel bytes would otherwise cost a 512-byte block each frame
 * (~0.17 ms at 24 MHz, ~4% of the frame). 8 covers that with room to
 * spare; each byte here costs one byte of RAM per SpriteFile. */
#ifndef SPRITE_TAIL_MAX
#define SPRITE_TAIL_MAX 8
#endif

/* ------------------------------------------------------------------------ */
/* Results                                                                   */
/* ------------------------------------------------------------------------ */
/* Return status codes for spriteLoad / spriteDraw / drawSpriteFile */
enum SpriteResult {
    SPRITE_OK                 =  0,
    SPRITE_ERR_FILE_OPEN      = -1,
    SPRITE_ERR_HEADER_READ    = -2,
    SPRITE_ERR_BAD_DIMENSIONS = -3,
    SPRITE_ERR_TOO_WIDE       = -4,   /* kept for compatibility; no longer raised */
    SPRITE_ERR_TRUNCATED      = -5,   /* file shorter than its header claims      */
    SPRITE_ERR_FRAGMENTED     = -6,   /* clusters not contiguous: cannot stream   */
    SPRITE_ERR_IO             = -7,   /* card read failed mid-draw                */
    SPRITE_ERR_NOT_LOADED     = -8    /* spriteDraw() on an empty SpriteFile      */
};

/* ------------------------------------------------------------------------ */
/* A sprite located on the card                                              */
/* ------------------------------------------------------------------------ */
struct SpriteFile {
    uint32_t firstBlock;              /* LBA of file byte 0                    */
    uint16_t blocks;                  /* blocks to stream (tail excluded)      */
    uint8_t  w, h;                    /* from the header; 0 = not loaded       */
    uint8_t  tailLen;                 /* pixel bytes held in tail[], 0..MAX    */
    uint8_t  tail[SPRITE_TAIL_MAX];   /* the bytes after the last full block   */
};



/* Open `path`, read its header, and record where its data sits on the card.
 * The file is closed again before returning; the SpriteFile is all that is
 * kept. Fails with SPRITE_ERR_FRAGMENTED if the file is not stored in
 * consecutive clusters (re-copy it to a freshly formatted card). */
int spriteLoad(SpriteFile &spriteFile, const char *path);


/* Draw a loaded sprite into the framebuffer at (x, y).
 * transparent  -1 = opaque; 0..15 = palette index to leave undrawn.
 * Only touches gfx_fb - call gfx_flush*() yourself afterwards. Waits for
 * any async flush in flight first, because it borrows CHGfx's two 512-byte
 * DMA chunk buffers as landing space for the card data. */
int spriteDraw(const SpriteFile &spriteFile, int x, int y, int transparent = -1);

bool loadAll(SpriteFile (&spriteFiles)[Images::Count]);

/* --------------------------------------------------------------------- */
/* SPI1 arbitration                                                       */
/* --------------------------------------------------------------------- */
/*
* The ST7735 and the microSD share SPI1 and want it configured differently.
* The SD side is self-healing: SPIClass::beginTransaction() calls spi_init(),
* which resets and reprograms SPI1 before every card operation. The LCD side
* is not, so spiClaimForLcd() puts the register back the way CHGfx's own
* spiInit() leaves it. Wrap every SD access in sdBegin()/sdEnd().
*
* sdBegin() waits for any in-flight frame DMA first: that DMA owns the bus,
* and the card would otherwise reprogram the peripheral out from under it.
*/
void spiClaimForLcd(void);
void sdBegin(void);
void sdEnd(void);
void sdSeek(File &file, uint64_t pos);

/* Reads a packed 4bpp sprite from `path` and blits it into the CHGfx
 * framebuffer at (x, y).
 *
 * If w and h are left at their default (-1), the file is expected to
 * carry a 2-byte header -- byte 0 width, byte 1 height -- and that header
 * is read and used. If both w and h are passed in (> 0), the header read
 * is skipped entirely and the file is treated as raw, headerless rows:
 * h rows of ceil(w/2) bytes, nothing else.
 *
 * Nothing is sent to the LCD here -- this only touches the in-RAM
 * framebuffer (gfx_blit). Call gfx_flush() yourself once you're done
 * drawing everything for the frame.
 *
 * transparentIndex: -1 (default) draws opaque; 0-15 skips pixels of that
 * palette index instead of drawing them.
 *
 * Returns 0 (SPRITE_OK) on success, or a negative error code on failure.
 * SPRITE_ERR_TOO_WIDE means the sprite's row doesn't fit in SPRITE_BUF_SIZE
 * bytes -- raise that constant, or narrow the sprite. */
/* --- By path: the file is opened and closed for you ---------------- */
int drawSpriteFile(const char *path, int x, int y, int transparentIndex = -1);
int drawSpriteFile(const char *path, int x, int y, int w, int h, int transparentIndex = -1);

/* --- By open File: pass a pointer to a File you've already opened --------
 * The file is NOT closed afterwards, and reading starts at its current
 * position (no seek), so a sprite can sit at an offset inside a larger
 * file, and consecutive calls walk through packed sprites. If the data
 * has no header, pass w and h; for a header, use the short form. A File
 * that isn't open returns SPRITE_ERR_FILE_OPEN. */
int drawSpriteFile(File *file, int x, int y, int w, int h, uint8_t idx, int transparentIndex = -1);

/* buf (full forms only): your own working buffer, declared as
 * `uint8_t buf[SPRITE_BUF_SIZE];`, so it needn't be allocated on the stack
 * per call. nullptr (default) uses an internal stack buffer of that size.
 * To pass a buffer with a headered sprite, use w = -1, h = -1.
 *
 * Examples:
 *   drawSpriteFile("/a.bin", 0, 0, -1);                                    // path, header in file (-1 transparent color)
 *   drawSpriteFile("/a.bin", 0, 0, 16, 16, -1);                            // path, no dimensions header in file (16x16 image, -1 transparent color)
 *   drawSpriteFile(&f, 0, 0, 16, 16, 0, -1);                               // open file, no dimensions header in file (16x16 image, -1 transparent color)
 */
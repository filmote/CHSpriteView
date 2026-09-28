#pragma once

#include <CHGfx.h> 


/* ------------------------------------------------------------------------ */
/* Palette                                                                   */
/* ------------------------------------------------------------------------ */

enum Colors : uint8_t { Transparent, Blue, BLANK_0, Red, Navy,
                 BLANK_2, BLANK_3, BLANK_4, BLANK_5, BLANK_6,
                 BLANK_7, HUD_TEXT, HUD_ACCENT, HUD_DIM, White, Black };

static const uint16_t palette[16] = {
    0x0000, 0x0001, 0x0020, 0xF949, 0xF128,
    0xFD00, 0xFF45, 0xF129, 0xF4E0, 0xF4E1,
    0xF724, 0xF725,
    0xFE60,     
    0x8410,     
    0xFFFF,     
    0x0000
};

enum Images : uint8_t {

    Fire_Start,
        Fire_00 = Fire_Start,
        Fire_01,
        Fire_02,
        Fire_03,
        Fire_04,
        Fire_05,
        Fire_06,
        Fire_07,
        Fire_08,
        Fire_09,
        Fire_10,
        Fire_11,
        Fire_12,
        Fire_13,
        Fire_14,
        Fire_15,
    Fire_End = Fire_15,
    Skull,
    Count

};

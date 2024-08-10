/****************************  format_tables.cpp  ***************************
* Author:        Agner Fog
* date created:  2018-02-18
* Last modified: 2022-12-22
* Version:       1.13
* Project:       Binary tools for ForwardCom instruction set
* Description:
* Format tables used by emulator, assembler, and disassembler
*
* Copyright 2018-2024 GNU General Public License http://www.gnu.org/licenses
*****************************************************************************/

#include "stdafx.h"

// formatList is a list of instruction formats. All format-dependent code should preferably rely on this list.
// The list contains all details about each instruction format.
// This is used by both the assembler, disassembler, and emulator. 
// See definition of structure SFormat in disassem.h.

// A lookup in formatList uses a nested table lookup as follows:
// il.mode.M is used as index into formatI. The indexes have constants FX000, etc.
// formatI contains an index into formatJ and a criterion for division into subgroups.
// formatJ contains an index into formatList or an index into formatJ if further subdivision is needed.

// Indexes into formatList:
const int FX000 = 0;
const int FX010 = FX000 + 1;
const int FX020 = FX010 + 1;
const int FX030 = FX020 + 1;
const int FX040 = FX030 + 1;
const int FX050 = FX040 + 1;
const int FX060 = FX050 + 1;
const int FX070 = FX060 + 1;
const int FX080 = FX070 + 1;
const int FX090 = FX080 + 1;
const int FX100 = FX090 + 1;
const int FX110 = FX100 + 1;
const int FX120 = FX110 + 1;
const int FX130 = FX120 + 1;
const int FX141 = FX130 + 1;
const int FX142 = FX141 + 1;
const int FX143 = FX142 + 1;
const int FX144 = FX143 + 1;
const int FX150 = FX144 + 1;
const int FX160 = FX150 + 1;
const int FX161 = FX160 + 1;
const int FX162 = FX161 + 1;
const int FX163 = FX162 + 1;
const int FX170 = FX163 + 1;
const int FX171 = FX170 + 1;
const int FX172 = FX171 + 1;
const int FX173 = FX172 + 1;
const int FX174 = FX173 + 1;
const int FX175 = FX174 + 1;
const int FX17A = FX175 + 1;
const int FX180 = FX17A + 1;
const int FX200 = FX180 + 1;
const int FX201 = FX200 + 1;
const int FX202 = FX201 + 1;
const int FX203 = FX202 + 1;
const int FX205 = FX203 + 1;
const int FX206 = FX205 + 1;
const int FX207 = FX206 + 1;
const int FX210 = FX207 + 1; 
const int FX220 = FX210 + 1;
const int FX221 = FX220 + 1;
const int FX222 = FX221 + 1;
const int FX223 = FX222 + 1;
const int FX224 = FX223 + 1;
const int FX225 = FX224 + 1;
const int FX226 = FX225 + 1;
const int FX227 = FX226 + 1; 
const int FX230 = FX227 + 1;
const int FX240 = FX230 + 1;
const int FX250 = FX240 + 1;
const int FX251 = FX250 + 1;
const int FX251X= FX251 + 1;
const int FX252 = FX251X+ 1;
const int FX252X= FX252 + 1;
const int FX253 = FX252X+ 1;
const int FX254 = FX253 + 1;
const int FX254X= FX254 + 1;
const int FX255 = FX254X+ 1;
const int FX255X= FX255 + 1;
const int FX256 = FX255X+ 1;   // unused
const int FX257 = FX256 + 1;
const int FX258 = FX257 + 1;
const int FX259 = FX258 + 1;
const int FX25A = FX259 + 1; 
const int FX260 = FX25A + 1; 
const int FX270 = FX260 + 1; 
const int FX280 = FX270 + 1;
const int FX290 = FX280 + 1;
const int FX291 = FX290 + 1;
const int FX300 = FX291 + 1;
const int FX301 = FX300 + 1;
const int FX302 = FX301 + 1;
const int FX303 = FX302 + 1;
const int FX305 = FX303 + 1;
const int FX307 = FX305 + 1;
const int FX310 = FX307 + 1;
const int FX311 = FX310 + 1;
const int FX311X= FX311 + 1; 
const int FX312 = FX311X+ 1;
const int FX313 = FX312 + 1;
const int FX318 = FX313 + 1;
const int FX320 = FX318 + 1;
const int FX321 = FX320 + 1;
const int FX322 = FX321 + 1;
const int FX323 = FX322 + 1;
const int FX325 = FX323 + 1;
const int FX327 = FX325 + 1;
const int FX330 = FX327 + 1;
const int FX380 = FX330 + 1;
const int FXEND = FX380 + 1;

// Indexes into formatJ:
const int FJ140 = 0;
const int FJ160 = FJ140 + 8;
const int FJ167 = FJ160 + 8;
const int FJ170 = FJ167 + 8;
const int FJ171 = FJ170 + 8;
const int FJ172 = FJ171 + 8;
const int FJ174 = FJ172 + 8;
const int FJ200 = FJ174 + 2;
const int FJ220 = FJ200 + 8; 
const int FJ25_ = FJ220 + 8;
const int FJ25J = FJ25_ + 8;
const int FJ250 = FJ25J + 8;
const int FJ251 = FJ250 + 8;
const int FJ252 = FJ251 + 8;
const int FJ254 = FJ252 + 8;
const int FJ254B= FJ254 + 8;
const int FJ255 = FJ254B+ 8;
const int FJ290 = FJ255 + 8;
const int FJ300 = FJ290 + 8; 
const int FJ31_ = FJ300 + 8;
const int FJ31J = FJ31_ + 8;
const int FJ311 = FJ31J + 8;
const int FJ320 = FJ311 + 8;
const int FJEND = FJ320 + 8;


// First list in nested lookup lists. Index is il.mode.M
// Each record contains: criterion and index for next table
// Criterion for lookup into next table: 0 = format table. 
// 1: mode2, 2: op1 / 8, 3: op1 % 8, 4: IM1 % 64 / 8, 5: IM1 % 8, 
// 6: IM12 == 0xFFFF

SFormatIndex formatI[] = {
    {0, FX000}, {0, FX080}, // 0.0, 0.8
    {0, FX010}, {0, FX090}, // 0.1, 0.9
    {0, FX020}, {0, FX020}, // 0.2
    {0, FX030}, {0, FX030}, // 0.3
    {0, FX040}, {0, FX040}, // 0.4
    {0, FX050}, {0, FX050}, // 0.5
    {0, FX060}, {0, FX060}, // 0.6
    {0, FX070}, {0, FX070}, // 0.7
    {0, FX100}, {0, FX180}, // 1.0, 1.8
    {0, FX110}, {0, FX110}, // 1.1 // no 1.9
    {0, FX120}, {0, FX120}, // 1.2

    {0, FX130}, {0, FX130}, // 1.3 
    {2, FJ140}, {2, FJ140}, // 1.4 subdivided by op1 / 8
    {0, FX150}, {0, FX150}, // 1.5
    {2, FJ160}, {2, FJ160}, // 1.6 subdivided by op1 / 8
    {2, FJ170}, {2, FJ170}, // 1.7 subdivided by op1 / 8    

    {1, FJ200}, {0, FX280}, // 2.0 subdivided by mode2, 2.8 
    {0, FX210}, {2, FJ290}, // 2.1, 2.9 subdivided by op1 / 8
    {1, FJ220}, {1, FJ220}, // 2.2 
    {0, FX230}, {0, FX230}, // 2.3
    {0, FX240}, {0, FX240}, // 2.4
    {2, FJ25_}, {2, FJ25_}, // 2.5, subdivided by op1 / 8
    {0, FX260}, {0, FX260}, // 2.6
    {2, FX270}, {2, FX270}, // 2.7
    {1, FJ300}, {0, FX380}, // 3.0, subdivided by mode2, 3.8
    {2, FJ31_}, {2, FJ31_}, // 3.1, subdivided by op1 / 8, there is no format 3.9
    {1, FJ320}, {1, FJ320}, // 3.2, subdivided by mode2
    {0, FX330}, {0, FX330}, // 3.3
    {0, FX380}, {0, FX380}, // reserved for longer instructions
    {0, FX380}, {0, FX380}, // 
    {0, FX380}, {0, FX380}, // 
    {0, FX380}, {0, FX380}  // 
};


// Next level of nested tables.
// Do not add or delete lines here without updating the index constants FJ140 etc. above

SFormatIndex formatJ[FJEND] = {
    //  FJ140: subdivision of format 1.4 by by op1 / 8
    {0, FX141},                                     // 141: 0  - 7
    {0, FX142}, {0, FX142}, {0, FX142},             // 142: 8  - 31
    {0, FX143},                                     // 143: 32 - 39
    {0, FX144}, {0, FX144},                         // 144: 40 - 55
    {0, FX144},                                     // 144: 56 - 63

    //  FJ160: subdivision of format 1.6 by by op1 / 8
    {0, FX160}, {0, FX160}, {0, FX160}, {0, FX160}, {0, FX160}, {0, FX160}, {0, FX160},  // 160: 0 - 55
    {3, FJ167},                                                                          // 161-163: 56-63

    //  FJ167: subdivision of format 1.6:7 by by op1 % 8
    {0, FX161}, {0, FX161}, {0, FX161}, {0, FX161},   // 161: 56 - 59
    {0, FX162}, {0, FX162},                           // 162: 60 - 61
    {0, FX163}, {0, FX163},                           // 163: 62 - 63

    //  FJ170: subdivision of format 1.7 by by op1 / 8
    {0, FX170}, {0, FX170},                           // 150:  0 - 15  1.7D
    {0, FX171}, {0, FX171}, {0, FX171}, {0, FX171},   // 151: 16 - 47  1.7C
    {3, FJ171}, {3, FJ172},                           // FJ171, FJ172: subdivision by op1 % 8

    //  FJ171: subdivision of format 1.7:6 by by op1 % 8
    {0, FX171}, {0, FX171}, {0, FX171}, {0, FX171},        // 172: 48 - 51
    {0, FX17A}, {0, FX17A},                                // 17A: 52 - 53
    {0, FX171}, {0, FX171},                                // 172: 54 - 55

    //  FJ172: subdivision of format 1.7:7 by by op1 % 8
    {0, FX172}, {0, FX172}, {0, FX172}, {0, FX172},        // 152: 56 - 59
    {0, FX173}, {0, FX173}, {0, FX173},                    // 153: 60 - 62
    {8, FJ174},                                            // 154: 63

    //  FJ174: subdivision of format 1.7:63 by by IM12 == 0xFFFF
    {0, FX174}, {0, FX175},                               // 154: 63, trap / filler

    //  FJ200: 2.0 subdivided by mode2
    {0, FX200}, {0, FX201}, {0, FX202}, {0, FX203},        // 2.0.0 - 2.0.3
    {0, FX200}, {0, FX205}, {0, FX206}, {0, FX207},        // 2.0.6 - 2.0.7

    //  FJ220: 2.2 subdivided by mode2
    {0, FX220}, {0, FX221}, {0, FX222}, {0, FX223},        // 2.2.0 - 2.2.3
    {0, FX224}, {0, FX225}, {0, FX226}, {0, FX227},        // 2.2.4 - 2.2.7

    //  FJ25_: subdivision of format 2.5 by op1 / 8
    {3, FJ25J}, {0, FX258}, {0, FX259}, {0, FX25A},        // jump, 2.5.8 - ...
    {0, FX25A}, {0, FX25A}, {0, FX25A}, {0, FX25A}, 
    //  FJ25J: subdivision of format 2.5 jump instructions by op1 % 8
    {4, FJ250}, {4, FJ251}, {4, FJ252}, {0, FX253},
    {4, FJ254}, {4, FJ255}, {0, FX256}, {0, FX257},
    //  FJ250: subdivision of format 2.5.0 jump instructions by opj / 8
    {0, FX250}, {0, FX250}, {0, FX250}, {0, FX250}, 
    {0, FX250}, {0, FX250}, {0, FX250}, {0, FX250},
    //  FJ251: subdivision of format 2.5.1 jump instructions by opj / 8
    {0, FX251}, {0, FX251}, {0, FX251}, {0, FX251}, 
    {0, FX251}, {0, FX251}, {0, FX251}, {0, FX251X},
    //  FJ252: subdivision of format 2.5.2 jump instructions by opj / 8
    {0, FX252}, {0, FX252}, {0, FX252}, {0, FX252}, 
    {0, FX252}, {0, FX252}, {0, FX252}, {0, FX252X}, 
    //  FJ254: subdivision of format 2.5.4 jump instructions by opj / 8
    {0, FX254}, {0, FX254}, {0, FX254}, {0, FX254},       // opj 0 - 31
    {0, FX254}, {0, FX254}, {5, FJ254B},{0, FX254},       // opj 32 - 63
    //  FJ254B: subdivision of format 2.5.4:6 by by opj % 8
    {0, FX254}, {0, FX254}, {0, FX254}, {0, FX254},        // 254: 48 - 51
    {0, FX254X},{0, FX254X},                               // 254: 52 - 53
    {0, FX254}, {0, FX254},                                // 254: 54 - 55
    //  FJ255: subdivision of format 2.5.5 jump instructions by opj / 8
    {0, FX255}, {0, FX255}, {0, FX255}, {0, FX255}, 
    {0, FX255}, {0, FX255}, {0, FX255}, {0, FX255X},
    //  FJ290: subdivision of format 2.9 by by op1 / 8
    {0, FX290}, {0, FX290}, {0, FX290}, {0, FX290},         // 2.9.0
    {0, FX291}, {0, FX291}, {0, FX291}, {0, FX291},         // 2.9.1

    //  FJ300: 3.0 subdivided by mode2
    {0, FX300}, {0, FX301}, {0, FX302}, {0, FX303},        // 3.0.0 - 3.0.3
    {0, FX300}, {0, FX305}, {0, FX300}, {0, FX307},        // 3.0.4 - 3.0.7

    //  FJ31_: subdivision of format 3.1 instructions by op1 / 8
    {3, FJ31J}, {0, FX318}, {0, FX318}, {0, FX318}, 
    {0, FX318}, {0, FX318}, {0, FX318}, {0, FX318},

    //  FJ31J: subdivision of format 3.1.0-8 jump instructions by op1 % 8
    {0, FX310}, {4, FJ311}, {0, FX312}, {0, FX313}, 
    {0, FX310}, {0, FX310}, {0, FX310}, {0, FX310}, 

    //  FJ311 subdivision of format 3.1.1 instructions by opj / 8
    {0, FX311}, {0, FX311}, {0, FX311}, {0, FX311}, 
    {0, FX311}, {0, FX311}, {0, FX311}, {0, FX311X}, 

    //  FJ320: 3.2 subdivided by mode2
    {0, FX320}, {0, FX321}, {0, FX322}, {0, FX323},        // 3.2.0 - 3.2.3
    {0, FX320}, {0, FX325}, {0, FX320}, {0, FX327}         // 3.2.4 - 3.2.7
};


// Definition of bit fields in all formats. The members of SFormat are defined in disassem.h.
// Do not add or delete lines in formatList without updating the index constants FX000 etc. above.

const SFormat formatList[FXEND] = {
//  form   cat tmpl  opav  ot     jump   addr  imm         v     mem   scl fi  xt
    {0x000, 3, 0xA,  0xB0, 0x00,  0, 0,  0, 0, 0, 0, 0,    0,    0x00, 0,   0,  1},
    {0x010, 3, 0xB,  0xA1, 0x00,  0, 0,  0, 0, 1, 0, 0,    0,    0x00, 0,   1,  1},
    {0x020, 3, 0xA,  0xB0, 0x00,  0, 0,  0, 0, 0, 0, 0,    1,    0x00, 0,   2,  1},
    {0x030, 3, 0xB,  0xA1, 0x00,  0, 0,  0, 0, 1, 0, 0,    1,    0x00, 0,   3,  1},
    {0x040, 3, 0xA,  0x82, 0x00,  0, 0,  0, 0, 0, 0, 0,    3,    0x02, 0,   4,  1},
    {0x050, 3, 0xA,  0x82, 0x00,  0, 0,  0, 0, 0, 0, 0,    3,    0x06, 4,   5,  1},
    {0x060, 3, 0xA,  0x82, 0x00,  0, 0,  0, 0, 0, 0, 0,    1,    0x06, 2,   6,  1},
    {0x070, 3, 0xB,  0x82, 0x00,  0, 0,  1, 0, 0, 0, 0,    1,    0x12, 1,   7,  1},
    {0x080, 3, 0xA,  0x82, 0x00,  0, 0,  0, 0, 0, 0, 0,    0,    0x06, 2,   8,  1},
    {0x090, 3, 0xB,  0x82, 0x00,  0, 0,  1, 0, 0, 0, 0,    0,    0x12, 1,   9,  1},

    // FX100
    {0x100, 1, 0xA,  0xB0, 0x00,  0, 0,  0, 0, 0, 0, 0,    0,    0x00, 0,   0,  4},  // Format 1.0 A
    {0x110, 1, 0xC,  0x81, 0x32,  0, 0,  0, 0, 2, 0, 0,    0,    0x00, 0,   0,  5},  // Format 1.1 C
    {0x120, 1, 0xA,  0xB0, 0x00,  0, 0,  0, 0, 0, 0, 0,    1,    0x00, 0,   0,  6},  // Format 1.2 A
    {0x130, 1, 0xB,  0xA1, 0x00,  0, 0,  0, 0, 1, 0, 0,    1,    0x00, 0,   0,  7},  // Format 1.3 B
    {0x141, 1, 0xC,  0x81, 0x11,  0, 0,  0, 0, 2, 0, 0,    1,    0x00, 0,   0,  8},  // Format 1.4 C, in16. op1 = 0-7
    {0x142, 1, 0xC,  0x81, 0x32,  0, 0,  0, 0, 2, 0, 0,    1,    0x00, 0,   0,  8},  // Format 1.4 C, int32/int64. op1 = 8-31
    {0x143, 1, 0xC,  0x81, 0x35,  0, 0,  0, 0, 2, 0, 0,    1,    0x00, 0,   0,  8},  // Format 1.4 C, float/double. op1 = 32-39
    {0x144, 1, 0xC,  0x81, 0x11,  0, 0,  0, 0, 2, 0, 0,    1,    0x00, 0,   0,  8},  // Format 1.4 C, half. op1 = 40-63
    {0x150, 1, 0xB,  0xA1, 0x00,  0, 0,  0, 0, 1, 0, 0,    1,    0x00, 0,   0,  8},  // Format 1.5 B. op1 = 0-63

    {0x160, 4, 0xB,  0xA0, 0x00,  1, 0,  0, 0, 0, 0, 0,    0x10, 0x00, 0,   0,  2},  // Format 1.6 jump. op1 = 0-55
    {0x161, 4, 0xB,  0x02, 0x13,  0, 0,  1, 0, 0, 0, 0,    0,    0x12, 1,  16,  2},  // Format 1.6.1 jump [mem] like format 0.9. op1 = 56-59
    {0x162, 4, 0xA,  0x82, 0x00,  0, 0,  0, 0, 0, 0, 0,    0,    0x06, 2,  17,  2},  // Format 1.6.2 jump (refpoint, [mem]). op1 = 60-61
    {0x163, 4, 0xA,  0xB0, 0x00,  0, 0,  0, 0, 0, 0, 0,    0,    0x00, 0,  18,  2},  // Format 1.6.3 return, sys_call. op1 = 62-63
    {0x170, 4, 0xD,  0x00, 0x13,  3, 0,  0, 0, 0, 0, 1,    0,    0x00, 0,  20,  3},  // Format 1.7 D. op1 = 0-15
    {0x171, 4, 0xC,  0x81, 0x12,  1, 0,  0, 0, 1, 1, 0,    0x10, 0x00, 0,   1,  2},  // Format 1.7 C. op1 = 16-55, except 52-53
    {0x172, 0, 0xC,  0x81, 0x13,  2, 0,  0, 0, 0, 0, 0,    0,    0x00, 0,  21,  2},  // Format 1.7.2 C, unconditional 16 bit jump. op1 = 56-59. Unused
    {0x173, 4, 0xC,  0x80, 0x13,  0, 0,  0, 0, 0, 0, 0,    0,    0x00, 0,  22,  2},  // Format 1.7.3 C, jump to register, sys_return. op1 = 60-62
    {0x174, 4, 0xC,  0x01, 0x13,  0, 0,  0, 0, 2, 0, 0,    0,    0x00, 0,  23,  2},  // Format 1.7.4 C, trap. op1 = 63
    {0x175, 4, 0xC,  0x00, 0x13,  0, 0,  0, 0, 2, 0, 0x40, 0,    0x00, 0,  24,  2},  // Format 1.7.5 C, filler. op1 = 63. IM12 = 0xFFFF
    {0x17A, 4, 0xC,  0x81, 0x13,  1, 0,  0, 0, 1, 1, 0,    0,    0x00, 0,  25,  2},  // Format 1.7.A C, sub_maxlen. op1 = 52-53
    {0x180, 1, 0xB,  0xA1, 0x00,  0, 0,  0, 0, 1, 0, 0,    0,    0x00, 0,   0,  9},

// FX200
    {0x200, 3, 0xE,  0xD2, 0x00,  0, 0,  2, 4, 0, 0, 2,    0,    0x12, 0, 16,  1},
    {0x201, 3, 0xE,  0xC2, 0x00,  0, 0,  2, 4, 0, 0, 2,    0,    0x16, 0, 17,  1},
    {0x202, 3, 0xE,  0xC2, 0x00,  0, 0,  2, 4, 0, 0, 2,    0,    0x16, 2, 18,  1},
    {0x203, 3, 0xE,  0xC2, 0x00,  0, 0,  2, 4, 0, 0, 2,    0,    0x26, 2, 19,  1},
    {0x205, 3, 0xE,  0xC3, 0x00,  0, 0,  2, 4, 1, 6, 0x100,0,    0x16, 2, 21,  1},
    {0x206, 3, 0xE,  0xF0, 0x00,  0, 0,  0, 0, 0, 0, 2,    0,    0x00, 0, 22,  1},
    {0x207, 3, 0xE,  0xF1, 0x00,  0, 0,  0, 0, 2, 4, 6,    0,    0x00, 0, 23,  1},
    {0x210, 3, 0xA,  0x92, 0x00,  0, 0,  4, 4, 0, 0, 0,    0,    0x12, 0, 13,  1},
    {0x220, 3, 0xE,  0xC2, 0x00,  0, 0,  2, 4, 0, 0, 2,    5,    0x12, 0, 24,  1},
    {0x221, 3, 0xE,  0xC2, 0x00,  0, 0,  2, 4, 0, 0, 2,    3,    0x12, 0, 25,  1},
    {0x222, 3, 0xE,  0xC2, 0x00,  0, 0,  2, 4, 0, 0, 2,    1,    0x16, 2, 26,  1},
    {0x223, 3, 0xE,  0xC2, 0x00,  0, 0,  2, 4, 0, 0, 2,    1,    0x26, 2, 27,  1},
    {0x224, 3, 0xE,  0xC2, 0x00,  0, 0,  2, 4, 0, 0, 2,    3,    0x16, 4, 28,  1},
    {0x225, 3, 0xE,  0xC3, 0x00,  0, 0,  2, 4, 1, 6, 0x100,3,    0x16, 0, 29,  1},
    {0x226, 3, 0xE,  0xF0, 0x00,  0, 0,  0, 0, 0, 0, 2,    1,    0x00, 0, 30,  1},
    {0x227, 3, 0xE,  0xF1, 0x00,  0, 0,  0, 0, 2, 4, 6,    1,    0x00, 0, 31,  1},
    {0x230, 3, 0xA,  0xB1, 0x00,  0, 0,  0, 0, 4, 4, 0,    1,    0x00, 0, 14,  1},
    {0x240, 3, 0xA,  0x82, 0x00,  0, 0,  4, 4, 0, 0, 0,    3,    0x12, 0, 15,  1},

    // FX250
//  form   cat tmpl  opav  ot     jump   addr  imm         v     mem  scl fi   xt
    {0x250, 4, 0xA,  0xB0, 0x00,  3, 4,  0, 0, 0, 0, 0x90, 0x10, 0x00, 0,  4,  2},  // Format 2.5.0. jump w three registers, 24-bit offset
    {0x251, 4, 0xB,  0xA1, 0x00,  2, 6,  0, 0, 2, 4, 0x80, 0x10, 0x00, 0,  5,  2},  // Format 2.5.1. jump w 16-bit imm + 16 bit offset. op1 = 0-55
    {0x251, 4, 0xB,  0xA1, 0x00,  0, 0,  0, 0, 4, 4, 0x80, 0,    0x00, 0, 28,  2},  // Format 2.5.1x system call. op1 = 56-63
    {0x252, 4, 0xB,  0x82, 0x00,  2, 6,  2, 4, 0, 0, 0x80, 0,    0x12, 0,  6,  2},  // Format 2.5.2. jump w memory operand and 16 bit jump offset
    {0x252, 4, 0xB,  0x82, 0x00,  0, 0,  4, 4, 0, 0, 0x80, 0,    0x12, 0, 29,  2},  // Format 2.5.2x jump [mem+i]. op1 = 56-63
    {0x253, 0, 0xA,  0xB0, 0x00,  3, 4,  0, 0, 0, 0, 0x90, 0x10, 0x00, 0,  7,  2},  // Format 2.5.3. unused
    {0x254, 4, 0xC,  0x81, 0x12,  4, 4,  0, 0, 1, 1, 0x80, 0x10, 0x00, 0,  8,  2},  // Format 2.5.4. jump w 8-bit imm + 32 bit offset. op1 = 0-55
    {0x254, 4, 0xC,  0x81, 0x13,  4, 4,  0, 0, 1, 1, 0x80, 0,    0x00, 0, 30,  2},  // Format 2.5.4x Same as 2.5.4, 64 bit operand size
    {0x255, 4, 0xC,  0x81, 0x12,  1, 1,  0, 0, 4, 4, 0x80, 0x10, 0x00, 0,  9,  2},  // Format 2.5.5. jump w 32-bit imm + 8 bit offset. op1 = 0-55
    {0x255, 4, 0xC,  0x81, 0x13,  0, 0,  0, 0, 4, 4, 0x80, 0,    0x00, 0, 31,  2},  // Format 2.5.5x conditional trap. op1 = 56-63
    {0x256, 0, 0xB,  0xA0, 0x00,  4, 4,  0, 0, 0, 0, 0x80, 0x10, 0x00, 0, 10,  2},  // Format 2.5.6. unused
    {0x257, 4, 0xC,  0x01, 0x13,  0, 0,  0, 0, 2, 0, 0xC0, 0,    0x00, 0, 32,  2},  // Format 2.5.7. system call w 16+32 bit imm 
    {0x258, 1, 0xB,  0x83, 0x00,  0, 0,  1, 0, 4, 4, 0,    0x10, 0x12, 1,  0, 10},  // single format instructions, format 2.5 B with both memory and immediate: store. op1 = 0-15
    {0x259, 1, 0xB,  0x82, 0x00,  0, 0,  4, 4, 0, 0, 0,    0,    0x12, 0,  0, 10},  // single format instructions, format 2.5 B with memory: compare_swap. op1 = 16-23
    {0x25A, 1, 0xA,  0x92, 0x00,  0, 0,  4, 4, 0, 0, 0,    1,    0x12, 0,  0, 10},  // Miscellaneous single format instructions, format 2.5 A with memory operand. op1 = 24-63
    {0x260, 1, 0xA,  0xB1, 0x00,  0, 0,  0, 0, 4, 4, 0,    1,    0x00, 0,  0, 11},
    {0x270, 0, 0xA,  0x00, 0x00,  0, 0,  0, 0, 0, 0, 0,    0,    0x00, 0,  0,  0},  // currently unused
    {0x280, 3, 0xA,  0xB1, 0x00,  0, 0,  0, 0, 4, 4, 0,    0,    0x00, 0, 12,  1},
    {0x290, 1, 0xA,  0xB1, 0x00,  0, 0,  0, 0, 4, 4, 0,    0,    0x00, 0,  0, 12},
    {0x291, 1, 0xA,  0xA2, 0x00,  0, 0,  4, 4, 0, 0, 0,    0,    0x12, 0,  0, 12},

    // FX300
    {0x300, 3, 0xE,  0xD2, 0x00,  0, 0,  4, 8, 0, 0, 2,    0,    0x12, 0, 36,  1},
    {0x301, 0, 0xE,  0xD2, 0x00,  0, 0,  4, 8, 0, 0, 2,    0,    0x12, 0, 36,  1},  // Format 3.0.1. unused
    {0x302, 3, 0xE,  0xC2, 0x00,  0, 0,  4, 8, 0, 0, 2,    0,    0x16, 2, 38,  1},
    {0x303, 3, 0xE,  0xC2, 0x00,  0, 0,  4, 8, 0, 0, 2,    0,    0x26, 2, 39,  1},
    {0x305, 3, 0xE,  0xC3, 0x00,  0, 0,  2, 4, 4, 8, 2,    0,    0x16, 2, 41,  1},
    {0x307, 3, 0xE,  0xF1, 0x00,  0, 0,  0, 0, 4, 8, 0xA,  0,    0x00, 0, 43,  1},

    {0x310, 4, 0xA,  0x92, 0x00,  3, 4,  4, 8, 0, 0, 0x90, 0x10, 0x12, 0, 12,  2},  // Jump w memory operand and 32 bit jump offset
    {0x311, 4, 0xB,  0xA1, 0x00,  4, 4,  0, 0, 4, 8, 0x80, 0x10, 0x00, 0, 13,  2},  // Jump w 32 bit offset + 32-bit imm. op1 = 0-55
    {0x311, 4, 0xB,  0xA1, 0x13,  0, 0,  0, 0, 8, 4, 0x80, 0,    0x00, 0, 40,  2},  // 3.1.1X. Syscall. op1 = 56-63
    {0x312, 0, 0xB,  0xA0, 0x00,  4, 4,  0, 0, 0, 0, 0x80, 0x10, 0x00, 0, 14,  2},  // Format 3.1.2. unused
    {0x313, 0, 0xB,  0xD2, 0x00,  0, 0,  4, 8, 0, 0, 2,    0,    0x12, 0, 15,  2},  // Format 3.1.3. unused

    {0x318, 1, 0xA,  0xB1, 0x00,  0, 0,  0, 0, 8, 4, 0,    1,    0x00, 0,  0, 13},  // Triple size single format instructions. op1 = 8-15
    {0x320, 3, 0xE,  0xC2, 0x00,  0, 0,  4, 8, 0, 0, 2,    5,    0x12, 0, 44,  1},
    {0x321, 3, 0xE,  0xC2, 0x00,  0, 0,  4, 8, 0, 0, 2,    3,    0x12, 0, 45,  1},
    {0x322, 3, 0xE,  0xC2, 0x00,  0, 0,  4, 8, 0, 0, 2,    1,    0x16, 2, 46,  1},
    {0x323, 3, 0xE,  0xC2, 0x00,  0, 0,  4, 8, 0, 0, 2,    1,    0x26, 2, 47,  1},
    {0x325, 3, 0xE,  0xC3, 0x00,  0, 0,  2, 4, 4, 8, 2,    3,    0x12, 0, 49,  1},
    {0x327, 3, 0xE,  0xF1, 0x00,  0, 0,  0, 0, 4, 8, 0xA,  1,    0x00, 0, 51,  1},
    {0x330, 3, 0xA,  0xB1, 0x00,  0, 0,  0, 0, 8, 4, 0x10, 1,    0x00, 0, 34,  1},  // 340 - 370 do not exist. reserved for larger instruction sizes
    {0x380, 3, 0xA,  0xB1, 0x00,  0, 0,  0, 0, 8, 4, 0x10, 0,    0x00, 0, 32,  1}
};

uint32_t formatListSize = TableSize(formatList);  // export size to other modules

// Data size for each operant type, max = 8
const uint32_t dataSizeTable[8] = {1, 2, 4, 8, 16, 4, 8, 16};   // data size in bytes
const uint32_t dataSizeTableMax8[8] = {1, 2, 4, 8, 8, 4, 8, 8}; // same, max 8
const uint32_t dataSizeTableLog[8] = {0, 1, 2, 3, 4, 2, 3, 4};  // log2(dataSizeTable)
const uint32_t dataSizeTableBits[8] = {8, 16, 32, 64, 128, 32, 64, 128};   // data size in bits
const uint64_t dataSizeMask[8] = {0xFF, 0xFFFF, 0xFFFFFFFFU, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFU, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF};



// Check integrity of format lists
void checkFormatListIntegrity() {
    if (FXEND != TableSize(formatList)) {
        printf("\nInternal error in formatList");
        exit(1);
    }
    if (FJEND != TableSize(formatJ)) {
        printf("\nInternal error in formatJ");
        exit(1);
    }
    if (sizeof(formatI) != 128) {
        printf("\nInternal error in formatI");
        exit(1);
    }
}


// Look up format in nested format lists.
// The parameter is the first 64 bits of the instruction
// The return value is a structure containing all information about the instruction format.
// (This method is optimized for speed for the sake of the emulator. The tables use
//  a minimum of cache size).
uint32_t lookupFormat(uint64_t instruct) {
    // Index into formatI is il.mode.M
    uint8_t il_mode_m = (instruct >> 26 & 0x3E) | (instruct >> 15 & 1);
    uint8_t crit = formatI[il_mode_m].crit;
    uint8_t t    = formatI[il_mode_m].index;
    uint8_t i;
    STemplate F;
    F.q = instruct;

    // Nested table lookup
    while (crit) {
        // Index into next table determined by criterion
        switch (crit) {
        case 1:  // mode2
            i = F.a.mode2; // instruct >> 59 & 7;
            break;
        case 2:  // op1 / 8 
            i = instruct >> 24 & 7;
            break;
        case 3:  // op1 % 8
            i = instruct >> 21 & 7;
            break;
        case 4:  // IM1 / 8 % 8
            i = instruct >> 3 & 7;
            break;
        case 5:  // IM1 % 8, 
            i = instruct & 7;
            break;
        case 6:  // op1 / 4. unused
            i = instruct >> 23 & 7;
            break;
        case 7:  // op1 / 2 % 16
            i = instruct >> 22 & 0xF;
            break;
        case 8:  // IM12 == 0xFFFF
            i = (instruct & 0xFFFF) == 0xFFFF;
            break;
        default:  // Error. Should never occur
            printf("\nInternal error in formatList");
            return FX380;
        }
        t += i;
        if (t >= FJEND) return FX380;  // Error. Should never occur        
        crit = formatJ[t].crit;
        t    = formatJ[t].index;
    }
    if (t >= FXEND) return FX380;  // Error. Should never occur

    return t;
}

// Table of tables, indexed by fInstr->exeTable
PFunc * metaFunctionTable[14] = {
    0, funcTab1, funcTab2, funcTab3, funcTab4, funcTab5, funcTab6, funcTab7,
    funcTab8, funcTab9, funcTab10, funcTab11, funcTab12, funcTab13
};

// constants used in numOperands table below 
const uint16_t D = 0x022;  // double size operation. take two vector elements at a time
const uint16_t M = 0x10A;  // ignore mask. call instruction function even if mask = 0. has option bits in E formats
const uint16_t m = 0x00A;  // RT is not a vector register
const uint16_t L = 0x01A;  // ignore mask. RT is not a vector, or destination is not a vector. vector length is determined by the instruction function
const uint16_t l = 0x019;  // same as L, single operand
const uint16_t N = 0x01B;  // same as L, three operands
const uint16_t A = 0x041;  // one operand, don't read memory operand
const uint16_t E = 0x05A;  // two operands, don't read memory operand
const uint16_t S = 0x0C9;  // store: one operand, don't read memory operand, ignore mask, don't modify RD
const uint16_t o = 0x301;  // one operand, has option bits
const uint16_t O = 0x302;  // two operands, has option bits
const uint16_t P = 0x303;  // three operands, has option bits
const uint16_t B = 0x30B;  // three operands, has option bits, call even if mask 0
const uint16_t C = 0x30C;  // four operands, has option bits, call even if mask 0

// Table of number of operands for each instruction
// bit 0-2: number of source operands
// bit 3: call execution function even if mask = 0
// bit 4: RT is not a vector register. vector length is determined by the instruction function
// bit 5: take two vector elements at a time
// bit 6: don't read memory operand before calling execution function
// bit 7: RD is unchanged (not destination)
// bit 8: IM5 in format E contains option bits, not shift count for integer operands
// bit 9: IM5 in format E contains option bits for floating point operands (this info is not used)
uint16_t numOperands[15][64] = {
    {0},  //          8                16               24               32               40               48               56
    {8,S,1,o,1,O,2,M, 2,2,2,2,2,2,O,O, O,O,2,2,O,O,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,M, M,M,2,2,2,2,2,2, P,P,P,P,3,3,3,2, 2,2,2,2,2,2,2,2},  // 1:  multi-format instructions
    {2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2},  // 2:  conditional and indirect jump instructions
    {1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},  // 3:  simple jump and call instructions
    {0,1,1,1,1,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2},  // 4:  format 1.0
    {1,1,1,1,1,1,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2},  // 5:  format 1.1
    {9,9,L,L,3,L,L,L, l,L,L,L,L,L,L,L, L,L,L,L,1,1,2,2, D,D,D,D,1,2,2,2, 2,2,2,2,2,2,D,D, D,D,D,D,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,L,N,L,N,L,N},  // 6:  format 1.2
    {l,l,L,L,N,2,L,L, 2,2,2,2,2,2,2,2, 2,2,2,1,2,2,1,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, N,N,L,2,2,2,2,2},  // 7:  format 1.3
    {1,2,2,2,2,2,2,2, 1,1,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 1,1,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2},  // 8:  format 1.4
    {2,3,2,2,1,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, m,m,m,m,m,m,m,m, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 3,3,2,2,2,2,2,3},  // 9:  format 1.8

    {2,2,2,2,2,2,2,2, A,2,2,2,2,2,2,2, 2,2,3,2,2,2,2,2, 2,2,2,2,2,2,2,2, N,2,2,2,2,2,2,2, E,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2},  //10:  format 2.5
    {l,L,2,2,2,2,L,2, N,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2},  //11:  format 2.6
    {1,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, A,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2},  //12:  format 2.9
    {2,2,2,l,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,L,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2},  //13:  format 3.1
    {0}
};

// Table of number of operands for each instruction, Format 2.0.7, op2 = 1
uint16_t numOperands2071[64] = {
  // 0                8                16               24               32               40               48               56
     P,3,3,3,3,3,3,3, 3,3,3,3,3,3,3,3, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0};

// Table of number of operands for each instruction, Format 2.2.6, op2 = 1
uint16_t numOperands2261[64] = {
  // 0                8                16               24               32               40               48               56
     N,N,N,0,0,0,0,0, C,3,3,3,3,3,3,3, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0};

// Table of number of operands for each instruction, Format 2.2.7, op2 = 1
uint16_t numOperands2271[64] = {
  // 0                8                16               24               32               40               48               56
     P,P,3,3,3,3,3,3, N,N,3,3,3,3,3,3, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0};

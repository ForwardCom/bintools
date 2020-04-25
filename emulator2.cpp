/****************************  emulator2.cpp  ********************************
* Author:        Agner Fog
* date created:  2018-02-18
* Last modified: 2018-03-30
* Version:       1.01
* Project:       Binary tools for ForwardCom instruction set
* Description:
* Emulator: Format tables used by emulator, assembler, and disassembler
*
* Copyright 2018 GNU General Public License http://www.gnu.org/licenses
*****************************************************************************/

#include "stdafx.h"

// formatList is a list of instruction formats. All format-dependent code should preferably rely on this list.
// The list contains all details about each instruction format.
// This is used by both the assembler, disassembler, and emulator. 
// See definition of structure SFormat in disassem.h.

// A lookup in formatList uses a nested table lookup as follows:
// il.mode.M is used as index into formatI
// formatI contains an index into formatJ and a criterion for subdivision
// formatJ contains an index into formatList or a further index into formatJ
// if more subdivision is needed

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
const int FX131 = FX130 + 1;
const int FX132 = FX131 + 1;
const int FX133 = FX132 + 1;
const int FX140 = FX133 + 1;
const int FX141 = FX140 + 1;
const int FX142 = FX141 + 1;
const int FX143 = FX142 + 1;
const int FX150 = FX143 + 1;
const int FX151 = FX150 + 1;
const int FX152 = FX151 + 1;
const int FX153 = FX152 + 1;
const int FX154 = FX153 + 1;
const int FX155 = FX154 + 1;
const int FX160 = FX155 + 1;
const int FX168 = FX160 + 1;
const int FX16E = FX168 + 1;
const int FX170 = FX16E + 1;
const int FX172 = FX170 + 1;
const int FX174 = FX172 + 1;
const int FX17C = FX174 + 1;
const int FX17E = FX17C + 1;
const int FX180 = FX17E + 1;
const int FX200 = FX180 + 1;
const int FX201 = FX200 + 1;
const int FX202 = FX201 + 1;
const int FX203 = FX202 + 1;
const int FX206 = FX203 + 1;
const int FX207 = FX206 + 1;
const int FX210 = FX207 + 1; 
const int FX220 = FX210 + 1;
const int FX221 = FX220 + 1;
const int FX222 = FX221 + 1;
const int FX223 = FX222 + 1;
const int FX224 = FX223 + 1;
const int FX226 = FX224 + 1;
const int FX227 = FX226 + 1; 
const int FX230 = FX227 + 1;
const int FX240 = FX230 + 1;
const int FX250 = FX240 + 1;
const int FX2501= FX250 + 1;
const int FX251 = FX2501+ 1;
const int FX2511= FX251 + 1;
const int FX252 = FX2511+ 1;
const int FX253 = FX252 + 1;
const int FX2531= FX253 + 1;
const int FX254 = FX2531+ 1;
const int FX258 = FX254 + 1;
const int FX259 = FX258 + 1;
const int FX25A = FX259 + 1;
const int FX260 = FX25A + 1; 
const int FX270 = FX260 + 1; 
const int FX280 = FX270 + 1;
const int FX290 = FX280 + 1;
const int FX291 = FX290 + 1;
const int FX300 = FX291 + 1;
const int FX302 = FX300 + 1;
const int FX303 = FX302 + 1;
const int FX307 = FX303 + 1;
const int FX310 = FX307 + 1;
const int FX311 = FX310 + 1;
const int FX318 = FX311 + 1;
const int FX320 = FX318 + 1;
const int FX321 = FX320 + 1;
const int FX322 = FX321 + 1;
const int FX323 = FX322 + 1;
const int FX327 = FX323 + 1;
const int FX330 = FX327 + 1;
const int FX380 = FX330 + 1;
const int FXEND = FX380 + 1;

// Indexes into formatJ:
const int FJ130 = 0;
const int FJ140 = FJ130 + 8;
const int FJ147 = FJ140 + 8;
const int FJ150 = FJ147 + 8;
const int FJ152 = FJ150 + 8;
const int FJ154 = FJ152 + 8;
const int FJ160 = FJ154 + 2;
const int FJ200 = FJ160 + 16;
const int FJ220 = FJ200 + 8; 
const int FJ25_ = FJ220 + 8;
const int FJ25J = FJ25_ + 8;
const int FJ250 = FJ25J + 8;
const int FJ251 = FJ250 + 8;
const int FJ253 = FJ251 + 8;
const int FJ290 = FJ253 + 8;
const int FJ300 = FJ290 + 8; 
const int FJ31_ = FJ300 + 8;
const int FJ31J = FJ31_ + 8;
const int FJ310 = FJ31J + 8;
const int FJ320 = FJ310 + 8;
const int FJEND = FJ320 + 8;


// First list in nested lookup lists. Index is il.mode.M
// Each record contains: criterion and index for next table
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
    {2, FJ130}, {2, FJ130}, // 1.3 subdivided by op1 / 8
    {2, FJ140}, {2, FJ140}, // 1.4 subdivided by op1 / 8
    {2, FJ150}, {2, FJ150}, // 1.5 subdivided by op1 / 8
    {7, FJ160}, {7, FJ160}, // 1.6 tiny instruction subdivided by op1 / 2
    {7, FJ160}, {7, FJ160}, // 1.7 tiny instruction subdivided by op1 / 2
    {1, FJ200}, {0, FX280}, // 2.0 subdivided by mode2, 2.8 
    {0, FX210}, {2, FJ290}, // 2.1, 2.9 subdivided by op1 / 8
    {1, FJ220}, {1, FJ220}, // 2.2 
    {0, FX230}, {0, FX230}, // 2.3
    {0, FX240}, {0, FX240}, // 2.4
    {2, FJ25_}, {2, FJ25_}, // 2.5, subdivided by op1 / 8
    {0, FX260}, {0, FX260}, // 2.6
    {2, FX270}, {2, FX270}, // 2.7
    {1, FJ300}, {0, FX380}, // 3.0 subdivided by mode2, 3.8
    {2, FJ31_}, {2, FJ31_}, // 3.1, subdivided by op1 / 8, no 3.9
    {1, FJ320}, {1, FJ320}, // 3.2  subdivided by mode2
    {0, FX330}, {0, FX330}, // 3.3
    {0, FX380}, {0, FX380}, // reserved for longer instructions
    {0, FX380}, {0, FX380}, // 
    {0, FX380}, {0, FX380}, // 
    {0, FX380}, {0, FX380}  // 
};


// Next level of nested tables
SFormatIndex formatJ[FJEND] = {
    //  FJ130: subdivision of format 1.3 by by op1 / 8
    {0, FX130}, {0, FX130}, {0, FX130}, {0, FX130}, // 130: 0  - 31
    {0, FX131},                                     // 131: 32 - 39
    {0, FX132}, {0, FX132},                         // 132: 40 - 55
    {0, FX133},                                     // 133: 56 - 63
    //  FJ140: subdivision of format 1.4 by by op1 / 8
    {0, FX140}, {0, FX140}, {0, FX140}, {0, FX140}, {0, FX140}, {0, FX140}, {0, FX140},  // 140: 0 - 55
    {3, FJ147},                                                                          // 141-143: 56-63
    //  FJ147: subdivision of format 1.4:7 by by op1 % 8
    {0, FX141}, {0, FX141}, {0, FX141}, {0, FX141},   // 141: 56 - 59
    {0, FX142}, {0, FX142},                           // 142: 60 - 61
    {0, FX143}, {0, FX143},                           // 143: 62 - 63
    //  FJ150: subdivision of format 1.5 by by op1 / 8
    {0, FX150}, {0, FX150},                           // 150: 0 - 15  1.5D
    {0, FX151}, {0, FX151}, {0, FX151}, {0, FX151}, {0, FX151}, // 151: 16 - 55  1.5C
    {3, FJ152},                                                 // 152: subdivision by op1 % 8
    //  FJ152: subdivision of format 1.5:7 by by op1 % 8
    {0, FX152}, {0, FX152}, {0, FX152}, {0, FX152},        // 152: 56 - 59
    {0, FX153}, {0, FX153}, {0, FX153},                    // 153: 60 - 62
    {8, FJ154},                                            // 154: 63
    //  FJ154: subdivision of format 1.5:63 by by IM12 == 0xFFFF
    {0, FX154}, {0, FX155},                               // 154: 63, trap / filler
    //  FJ160: subdivision of tiny instructions by op1 / 2
    {0, FX160}, {0, FX160}, {0, FX160}, {0, FX160},        // 160: 0 - 7
    {0, FX168}, {0, FX168}, {0, FX168}, {0, FX16E},        // 168: 8 - 13, 16E: 14 - 15
    {0, FX170}, {0, FX172}, {0, FX174}, {0, FX174},        // 170: 16-17, 172: 18-19, 174: 20-25
    {0, FX174}, {0, FX174}, {0, FX17C}, {0, FX17E},        // 17C: 28-29, 17E: 30-31

    //  FJ200: 2.0 subdivided by mode2
    {0, FX200}, {0, FX201}, {0, FX202}, {0, FX203},        // 2.0.0 - 2.0.3
    {0, FX200}, {0, FX200}, {0, FX206}, {0, FX207},        // 2.0.6 - 2.0.7

    //  FJ220: 2.2 subdivided by mode2
    {0, FX220}, {0, FX221}, {0, FX222}, {0, FX223},        // 2.2.0 - 2.2.3
    {0, FX224}, {0, FX220}, {0, FX226}, {0, FX227},        // 2.2.4 - 2.2.7

    //  FJ25_: subdivision of format 2.5 by op1 / 8
    {3, FJ25J}, {0, FX258}, {0, FX259}, {0, FX25A},        // jump, 2.5.8 - ...
    {0, FX25A}, {0, FX25A}, {0, FX25A}, {0, FX25A}, 
    //  FJ25J: subdivision of format 2.5 jump instructions by op1 % 8
    {4, FJ250}, {4, FJ251}, {0, FX252}, {4, FJ253},
    {0, FX254}, {0, FX254}, {0, FX254}, {0, FX254},
    //  FJ250: subdivision of format 2.5.0 jump instructions by opj / 8
    {0, FX250}, {0, FX250}, {0, FX250}, {0, FX250}, 
    {0, FX250}, {0, FX250}, {0, FX250}, {0, FX2501},
    //  FJ251: subdivision of format 2.5.1 jump instructions by opj / 8
    {0, FX251}, {0, FX251}, {0, FX251}, {0, FX251}, 
    {0, FX251}, {0, FX251}, {0, FX251}, {0, FX2511},
    //  FJ253: subdivision of format 2.5.3 jump instructions by opj / 8
    {0, FX253}, {0, FX253}, {0, FX253}, {0, FX253}, 
    {0, FX253}, {0, FX253}, {0, FX253}, {0, FX2531},
    //  FJ290: subdivision of format 2.9 by by op1 / 8
    {0, FX290}, {0, FX290}, {0, FX290}, {0, FX290},         // 2.9.0
    {0, FX291}, {0, FX291}, {0, FX291}, {0, FX291},         // 2.9.1

    //  FJ300: 3.0 subdivided by mode2
    {0, FX300}, {0, FX300}, {0, FX302}, {0, FX303},        // 3.0.0 - 3.0.3
    {0, FX300}, {0, FX300}, {0, FX300}, {0, FX307},        // 3.0.6 - 3.0.7

    //  FJ31_: subdivision of format 3.1 instructions by op1 / 8
    {4, FJ31J}, {0, FX318}, {0, FX318}, {0, FX318}, 
    {0, FX318}, {0, FX318}, {0, FX318}, {0, FX318}, 
    //  FJ31J: subdivision of format 3.1.0 jump instructions by opj / 8
    {0, FX310}, {0, FX310}, {0, FX310}, {0, FX310}, 
    {0, FX310}, {0, FX310}, {0, FX310}, {5, FJ310}, 
    //  FJ310: subdivision of format 3.1.0:56 instructions by opj % 8
    {0, FX310}, {0, FX310}, {0, FX310}, {0, FX310}, 
    {0, FX310}, {0, FX310}, {0, FX310}, {0, FX311},

    //  FJ320: 3.2 subdivided by mode2
    {0, FX320}, {0, FX321}, {0, FX322}, {0, FX323},        // 3.2.0 - 3.2.3
    {0, FX320}, {0, FX320}, {0, FX320}, {0, FX327}         // 3.2.6 - 3.2.7
};


/*
format2:  0x0XYZ, where X = il, Y = mode, Z = subformat (mode2 or OP1, etc.)
cat:      Category: 1 = single format, 2 = tiny, 3 = multi-format, 4 = jump instruction                         
tmpl:     Template: 0xA, 0xB, 0xC, 0xD, 0xE. 0 = tiny
opAvail:  Operands available: 1 = immediate, 2 = memory, 0x10 = RT, 0x20 = RS, 0x40 = RU, 0x80 = RD
ot:       Operand type. 0: determined by OT field. 
0x10 - 0x17: 0-7. 
0x32: int32 for even OP1, int64  for odd OP1
0x35: float for even OP1, double for odd OP1

addrSize: Size of address/offset field (bytes)
addrPos:  Position of address/offset field (bytes)
immSize:  Size of first immediate operand, if any (bytes)
immPos:   Position of first immediate operand (bytes)
imm2:     Size and position of second immediate operand

vect:     1: vector registers used, 2: vector length in RS, 4: broadcast length in RS
0x10 = vector registers used if M bit
0x80: vector length should not be specified
mem:      1 = base in RT, 2 = base in RS, 4 = index in RS, 
0x10 = has offset, 0x20 = has limit,
0x80 = self-relative jump address
scale:    1 = offset is scaled, 2 = index is scaled, 4 = scale factor is -1
formatIndex: Bit index into format in instruction list
exeTable: Table of function pointers used during emulation
*/

const SFormat formatList[FXEND] = {
//  form    cat tmpl  opav  ot      addr  imm         v     mem   scl fi  xt
    {0x000, 3,  0xA,  0xB0, 0x00,   0, 0, 0, 0, 0,    0,    0x00, 0,  0,  2},
    {0x010, 3,  0xB,  0xA1, 0x00,   0, 0, 1, 0, 0,    0,    0x00, 0,  1,  2},
    {0x020, 3,  0xA,  0xB0, 0x00,   0, 0, 0, 0, 0,    1,    0x00, 0,  2,  2},
    {0x030, 3,  0xB,  0xA1, 0x00,   0, 0, 1, 0, 0,    1,    0x00, 0,  3,  2},
    {0x040, 3,  0xA,  0x82, 0x00,   0, 0, 0, 0, 0,    3,    0x01, 0,  4,  2},
    {0x050, 3,  0xA,  0x82, 0x00,   0, 0, 0, 0, 0,    3,    0x05, 4,  5,  2},
    {0x060, 3,  0xA,  0x82, 0x00,   0, 0, 0, 0, 0,    1,    0x05, 2,  6,  2},
    {0x070, 3,  0xB,  0x82, 0x00,   1, 0, 0, 0, 0,    1,    0x12, 1,  7,  2},
    {0x080, 3,  0xA,  0x82, 0x00,   0, 0, 0, 0, 0,    0,    0x05, 2,  8,  2},
    {0x090, 3,  0xB,  0x82, 0x00,   1, 0, 0, 0, 0,    0,    0x12, 1,  9,  2},
    // FX100
    {0x100, 1,  0xA,  0xB0, 0x00,   0, 0, 0, 0, 0,    0,    0x00, 0,  0,  5},
    {0x110, 1,  0xC,  0x81, 0x13,   0, 0, 2, 0, 0,    0,    0x00, 0,  0,  6},
    {0x120, 1,  0xA,  0xB0, 0x00,   0, 0, 0, 0, 0,    1,    0x00, 0,  0,  7},
    {0x130, 1,  0xB,  0xA1, 0x00,   0, 0, 1, 0, 0,    1,    0x00, 0,  0,  8},  // Format 1.3 B. op1 = 0-31
    {0x131, 1,  0xC,  0x81, 0x11,   0, 0, 2, 0, 0,    1,    0x00, 0,  0,  8},  // Format 1.3 C, in16. op1 = 32-39
    {0x132, 1,  0xC,  0x81, 0x32,   0, 0, 2, 0, 0,    1,    0x00, 0,  0,  8},  // Format 1.3 C, int32/int64. op1 = 40-55
    {0x133, 1,  0xC,  0x81, 0x35,   0, 0, 2, 0, 0,    1,    0x00, 0,  0,  8},  // Format 1.3 C, float/double. op1 = 56-63
                                                                               // FX140
    {0x140, 4,  0xB,  0xA0, 0x00,   1, 0, 0, 0, 0,    0x10, 0x80, 0,  0,  3},  // Format 1.4 jump. op1 = 0-55
    {0x141, 4,  0xB,  0x02, 0x13,   1, 0, 0, 0, 0,    0,    0x12, 1,  1,  3},  // Format 1.4 jump [mem] like format 0.9. op1 = 56-59
    {0x142, 4,  0xA,  0x82, 0x00,   0, 0, 0, 0, 0,    0,    0x05, 2,  2,  3},  // Format 1.4 jump (os, [mem]). op1 = 60-61
    {0x143, 4,  0xA,  0xB0, 0x00,   0, 0, 0, 0, 0,    0,    0x00, 0,  3,  3},  // Format 1.4 return, sys_call. op1 = 62-63
    {0x150, 4,  0xD,  0x00, 0x13,   3, 0, 0, 0, 1,    0,    0x80, 0,  4,  4},  // Format 1.5 D. op1 = 0-15
    {0x151, 4,  0xC,  0x81, 0x13,   1, 0, 1, 1, 0,    0,    0x80, 0,  5,  3},  // Format 1.5 C. op1 = 16-55
    {0x152, 4,  0xC,  0x81, 0x13,   2, 0, 0, 0, 0,    0,    0x80, 0,  6,  3},  // Format 1.5 C, unconditional 16 bit jump. op1 = 56-59
    {0x153, 4,  0xC,  0x80, 0x13,   0, 0, 0, 0, 0,    0,    0x00, 0,  7,  3},  // Format 1.5 C, jump to register, sys_return. op1 = 60-62
    {0x154, 4,  0xC,  0x01, 0x13,   0, 0, 2, 0, 0,    0,    0x00, 0,  6,  3},  // Format 1.5 C, trap. op1 = 63
    {0x155, 4,  0xC,  0x00, 0x13,   0, 0, 0, 0, 0x40, 0,    0x00, 0,  8,  3},  // Format 1.5 C, filler. op1 = 63. IM12 = 0xFFFF
                                                                               // FX160 tiny instructions
    {0x160, 2,  0x0,  0x81, 0x13,   0, 0,14, 0, 0,    0,    0x00, 0,  0,  1},  // Tiny instruction, g.p. register, 4-bit immediate. op1 = 0-7
    {0x168, 2,  0x0,  0xA0, 0x13,   0, 0, 0, 0, 0,    0,    0x00, 0,  0,  1},  // Tiny instruction, two g.p. registers. op1 = 8-13
    {0x16E, 2,  0x0,  0x82, 0x13,   0, 0, 0, 0, 0,    0,    0x02, 0,  0,  1},  // Tiny instruction, g.p. register and memory pointer. op1 = 14-15
    {0x170, 2,  0x0,  0xA0, 0x13,   0, 0, 0, 0, 0,    1,    0x00, 0,  0,  1},  // Tiny instruction, two vector registers. op1 = 16-17
    {0x172, 2,  0x0,  0x81, 0x35,   0, 0,14, 0, 0,    1,    0x00, 0,  0,  1},  // Tiny instruction, vector scalar and 4-bit immediate. op1 = 18-19
    {0x174, 2,  0x0,  0xA0, 0x35,   0, 0, 0, 0, 0,    1,    0x00, 0,  0,  1},  // Tiny instruction, two vector registers, single or double precision. op1 = 20-25
    {0x17C, 2,  0x0,  0xA0, 0x13,   0, 0, 0, 0, 0,    1,    0x00, 0,  0,  1},  // Tiny instruction, vector register and g.p. register. op1 = 28-29
    {0x17E, 2,  0x0,  0x82, 0x13,   0, 0, 0, 0, 0,    0x81, 0x02, 0,  0,  1},  // Tiny instruction, vector register and memory pointer. op1 = 30-31

    {0x180, 1,  0xB,  0xA1, 0x00,   0, 0, 1, 0, 0,    0,    0x00, 0,  0,  9},
    // FX200
    {0x200, 3,  0xE,  0xE2, 0x00,   2, 4, 0, 0, 2,    0,    0x11, 0, 16,  2},
    {0x201, 3,  0xE,  0xC2, 0x00,   2, 4, 0, 0, 2,    0,    0x15, 0, 17,  2},
    {0x202, 3,  0xE,  0xC2, 0x00,   2, 4, 0, 0, 2,    0,    0x15, 2, 18,  2},
    {0x203, 3,  0xE,  0xC2, 0x00,   2, 4, 0, 0, 2,    0,    0x25, 2, 19,  2},
    {0x206, 3,  0xE,  0xF0, 0x00,   0, 0, 0, 0, 2,    0,    0x00, 0, 22,  2},
    {0x207, 3,  0xE,  0xF1, 0x00,   0, 0, 2, 4, 6,    0,    0x00, 0, 23,  2},
    {0x210, 3,  0xA,  0xA2, 0x00,   4, 4, 0, 0, 0,    0,    0x11, 0, 13,  2},
    {0x220, 3,  0xE,  0xC2, 0x00,   2, 4, 0, 0, 2,    5,    0x11, 0, 24,  2},
    {0x221, 3,  0xE,  0xC2, 0x00,   2, 4, 0, 0, 2,    3,    0x11, 0, 25,  2},
    {0x222, 3,  0xE,  0xC2, 0x00,   2, 4, 0, 0, 2,    1,    0x15, 2, 26,  2},
    {0x223, 3,  0xE,  0xC2, 0x00,   2, 4, 0, 0, 2,    1,    0x25, 2, 27,  2},
    {0x224, 3,  0xE,  0xC2, 0x00,   2, 4, 0, 0, 2,    3,    0x15, 4, 28,  2},
    {0x226, 3,  0xE,  0xF0, 0x00,   0, 0, 0, 0, 2,    1,    0x00, 0, 30,  2},
    {0x227, 3,  0xE,  0xF1, 0x00,   0, 0, 2, 4, 6,    1,    0x00, 0, 31,  2},
    {0x230, 3,  0xA,  0xB1, 0x00,   0, 0, 4, 4, 0,    1,    0x00, 0, 14,  2},
    {0x240, 3,  0xA,  0x82, 0x00,   4, 4, 0, 0, 0,    3,    0x11, 0, 15,  2},
    // FX250
    {0x250, 4,  0xB,  0xA0, 0x00,   4, 4, 0, 0, 0x80, 0x10, 0x80, 0, 12,  3},  // Format 2.5.0 jump w 32-bit offset. op1 = 0-55
    {0x250, 4,  0xB,  0x82, 0x00,   4, 4, 0, 0, 0x80, 0,    0x12, 0, 13,  3},  // Format 2.5.0 jump [mem+i]. op1 = 56-63
    {0x251, 4,  0xB,  0xA1, 0x00,   2, 4, 2, 6, 0x80, 0x10, 0x80, 0, 14,  3},  // Format 2.5.1 jump w 16-bit imm + 16 bit offset. op1 = 0-55
    {0x251, 4,  0xB,  0xA1, 0x00,   0, 0, 4, 4, 0x80, 0,    0x00, 0, 15,  3},  // Format 2.5.1 jump. op1 = 56-63
    {0x252, 4,  0xC,  0x81, 0x13,   4, 4, 1, 1, 0x80, 0x10, 0x80, 0, 16,  3},  // Format 2.5.2 jump w 8-bit imm + 32 bit offset. op1 = 0-55
    {0x253, 4,  0xC,  0x81, 0x13,   1, 1, 4, 4, 0x80, 0x10, 0x80, 0, 17,  3},  // Format 2.5.3 jump w 32-bit imm + 8 bit offset. op1 = 0-55
    {0x253, 4,  0xC,  0x81, 0x13,   1, 1, 4, 4, 0x80, 0,    0x00, 0, 18,  3},  // Format 2.5.3 conditional trap. op1 = 56-63
    {0x254, 4,  0xC,  0x01, 0x13,   0, 0, 2, 0, 0xC0, 0,    0x00, 0, 19,  3},  // Format 2.5.4 system call w 16+32 bit imm 
    {0x258, 1,  0xB,  0xA3, 0x00,   1, 0, 4, 4, 0,    0x10, 0x12, 1,  0, 10},  // single format instructions, format 2.5 B with both memory and immediate: store. op1 = 0-15
    {0x259, 1,  0xB,  0xA2, 0x00,   4, 4, 0, 0, 0,    0,    0x11, 0,  0, 10},  // single format instructions, format 2.5 B with memory: compare_swap. op1 = 16-23
    {0x25A, 1,  0xA,  0xA2, 0x00,   4, 4, 0, 0, 0,    1,    0x11, 0,  0, 10},  // Miscellaneous single format instructions, format 2.5 A with memory operand. op1 = 24-63
    // FX260
    {0x260, 1,  0xA,  0xB1, 0x00,   0, 0, 4, 4, 0,    1,    0x00, 0,  0, 11},
    {0x270, 0,  0xA,  0x00, 0x00,   0, 0, 0, 0, 0,    0,    0x00, 0,  0,  0},  // currently unused
    {0x280, 3,  0xA,  0xB1, 0x00,   0, 0, 4, 4, 0,    0,    0x00, 0, 12,  2},
    {0x290, 1,  0xA,  0xB1, 0x00,   0, 0, 4, 4, 0,    0,    0x00, 0,  0, 12},
    {0x291, 1,  0xA,  0xA2, 0x00,   4, 4, 0, 0, 0,    0,    0x11, 0,  0, 12},

    // FX300
    {0x300, 3,  0xE,  0xE2, 0x00,   4, 8, 0, 0, 2,    0,    0x11, 0, 36,  2},
    {0x302, 3,  0xE,  0xC2, 0x00,   4, 8, 0, 0, 2,    0,    0x15, 2, 38,  2},
    {0x303, 3,  0xE,  0xC2, 0x00,   4, 8, 0, 0, 2,    0,    0x25, 2, 39,  2},
    {0x307, 3,  0xE,  0xF1, 0x00,   0, 0, 4, 8, 0xA,  0,    0x00, 0, 43,  2},

    {0x310, 4,  0xB,  0xA1, 0x00,   4, 8, 4, 4, 0x80, 0x10, 0x80, 0, 20,  3},      // Jump w 32-bit imm + 32 bit offset. op1 = 0-55
    {0x311, 4,  0xB,  0xA1, 0x13,   0, 0, 8, 4, 0x80, 0,    0x00, 0, 25,  3},      // Syscall. op1 = 56-63
    {0x318, 1,  0xA,  0xB1, 0x00,   0, 0, 8, 4, 0,    1,    0x00, 0,  0, 13},      // Triple size single format

    {0x320, 3,  0xE,  0xC2, 0x00,   4, 8, 0, 0, 2,    5,    0x11, 0, 44,  2},
    {0x321, 3,  0xE,  0xC2, 0x00,   4, 8, 0, 0, 2,    3,    0x11, 0, 45,  2},
    {0x322, 3,  0xE,  0xC2, 0x00,   4, 8, 0, 0, 2,    1,    0x15, 2, 46,  2},
    {0x323, 3,  0xE,  0xC2, 0x00,   4, 8, 0, 0, 2,    1,    0x25, 2, 47,  2},
    {0x327, 3,  0xE,  0xF1, 0x00,   0, 0, 4, 8, 0xA,  1,    0x00, 0, 51,  2},

    {0x330, 3,  0xA,  0xB1, 0x00,   0, 0, 8, 4, 0x10, 1,    0x00, 0, 34,  2},      // 340 - 370 do not exist. reserved for larger instruction sizes
    {0x380, 3,  0xA,  0xB1, 0x00,   0, 0, 8, 4, 0x10, 0,    0x00, 0, 32,  2}
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

    // Nested table lookup
    while (crit) {
        // Index into next table determined by criterion
        switch (crit) {
        case 1:  // mode2
            i = instruct >> 59 & 7;
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
        case 7:  // op1 / 2 %16
            i = instruct >> 22 & 0xF;
            break;
        case 8:  // IM12 == 0xFFFF
            i = (instruct & 0xFFFF) == 0xFFFF;
            break;
        default:  // Error. Should never occur
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
    0, funcTab1, funcTab2, funcTab3, funcTab4, funcTab5, funcTab6, funcTab7, funcTab8,
    funcTab9, funcTab10, funcTab11, funcTab12, funcTab13
};

// constants used in numOperands table below 
const uint8_t D = 0x22;  // double size operation. take two vector elements at a time
const uint8_t M = 0x0A;  // ignore mask. call instruction function even if mask = 0
const uint8_t L = 0x1A;  // ignore mask. RS is not a vector, or destination is not a vector. vector length is determined by the instruction function
const uint8_t l = 0x19;  // same as L, single operand
const uint8_t N = 0x1B;  // same as L, three operands
const uint8_t A = 0x41;  // one operand, don't read memory operand
const uint8_t S = 0xC9;  // store: one operand, don't read memory operand, ignore mask, don't modify RD

// Table of number of operands for each instruction
// bit 0-2: number of source operands
// bit 3: call execution function even if mask = 0
// bit 4: RS is not a vector register. vector length is determined by the instruction function
// bit 5: take two vector elements at a time
// bit 6: don't read memory operand before calling execution function
// bit 7: RD is unchanged (not destination)
uint8_t numOperands[15][64] = {
    {0},  //          8                16               24               32               40               48               56
    {0,1,2,2,2,2,2,2, 1,2,2,2,2,2,1,A, 0,1,1,1,2,2,2,2, 2,2,2,2,L,L,S,S, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0}, // 1:  tiny instructions
    {0,S,1,1,1,1,2,M, 2,2,2,2,2,2,D,D, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 3,3,3,3,2,2,2,2, 2,2,2,2,2,2,2,2}, // 2:  multi-format instructions
    {2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, M,M,M,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2}, // 3:  conditional and indirect jump instructions
    {1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0}, // 4:  simple jump and call instructions
    {0,1,1,1,1,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2}, // 5:  format 1.0
    {1,1,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2}, // 6:  format 1.1
    {L,9,L,L,3,L,N,L, L,L,L,L,L,L,L,L, L,L,L,L,L,L,L,L, D,D,2,2,D,D,2,2, 2,2,2,2,2,2,D,D, D,D,D,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,L,L,L,L,L,L}, // 7:  format 1.2

    {l,l,L,L,N,L,L,L, 2,2,2,2,2,2,2,2, 2,2,l,l,2,2,2,2, 2,3,2,2,2,2,2,2, 1,2,2,2,2,2,2,2, 1,1,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 1,1,2,2,2,2,2,2}, // 8:  format 1.3
    {2,3,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, M,M,M,M,M,M,M,M, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2}, // 9:  format 1.8
    {2,2,2,2,2,2,2,2, A,2,2,2,2,2,2,2, 2,2,3,2,2,2,2,2, 2,2,2,2,2,2,2,2, 0x5A,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2}, // 10: format 2.5
    {l,L,2,2,2,2,L,2, L,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2}, // 11: format 2.6
    {1,1,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, A,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2}, // 12: format 2.9
    {2,2,2,l,2,2,2,2, 2,L,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2}, // 13: format 3.1
    {0}
};

// Table of number of operands for each instruction, Format 2.0.7, op2 = 1
uint8_t numOperands2071[64] = {
  // 0                8                16               24               32               40               48               56
     3,3,3,3,3,3,3,3, 3,3,3,3,3,3,3,3, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0};

// Table of number of operands for each instruction, Format 2.2.6, op2 = 1
uint8_t numOperands2261[64] = {
  // 0                8                16               24               32               40               48               56
     N,N,N,0,0,0,0,0, 0xB,3,3,3,3,3,3,3, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0};

// Table of number of operands for each instruction, Format 2.2.7, op2 = 1
uint8_t numOperands2271[64] = {
  // 0                8                16               24               32               40               48               56
     3,L,3,3,3,3,3,3, N,N,3,3,3,3,3,3, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0};

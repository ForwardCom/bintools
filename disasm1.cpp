/****************************  disasm1.cpp   ********************************
* Author:        Agner Fog
* Date created:  2017-04-26
* Last modified: 2017-11-03
* Version:       1.00
* Project:       Binary tools for ForwardCom instruction set
* Module:        disassem.h
* Description:
* Disassembler for ForwardCom
*
* Copyright 2007-2017 GNU General Public License http://www.gnu.org/licenses
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
format:   0x0XYZ, where X = il, Y = mode, Z = subformat (mode2 or OP1, etc.)
cat:      Category: 1 = single format, 2 = tiny, 3 = multi-format, 4 = jump instruction                         
tmpl:     Template: 0xA, 0xB, 0xC, 0xD, 0xE. 0 = tiny
opmax:    Maximum value of OP1 for this record
uuu:      Unused

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
*/

const SFormat formatList[FXEND] = {
//   form   cat tmpl  opav  ot      addr  imm         v     mem   sc findx unused
    {0x000, 3,  0xA,  0xB0, 0x00,   0, 0, 0, 0, 0,    0,    0x00, 0,  0,  0},
    {0x010, 3,  0xB,  0xA1, 0x00,   0, 0, 1, 0, 0,    0,    0x00, 0,  1,  1},
    {0x020, 3,  0xA,  0xB0, 0x00,   0, 0, 0, 0, 0,    1,    0x00, 0,  2,  2},
    {0x030, 3,  0xB,  0xA1, 0x00,   0, 0, 1, 0, 0,    1,    0x00, 0,  3,  3},
    {0x040, 3,  0xA,  0x82, 0x00,   0, 0, 0, 0, 0,    3,    0x01, 0,  4,  4},
    {0x050, 3,  0xA,  0x82, 0x00,   0, 0, 0, 0, 0,    3,    0x05, 4,  5,  5},
    {0x060, 3,  0xA,  0x82, 0x00,   0, 0, 0, 0, 0,    1,    0x05, 2,  6,  6},
    {0x070, 3,  0xB,  0x82, 0x00,   1, 0, 0, 0, 0,    1,    0x12, 1,  7,  7},
    {0x080, 3,  0xA,  0x82, 0x00,   0, 0, 0, 0, 0,    0,    0x05, 2,  8,  8},
    {0x090, 3,  0xB,  0x82, 0x00,   1, 0, 0, 0, 0,    0,    0x12, 1,  9,  9},
    // FX100
    {0x100, 1,  0xA,  0xB0, 0x00,   0, 0, 0, 0, 0,    0,    0x00, 0,  0, 10},
    {0x110, 1,  0xC,  0x81, 0x13,   0, 0, 2, 0, 0,    0,    0x00, 0,  0, 11},
    {0x120, 1,  0xA,  0xB0, 0x00,   0, 0, 0, 0, 0,    1,    0x00, 0,  0, 12},
    {0x130, 1,  0xB,  0xA1, 0x00,   0, 0, 1, 0, 0,    1,    0x00, 0,  0, 13},  // Format 1.3 B. op1 = 0-31
    {0x131, 1,  0xC,  0x81, 0x11,   0, 0, 2, 0, 0,    1,    0x00, 0,  0, 14},  // Format 1.3 C, in16. op1 = 32-39
    {0x132, 1,  0xC,  0x81, 0x32,   0, 0, 2, 0, 0,    1,    0x00, 0,  0, 15},  // Format 1.3 C, int32/int64. op1 = 40-55
    {0x133, 1,  0xC,  0x81, 0x35,   0, 0, 2, 0, 0,    1,    0x00, 0,  0, 16},  // Format 1.3 C, float/double. op1 = 56-63
    // FX140
    {0x140, 4,  0xB,  0xA0, 0x00,   1, 0, 0, 0, 0,    0x10, 0x80, 0,  0, 17},  // Format 1.4 jump. op1 = 0-55
    {0x141, 4,  0xB,  0x02, 0x13,   1, 0, 0, 0, 0,    0,    0x12, 1,  1, 18},  // Format 1.4 jump [mem] like format 0.9. op1 = 56-59
    {0x142, 4,  0xA,  0x82, 0x00,   0, 0, 0, 0, 0,    0,    0x05, 2,  2, 19},  // Format 1.4 jump (os, [mem]). op1 = 60-61
    {0x143, 4,  0xA,  0xB0, 0x00,   0, 0, 0, 0, 0,    0,    0x00, 0,  3, 20},  // Format 1.4 return, sys_call. op1 = 62-63
    {0x150, 4,  0xD,  0x00, 0x13,   3, 0, 0, 0, 1,    0,    0x80, 0,  4, 21},  // Format 1.5 D. op1 = 0-15
    {0x151, 4,  0xC,  0x81, 0x13,   1, 0, 1, 1, 0,    0,    0x80, 0,  5, 22},  // Format 1.5 C. op1 = 16-55
    {0x152, 4,  0xC,  0x81, 0x13,   2, 0, 0, 0, 0,    0,    0x80, 0,  6, 23},  // Format 1.5 C, unconditional 16 bit jump. op1 = 56-59
    {0x153, 4,  0xC,  0x81, 0x13,   0, 0, 0, 0, 0,    0,    0x00, 0,  7, 24},  // Format 1.5 C, jump to register, sys_return. op1 = 60-62
    {0x154, 4,  0xC,  0x01, 0x13,   0, 0, 2, 0, 0,    0,    0x00, 0,  6, 25},  // Format 1.5 C, trap. op1 = 63
    {0x155, 4,  0xC,  0x00, 0x13,   0, 0, 0, 0, 0,    0,    0x00, 0,  6, 26},  // Format 1.5 C, filler. op1 = 63. IM12 = 0xFFFF
    // FX160 tiny instructions
    {0x160, 2,  0x0,  0x81, 0x13,   0, 0,14, 0, 0,    0,    0x00, 0,  0, 27},  // Tiny instruction, g.p. register, 4-bit immediate. op1 = 0-7
    {0x168, 2,  0x0,  0xA0, 0x13,   0, 0, 0, 0, 0,    0,    0x00, 0,  0, 28},  // Tiny instruction, two g.p. registers. op1 = 8-13
    {0x16E, 2,  0x0,  0x82, 0x13,   0, 0, 0, 0, 0,    0,    0x02, 0,  0, 29},  // Tiny instruction, g.p. register and memory pointer. op1 = 14-15
    {0x170, 2,  0x0,  0xA0, 0x00,   0, 0, 0, 0, 0,    1,    0x00, 0,  0, 30},  // Tiny instruction, two vector registers. op1 = 16-17
    {0x172, 2,  0x0,  0x81, 0x35,   0, 0,14, 0, 0,    1,    0x00, 0,  0, 31},  // Tiny instruction, vector scalar and 4-bit immediate. op1 = 18-19
    {0x174, 2,  0x0,  0xA0, 0x35,   0, 0, 0, 0, 0,    1,    0x00, 0,  0, 32},  // Tiny instruction, two vector registers, single or double precision. op1 = 20-25
    {0x17C, 2,  0x0,  0xA0, 0x13,   0, 0, 0, 0, 0,    1,    0x00, 0,  0, 33},  // Tiny instruction, vector register and g.p. register. op1 = 28-29
    {0x17E, 2,  0x0,  0x82, 0x13,   0, 0, 0, 0, 0,    0x81, 0x02, 0,  0, 34},  // Tiny instruction, vector register and memory pointer. op1 = 30-31
    {0x180, 1,  0xB,  0xA1, 0x00,   0, 0, 1, 0, 0,    0,    0x00, 0,  0, 35},
    // FX200
    {0x200, 3,  0xE,  0xE2, 0x00,   2, 4, 0, 0, 2,    0,    0x11, 0, 16, 36},
    {0x201, 3,  0xE,  0xC2, 0x00,   2, 4, 0, 0, 2,    0,    0x15, 0, 17, 37},
    {0x202, 3,  0xE,  0xC2, 0x00,   2, 4, 0, 0, 2,    0,    0x15, 2, 18, 38},
    {0x203, 3,  0xE,  0xC2, 0x00,   2, 4, 0, 0, 2,    0,    0x25, 2, 19, 39},
    {0x206, 3,  0xE,  0xF0, 0x00,   0, 0, 0, 0, 2,    0,    0x00, 0, 22, 40},
    {0x207, 3,  0xE,  0xF1, 0x00,   0, 0, 2, 4, 6,    0,    0x00, 0, 23, 41},
    {0x210, 3,  0xA,  0xA2, 0x00,   4, 4, 0, 0, 0,    0,    0x11, 0, 13, 42},
    {0x220, 3,  0xE,  0xC2, 0x00,   2, 4, 0, 0, 2,    5,    0x11, 0, 24, 43},
    {0x221, 3,  0xE,  0xC2, 0x00,   2, 4, 0, 0, 2,    3,    0x11, 0, 25, 44},
    {0x222, 3,  0xE,  0xC2, 0x00,   2, 4, 0, 0, 2,    1,    0x15, 2, 26, 45},
    {0x223, 3,  0xE,  0xC2, 0x00,   2, 4, 0, 0, 2,    1,    0x25, 2, 27, 46},
    {0x224, 3,  0xE,  0xC2, 0x00,   2, 4, 0, 0, 2,    3,    0x15, 4, 28, 45},
    {0x226, 3,  0xE,  0xF0, 0x00,   0, 0, 0, 0, 2,    1,    0x00, 0, 30, 47},
    {0x227, 3,  0xE,  0xF1, 0x00,   0, 0, 2, 4, 6,    1,    0x00, 0, 31, 48},

    {0x230, 3,  0xA,  0xB1, 0x00,   0, 0, 4, 4, 0,    1,    0x00, 0, 14, 49},
    {0x240, 3,  0xA,  0x82, 0x00,   4, 4, 0, 0, 0,    3,    0x11, 0, 15, 50},
    // FX250
    {0x250, 4,  0xB,  0xA0, 0x00,   4, 4, 0, 0, 0x80, 0x10, 0x80, 0, 12, 51},  // Format 2.5.0 jump w 32-bit offset. op1 = 0-55
    {0x250, 4,  0xB,  0x82, 0x00,   4, 4, 0, 0, 0x80, 0,    0x12, 0, 13, 52},  // Format 2.5.0 jump [mem+i]. op1 = 56-63
    {0x251, 4,  0xB,  0xA1, 0x00,   2, 4, 2, 6, 0x80, 0x10, 0x80, 0, 14, 53},  // Format 2.5.1 jump w 16-bit imm + 16 bit offset. op1 = 0-55
    {0x251, 4,  0xB,  0xA1, 0x00,   0, 0, 4, 4, 0x80, 0,    0x00, 0, 15, 54},  // Format 2.5.1 jump. op1 = 56-63
    {0x252, 4,  0xC,  0x81, 0x13,   4, 4, 1, 1, 0x80, 0,    0x80, 0, 16, 55},  // Format 2.5.2 jump w 8-bit imm + 32 bit offset. op1 = 0-55
    {0x253, 4,  0xC,  0x81, 0x13,   1, 1, 4, 4, 0x80, 0,    0x80, 0, 17, 56},  // Format 2.5.3 jump w 32-bit imm + 8 bit offset. op1 = 0-55
    {0x253, 4,  0xC,  0x81, 0x13,   1, 1, 4, 4, 0x80, 0,    0x00, 0, 18, 57},  // Format 2.5.3 conditional trap. op1 = 56-63
    {0x254, 4,  0xC,  0x01, 0x13,   0, 0, 2, 0, 0,    0,    0x00, 0, 19, 58},  // Format 2.5.4 system call w 16+32 bit imm 
    {0x258, 1,  0xB,  0xA3, 0x00,   1, 0, 4, 4, 0,    0x10, 0x12, 1,  0, 59},  // single format instructions, format 2.5 B with both memory and immediate: store. op1 = 0-15
    {0x259, 1,  0xB,  0xA3, 0x00,   4, 4, 0, 0, 0,    0,    0x11, 0,  0, 60},  // single format instructions, format 2.5 B with both memory and immediate: fence. op1 = 16-23
    {0x25A, 1,  0xA,  0xA2, 0x00,   4, 4, 0, 0, 0,    1,    0x11, 0,  0, 61},  // Miscellaneous single format instructions, format 2.5 A with memory operand. op1 = 24-63
    // FX260
    {0x260, 1,  0xA,  0xB1, 0x00,   0, 0, 4, 4, 0,    1,    0x00, 0,  0, 62},
    {0x270, 0,  0xA,  0x00, 0x00,   0, 0, 0, 0, 0,    0,    0x00, 0,  0, 63},  // currently unused
    {0x280, 3,  0xA,  0xB1, 0x00,   0, 0, 4, 4, 0,    0,    0x00, 0, 12, 64},
    {0x290, 1,  0xA,  0xB1, 0x00,   0, 0, 4, 4, 0,    0,    0x00, 0,  0, 65},
    {0x291, 1,  0xA,  0xA2, 0x00,   4, 4, 0, 0, 0,    0,    0x11, 0,  0, 65},

    // FX300
    {0x300, 3,  0xE,  0xE2, 0x00,   4, 8, 0, 0, 2,    0,    0x11, 0, 36, 66},
    {0x302, 3,  0xE,  0xC2, 0x00,   4, 8, 0, 0, 2,    0,    0x15, 2, 38, 38},
    {0x303, 3,  0xE,  0xC2, 0x00,   4, 8, 0, 0, 2,    0,    0x25, 2, 39, 67},
    {0x307, 3,  0xE,  0xF1, 0x00,   0, 0, 4, 8, 0xA,  0,    0x00, 0, 43, 68},

    {0x310, 4,  0xB,  0xA1, 0x00,   4, 8, 4, 4, 0x80, 0x10, 0x80, 0, 20, 69},      // Jump w 32-bit imm + 32 bit offset. op1 = 0-55
    {0x311, 4,  0xB,  0xA1, 0x13,   0, 0, 8, 4, 0x80, 0,    0x00, 0, 21, 70},      // Syscall. op1 = 56-63
    {0x318, 1,  0xA,  0xB1, 0x00,   0, 0, 8, 4, 0,    1,    0x00, 0,  0, 71},      // Triple size single format

    {0x320, 3,  0xE,  0xC2, 0x00,   4, 8, 0, 0, 2,    5,    0x11, 0, 44, 72},
    {0x321, 3,  0xE,  0xC2, 0x00,   4, 8, 0, 0, 2,    3,    0x11, 0, 45, 73},
    {0x322, 3,  0xE,  0xC2, 0x00,   4, 8, 0, 0, 2,    1,    0x15, 2, 46, 00},
    {0x323, 3,  0xE,  0xC2, 0x00,   4, 8, 0, 0, 2,    1,    0x25, 2, 47, 74},
    {0x327, 3,  0xE,  0xF1, 0x00,   0, 0, 4, 8, 0xA,  1,    0x00, 0, 51, 75},

    {0x330, 3,  0xA,  0xB1, 0x00,   0, 0, 8, 4, 0x10, 1,    0x00, 0, 34, 76},      // 340 - 370 do not exist. reserved for larger instruction sizes
    {0x380, 3,  0xA,  0xB1, 0x00,   0, 0, 8, 4, 0x10, 0,    0x00, 0, 32, 77}
};


uint32_t formatListSize = TableSize(formatList);  // export size to other modules


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

    // uint32_t ff = formatList[t].format2;  printf("\n%03X", ff);

    return t;
}


uint64_t interpretTemplateVariants(const char * s) {
    // Interpret template variants in instruction record
    // Return value:
    // VARIANT_D0 (bit 0):     D0, no destination, no operant type
    // VARIANT_D1 (bit 1):     D1, no destination, but operant type specified
    // VARIANT_D2 (bit 2):     D2, operant type ignored
    // VARIANT_M0 (bit 3):     M0, memory operand destination
    // VARIANT_M1 (bit 4):     M1, IM3 is used as extra immediate operand in E formats with a memory operand
    // VARIANT_R0 (bit 8):     R0, destination is general purpose register
    // VARIANT_R1 (bit 9):     R1, first source operand is general purpose register
    // VARIANT_R2 (bit 10):    R2, second source operand is general purpose register
    // VARIANT_R3 (bit 11):    R3, third source operand is general purpose register
    // VARIANT_RL (bit 12):    RL, RS is a general purpose register specifying length
    // VARIANT_I2 (bit 16):    I2, second source operand is integer
    // VARIANT_I3 (bit 17):    I3, third source operand is integer
    // VARIANT_U0 (bit 18):    U0, integer operands are unsigned
    // VARIANT_U3 (bit 19):    U3, integer operands are unsigned if bit 3 in IM3 (format 2.4.x, 2.8.x) is set.
    // VARIANT_On (bit 24-26): On, n IM3 bits used for options
    // VARIANT_H0 (bit 28):    H0, half precision floating point operands
    // VARIANT_SPEC  (bit 32-37): Special register types for operands: bit 32-35 = register type, 
    // VARIANT_SPECS (bit 36):    source, 
    // VARIANT_SPECD (bit 37):    destination

    uint64_t v = 0;
    for (int i = 0; i < 8; i++) {          // Loop through string
        char c = s[i], d = s[i+1];
        switch (c) {
        case 0:
            return v;                      // End of string
        case 'D': 
            if (d == '0') v |= VARIANT_D0; // D0
            if (d == '1') v |= VARIANT_D1; // D1
            if (d == '2') v |= VARIANT_D2; // D1
            continue;
        case 'M':
            if (d == '0') v |= VARIANT_M0; // M0
            if (d == '1') v |= VARIANT_M1; // M1
            continue;
        case 'R':
            if (d == '0') v |= VARIANT_R0; // R0
            if (d == '1') v |= VARIANT_R1; // R1
            if (d == '2') v |= VARIANT_R2; // R2
            if (d == '3') v |= VARIANT_R3; // R3
            if (d == 'L') v |= VARIANT_RL; // RL
            i++;
            continue;
        case 'I':
            if (d == '2') v |= VARIANT_I2; // I2
            continue;
        case 'O':
            if (d > '0' && d < '7') v |= (d - '0') << 24;    // O1 - O6
            continue;
        case 'U':
            if (d == '0') v |= VARIANT_U0; // U0
            if (d == '3') v |= VARIANT_U3; // U3
            continue;
        case 'H':
            if (d == '0') v |= VARIANT_H0; // H0
            continue;
        case 'X':
            v |= uint64_t(((d-'0') & 0xF) | 0x10) << 32; // X0 - X9
            continue;
        case 'Y':
            v |= uint64_t(((d-'0') & 0xF) | 0x20) << 32; // Y0 - Y9
            continue;
        }
    }
    return v;
}


void CDisassembler::sortSymbolsAndRelocations() {
    // Sort symbols by address. This is useful when symbol labels are written out
    uint32_t i;                                            // loop counter
    // The values of st_reguse1 and st_reguse2 are no longer needed after these values have been written out.
    // Save old index in st_reguse1. 
    // Set st_reguse2 to zero, it is used later for data type

    for (i = 0; i < symbols.numEntries(); i++) {
        symbols[i].st_reguse1 = i;
        symbols[i].st_reguse2 = 0;
    }
    // Sort symbols by address
    symbols.sort();

    // Add dummy empty symbol number 0
    ElfFWC_Sym nulsymbol = {0,0,0,0,0,0,0,0,0};
    symbols.addUnique(nulsymbol);

    // Update all relocations to the new symbol indexes
    // Translate old to new symbol index in all relocation records
    // Allocate array for translating  old to new symbol index
    CDynamicArray<uint32_t> old2newSymbolIndex;
    old2newSymbolIndex.setNum(symbols.numEntries());

    // Make translation table
    for (i = 0; i < symbols.numEntries(); i++) {
        uint32_t oldindex = symbols[i].st_reguse1;
        if (oldindex < symbols.numEntries()) {
            old2newSymbolIndex[oldindex] = i;
        }
    }

    // Translate all symbol indices in relocation records
    for (i = 0; i < relocations.numEntries(); i++) {
        if (relocations[i].r_sym < old2newSymbolIndex.numEntries()) {
            relocations[i].r_sym = old2newSymbolIndex[relocations[i].r_sym];            
        }
        else relocations[i].r_sym = 0; // index out of range!!
        if ((relocations[i].r_type & R_FORW_RELTYPEMASK) == R_FORW_REFP) {
            // relocation record has an additional reference point
            // bit 30 indicates relocation used OK
            uint32_t refsym = relocations[i].r_refsym & ~0x40000000;
            if (refsym < old2newSymbolIndex.numEntries()) {
                relocations[i].r_refsym = old2newSymbolIndex[refsym] | (relocations[i].r_refsym & 0x40000000);
            }
            else relocations[i].r_refsym = 0; // index out of range
        }
    }

    // Sort relocations by address
    relocations.sort();
}


// Join the tables: symbols and newSymbols
void CDisassembler::joinSymbolTables() {
    /* There are two symbol tables: 'symbols' and 'newSymbols'.
    'symbols' contains the symbols that were in the original file. This table is sorted
    by address in sortSymbolsAndRelocations() in order to make it easy to find a symbol
    at a given address.
    'newSymbols' contains new symbols that were created during pass 1. It is not sorted.
    The reason why we have two symbol tables is that the symbol indexes would change if 
    we add to the 'symbols' table during pass 1 and keep it sorted. We need to have 
    consistent indexes during pass 1 in order to access symbols by their index. Likewise,
    'newSymbols' is not sorted because indexes would change when new symbols are added to it.
    'newSymbols' may contain dublets because it is not sorted so dublets are not detected
    when new symbols are added.
    'joinSymbolTables()' is called after pass 1 when we are finished making new symbols.
    This function joins the two tables together, removes any dublets, updates symbol indexes
    in all relocation records, and tranfers data type information from relocation records
    to symbol records.
    */
    uint32_t r;                                            // Relocation index
    uint32_t s;                                            // Symbol index
    uint32_t newsymi;                                      // Symbol index in newSymbols
    uint32_t newsymi2;                                     // Index of new symbol after transfer to symbols table
    uint32_t symTempIndex = symbols.numEntries();       // Temporary index of symbol after transfer

    // Remember index of each symbol before adding new symbols and reordering
    for (s = 0; s < symbols.numEntries(); s++) {
        symbols[s].st_reguse1 = s;
    }

    // Loop through relocations to find references to new symbols
    for (r = 0; r < relocations.numEntries(); r++) {
        if (relocations[r].r_sym & 0x80000000) {           // Refers to newSymbols table
            newsymi = relocations[r].r_sym & ~0x80000000;
            if (newsymi < newSymbols.numEntries()) {            
                // Put symbol into old table if no equivalent symbol exists here
                newsymi2 = symbols.addUnique(newSymbols[newsymi]);
                // Give it a temporary index if it doesn't have one
                if (symbols[newsymi2].st_reguse1 == 0) symbols[newsymi2].st_reguse1 = symTempIndex++;
                // update reference in relocation record to temporary index
                relocations[r].r_sym = symbols[newsymi2].st_reguse1;
            }
        }
        // Do the same with any reference point
        if ((relocations[r].r_type & R_FORW_RELTYPEMASK) == R_FORW_REFP && relocations[r].r_refsym & 0x80000000) {
            newsymi = relocations[r].r_refsym & ~0xC0000000;
            if (newsymi < newSymbols.numEntries()) {            
                // Put symbol into old table if no equivalent symbol exists here
                newsymi2 = symbols.addUnique(newSymbols[newsymi]);
                // Give it a temporary index if it doesn't have one
                if (symbols[newsymi2].st_reguse1 == 0) symbols[newsymi2].st_reguse1 = symTempIndex++;
                // update reference in relocation record to temporary index
                relocations[r].r_refsym = symbols[newsymi2].st_reguse1 | (relocations[r].r_refsym & 0x40000000);
            }
        }
    }
    // Make symbol index translation table
    CDynamicArray<uint32_t> old2newSymbolIndex;
    old2newSymbolIndex.setNum(symbols.numEntries());
    for (s = 0; s < symbols.numEntries(); s++) {
        uint32_t oldsymi = symbols[s].st_reguse1;
        if (oldsymi < old2newSymbolIndex.numEntries()) {
            old2newSymbolIndex[oldsymi] = s;
        }
    }
    // Update indexes in relocation records
    for (r = 0; r < relocations.numEntries(); r++) {
        if (relocations[r].r_sym < old2newSymbolIndex.numEntries()) { // Refers to newSymbols table
            relocations[r].r_sym = old2newSymbolIndex[relocations[r].r_sym];
            // Give the symbol a data type from relocation record if it doesn't have one
            if (symbols[relocations[r].r_sym].st_reguse2 == 0) {
                symbols[relocations[r].r_sym].st_reguse2 = relocations[r].r_type >> 8;
            }
        }
        // Do the same with any reference point
        uint32_t refsym = relocations[r].r_refsym & ~0xC0000000;
        if ((relocations[r].r_type & R_FORW_RELTYPEMASK) == R_FORW_REFP && refsym < old2newSymbolIndex.numEntries()) {
            relocations[r].r_refsym = old2newSymbolIndex[refsym] | (relocations[r].r_refsym & 0x40000000);
        }
    }
}


void CDisassembler::assignSymbolNames() {
    // Assign names to symbols that do not have a name
    uint32_t i;                                            // New symbol index
    uint32_t numDigits;                                    // Number of digits in new symbol names
    char name[64];                                         // sectionBuffer for making symbol name
    static char format[64];
    uint32_t unnamedNum = 0;                               // Number of unnamed symbols
    //uint32_t addMoreSymbols = 0;                           // More symbols need to be added

    // Find necessary number of digits
    numDigits = 3; i = symbols.numEntries();
    while (i >= 1000) {
        i /= 10;
        numDigits++;
    }

    // format string for symbol names
    sprintf(format, "%s%c0%i%c", "@_", '%', numDigits, 'i');

    // Loop through symbols
    for (i = 1; i < symbols.numEntries(); i++) {
        if (symbols[i].st_name == 0 ) {
            // Symbol has no name. Make one
            sprintf(name, format, ++unnamedNum);
            // Store new name
            symbols[i].st_name = stringBuffer.pushString(name);
        }
    }

#if 0
    // For debugging: list all symbols
    printf("\n\nSymbols:");
    for (i = 0; i < symbols.getNumEntries(); i++) {
        printf("\n%3X %3X %s sect %i offset %X type %X size %i Scope %i", 
            i, symbols[i].st_name, stringBuffer.buf() +  symbols[i].st_name,
            symbols[i].st_shndx, (uint32_t)symbols[i].st_value, symbols[i].st_type, 
            (uint32_t)symbols[i].st_size, symbols[i].st_other);
        if (symbols[i].st_reguse2) printf(" Type %X", symbols[i].st_reguse2);
    }
#endif
#if 0
    // For debugging: list all relocations
    printf("\n\nRelocations:");
    for (uint32_t i = 0; i < relocations.numEntries(); i++) {
        printf("\nsect %i, os %X, type %X, sym %i, add %X, refsym %X",
            (uint32_t)(relocations[i].r_section), (uint32_t)relocations[i].r_offset, relocations[i].r_type, 
            relocations[i].r_sym, relocations[i].r_addend, relocations[i].r_refsym);
    }
#endif
}



/**************************  class CDisassembler  *****************************
Members of class CDisassembler
Members that relate to file output are in disasm2.cpp
******************************************************************************/

CDisassembler::CDisassembler() {
    // Constructor. Initialize variables
    pass = 0;
    nextSymbol = 0;
    currentFunction = 0;
    currentFunctionEnd = 0;
};

void CDisassembler::initializeInstructionList() {
    // Read and initialize instruction list and sort it by category, format, and op1
    CCSVFile instructionListFile(cmd.instructionListFile); // Filename of list of instructions
    instructionListFile.parse();                 // Read and interpret instruction list file
    instructionlist << instructionListFile.instructionlist; // Transfer instruction list to my own container
    instructionlist.sort();                      // Sort list, using sort order defined by SInstruction2
}

// Read instruction list, split ELF file into components
void CDisassembler::getComponents1() {

    // list file name from command line
    outputFileName = cmd.outputFile;

    // Check code integrity
    checkFormatListIntegrity();

    // Read instruction list
    initializeInstructionList();

    // Split ELF file into containers
    split();
}

// Read instruction list, get ELF components for assembler output listing
void CDisassembler::getComponents2(CELF const & assembler, CMemoryBuffer const & instructList) {
    // This function replaces getComponents1() when making an output listing for the assembler
    // list file name from command line
    outputFileName = cmd.outputListFile;

    // copy containers from assembler outFile
    sectionHeaders.copy(assembler.getSectionHeaders());
    symbols.copy(assembler.getSymbols());
    relocations.copy(assembler.getRelocations());
    stringBuffer.copy(assembler.getStringBuffer());
    dataBuffer.copy(assembler.getDataBuffer());
    // copy instruction list from assembler to avoid reading the csv file again
    instructionlist.copy(instructList);
    instructionlist.sort();  // Sort list, using the sort order needed by the disassembler as defined by SInstruction2
}


// Do the disassembly
void CDisassembler::go() {

    // Begin writing output file
    writeFileBegin();

    // Sort symbols by address
    sortSymbolsAndRelocations();

    // pass 1: Find symbols types and unnamed symbols
    pass = 1;
    pass1();

    if (pass & 0x10) {
        // Repetition of pass 1 requested
        pass = 2;
        pass1();
    }

    // Join the tables: symbols and newSymbols;
    joinSymbolTables();

    // put names on unnamed symbols
    assignSymbolNames();

    // pass 2: Write all sections to output file
    pass = 0x100;
    pass2();

    // Check for illegal entries in symbol table and relocations table
    finalErrorCheck();

    // Finish writing output file
    writeFileEnd();

    // write output file
    outFile.outputFileName = outputFileName;
    outFile.write();
};


void CDisassembler::pass1() {

    /*             pass 1: does the following jobs:
    --------------------------------

    * Scans all code sections, instruction by instruction.

    * Follows all references to data in order to determine data type for 
    each data symbol.

    * Assigns symbol table entries for all jump and call targets that do not
    allready have a name.

    * Identifies and analyzes tables of jump addresses and call addresses,
    e.g. switch/case tables and virtual function tables. (to do !!)

    * Tries to identify any data in the code section.

    */
    //uint32_t sectionType;

    // Loop through sections, pass 1
    for (section = 1; section < sectionHeaders.numEntries(); section++) {

        // Get section type
        //sectionType = sectionHeaders[section].sh_type;
        codeMode = (sectionHeaders[section].sh_flags & SHF_EXEC) ? 1 : 4;

        sectionBuffer = dataBuffer.buf() + sectionHeaders[section].sh_offset;
        sectionEnd = (uint32_t)sectionHeaders[section].sh_size;

        if (codeMode < 4) {
            // This is a code section

            //sectionAddress = sectionHeaders[section].sh_addr;
            if (sectionEnd == 0) continue;

            iInstr = 0;

            // Loop through instructions
            while (iInstr < sectionEnd) {

                // Check if code not dubious
                if (codeMode == 1) {

                    parseInstruction();                    // Parse instruction

                    updateSymbols();                       // Detect symbol types for operands of this instruction

                    updateTracer();                        // Trace register values

                    iInstr += instrLength * 4;             // Next instruction
                }
                else {
                  //  iEnd = labelEnd;
                }
            }
        }
    }
}


void CDisassembler::pass2() {

    /*             pass 2: does the following jobs:
    --------------------------------

    * Scans through all sections, code and data.

    * Outputs warnings for suboptimal instruction codes and error messages
    for erroneous code and erroneous relocations.

    * Outputs disassembly of all instructions, operands and relocations,
    followed by the binary code listing as comment.

    * Outputs disassembly of all data, followed by alternative representations
    as comment.
    */

    //uint32_t sectionType;

    // Loop through sections, pass 2
    for (section = 1; section < sectionHeaders.numEntries(); section++) {

        // Get section type
        //sectionType = sectionHeaders[section].sh_type;
        codeMode = (sectionHeaders[section].sh_flags & SHF_EXEC) ? 1 : 4;

        // Initialize code parser
        sectionBuffer = dataBuffer.buf() + sectionHeaders[section].sh_offset;
        sectionEnd = (uint32_t)sectionHeaders[section].sh_size;

        writeSectionBegin();                               // Write segment directive

        if (codeMode < 4) {
            // This is a code section
            if (sectionEnd == 0) continue;
            iInstr = 0;

            // Loop through instructions
            while (iInstr < sectionEnd) {

                writeLabels();                             // Find any label here

                // Check if code not dubious
                if (codeMode == 1) {
                    
                    parseInstruction();                    // Parse instruction

                    writeInstruction();                    // Write instruction

                    iInstr += instrLength * 4;             // Next instruction

                }
                else {
                    // This is data Skip to next label                                            
                }
            }
            writeSectionEnd();                             // Write segment directive
        }
        else {
            // This is a data section
            pInstr = 0; iRecord = 0; fInstr = 0;           // Set invalid pointers to zero
            operandType = 2;                               // Default data type is int32
            instrLength = 4;                               // Default data size is 4 bytes
            iInstr = 0;                                    // Instruction position
            nextSymbol = 0;
            
            writeDataItems();                              // Loop through data. Write data

            writeSectionEnd();                             // Write segment directive
        }
    }
}



/********************  Explanation of tracer:  ***************************

This is a machine which can trace the contents of each register in certain
situations. It is currently used for recognizing pointers to jump tables
in order to identify jump tables (to do!!)
*/
void CDisassembler::updateTracer() {
    // Trace register values. See explanation above
}


void CDisassembler::updateSymbols() {
    // Find unnamed symbols, determine symbol types,
    // update symbol list, call checkJumpTarget if jump/call.
    // This function is called during pass 1 for every instruction
    uint32_t relSource = iInstr + fInstr->addrPos; // Position of relocated field
    if (fInstr->cat == 4 && fInstr->mem & 0x80) {
        // Self-relative jump instruction. Check OPJ
        // uint32_t opj = (instrLength == 1) ? pInstr->a.op1 : pInstr->b[0]; // Jump instruction opcode
        if (/*opj < 60 &&*/ fInstr->addrSize) {
            // This instruction has a self-relative jump target.
            // Check if there is a relocation here
            ElfFWC_Rela2 rel;
            rel.r_offset = relSource;
            rel.r_section = section;
            rel.r_addend = 0;
            if (relocations.findFirst(rel) < 0) {
                // There is no relocation. Target must be in the same section. Find target
                int32_t offset = 0;
                switch (fInstr->addrSize) {                // Read offset of correct size
                case 1:      // 8 bit
                    offset = *(int8_t*)(sectionBuffer + relSource);
                    rel.r_type = R_FORW_8 | 0x80000000;  //  add 0x80000000 to remember that this is not a real relocation
                    break;
                case 2:      // 16 bit
                    offset = *(int16_t*)(sectionBuffer + relSource);
                    rel.r_type = R_FORW_16 | 0x80000000;
                    break;
                case 3:      // 24 bit. Sign extend to 32 bits
                    offset = *(int32_t*)(sectionBuffer + relSource) << 8 >> 8;
                    rel.r_type = R_FORW_24 | 0x80000000;
                    break; 
                case 4:      // 32 bit
                    offset = *(int32_t*)(sectionBuffer + relSource);
                    rel.r_type = R_FORW_32 | 0x80000000;
                    break;
                }
                // Scale offset by 4 and add offset to end of instruction
                int32_t target = iInstr + instrLength * 4 + offset * 4;

                // Add a symbol at target address if none exists
                ElfFWC_Sym sym = {0, 0, STB_LOCAL, STV_EXEC, section, uint32_t(target), 0, 0, 0 };
                int32_t symi = symbols.findFirst(sym);
                if (symi < 0) {
                    symi = newSymbols.push(sym);           // Add symbol to new symbols table
                    symi |= 0x80000000;                    // Upper bit means index refers to newSymbols
                }
                // Add a dummy relocation record for this symbol. 
                // This relocation does not need type, scale, or addend because the only purpose is to identify the symbol.
                // It does have a size, though, because this is checked later in writeRelocationTarget()
                rel.r_sym = (uint32_t)symi;
                relocations.addUnique(rel);
            }
        }
    }
    else { // Not a jump instruction
        // Check if instruction has a memory reference relative to IP, DATAP, or THREADP
        uint32_t basePointer = 0;
        if (fInstr->mem & 1) basePointer = pInstr->a.rt;
        else if (fInstr->mem & 2) basePointer = pInstr->a.rs;

        if (fInstr->addrSize > 1 && basePointer >= 28 && basePointer <= 30) {
            // Memory operand is relative to THREADP, DATAP or IP
            // Check if there is a relocation here
            uint32_t relpos = iInstr + fInstr->addrPos;
            ElfFWC_Rela2 rel;
            rel.r_offset = relpos;
            rel.r_section = section;
            rel.r_type = (operandType | 0x80) << 24;
            uint32_t nrel, irel = 0;
            nrel = relocations.findAll(&irel, rel);
            if (nrel > 1) writeWarning("Overlapping relocations here");
            if (nrel) {
                // Relocation found. Put the data type into the relocation record. 
                // The data type will later be transferred to the symbol record in joinSymbolTables()
                if (!(relocations[irel].r_type & 0x80000000)) {
                    // Save target data type in upper 8 bits of r_type
                    relocations[irel].r_type = (relocations[irel].r_type & 0x00FFFFFF) | (operandType /*| 0x80*/) << 24;
                }
                // Check if the target is a section + offset
                uint32_t symi = relocations[irel].r_sym;
                if (symi < symbols.numEntries() && symbols[symi].st_type == STT_SECTION && relocations[irel].r_addend > 0) {
                    // Add a new symbol at this address
                    ElfFWC_Sym sym = {0, 0, STB_LOCAL, STT_OBJECT, symbols[symi].st_shndx, symbols[symi].st_value + relocations[irel].r_addend, 0, 0, 0};
                    uint32_t symi2 = newSymbols.push(sym);
                    relocations[irel].r_sym = symi2 | 0x80000000;  // Upper bit means index refers to newSymbols
                    relocations[irel].r_addend = 0;
                }
            }
            else if (basePointer == (REG_IP & 0xFF) && fInstr->addrSize > 1) {
                // No relocation found. Insert new relocation and new symbol
                // This fits the address instruction with a local IP target.
                // todo: Make it work for other cases

                // Add a symbol at target address if none exists
                int32_t target = iInstr + instrLength * 4;
                switch (fInstr->addrSize) {                // Read offset of correct size
                /* case 1:      // 8 bit. cannot use IP
                    target += *(int8_t*)(sectionBuffer + relSource) << (operandType & 7);
                    rel.r_type = R_FORW_8 | R_FORW_SELFREL | 0x80000000; 
                    break;*/
                case 2:      // 16 bit
                    target += *(int16_t*)(sectionBuffer + relSource);
                    rel.r_type = R_FORW_16 | R_FORW_SELFREL | 0x80000000;
                    break;
                case 4:      // 32 bit
                    target += *(int32_t*)(sectionBuffer + relSource);
                    rel.r_type = R_FORW_32 | R_FORW_SELFREL | 0x80000000;
                    break;
                }
                ElfFWC_Sym sym = {0, 0, STB_LOCAL, STV_EXEC, section, (uint32_t)target, 0, 0, 0 };
                int32_t symi = symbols.findFirst(sym);
                if (symi < 0) {
                    symi = newSymbols.push(sym);           // Add symbol to new symbols table
                    symi |= 0x80000000;                    // Upper bit means index refers to newSymbols
                }
                // Add a dummy relocation record for this symbol. 
                // This relocation does not need type, scale, or addend because the only purpose is to identify the symbol.
                // It does have a size, though, because this is checked later in writeRelocationTarget()
                rel.r_offset = iInstr + fInstr->addrPos; // Position of relocated field
                rel.r_section = section;
                rel.r_addend = -4;
                rel.r_sym = (uint32_t)symi;
                relocations.addUnique(rel);
            }
        }
    }
}


void CDisassembler::followJumpTable(uint32_t symi, uint32_t RelType) {
    // Check jump/call table and its targets
    // to do !!
}


void CDisassembler::markCodeAsDubious() {
    // Remember that this may be data in a code segment
}


// List of instructionlengths, used in parseInstruction
static const uint8_t lengthList[8] = {1,1,1,1,2,2,3,4};


void CDisassembler::parseInstruction() {
    // Parse one opcode at position iInstr
    instructionWarning = 0;

    // Get instruction
    pInstr = (STemplate*)(sectionBuffer + iInstr);

    // Get op1
    uint8_t op = pInstr->a.op1;

    // Get format
    format = (pInstr->a.il << 8) + (pInstr->a.mode << 4); // Construct format = (il,mode,submode)
                                                          
    // Get submode
    switch (format) {
    case 0x200: case 0x220:  // submode in mode2
        format += pInstr->a.mode2;
        break;
    case 0x250: case 0x310: // Submode for jump instructions etc.
        if (op < 8) {
            format += op;  op = pInstr->b[0] & 0x3F;
        }
        else {
            format += 8;
        }
        break;
    }

    // Get format details
    if ((format & 0xFEF) == 0x160) {
        // tiny instruction pair
        static SFormat formT = {0x160, 2, 0, 31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        fInstr = &formT;
    }
    else {
        // Look up format details
        static SFormat form;
        fInstr = &formatList[lookupFormat(pInstr->q)];
        format = fInstr->format2;                          // Include subformat depending on op1
        if (fInstr->tmpl == 0xE && pInstr->a.op2) {
            // Single format instruction if op2 != 0
            form = *fInstr;
            form.cat = 1;
            fInstr = &form;
        }
    }

    // Get operand type
    if (fInstr->ot == 0) {                                 // Operand type determined by OT field
        operandType = pInstr->a.ot;                        // Operand type
        if (!(pInstr->a.mode & 6) && !(fInstr->vect & 0x10)) {
            // Check use of M bit
            format |= (operandType & 4) << 5;              // Add M bit to format
            operandType &= ~4;                             // Remove M bit from operand type
        }
    }
    else if ((fInstr->ot & 0xF0) == 0x10) {                // Operand type fixed. Value in formatList
        operandType = fInstr->ot & 7;
    }
    else if (fInstr->ot == 0x32) {                         // int32 for even op1, int64 for odd op1
        operandType = 2 + (pInstr->a.op1 & 1);
    }
    else if (fInstr->ot == 0x35) {                         // Float for even op1, double for odd op1
        operandType = 5 + (pInstr->a.op1 & 1);
    }
    else {
        operandType = 0;                                   // Error in formatList. Should not occur
    }

    // Find instruction length
    instrLength = lengthList[pInstr->i[0] >> 29];           // Length up to 3 determined by il. Length 4 by upper bit of mode

    // Find any reasons for warnings
    //findWarnings(p);

    // Find any errors
    //findErrors(p);
}



/*****************************************************************************
Functions for reading instruction list from comma-separated file,
sorting, and searching
*****************************************************************************/

// Members of class CCSVFile for reading comma-separated file

// Read and parse file
void CCSVFile::parse() {
    // Sorry for the ugly code!

    const char * fields[numInstructionColumns];  // pointer to each field in line
    int fi = 0;                                  // field index
    uint32_t i, j;                               // loop counters
    char * s, * t = 0;                           // point to begin and end of field
    char c;
    char separator = 0;                          // separator character, preferably comma
    int line = 1;                                // line number
    SInstruction record;                         // record constructed from line
    memset(fields, 0, sizeof(fields));
                                                 
    if (data_size==0) read(2);                    // read file if it has not already been read
    if (err.number()) return;

    // loop through file
    for (i = 0; i < data_size; i++) {
        // find begin of field, quoted or not
        s = (char*)buf() + i;
        c = *s;
        if (c == ' ') continue;                  // skip leading spaces

        if (c == '"' || c == 0x27) {             // single or double quote
            fields[fi] = s+1;                    // begin of quoted string
            for (i++; i < data_size; i++) {      // search for matching end quote
                t = (char*)buf() + i;
                if (*t == c) {
                    *t = 0; i++;                 // End quote found. Put end of string here
                    goto SEARCHFORCOMMA;
                }
                if (*t == '\n') break;           // end of line found before end quote
            }
            // end quote not found
            err.submit(ERR_INSTRUCTION_LIST_QUOTE, line);
            return;
        }
        if (c == '\r' || c == '\n') 
            goto NEXTLINE;  // end of line found
        if (c == separator || c == ',') {
            // empty field
            fields[fi] = "";            
            goto SEARCHFORCOMMA;
        }

        // Anything else: begin of unquoted string
        fields[fi] = s;
        // search for end of field

    SEARCHFORCOMMA:
        for (; i < data_size; i++) {  // search for comma after field
            t = (char*)buf() + i;
            if (*t == separator || (separator == 0 && (*t == ',' || *t == ';' || *t == '\t'))) {
                separator = *t;               // separator set to the first comma, semicolon or tabulator
                *t = 0;                       // put end of string here
                goto NEXTFIELD;
            }
            if (*t == '\n') break;        // end of line found before comma
        }
        fi++; 
        goto NEXTLINE;

    NEXTFIELD:
        // next field
        fi++;
        if (fi != numInstructionColumns) continue;
        // end of last field

    NEXTLINE:            
        for (; i < data_size; i++) {  // search for end. of line
            t = (char*)buf() + i;
            // accept newlines as "\r", "\n", or "\r\n"
            if (*t == '\r' || *t == '\n') break;
        }         
        if (*t == '\r' && *(t+1) == '\n') i++;  // end of line is two characters
        *t = 0;  // terminate line

        // make any remaining fields blank
        for (; fi < numInstructionColumns; fi++) {
            fields[fi] = "";
        }
        // Begin next line
        line++;
        fi = 0;

        // Check if blank or heading record
        if (fields[2][0] < '0' || fields[2][0] > '9') continue;

        // save values to record
        // most fields are decimal or hexadecimal numbers
        record.id = (uint32_t)interpretNumber(fields[1]);
        record.category = (uint32_t)interpretNumber(fields[2]);
        record.format = interpretNumber(fields[3]);
        record.templt = (uint32_t)interpretNumber(fields[4]);
        record.sourceoperands = (uint32_t)interpretNumber(fields[6]);
        record.op1 = (uint32_t)interpretNumber(fields[7]);
        record.op2 = (uint32_t)interpretNumber(fields[8]);
        record.optypesgp = (uint32_t)interpretNumber(fields[9]);
        record.optypesscalar = (uint32_t)interpretNumber(fields[10]);
        record.optypesvector = (uint32_t)interpretNumber(fields[11]);
        record.opimmediate = (uint32_t)interpretNumber(fields[12]);
        // copy strings from fields 0 and 5, convert to upper or lower case
        for (j = 0; j < sizeof(record.template_variant)-1; j++) {
            c = fields[5][j];
            if (c == 0) break;
            record.template_variant[j] = toupper(c);
        }
        record.template_variant[j] = 0;
        for (j = 0; j < sizeof(record.name)-1; j++) {
            c = fields[0][j];
            if (c == 0) break;
            record.name[j] = tolower(c);
        }
        record.name[j] = 0;

        // add record to list
        instructionlist.push(record);

    }
}

// Interpret number in instruction list
uint64_t CCSVFile::interpretNumber(const char * text) {
    uint32_t error = 0;
    uint64_t result = uint64_t(::interpretNumber(text, 64, &error));
    if (error)  err.submit(ERR_INSTRUCTION_LIST_SYNTAX, text);
    return result;
} 


// Interpret a string with a decimal, binary, octal, or hexadecimal number
int64_t interpretNumber(const char * text, uint32_t maxLength, uint32_t * error) {
    int state = 0;           // 0: begin, 1: after 0, 
                             // 2: after 0x, 3: after 0b, 4: after 0o
                             // 5: after decimal digit, 6: trailing space
    uint64_t number = 0;
    uint8_t c, clower, digit;
    bool sign = false;
    uint32_t i;
    *error = 0;
    if (text == 0) {
        *error = 1; return number;
    }

    for (i = 0; i < maxLength; i++) {
        c = text[i];                    // read character
        clower = c | 0x20;              // convert to lower case
        if (clower == 'x') {
            if (state != 1) {
                *error = 1;  return 0;
            }
            state = 2;
        }
        else if (clower == 'o') {
            if (state != 1) { 
                *error = 1;  return 0;
            }
            state = 4;
        }
        else if (clower == 'b' && state == 1) {
            state = 3;
        }
        else if (c >= '0' && c <= '9') {
            // digit 0 - 9
            digit = c - '0'; 
            switch (state) {
            case 0:
                state = (digit == 0) ? 1 : 5;
                number = digit;
                break;
            case 1:
                state = 5;
                // continue in case 5:
            case 5:
                // decimal
                number = number * 10 + digit;
                break;
            case 2:
                // hexadecimal
                number = number * 16 + digit;
                break;
            case 3:
                // binary
                if (digit > 1) {
                    *error = 1;  return 0;
                }
                number = number * 2 + digit;
                break;
            case 4:
                // octal
                if (digit > 7) {
                    *error = 1;  return 0;
                }
                number = number * 8 + digit;
                break;
            default:
                *error = 1; 
                return 0;
            }
        }
        else if (clower >= 'a' && clower <= 'f') {
            // hexadecimal digit
            digit = clower - ('a' - 10);
            if (state != 2)  {
                *error = 1;  return 0;
            }
            number = number * 16 + digit;
        }
        else if (c == ' ' || c == '+') {
            // ignore leading or trailing blank or plus
            if (state > 0) state = 6;
        }
        else if (c == '-') {
            // change sign
            if (state != 0) {
                *error = 1;  return 0;
            }
            sign = ! sign;
        }
        else if (c == 0) break;  // end of string
        else {
            // illegal character
            *error = 1;  return 0;
        }        
    }
    if (sign) number = uint64_t(-int64_t(number));
    return (int64_t)number;
}

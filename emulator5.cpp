﻿/****************************  emulator5.cpp  ********************************
* Author:        Agner Fog
* date created:  2018-02-18
* Last modified: 2020-10-24
* Version:       1.11
* Project:       Binary tools for ForwardCom instruction set
* Description:
* Emulator: Execution functions for single format instructions, part 1
*
* Copyright 2020 GNU General Public License http://www.gnu.org/licenses
*****************************************************************************/

#include "stdafx.h"


// Format 1.0 A. Three general purpose registers

// Currently no instructions with format 1.0


// Format 1.1 C. One general purpose register and a 16 bit immediate operand. int64

static uint64_t move_16s(CThread * t) {
    // Move 16-bit sign-extended constant to general purpose register.
    return t->parm[2].q;
}

static uint64_t move_16u(CThread * t) {
    // Move 16-bit zero-extended constant to general purpose register.
    return t->parm[2].s;
}

static uint64_t shift16_add(CThread * t) {
    // Shift 16-bit signed constant left by 16 and add.
    t->parm[2].qs <<= 16;
    return f_add(t);
}

static uint64_t shifti1_move(CThread * t) {
    // RD = IM2 << IM1. Sign-extend IM2 to 64 bits and shift left by the unsigned value IM1
    return (t->parm[2].qs >> 8) << t->parm[2].b;
}

static uint64_t shifti1_add(CThread * t) {
    // RD += IM2 << IM1. Sign-extend IM2 to 64 bits and shift left by the unsigned value IM1 and add
    t->parm[2].q = (t->parm[2].qs >> 8) << t->parm[2].b;
    return f_add(t);
}

static uint64_t shifti1_and(CThread * t) {
    // RD &= IM2 << IM1
    return t->parm[1].q & ((t->parm[2].qs >> 8) << t->parm[2].b);
}

static uint64_t shifti1_or(CThread * t) {
    // RD |= IM2 << IM1
    return t->parm[1].q | ((t->parm[2].qs >> 8) << t->parm[2].b);
}

static uint64_t shifti1_xor(CThread * t) {
    // RD ^= IM2 << IM1
    return t->parm[1].q ^ ((t->parm[2].qs >> 8) << t->parm[2].b);
}

// Format 1.8 B. Two general purpose registers and an 8-bit immediate operand. int64

static uint64_t abs_64(CThread * t) {
    // Absolute value of signed integer. 
    // IM1 determines handling of overflow: 0: wrap around, 1: saturate, 2: zero.
    SNum a = t->parm[1];
    uint64_t sizeMask = dataSizeMask[t->operandType]; // mask for data size
    uint64_t signBit = (sizeMask >> 1) + 1;        // sign bit
    if ((a.q & sizeMask) == signBit) {  // overflow
        if (t->parm[2].b & 4) t->interrupt(INT_OVERFL_SIGN);
        switch (t->parm[2].b & ~4) {
        case 0:  return a.q;     // wrap around
        case 1:  return sizeMask >> 1; // saturate
        case 2:  return 0;       // zero
        default: t->interrupt(INT_INST_ILLEGAL);
        }
    }
    if (a.q & signBit) {  // negative
        a.qs = - a.qs;    // change sign
    }
    return a.q;
}

static uint64_t shifti_add(CThread * t) {
    // Shift and add. RD += RS << IM1
    SNum a = t->parm[0];
    SNum b = t->parm[1];
    SNum c = t->parm[2];
    SNum r1, r2;                                 // result
    r1.q = b.q << c.b;                           // shift left
    uint8_t nbits = dataSizeTableBits[t->operandType];
    if (c.q >= nbits) r1.q = 0;                  // shift out of range gives zero
    r2.q = a.q + r1.q;                           // add
    /*
    if (t->numContr & MSK_OVERFL_I) {  // check for overflow
        if (t->numContr & MSK_OVERFL_SIGN) {  // check for signed overflow
            uint64_t sizeMask = dataSizeMask[t->operandType]; // mask for data size
            uint64_t signBit = (sizeMask >> 1) + 1;        // sign bit
            uint64_t ovfl = ~(a.q ^ r1.q) & (a.q ^ r2.q);  // overflow if a and b have same sign and result has opposite sign
            if (r1.qs >> c.b != b.qs || (ovfl & signBit) || c.q >= nbits) t->interrupt(INT_OVERFL_SIGN);  // signed overflow
        }
        else if (t->numContr & MSK_OVERFL_UNSIGN) {  // check for unsigned overflow
            if (r2.q < a.q || r1.q >> c.b != b.q || c.q >= nbits) t->interrupt(INT_OVERFL_UNSIGN);  // unsigned overflow
        }
    } */
    return r2.q;         // add
}

uint64_t bitscan_ (CThread * t) {
    // Bit scan forward or reverse. Find index to first or last set bit in RS
    SNum a = t->parm[1];                         // input value
    uint8_t IM1 = t->parm[2].b;                  // immediate operand
    a.q &= dataSizeMask[t->operandType];         // mask for operand size
    if (a.q == 0) {
        a.qs = IM1 & 4 ? -1 : 0; // return 0 or -1 if intput is 0
    }
    else if (IM1 & 1) {
        // reverse
        a.q = bitScanReverse(a.q);
    }
    else {
        // forward    
        a.q = bitScanForward(a.q);
    }
    return a.q;
}

static uint64_t roundp2(CThread * t) {
    // Round up or down to nearest power of 2.
    SNum a = t->parm[1];                         // input operand
    uint8_t IM1 = t->parm[2].b;                  // immediate operand
    a.q &= dataSizeMask[t->operandType];         // mask off unused bits
    if (dataSizeTable[t->operandType] > 8) t->interrupt(INT_INST_ILLEGAL); // illegal operand type
    if (a.q == 0) {
        a.qs = IM1 & 2 ? -1 : 0;                 // return 0 or -1 if the intput is 0
    }
    else if (!(a.q & (a.q-1))) {
        return a.q;                              // the number is a power of 2. Return unchanged
    }
    else if (IM1 & 1) {
        // round up to nearest power of 2
        uint32_t s = bitScanReverse(a.q);        // highest set bit
        if (s+1 >= dataSizeTableBits[t->operandType]) { // overflow
            a.qs = IM1 & 4 ? -1 : 0;             // return 0 or -1 on overflow
        }
        else {
            a.q = (uint64_t)1 << (s+1);          // round up
        }
    }
    else {
        // round down to nearest power of 2
        a.q = (uint64_t)1 << bitScanReverse(a.q);
    }
    return a.q;
}

static uint32_t popcount32(uint32_t x) { // count bits in 32 bit integer. used by popcount_ function
    x = x - ((x >> 1) & 0x55555555);
    x = (x >> 2 & 0x33333333) + (x & 0x33333333);
    x = (x + (x >> 4)) & 0x0F0F0F0F;
    x = (x + (x >> 8)) & 0x00FF00FF;
    x = uint16_t(x + (x >> 16));
    return x;
}

uint64_t popcount_ (CThread * t) {
    // Count the number of bits in RS that are 1
    SNum a = t->parm[1];                         // value
    a.q &= dataSizeMask[t->operandType];         // mask for operand size
    return popcount32(a.i) + popcount32(a.q >> 32);
}

static uint64_t read_spec(CThread * t) {
    // Read special register RS into g. p. register RD.
    uint8_t rs = t->operands[4];                 // source register
    uint64_t retval = 0;
    if (rs == 0) {                               // numcontr register
        retval = t->numContr;    
    }
    else {
        t->interrupt(INT_INST_ILLEGAL);          // other register not implemented
    }
    return retval;
}

static uint64_t write_spec(CThread * t) {
    // Write g. p. register RS to special register RD
    uint8_t rd = t->operands[0];                 // destination register
    SNum a = t->parm[1];                         // value
    if (rd == 0) {                               // numcontr register
        t->numContr = a.i | 1;                   // bit 0 must be set
        if (((t->numContr ^ t->lastMask) & MSK_SUBNORMAL) != 0) {
            // subnormal status changed
            enableSubnormals(t->numContr & MSK_SUBNORMAL);
        }
        t->lastMask = t->numContr;
    }
    else {
        t->interrupt(INT_INST_ILLEGAL);          // other register not implemented
    }
    t->returnType = 0;
    return 0;
}

static uint64_t read_capabilities(CThread * t) {
    // Read capabilities register RS into g. p. register RD
    return 0;
}

static uint64_t write_capabilities(CThread * t) {
    // Write g. p. register RS to capabilities register RD
    t->returnType = 0;
    return 0;
}

static uint64_t read_perf(CThread * t) {
    // Read performance counter
    return 0;
}

static uint64_t read_sys(CThread * t) {
    // Read system register RS into g. p. register RD
    return 0;
}

static uint64_t write_sys(CThread * t) {
    // Write g. p. register RS to system register RD
    t->returnType = 0;
    return 0;
}

static uint64_t push_r(CThread * t) {
    // push one or more g.p. registers on a stack pointed to by rd
    int32_t step = dataSizeTable[t->operandType];
    if (!(t->parm[4].i & 0x80)) step = -step;
    uint8_t reg0 = t->operands[0] & 0x1F;   // pointer register
    uint8_t reg1 = t->operands[4] & 0x1F;   // first push register
    uint8_t reglast = t->parm[4].i & 0x1F;  // last push register
    uint8_t reg;
    uint64_t pointer = t->registers[reg0];
    // loop through registers to push
    for (reg = reg1; reg <= reglast; reg++) {
        pointer += (int64_t)step;
        uint64_t value = t->registers[reg];
        t->writeMemoryOperand(value, pointer);    
        t->listResult(value);
    }
    t->registers[reg0] = pointer;
    return pointer;
}

static uint64_t pop_r(CThread * t) {
    // pop one or more g.p. registers from a stack pointed to by rd
    int32_t step = dataSizeTable[t->operandType];
    if (t->parm[4].i & 0x80) step = -step;
    uint8_t reg0 = t->operands[0] & 0x1F;   // pointer register
    uint8_t reg1 = t->operands[4] & 0x1F;   // first push register
    uint8_t reglast = t->parm[4].i & 0x1F;  // last push register
    uint8_t reg;
    uint64_t pointer = t->registers[reg0];
    // loop through registers to pop in reverse order
    for (reg = reglast; reg >=  reg1; reg--) {
        uint64_t value = t->readMemoryOperand(pointer);
        t->registers[reg] = value;
        pointer += (int64_t)step;
        t->listResult(value);
    }
    t->registers[reg0] = pointer;
    return pointer;
}

static uint64_t vectors_used(CThread * t) {
    // tell which vector registers are used
    uint32_t value = 0;
    for (int i = 0; i < 32; i++) {
        if (t->vectorLength[i] > 0) value |= 1 << i;
    }
    return value;
}


// Format 2.9 A. Three general purpose registers and a 32-bit immediate operand

static uint64_t move_hi32(CThread * t) {
    // Load 32-bit constant into the high part of a general purpose register. The low part is zero. RD = IM2 << 32.
    return t->parm[2].q << 32;
}

static uint64_t insert_hi32(CThread * t) {
    // Insert 32-bit constant into the high part of a general purpose register, leaving the low part unchanged.
    return t->parm[2].q << 32 | t->parm[1].i;
}

static uint64_t add_32u(CThread * t) {
    // Add zero-extended 32-bit constant to general purpose register
    t->parm[2].q = t->parm[2].i;
    return f_add(t);
}

static uint64_t sub_32u(CThread * t) {
    // Subtract zero-extended 32-bit constant from general purpose register
    t->parm[2].q = t->parm[2].i;
    return f_sub(t);
}

static uint64_t add_hi32(CThread * t) {
    // Add 32-bit constant to high part of general purpose register. RD = RT + (IM2 << 32).
    t->parm[2].q <<= 32;
    return f_add(t);
}

static uint64_t and_hi32(CThread * t) {
    // AND high part of general purpose register with 32-bit constant. RD = RT & (IM2 << 32).
    return t->parm[1].q & t->parm[2].q << 32;
}

static uint64_t or_hi32(CThread * t) {
    // OR high part of general purpose register with 32-bit constant. RD = RT | (IM2 << 32).
    return t->parm[1].q | t->parm[2].q << 32;
}

static uint64_t xor_hi32(CThread * t) {
    // XOR high part of general purpose register with 32-bit constant. RD = RT ^ (IM2 << 32).
    return t->parm[1].q ^ t->parm[2].q << 32;
}

static uint64_t replace_bits(CThread * t) {
    // Replace a group of contiguous bits in RT by a specified constant
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    uint64_t val = b.s;                          // value to insert
    uint8_t  pos = uint8_t(b.i >> 16);           // start position
    uint8_t  num = uint8_t(b.i >> 24);           // number of bits to replace
    if (num > 32 || pos + num > 64) t->interrupt(INT_INST_ILLEGAL);
    uint64_t mask = ((uint64_t)1 << num) - 1;    // mask with 'num' 1-bits
    return (a.q & ~(mask << pos)) | ((val & mask) << pos);
}

static uint64_t address_(CThread * t) {
    // RD = RT + IM2, RT can be THREADP (28), DATAP (29) or IP (30)
    t->returnType = 0x13;
    return t->memAddress;
}

// Format 1.2 A. Three vector register operands

static uint64_t set_len(CThread * t) {
    // RD = vector register RT with length changed to value of g.p. register RS
    // set_len: the new length is indicated in bytes
    // set_num: the new length is indicated in elements
    uint8_t  rd = t->operands[0];
    uint8_t  rs = t->operands[4];
    uint8_t  rt = t->operands[5];
    uint32_t oldLength = t->vectorLength[rt];
    uint64_t newLength = t->registers[rs];
    if (t->op == 2) newLength *= dataSizeTable[t->operandType];  // set_num: multiply by operand size
    if (newLength > t->MaxVectorLength) newLength = t->MaxVectorLength;
    if (newLength > oldLength) { 
        memcpy(t->vectors.buf() + rd*t->MaxVectorLength, t->vectors.buf() + rt*t->MaxVectorLength, oldLength);  // copy first part from RT
        memset(t->vectors.buf() + rd*t->MaxVectorLength + oldLength, 0, size_t(newLength - oldLength));               // set the rest to zero
    }
    else {
        memcpy(t->vectors.buf() + rd*t->MaxVectorLength, t->vectors.buf() + rt*t->MaxVectorLength, size_t(newLength));  // copy newLength from RT
    }
    t->vectorLength[rd] = (uint32_t)newLength;             // set new length
    t->vect = 4;                                           // stop vector loop
    t->running = 2;                                       // don't save RD
    return 0;
}

static uint64_t get_len(CThread * t) {
    // Get length of vector register RT into general purpose register RD
    // get_len: get the length in bytes
    // get_num: get the length in elements
    uint8_t  rd = t->operands[0];
    uint8_t  rt = t->operands[4];
    uint32_t length = t->vectorLength[rt];                 // length of RT
    if (t->op == 3) length >>= dataSizeTableLog[t->operandType];  // get_num: divide by operand size (round down)
    t->registers[rd] = length;                             // save in g.p. register, not vector register
    t->vect = 4;                                           // stop vector loop
    t->running = 2;                                        // don't save to vector register RD
    t->returnType = 0x12;                                  // debug return output
    return length;
}

uint64_t insert_(CThread * t) {
    // Replace one element in vector RD, starting at offset RS·OS, with scalar RT
    uint64_t pos;                         // position of element insert
    uint8_t  rd = t->operands[3];         // source and destination register
    uint8_t  operandType = t->operandType;       // operand type
    uint64_t returnval;
    uint8_t  dsizelog = dataSizeTableLog[operandType]; // log2(elementsize)
    uint8_t  sourceVector;
    if (t->fInstr->format2 == 0x120) {
        uint8_t  rs = t->operands[4];         // index register
        pos = t->registers[rs] << dsizelog;
        sourceVector = t->operands[5];      // source register 
    }
    else {   // format 0x130
        sourceVector = t->operands[4];      // source register 
        pos = t->parm[4].q << dsizelog;
    }
    t->vectorLengthR = t->vectorLength[rd];
    if (pos == t->vectorOffset) {
        if (dsizelog == 4) {  // 128 bits.
            t->parm[5].q = t->readVectorElement(sourceVector, 8); // high part of 128-bit result
        }
        returnval = t->readVectorElement(sourceVector, 0);      // first element of sourceVector
    }
    else {
        if (dsizelog == 4) {  // 128 bits.
            t->parm[5].q = t->readVectorElement(rd, t->vectorOffset + 8); // high part of 128-bit result
        }
        returnval = t->parm[0].q;                     // rd unchanged
    }
    return returnval;
}

uint64_t extract_(CThread * t) {
    // Extract one element from vector RT, at offset RS·OS or IM1·OS, with size OS 
    // into scalar in vector register RD.
    uint8_t  rd = t->operands[0];                   // destination register
    uint8_t  rsource;                               // source vector
    uint8_t  operandType = t->operandType;                 // operand type
    uint8_t  dsizelog = dataSizeTableLog[operandType];     // log2(elementsize)
    uint64_t pos;                                       // position = index * OS
    if (t->fInstr->format2 == 0x120) {
        rsource = t->operands[5];                   // source vector
        uint8_t  rs = t->operands[4];                   // index register
        pos = t->registers[rs] << dsizelog;
    }
    else {  // format 0x130
        rsource = t->operands[4];                   // source vector
        pos = t->parm[4].q << dsizelog;
    }

    uint32_t sourceLength = t->vectorLength[rsource];           // length of source vector
    uint64_t result;
    if (pos >= sourceLength) {
        result = 0;                                        // beyond end of source vector
    }
    else {
        int8_t * source = t->vectors.buf() + rsource * t->MaxVectorLength; // address of rsource data
        result = *(uint64_t*)(source+pos);                 // no problem reading too much, it will be cut off later if the operand size is < 64 bits
        if (dsizelog >= 4) {                               // 128 bits
            t->parm[5].q = *(uint64_t*)(source+pos+8);     // store high part of 128 bit element
        }
    }
    t->vectorLength[rd] = 1 << dsizelog;                   // length of destination vector
    t->vect = 4;                                           // stop vector loop
    return result;
}



static uint64_t compress_sparse(CThread * t) {
    // Compress sparse vector elements indicated by mask bits into contiguous vector. 
    // RS = length of input vector
    uint8_t  rd = t->operands[0];         // destination vector
    uint8_t  rs = t->operands[4];         // length indicator
    uint8_t  rt = t->operands[5];         // source vector
    uint8_t  rm = t->operands[1];         // mask vector
    uint32_t sourceLength = t->vectorLength[rt]; // length of source vector
    uint32_t maskLength = t->vectorLength[rm];   // length of mask vector
    uint64_t newLength = t->registers[rs];       // length of destination
    uint32_t elementSize = dataSizeTable[t->operandType];            // size of each element
    int8_t * source = t->vectors.buf() + rt*t->MaxVectorLength;      // address of RT data
    int8_t * masksrc = t->vectors.buf() + rm*t->MaxVectorLength;     // address of mask data
    int8_t * destination = t->vectors.buf() + rd*t->MaxVectorLength; // address of RD data
    // limit length
    if (newLength > t->MaxVectorLength) newLength = t->MaxVectorLength;
    if (newLength > maskLength) newLength = maskLength;              // no reason to go beyond mask
    if (newLength > sourceLength) {                                  // reading beyond the end of the source vector
        memset(source + sourceLength, 0, size_t(newLength - sourceLength));  // make sure the rest is zero
    }
    uint32_t pos1 = 0;                           // position in source vector
    uint32_t pos2 = 0;                           // position in destination vector
    // loop through mask register
    for (pos1 = 0; pos1 < newLength; pos1 += elementSize) {
        if (*(masksrc + pos1) & 1) {             // check mask bit
            // copy from pos1 in source to pos2 in destination
            switch (elementSize) {
            case 1:  // int8
                *(destination+pos2) = *(source+pos1);
                break;
            case 2:  // int16
                *(uint16_t*)(destination+pos2) = *(uint16_t*)(source+pos1);
                break;
            case 4:  // int32, float
                *(uint32_t*)(destination+pos2) = *(uint32_t*)(source+pos1);
                break;
            case 8:  // int64, double
                *(uint64_t*)(destination+pos2) = *(uint64_t*)(source+pos1);
                break;
            case 16:  // int128, float128
                *(uint64_t*)(destination+pos2)   = *(uint64_t*)(source+pos1);
                *(uint64_t*)(destination+pos2+8) = *(uint64_t*)(source+pos1+8);
                break;
            }
            pos2 += elementSize;
        }
    }
    // set new length of destination vector
    t->vectorLength[rd] = pos2;
    t->vect = 4;                                 // stop vector loop
    t->running = 2;                              // don't save. result has already been saved
    return 0;
}

static uint64_t expand_sparse(CThread * t) {
    // Expand contiguous vector into sparse vector with positions indicated by mask bits
    // RS = length of output vector
    uint8_t  rd = t->operands[0];         // destination vector
    uint8_t  rs = t->operands[4];         // length indicator
    uint8_t  rt = t->operands[5];         // source vector
    uint8_t  rm = t->operands[1];         // mask vector
    uint32_t sourceLength = t->vectorLength[rt]; // length of source vector
    uint32_t maskLength = t->vectorLength[rm];   // length of mask vector
    uint64_t newLength = t->registers[rs];       // length of destination
    uint32_t elementSize = dataSizeTable[t->operandType & 7];        // size of each element
    int8_t * source = t->vectors.buf() + rt*t->MaxVectorLength;      // address of RT data
    int8_t * masksrc = t->vectors.buf() + rm*t->MaxVectorLength;     // address of mask data
    int8_t * destination = t->vectors.buf() + rd*t->MaxVectorLength; // address of RD data
    if (rd == rt) {
        // source and destination are the same. Make a temporary copy of source to avoid overwriting
        memcpy(t->tempBuffer, source, sourceLength);
        source = t->tempBuffer;    
    } 
    // limit length
    if (newLength > t->MaxVectorLength) newLength = t->MaxVectorLength;
    if (newLength > maskLength) newLength = maskLength;              // no reason to go beyond mask
    if (newLength > sourceLength) {                                  // reading beyond the end of the source vector
        memset(source + sourceLength, 0, size_t(newLength - sourceLength));  // make sure the rest is zero
    }
    uint32_t pos1 = 0;                           // position in source vector
    uint32_t pos2 = 0;                           // position in destination vector

    // loop through mask register
    for (pos2 = 0; pos2 < newLength; pos2 += elementSize) {
        if (*(masksrc + pos2) & 1) {             // check mask bit
            // copy from pos1 in source to pos2 in destination
            switch (elementSize) {
            case 1:  // int8
                *(destination+pos2) = *(source+pos1);
                break;
            case 2:  // int16
                *(uint16_t*)(destination+pos2) = *(uint16_t*)(source+pos1);
                break;
            case 4:  // int32, float
                *(uint32_t*)(destination+pos2) = *(uint32_t*)(source+pos1);
                break;
            case 8:  // int64, double
                *(uint64_t*)(destination+pos2) = *(uint64_t*)(source+pos1);
                break;
            case 16:  // int128, float128
                *(uint64_t*)(destination+pos2)   = *(uint64_t*)(source+pos1);
                *(uint64_t*)(destination+pos2+8) = *(uint64_t*)(source+pos1+8);
                break;
            }
            pos1 += elementSize;
        }
        else {
            // mask is zero. insert zero
            switch (elementSize) {
            case 1:  // int8
                *(destination+pos2) = 0;
                break;
            case 2:  // int16
                *(uint16_t*)(destination+pos2) = 0;
                break;
            case 4:  // int32, float
                *(uint32_t*)(destination+pos2) = 0;
                break;
            case 8:  // int64, double
                *(uint64_t*)(destination+pos2) = 0;
                break;
            case 16:  // int128, float128
                *(uint64_t*)(destination+pos2)   = 0;
                *(uint64_t*)(destination+pos2+8) = 0;
                break;
            }

        }
    }
    // set new length of destination vector
    t->vectorLength[rd] = pos2;
    t->vect = 4;                                 // stop vector loop
    t->running = 2;                              // don't save. result has already been saved
    return 0;
}

static uint64_t broad_(CThread * t) {
    // Broadcast first element of vector RT into all elements of RD with length RS
    uint8_t  rs = t->operands[4];         // index register
    uint8_t  rt = t->operands[5];         // source vector
    uint8_t  rd = t->operands[0];         // source vector
    uint64_t index = t->registers[rs];           // value of RS
    uint64_t destinationLength = index; // length of destination register
    if (destinationLength > t->MaxVectorLength) destinationLength = t->MaxVectorLength; // limit length
    // set length of destination register, let vector loop continue to this length
    t->vectorLength[rd] = t->vectorLengthR = (uint32_t)destinationLength;
    return t->readVectorElement(rt, 0);         // first element of RT
}

static uint64_t bits2bool(CThread * t) {
    // The lower n bits of RT are unpacked into a boolean vector RD with length RS
    // with one bit in each element, where n = RS / OS.
    uint8_t  rd = t->operands[0];         // destination vector
    uint8_t  rt = t->operands[5];         // RT = source vector
    uint8_t  rs = t->operands[4];         // RS indicates length
    SNum mask = t->parm[3];                      // mask
    uint8_t * source = (uint8_t*)t->vectors.buf() + rt*t->MaxVectorLength; // address of RT data
    uint8_t * destination = (uint8_t*)t->vectors.buf() + rd*t->MaxVectorLength; // address of RD data
    uint64_t destinationLength = t->registers[rs]; // value of RS = length of destination
    uint8_t  dsizelog = dataSizeTableLog[t->operandType]; // log2(elementsize)
    if (destinationLength > t->MaxVectorLength) destinationLength = t->MaxVectorLength; // limit length
    // set length of destination register
    t->vectorLength[rd] = (uint32_t)destinationLength;
    uint32_t num = (uint32_t)destinationLength >> dsizelog; // number of elements
    destinationLength = num << dsizelog;          // round down length to nearest multiple of element size
    // number of bits in source
    uint32_t srcnum = t->vectorLength[rt] * 8;
    if (num < srcnum) num = srcnum;              // limit to the number of bits in source
    mask.q &= -(int64_t)2;                       // remove lower bit of mask. it will be replaced by source bit
    // loop through bits
    for (uint32_t i = 0; i < num; i++) {
        uint8_t bit = (source[i / 8] >> (i & 7)) & 1;  // extract single bit from source
        switch (dsizelog) {
        case 0:  // int8
            *destination = mask.b | bit;  break;
        case 1:  // int16
            *(uint16_t*)destination = mask.s | bit;  break;
        case 2:  // int32
            *(uint32_t*)destination = mask.i | bit;  break;
        case 3:  // int64
            *(uint64_t*)destination = mask.q | bit;  break;
        case 4:  // int128
            *(uint64_t*)destination = mask.q | bit;
            *(uint64_t*)(destination+8) = mask.q | bit;  
            break;
        }
        destination += (uint64_t)1 << dsizelog;
    }
    t->vect = 4;                                           // stop vector loop
    t->running = 2;                                        // don't save RD
    if ((t->returnType & 7) >= 5) t->returnType -= 3;      // make return type integer
    return 0;
}

static uint64_t bool2bits(CThread * t) {
    // The boolean vector RT with length RS is packed into the lower n bits of RD, 
    // taking bit 0 of each element, where n = RS / OS. 
    // The length of RD will be at least sufficient to contain n bits.

    uint8_t  rd = t->operands[0];         // destination vector
    uint8_t  rt = t->operands[5];         // RT = source vector
    uint8_t  rs = t->operands[4];         // RS indicates length
    uint8_t * destination = (uint8_t*)t->vectors.buf() + (int64_t)rd * t->MaxVectorLength; // address of RD data
    uint64_t length = t->registers[rs]; // value of RS = length of destination
    uint8_t  dsizelog = dataSizeTableLog[t->operandType]; // log2(elementsize)
    if (length > t->MaxVectorLength) length = t->MaxVectorLength; // limit length
    uint32_t num = (uint32_t)length >> dsizelog;           // number of elements
    length = num << dsizelog;                    // round down length to nearest multiple of element size 
    // collect bits into blocks of 32 bits
    uint32_t bitblock = 0;
    // loop through elements of source vector
    for (uint32_t i = 0; i < num; i++) {
        uint8_t  bit = t->readVectorElement(rt, i << dsizelog) & 1;
        uint8_t  bitindex = i & 31;                        // bit position with 32 bit block of destination
        bitblock |= bit << bitindex;                       // add bit to bitblock
        if (bitindex == 31 || i == num - 1) {              // last bit in this block
            *(uint32_t*)(destination + (i/8 & -4)) = bitblock; // write 32 bit block to destination
            bitblock = 0;                                  // start next block
        }
    }
    // round up length of destination to multiple of 4 bytes
    uint32_t destinationLength = ((num+7)/8 + 3) & -4;
    if (destinationLength == 0) {
        destinationLength = 4;  *(uint32_t*)destination = 0;
    }
    // set length of destination vector (must be done after reading source because source and destination may be the same)
    t->vectorLength[rd] = destinationLength;
    t->vect = 4;                                           // stop vector loop
    t->running = 2;                                        // don't save RD
    if ((t->returnType & 7) >= 5) t->returnType -= 3;      // make return type integer
    return 0;
}

static uint64_t bool_reduce(CThread * t) {
    // integer vector: bool_reduce. The boolean vector RT with length RS is reduced by combining bit 0 of all elements.
    // The output is a scalar integer where bit 0 is the AND combination of all the bits, 
    // and bit 1 is the OR combination of all the bits. 
    // The remaining bits are reserved for future use
    // float vector: category_reduce: Each bit in RD indicates that at least one element in RT belongs
    // to a certain category:
    // bit 0: NAN, bit 1: zero, bit 2: - subnormal, bitt 3: + subnormal,
    // bit 4: - normal, bit 5: + normal, bit 6: - INF, bit 7: + INF
    uint8_t  rd = t->operands[0];                   // destination vector
    uint8_t  rt = t->operands[5];                   // RT = source vector
    uint8_t  rs = t->operands[4];                   // RS indicates length
    uint8_t bitOR = 0;                                     // OR combination of all bits
    uint8_t bitAND = 1;                                    // AND combination of all bits
    uint64_t result = 0;                                   // result value
    uint8_t * source = (uint8_t*)t->vectors.buf() + rt*t->MaxVectorLength; // address of RT data
    uint64_t length = t->registers[rs];                    // value of RS = length of destination
    if (length > t->MaxVectorLength) length = t->MaxVectorLength; // limit length
    uint32_t elementSize = dataSizeTable[t->operandType];  // vector element size
    uint8_t  dsizelog = dataSizeTableLog[t->operandType];  // log2(elementsize)
    uint32_t sourceLength = t->vectorLength[rt];           // length of source vector
    length = length >> dsizelog << dsizelog;               // round down to nearest multiple of element size
    if (length > sourceLength) {
        length = sourceLength;                             // limit to length of source vector
        bitAND = 0;                                        // bits beyond vector are 0
    }
    switch (t->operandType) {
    case 0: case 1: case 2: case 3: case 4:                // integer types: bool_reduce
        for (uint32_t pos = 0; pos < length; pos += elementSize) { // loop through elements of source vector
            uint8_t  bit = *(source + pos) & 1;            // get bit from source vector element
            bitOR |= bit;  bitAND &= bit;
        }
        result = bitAND | bitOR << 1;
        break;
    case 5:                                                // float type: category_reduce
        for (uint32_t pos = 0; pos < length; pos += elementSize) { // loop through elements of source vector
            uint32_t val = *(int32_t*)(source + pos);
            uint8_t exponent = val >> 23 & 0xFF;           // isolate exponent
            uint8_t category;
            if (exponent == 0xFF) {                        // nan or inf
                if (val << 9) category = 1;                // nan
                else if (val >> 31) category = 0x40;       // -inf
                else category = 0x80;                      // + inf
            }
            else if (exponent == 0) {
                if ((val << 9) == 0) category = 2;         // zero
                else if (val >> 31)  category = 4;         // - subnormal
                else category = 8;                         // + subnormal
            }
            else if (val >> 31) category = 0x10;           // - normal    
            else category = 0x20;                          // + normal
            result |= category;                            // combine categories
        }
        break;
    case 6:                                                // double type: category_reduce
        for (uint32_t pos = 0; pos < length; pos += elementSize) {    // loop through elements of source vector
            uint64_t val = *(int64_t*)(source + pos);
            uint32_t exponent = val >> 52 & 0x7FF;         // isolate exponent
            uint8_t category;
            if (exponent == 0x7FF) {                       // nan or inf
                if (val << 12) category = 1;               // nan
                else if (val >> 63) category = 0x40;       // -inf
                else category = 0x80;                      // + inf
            }
            else if (exponent == 0) {
                if ((val << 12) == 0) category = 2;        // zero
                else if (val >> 63)  category = 4;         // - subnormal
                else category = 8;                         // + subnormal
            }
            else if (val >> 63) category = 0x10;           // - normal    
            else category = 0x20;                          // + normal
            result |= category;                            // combine categories
        }
        break;
    default: 
        t->interrupt(INT_INST_ILLEGAL);
    }
    t->vectorLength[rd] = 8;                               // set length of destination vector to 64 bits
    uint8_t * destination = (uint8_t*)t->vectors.buf() + rd*t->MaxVectorLength; // address of RD data
    *(uint64_t*)destination = result;                      // write 64 bits to destination
    // (using writeVectorElement would possibly write less than 64 bits, leaving some of the destination vector unchanged)
    t->vect = 4;                                           // stop vector loop
    t->running = 2;                                        // don't save RD. It has already been saved
    if ((t->returnType & 7) >= 5) t->returnType -= 3;      // make return type integer
    return result;
}

static uint64_t shift_expand(CThread * t) {
    // Shift vector RT up by RS bytes and extend the vector length by RS. 
    // The lower RS bytes of RD will be zero.
    uint8_t  rd = t->operands[0];         // destination vector
    uint8_t  rt = t->operands[5];         // RT = source vector
    uint8_t  rs = t->operands[4];         // RS indicates length
    uint8_t * source = (uint8_t*)t->vectors.buf() + rt*t->MaxVectorLength; // address of RT data
    uint8_t * destination = (uint8_t*)t->vectors.buf() + rd*t->MaxVectorLength; // address of RD data
    uint64_t shiftCount = t->registers[rs];      // value of RS = shift count
    if (shiftCount > t->MaxVectorLength) shiftCount = t->MaxVectorLength; // limit length
    uint32_t sourceLength = t->vectorLength[rt]; // length of source vector
    uint32_t destinationLength = sourceLength + (uint32_t)shiftCount; // length of destination vector
    if (destinationLength > t->MaxVectorLength) destinationLength = t->MaxVectorLength; // limit length
    // set length of destination vector
    t->vectorLength[rd] = destinationLength;
    // set lower part of destination to zero
    memset(destination, 0, size_t(shiftCount));
    // copy the rest from source
    if (destinationLength > shiftCount) {
        memmove(destination + shiftCount, source, size_t(destinationLength - shiftCount));
    }
    t->vect = 4;                                 // stop vector loop
    t->running = 2;                              // don't save RD. It has already been saved
    return 0;
}

static uint64_t shift_reduce(CThread * t) {
    // Shift vector RT down RS bytes and reduce the length by RS. 
    // The lower RS bytes of RT are lost
    uint8_t  rd = t->operands[0];         // destination vector
    uint8_t  rt = t->operands[5];         // RT = source vector
    uint8_t  rs = t->operands[4];         // RS indicates length
    uint8_t * source = (uint8_t*)t->vectors.buf() + rt*t->MaxVectorLength; // address of RT data
    uint8_t * destination = (uint8_t*)t->vectors.buf() + rd*t->MaxVectorLength; // address of RD data
    uint32_t sourceLength = t->vectorLength[rt]; // length of source vector
    uint64_t shiftCount = t->registers[rs];      // value of RS = shift count
    if (shiftCount > sourceLength) shiftCount = sourceLength; // limit length
    uint32_t destinationLength = sourceLength - (uint32_t)shiftCount; // length of destination vector
    t->vectorLength[rd] = destinationLength;     // set length of destination vector
    // copy data from source
    if (destinationLength > 0) {
        memmove(destination, source + shiftCount, destinationLength);
    }
    t->vect = 4;                                           // stop vector loop
    t->running = 2;                                        // don't save RD. It has already been saved
    return 0;
}

static uint64_t shift_up(CThread * t) {
    // Shift elements of vector RT up RS elements.
    // The lower RS elements of RD will be zero, the upper RS elements of RT are lost.
    uint8_t  rd = t->operands[0];         // destination vector
    uint8_t  rt = t->operands[5];         // RT = source vector
    uint8_t  rs = t->operands[4];         // RS indicates length
    uint8_t * source = (uint8_t*)t->vectors.buf() + rt * t->MaxVectorLength; // address of RT data
    uint8_t * destination = (uint8_t*)t->vectors.buf() + rd * t->MaxVectorLength; // address of RD data
    uint8_t  dsizelog = dataSizeTableLog[t->operandType];  // log2(elementsize)
    uint64_t shiftCount = t->registers[rs] << dsizelog;      // value of RS = shift count, elements
    if (shiftCount > t->MaxVectorLength) shiftCount = t->MaxVectorLength; // limit length
    uint32_t sourceLength = t->vectorLength[rt]; // length of source vector
    t->vectorLength[rd] = sourceLength;          // set length of destination vector to the same as source vector
    // copy from source
    if (sourceLength > shiftCount) {
        memmove(destination + shiftCount, source, size_t(sourceLength - shiftCount));
    }
    // set lower part of destination to zero
    memset(destination, 0, size_t(shiftCount));
    t->vect = 4;                                           // stop vector loop
    t->running = 2;                                        // don't save RD. It has already been saved
    return 0;
}

static uint64_t shift_down(CThread * t) {
    // Shift elements of vector RT down RS elements.
    // The upper RS elements of RD will be zero, the lower RS elements of RT are lost.
    uint8_t  rd = t->operands[0];                   // destination vector
    uint8_t  rt = t->operands[5];                   // RT = source vector
    uint8_t  rs = t->operands[4];                   // RS indicates length
    uint8_t * source = (uint8_t*)t->vectors.buf() + rt*t->MaxVectorLength; // address of RT data
    uint8_t * destination = (uint8_t*)t->vectors.buf() + rd*t->MaxVectorLength; // address of RD data
    uint32_t sourceLength = t->vectorLength[rt];           // length of source vector
    uint8_t  dsizelog = dataSizeTableLog[t->operandType];  // log2(elementsize)
    uint64_t shiftCount = t->registers[rs] << dsizelog;    // value of RS = shift count, elements
    if (shiftCount > sourceLength) shiftCount = sourceLength; // limit length
    t->vectorLength[rd] = sourceLength;                    // set length of destination vector
    if (sourceLength > shiftCount) {                       // copy data from source
        memmove(destination, source + shiftCount, size_t(sourceLength - shiftCount));
    }
    if (shiftCount > 0) {                                  // set the rest to zero
        memset(destination + sourceLength - shiftCount, 0, size_t(shiftCount));
    }
    t->vect = 4;                                           // stop vector loop
    t->running = 2;                                        // don't save RD. It has already been saved
    return 0;
}

static uint64_t rotate_up (CThread * t) {
    // Rotate vector RT up one element. 
    // The length of the vector is RS bytes.
    uint8_t  rd = t->operands[0];         // destination vector
    uint8_t  rt = t->operands[5];         // RT = source vector
    uint8_t  rs = t->operands[4];         // RS indicates length
    int8_t * source = t->vectors.buf() + rt*t->MaxVectorLength; // address of RT data
    int8_t * destination = t->vectors.buf() + rd*t->MaxVectorLength; // address of RD data
    uint64_t length = t->registers[rs];          // value of RS = vector length
    if (length > t->MaxVectorLength) length = t->MaxVectorLength; // limit length
    uint32_t sourceLength = t->vectorLength[rt]; // length of source vector
    if (rd == rt) {
        // source and destination are the same. Make a temporary copy of source to avoid overwriting
        memcpy(t->tempBuffer, source, length);
        source = t->tempBuffer;    
    } 
    if (length > sourceLength) {                 // reading beyond the end of the source vector. make sure the rest is zero
        memset(source + sourceLength, 0, size_t(length - sourceLength));
    }
    uint32_t elementSize = dataSizeTable[t->operandType];            // size of each element
    if (elementSize > length) elementSize = (uint32_t)length;
    t->vectorLength[rd] = (uint32_t)length;                // set length of destination vector
    memcpy(destination, source + length - elementSize, elementSize); // copy top element to bottom
    memcpy(destination + elementSize, source, size_t(length - elementSize)); // copy the rest
    t->vect = 4;                                           // stop vector loop
    t->running = 2;                                        // don't save RD. It has already been saved
    return 0;
}

static uint64_t rotate_down (CThread * t) {
    // Rotate vector RT down one element. 
    // The length of the vector is RS bytes.
    uint8_t  rd = t->operands[0];         // destination vector
    uint8_t  rt = t->operands[5];         // RT = source vector
    uint8_t  rs = t->operands[4];         // RS indicates length
    int8_t * source = t->vectors.buf() + rt*t->MaxVectorLength; // address of RT data
    int8_t * destination = t->vectors.buf() + rd*t->MaxVectorLength; // address of RD data
    uint64_t length = t->registers[rs];          // value of RS = vector length
    if (length > t->MaxVectorLength) length = t->MaxVectorLength; // limit length
    if (rd == rt) {
        // source and destination are the same. Make a temporary copy of source to avoid overwriting
        memcpy(t->tempBuffer, source, length);
        source = t->tempBuffer;    
    }
    uint32_t sourceLength = t->vectorLength[rt]; // length of source vector
    if (length > sourceLength) {                 // reading beyond the end of the source vector. make sure the rest is zero
        memset(source + sourceLength, 0, size_t(length - sourceLength));
    }
    uint32_t elementSize = dataSizeTable[t->operandType];            // size of each element
    if (elementSize > length) elementSize = (uint32_t)length;
    t->vectorLength[rd] = (uint32_t)length;      // set length of destination vector
    memcpy(destination, source + elementSize, size_t(length - elementSize)); // copy down
    memcpy(destination + length - elementSize, source, elementSize); // copy the bottom element to top
    t->vect = 4;                                           // stop vector loop
    t->running = 2;                                        // don't save RD. It has already been saved
    return 0;
}

static uint64_t div_ex (CThread * t) {
    // Divide vector of double-size integers RS by integers RT. 
    // RS has element size 2·OS. These are divided by the even numbered elements of RT with size OS.
    // The truncated results are stored in the even-numbered elements of RD. 
    // The remainders are stored in the odd-numbered elements of RD
    // op = 24: signed, 25: unsigned
    SNum result;                                 // quotient
    SNum remainder;                              // remainder
    SNum a_lo = t->parm[1];                      // low part of dividend
    SNum b = t->parm[2];                         // divisor
    uint8_t rs = t->operands[4];          // RS indicates length
    uint32_t elementSize = dataSizeTable[t->operandType];            // size of each element
    SNum a_hi;
    a_hi.q = t->readVectorElement(rs, t->vectorOffset + elementSize);  // high part of dividend
    uint64_t sizemask = dataSizeMask[t->operandType]; // mask for operand size
    uint64_t signbit = (sizemask >> 1) + 1;      // mask indicating sign bit
    SNum mask = t->parm[3];                      // mask register value or NUMCONTR
    bool isUnsigned = t->op & 1;                 // 24: signed, 25: unsigned
    bool overflow = false;
    int sign = 0;                                // 1 if result is negative

    if (!isUnsigned) {                           // convert signed division to unsigned
        if (b.q & signbit) {                     // b is negative. make it positive
            b.qs = -b.qs;  sign = 1;
        }
        if (a_hi.q & signbit) {                  // a is negative. make it positive
            a_lo.qs = - a_lo.qs;
            a_hi.q  = ~ a_hi.q;
            if ((a_lo.q & sizemask) == 0) a_hi.q++; // carry from low to high part
            sign ^= 1;                           // invert sign
        }
    }
    // limit data size
    b.q    &= sizemask;
    a_hi.q &= sizemask;
    a_lo.q &= sizemask;
    result.q = 0;
    remainder.q = 0;
    // check for overflow
    if (a_hi.q >= b.q || b.q == 0) {
        overflow = true;
    }
    else {
        switch (t->operandType) {
        case 0: // int8
            a_lo.s |= a_hi.s << 8;
            result.s = a_lo.s / b.s;
            remainder.s = a_lo.s % b.s;
            break;
        case 1: // int16
            a_lo.i |= a_hi.i << 16;
            result.i = a_lo.i / b.i;
            remainder.i = a_lo.i % b.i;
            break;
        case 2: // int32
            a_lo.q |= a_hi.q << 32;
            result.q = a_lo.q / b.q;
            remainder.q = a_lo.q % b.q;
            break;
        case 3: // int64
            // to do: implement 128/64 -> 64 division by intrinsic or inline assembly
            // or bit shift method (other methods are too complex)
        default:
            t->interrupt(INT_INST_ILLEGAL);
        }
    }
    // check sign
    if (sign) {
        if (result.q == signbit) overflow = true;
        result.qs = - result.qs;
        if (remainder.q == signbit) overflow = true;
        remainder.qs = - remainder.qs;
    }
    if (overflow) {
        if (isUnsigned) {   // unsigned overflow
            //if (mask.i & MSK_OVERFL_UNSIGN) t->interrupt(INT_OVERFL_UNSIGN);  // unsigned overflow
            result.q = sizemask;
            remainder.q = 0;
        }
        else {       // signed overflow
            //if (mask.i & MSK_OVERFL_SIGN) t->interrupt(INT_OVERFL_SIGN);      // signed overflow
            result.q = signbit;
            remainder.q = 0;
        }
    }
    t->parm[5].q = remainder.q;                  // save remainder
    return result.q;
}

static uint64_t sqrt_ (CThread * t) {
    // square root
    SNum a = t->parm[2];                         // input operand
    SNum result;  result.q = 0;
    uint32_t mask = t->parm[3].i;
    uint8_t operandType = t->operandType;
    bool detectExceptions = (mask & (0xF << MSKI_EXCEPTIONS)) != 0;  // make NAN if exceptions
    bool roundingMode = (mask & (3 << MSKI_ROUNDING)) != 0;  // non-standard rounding mode
    bool error = false;
    switch (operandType) {
    case 0:   // int8
        if (a.bs < 0) error = true;
        else result.b = (int8_t)sqrtf(a.bs);
        break;
    case 1:   // int16
        if (a.ss < 0) error = true;
        else result.s = (int16_t)sqrtf(a.bs);
        break;
    case 2:   // int32
        if (a.is < 0) error = true;
        else result.i = (int32_t)sqrt(a.bs);
        break;
    case 3:   // int64
        if (a.qs < 0) error = true;
        else result.q = (int64_t)sqrt(a.bs);
        break;
    case 5:   // float
        if (a.f < 0) {
            result.q = t->makeNan(nan_invalid_sqrt, operandType);
        }
        else {
            if (detectExceptions) clearExceptionFlags();   // clear previous exceptions
            if (roundingMode) setRoundingMode(mask >> MSKI_ROUNDING);
            result.f = sqrtf(a.f);                         // calculate square root
            if (roundingMode) setRoundingMode(0);
            if (detectExceptions) {
                uint32_t x = getExceptionFlags();          // read exceptions
                if ((mask & MSK_UNDERFLOW) && (x & 0x10)) result.q = t->makeNan(nan_underflow, operandType);
                else if ((mask & MSK_INEXACT) && (x & 0x20)) result.q = t->makeNan(nan_inexact, operandType);
            }
        }
        break;
    case 6:   // double
        if (a.d < 0) {
            result.q = t->makeNan(nan_invalid_sqrt, operandType);
        }
        else {
            if (detectExceptions) clearExceptionFlags();   // clear previous exceptions
            if (roundingMode) setRoundingMode(mask >> MSKI_ROUNDING);
            result.d = sqrt(a.d);                          // calculate square root
            if (roundingMode) setRoundingMode(0);
            if (detectExceptions) {
                uint32_t x = getExceptionFlags();          // read exceptions
                if ((mask & MSK_UNDERFLOW) && (x & 0x10)) result.q = t->makeNan(nan_underflow, operandType);
                else if ((mask & MSK_INEXACT) && (x & 0x20)) result.q = t->makeNan(nan_inexact, operandType);
            }
        }
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
    }
    return result.q;
}

static uint64_t add_c (CThread * t) {
    // Add with carry. Vector has two elements. 
    // The upper element is used as carry on input and output
    SNum a = t->parm[1];                         // input operand
    SNum b = t->parm[2];                         // input operand
    SNum result;
    uint8_t rs = t->operands[4];          // RS is first input vector
    uint32_t elementSize = dataSizeTable[t->operandType]; // size of each element
    SNum carry;
    carry.q = t->readVectorElement(rs, t->vectorOffset + elementSize);  // high part of first input vector
    uint64_t sizeMask = dataSizeMask[t->operandType]; // mask for data size    
    result.q = a.q + b.q;                        // add    
    uint8_t newCarry = (result.q & sizeMask) < (a.q & sizeMask); // get new carry
    result.q += carry.q & 1;                     // add carry
    if ((result.q & sizeMask) == 0) newCarry = 1;// carry
    t->parm[5].q = newCarry;                     // save new carry
    return result.q;
}

static uint64_t sub_b (CThread * t) {
    // Subtract with borrow. Vector has two elements. 
    // The upper element is used as borrow on input and output
    SNum a = t->parm[1];                         // input operand
    SNum b = t->parm[2];                         // input operand
    SNum result;
    uint8_t rs = t->operands[4];          // RS is first input vector
    uint32_t elementSize = dataSizeTable[t->operandType]; // size of each element
    SNum carry;
    carry.q = t->readVectorElement(rs, t->vectorOffset + elementSize);  // high part of first input vector
    uint64_t sizeMask = dataSizeMask[t->operandType]; // mask for data size    
    result.q = a.q - b.q;                        // subtract
    uint8_t newCarry = (result.q & sizeMask) > (a.q & sizeMask); // get new carry
    result.q -= carry.q & 1;                     // subtract borrow
    if ((result.q & sizeMask) == sizeMask) newCarry = 1;// borrow
    t->parm[5].q = newCarry;                     // save new borrow
    return result.q;
}

static uint64_t add_ss (CThread * t) {
    // Add integer vectors, signed with saturation
    SNum a = t->parm[1];                         // input operand
    SNum b = t->parm[2];                         // input operand
    SNum result;
    uint64_t sizeMask = dataSizeMask[t->operandType]; // mask for data size
    uint64_t signBit = (sizeMask >> 1) + 1;      // sign bit
    result.q = a.q + b.q;                        // add
    uint64_t overfl = ~(a.q ^ b.q) & (a.q ^ result.q); // overflow if a and b have same sign and result has opposite sign
    if (overfl & signBit) { // overflow
        result.q = (sizeMask >> 1) + ((a.q & signBit) != 0); // INT_MAX or INT_MIN
    }
    return result.q;
}

static uint64_t sub_ss (CThread * t) {
    // subtract integer vectors, signed with saturation
    SNum a = t->parm[1];                         // input operand
    SNum b = t->parm[2];                         // input operand
    SNum result;
    uint64_t sizeMask = dataSizeMask[t->operandType]; // mask for data size
    uint64_t signBit = (sizeMask >> 1) + 1;      // sign bit
    result.q = a.q - b.q;                        // subtract
    uint64_t overfl = (a.q ^ b.q) & (a.q ^ result.q); // overflow if a and b have different sign and result has opposite sign of a
    if (overfl & signBit) { // overflow
        result.q = (sizeMask >> 1) + ((a.q & signBit) != 0); // INT_MAX or INT_MIN
    }
    return result.q;
}

static uint64_t add_us (CThread * t) {
    // Add integer vectors, unsigned with saturation
    SNum a = t->parm[1];                         // input operand
    SNum b = t->parm[2];                         // input operand
    SNum result;
    uint64_t sizeMask = dataSizeMask[t->operandType]; // mask for data size
    result.q = a.q + b.q;                        // add
    if ((result.q & sizeMask) < (a.q & sizeMask)) {   // overflow
        result.q = sizeMask;                     // UINT_MAX
    }
    return result.q;
}

static uint64_t sub_us (CThread * t) {
    // subtract integer vectors, unsigned with saturation
    SNum a = t->parm[1];                         // input operand
    SNum b = t->parm[2];                         // input operand
    SNum result;
    uint64_t sizeMask = dataSizeMask[t->operandType]; // mask for data size
    result.q = a.q - b.q;                        // add
    if ((result.q & sizeMask) > (a.q & sizeMask)) {   // overflow
        result.q = 0;                            // 0
    }
    return result.q;
}

static uint64_t mul_ss (CThread * t) {
    // multiply integer vectors, signed with saturation
    SNum a = t->parm[1];                         // input operand
    SNum b = t->parm[2];                         // input operand
    SNum result;
    uint64_t sizeMask = dataSizeMask[t->operandType]; // mask for data size
    uint64_t signBit = (sizeMask >> 1) + 1;      // sign bit

    // check for overflow
    bool overflow = false;
    switch (t->operandType) {
    case 0:  // int8
        result.is = (int32_t)a.bs * (int32_t)b.bs;                        // multiply
        overflow = result.bs != result.is;  break;
    case 1:  // int16
        result.is = (int32_t)a.ss * (int32_t)b.ss;                        // multiply
        overflow = result.ss != result.is;  break;
    case 2:  // int32
        result.qs = (int64_t)a.is * (int64_t)b.is;                        // multiply
        overflow = result.is != result.qs;  break;
    case 3:  // int64
        result.qs = a.qs * b.qs;                        // multiply
        overflow = fabs((double)a.qs * (double)b.qs - (double)result.qs) > 1.E8; 
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
    }
    if (overflow) {
        result.q = (sizeMask >> 1) + (((a.q ^ b.q) & signBit) != 0);  // INT_MAX or INT_MIN
    }
    return result.q;
}

static uint64_t mul_us (CThread * t) {
    // multiply integer vectors, unsigned with saturation
    SNum a = t->parm[1];                         // input operand
    SNum b = t->parm[2];                         // input operand
    SNum result;
    uint64_t sizeMask = dataSizeMask[t->operandType]; // mask for data size

    // check for overflow
    bool overflow = false;
    switch (t->operandType) {
    case 0:
        result.i = (uint32_t)a.b * (uint32_t)b.b;                        // multiply
        overflow = result.b != result.i;  break;
    case 1:
        result.i = (uint32_t)a.s * (uint32_t)b.s;
        overflow = result.s != result.i;  break;
    case 2:
        result.q = (uint64_t)a.i * (uint64_t)b.i;
        overflow = result.i != result.q;  break;
    case 3:
        result.q = a.q * b.q;
        overflow = fabs((double)a.q * (double)b.q - (double)result.q) > 1.E8; 
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
    }
    if (overflow) {
        result.q = sizeMask;
    }
    return result.q;
}

/*
static uint64_t shift_ss (CThread * t) {
    // Shift left integer vectors, signed with saturation
    SNum a = t->parm[1];                         // input operand
    SNum b = t->parm[2];                         // input operand
    SNum result;
    result.q = a.q << b.i;                       // shift left
    uint64_t sizeMask = dataSizeMask[t->operandType]; // mask for data size
    uint64_t signBit = (sizeMask >> 1) + 1;      // sign bit
    uint32_t bits1 = bitScanReverse(a.q & sizeMask) + 1;  // number of bits in a
    uint32_t bitsMax = dataSizeTable[t->operandType]; // maximum number of bits if negative
    uint8_t negative = (a.q & signBit) != 0;     // a is negative
    if (!negative) bitsMax--;                    // maximum number of bits if positive
    if ((a.q & sizeMask) != 0 && bits1 + (b.q & sizeMask) > bitsMax) { // overflow
        result.q = (sizeMask >> 1) + negative;   // INT_MAX or INT_MIN
    }
    return result.q;
}

static uint64_t shift_us (CThread * t) {
    // Shift left integer vectors, unsigned with saturation
    SNum a = t->parm[1];                         // input operand
    SNum b = t->parm[2];                         // input operand
    SNum result;
    result.q = a.q << b.i;                       // shift left
    uint64_t sizeMask = dataSizeMask[t->operandType]; // mask for data size
    uint32_t bits1 = bitScanReverse(a.q & sizeMask) + 1;  // number of bits in a
    uint32_t bitsMax = dataSizeTable[t->operandType]; // maximum number of bits
    if ((a.q & sizeMask) != 0 && bits1 + (b.q & sizeMask) > bitsMax) { // overflow
        result.q = sizeMask;                     // UINT_MAX
    }
    return result.q;
} */

/*
Instructions with overflow check use the even-numbered vector elements for arithmetic instructions.
Each following odd-numbered vector element is used for overflow detection. If the first source operand
is a scalar then the result operand will be a vector with two elements.
Overflow conditions are indicated with the following bits:
bit 0. Unsigned integer overflow (carry).
bit 1. Signed integer overflow.
The values are propagated so that the overflow result of the operation is OR’ed with the corresponding
values of both input operands. */

static uint64_t add_oc (CThread * t) {
    // add with overflow check
    SNum a = t->parm[1];                         // input operand
    SNum b = t->parm[2];                         // input operand
    uint8_t rs = t->operands[4];          // RS is first input vector
    uint8_t rt = t->operands[5];          // RT is first input vector
    uint32_t elementSize = dataSizeTable[t->operandType]; // size of each element
    SNum carry;
    carry.q  = t->readVectorElement(rs, t->vectorOffset + elementSize); // high part of first input vector
    carry.q |= t->readVectorElement(rt, t->vectorOffset + elementSize);  // high part of second input vector
    SNum result;

    if (t->operandType < 4) {
        uint64_t sizeMask = dataSizeMask[t->operandType]; // mask for data size
        result.q = a.q + b.q;                    // add
        if ((result.q & sizeMask) < (a.q & sizeMask)) { // unsigned overflow
            carry.b |= 1;
        }
        // signed overflow if a and b have same sign and result has opposite sign
        uint64_t signedOverflow = ~(a.q ^ b.q) & (a.q ^ result.q);
        uint64_t signBit = (sizeMask >> 1) + 1;      // sign bit
        if (signedOverflow & signBit) {
            carry.b |= 2;
        }
    }
    else {
        // unsupported operand type
        t->interrupt(INT_INST_ILLEGAL);  result.q = 0;
    }
    t->parm[5].q = carry.q & 3;                  // return carry
    return result.q;                             // return result
}

static uint64_t sub_oc (CThread * t) {
    // subtract with overflow check
    SNum a = t->parm[1];                         // input operand
    SNum b = t->parm[2];                         // input operand
    uint8_t rs = t->operands[4];          // RS is first input vector
    uint8_t rt = t->operands[5];          // RT is second input vector
    uint32_t elementSize = dataSizeTable[t->operandType]; // size of each element
    SNum carry;
    carry.q  = t->readVectorElement(rs, t->vectorOffset + elementSize);  // high part of first input vector
    carry.q |= t->readVectorElement(rt, t->vectorOffset + elementSize);  // high part of second input vector
    SNum result; 
    if (t->operandType < 4) {
        uint64_t sizeMask = dataSizeMask[t->operandType]; // mask for data size
        result.q = a.q - b.q;                    // add
        if ((result.q & sizeMask) > (a.q & sizeMask)) { // unsigned overflow
            carry.b |= 1;
        }
        // signed overflow if a and b have opposite sign and result has opposite sign of a
        uint64_t signedOverflow = (a.q ^ b.q) & (a.q ^ result.q);
        uint64_t signBit = (sizeMask >> 1) + 1;      // sign bit
        if (signedOverflow & signBit) {
            carry.b |= 2;
        }
    }
    else {
        // unsupported operand type
        t->interrupt(INT_INST_ILLEGAL);  result.q = 0;
    }
    t->parm[5].q = carry.q & 3;                  // return carry
    return result.q;                             // return result
}

static uint64_t mul_oc (CThread * t) {
    // multiply with overflow check
    SNum a = t->parm[1];                         // input operand
    SNum b = t->parm[2];                         // input operand
    uint8_t rs = t->operands[4];          // RS is first input vector
    uint8_t rt = t->operands[5];          // RT is second input vector
    uint32_t elementSize = dataSizeTable[t->operandType]; // size of each element
    SNum carry;
    carry.q  = t->readVectorElement(rs, t->vectorOffset + elementSize);  // high part of first input vector
    carry.q |= t->readVectorElement(rt, t->vectorOffset + elementSize);  // high part of second input vector
    SNum result;
    bool signedOverflow = false;
    bool unsignedOverflow = false;

    // multiply and check for signed and unsigned overflow
    switch (t->operandType) {
    case 0:
        result.is = (int32_t)a.bs * (int32_t)b.bs;                        // multiply
        unsignedOverflow = result.b != result.i;
        signedOverflow = result.bs != result.is;
        break;
    case 1:
        result.is = (int32_t)a.ss * (int32_t)b.ss;
        unsignedOverflow = result.s != result.i;
        signedOverflow = result.ss != result.is;
        break;
    case 2:
        result.qs = (int64_t)a.is * (int64_t)b.is;
        unsignedOverflow = result.q != result.i;
        signedOverflow = result.qs != result.is;
        break;
    case 3:
        result.qs = a.qs * b.qs;
        unsignedOverflow = fabs((double)a.q * (double)b.q - (double)result.q) > 1.E8;
        signedOverflow   = fabs((double)a.qs * (double)b.qs - (double)result.qs) > 1.E8;
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
    }
    if (unsignedOverflow) carry.b |= 1;     // unsigned overflow
    if (signedOverflow)   carry.b |= 2;     // signed overflow
    t->parm[5].q = carry.q & 3;                  // return carry
    return result.q;                             // return result
}

static uint64_t div_oc (CThread * t) {
    // signed divide with overflow check
    SNum a = t->parm[1];                         // input operand
    SNum b = t->parm[2];                         // input operand
    uint8_t rs = t->operands[4];          // RS is first input vector
    uint8_t rt = t->operands[5];          // RT is second input vector
    uint32_t elementSize = dataSizeTable[t->operandType]; // size of each element
    SNum carry;
    carry.q  = t->readVectorElement(rs, t->vectorOffset + elementSize);  // high part of first input vector
    carry.q |= t->readVectorElement(rt, t->vectorOffset + elementSize);  // high part of second input vector
    SNum result;

    // to do: rounding mode!!

    switch (t->operandType) {
    case 0:  // int8
        if (b.b == 0) {
            result.i = 0x80; carry.b |= 3;     // signed and unsigned overflow
        }
        else if (a.b == 0x80 && b.bs == -1) {
            result.i = 0x80; carry.b |= 2;     // signed overflow
        }
        else result.i = a.bs / b.bs;
        break;
    case 1:  // int16
        if (b.s == 0) {
            result.i = 0x8000; carry.b |= 3;     // signed and unsigned overflow
        }
        else if (a.s == 0x8000 && b.ss == -1) {
            result.i = 0x8000; carry.b |= 2;     // signed overflow
        }
        else result.i = a.ss / b.ss;
        break;
    case 2:  // int32
        if (b.i == 0) {
            result.i = sign_f; carry.b |= 3;     // signed and unsigned overflow
        }
        else if (a.i == sign_f && b.is == -1) {
            result.i = sign_f; carry.b |= 2;     // signed overflow
        }
        else result.i = a.is / b.is;
        break;
    case 3:  // int64
        if (b.q == 0) {
            result.q = sign_d; carry.b |= 3;     // signed and unsigned overflow
        }
        else if (a.q == sign_d && b.qs == int64_t(-1)) {
            result.q = sign_d; carry.b |= 2;     // signed overflow
        }
        else result.qs = a.qs / b.qs;
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
    }
    t->parm[5].q = carry.q & 3;                  // return carry
    return result.q;                             // return result
}


static uint64_t read_call_stack (CThread * t) {
    // read internal call stack. RD = vector register destination of length RS, RT-RS = internal address
    return 0; // to do
}

static uint64_t write_call_stack (CThread * t) {
    // write internal call stack. RD = vector register source of length RS, RT-RS = internal address 
    return 0; // to do
}

static uint64_t read_memory_map (CThread * t) {
    // read memory map. RD = vector register destination of length RS, RT-RS = internal address 
    return 0; // to do
}

static uint64_t write_memory_map (CThread * t) {
    // write memory map. RD = vector register
    return 0; // to do
}

static uint64_t input_ (CThread * t) {
    // read from input port. RD = vector register, RT = port address, RS = vector length
    return 0; // to do
}

static uint64_t output_ (CThread * t) {
    // write to output port. RD = vector register source operand, RT = port address, RS = vector length
    return 0; // to do
}


// tables of single format instructions
// Format 1.0 A. Three general purpose registers
PFunc funcTab4[64] = {
    0, 0, 0, 0, 0, 0, 0, 0
};

// Format 1.1 C. One general purpose register and a 16 bit immediate operand. int64
PFunc funcTab5[64] = {
    move_16s, move_16u, f_add, 0, 0, f_mul, f_div, shift16_add,  // 0 - 7
    0, 0, 0, 0, 0, 0, 0, 0,                                      // 8 - 15
    shifti1_move, shifti1_add, shifti1_and, shifti1_or, shifti1_xor, 0, 0, 0, // 16 -23
};

// Format 1.2 A. Three vector register operands
PFunc funcTab6[64] = {
    set_len, get_len, set_len, get_len, insert_, extract_, broad_, 0,  // 0  - 7
    compress_sparse, expand_sparse, 0, 0, bits2bool, bool2bits, bool_reduce, 0,  // 8 - 15
    shift_expand, shift_reduce, shift_up, shift_down, rotate_up, rotate_down, 0, 0, // 16 - 23
    div_ex, div_ex, sqrt_, 0, add_c, sub_b, add_ss, add_us,                                // 24 - 31
    sub_ss, sub_us, mul_ss, mul_us, 0, 0, add_oc, sub_oc,                                  // 32 - 39
    0, mul_oc, div_oc, 0, 0, 0, 0, 0,                                                      // 40 - 47
    0, 0, 0, 0, 0, 0, 0, 0,                                                                // 48 - 55
    0, 0, read_call_stack, write_call_stack, read_memory_map, write_memory_map, input_, output_
};


// Format 1.8 B. Two general purpose registers and an 8-bit immediate operand. int64
PFunc funcTab9[64] = {
    abs_64, shifti_add, bitscan_, roundp2, popcount_, 0, 0, 0,   // 0  - 7
    0, 0, 0, 0, 0, 0, 0, 0,                                      // 8 - 15
    0, 0, 0, 0, 0, 0, 0, 0,                                      // 16 - 23
    0, 0, 0, 0, 0, 0, 0, 0,                                      // 24 - 31
    read_spec, write_spec, read_capabilities, write_capabilities, read_perf, read_perf, read_sys, write_sys, // 32 - 39
    0, 0, 0, 0, 0, 0, 0, 0,                                      // 40 - 47
    0, 0, 0, 0, 0, 0, 0, 0,                                      // 48 - 55
    push_r, pop_r, 0, vectors_used, 0, 0, 0, 0                   // 56 - 63
};

// Format 2.9 A. Three general purpose registers and a 32-bit immediate operand
PFunc funcTab12[64] = {
    move_hi32, insert_hi32, add_32u, sub_32u, add_hi32, and_hi32, or_hi32, xor_hi32,  // 0  - 7
    0, replace_bits, 0, 0, 0, 0, 0, 0,                                                // 8 - 15
    0, 0, 0, 0, 0, 0, 0, 0,                                                           // 16 - 23
    0, 0, 0, 0, 0, 0, 0, 0,                                                           // 24 - 31
    address_, 0, 0, 0, 0, 0, 0, 0,                                                    // 32 - 39
    0, 0, 0, 0, 0, 0, 0, 0,                                                           // 40 - 47
};

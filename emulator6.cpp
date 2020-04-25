/****************************  emulator6.cpp  ********************************
* Author:        Agner Fog
* date created:  2018-02-18
* Last modified: 2020-04-21
* Version:       1.09
* Project:       Binary tools for ForwardCom instruction set
* Description:
* Emulator: Execution functions for single format instructions, continued
*
* Copyright 2018-2020 GNU General Public License http://www.gnu.org/licenses
*****************************************************************************/

#include "stdafx.h"

// Format 1.3 B. Two vector registers and a broadcast 8-bit immediate operand.

static uint64_t gp2vec (CThread * t) {
    // Move value of general purpose register RS to scalar in vector register RD.
    uint8_t  rd = t->operands[0];
    uint8_t  rs = t->operands[4];
    uint64_t result = t->registers[rs];                    // read general purpose register
    t->vectorLength[rd] = dataSizeTable[t->operandType];   // set length of destination
    t->vect = 4;                                           // stop vector loop
    return result;
}

static uint64_t vec2gp (CThread * t) {
    // Move value of first element of vector register RS to general purpose register RD.
    uint8_t  rd = t->operands[0];
    uint8_t  rs = t->operands[4];
    uint8_t size = dataSizeTable[t->operandType];
    if (size > t->vectorLength[rs]) size = t->vectorLength[rs]; // limit size to vector length
    uint64_t result = *(uint64_t*)(t->vectors.buf() + t->MaxVectorLength*rs); // read directly from vector
    if (size < 8) result &= ((uint64_t)1 << size*8) - 1;   // mask off to size
    t->registers[rd] = result;                             // write to general purpose register
    t->vect = 4;                                           // stop vector loop
    t->running = 2;                                        // don't save RD
    t->returnType &= ~ 0x100;                              // debug return type not vector
    return result;
}

static uint64_t read_spev (CThread * t) {
    // Read special register RT into vector register RD with length RS.
    // to do
    return 0;
}

static uint64_t make_sequence (CThread * t) {
    // Make a vector with RS sequential numbers. First value is IM1.
    uint8_t  rd = t->operands[0];
    uint8_t  rs = t->operands[4];
    int32_t  val = int8_t(t->pInstr->b[0]);      // immediate operand, sign extended integer
    uint64_t num = t->registers[rs];             // number of elements
    uint32_t elementSize = dataSizeTable[t->operandType];
    uint8_t  dsizelog = dataSizeTableLog[t->operandType]; // log2(elementsize)
    SNum temp;
    // limit length
    uint64_t length = num << dsizelog;
    if (length > t->MaxVectorLength) {
        length = t->MaxVectorLength;  num = length >> dsizelog;
    }
    // set length of rd
    t->vectorLength[rd] = (uint32_t)length;
    // loop through destination vector
    for (uint32_t pos = 0; pos < length; pos += elementSize) {
        switch (t->operandType) {
        case 0: case 1: case 2: case 3:
            t->writeVectorElement(rd, (uint64_t)(int64_t)val, pos);  break;
        case 4: 
            t->writeVectorElement(rd, (uint64_t)(int64_t)val, pos);          // int128
            t->writeVectorElement(rd, (uint64_t)((int64_t)val >> 63), pos+8); break;
        case 5:   // float
            temp.f = float(val);                 // convert to float
            t->writeVectorElement(rd, temp.q, pos); 
            break;
        case 6:   // double
            temp.d = double(val);                // convert to double
            t->writeVectorElement(rd, temp.q, pos); 
            break;
        default:
            t->interrupt(INT_INST_ILLEGAL);
        }
        val++;                                   // increment value
    }
    t->vect = 4;                                 // stop vector loop
    t->running = 2;                              // don't save RD
    return 0;
}

static uint64_t compress(CThread * t) {
    // Compress vector RT of length RS to a vector of half the length and half the element size.
    // Double precision -> single precision, 64-bit integer -> 32-bit integer, etc.

    // operands:
    uint8_t  rd = t->operands[0];
    uint8_t  rs = t->operands[4];
    uint8_t IM1 = t->parm[4].b;
    if (IM1 & 0xC0) t->interrupt(INT_INST_ILLEGAL);
    //uint32_t initLength = t->vectorLength[rt];
    uint32_t oldLength = t->vectorLength[rs]; // (uint32_t)t->registers[rs];
    uint32_t newLength = oldLength / 2;
    uint32_t pos;  // position in destination vector
    uint8_t overflowU  = 0;                      // unsigned overflow in current element
    uint8_t overflowS  = 0;                      // signed overflow in current element
    uint8_t overflowU2 = 0;                      // unsigned overflow in any element
    uint8_t overflowS2 = 0;                      // signed overflow in any element
    uint8_t overflowF2 = 0;                      // floating point overflow in any element
    SNum mask = t->parm[3];                      // options mask
    int8_t * source = t->vectors.buf() + (uint64_t)rs * t->MaxVectorLength;      // address of RS data
    int8_t * destination = t->vectors.buf() + (uint64_t)rd * t->MaxVectorLength; // address of RD data

    uint8_t roundingMode = (IM1 >> 3) & 7;       // floating point rounding mode
    if (roundingMode == 0) roundingMode = ((t->parm[3].i >> MSKI_ROUNDING) & 7) | 4;
    uint8_t exceptionControl = IM1 & 7;          // floating point exception enable bits:
                                                 // 1: overflow, 2: underflow, 4: inexact
    if (exceptionControl == 0) {                 // floating point exception control
        exceptionControl = mask.i >> (MSKI_EXCEPTIONS + 1) & 7; // exceptions from NUMCONTR
    }
    else if (exceptionControl == 7) {
        exceptionControl = 0;                    // 7 means none (5 means all)
    }

    switch (t->operandType) {                    //  source operand type
    case 0:   // int8 -> int4
        for (pos = 0; pos < newLength; pos += 1) {
            union {
                uint16_t s;
                uint8_t b[2];
            } u;
            u.s = *(uint16_t*)(source + 2*pos);  // two values to convert to one byte
            for (int i = 0; i < 2; i++) {        // loop for two bytes to convert
                uint8_t val = u.b[i];
                overflowU = val > 0x0F;          // unsigned overflow
                overflowS = val - 0xF8 > 0x0F;   // signed overflow
                overflowU2 |= overflowU;  overflowS2 |= overflowS;
                switch (IM1 & 7) {
                case 0: default:                 // wrap around
                    break;
                case 4:                          // signed integer overflow gives zero
                    if (overflowS) val = 0;
                    break;
                case 5:                          // signed integer overflow gives signed saturation
                    if (overflowS) val = 0x7 + (val >> 7);
                    break;
                case 6:                          // unsigned integer overflow gives zero
                    if (overflowU) val = 0;
                    break;
                case 7:                          // unsigned integer overflow gives unsigned saturation
                    if (overflowU) val = 0xF;
                    break;
                }
                u.b[i] = val;           
            }
            uint8_t val2 = (u.b[0] & 0xF) | u.b[1] << 4;
            *(uint8_t*)(destination + pos) = val2;         // store two values
        }
        t->returnType = 0x110;            
        break;
    case 1:   // int16 -> int8
        for (pos = 0; pos < newLength; pos += 1) {
            uint16_t val = *(uint16_t*)(source + 2*pos);   // value to convert
            overflowU = val > 0xFF;                        // unsigned overflow
            overflowS = val - 0xFF80 > 0xFF;               // signed overflow
            overflowU2 |= overflowU;  overflowS2 |= overflowS;
            switch (IM1 & 7) {
            case 0: default: // wrap around
                break;
            case 4:          // signed integer overflow gives zero
                if (overflowS) val = 0;
                break;
            case 5:          // signed integer overflow gives signed saturation
                if (overflowS) val = 0x7F + (val >> 15);
                break;
            case 6:          // unsigned integer overflow gives zero
                if (overflowU) val = 0;
                break;
            case 7:          // unsigned integer overflow gives unsigned saturation
                if (overflowU) val = 0xFF;
                break;
            }
            *(uint8_t*)(destination + pos) = (uint8_t)val; // store value
        }
        t->returnType = 0x110;            
        break;
    case 2:   // int32 -> int16
        for (pos = 0; pos < newLength; pos += 2) {
            uint32_t val = *(uint32_t*)(source + 2*pos);   // value to convert
            overflowU = val > 0xFFFF;                      // unsigned overflow
            overflowS = val - 0xFFFF8000 > 0xFFFF;         // signed overflow
            switch (IM1 & 7) {
            case 0: default: // wrap around
                break;
            case 4:          // signed integer overflow gives zero
                if (overflowS) val = 0;
                break;
            case 5:          // signed integer overflow gives signed saturation
                if (overflowS) val = 0x7FFF + (val >> 31);
                break;
            case 6:          // unsigned integer overflow gives zero
                if (overflowU) val = 0;
                break;
            case 7:          // unsigned integer overflow gives unsigned saturation
                if (overflowU) val = 0xFFFF;
                break;
            }
            *(uint16_t*)(destination + pos) = (uint16_t)val; // store value
        }
        t->returnType = 0x111;            
        break;
    case 3:   // int64 -> int32
        for (pos = 0; pos < newLength; pos += 4) {
            uint64_t val = *(uint64_t*)(source + 2*pos);  // value to convert
            overflowU = val > 0xFFFFFFFFU;                // unsigned overflow
            overflowS = val - 0xFFFFFFFF80000000 > 0xFFFFFFFFU; // signed overflow
            switch (IM1 & 7) {
            case 0: default: // wrap around
                break;
            case 4:          // signed integer overflow gives zero
                if (overflowS) val = 0;
                break;
            case 5:          // signed integer overflow gives signed saturation
                if (overflowS) val = 0x7FFFFFFF + (val >> 63);
                break;
            case 6:          // unsigned integer overflow gives zero
                if (overflowU) val = 0;
                break;
            case 7:          // unsigned integer overflow gives unsigned saturation
                if (overflowU) val = 0xFFFFFFFF;
                break;
            }
            *(uint32_t*)(destination + pos) = (uint32_t)val; // store value
        }
        t->returnType = 0x112;            
        break;
    case 4:   // int128 -> int64
        for (pos = 0; pos < newLength; pos += 8) {
            uint64_t valLo = *(uint64_t*)(source + 2*pos);      // value to convert, low part
            uint64_t valHi = *(uint64_t*)(source + 2*pos + 8);  // value to convert, high part
            overflowU = valHi != 0;                             // unsigned overflow
            if ((int64_t)valLo < 0) overflowS = valHi+1 != 0;   // signed overflow
            else overflowS = valHi != 0;
            overflowU2 |= overflowU;  overflowS2 |= overflowS;
            switch (IM1 & 7) {
            case 0: default: // wrap around
                break;
            case 4:          // signed integer overflow gives zero
                if (overflowS) valLo = 0;
                break;
            case 5:          // signed integer overflow gives signed saturation
                if (overflowS) valLo = nsign_d + (valHi >> 63);
                break;
            case 6:          // unsigned integer overflow gives zero
                if (overflowU) valHi = valLo = 0;
                break;
            case 7:          // unsigned integer overflow gives unsigned saturation
                if (overflowU) valLo = 0xFFFFFFFFFFFFFFFF;
                break;
            }
        }
        t->returnType = 0x113;            
        break;
    case 5:   // float -> float16
        for (pos = 0; pos < newLength; pos += 2) {
            SNum val;
            val.i = *(uint32_t*)(source + 2 * pos);        // value to convert
            uint16_t val2 = float2half(val.f);             // convert to half precision
            if (!isnan_or_inf_f(val.i)) {
                // check rounding mode
                switch (roundingMode) {
                case 1:          // odd if not exact
                    if (half2float(val2) != val.f) val2 |= 1;
                    break;
                case 4: default: // nearest or even
                    break;
                case 5:          // down
                    if (half2float(val2) > val.f) {
                        if (val2 << 1 == 0) val2 = 0x8001; // 0 -> subnormal negative
                        else if (int16_t(val2) > 0) val2--;
                        else val2++;
                    }
                    break;
                case 6:          // up
                    if (half2float(val2) < val.f) {
                        if (val2 << 1 == 0) val2 = 0x0001; // 0 -> subnormal positive
                        else if (int16_t(val2) > 0) val2++;
                        else val2--;
                    }
                    break;
                case 7:          // towards zero
                    if (half2float(val2) != val.f && (val2 << 1 != 0)) {
                        val2--;
                    }
                    break;
                }
                // check overflow
                overflowS = (val2 & 0x7FFF) == 0x7C00 && !isinf_f(val.i);// detect overflow
                overflowF2 |= overflowS;
                if (overflowS) {                               // check for overflow
                    if (exceptionControl & 1) {                // overflow exception -> NAN
                        val2 = (uint16_t)t->makeNan(nan_overflow_conv, 1);  // overflow
                    }
                }
                else if ((exceptionControl & 6) && val2 << 1 == 0 && val.f != 0.f) {
                    val2 = (uint16_t)t->makeNan(nan_underflow, 1); // underflow exception (inexact implies underflow)
                }
                else if ((exceptionControl & 4) && half2float(val2) != val.f) {
                    val2 = (uint16_t)t->makeNan(nan_inexact, 1);   // inexact exception
                }
            }
            *(uint16_t*)(destination + pos) = val2;        // store value
        } 
        t->returnType = 0x118;
        break;
    case 6:   // double -> float
        for (pos = 0; pos < newLength; pos += 4) {
            SNum val1, val2;
            val1.q = *(uint64_t*)(source + 2 * pos);       // value to convert
            // check NAN and INF
            if (isnan_or_inf_d(val1.q)) {
                union {                                    // single precision float
                    float f;
                    struct {                               // structure of a NAN
                        uint32_t payload : 22;
                        uint32_t quiet : 1;
                        uint32_t expo : 8;
                        uint32_t sign : 1;
                    };
                } u;
                u.payload = val1.i & (1 << 22) - 1;        // ForwardCom has right-justified NAN payload, unlike other binary systems
                u.quiet = val1.q >> 51 & 1;
                u.expo = 0xFF;
                u.sign = val1.q >> 63 & 1;
                val2.f = u.f;
            }
            else {
                val2.f = float(val1.d);                    // convert to single precision
                // check rounding mode
                uint8_t roundingMode = (IM1 >> 3) & 7;
                if (roundingMode == 0) roundingMode = ((t->parm[3].i >> MSKI_ROUNDING) & 7) | 4;
                switch (roundingMode) {
                case 1:          // odd if not exact
                    if (val2.f != val1.d) {
                        val2.i |= 1;
                    }
                    break;
                case 4: default: // nearest or even
                    break;
                case 5:          // down
                    if (val2.f > val1.d) {
                        if (val2.f == 0.f) val2.i = 0x80000001; // 0 -> subnormal negative
                        else if (val2.i > 0) val2.i--;
                        else val2.i++;
                    }
                    break;
                case 6:          // up
                    if (val2.f < val1.d) {
                        if (val2.f == 0.f) val2.i = 0x00000001; // 0 -> subnormal positive
                        else if (val2.i > 0) val2.i++;
                        else val2.i--;
                    }
                    break;
                case 7:         // towards zero
                    if (val2.f != val1.d && val2.f != 0.f) {
                        val2.i--;
                    }
                    break;
                }
                // check overflow
                overflowS = isinf_f(val2.i) && !isinf_d(val1.q); // detect overflow
                overflowF2 |= overflowS;
                if (overflowS) {                               // check for overflow
                    if (exceptionControl & 1) {                // overflow exception -> NAN
                        val2.q = t->makeNan(nan_overflow_conv, 5);  // overflow
                    }
                }
                else if ((exceptionControl & 6) && val2.f == 0.f && val1.d != 0.) {
                    val2.q = t->makeNan(nan_underflow, 5);     // underflow exception
                }
                else if ((exceptionControl & 4) && val2.f != val1.d) {
                    val2.q = t->makeNan(nan_inexact, 5);       // inexact exception
                }
            }
            *(uint32_t*)(destination + pos) = val2.i;          // store value
        }
        t->returnType = 0x115;
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
    }
    // check overflow traps
    /*
    if (mask.i & MSK_OVERFL_ALL) {
        if      ((mask.i & MSK_OVERFL_SIGN)   && overflowS2) t->interrupt(INT_OVERFL_SIGN);   // signed overflow
        else if ((mask.i & MSK_OVERFL_UNSIGN) && overflowU2) t->interrupt(INT_OVERFL_UNSIGN); // unsigned overflow
        else if ((mask.i & MSK_OVERFL_FLOAT)  && overflowF2) t->interrupt(INT_OVERFL_FLOAT);  // float overflow
    } */
    t->vectorLength[rd] = newLength;             // save new vector length
    t->vect = 4;                                 // stop vector loop
    t->running = 2;                              // don't save. result has already been saved
    return 0;
}

static uint64_t expand(CThread * t) {
    // Expand vector RS to a vector of the double length and the double element size.
    // OT specifies the element size or precision of the destination.
    // Half precision -> single precision, 32-bit integer -> 64-bit integer, etc.
    
    // Operands:
    uint8_t  rd = t->operands[0];
    uint8_t  rs = t->operands[4];
    uint8_t IM1 = t->parm[4].b;
    if (IM1 & 0xFC) t->interrupt(INT_INST_ILLEGAL);
    bool signExtend = (IM1 & 2) == 0;

    uint32_t initLength = t->vectorLength[rs];
    uint32_t newLength = 2 * initLength;
    if (newLength > t->MaxVectorLength) newLength = t->MaxVectorLength;
    // uint32_t oldLength = newLength / 2;
    uint32_t pos;                                // position in source vector
    int8_t * source = t->vectors.buf() + (uint32_t)rs * t->MaxVectorLength;      // address of RT data
    int8_t * destination = t->vectors.buf() + (uint32_t)rd * t->MaxVectorLength; // address of RD data
    if (rd == rs) {
        // source and destination are the same. Make a temporary copy of source to avoid overwriting
        memcpy(t->tempBuffer, source, initLength);
        source = t->tempBuffer;    
    }
    switch (t->operandType) {
    case 0:   // int4 -> int8
        for (pos = 0; pos < newLength; pos += 1) {
            uint8_t val1 = *(uint8_t*)(source + pos);  // values to convert
            union {
                uint16_t s;
                uint8_t b[2];
                int8_t bs[2];
            } val2;
            if (signExtend) {
                val2.bs[0] = (int8_t)val1 << 4 >> 4;   // sign extend
                val2.bs[1] = (int8_t)val1 >> 4;        // sign extend
            }
            else {
                val2.b[0] = val1 & 0xF;                // zero extend
                val2.b[1] = val1 >> 4;                 // zero extend
            }
            *(uint16_t*)(destination + pos*2) = val2.s;         // store value
        }
        break;
    case 1:   // int8 -> int16
        for (pos = 0; pos < newLength; pos += 1) {
            uint16_t val = *(uint8_t*)(source + pos);  // value to convert
            if (signExtend) val = uint16_t((int16_t)(val << 8) >> 8);   // sign extend
            *(uint16_t*)(destination + pos*2) = val; // store value
        }
        break;
    case 2:   // int16 -> int32
        for (pos = 0; pos < newLength; pos += 2) {
            uint32_t val = *(uint16_t*)(source + pos);  // value to convert
            if (signExtend) val = uint32_t((int32_t)(val << 16) >> 16);   // sign extend
            *(uint32_t*)(destination + pos*2) = val; // store value
        }
        break;
    case 3:   // int32 -> int64
        for (pos = 0; pos < newLength; pos += 4) {
            uint64_t val = *(uint32_t*)(source + pos);  // value to convert
            if (signExtend) val = uint64_t((int64_t)(val << 32) >> 32);   // sign extend
            *(uint64_t*)(destination + pos*2) = val; // store value
        }
        break;
    case 4:   // int64 -> int128
        for (pos = 0; pos < newLength; pos += 8) {
            uint64_t valLo = *(uint64_t*)(source + pos);   // value to convert
            uint64_t valHi = 0;
            if (signExtend) valHi = uint64_t((int64_t)valLo >> 63);   // sign extend
            *(uint64_t*)(destination + pos*2) = valLo;     // store low part
            *(uint64_t*)(destination + pos*2 + 8) = valHi; // store high part
        }
        break;
    case 5:   // float16 -> float
        for (pos = 0; pos < newLength; pos += 2) {
            uint16_t val1 = *(uint16_t*)(source + pos);    // value to convert
            float val2 = half2float(val1);                 // convert half precision to float
            *(float*)(destination + pos*2) = val2;         // store value
        }
        break;
    case 6:   // float -> double
        for (pos = 0; pos < newLength; pos += 4) {
            SNum val1;
            val1.i = *(uint32_t*)(source + pos);           // value to convert
            double val2 = val1.f;                          // convert to double precision
            // check NAN
            // ForwardCom has right-justified NAN payload, unlike other binary systems
            if (isnan_f(val1.i)) {
                union {                                    // single precision float
                    double d;
                    struct {                               // structure of a NAN
                        uint64_t payload : 51;
                        uint64_t quiet   : 1;
                        uint64_t expo    : 11;
                        uint64_t sign    : 1;
                    };
                } u;
                u.payload = val1.q & (1 << 22) - 1;
                u.quiet = val1.i >> 22 & 1;
                u.expo = 0x7FF;
                u.sign = val1.q >> 63 & 1;
                val2 = u.d;
            }
            *(double*)(destination + pos*2) = val2;        // store value
        }
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
    }
    t->vectorLength[rd] = newLength;                       // save new vector length
    t->vect = 4;                                           // stop vector loop
    t->running = 2;                                        // don't save. result has already been saved
    return 0;
}

static uint64_t float2int (CThread * t) {
    // Conversion of floating point to signed or unsigned integer with the same operand size. 
    // The rounding mode and overflow control is specified in IM1.
    SNum a = t->parm[1];
    SNum b = t->parm[4];
    int64_t result = 0;
    uint32_t dataSize = dataSizeTable[t->operandType];
    uint8_t roundingMode = b.b >> 3 & 3;
    uint8_t signMode = roundingMode | (b.b & 2) << 1; // bit 0-1: rounding mode, bit 2: usigned
    bool overflow = false;
    bool invalid = false; 

    if (dataSize == 2) {  // float16 -> int16
        const float max = (float)(int32_t)0x7FFF;
        const float min = -max - 1.0f;
        const float umax = (float)(uint32_t)0xFFFFu;
        if (isnan_h(a.s)) {
            invalid = true;
        }
        else {
            float f = half2float(a.s);
            switch (signMode) { // rounding mode:
            case 0: // nearest or even
                if (f >= max + 0.5f || f < min - 0.5f) overflow = true;
                result = (int)(nearbyint(f));
                break;
            case 1: // down 
                if (f >= max + 1.0f || f <= min) overflow = true;
                result = (int)(floor(f));
                break;
            case 2: // up
                if (f > max || f <= min - 1.0f) overflow = true;
                result = (int)(ceil(f));
                break;
            case 3: // towards zero
                if (f >= max + 1.0f || f <= min - 1.0f) overflow = true;
                result = (int)(f);
                break;
            case 4: // unsigned nearest or even
                if (f >= umax + 0.5f || f < - 0.5f) overflow = true;
                result = (int)(nearbyint(f));
                break;
            case 5: case 7: // unsigned down
                if (f >= umax + 1.0f || f < 0.0f) overflow = true;
                result = (int)(floor(f));
                break;
            case 6: // unsigned up
                if (f > umax || f <= -1.0f) overflow = true;
                else result = (int)(ceil(f));
            }
            if (overflow) {
                switch (b.b & 7) { // overflow options
                case 0: default: // wrap around
                    result &= 0xFFFFu;
                    break;
                case 4: case 6:
                    result = 0;
                    break;
                case 5: // signed saturation
                    result = 0x7FFF + int(f < 0);
                    break;
                case 7: // unsigned saturation
                    result = 0xFFFFu;
                    break;
                }
            }
            if (invalid) {
                result = (b.b & 0x20) ? 0x8000u : 0;
            }
        }
    }
    else if (dataSize == 4) {  // float -> int32
        const float max = (float)(int32_t)nsign_f;
        const float min = -max - 1.0f;
        const float umax = (float)(uint32_t)0xFFFFFFFFu;
        if (isnan_f(a.i)) {
            invalid = true;
        }
        else {
            switch (signMode) { // rounding mode:
            case 0: // nearest or even
                if (a.f >= max + 0.5f || a.f < min - 0.5f) overflow = true;
                result = (int64_t)(nearbyint(a.f));
                break;
            case 1: // down 
                if (a.f >= max + 1.0f || a.f <= min) overflow = true;
                result = (int64_t)(floor(a.f));
                break;
            case 2: // up
                if (a.f > max || a.f <= min - 1.0f) overflow = true;
                result = (int64_t)(ceil(a.f));
                break;
            case 3: // towards zero
                if (a.f >= max + 1.0f || a.f <= min - 1.0f) overflow = true;
                result = (int64_t)(a.f);
                break;
            case 4: // unsigned nearest or even
                if (a.f >= umax + 0.5f || a.f < - 0.5f) overflow = true;
                result = (int64_t)(nearbyint(a.f));
                break;
            case 5: case 7: // unsigned down
                if (a.f >= umax + 1.0f || a.f < 0.0f) overflow = true;
                result = (int64_t)(floor(a.f));
                break;
            case 6: // unsigned up
                if (a.f > umax || a.f <= -1.0f) overflow = true;
                else result = (int64_t)(ceil(a.f));
            }
            if (overflow) {
                switch (b.b & 7) { // overflow options
                case 0:  // wrap around
                    result &= 0xFFFFFFFFu;
                    break;
                case 4: case 6:
                    result = 0;
                    break;
                case 5: // signed saturation
                    result = 0x7FFFFFFF + int(a.f < 0);
                    break;
                case 7: // unsigned saturation
                    result = 0xFFFFFFFFu;
                    break;
                }
            }
            if (invalid) {
                result = (b.b & 0x20) ? sign_f : 0;
            }
        }
    }
    else if (dataSize == 8) {   // double -> int64
        const double max = (double)(int64_t)nsign_d;
        const double min = -max - 1.0f;
        const double umax = (double)0xFFFFFFFFFFFFFFFFu;
        if (isnan_d(a.q)) {
            invalid = true;
        }
        else {
            switch (signMode) { // rounding mode:
            case 0: // nearest or even
                if (a.d >= max + 0.5 || a.d < min - 0.5) overflow = true;
                result = (int64_t)(nearbyint(a.d));
                break;
            case 1: // down 
                if (a.d >= max + 1.0 || a.d <= min) overflow = true;
                result = (int64_t)(floor(a.d));
                break;
            case 2: // up
                if (a.d > max || a.d <= min - 1.0) overflow = true;
                result = (int64_t)(ceil(a.d));
                break;
            case 3: // towards zero
                if (a.d >= max + 1.0 || a.d <= min - 1.0) overflow = true;
                result = (int64_t)(a.d);
                break; 
            case 4: // unsigned nearest or even
                if (a.d >= umax + 0.5 || a.d < - 0.5) overflow = true;
                result = (uint64_t)(nearbyint(a.d));
                break;
            case 5: case 7: // unsigned down
                if (a.d >= umax + 1.0 || a.d < 0.0) overflow = true;
                result = (uint64_t)(floor(a.d));
                break;
            case 6: // unsigned up
                if (a.d > umax || a.d <= -1.0) overflow = true;
                result = (uint64_t)(ceil(a.d));
            }
        }
        if (overflow) {
            switch (b.b & 7) { // overflow options
            case 0:  // wrap around
                break;
            case 4: case 6:
                result = 0;
                break;
            case 5: // signed saturation
                result = nsign_d + int(a.d < 0);
                break;
            case 7: // unsigned saturation
                result = 0xFFFFFFFFFFFFFFFFu;
                break;
            }
        }
        if (invalid) {
            result = (b.b & 0x20) ? sign_d : 0;
        }
    }
    else t->interrupt(INT_INST_ILLEGAL);
    /* Traps not supported
    if (overflow && (mask.i & MSK_OVERFL_SIGN)) {
        t->interrupt(INT_OVERFL_SIGN);  // signed overflow
        result = dataSizeMask[t->operandType] >> 1; // INT_MAX
    }
    if (invalid && (mask.i & MSK_FLOAT_NAN_LOSS)) {
        t->interrupt(INT_FLOAT_NAN_LOSS);  // nan converted to integer
        result = dataSizeMask[t->operandType] >> 1; // INT_MAX
    } */
    if ((t->operandType & 7) >= 5) t->operandType -= 3;    // debug return type is integer
    return result;
}

static uint64_t int2float (CThread * t) {
    //  Conversion of signed or unsigned integer to floating point with same operand size.
    SNum a = t->parm[1];
    SNum IM1 = t->parm[4];
    bool isSigned = (IM1.b & 1) == 0;  // signed integer
    bool inexactX = (IM1.b & 4) != 0;  // make NAN exception if inexact

    SNum result;
    uint32_t dataSize = dataSizeTable[t->operandType];
    switch (dataSize) {
    case 2:  // int16 -> float16
        if (isSigned) {
            result.s = float2half(float(a.ss));
            if (inexactX && int32_t(half2float(result.s)) != a.ss) {
                result.q = t->makeNan(nan_inexact, 1);
            } 
        }
        else { // unsigned
            result.s = float2half(float(a.s));
            if (inexactX && uint32_t(half2float(result.s)) != a.s) {
                result.q = t->makeNan(nan_inexact, 1);
            } 
        }
        t->returnType = 0x118;         // debug return type is float16
        break;

    case 4: // int32 -> float
        if (isSigned) {
            result.f = (float)a.is;
            if (inexactX && int32_t(result.f) != a.is) {
                result.q = t->makeNan(nan_inexact, 5);
            } 
        }
        else {
            result.f = (float)a.i;
            if (inexactX && uint32_t(result.f) != a.i) {
                result.q = t->makeNan(nan_inexact, 5);
            } 
        }
        t->returnType = 0x115;        // debug return type is float
        break;

    case 8:  // int64 -> double
        if (isSigned) {
            result.d = (double)a.qs;
            if (inexactX && int64_t(result.d) != a.qs) {
                result.q = t->makeNan(nan_inexact, 6);
            }
        }
        else {
            result.d = (double)a.q;
            if (inexactX && uint64_t(result.d) != a.q) {
                result.q = t->makeNan(nan_inexact, 6);
            }
        }
        t->returnType = 0x116;        // debug return type is double
        break;

    default: 
        t->interrupt(INT_INST_ILLEGAL);
        result.q = 0;
    }
    return result.q;
}

static uint64_t round_ (CThread * t) {
    // Round floating point to integer in floating point representation. 
    // The rounding mode is specified in IM1.
    // Conversion of floating point to signed integer with the same operand size. 
    // The rounding mode is specified in IM1.
    SNum a = t->parm[1];
    SNum b = t->parm[4];
    SNum result;
    uint32_t dataSize = dataSizeTable[t->operandType];
    if (dataSize == 4) {  // float -> int32
        switch (b.b) { // rounding mode:
        case 0: // nearest or even
            result.f = nearbyintf(a.f);
            break;
        case 1: // down 
            result.f = floorf(a.f);
            break;
        case 2: // up
            result.f = ceilf(a.f);
            break;
        case 3: // towards zero
            result.f = truncf(a.f);
            break;
        default: t->interrupt(INT_INST_ILLEGAL);
        }
    }
    else if (dataSize == 8) {   // double -> int64
        switch (b.b) { // rounding mode:
        case 0: // nearest or even
            result.d = nearbyint(a.d);
            break;
        case 1: // down 
            result.d = floor(a.d);
            break;
        case 2: // up
            result.d = ceil(a.d);
            break;
        case 3: // towards zero
            result.d = trunc(a.d);
            break;
        default: t->interrupt(INT_INST_ILLEGAL);
        }
    }
    return result.q;
}

static uint64_t round2n (CThread * t) {
    // Round to nearest multiple of 2n.
    // RD = 2^n * round(2^(−n)*RS). 
    // n is a signed integer constant in IM1
    SNum b = t->parm[4];                    // n
    SNum mask = t->parm[3];
    uint32_t exponent1;
    uint64_t result = 0;
    if (t->operandType == 5) {  // float
        union {
            uint32_t i;
            float f;
            struct {
                uint32_t mantissa : 23;
                uint32_t exponent : 8;
                uint32_t sign     : 1;
            };
        } u;
        u.i = t->parm[1].i;             // input a
        if (isnan_f(u.i)) return u.i;   // a is nan
        exponent1 = u.exponent;
        if (exponent1 == 0) {
            u.mantissa = 0;                // a is zero or subnormal. return zero
            return u.i; 
        }
        exponent1 -= b.i;                  // subtract b from exponent
        if ((int32_t)exponent1 <= 0) { // underflow
            //if (mask.i & MSK_FLOAT_UNDERFL) t->interrupt(INT_FLOAT_UNDERFL);
            return 0;
        }
        else if ((int32_t)exponent1 >= 0xFF) { // overflow
            //if (mask.i & MSK_OVERFL_FLOAT) t->interrupt(INT_OVERFL_FLOAT);
            return inf_f;
        }
        u.exponent = exponent1;
        u.f = nearbyintf(u.f);   // round
        if (u.f != 0) u.exponent += b.i;              // add b to exponent
        result = u.i;
    }
    else if (t->operandType == 6) {   // double
        union {
            uint64_t q;
            double d;
            struct {
                uint64_t mantissa : 52;
                uint64_t exponent : 11;
                uint64_t sign     :  1;
            };
        } u;
        u.q = t->parm[1].q;             // input a
        if (isnan_d(u.q)) return u.q;   // a is nan
        exponent1 = u.exponent;
        if (exponent1 == 0) {
            u.mantissa = 0;                // a is zero or subnormal. return zero
            return u.q; 
        }
        exponent1 -= b.i;                  // subtract b from exponent
        if ((int32_t)exponent1 <= 0) { // underflow
            //if (mask.i & MSK_FLOAT_UNDERFL) t->interrupt(INT_FLOAT_UNDERFL);
            return 0;
        }
        else if ((int32_t)exponent1 >= 0x7FF) { // overflow
            //if (mask.i & MSK_OVERFL_FLOAT) t->interrupt(INT_OVERFL_FLOAT);
            return inf_d;
        }
        u.exponent = exponent1;
        u.d = nearbyint(u.d);   // round
        if (u.d != 0) u.exponent += b.i;              // add b to exponent
        result = u.q;
    }
    else t->interrupt(INT_INST_ILLEGAL);
    return result;
}

static uint64_t abs_ (CThread * t) {
    // Absolute value of integer. 
    // IM1 determines handling of overflow: 0: wrap around, 1: saturate, 2: zero, 3: trap
    SNum a = t->parm[1];                                   // x
    SNum b = t->parm[4];                                   // option
    uint64_t sizemask = dataSizeMask[t->operandType];      // mask for operand size
    uint64_t signbit = (sizemask >> 1) + 1;                // just the sign bit
    if (a.q & signbit) {
        // a is negative
        if (t->operandType > 4) {                          // floating point types
            return a.q & ~signbit;                         // just remove sign bit
        }
        if ((a.q & sizemask) == signbit) {
            // overflow
            switch (b.b & ~4) {
            case 0:  // wrap around
                break;
            case 1:  // saturate
                return a.q - 1;
            case 2:  // zero
                return 0;
            default:
                t->interrupt(INT_INST_ILLEGAL);
            }
            if ((b.b & 4) /* && (t->parm[3].i & MSK_OVERFL_SIGN)*/) { // trap
                t->interrupt(INT_OVERFL_SIGN);  // signed overflow
            }
        }
        a.qs = - a.qs;                           // change sign
    }
    return a.q;
}

static uint64_t fp_category (CThread * t) {
    // Check if floating point numbers belong to the categories indicated by constant
    //  0 ± NAN, 1 ± Zero, 2 −Subnormal, 3 +Subnormal, 4 −Normal, 5 +Normal, 6 −Infinite, 7 +Infinite
    SNum a = t->parm[1];                         // x
    SNum b = t->parm[4];                         // option
    uint32_t exponent; 
    uint8_t category = 0;                        // detected category bits
    switch (t->operandType) {
    case 2: case 5:                              // float
        exponent = a.i >> 23 & 0xFF;             // isolate exponent
        if (exponent == 0xFF) {                  // nan or inf
            if (a.i << 9) category = 1;          // nan
            else if (a.i >> 31) category = 0x40; // -inf
            else category = 0x80;                // + inf
        }
        else if (exponent == 0) { 
            if ((a.i << 9) == 0) category = 2;   // zero
            else if (a.i >> 31)  category = 4;   // - subnormal
            else category = 8;                   // + subnormal
        }
        else if (a.i >> 31) category = 0x10;     // - normal    
        else category = 0x20;                    // + normal
        break;
    case 3: case 6:                              // double
        exponent = a.q >> 52 & 0x7FF;            // isolate exponent
        if (exponent == 0x7FF) {                 // nan or inf
            if (a.q << 12) category = 1;         // nan
            else if (a.q >> 63) category = 0x40; // -inf
            else category = 0x80;                // + inf
        }
        else if (exponent == 0) { 
            if ((a.q << 12) == 0) category = 2;  // zero
            else if (a.q >> 63)  category = 4;   // - subnormal
            else category = 8;                   // + subnormal
        }
        else if (a.q >> 63) category = 0x10;     // - normal    
        else category = 0x20;                    // + normal
        break;
    default:    
        t->interrupt(INT_INST_ILLEGAL);
    }
    uint8_t result = (category & b.b) != 0;                // test if a belongs to any of the indicated categories
    if ((t->operandType & 7) >= 5) t->operandType -= 3;    // debug return type is integer
    return (t->numContr & ~(uint64_t)1) | result;          // get remaining bits from NUMCONTR
}

static uint64_t broad_ (CThread * t) {
    // 18: Broadcast 8-bit signed constant into all elements of RD with length RS (31 in RS field gives scalar output).
    // 19: broadcast_max. Broadcast 8-bit constant into all elements of RD with maximum vector length.
    uint8_t  rd = t->operands[0];
    uint8_t  rs = t->operands[4];
    uint8_t  rm = t->operands[1];                // mask register
    SNum b = t->parm[2];                         // constant
    uint64_t length;                             // length of destination vector
    if (t->op == 18) {                           // length given by RS
        length = t->registers[rs];
        if (length > t->MaxVectorLength) length = t->MaxVectorLength;
    }
    else {                                       // length is maximum
        length = t->MaxVectorLength;
    }
    uint8_t  dsizelog = dataSizeTableLog[t->operandType]; // log2(elementsize)
    length = length >> dsizelog << dsizelog;     // round down to nearest multiple of operand size
    // set length of destination vector
    t->vectorLength[rd] = (uint32_t)length;
    // loop to set all elements
    for (uint32_t pos = 0; pos < (uint32_t)length; pos += 1 << dsizelog) {
        if ((rm & 0x1F) != 0x1F && !(t->readVectorElement(rm, pos) & 1)) { // mask is zero. get fallback
            if (t->op == 18 || rs >= 31) b.q = 0;   // threre is no fallback. write zero
            else b.q = t->readVectorElement(rs, pos);  // rs is fallback
        }
        t->writeVectorElement(rd, b.q, pos);     // write vector element
    }
    t->vect = 4;                                 // stop vector loop
    t->running = 2;                              // don't save RD
    return 0;
}

static uint32_t byteSwap(uint32_t x) {      // swap bytes, used by byte_reverse function
    union {
        uint32_t i;
        uint8_t b[4];
    } a, b;
    a.i = x;
    b.b[0] = a.b[3]; b.b[1] = a.b[2]; b.b[2] = a.b[1]; b.b[3] = a.b[0];
    return b.i;
}

static uint8_t bitSwap(uint8_t x) {         // swap bits, used by bit_reverse function
    x = x >> 4 | x << 4;                    // swap 4-bit nipples
    x = (x >> 2 & 0x33) | (x << 2 & 0xCC);  // swap 2-bit groups
    x = (x >> 1 & 0x55) | (x << 1 & 0xAA);  // swap single bits
    return x;
}

static uint64_t byte_reverse (CThread * t) {
    // Reverse the order of bits or bytes in each element of vector
    SNum a = t->parm[1];                         // value
    uint8_t IM1 = t->parm[2].b;                  // immediate operand
    if (IM1 & 1) {
        // bit reverse: Reverse the order of bits in each element of vector
        union {
            uint64_t q;
            uint32_t i[2];
            uint8_t  b[8];
        } u;
        u.q = a.q;
        uint8_t t1;  uint32_t t2;
        switch (dataSizeTableLog[t->operandType]) {
        case 0:  // 8 bit
            u.b[0] = bitSwap(u.b[0]); break;
        case 1:  // 16 bit
            t1 = bitSwap(u.b[0]); u.b[0] = bitSwap(u.b[1]); u.b[1] = t1; break;
        case 2:  // 32 bit
            u.i[0] = byteSwap(u.i[0]);
            for (t1 = 0; t1 < 4; t1++) u.b[t1] = bitSwap(u.b[t1]);
            break;
        case 3:  // 64 bit
            t2 = byteSwap(u.i[0]); u.i[0] = byteSwap(u.i[1]); u.i[1] = t2;
            for (t1 = 0; t1 < 8; t1++) u.b[t1] = bitSwap(u.b[t1]);
            break;
        case 4:  // 128 bit
            t->interrupt(INT_INST_ILLEGAL);
        }
        return u.q;
    }
    else {
        // byte reverse: Reverse the order of bytes in each element of a vector
        uint8_t  rs = t->operands[4];
        uint32_t tmp;
        switch (dataSizeTableLog[t->operandType]) {
        case 0:  // 8 bit
            break;
        case 1:  // 16 bit
            a.s = a.s >> 8 | a.b << 8;  break;  // swap bytes
        case 2:  // 32 bit
            a.i = byteSwap(a.i); break;
        case 3:  // 64 bit
            tmp = byteSwap(a.i); a.q = byteSwap(a.q >> 32) | (uint64_t)tmp << 32;
            break;
        case 4:  // 128 bit
            tmp = byteSwap(a.i); t->parm[5].q = byteSwap(a.q >> 32) | (uint64_t)tmp << 32;
            a.q = t->readVectorElement(rs, t->vectorOffset + 8); // high part of input
            tmp = byteSwap(a.i); a.q = byteSwap(a.q >> 32) | (uint64_t)tmp << 32;
            break;
        }
        return a.q;
    }
}


static uint64_t truth_tab2 (CThread * t) {
    // Boolean function of two inputs, given by a truth table
    SNum a = t->parm[0];                         // value
    SNum b = t->parm[1];                         // value
    SNum c = t->parm[4];                         // truth table
    return ((c.b >> ((a.b & 1) | (b.b & 1) << 1)) & 1) | (a.q & ~uint64_t(1));
}

// Format 1.3 C. One vector register and a broadcast 16-bit immediate operand.

static uint64_t move_i16 (CThread * t) {
    // Move 16 bit integer constant to 16-bit scalar
    uint8_t rd = t->operands[0];                 // destination vector
    t->vectorLength[rd] = 2;                     // set length of destination
    t->vect = 4;                                 // stop vector loop
    return t->parm[2].q;
}

//static uint64_t add_i16 (CThread * t) {return f_add(t);} // Add broadcasted 16 bit constant to 16-bit vector elements

static uint64_t and_i16 (CThread * t) {
    // AND broadcasted 16 bit constant
    return t->parm[1].q & t->parm[2].q;
}

static uint64_t or_i16 (CThread * t) {
    // OR broadcasted 16 bit constant
    return t->parm[1].q | t->parm[2].q;
}

static uint64_t xor_i16 (CThread * t) {
    // XOR broadcasted 16 bit constant
    return t->parm[1].q ^ t->parm[2].q;
}

static uint64_t add_h16 (CThread * t) {
    // add constant to half precision vector
    return f_add_h(t);
}

static uint64_t mul_h16 (CThread * t) {
    // multiply half precision vector with constant
    return f_mul_h(t);
}

static uint64_t move_8shift8 (CThread * t) {
    // RD = IM2 << IM1. Sign-extend IM2 and shift left by the unsigned value IM1 to make 32/64 bit scalar 
    // 40: 32 bit, 41: 64 bit
    uint8_t rd = t->operands[0];                 // destination vector
    t->vectorLength[rd] = (t->op & 1) ? 8 : 4;   // set length of destination
    t->vect = 4;                                 // stop vector loop
    return (uint64_t)(int64_t(t->parm[2].ss) >> 8 << t->parm[2].bs);  // shift and sign extend
}
        
static uint64_t add_8shift8 (CThread * t) {
    // RD += IM2 << IM1. Sign-extend IM2 and shift left by the unsigned value IM1, add to 32/64 bit vector
    // 42: 32 bit, 43: 64 bit
    int64_t save2 = t->parm[2].qs;
    t->parm[2].qs = int64_t(t->parm[2].ss) >> 8 << t->parm[2].bs;  // shift and sign extend
    int64_t result = f_add(t);                             // use f_add for getting overflow traps
    t->parm[2].qs = save2;                                 // restore constant
    return result;
}

static uint64_t and_8shift8 (CThread * t) {
    // RD &= IM2 << IM1. Sign-extend IM2 and shift left by the unsigned value IM1, AND with 32/64 bit vector
    // 44: 32 bit, 45: 64 bit
    int64_t a = int64_t(t->parm[2].ss) >> 8 << t->parm[2].bs;  // shift and sign extend
    return t->parm[1].q & a;
}

static uint64_t or_8shift8 (CThread * t) {
    // RD |= IM2 << IM1. Sign-extend IM2 and shift left by the unsigned value IM1, OR with 32/64 bit vector
    // 46: 32 bit, 47: 64 bit
    int64_t a = int64_t(t->parm[2].ss) >> 8 << t->parm[2].bs;  // shift and sign extend
    return t->parm[1].q | a;
}

static uint64_t xor_8shift8 (CThread * t) {
    // RD |= IM2 << IM1. Sign-extend IM2 and shift left by the unsigned value IM1, XOR with 32/64 bit vector
    // 48: 32 bit, 49: 64 bit
    int64_t a = int64_t(t->parm[2].ss) >> 8 << t->parm[2].bs;  // shift and sign extend
    return t->parm[1].q ^ a;
}

static uint64_t move_half2float (CThread * t) {
    // Move converted half precision floating point constant to single precision scalar
    t->vectorLength[t->operands[0]] = 4;         // set length of destination
    t->vectorLengthR = 4;
    t->vect = 4;                                 // stop vector loop
    return t->parm[2].q;
}

static uint64_t move_half2double (CThread * t) {
    // Move converted half precision floating point constant to double precision scalar
    t->vectorLength[t->operands[0]] = 8;         // set length of destination
    t->vect = 4;                                 // stop vector loop
    return t->parm[2].q;
}

static uint64_t add_half2float (CThread * t) {
    // Add broadcast half precision floating point constant to single precision vector
    return f_add(t);
}

static uint64_t add_half2double (CThread * t) {
    // Add broadcast half precision floating point constant to double precision vector
    return f_add(t);
}

static uint64_t mul_half2float (CThread * t) {
    // multiply broadcast half precision floating point constant with single precision vector
    return f_mul(t);
}

static uint64_t mul_half2double (CThread * t) {
    // multiply broadcast half precision floating point constant with double precision vector
    return f_mul(t);
}

// Format 2.6 A. Three vector registers and a 32-bit immediate operand.

static uint64_t load_hi (CThread * t) {
    // Make vector of two elements. dest[0] = 0, dest[1] = IM2.
    uint8_t rd = t->operands[0];
    uint8_t dsize = dataSizeTable[t->operandType];
    t->vectorLength[rd] = dsize * 2;             // set length of destination
    t->writeVectorElement(rd, 0, 0);             // write 0
    t->writeVectorElement(rd, t->parm[2].q, dsize);// write IM2
    t->vect = 4;                                 // stop vector loop
    t->running = 2;                              // don't save RD
    return 0;
}

static uint64_t insert_hi (CThread * t) {
    // Make vector of two elements. dest[0] = src1[0], dest[1] = IM2.
    uint8_t rd = t->operands[0];
    uint8_t dsize = dataSizeTable[t->operandType];
    t->vectorLength[rd] = dsize * 2;             // set length of destination
    t->writeVectorElement(rd, t->parm[1].q, 0);  // write src1
    t->writeVectorElement(rd, t->parm[2].q, dsize);// write IM2
    t->vect = 4;                                 // stop vector loop
    t->running = 2;                              // don't save RD
    return 0;
}

static uint64_t make_mask (CThread * t) {
    // Make vector where bit 0 of each element comes from bits in IM2, the remaining bits come from RT.
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    uint8_t  dsizelog = dataSizeTableLog[t->operandType];  // log2(elementsize)
    uint32_t elementNum = t->vectorOffset >> dsizelog;     // index to vector element
    if ((t->operandType & 7) >= 5) t->operandType -= 3;    // debug return type is integer
    return (a.q & ~(uint64_t)1) | (b.i >> (elementNum & 31) & 1);
}

static uint64_t replace_ (CThread * t) {
    // Replace elements in RT by constant IM2
    // format 2.6: 32 bits, format 3.1: 64 bits
    return t->parm[2].q;
}

static uint64_t replace_even (CThread * t) {
    // Replace even-numbered elements in RT by constant IM2
    uint8_t  dsizelog = dataSizeTableLog[t->operandType]; // log2(elementsize)
    uint32_t elementNum = t->vectorOffset >> dsizelog;    // index to vector element
    return (elementNum & 1) ? t->parm[1].q : t->parm[2].q;
}

static uint64_t replace_odd (CThread * t) {
    // Replace odd-numbered elements in RT by constant IM2
    uint8_t  dsizelog = dataSizeTableLog[t->operandType]; // log2(elementsize)
    uint32_t elementNum = t->vectorOffset >> dsizelog;    // index to vector element
    return (elementNum & 1) ? t->parm[2].q : t->parm[1].q;
}

static uint64_t broadcast_32 (CThread * t) {
    // Broadcast 32-bit or 64 -bit constant into all elements of RD with length RS (31 in RS field gives scalar output).
    uint8_t  rd = t->operands[0];
    uint8_t  rs = t->operands[4];
    uint8_t  rm = t->operands[1];                // mask register
    uint32_t elementSize = dataSizeTable[t->operandType];
    uint8_t  dsizelog = dataSizeTableLog[t->operandType]; // log2(elementsize)
    uint64_t length;                             // length of destination 
    int64_t  value;
    if (rs == 31) length = elementSize;
    else length = t->registers[rs] << dsizelog >> dsizelog;   // round length to multiple of elementSize
    if (length > t->MaxVectorLength) length = t->MaxVectorLength;
    t->vectorLength[rd] = (uint32_t)length;                   // set length of destination
    for (uint32_t pos = 0; pos < length; pos += elementSize) { // loop through vector
        if (rm >= 7 || (t->readVectorElement(rm, pos) & 1)) value = t->parm[2].qs; // check mask
        else value = 0;
        t->writeVectorElement(rd, value, pos);             // write to destination
    } 
    t->vect = 4;                                           // stop vector loop
    t->running = 2;                                        // don't save RD
    return 0;
}

static uint64_t permute (CThread * t) {
    // The vector elements of RT are permuted within each block of size RS bytes. 
    // The number of elements in each block, n = RS / OS
    // format 2.2.6 op 1.1: index vector is last operand
    // format 2.6   op   8: index vector is constant IM2, 4 bits for each element
    uint8_t  rd = t->operands[0];                          // destination
    uint8_t  rm = t->operands[1];                          // mask register
    uint8_t  vin;                                          // input data register
    uint8_t  vpat = 0;                                     // pattern register
    uint8_t  rs;                                           // block size, g.p. register
    uint32_t pattern = 0;                                  // IM2 = pattern, if constant
    bool     constPat = false;                             // pattern is a constant
    if (t->fInstr->format2 == 0x226) {
        vin  = t->operands[3];                             // ru = input data
        vpat = t->operands[5];                             // rt = pattern
        rs  = t->operands[4];                              // block size, g.p. register
    }
    else {                                                 // format 2.6
        rs  = t->operands[3];                              // block size, g.p. register
        vin = t->operands[4];                              // rt = input data
        pattern = t->parm[4].i;                            // IM2 = pattern, if constant
        constPat = true;
    }
    uint8_t  dsizelog = dataSizeTableLog[t->operandType];  // log2(elementsize)
    uint32_t elementSize = 1 << dsizelog;
    uint32_t length = t->vectorLength[vin];                // vector length
    t->vectorLength[rd] = length;                          // set length of destination
    int8_t * source = t->vectors.buf() + (uint32_t)(vin & 0x1F) * t->MaxVectorLength; // address of source data vector
    if (vin == rd) {
        // source and destination are the same. Make a temporary copy of source to avoid overwriting
        memcpy(t->tempBuffer, source, length);
        source = t->tempBuffer;    
    }
    uint64_t blocksize = t->registers[rs];                 // bytes per block
    uint64_t value;                                        // value of element
    uint64_t index;                                        // index to source element
    if (blocksize == 0 || (blocksize & (blocksize-1)) || blocksize > t->MaxVectorLength) {
        t->interrupt(INT_INST_ILLEGAL);                    // RS must be a power of 2
    }
    else {
        uint32_t num = (uint32_t)blocksize >> dsizelog;    // elements per block
        for (uint32_t block = 0; block < length; block += (uint32_t)blocksize) {  // loop through blocks
            for (uint32_t element = 0; element < num; element++) { // loop through elements within block
                if (constPat) {  // get index from constant              
                    index = (pattern >> (element&7)*4) & 0xF; // index to select block element
                }
                else {  // get index from vector
                    index = t->readVectorElement(vpat, block + (element << dsizelog));
                }
                if (index < num && (rm == 7 || t->readVectorElement(rm, block + (element << dsizelog)) & 1)) { // check mask
                    value = *(uint64_t*)(source + block + ((uint32_t)index << dsizelog));        // pick indexed element from source vector
                }
                else value = 0; // index out of range or mask = 0
                t->writeVectorElement(rd, value, block + (element << dsizelog));  // write destination
            }
        }
    }
    t->vect = 4;                                           // stop vector loop
    t->running = 2;                                        // don't save RD
    return 0;
}

static uint64_t replace_bits (CThread * t) {
    // Replace a group of contiguous bits in RT by a specified constant
    SNum a = t->parm[1];                         // input operand
    SNum b = t->parm[2];                         // input constant
    uint64_t val = b.s;                          // value of replacement bits
    uint8_t  pos = uint8_t(b.i >> 16);           // position of replacement
    uint8_t  num = uint8_t(b.i >> 24);           // number of consecutive bits to replace
    uint64_t mask = ((uint64_t)1 << num) - 1;    // mask with num 1-bits
    return (a.q & ~(mask<<pos)) | ((val & mask) << pos);
}

// Format 2.5 A. Single format instructions with memory operands or mixed register types

static uint64_t store_i32 (CThread * t) {
    // Store 32-bit constant IM2 to memory operand [RS+IM1] 
    uint64_t value = t->parm[2].q;
    if ((t->parm[3].b & 1) == 0) value = 0;      // check mask
    t->writeMemoryOperand(value, t->memAddress);
    t->running = 2;                              // don't save RD
    t->returnType = (t->returnType & 7) | 0x20;
    return 0;
}

//static uint64_t fence_ (CThread * t) {return f_nop(t);}

static uint64_t compare_swap (CThread * t) {
    // Atomic compare and exchange with address [RT+IM2]
    uint64_t val1 = t->parm[0].q;
    uint64_t val2 = t->parm[1].q;
    // to do: use intrinsic compareandexchange or mutex or pause all threads if multiple threads
    uint64_t address = t->memAddress;
    uint64_t sizemask = dataSizeMask[t->operandType]; // mask for operand size
    uint64_t val3 = t->readMemoryOperand(address);    // read value from memory
    if (((val3 ^ val1) & sizemask) == 0) {            // value match
        t->writeMemoryOperand(val2, address);         // write new value to memory
    }
    t->vect = 4;                                      // stop vector loop
    return val3;                                      // return old value
}

static uint64_t read_insert (CThread * t) {
    // Replace one element in vector RD, starting at offset RS*OS, with scalar memory operand [RT+IM2]
    uint8_t  rd = t->operands[0];
    uint8_t  rs = t->operands[4];
    uint32_t elementSize = dataSizeTable[t->operandType];
    uint64_t value = t->readMemoryOperand(t->memAddress);
    uint64_t pos = t->registers[rs] * elementSize;
    if (pos < t->vectorLength[rd]) {
        t->writeVectorElement(rd, value, (uint32_t)pos);
    }
    t->vect = 4;                                           // stop vector loop
    t->running = 2;                                        // don't save RD
    return 0;
}

static uint64_t extract_store (CThread * t) {
    // Extract one element from vector RD, starting at offset RS*OS, with size OS into memory operand [RT+IM2]
    uint8_t  rd = t->operands[0];
    uint8_t  rs = t->operands[4];
    uint32_t elementSize = dataSizeTable[t->operandType];
    uint64_t pos = t->registers[rs] * elementSize;
    uint64_t value = t->readVectorElement(rd, (uint32_t)pos);
    t->writeMemoryOperand(value, t->memAddress);
    t->returnType = (t->returnType & 7) | 0x20;            // debug return type is memory
    t->vect = 4;                                           // stop vector loop
    t->running = 2;                                        // don't save RD
    t->vectorLengthR = elementSize;                        // size of memory destination
    return 0;
}


// Format 2.2.6 E. Four vector registers

static uint64_t concatenate (CThread * t) {
    // A vector RU of length RS and a vector RT of length RS are concatenated into a vector RD of length 2*RS.
    uint8_t  rd = t->operands[0];
    uint8_t  ru = t->operands[3];
    uint8_t  rs = t->operands[4];
    uint8_t  rt = t->operands[5];
    uint64_t length1 = t->registers[rs];
    if (length1 > t->MaxVectorLength) length1 = t->MaxVectorLength;
    uint32_t length2 = 2 * (uint32_t)length1;
    if (length2 > t->MaxVectorLength) length2 = t->MaxVectorLength;
    t->vectorLength[rd] = length2;                                   // set length of destination vector
    int8_t * source1 = t->vectors.buf() + ru*t->MaxVectorLength;     // address of RU data
    int8_t * source2 = t->vectors.buf() + rt*t->MaxVectorLength;     // address of RT data
    int8_t * destination = t->vectors.buf() + rd*t->MaxVectorLength; // address of RD data
    memcpy(destination, source1, (uint32_t)length1);                 // copy from RU
    memcpy(destination + (uint32_t)length1, source2, length2 - (uint32_t)length1);  // copy from RT
    t->vect = 4;                                                     // stop vector loop
    t->running = 2;                                                  // don't save RD
    return 0;
}

static uint64_t interleave (CThread * t) {
    // Interleave elements of vectors RU and RT of length RS/2 to produce vector RD of length RS.
    // Even-numbered elements of the destination come from RU and odd-numbered elements from RT.
    uint8_t  rd = t->operands[0];                // destination
    uint8_t  ru = t->operands[3];                // first input vector
    uint8_t  rs = t->operands[4];                // length
    uint8_t  rt = t->operands[5];                // second input vector
    uint8_t  rm = t->operands[1];                // mask
    uint64_t length = t->registers[rs];
    if (length > t->MaxVectorLength) length = t->MaxVectorLength;
    uint8_t  dsizelog = dataSizeTableLog[t->operandType]; // log2(elementsize)
    length = length >> dsizelog << dsizelog;     // round down to nearest multiple of element size
    uint32_t elementSize = 1 << dsizelog;        // size of each element
    t->vectorLength[rd] = (uint32_t)length;      // set length of destination
    uint8_t even = 1;
    uint32_t pos1 = 0;
    uint64_t value;
    for (uint32_t pos2 = 0; pos2 < length; pos2 += elementSize) {
        if (even) {
            value = t->readVectorElement(ru, pos1);
        }
        else {
            value = t->readVectorElement(rt, pos1);
            pos1 += elementSize;
        }
        even ^= 1;                               // toggle between even and odd
        if (rm < 7 && (t->readVectorElement(rm, pos2) & 1) == 0) value = 0; // mask is 0
        t->writeVectorElement(rd, value, pos2);
    }
    t->vect = 4;                                 // stop vector loop
    t->running = 2;                              // don't save RD
    return 0;
}


// Format 2.2.7 E. Three vector registers and a 16 bit immediate

static uint64_t move_bits (CThread * t) {
    // Replace one or more contiguous bits at one position of RS with contiguous bits from another position of RT
    // Format 2.0.7 E: general purpose registers
    // Format 2.2.7 E: vector registers
    // The position in src2 is the lower 8 bits of IM2. a = IM2 & 0xFF.
    // The position in src1 is the upper 8 bits of IM2. b = IM2 >> 0xFF.
    // The number of bits to move is c = IM3.
    SNum s1 = t->parm[0];                        // input operand src1
    SNum s2 = t->parm[1];                        // input operand src2
    SNum im = t->parm[4];                        // input operand IM2
    SNum mask = t->parm[3];                      // 
    uint8_t c = t->pInstr->a.im3;                // input operand IM3 = number of bits
    uint8_t pos1 = im.s >> 8;                    // bit position in src1. (can overflow, not handled)
    uint8_t pos2 = im.b;                         // bit position in src2. (can overflow, not handled)
    uint64_t bitmask = ((uint64_t)1 << c) - 1;   // mask of c bits. (cannot overflow because c is max 63)
    uint64_t result = (s1.q & ~(bitmask << pos1)) | ((s2.q >> pos2) & bitmask) << pos1;
    if ((mask.b & 1) == 0) {                     // single format instructions with template E must handle mask here
        result = s1.q;                           // fallback
        if (t->operands[2] == 31) result = 0;    // fallback = 0
    }
    return result;
}

static uint64_t mask_length (CThread * t) {
    // Make a boolean vector to mask the first n bytes of a vector.
    // The output vector RD will have the same length as the input vector RT. 
    // RS indicates the length of the part that is enabled by the mask (n).
    // IM3 contains the following option bits:
    // bit 0 = 0: bit 0 will be 1 in the first n bytes in the output and 0 in the rest.
    // bit 0 = 1: bit 0 will be 0 in the first n bytes in the output and 1 in the rest.
    // bit 1 = 1: copy remaining bits from input vector RT into each vector element.
    // bit 2 = 1: copy remaining bits from the numeric control register.
    // bit 4 = 1: broadcast remaining bits from IM2 into all 32-bit words of RD:
    //    Bit 1-7 of IM2 go to bit 1-7 of RD. Bit 8-11 of IM2 go to bit 20-23 of RD. Bit 12-15 of IM2 go to bit 26-29 of RD.
    // Output bits that are not set by any of these options will be zero. If multiple options are specified, the results will be OR’ed.
    uint8_t rd = t->operands[0];                 // destination
    uint8_t rs = t->operands[3];                 // length
    uint8_t rt = t->operands[4];                 // src2
    SNum s2 = t->parm[1];                        // input operand src2
    SNum im2 = t->parm[4];                       // input operand IM2
    uint8_t im3 = t->pInstr->a.im3;              // input operand IM3 = options
    t->vectorLengthR = t->vectorLength[rd] = t->vectorLength[rt]; // set length of destination
    uint8_t  dsizelog = dataSizeTableLog[t->operandType]; // log2(elementsize)
    uint64_t n = t->registers[rs];               // number of masked elements
    uint32_t i = t->vectorOffset >> dsizelog;    // current element index
    uint8_t bit = i < n;                         // element is within the first n
    bit ^= im3 & 1;                              // invert option
    uint64_t result = 0;
    if (im3 & 2) result |= s2.q;                 // copy remaining bits from src1
    if (im3 & 4) result |= t->numContr;          // copy remaining bits from NUMCONTR
    if (im3 & 0x10) {                            // copy bits from IM2
        uint32_t rr = (im2.b & ~1) | bit;        // bit 1-7 -> bit 1-7
        rr |= (im2.s & 0xF00) << 12;             // bit 8-11 -> bit 20-23
        rr |= (im2.s & 0xF000) << 14;            // bit 12-15 -> bit 26-29
        result |= rr | ((uint64_t)rr << 32);     // copy these bits twice
    }
    result = (result & ~(uint64_t)1) | bit;      // combine
    return result;
}

static uint64_t truth_tab3 (CThread * t) {
    //  Boolean function of three inputs, given by a truth table 
    SNum a = t->parm[0];                         // first operand
    SNum b = t->parm[1];                         // second operand
    SNum c = t->parm[2];                         // third operand
    SNum mask = t->parm[3];                      // mask register
    uint32_t table = t->pInstr->a.im2;           // truth table
    uint8_t  options = t->pInstr->a.im3;         // option bits
    uint32_t select = (a.i & 1) | (b.i & 1) << 1 | (c.i & 1) << 2; // combine bit 0 of three inputs
    uint8_t bit = (table >> select);             // look up in 8-bit truth table
    bit &= mask.b;                               // mask use options: 0: fallback, 1: and with mask
    if (!(options & 1)) bit |= ~mask.b & a.b;
    // get remaining bits
    uint64_t result = 0;
    if (options & 2) result |= a.q;              // copy remaining bits from src1
    if (options & 4) result |= t->numContr;      // copy remaining bits from NUMCONTR
    if (options & 8) result |= mask.q;           // copy remaining bits from mask register
    if (options & 0x10) {                        // copy bits from IM2
        uint32_t rr = (table & 0xF00) << 12;     // bit 8-11 -> bit 20-23
        rr |= (table & 0xF000) << 14;            // bit 12-15 -> bit 26-29
        result |= rr | (uint64_t)rr << 32;       // copy these bits twice
    }
    result = (result & ~(uint64_t)1) | (bit & 1);// combine
    return result;
}

static uint64_t repeat_block (CThread * t) {
    // Repeat a block of data to make a longer vector. 
    // RT is input vector containing data block to repeat. 
    // IM2 is length in bytes of the block to repeat (must be a multiple of 4). 
    // RS is the length of destination vector RD.
    uint8_t  rd = t->operands[0];
    uint8_t  rs = t->operands[3];
    uint8_t  rt = t->operands[4];
    uint32_t blen = t->parm[4].i;                // block length
    uint64_t length = t->registers[rs];          // length of destination
    if (length > t->MaxVectorLength) length = t->MaxVectorLength;
    if (blen > t->MaxVectorLength) blen = t->MaxVectorLength;
    t->vectorLength[rd] = (uint32_t)length;        // set length of destination
    if (blen & 3) t->interrupt(INT_INST_ILLEGAL);  // must be a multiple of 4
    int8_t * source = t->vectors.buf() + rt*t->MaxVectorLength;      // address of RT data
    int8_t * destination = t->vectors.buf() + rd*t->MaxVectorLength; // address of RD data
    if (length > t->vectorLength[rt]) { // reading beyond the end of the source vector. make sure the rest is zero
        memset(source + t->vectorLength[rt], 0, size_t(length - t->vectorLength[rt]));
    } 
    for (uint32_t pos = 0; pos < length; pos += blen) {  // loop through blocks
        uint32_t blen2 = blen;
        if (pos + blen2 > length) blen2 = (uint32_t)length - pos;  // avoid last block going too far
        memcpy(destination + pos, source, blen2);          // copy block
    }
    t->vect = 4;                                           // stop vector loop
    t->running = 2;                                        // don't save RD
    return 0;
}

static uint64_t repeat_within_blocks (CThread * t) {
    // Broadcast the first element of each block of data in a vector to the entire block. 
    // RT is input vector containing data blocks. 
    // IM2 is length in bytes of each block (must be a multiple of the operand size). 
    // RS is length of destination vector RD. 
    // The operand size must be at least 4 bytes.
    uint8_t  rd = t->operands[0];
    uint8_t  rs = t->operands[3];
    uint8_t  rt = t->operands[4];
    uint32_t blen = t->parm[4].i;                // block length
    uint64_t length = t->registers[rs];          // length of destination
    if (length > t->MaxVectorLength) length = t->MaxVectorLength;
    if (blen > t->MaxVectorLength) blen = t->MaxVectorLength;
    t->vectorLength[rd] = (uint32_t)length;        // set length of destination
    uint32_t elementSize = dataSizeTable[t->operandType];
    if (elementSize < 4 || (blen & (elementSize - 1))) t->interrupt(INT_INST_ILLEGAL);  // must be a multiple of elementsize
    int8_t * source = t->vectors.buf() + rt*t->MaxVectorLength;      // address of RT data
    int8_t * destination = t->vectors.buf() + rd*t->MaxVectorLength; // address of RD data
    if (length > t->vectorLength[rt]) { // reading beyond the end of the source vector. make sure the rest is zero
        memset(source + t->vectorLength[rt], 0, size_t(length - t->vectorLength[rt]));
    } 
    for (uint32_t pos = 0; pos < length; pos += blen) {  // loop through blocks
        uint32_t blen2 = blen;
        if (pos + blen2 > length) blen2 = (uint32_t)length - pos;  // avoid last block going too far
        for (uint32_t i = 0; i < blen2; i += elementSize) {  // loop within block        
            memcpy(destination + pos + i, source + pos, elementSize);  // copy first element
        }
    }
    t->vect = 4;                                           // stop vector loop
    t->running = 2;                                        // don't save RD
    return 0;
}

 
// tables of single format instructions

// Format 1.3 B. Two vector registers and a broadcast 8-bit immediate operand.
// Format 1.3 C. One vector register and a broadcast 16-bit immediate operand.
PFunc funcTab8[64] = {
    gp2vec, vec2gp, read_spev, make_sequence, insert_, extract_, compress, expand,  // 0  - 7
    0, 0, 0, 0, float2int, int2float, round_, round2n,                              // 8 - 15
    abs_, fp_category, broad_, broad_, byte_reverse, bitscan_, popcount_, 0,        // 16 - 23
    truth_tab2, 0, 0, 0, 0, 0, 0, 0,                                                // 24 - 31
    move_i16, f_add, and_i16, or_i16, xor_i16, add_h16, mul_h16, 0,                 // 32 - 39
    move_8shift8, move_8shift8, add_8shift8, add_8shift8, and_8shift8, and_8shift8, or_8shift8, or_8shift8, // 40 - 47
    xor_8shift8, xor_8shift8, 0, 0, 0, 0, 0, 0,                                     // 48 - 55
    move_half2float, move_half2double, add_half2float, add_half2double, mul_half2float, mul_half2double, 0, 0
};

// Format 2.5 A. Single format instructions with memory operands or mixed register types
PFunc funcTab10[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,                                                 // 0 - 7
    store_i32, 0, 0, 0, 0, 0, 0, 0,                                         // 8 - 15
    f_nop, 0, compare_swap, 0, 0, 0, 0, 0,                                  // 16 - 23
    read_insert, 0, 0, 0, 0, 0, 0, 0,                                       // 24 - 31
    extract_store, 0, 0, 0, 0, 0, 0, 0,                                     // 32 - 39
};


// Format 2.6 A. Three vector registers and a 32-bit immediate operand.
PFunc funcTab11[64] = {
    load_hi, insert_hi, make_mask, replace_, replace_even, replace_odd, broadcast_32, 0, // 0 - 7
    permute, replace_bits, 0, 0, 0, 0, 0, 0                                        // 8 - 15
};

// Format 3.1 A. Three vector registers and a 64-bit immediate operand.
PFunc funcTab13[64] = {           
    0, 0, 0, 0, 0, 0, 0, 0,                                                    // 0 - 7
    replace_, broadcast_32, 0, 0, 0, 0, 0, 0,                                             // 8 - 15
};


// Dispatch functions for single format instruction with E template.
// (full tables of all possible single format instruction with E template would 
//  be too large with most places unused).

// Format 2.0.7 E. Three general purpose registers and a 16-bit immediate constant
static uint64_t dispatch207_1 (CThread * t) {
    switch (t->op) {
    case 0: return move_bits(t);
    default:
        t->interrupt(INT_UNKNOWN_INST);
    }
    return 0;
}

// Format 2.2.6 E. Four vector registers
static uint64_t dispatch226_1 (CThread * t) {
    switch (t->op) {
    case 0: return concatenate(t);
    case 1: return permute(t);
    case 2: return interleave(t);
    case 8: return truth_tab3(t);
    default:
        t->interrupt(INT_UNKNOWN_INST);
    }
    return 0;
}

// Format 2.2.7 E. Three vector registers and a 16-bit immediate constant
static uint64_t dispatch227_1 (CThread * t) {
    switch (t->op) {
    case 0: return move_bits(t);
    case 1: return mask_length(t);
    case 8: return repeat_block(t);
    case 9: return repeat_within_blocks(t);
    default:
        t->interrupt(INT_UNKNOWN_INST);
    }
    return 0;
}

// Table of dispatch functions for all possible single format instructions with E template
PFunc EDispatchTable[96] = {
    0, 0, 0, 0, 0, 0, 0, dispatch207_1,                    // 2.0.x i.1
    0, 0, 0, 0, 0, 0, dispatch226_1, dispatch227_1,        // 2.2.x i.1
    0, 0, 0, 0, 0, 0, 0, 0,                                // 3.0.x i.1
    0, 0, 0, 0, 0, 0, 0, 0,                                // 3.2.x i.1

    0, 0, 0, 0, 0, 0, 0, 0,                                // 2.0.x i.2
    0, 0, 0, 0, 0, 0, 0, 0,                                // 2.2.x i.2
    0, 0, 0, 0, 0, 0, 0, 0,                                // 3.0.x i.2
    0, 0, 0, 0, 0, 0, 0, 0,                                // 3.2.x i.2

    0, 0, 0, 0, 0, 0, 0, 0,                                // 2.0.x i.3
    0, 0, 0, 0, 0, 0, 0, 0,                                // 2.2.x i.3
    0, 0, 0, 0, 0, 0, 0, 0,                                // 3.0.x i.3
    0, 0, 0, 0, 0, 0, 0, 0                                 // 3.2.x i.3
};

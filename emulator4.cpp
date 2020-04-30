/****************************  emulator4.cpp  ********************************
* Author:        Agner Fog
* date created:  2018-02-18
* Last modified: 2020-04-17
* Version:       1.09
* Project:       Binary tools for ForwardCom instruction set
* Description:
* Emulator: Execution functions for tiny instructions and multiformat instructions
*
* Copyright 2018-2020 GNU General Public License http://www.gnu.org/licenses
*****************************************************************************/

#include "stdafx.h"

// get intrinsic functions for _mm_getcsr and _mm_setcsr to control floating point rounding and exceptions
#if defined(_M_X64) || defined(__x86_64__) || defined(__amd64) || defined(__SSE2__)
#if defined(__FMA__) || defined(__AVX2__)
#define FMA_AVAILABLE 1
#else 
#define FMA_AVAILABLE 0
#endif
#if defined(_MSC_VER) && !FMA_AVAILABLE
#include <xmmintrin.h>
#else
#include <immintrin.h>
#endif
#define MCSCR_AVAILABLE 1
#else
#define MCSCR_AVAILABLE 0
#endif


//////////////////////////////////////////////////////////////////////////////////////////////////////
// functions for detecting exceptions and controlling rounding mode on the CPU that runs the emulator
// Note: these functions are only available in x86 systems with SSE2 or x64 enabled
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Error message if MXCSR not available
void errorFpControlMissing() {
    static int repeated = 0;
    if (!repeated) {
        fprintf(stderr, "Error: Cannot control floating point exceptions and rounding mode on this platform");
        repeated = 1;
    }
}

void setRoundingMode(uint8_t r) {
    // change rounding mode
#if MCSCR_AVAILABLE
    uint32_t e = _mm_getcsr();
    e = (e & 0x9FFF) | (r & 3) << 13;
    _mm_setcsr(e);
#else
    errorFpControlMissing();
#endif
}

void clearExceptionFlags() {
    // clear exception flags before detecting exceptions
#if MCSCR_AVAILABLE
    uint32_t e = _mm_getcsr();
    _mm_setcsr(e & 0xFFC0);
#else
    errorFpControlMissing();
#endif
}

uint32_t getExceptionFlags() {
    // read exception flags after instructions that may cause exceptions
    // 1: invalid operation
    // 2: denormal
    // 4: divide by zero
    // 8: overflow
    // 0x10: underflow
    // 0x20: precision
#if MCSCR_AVAILABLE
    return _mm_getcsr() & 0x3F;
#else
    errorFpControlMissing();
    return 0;
#endif
}

void enableSubnormals(uint32_t e) {
    // enable or disable subnormal numbers
#if MCSCR_AVAILABLE
    uint32_t x = _mm_getcsr();
    if (e != 0) {    
        _mm_setcsr(x & ~0x8040);
    }
    else {
        _mm_setcsr(x | 0x8040);
    }
#else
    errorFpControlMissing();
#endif
}


//////////////////////
// Tiny instructions
//////////////////////

static uint64_t t_nop(CThread * t) {
    // No operation
    t->vect = 4;                                 // stop vector loop
    t->running = 2;                              // don't save RD
    t->returnType = 0;                           // debug return output
    return 0;
}

static uint64_t t_move_iu(CThread * t) {
    // RD = unsigned constant RS.
    t->returnType = 0x13;                        // debug return output
    return t->parm[2].q;
}

static uint64_t t_add(CThread * t) {
    // RD += unsigned constant RS
    uint64_t a = t->registers[t->operands[4]];
    uint64_t b = t->parm[2].q;
    uint64_t result = a + b;
    /*
    if (t->numContr & MSK_OVERFL_I) {  // check for overflow
        if ((t->numContr & MSK_OVERFL_UNSIGN) && result < a) t->interrupt(INT_OVERFL_UNSIGN);  // unsigned overflow
        if ((t->numContr & MSK_OVERFL_SIGN) && (result & ~a) >> 63) t->interrupt(INT_OVERFL_SIGN);  // signed overflow
    } */
    t->returnType = 0x13;                        // debug return output
    return result;
}

static uint64_t t_sub(CThread * t) {
    // RD -= unsigned constant RS
    uint64_t a = t->registers[t->operands[4]];
    uint64_t b = t->parm[2].q;
    uint64_t result = a - b;
    /*
    if (t->numContr & MSK_OVERFL_I) {  // check for overflow
        if ((t->numContr & MSK_OVERFL_UNSIGN) && result > a) t->interrupt(INT_OVERFL_UNSIGN);  // unsigned overflow
        if ((t->numContr & MSK_OVERFL_SIGN) && (a & ~result) >> 63) t->interrupt(INT_OVERFL_SIGN);  // signed overflow
    } */
    t->returnType = 0x13;                        // debug return output
    return result;
}

static uint64_t t_shift_left(CThread * t) {
    // RD <<= unsigned constant RS (no overflow detection on shift instructions)
    t->returnType = 0x13;                        // debug return output
    uint64_t result = t->registers[t->operands[4]] << t->parm[2].b;
    if (t->parm[2].b > 63) result = 0;
    return result;
}

static uint64_t t_shift_right_u(CThread * t) {
    // RD >>= unsigned constant RS (no overflow detection on shift instructions)
    t->returnType = 0x13;                        // debug return output
    uint64_t result = t->registers[t->operands[4]] >> t->parm[2].b;
    if (t->parm[2].b > 63) result = 0;
    return result;
}

static uint64_t t_move_r(CThread * t) {
    // RD = register operand RS
    t->returnType = 0x13;                        // debug return output
    return t->registers[t->operands[5]];
}

static uint64_t t_add_r(CThread * t) {
    // RD += register operand RS
    uint64_t a = t->registers[t->operands[4]];
    uint64_t b = t->registers[t->operands[5]];
    uint64_t result = a + b;
    /*
    if (t->numContr & MSK_OVERFL_I) {  // check for overflow
        if ((t->numContr & MSK_OVERFL_UNSIGN) && result < a) t->interrupt(INT_OVERFL_UNSIGN);  // unsigned overflow
        if ((t->numContr & MSK_OVERFL_SIGN) && 
            int64_t(~(a ^ b) & (a ^ result)) < 0) t->interrupt(INT_OVERFL_SIGN);  // signed overflow
    } */
    t->returnType = 0x13;                        // debug return output
    return result;
}

static uint64_t t_sub_r(CThread * t) {
    // RD -= register operand RS
    uint64_t a = t->registers[t->operands[4]];
    uint64_t b = t->registers[t->operands[5]];
    uint64_t result = a - b;
    /*
    if (t->numContr & MSK_OVERFL_I) {  // check for overflow
        if ((t->numContr & MSK_OVERFL_UNSIGN) && result < a) t->interrupt(INT_OVERFL_UNSIGN);  // unsigned overflow
        if ((t->numContr & MSK_OVERFL_SIGN) && 
            int64_t((a ^ b) & (a ^ result)) < 0) t->interrupt(INT_OVERFL_SIGN);  // signed overflow
    } */
    t->returnType = 0x13;                        // debug return output
    return result;
}

static uint64_t t_and_r(CThread * t) {
    // RD &= register operand RS
    t->returnType = 0x13;                        // debug return output
    return t->registers[t->operands[4]] & t->registers[t->operands[5]];
}

static uint64_t t_or_r(CThread * t) {
    // RD |= register operand RS
    t->returnType = 0x13;                        // debug return output
    return t->registers[t->operands[4]] | t->registers[t->operands[5]];
}

static uint64_t t_xor_r(CThread * t) {
    // RD ^= register operand RS
    t->returnType = 0x13;                        // debug return output
    return t->registers[t->operands[4]] ^ t->registers[t->operands[5]];
}

static uint64_t t_read_r(CThread * t) {
    // Read RD from memory operand with pointer RS (RS = r0-r14, r31)
    if (t->rs == 15) t->rs = 31;                           // 15 is stack pointer
    t->returnType = 0x13;                                  // debug return output
    return t->readMemoryOperand(t->getMemoryAddress());
}

static uint64_t t_write_r(CThread * t) {
    // Write RD to memory operand with pointer RS (RS = r0-r14, r31)
    if (t->rs == 15) t->rs = 31;                           // 15 is stack pointer
    uint64_t value = t->registers[t->operands[4]];         // get RD
    t->writeMemoryOperand(value, t->getMemoryAddress());   // write to memory
    t->returnType = 0x23;                                  // debug return output
    t->running = 2;                                        // don't save RD
    return value;                                          // return RD unchanged
}

static uint64_t t_clear(CThread * t) {
    // Clear register RD by setting the length to zero
    uint8_t rd = t->operands[4];
    t->vectorLength[rd] = 0;
    t->vectorLengthR = 0;
    t->vect = 4;                                           // stop vector loop
    t->running = 2;                                        // don't save RD
    return 0;
}

static uint64_t t_move_v(CThread * t) {
    // RD = RS. Copy vector of any type.
    uint8_t rs = t->operands[5];
    t->returnType = 0x112;                                 // debug return output
    if (t->vectorLength[rs] == 0) return 0;                // empty vector
    while ((t->vectorLengthR & (dataSizeTableMax8[t->operandType] - 1)) && t->operandType) { 
        t->operandType--;       // length is not divisible by data size. reduce operand type
        t->vectorLengthR = dataSizeTableMax8[t->operandType];
    }
    return t->readVectorElement(rs, t->vectorOffset);
}

static uint64_t t_move_uf(CThread * t) {
    // RD = RS. unsigned 4-bit integer RS, converted to single precision scalar.
    SNum result;
    result.f = (float)(t->parm[2].is);
    uint8_t rd = t->operands[0];
    t->vectorLength[rd] = 4;
    t->vectorLengthR = 4;
    t->returnType = 0x115;                       // debug return output
    return result.q;
}

static uint64_t t_move_ud(CThread * t) {
    // RD = RS. unsigned 4-bit integer RS, converted to double precision scalar.
    SNum result;
    result.d = (double)(t->parm[2].is);
    uint8_t rd = t->operands[0];
    t->vectorLength[rd] = 8;
    t->vectorLengthR = 8;
    t->returnType = 0x116;                       // debug return output
    return result.q;
}

static uint64_t t_add_f(CThread * t) {
    // RD += RS, single precision float vector
    // set up parameters for using f_add
    t->operandType = 5;
    t->parm[3].q = t->numContr;
    t->parm[1].q = t->readVectorElement(t->operands[4], t->vectorOffset);
    t->parm[2].q = t->readVectorElement(t->operands[5], t->vectorOffset);
    t->returnType = 0x115;                       // debug return output
    return f_add(t);
}

static uint64_t t_add_d(CThread * t) {
    // RD += RS, double precision float vector.
    // set up parameters for using f_add
    t->operandType = 6;
    t->parm[3].q = t->numContr;
    t->parm[1].q = t->readVectorElement(t->operands[4], t->vectorOffset);
    t->parm[2].q = t->readVectorElement(t->operands[5], t->vectorOffset);
    t->returnType = 0x116;                       // debug return output
    return f_add(t);
}

static uint64_t t_sub_f(CThread * t) {
    // RD -= RS, single precision float vector
    // set up parameters for using f_sub
    t->operandType = 5;
    t->parm[3].q = t->numContr;
    t->parm[1].q = t->readVectorElement(t->operands[4], t->vectorOffset);
    t->parm[2].q = t->readVectorElement(t->operands[5], t->vectorOffset);
    t->returnType = 0x115;                       // debug return output
    return f_sub(t);
}

static uint64_t t_sub_d(CThread * t) {
    // RD -= RS, double precision float vector.
    // set up parameters for using f_sub
    t->operandType = 6;
    t->parm[3].q = t->numContr;
    t->parm[1].q = t->readVectorElement(t->operands[4], t->vectorOffset);
    t->parm[2].q = t->readVectorElement(t->operands[5], t->vectorOffset);
    t->returnType = 0x116;                       // debug return output
    return f_sub(t);
}

static uint64_t t_mul_f(CThread * t) {
    // RD *= RS, single precision float vector
    // set up parameters for using f_mul
    t->operandType = 5;
    t->parm[3].q = t->numContr;
    t->parm[1].q = t->readVectorElement(t->operands[4], t->vectorOffset);
    t->parm[2].q = t->readVectorElement(t->operands[5], t->vectorOffset);
    t->returnType = 0x115;                       // debug return output
    return f_mul(t);
}

static uint64_t t_mul_d(CThread * t) {
    // RD *= RS, double precision float vector.
    // set up parameters for using f_mul
    t->operandType = 6;
    t->parm[3].q = t->numContr;
    t->parm[1].q = t->readVectorElement(t->operands[4], t->vectorOffset);
    t->parm[2].q = t->readVectorElement(t->operands[5], t->vectorOffset);
    t->returnType = 0x116;                       // debug return output
    return f_mul(t);
}

static uint64_t t_add_cps(CThread * t) {
    // Get size of compressed image for RD and add it to pointer register RS.
    uint8_t rd = t->operands[4];
    if (t->rs == 15) t->rs = 31;                           // 15 is stack pointer
    uint32_t len = t->vectorLength[rd];
    if (len <= 4) {
        len = 8;                                           // up to 4 bytes stored as one stack entry with 4 bytes for length and 4 bytes for data
    }
    else {
        len = ((len + 7) & -8) + 8;                        // round up to nearest multiple of 8 + 8 bytes for length
    }
    t->registers[t->rs] += len;                            // add this to rs
    t->vect = 4;                                           // stop vector loop
    t->returnType = 0;                                     // debug return output
    t->operands[0] = t->rs;                                // rs is destination
    t->returnType = 0x13;                                  // debug return output
    t->running = 2;                                        // don't save RD
    return t->registers[t->rs];
}

static uint64_t t_sub_cps(CThread * t) {
    // Get size of compressed image for RD and add it to pointer register RS.
    uint8_t rd = t->operands[4];
    if (t->rs == 15) t->rs = 31;                           // 15 is stack pointer
    uint32_t len = t->vectorLength[rd];
    if (len <= 4) {
        len = 8;                                           // up to 4 bytes stored as one stack entry with 4 bytes for length and 4 bytes for data
    }
    else {
        len = ((len + 7) & -8) + 8;                        // round up to nearest multiple of 8 + 8 bytes for length
    }
    t->registers[t->rs] -= len;                            // subtract this from rs
    t->vect = 4;                                           // stop vector loop
    t->operands[0] = t->rs;                                // rs is destination
    t->returnType = 0x13;                                  // debug return output
    t->running = 2;                                        // don't save RD
    return t->registers[t->rs];
}

static uint64_t t_restore_cp(CThread * t) {
    // Restore vector register RD from compressed image pointed to by RS
    uint8_t rd = t->operands[4];
    if (t->rs == 15) t->rs = 31;                           // 15 is stack pointer
    uint64_t address = t->getMemoryAddress();
    uint64_t len1 = t->readMemoryOperand(address);         // length and possibly 4 bytes of data
    // save length
    uint32_t len = (uint32_t)len1;                         // length
    if (len > t->MaxVectorLength) len = t->MaxVectorLength;
    if (len <= 4) {
        // contained in a single stack entry
        t->vectorLength[rd] = 8;                           // temporarily set vectorlength for writeVectorElement
        t->writeVectorElement(rd, len1 >> 32, 0);
    }
    else {
        // copy whole vector
        t->vectorLength[rd] = t->MaxVectorLength;          // temporarily set vectorlength to max for writeVectorElement
        for (uint32_t offset = 0; offset < len; offset += 8) { // we may be reading up to 7 bytes beyond end, but this is allowed
            uint64_t val = t->readMemoryOperand(address + 8 + offset);
            t->writeVectorElement(rd, val, offset);
        }
    }
    t->vectorLength[rd] = len;                             // now set actual vector length
    t->vect = 4;                                           // stop vector loop
    t->running = 2;                                        // don't save RD
    t->returnType = 0x113;                                 // debug return output
    return 0; 
}

static uint64_t t_save_cp(CThread * t) {
    // Save vector register RD to compressed image pointed to by RS
    uint8_t rd = t->operands[4];
    if (t->rs == 15) t->rs = 31;                           // 15 is stack pointer
    uint64_t address = t->getMemoryAddress();
    uint32_t len = t->vectorLength[rd];                    // length
    t->returnType = 0x123;                                 // debug return output. memory
    if (len <= 4) {
        // contained in a single stack entry
        uint64_t val1 = t->vectors.get<uint32_t>(rd*t->MaxVectorLength);
        val1 = (val1 << 32) + len;  // combine value and length into 64 bits
        t->writeMemoryOperand(val1, address);
    }
    else {
        // write length
        t->writeMemoryOperand(len, address);
        t->returnType |= 0x40;                             // one extra element invector
        // write whole vector
        for (uint32_t offset = 0; offset < len; offset += 8) {
            uint64_t val = t->readVectorElement(rd, offset);
            t->writeMemoryOperand(val, address + 8 + offset);
        }
    }
    t->vect = 4;                                           // stop vector loop
    t->running = 2;                                        // don't save RD
    return 0;
} 


/////////////////////////////
// Multi-format instructions
/////////////////////////////

uint64_t f_nop(CThread * t) {
    // No operation
    t->running = 2;                                        // don't save RD
    t->returnType = 0;                                     // debug return output
    return 0;
}

static uint64_t f_store(CThread * t) {
    // Store value RD to memory
    uint8_t rd = t->operands[0];
    uint64_t value = t->registers[rd];
    if (t->vect) {
        value = t->readVectorElement(rd, t->vectorOffset);
    }
    // check mask
    if ((t->parm[3].b & 1) == 0) {
        uint8_t fallback = t->operands[2];                 // mask is 0. get fallback
        if (fallback == 0x1F) value = 0;
        else if (t->vect) value = t->readVectorElement(fallback, t->vectorOffset);
        else value = t->registers[fallback];
    }
    uint64_t address = t->memAddress;
    if (t->vect) address += t->vectorOffset;
    t->writeMemoryOperand(value, address);
    t->returnType = (t->returnType & ~0x10) | 0x20;        // return type is memory
    t->running = 2;                                        // don't save RD
    return 0;
}

static uint64_t f_move(CThread * t) {
    // copy value
    return t->parm[2].q;
}

static uint64_t f_prefetch(CThread * t) {
    // prefetch from address. not emulated
    return f_nop(t);
}

static uint64_t f_sign_extend(CThread * t) {
    // sign-extend integer to 64 bits
    int64_t value = 0;
    switch (t->operandType) {
    case 0:
        value = (int64_t)(int8_t)t->parm[2].b;
        break;
    case 1:
        value = (int64_t)(int16_t)t->parm[2].s;
        break;
    case 2:
        value = (int64_t)(int32_t)t->parm[2].i;
        break;
    case 3:
        value = (int64_t)t->parm[2].q;
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
        value = 0;
    }
    t->operandType = 3;  // change operand size of result    
    if (t->vect) {    
        t->vectorLength[t->operands[0]] = t->vectorLengthR = 8; // change vector length of result and stop vector loop
    }
    t->returnType = (t->returnType & ~7) | 3;              // debug return output
    return (uint64_t)value;
}

static uint64_t f_sign_extend_add(CThread * t) {
    // sign-extend integer to 64 bits and add 64-bit register
    int64_t value = 0;
    switch (t->operandType) {
    case 0:
        value = (int64_t)(int8_t)t->parm[2].b;
        break;
    case 1:
        value = (int64_t)(int16_t)t->parm[2].s;
        break;
    case 2:
        value = (int64_t)(int32_t)t->parm[2].i;
        break;
    case 3:
        value = (int64_t)t->parm[2].q;
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
        value = 0;
    }
    uint8_t r1 = t->operands[4];                           // first operand. g.p. register
    value += t->registers[r1];                             // read register with full size
    t->operandType = 3;                                    // change operand size of result    
    t->returnType = (t->returnType & ~7) | 3;              // debug return output
    if (t->vect) t->interrupt(INT_INST_ILLEGAL);
    return (uint64_t)value;
}

static uint64_t f_compare(CThread * t) {
    // compare two source operands and generate a boolean result
    // get condition code
    uint8_t cond = 0;
    uint32_t mask = t->parm[3].i;                          // mask register value or NUMCONTR
    if (t->fInstr->tmpl == 0xE) {
        cond = t->pInstr->a.im3;                           // E template. get condition from IM3
    }
    else if (t->operands[1] < 7) {
        cond = uint8_t(mask >> MSKI_OPTIONS) & 0xF;        // there is a mask. get condition from mask
    }
    // get operands
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    if ((t->fInstr->imm2 & 4) && t->operandType < 5) {
        b.q = t->pInstr->a.im2;                            // avoid immediate operand shifted by imm3
    }
    uint8_t result = 0;
    uint8_t cond1 = cond >> 1 & 3;                         // bit 1 - 2 of condition
    bool isnan = false;
    // select operand type
    if (t->operandType < 5) {  // integer types
        uint64_t sizeMask = dataSizeMask[t->operandType];  // mask for data size
        uint64_t signBit = (sizeMask >> 1) + 1;            // sign bit
        a.q &= sizeMask;  b.q &= sizeMask;                 // mask to desired size
        if (cond1 != 3 && !(cond & 8)) {                   // signed 
            a.q ^= signBit;  b.q ^= signBit;               // flip sign bit to use unsigned compare
        }
        switch (cond1) {  // select condition
        case 0:   // a == b
            result = a.q == b.q;  
            break;
        case 1:   // a < b
            result = a.q < b.q;  
            break;
        case 2:   // a > b
            result = a.q > b.q;  
            break;
        case 3:   // abs(a) < abs(b)
            if (a.q & signBit) a.q = (~a.q + 1) & sizeMask;  // change sign. overflow allowed
            if (b.q & signBit) b.q = (~b.q + 1) & sizeMask;  // change sign. overflow allowed
            result = a.q < b.q;  
            break;
        }
    }
    else if (t->operandType == 5) {    // float
        isnan = isnan_f(a.i) || isnan_f(b.i);              // check for NAN
        if (!isnan) {
            switch (cond1) {  // select condition
            case 0:   // a == b
                result = a.f == b.f;  
                break;
            case 1:   // a < b
                result = a.f < b.f;  
                break;
            case 2:   // a > b
                result = a.f > b.f;  
                break;
            case 3:   // abs(a) < abs(b)
                result = fabsf(a.f) < fabsf(b.f);
                break;
            }
        }
    }
    else if (t->operandType == 6) {   // double
        isnan = isnan_d(a.q) || isnan_d(b.q);
        if (!isnan) {
            switch (cond1) {  // select condition
            case 0:   // a == b
                result = a.d == b.d;  
                break;
            case 1:   // a < b
                result = a.d < b.d;  
                break;
            case 2:   // a > b
                result = a.d > b.d;  
                break;
            case 3:   // abs(a) < abs(b)
                result = fabs(a.d) < fabs(b.d);
                break;
            }
        }
    }
    else t->interrupt(INT_INST_ILLEGAL);                   // unsupported type
    // invert result
    if (cond & 1) result ^= 1;

    // check for NAN
    if (isnan) {
        result = (cond >> 3) & 1;                          // bit 3 tells what to get if unordered
        //if (t->parm[3].i & MSK_FLOAT_NAN_LOSS) t->interrupt(INT_FLOAT_NAN_LOSS); // mask bit 29: trap if NAN loss
    }

    // mask and fallback
    //uint8_t fallbackreg = t->operands[2];
    uint8_t fallback = t->parm[3].b;
    switch (cond >> 4) {
    case 0:    // normal fallback
        if (!(mask & 1)) result = fallback;
        break;
    case 1:    // mask & result & fallback
        result &= mask & fallback;
        t->parm[3].b = 1;  break;   // prevent normal mask operation
    case 2:    // mask & (result | fallback)
        result = mask & (result | fallback);
        t->parm[3].b = 1;  break;   // prevent normal mask operation
    case 3:    // mask & (result ^ fallback)
        result = mask & (result ^ fallback);
        t->parm[3].b = 1;  break;   // prevent normal mask operation
    }
    if ((t->returnType & 7) >= 5) t->returnType -= 3;  // debug return output must be integer
    // get remaining bits from mask
    return (result & 1) | (t->parm[3].q & ~(uint64_t)1);
}

uint64_t f_add(CThread * t) {
    // add two numbers
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    uint32_t mask = t->parm[3].i;
    SNum result;
    bool roundingMode = (mask & (3 << MSKI_ROUNDING)) != 0;  // non-standard rounding mode
    bool detectExceptions = (mask & (0xF << MSKI_EXCEPTIONS)) != 0;  // make NAN if exceptions
    uint8_t operandType = t->operandType;

    if (((mask ^ t->lastMask) & MSK_SUBNORMAL) != 0) {
        // subnormal status changed
        enableSubnormals (mask & MSK_SUBNORMAL);
        t->lastMask = mask;
    }
    // operand type
    if (operandType < 4) {  // integer
        // uint64_t sizeMask = dataSizeMask[t->operandType]; // mask for data size        
        result.q = a.q + b.q;
    }
    else if (operandType == 5) {                           // float
        bool nana = isnan_f(a.i);                          // check for NAN input
        bool nanb = isnan_f(b.i);   
        if (nana && nanb) {                                // both are NAN
            return (a.i << 1) > (b.i << 1) ? a.i : b.i;    // return the biggest payload
        }
        else if (nana) return a.q;
        else if (nanb) return b.q;
        if (roundingMode) setRoundingMode(mask >> MSKI_ROUNDING);
        if (detectExceptions) clearExceptionFlags();       // clear previous exceptions
        result.f = a.f + b.f;                              // this is the actual addition
        if (isnan_f(result.i)) {
            // the result is NAN but neither input is NAN. This must be INF-INF
            result.q = t->makeNan(nan_invalid_sub, operandType);
        }
        if (detectExceptions) {
            uint32_t x = getExceptionFlags();              // read exceptions
            if ((mask & MSK_OVERFLOW) && (x & 8)) result.q = t->makeNan(nan_overflow_add, operandType);
            else if ((mask & MSK_UNDERFLOW) && (x & 0x10)) result.q = t->makeNan(nan_underflow, operandType);
            else if ((mask & MSK_INEXACT) && (x & 0x20)) result.q = t->makeNan(nan_inexact, operandType);
        }
        if (roundingMode) setRoundingMode(0);              // reset rounding mode
    }

    else if (operandType == 6) {                           // double
        bool nana = isnan_d(a.q);                          // check for NAN input
        bool nanb = isnan_d(b.q);   
        if (nana && nanb) {                                // both are NAN
            return (a.q << 1) > (b.q << 1) ? a.q : b.q;    // return the biggest payload
        }
        else if (nana) return a.q;
        else if (nanb) return b.q;
        if (roundingMode) setRoundingMode(mask >> MSKI_ROUNDING);
        if (detectExceptions) clearExceptionFlags();       // clear previous exceptions
        result.d = a.d + b.d;                              // this is the actual addition
        if (isnan_d(result.q)) {
            // the result is NAN but neither input is NAN. This must be INF-INF
            result.q = t->makeNan(nan_invalid_sub, operandType);
        }
        if (detectExceptions) {
            uint32_t x = getExceptionFlags();              // read exceptions
            if ((mask & MSK_OVERFLOW) && (x & 8)) result.q = t->makeNan(nan_overflow_add, operandType);
            else if ((mask & MSK_UNDERFLOW) && (x & 0x10)) result.q = t->makeNan(nan_underflow, operandType);
            else if ((mask & MSK_INEXACT) && (x & 0x20)) result.q = t->makeNan(nan_inexact, operandType);
        }
        if (roundingMode) setRoundingMode(0);              // reset rounding mode
    }
    else {
        // unsupported operand type
        t->interrupt(INT_INST_ILLEGAL);
        result.i = 0;
    }
    return result.q;
}

uint64_t f_sub(CThread * t) {
    // subtract two numbers
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    uint32_t mask = t->parm[3].i;
    SNum result;
    bool roundingMode = (mask & (3 << MSKI_ROUNDING)) != 0;  // non-standard rounding mode
    bool detectExceptions = (mask & (0xF << MSKI_EXCEPTIONS)) != 0;  // make NAN if exceptions
    uint8_t operandType = t->operandType;
    if (((mask ^ t->lastMask) & MSK_SUBNORMAL) != 0) {
        // subnormal status changed
        enableSubnormals (mask & MSK_SUBNORMAL);
        t->lastMask = mask;
    }
    if (operandType < 4) {      // integer
        result.q = a.q - b.q;   // subtract
    }
    else if (operandType == 5) { // float
        bool nana = isnan_f(a.i);                          // check for NAN input
        bool nanb = isnan_f(b.i);   
        if (nana && nanb) {                                // both are NAN
            return (a.i << 1) > (b.i << 1) ? a.i : b.i;    // return the biggest payload
        }
        else if (nana) return a.q;
        else if (nanb) return b.q;
        if (roundingMode) setRoundingMode(mask >> MSKI_ROUNDING);
        if (detectExceptions) clearExceptionFlags();       // clear previous exceptions
        result.f = a.f - b.f;                              // this is the actual subtraction
        if (isnan_f(result.i)) {
            // the result is NAN but neither input is NAN. This must be INF-INF
            result.q = t->makeNan(nan_invalid_sub, operandType);
        }
        if (detectExceptions) {
            uint32_t x = getExceptionFlags();              // read exceptions
            if ((mask & MSK_OVERFLOW) && (x & 8)) result.q = t->makeNan(nan_overflow_add, operandType);
            else if ((mask & MSK_UNDERFLOW) && (x & 0x10)) result.q = t->makeNan(nan_underflow, operandType);
            else if ((mask & MSK_INEXACT) && (x & 0x20)) result.q = t->makeNan(nan_inexact, operandType);
        }
        if (roundingMode) setRoundingMode(0);              // reset rounding mode
    }
    else if (operandType == 6) {// double
        bool nana = isnan_d(a.q);                          // check for NAN input
        bool nanb = isnan_d(b.q);   
        if (nana && nanb) {                                // both are NAN
            return (a.q << 1) > (b.q << 1) ? a.q : b.q;    // return the biggest payload
        }
        else if (nana) return a.q;
        else if (nanb) return b.q;
        if (roundingMode) setRoundingMode(mask >> MSKI_ROUNDING);
        if (detectExceptions) clearExceptionFlags();       // clear previous exceptions
        result.d = a.d - b.d;                              // this is the actual subtraction
        if (isnan_d(result.q)) {
            // the result is NAN but neither input is NAN. This must be INF-INF
            result.q = t->makeNan(nan_invalid_sub, operandType);
        }
        if (detectExceptions) {
            uint32_t x = getExceptionFlags();              // read exceptions
            if ((mask & MSK_OVERFLOW) && (x & 8)) result.q = t->makeNan(nan_overflow_add, operandType);
            else if ((mask & MSK_UNDERFLOW) && (x & 0x10)) result.q = t->makeNan(nan_underflow, operandType);
            else if ((mask & MSK_INEXACT) && (x & 0x20)) result.q = t->makeNan(nan_inexact, operandType);
        }
        if (roundingMode) setRoundingMode(0);              // reset rounding mode
    }
    else {
        // unsupported operand type
        t->interrupt(INT_INST_ILLEGAL);
        result.i = 0;
    }
    return result.q;
}

extern uint64_t f_sub_rev(CThread * t) {
    // subtract two numbers, b-a
    uint64_t temp = t->parm[2].q;  // swap operands
    t->parm[2].q = t->parm[1].q;
    t->parm[1].q = temp;
    uint64_t retval = f_sub(t);
    t->parm[2].q = temp;           // restore parm[2] in case it is a constant
    return retval;
}

uint64_t f_mul(CThread * t) {
    // multiply two numbers
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    uint32_t mask = t->parm[3].i;
    SNum result;
    bool roundingMode = (mask & (3 << MSKI_ROUNDING)) != 0;  // non-standard rounding mode
    bool detectExceptions = (mask & (0xF << MSKI_EXCEPTIONS)) != 0;  // make NAN if exceptions
    uint8_t operandType = t->operandType;
    if (((mask ^ t->lastMask) & MSK_SUBNORMAL) != 0) {
        // subnormal status changed
        enableSubnormals (mask & MSK_SUBNORMAL);
        t->lastMask = mask;
    }
    if (operandType < 4) {
        // integer
        result.q = a.q * b.q;
    }
    else if (operandType == 5) {                           // float
        bool nana = isnan_f(a.i);                          // check for NAN input
        bool nanb = isnan_f(b.i);   
        if (nana && nanb) {                                // both are NAN
            return (a.i << 1) > (b.i << 1) ? a.i : b.i;    // return the biggest payload
        }
        else if (nana) return a.q;
        else if (nanb) return b.q;
        if (roundingMode) setRoundingMode(mask >> MSKI_ROUNDING);
        if (detectExceptions) clearExceptionFlags();       // clear previous exceptions
        result.f = a.f * b.f;                              // this is the actual multiplication
        if (isnan_f(result.i)) {
            // the result is NAN but neither input is NAN. This must be 0*INF
            result.q = t->makeNan(nan_invalid_0mulinf, operandType);
        }
        if (detectExceptions) {
            uint32_t x = getExceptionFlags();              // read exceptions
            if ((mask & MSK_OVERFLOW) && (x & 8)) result.q = t->makeNan(nan_overflow_mul, operandType);
            else if ((mask & MSK_UNDERFLOW) && (x & 0x10)) result.q = t->makeNan(nan_underflow, operandType);
            else if ((mask & MSK_INEXACT) && (x & 0x20)) result.q = t->makeNan(nan_inexact, operandType);
        }
        if (roundingMode) setRoundingMode(0);              // reset rounding mode
    }

    else if (operandType == 6) { // double
        bool nana = isnan_d(a.q);                          // check for NAN input
        bool nanb = isnan_d(b.q);   
        if (nana && nanb) {                                // both are NAN
            return (a.q << 1) > (b.q << 1) ? a.q : b.q;    // return the biggest payload
        }
        else if (nana) return a.q;
        else if (nanb) return b.q;
        if (roundingMode) setRoundingMode(mask >> MSKI_ROUNDING);
        if (detectExceptions) clearExceptionFlags();       // clear previous exceptions
        result.d = a.d * b.d;                              // this is the actual multiplication
        if (isnan_d(result.q)) {
            // the result is NAN but neither input is NAN. This must be 0*INF
            result.q = t->makeNan(nan_invalid_0mulinf, operandType);
        }
        if (detectExceptions) {
            uint32_t x = getExceptionFlags();              // read exceptions
            if ((mask & MSK_OVERFLOW) && (x & 8)) result.q = t->makeNan(nan_overflow_mul, operandType);
            else if ((mask & MSK_UNDERFLOW) && (x & 0x10)) result.q = t->makeNan(nan_underflow, operandType);
            else if ((mask & MSK_INEXACT) && (x & 0x20)) result.q = t->makeNan(nan_inexact, operandType);
        }
        if (roundingMode) setRoundingMode(0);              // reset rounding mode
    }
    else {
        // unsupported operand type
        t->interrupt(INT_INST_ILLEGAL);
        result.i = 0;
    }
    return result.q;
}


uint64_t f_div(CThread * t) {
    // divide two floating point numbers or signed integers
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    uint32_t mask = t->parm[3].i;
    SNum result;
    bool overflow = false;
    bool roundingMode = (mask & (3 << MSKI_ROUNDING))!=0;  // non-standard floating point rounding mode
    bool detectExceptions = (mask & (0xF << MSKI_EXCEPTIONS)) != 0;  // make NAN if exceptions
    bool nana, nanb;                                       // inputs are NAN
    uint8_t operandType = t->operandType;
    uint32_t intRounding = 0;                              // integer rounding mode
    if (t->fInstr->tmpl == 0xE) {
        intRounding = t->pInstr->a.im3;                    // E template. get integer rounding mode from IM3
    }
    if (((mask ^ t->lastMask) & MSK_SUBNORMAL) != 0) {
        // subnormal status changed
        enableSubnormals (mask & MSK_SUBNORMAL);
        t->lastMask = mask;
    }
    switch (operandType) {
    case 0:  // int8
        if (b.b == 0 || (a.b == 0x80 && b.bs == -1)) {
            result.i = 0x80; overflow = true;
        }
        else {
            result.i = a.bs / b.bs;
            if (intRounding != 0 && intRounding != 7 && abs(b.bs) != 1) {
                int rem = a.bs % b.bs;
                switch (intRounding) {
                case 4: { // nearest or even
                    uint32_t r2 = 2*abs(rem);
                    uint32_t b2 = abs(b.bs);
                    int s = int8_t(a.i ^ b.i) < 0 ? -1 : 1;  // one with sign of result
                    if (r2 > b2 || (r2 == b2 && (result.b & 1))) result.i += s;
                    break;}
                case 5:  // down
                    if (rem != 0 && int8_t(a.i ^ b.i) < 0 && result.b != 0x80u) result.i--;
                    break;
                case 6:  // up
                    if (rem != 0 && int8_t(a.i ^ b.i) >= 0) result.i++;
                    break;
                }
            }
        }
        break;
    case 1:  // int16
        if (b.s == 0 || (a.s == 0x8000u && b.ss == -1)) {
            result.i = 0x8000; overflow = true;
        }
        else {
            result.i = a.ss / b.ss;
            if (intRounding != 0 && intRounding != 7 && abs(b.ss) != 1) {
                int16_t rem = a.ss % b.ss;
                switch (intRounding) {
                case 4: { // nearest or even
                    uint16_t r2 = 2*abs(rem);
                    uint16_t b2 = abs(b.is);
                    int16_t  s = int16_t(a.s ^ b.s) < 0 ? -1 : 1;  // one with sign of result
                    if (r2 > b2 || (r2 == b2 && (result.s & 1))) result.s += s;
                    break;}
                case 5:  // down
                    if (rem != 0 && int16_t(a.s ^ b.s) < 0 && result.s != 0x8000u) result.s--;
                    break;
                case 6:  // up
                    if (rem != 0 && int16_t(a.s ^ b.s) >= 0) result.s++;
                    break;
                }
            }
        }
        break;
    case 2:  // int32
        if (b.i == 0 || (a.i == sign_f && b.is == -1)) {
            result.i = sign_f; overflow = true;
        }
        else {
            result.i = a.is / b.is;
            if (intRounding != 0 && intRounding != 7 && abs(b.is) != 1) {
                int rem = a.is % b.is;
                switch (intRounding) {
                case 4: { // nearest or even
                    uint32_t r2 = 2*abs(rem);
                    uint32_t b2 = abs(b.is);
                    int s = int32_t(a.i ^ b.i) < 0 ? -1 : 1;  // one with sign of result
                    if (r2 > b2 || (r2 == b2 && (result.i & 1))) result.i += s;
                    break;}
                case 5:  // down
                    if (rem != 0 && int32_t(a.i ^ b.i) < 0 && result.i != 0x80000000u) result.i--;
                    break;
                case 6:  // up
                    if (rem != 0 && int32_t(a.i ^ b.i) >= 0) result.i++;
                    break;
                }
            }
        }
        break;
    case 3:  // int64
        if (b.q == 0 || (a.q == sign_d && b.qs == int64_t(-1))) {
            result.q = sign_d; overflow = true;
        }
        else {
            result.qs = a.qs / b.qs;
            if (intRounding != 0 && intRounding != 7 && abs(b.qs) != 1) {
                int64_t rem = a.qs % b.qs;
                switch (intRounding) {
                case 4: { // nearest or even
                    uint64_t r2 = 2*abs(rem);
                    uint64_t b2 = abs(b.qs);
                    int64_t s = int64_t(a.q ^ b.q) < 0 ? -1 : 1;  // one with sign of result
                    if (r2 > b2 || (r2 == b2 && (result.i & 1))) result.q += s;
                    break;}
                case 5:  // down
                    if (rem != 0 && int64_t(a.q ^ b.q) < 0 && result.q != 0x8000000000000000u) result.q--;
                    break;
                case 6:  // up
                    if (rem != 0 && int64_t(a.q ^ b.q) >= 0) result.q++;
                    break;
                }
            }
        }
        break;
    case 5:  // float
        nana = isnan_f(a.i);                          // check for NAN input
        nanb = isnan_f(b.i);   
        if (nana && nanb) {                                // both are NAN
            result.i = (a.i << 1) > (b.i << 1) ? a.i : b.i;    // return the biggest payload
        }
        else if (nana) result.i = a.i;
        else if (nanb) result.i = b.i;
        else if (b.i << 1 == 0) { // division by zero
            if (a.i << 1 == 0) { // 0./0. = nan
                result.q = t->makeNan(nan_invalid_0div0, operandType);
            }
            else {
                // a / 0. = infinity
                if (mask & MSK_DIVZERO) result.q = t->makeNan(nan_div0, operandType);
                else result.i = inf_f;
            }
            result.i |= (a.i ^ b.i) & sign_f; // sign bit
        }
        else if (isinf_f(a.i) && isinf_f(b.i)) {
            result.i = (uint32_t)t->makeNan(nan_invalid_divinf, operandType); // INF/INF
            result.i |= (a.i ^ b.i) & sign_f; // sign bit
        }
        else {
            if (roundingMode) setRoundingMode(mask >> MSKI_ROUNDING);
            if (detectExceptions) clearExceptionFlags();       // clear previous exceptions
            result.f = a.f / b.f;                              // normal division
            if (detectExceptions) {
                uint32_t x = getExceptionFlags();              // read exceptions
                if ((mask & MSK_OVERFLOW) && (x & 8)) result.q = t->makeNan(nan_overflow_div, operandType);
                else if ((mask & MSK_UNDERFLOW) && (x & 0x10)) result.q = t->makeNan(nan_underflow, operandType);
                else if ((mask & MSK_INEXACT) && (x & 0x20)) result.q = t->makeNan(nan_inexact, operandType);
            }
            if (roundingMode) setRoundingMode(0);              // reset rounding mode
        }
        break;
    case 6:  // double
        nana = isnan_d(a.q);                          // check for NAN input
        nanb = isnan_d(b.q);   
        if (nana && nanb) {                                // both are NAN
            result.q = (a.q << 1) > (b.q << 1) ? a.q : b.q;    // return the biggest payload
        }
        else if (nana) result.q = a.q;
        else if (nanb) result.q = b.q;
        else if (b.q << 1 == 0) { // division by zero
            if (a.q << 1 == 0) { // 0./0. = nan
                result.q = t->makeNan(nan_invalid_0div0, operandType);
            }
            else {
                // a / 0. = infinity
                if (mask & MSK_DIVZERO) result.q = t->makeNan(nan_div0, operandType);
                else result.q = inf_d;
            }
            result.q |= (a.q ^ b.q) & sign_d; // sign bit
        }
        else if (isinf_d(a.q) && isinf_d(b.q)) {
            result.q = t->makeNan(nan_invalid_divinf, operandType); // INF/INF
            result.q |= (a.q ^ b.q) & sign_d; // sign bit
        }
        else {
            if (roundingMode) setRoundingMode(mask >> MSKI_ROUNDING);
            if (detectExceptions) clearExceptionFlags();       // clear previous exceptions
            result.d = a.d / b.d;                              // normal division
            if (detectExceptions) {
                uint32_t x = getExceptionFlags();              // read exceptions
                if ((mask & MSK_OVERFLOW) && (x & 8)) result.q = t->makeNan(nan_overflow_div, operandType);
                else if ((mask & MSK_UNDERFLOW) && (x & 0x10)) result.q = t->makeNan(nan_underflow, operandType);
                else if ((mask & MSK_INEXACT) && (x & 0x20)) result.q = t->makeNan(nan_inexact, operandType);
            }
            if (roundingMode) setRoundingMode(0);              // reset rounding mode
        }
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
        result.i = 0;
    }
    return result.q;
}

uint64_t f_div_u(CThread * t) {
    // divide two unsigned numbers

    if (t->operandType > 4) {
        return f_div(t);                         // floating point: same as f_div
    }
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    SNum mask = t->parm[3];
    SNum result;
    bool overflow = false;
    uint32_t intRounding = 0;                              // integer rounding mode
    if (t->fInstr->tmpl == 0xE) {
        intRounding = t->pInstr->a.im3;                    // E template. get integer rounding mode from IM3
    }

    switch (t->operandType) {
    case 0:  // int8
        if (b.b == 0) {
            result.i = 0xFF; overflow = true;
        }
        else {
            result.i = a.b / b.b;
            if (intRounding == 4 || intRounding == 6) {
                uint32_t rem = a.b % b.b;
                switch (intRounding) {
                case 4:  // nearest or even
                    if (rem*2 > b.b || (rem*2 == b.b && (result.i & 1))) result.i++;
                    break;
                case 6:  // up
                    if (rem != 0) result.i++;
                    break;
                }
            }
        }
        break;
    case 1:  // int16
        if (b.s == 0) {
            result.i = 0xFFFF; overflow = true;
        }
        else {
            result.i = a.s / b.s;
            if (intRounding == 4 || intRounding == 6) {
                uint32_t rem = a.s % b.s;
                switch (intRounding) {
                case 4:  // nearest or even
                    if (rem*2 > b.s || (rem*2 == b.s && (result.i & 1))) result.i++;
                    break;
                case 6:  // up
                    if (rem != 0) result.i++;
                    break;
                }
            }
        }
        break;
    case 2:  // int32
        if (b.i == 0) {
            result.is = -1; overflow = true;
        }
        else {
            result.i = a.i / b.i;
            if (intRounding == 4 || intRounding == 6) {
                uint32_t rem = a.i % b.i;
                switch (intRounding) {
                case 4:  // nearest or even
                    if (rem*2 > b.i || (rem*2 == b.i && (result.i & 1))) result.i++;
                    break;
                case 6:  // up
                    if (rem != 0) result.i++;
                    break;
                }
            }
        }
        break;
    case 3:  // int64
        if (b.q == 0) {
            result.qs = -(int64_t)1; overflow = true;
        }
        else {
            result.q = a.q / b.q;
            if (intRounding == 4 || intRounding == 6) {
                uint64_t rem = a.q % b.q;
                switch (intRounding) {
                case 4:  // nearest or even
                    if (rem*2 > b.q || (rem*2 == b.q && (result.q & 1))) result.q++;
                    break;
                case 6:  // up
                    if (rem != 0) result.q++;
                    break;
                }
            }
        }
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
    }
    //if (overflow && (mask.i & MSK_OVERFL_UNSIGN)) t->interrupt(INT_OVERFL_UNSIGN);    // unsigned integer overflow
    return result.q;
}

static uint64_t f_div_rev(CThread * t) {
    // divide two numbers, b/a
    uint64_t temp = t->parm[2].q;  // swap operands
    t->parm[2].q = t->parm[1].q;
    t->parm[1].q = temp;
    uint64_t retval = f_div(t);
    t->parm[2].q = temp;           // restore parm[2] in case it is a constant
    return retval;
}

uint64_t mul64_128u(uint64_t * low, uint64_t a, uint64_t b) {
    // extended unsigned multiplication 64*64 -> 128 bits.
    // Note: you may replace this by inline assembly or intrinsic functions on
    // platforms that have extended multiplication instructions.

    // The return value is the high half of the product, 
    // *low receives the low half of the product
    union { // arrays for storing result
        uint64_t q[2];
        uint32_t i[4];
    } res;
    uint64_t t;             // temporary product
    uint64_t k;             // temporary carry
    uint64_t a0 = (uint32_t)a;     // low a
    uint64_t a1 = a >> 32;         // high a
    uint64_t b0 = (uint32_t)b;     // low b
    uint64_t b1 = b >> 32;         // high b 
    t = a0 * b0;
    res.i[0] = (uint32_t)t;
    k = t >> 32;
    t = a1 * b0 + k;
    res.i[1] = (uint32_t)t;
    k = t >> 32;
    res.i[2] = (uint32_t)k;
    t = a0 * b1 + res.i[1];
    res.i[1] = (uint32_t)t;
    k = t >> 32;
    t = a1 * b1 + k + res.i[2];
    res.i[2] = (uint32_t)t;
    k = t >> 32;
    res.i[3] = (uint32_t)k;
    if (low) *low = res.q[0];
    return res.q[1];
}

int64_t mul64_128s(uint64_t * low, int64_t a, int64_t b) {
    // extended signed multiplication 64*64 -> 128 bits.
    // Note: you may replace this by inline assembly or intrinsic functions on
    // platforms that have extended multiplication instructions.

    // The return value is the high half of the product, 
    // *low receives the low half of the product
    bool sign = false;
    if (a < 0) {
        a = -a, sign = true;
    }
    if (b < 0) {
        b = -b; sign = !sign;
    }
    uint64_t lo, hi;
    hi = mul64_128u(&lo, a, b);
    if (sign) {             // change sign
        lo = uint64_t(-int64_t(lo));
        hi = ~hi;
        if (lo == 0) hi++;  // carry
    }
    if (low) *low = lo;
    return (int64_t)hi;
}

static uint64_t f_mul_hi(CThread * t) {
    // high part of extended signed multiply
    SNum result;
    switch (t->operandType) {
    case 0:   // int8
        result.qs = ((int32_t)t->parm[1].bs * (int32_t)t->parm[2].bs) >> 8;
        break;
    case 1:   // int16
        result.qs = ((int32_t)t->parm[1].ss * (int32_t)t->parm[2].ss) >> 16;
        break;
    case 2:   // int32
        result.qs = ((int64_t)t->parm[1].is * (int64_t)t->parm[2].is) >> 32;
        break;
    case 3:   // int64
        result.qs = mul64_128s(0, t->parm[1].qs, t->parm[2].qs);
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
        result.q = 0;
    }
    return result.q;
}

static uint64_t f_mul_hi_u(CThread * t) {
    // high part of extended unsigned multiply
    SNum result;
    switch (t->operandType) {
    case 0:   // int8
        result.q = ((uint32_t)t->parm[1].b * (uint32_t)t->parm[2].b) >> 8;
        break;
    case 1:   // int16
        result.q = ((uint32_t)t->parm[1].s * (uint32_t)t->parm[2].s) >> 16;
        break;
    case 2:   // int32
        result.q = ((uint64_t)t->parm[1].i * (uint64_t)t->parm[2].i) >> 32;
        break;
    case 3:   // int64
        result.q = mul64_128u(0, t->parm[1].q, t->parm[2].q);
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
        result.q = 0;
    }
    return result.q;
}

static uint64_t f_mul_ex(CThread * t) {
    // extended signed multiply. result uses two consecutive array elements
    if (!t->vect) {
        t->interrupt(INT_INST_ILLEGAL);  return 0;
    }
    SNum result;
    switch (t->operandType) {
    case 0:   // int8
        result.is = ((int32_t)t->parm[1].bs * (int32_t)t->parm[2].bs);
        t->parm[5].is = result.is >> 8;  // store high part in parm[q]
        break;
    case 1:   // int16
        result.is = ((int32_t)t->parm[1].ss * (int32_t)t->parm[2].ss) >> 16;
        t->parm[5].is = result.is >> 16;  // store high part in parm[5]
        break;
    case 2:   // int32
        result.qs = ((int64_t)t->parm[1].is * (int64_t)t->parm[2].is) >> 32;
        t->parm[5].qs = result.qs >> 32;  // store high part in parm[5]
        break;
    case 3:   // int64
        result.qs = mul64_128s(&t->parm[5].q, t->parm[1].qs, t->parm[2].qs);
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
        result.i = 0;
    }
    return result.q;
}

static uint64_t f_mul_ex_u(CThread * t) {
    // extended unsigned multiply. result uses two consecutive array elements
    if (!t->vect) {
        t->interrupt(INT_INST_ILLEGAL);  return 0;
    }
    SNum result;
    switch (t->operandType) {
    case 0:   // int8
        result.i = ((uint32_t)t->parm[1].b * (uint32_t)t->parm[2].b);
        t->parm[5].i = result.i >> 8;  // store high part in parm[5]
        break;
    case 1:   // int16
        result.i = ((uint32_t)t->parm[1].s * (uint32_t)t->parm[2].s) >> 16;
        t->parm[5].i = result.i >> 16;  // store high part in parm[5]
        break;
    case 2:   // int32
        result.q = ((uint64_t)t->parm[1].i * (uint64_t)t->parm[2].i) >> 32;
        t->parm[5].q = result.q >> 32;  // store high part in parm[5]
        break;
    case 3:   // int64
        result.q = mul64_128u(&t->parm[5].q, t->parm[1].q, t->parm[2].q);
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
        result.i = 0;
    }
    return result.q;
}


static uint64_t f_rem(CThread * t) {
    // remainder/modulo of two signed numbers
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    SNum mask = t->parm[3];
    SNum result;
    bool overflow = false;

    switch (t->operandType) {
    case 0:  // int8
        if (b.b == 0 || (a.b == 0x80 && b.bs == -1)) {
            result.i = 0x80; overflow = true;
        }
        else result.is = a.bs % b.bs;
        break;
    case 1:  // int16
        if (b.s == 0 || (a.s == 0x8000 && b.ss == -1)) {
            result.i = 0x8000; overflow = true;
        }
        else result.is = a.ss % b.ss;
        break;
    case 2:  // int32
        if (b.i == 0 || (a.i == sign_f && b.is == -1)) {
            result.i = sign_f; overflow = true;
        }
        else result.is = a.is % b.is;
        break;
    case 3:  // int64
        if (b.q == 0 || (a.q == sign_d && b.qs == int64_t(-1))) {
            result.q = sign_d; overflow = true;
        }
        else result.qs = a.qs % b.qs;
        break;
    case 5:  // float
        if (isnan_f(a.i) && isnan_f(b.i)) {    // both are NAN
            result.i = (a.i << 1) > (b.i << 1) ? a.i : b.i; // return the biggest payload
        }
        else if (b.i << 1 == 0 || isinf_f(a.i)) { // rem(1,0) or rem(inf,1)
            result.q = t->makeNan(nan_invalid_rem, 5);
        }
        else {
            result.f = fmodf(a.f, b.f);           // normal modulo
        }
        break;
    case 6:  // double
        if (isnan_d(a.q) && isnan_d(b.q)) {    // both are NAN
            result.q = (a.q << 1) > (b.q << 1) ? a.q : b.q; // return the biggest payload
        }
        else if (b.q << 1 == 0 || isinf_d(a.q)) { // rem(1,0) or rem(inf,1)
            result.q = t->makeNan(nan_invalid_rem, 5);
        }
        else {
            result.d = fmod(a.d, b.d);           // normal modulo
        }
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
        result.i = 0;
    }
    //if (overflow&& (mask.i & MSK_OVERFL_SIGN)) t->interrupt(INT_OVERFL_SIGN);    // signed integer overflow
    return result.q;
}

static uint64_t f_rem_u(CThread * t) {
    // remainder/modulo of two unsigned numbers
    if (t->operandType > 4) return f_rem(t);  // float types use f_rem
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    SNum mask = t->parm[3];
    SNum result;
    bool overflow = false;

    switch (t->operandType) {
    case 0:  // int8
        if (b.b == 0) {
            result.i = 0x80; overflow = true;
        }
        else result.i = a.b % b.b;
        break;
    case 1:  // int16
        if (b.s == 0) {
            result.i = 0x8000; overflow = true;
        }
        else result.i = a.s % b.s;
        break;
    case 2:  // int32
        if (b.i == 0) {
            result.i = sign_f; overflow = true;
        }
        else result.i = a.i % b.i;
        break;
    case 3:  // int64
        if (b.q == 0) {
            result.q = sign_d; overflow = true;
        }
        else result.q = a.q % b.q;
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
        result.i = 0;
    }
    //if (overflow&& (mask.i & MSK_OVERFL_SIGN)) t->interrupt(INT_OVERFL_SIGN);    // signed integer overflow
    return result.q;
}

static uint64_t f_min(CThread * t) {
    // minimum of two signed numbers
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    SNum result;
    int8_t isnan;
    switch (t->operandType) {
    case 0:   // int8
        result.is = a.bs < b.bs ? a.bs : b.bs;
        break;
    case 1:   // int16
        result.is = a.ss < b.ss ? a.ss : b.ss;
        break;
    case 2:   // int32
        result.is = a.is < b.is ? a.is : b.is;
        break;
    case 3:   // int64
        result.qs = a.qs < b.qs ? a.qs : b.qs;
        break;
    case 5:   // float
        result.f = a.f < b.f ? a.f : b.f;
        // check NANs
        isnan  = isnan_f(a.i);           // a is nan
        isnan |= isnan_f(b.i) << 1;      // b is nan
        if (isnan) { // propagate NAN according to the 2019 revision of the IEEE-754 standard
            if (isnan == 1) result.i = a.i;
            else if (isnan == 2) result.i = b.i;
            else result.i = (a.i << 1) > (b.i << 1) ? a.i : b.i; // return the biggest payload
        }
        break;
    case 6:   // double
        result.d = a.d < b.d ? a.d : b.d;
        // check NANs
        isnan  = isnan_d(a.q);           // a is nan
        isnan |= isnan_d(b.q) << 1;      // b is nan
        if (isnan) { // propagate NAN according to the 2019 revision of the IEEE-754 standard
            if (isnan == 1) result.q = a.q;
            else if (isnan == 2) result.q = b.q;
            else result.q = (a.q << 1) > (b.q << 1) ? a.q : b.q; // return the biggest payload
        }
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
        result.i = 0;
    }
    return result.q;
}

static uint64_t f_min_u(CThread * t) {
    // minimum of two unsigned numbers
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    SNum result;
    switch (t->operandType) {
    case 0:   // int8
        result.i = a.b < b.b ? a.b : b.b;
        break;
    case 1:   // int16
        result.i = a.s < b.s ? a.s : b.s;
        break;
    case 2:   // int32
        result.i = a.i < b.i ? a.i : b.i;
        break;
    case 3:   // int64
        result.q = a.q < b.q ? a.q : b.q;
        break;
    case 5:   // float
    case 6:   // double
        return f_min(t);
    default:
        t->interrupt(INT_INST_ILLEGAL);
        result.i = 0;
    }
    return result.q;
}

static uint64_t f_max(CThread * t) {
    // maximum of two signed numbers
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    SNum result;
    uint8_t isnan;
    switch (t->operandType) {
    case 0:   // int8
        result.is = a.bs > b.bs ? a.bs : b.bs;
        break;
    case 1:   // int16
        result.is = a.ss > b.ss ? a.ss : b.ss;
        break;
    case 2:   // int32
        result.is = a.is > b.is ? a.is : b.is;
        break;
    case 3:   // int64
        result.qs = a.qs > b.qs ? a.qs : b.qs;
        break;
    case 5:   // float
        result.f = a.f > b.f ? a.f : b.f;
        // check NANs
        isnan  = isnan_f(a.i);           // a is nan
        isnan |= isnan_f(b.i) << 1;      // b is nan
        if (isnan) {
            // propagate NAN according to the 2019 revision of the IEEE-754 standard
            if (isnan == 1) result.i = a.i;
            else if (isnan == 2) result.i = b.i;
            else result.i = (a.i << 1) > (b.i << 1) ? a.i : b.i; // return the biggest payload
        }
        break;
    case 6:   // double
        result.d = a.d > b.d ? a.d : b.d;
        // check NANs
        isnan  = isnan_d(a.q);           // a is nan
        isnan |= isnan_d(b.q) << 1;      // b is nan
        if (isnan) {
            // propagate NAN according to the 2019 revision of the IEEE-754 standard
            if (isnan == 1) result.q = a.q;
            else if (isnan == 2) result.q = b.q;
            else result.q = (a.q << 1) > (b.q << 1) ? a.q : b.q; // return the biggest payload
        }
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
        result.i = 0;
    }
    return result.q;
}

static uint64_t f_max_u(CThread * t) {
    // maximum of two unsigned numbers
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    SNum result;
    switch (t->operandType) {
    case 0:   // int8
        result.i = a.b > b.b ? a.b : b.b;
        break;
    case 1:   // int16
        result.i = a.s > b.s ? a.s : b.s;
        break;
    case 2:   // int32
        result.i = a.i > b.i ? a.i : b.i;
        break;
    case 3:   // int64
        result.q = a.q > b.q ? a.q : b.q;
        break;
    case 5:   // float
    case 6:   // double
        return f_max(t);
    default:
        t->interrupt(INT_INST_ILLEGAL);
        result.i = 0;
    }
    return result.q;
}

static uint64_t f_and(CThread * t) {
    // bitwise AND of two numbers
    return t->parm[1].q & t->parm[2].q;
}

static uint64_t f_and_not(CThread * t) {
    // a & ~b
    return t->parm[1].q & ~ t->parm[2].q;
}

static uint64_t f_or(CThread * t) {
    // bitwise OR of two numbers
    return t->parm[1].q | t->parm[2].q;
}

static uint64_t f_xor(CThread * t) {
    // bitwise exclusive OR of two numbers
    return t->parm[1].q ^ t->parm[2].q;
}

static uint64_t f_shift_left(CThread * t) {
    // integer: a << b, float a * 2^b where b is interpreted as integer
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    if (t->fInstr->immSize && t->operandType >= 5) b = t->parm[4]; // avoid conversion of b to float
    SNum mask = t->parm[3];
    SNum result;
    uint64_t exponent;
    switch (t->operandType) {
    case 0:   // int8
        result.b = a.b << b.b;
        if (b.b > 7) result.q = 0;
        break;
    case 1:   // int16
        result.s = a.s << b.s;
        if (b.b > 15) result.q = 0;
        break;
    case 2:   // int32
        result.i = a.i << b.i;
        if (b.b > 31) result.q = 0;
        break;
    case 3:   // int64
        result.q = a.q << b.q;
        if (b.b > 63) result.q = 0;
        break;
    case 5:   // float
        if (isnan_f(a.i)) return a.q;  // a is nan
        exponent = a.i >> 23 & 0xFF;       // get exponent
        if (exponent == 0) return a.i & sign_f; // a is zero or subnormal. return zero
        exponent += b.i;                  // add integer to exponent
        if ((int32_t)exponent >= 0xFF) { // overflow
            result.i = inf_f;
        }
        else if ((int32_t)exponent <= 0) { // underflow
            if ((mask.i & MSK_UNDERFLOW) != 0) {  // make NAN if exception
                result.q = t->makeNan(nan_underflow, 5);
            }
            else {
                result.q = 0;
            }
        }
        else {
            result.i = (a.i & 0x807FFFFF) | uint32_t(exponent) << 23; // insert new exponent
        }
        break;
    case 6:   // double
        if (isnan_d(a.q)) return a.q;  // a is nan
        exponent = a.q >> 52 & 0x7FF;
        if (exponent == 0) return a.q & sign_d; // a is zero or subnormal. return zero
        exponent += b.q;                  // add integer to exponent
        if ((int64_t)exponent >= 0x7FF) { // overflow
            result.q = inf_d;
            //if (mask.i & MSK_OVERFL_FLOAT) t->interrupt(INT_OVERFL_FLOAT);
        }
        else if ((int64_t)exponent <= 0) { // underflow
            if ((mask.i & MSK_UNDERFLOW) != 0) {  // make NAN if exception
                result.q = t->makeNan(nan_underflow, 6);
            }
            else {
                result.q = 0;
            }
        }
        else {
            result.q = (a.q & 0x800FFFFFFFFFFFFF) | (exponent << 52); // insert new exponent
        }
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
        result.i = 0;
    }
    return result.q;
}

static uint64_t f_rotate(CThread * t) {
    // rotate bits left
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    if (t->fInstr->immSize && t->operandType >= 5) b = t->parm[4]; // avoid conversion of b to float
    SNum result;
    switch (t->operandType) {
    case 0:   // int8
        result.b = a.b << (b.b & 7) | a.b >> (8 - (b.b & 7));
        break;
    case 1:   // int16
        result.s = a.s << (b.s & 15) | a.s >> (16 - (b.s & 15));
        break;
    case 2:   // int32
    case 5:   // float
        result.i = a.i << (b.i & 31) | a.i >> (32 - (b.i & 31));
        break;
    case 3:   // int64
    case 6:   // double
        result.q = a.q << (b.q & 63) | a.q >> (64 - (b.q & 63));
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
        result.i = 0;
    }
    return result.q;
}

static uint64_t f_shift_right_s(CThread * t) {
    // integer only: a >> b, with sign extension
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    if (t->fInstr->immSize && t->operandType >= 5) b = t->parm[4]; // avoid conversion of b to float
    SNum result;
    switch (t->operandType) {
    case 0:   // int8
        result.bs = a.bs >> b.bs;
        if (b.b > 7) result.qs = a.bs >> 7;
        break;
    case 1:   // int16
        result.ss = a.ss >> b.ss;
        if (b.s > 15) result.qs = a.ss >> 15;
        break;
    case 2:   // int32
        result.is = a.is >> b.is;
        if (b.i > 31) result.qs = a.is >> 31;
        break;
    case 3:   // int64
        result.qs = a.qs >> b.qs;
        if (b.q > 63) result.qs = a.qs >> 63;
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
        result.i = 0;
    }
    return result.q;
}

static uint64_t f_shift_right_u(CThread * t) {
    // integer only: a >> b, with zero extension
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    if (t->fInstr->immSize && t->operandType >= 5) b = t->parm[4]; // avoid conversion of b to float
    SNum result;
    switch (t->operandType) {
    case 0:   // int8
        result.b = a.b >> b.b;
        if (b.b > 7) result.q = 0;
        break;
    case 1:   // int16
        result.s = a.s >> b.s;
        if (b.s > 15) result.q = 0;
        break;
    case 2:   // int32
        result.i = a.i >> b.i;
        if (b.i > 31) result.q = 0;
        break;
    case 3:   // int64
        result.q = a.q >> b.q;
        if (b.q > 63) result.q = 0;
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
        result.i = 0;
    }
    return result.q;
}

static uint64_t f_set_bit(CThread * t) {
    // a | 1 << b
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    if (t->fInstr->immSize && t->operandType >= 5) b = t->parm[4]; // avoid conversion of b to float
    SNum result;
    switch (t->operandType) {
    case 0:   // int8
        result.b = a.b;
        if (b.b < 8) result.b |= 1 << b.b;
        break;
    case 1:   // int16
        result.s = a.s;
        if (b.s < 16) result.s |= 1 << b.s;
        break;
    case 2:   // int32
    case 5:   // float
        result.i = a.i;
        if (b.i < 32) result.i |= 1 << b.i;
        break;
    case 3:   // int64
    case 6:   // double
        result.q = a.q;
        if (b.q < 64) result.q |= (uint64_t)1 << b.q;
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
        result.i = 0;
    }
    return result.q;
}

static uint64_t f_clear_bit(CThread * t) {
    // a & ~ (1 << b)
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    if (t->fInstr->immSize && t->operandType >= 5) b = t->parm[4]; // avoid conversion of b to float
    SNum result;
    switch (t->operandType) {
    case 0:   // int8
        result.b = a.b;
        if (b.b < 8) result.b &= ~(1 << b.b);
        break;
    case 1:   // int16
        result.s = a.s;
        if (b.s < 16) result.s &= ~(1 << b.s);
        break;
    case 2:   // int32
    case 5:   // float
        result.i = a.i;
        if (b.i < 32) result.i &= ~(1 << b.i);
        break;
    case 3:   // int64
    case 6:   // double
        result.q = a.q;
        if (b.q < 64) result.q &= ~((uint64_t)1 << b.q);
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
        result.i = 0;
    }
    return result.q;
}

static uint64_t f_toggle_bit(CThread * t) {
    // a ^ (1 << b)
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    if (t->fInstr->immSize && t->operandType >= 5) b = t->parm[4]; // avoid conversion of b to float
    SNum result;
    switch (t->operandType) {
    case 0:   // int8
        result.b = a.b;
        if (b.b < 8) result.b ^= 1 << b.b;
        break;
    case 1:   // int16
        result.s = a.s;
        if (b.s < 16) result.s ^= 1 << b.s;
        break;
    case 2:   // int32
    case 5:   // float
        result.i = a.i;
        if (b.i < 32) result.i ^= 1 << b.i;
        break;
    case 3:   // int64
    case 6:   // double
        result.q = a.q;
        if (b.q < 64) result.q ^= (uint64_t)1 << b.q;
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
        result.i = 0;
    }
    return result.q;
}

static uint64_t f_and_bit(CThread * t) {
    // clear all bits except one
    // a & (1 << b)
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    if (t->fInstr->immSize && t->operandType >= 5) b = t->parm[4]; // avoid conversion of b to float
    SNum result;
    switch (t->operandType) {
    case 0:   // int8
        result.b = a.b;
        if (b.b < 8) result.b &= 1 << b.b;
        break;
    case 1:   // int16
        result.s = a.s;
        if (b.s < 16) result.s &= 1 << b.s;
        break;
    case 2:   // int32
    case 5:   // float
        result.i = a.i;
        if (b.i < 32) result.i &= 1 << b.i;
        break;
    case 3:   // int64
    case 6:   // double
        result.q = a.q;
        if (b.q < 64) result.q &= (uint64_t)1 << b.q;
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
        result.i = 0;
    }
    return result.q;
}

static uint64_t f_test_bit(CThread * t) {
    // test a single bit: a >> b & 1
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    if (t->fInstr->immSize && t->operandType >= 5) b = t->parm[4]; // avoid conversion of b to float
    if (t->fInstr->imm2 & 4) {
        b.q = t->pInstr->a.im2;                  // avoid immediate operand shifted by imm3
    }
    SNum result;
    result.q = 0;
    SNum mask = t->parm[3];
    uint8_t fallbackreg = t->operands[2];  // fallback register
    SNum fallback;  // fallback value
    fallback.q = (fallbackreg & 0x1F) != 0x1F ? t->readRegister(fallbackreg & 0x1F) : 0;
    switch (t->operandType) {
    case 0:   // int8
        if (b.b < 8) result.b = a.b >> b.b & 1;
        break;
    case 1:   // int16
        if (b.s < 16) result.s = a.s >> b.s & 1;
        break;
    case 2:   // int32
    case 5:   // float
        if (b.i < 32) result.i = a.i >> b.i & 1;
        break;
    case 3:   // int64
    case 6:   // double
        if (b.q < 64) result.q = a.q >> b.q & 1;
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
        result.i = 0;
    }
    // get additional options
    uint8_t options = 0;
    if (t->fInstr->tmpl == 0xE && t->fInstr->mem == 0) options = t->pInstr->a.im3;
    if (options & 4) result.b ^= 1;    // invert result
    if (options & 8) fallback.b ^= 1;  // invert fallback
    if (options & 0x10) mask.b ^= 1;   // invert mask
    switch (options & 3) {
    case 0:
        result.b = mask.b ? result.b : fallback.b;
        break;
    case 1:
        result.b &= mask.b & fallback.b;
        break;
    case 2:
        result.b = mask.b  & (result.b | fallback.b);
        break;
    case 3:
        result.b = mask.b  & (result.b ^ fallback.b);
    }
    // disable normal fallback process
    t->parm[3].b = 1;
    return result.q;
}

static uint64_t f_test_bits(CThread * t) {
    // Test if at least one of the indicated bits is 1. 
    // result = (a & b) != 0
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    if (t->fInstr->immSize && t->operandType >= 5) b = t->parm[4]; // avoid conversion of b to float
    if (t->fInstr->imm2 & 4) {
        b.q = t->pInstr->a.im2;                  // avoid immediate operand shifted by imm3
    }
    SNum result;
    SNum mask = t->parm[3];
    uint8_t fallbackreg = t->operands[2];  // fallback register
    SNum fallback;  // fallback value
    fallback.q = (fallbackreg & 0x1F) != 0x1F ? t->readRegister(fallbackreg & 0x1F) : 0;
    switch (t->operandType) {
    case 0:   // int8
        result.b = (a.b & b.b) != 0;
        break;
    case 1:   // int16
        result.s = (a.s & b.s) != 0;
        break;
    case 2:   // int32
    case 5:   // float
        result.i = (a.i & b.i) != 0;
        break;
    case 3:   // int64
    case 6:   // double
        result.q = (a.q & b.q) != 0;
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
        result.i = 0;
    }
    // get additional options
    uint8_t options = 0;
    if (t->fInstr->tmpl == 0xE && t->fInstr->mem == 0) options = t->pInstr->a.im3;
    if (options & 4) result.b ^= 1;    // invert result
    if (options & 8) fallback.b ^= 1;  // invert fallback
    if (options & 0x10) mask.b ^= 1;   // invert mask
    switch (options & 3) {
    case 0:
        result.b = mask.b ? result.b : fallback.b;
        break;
    case 1:
        result.b &= mask.b & fallback.b;
        break;
    case 2:
        result.b = mask.b  & (result.b | fallback.b);
        break;
    case 3:
        result.b = mask.b  & (result.b ^ fallback.b);
    }
    // disable normal fallback process
    t->parm[3].b = 1;
    return result.q;
}

static uint64_t f_test_bits_all1(CThread * t) {
    // Test if all the indicated bits are 1
    // result = (a & b) == b
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    if (t->fInstr->immSize && t->operandType >= 5) b = t->parm[4]; // avoid conversion of b to float
    if (t->fInstr->imm2 & 4) {
        b.q = t->pInstr->a.im2;                  // avoid immediate operand shifted by imm3
    }
    SNum result;
    SNum mask = t->parm[3];
    uint8_t fallbackreg = t->operands[2];        // fallback register
    SNum fallback;  // fallback value
    fallback.q = (fallbackreg & 0x1F) != 0x1F ? t->readRegister(fallbackreg & 0x1F) : 0;
    switch (t->operandType) {
    case 0:   // int8
        result.b = (a.b & b.b) == b.b;
        break;
    case 1:   // int16
        result.s = (a.s & b.s) == b.s;
        break;
    case 2:   // int32
    case 5:   // float
        result.i = (a.i & b.i) == b.i;
        break;
    case 3:   // int64
    case 6:   // double
        result.q = (a.q & b.q) == b.q;
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
        result.i = 0;
    }
    // get additional options
    uint8_t options = 0;
    if (t->fInstr->tmpl == 0xE && t->fInstr->mem == 0) options = t->pInstr->a.im3;
    if (options & 4) result.b ^= 1;    // invert result
    if (options & 8) fallback.b ^= 1;  // invert fallback
    if (options & 0x10) mask.b ^= 1;   // invert mask
    switch (options & 3) {
    case 0:
        result.b = mask.b ? result.b : fallback.b;
        break;
    case 1:
        result.b &= mask.b & fallback.b;
        break;
    case 2:
        result.b = mask.b  & (result.b | fallback.b);
        break;
    case 3:
        result.b = mask.b  & (result.b ^ fallback.b);
    }
    // disable normal fallback process
    t->parm[3].b = 1;
    return result.q;
}

float mul_add_f(float a, float b, float c) {
    // calculate a * b + c with extra precision on the intermediate product.
#if FMA_AVAILABLE
    // use FMA instruction for correct precision if available
    return _mm_cvtss_f32(_mm_fmadd_ss(_mm_load_ss(&a), _mm_load_ss(&b), _mm_load_ss(&c)));
#else
    return float((double)a * (double)b + (double)c);
#endif
}

double mul_add_d(double a, double b, double c) {
    // calculate a * b + c with extra precision on the intermediate product.
#if FMA_AVAILABLE
    // use FMA instruction for correct precision if available
    return _mm_cvtsd_f64(_mm_fmadd_sd(_mm_load_sd(&a), _mm_load_sd(&b), _mm_load_sd(&c)));
#else
    // calculate a*b-c with extended precision. This code is not as good as the real FMA instruction
    SNum aa, bb, ahi, bhi, alo, blo;
    uint64_t upper_mask = 0xFFFFFFFFF8000000;
    aa.d = a;  bb.d = b;
    ahi.q = aa.q & upper_mask;                   // split into high and low parts
    alo.d = a - ahi.d;
    bhi.q = bb.q & upper_mask;
    blo.d = b - bhi.d;
    double r1 = ahi.d * bhi.d;                   // this product is exact
    // perhaps a different order of addition is better here in some cases?
    double r2 = r1 + c;                          // add c to high product
    double r3 = r2 + (ahi.d * blo.d + bhi.d * alo.d) + alo.d * blo.d; // add rest of product
    return r3;
#endif
}

uint64_t f_mul_add(CThread * t) {
    // a + b * c, calculated with extra precision on the intermediate product
    SNum a = t->parm[0];
    SNum b = t->parm[1];
    SNum c = t->parm[2];
    if ((t->fInstr->imm2 & 4) && t->operandType < 5) {
        c.q = t->pInstr->a.im2;                  // avoid immediate operand shifted by imm3
    }
    uint32_t mask = t->parm[3].i;
    SNum result;
    bool roundingMode = (mask & (3 << MSKI_ROUNDING)) != 0;  // non-standard rounding mode
    bool detectExceptions = (mask & (0xF << MSKI_EXCEPTIONS)) != 0;  // make NAN if exceptions

    // get sign options
    uint8_t options = 0;
    if (t->fInstr->tmpl == 0xE) options = t->pInstr->a.im3;
    else if (t->fInstr->tmpl == 0xA) options = (mask >> MSKI_OPTIONS) & 0xF;
    if (t->vect == 2) { // odd vector element
        options >>= 1;
    }
    bool unsignedOverflow = false;
    bool signedOverflow = false;
    uint8_t operandType = t->operandType;
    switch (operandType) {
    case 0:   // int8
        a.is = a.bs; b.is = b.bs;    // sign extend to avoid overflow during sign change
        if (options & 1) a.is = -a.is;
        if (options & 4) b.is = -b.is;
        result.is = a.is + b.is * c.bs;
        signedOverflow = result.is != result.bs;            
        unsignedOverflow = result.i != result.b;
        break;
    case 1:   // int16
        a.is = a.ss; b.is = b.ss;    // sign extend to avoid overflow during sign change
        if (options & 1) a.is = -a.is;
        if (options & 4) b.is = -b.is;
        result.is = a.is + b.is * c.ss;
        signedOverflow = result.is != result.ss;            
        unsignedOverflow = result.i != result.s;
        break;
    case 2:   // int32
        a.qs = a.is; b.qs = b.is;    // sign extend to avoid overflow during sign change
        if (options & 1) a.qs = -a.qs;
        if (options & 4) b.qs = -b.qs;
        result.qs = a.qs + b.qs * c.is;
        signedOverflow = result.qs != result.is;
        unsignedOverflow = result.q != result.i;
        break;
    case 3:   // int64
        if (options & 1) {
            if (a.q == sign_d) signedOverflow = true;
            a.qs = -a.qs;
        }
        if (options & 4) {
            if (b.q == sign_d) signedOverflow = true;
            b.qs = -b.qs;
        }
        result.qs = a.qs + b.qs * c.qs;
        /*
        if (mask.b & MSK_OVERFL_UNSIGN) {                  // check for unsigned overflow
            if (fabs((double)a.q + (double)b.q * (double)c.q - (double)result.q) > 1.E8) unsignedOverflow = true;
        }
        if (mask.b & MSK_OVERFL_SIGN) {                    // check for signed overflow
            if (fabs((double)a.qs + (double)b.qs * (double)c.qs - (double)result.qs) > 1.E8) signedOverflow = true;
        } */
        break;
    case 5:   // float
        if (options & 1) a.f = -a.f;
        if (options & 4) b.f = -b.f;
        if (roundingMode) setRoundingMode(mask >> MSKI_ROUNDING);
        if (detectExceptions) clearExceptionFlags();       // clear previous exceptions

        result.f = mul_add_f(c.f, b.f, a.f);               // do the calculation
        if (isnan_or_inf_f(result.i)) {                    // check for overflow and nan
            uint32_t nans = 0;                             // biggest NAN
            uint32_t infs = 0;                             // count INF inputs
            for (int i = 0; i < 3; i++) {                  // loop through input operands
                uint32_t tmp = t->parm[i].i & nsign_f;     // ignore sign bit
                if (tmp == inf_f) infs++;                  // is INF
                else if (tmp > nans) nans = tmp;           // get the biggest if there are multiple NANs
            }
            if (nans) result.i = nans;                     // there is at least one NAN. return the biggest (sign bit is lost)
            else if (isnan_f(result.i)) {
                // result is NAN, but no input is NAN. This can be 0*INF or INF-INF
                if ((a.i << 1 == 0 || b.i << 1 == 0) && infs) result.q = t->makeNan(nan_invalid_0mulinf, operandType);
                else result.q = t->makeNan(nan_invalid_sub, operandType);
            }
        }
        else if (detectExceptions) {
            uint32_t x = getExceptionFlags();              // read exceptions
            if ((mask & MSK_OVERFLOW) && (x & 8)) result.q = t->makeNan(nan_overflow_mul, operandType);
            else if ((mask & MSK_UNDERFLOW) && (x & 0x10)) result.q = t->makeNan(nan_underflow, operandType);
            else if ((mask & MSK_INEXACT) && (x & 0x20)) result.q = t->makeNan(nan_inexact, operandType);
        }
        if (roundingMode) setRoundingMode(0);              // reset rounding mode
        break;

    case 6:   // double
        if (options & 1) a.d = -a.d;
        if (options & 4) b.d = -b.d;
        if (roundingMode) setRoundingMode(mask >> MSKI_ROUNDING);
        if (detectExceptions) clearExceptionFlags();       // clear previous exceptions

        result.d = mul_add_d(c.d, b.d, a.d);               // do the calculation
        if (isnan_or_inf_d(result.q)) {                    // check for overflow and nan
            uint64_t nans = 0;                             // biggest NAN
            uint32_t infs = 0;                             // count INF inputs
            for (int i = 0; i < 3; i++) {                  // loop through input operands
                uint64_t tmp = t->parm[i].q & nsign_d;     // ignore sign bit
                if (tmp == inf_d) infs++;                  // is INF
                else if (tmp > nans) nans = tmp;           // get the biggest if there are multiple NANs
            }
            if (nans) result.q = nans;                     // there is at least one NAN. return the biggest (sign bit is lost)
            else if (isnan_d(result.q)) {
                // result is NAN, but no input is NAN. This can be 0*INF or INF-INF
                if ((a.q << 1 == 0 || b.q << 1 == 0) && infs) result.q = t->makeNan(nan_invalid_0mulinf, operandType);
                else result.q = t->makeNan(nan_invalid_sub, operandType);
            }
        }
        else if (detectExceptions) {
            uint32_t x = getExceptionFlags();              // read exceptions
            if ((mask & MSK_OVERFLOW) && (x & 8)) result.q = t->makeNan(nan_overflow_mul, operandType);
            else if ((mask & MSK_UNDERFLOW) && (x & 0x10)) result.q = t->makeNan(nan_underflow, operandType);
            else if ((mask & MSK_INEXACT) && (x & 0x20)) result.q = t->makeNan(nan_inexact, operandType);
        }
        if (roundingMode) setRoundingMode(0);              // reset rounding mode
        break;

    default:
        t->interrupt(INT_INST_ILLEGAL);
        result.i = 0;
    }
    return result.q;
}

static uint64_t f_mul_add2(CThread * t) {
    // a * b + c, calculated with extra precision on the intermediate product
    uint64_t temp = t->parm[2].q;                // swap operands
    t->parm[2].q = t->parm[0].q;
    t->parm[0].q = temp;
    uint64_t result = f_mul_add(t);              // use f_mul_add above 
    t->parm[2].q = temp;                         // restore parm[2] in case it is a constant
    return result; 
}

static uint64_t f_add_add(CThread * t) {
    // a + b + c, calculated with extra precision on the intermediate sum
    int i, j;
    SNum parm[3];
    // copy parameters so that we change sign and reorder them without changing original constant
    for (i = 0; i < 3; i++) parm[i] = t->parm[i];
    if ((t->fInstr->imm2 & 4) && t->operandType < 5) {
        parm[2].q = t->pInstr->a.im2;            // avoid immediate operand shifted by imm3
    }
    uint32_t mask = t->parm[3].i;
    bool roundingMode = (mask & (3 << MSKI_ROUNDING)) != 0;  // non-standard rounding mode
    bool detectExceptions = (mask & (0xF << MSKI_EXCEPTIONS)) != 0;  // make NAN if exceptions
    uint8_t operandType = t->operandType;
    SNum sumS, sumU;                             // signed and unsigned sums
    SNum nanS;                                   // combined nan's
    // get sign options
    uint8_t options = 0;
    if (t->fInstr->tmpl == 0xE) options = t->pInstr->a.im3;
    else if (t->fInstr->tmpl == 0xA) options = uint8_t(mask >> MSKI_OPTIONS);
    
    uint8_t signedOverflow = 0;
    uint8_t unsignedOverflow = 0;
    bool parmInf = false;
    sumS.q = sumU.q = 0;
    uint32_t temp1;
    uint64_t temp2;

    switch (operandType) {
    case 0:   // int8
        for (i = 0; i < 3; i++) {                // loop through operands
            if (options & 1) {                   // change sign
                if (parm[i].b == 0x80) signedOverflow ^= 1;
                if (parm[i].b != 0) unsignedOverflow ^= 1;
                parm[i].is = - parm[i].is;
            }
            options >>= 1;                       // get next option bit
            sumU.i  += parm[i].b;                // unsigned sum
            sumS.is += parm[i].bs;               // sign-extended sum
        }
        if (sumU.b != sumU.i) unsignedOverflow ^= 1;
        if (sumS.bs != sumS.is) signedOverflow ^= 1;
        break;
    case 1:   // int16
        for (i = 0; i < 3; i++) {                // loop through operands
            if (options & 1) {                   // change sign
                if (parm[i].s == 0x8000) signedOverflow ^= 1;
                if (parm[i].s != 0) unsignedOverflow ^= 1;
                parm[i].is = - parm[i].is;
            }
            options >>= 1;                       // get next option bit
            sumU.i  += parm[i].s;                // unsigned sum
            sumS.is += parm[i].ss;               // sign-extended sum
        }
        if (sumU.s != sumU.i) unsignedOverflow ^= 1;
        if (sumS.ss != sumS.is) signedOverflow ^= 1;
        break;
    case 2:   // int32
        for (i = 0; i < 3; i++) {                // loop through operands
            if (options & 1) {                   // change sign
                if (parm[i].i == sign_f) signedOverflow ^= 1;
                if (parm[i].i != 0) unsignedOverflow ^= 1;
                parm[i].is = - parm[i].is;
            }
            options >>= 1;                       // get next option bit
            sumU.q  += parm[i].i;                // unsigned sum
            sumS.qs += parm[i].is;               // sign-extended sum
        }
        if (sumU.i != sumU.q) unsignedOverflow ^= 1;
        if (sumS.is != sumS.qs) signedOverflow ^= 1;
        break;
    case 3:   // int64
        for (i = 0; i < 3; i++) {                // loop through operands
            if (options & 1) {                   // change sign
                if (parm[i].q == sign_d) signedOverflow ^= 1;
                if (parm[i].q != 0) unsignedOverflow ^= 1;
                parm[i].qs = - parm[i].qs;
            }
            options >>= 1;                       // get next option bit
            uint64_t a = parm[i].q;
            uint64_t b = sumU.q;
            sumU.q  = a + b;     // sum
            if (sumU.q < a) unsignedOverflow ^= 1;
            if (int64_t(~(a ^ b) & (a ^ sumU.q)) < 0) signedOverflow ^= 1;
        }
        break;
    case 5:   // float
        sumS.is = -1;  nanS.i = 0;
        for (i = 0; i < 3; i++) {                          // loop through operands
            if (options & 1) parm[i].f = -parm[i].f;       // change sign
            // find the smallest of the three operands
            if ((parm[i].i << 1) < sumS.i) {
                sumS.i = (parm[i].i << 1);  j = i;
            }
            // find NANs and infs
            temp1 = parm[i].i & nsign_f;                   // ignore sign bit
            if (temp1 > nanS.i) nanS.i = temp1;            // find the biggest NAN
            if (temp1 == inf_f) parmInf = true;            // OR of all INFs
            options >>= 1;                                 // next option bit
        }
        if (nanS.i > inf_f) return nanS.i;                 // result is NAN
        // get the smallest operand last to minimize loss of precision if the two biggest operands have opposite signs
        temp1 = parm[j].i;
        parm[j].i = parm[2].i;
        parm[2].i = temp1;

        if (roundingMode) setRoundingMode(mask >> MSKI_ROUNDING);
        if (detectExceptions) clearExceptionFlags();       // clear previous exceptions

        // calculate sum
        sumU.f = (parm[0].f + parm[1].f) + parm[2].f;

        if (isnan_f(sumU.i)) {
            // the result is NAN but neither input is NAN. This must be INF-INF
            sumU.q = t->makeNan(nan_invalid_sub, operandType);
        }
        if (detectExceptions) {
            uint32_t x = getExceptionFlags();              // read exceptions
            if ((mask & MSK_OVERFLOW) && (x & 8)) sumU.q = t->makeNan(nan_overflow_add, operandType);
            else if ((mask & MSK_UNDERFLOW) && (x & 0x10)) sumU.q = t->makeNan(nan_underflow, operandType);
            else if ((mask & MSK_INEXACT) && (x & 0x20)) sumU.q = t->makeNan(nan_inexact, operandType);
        }
        if (roundingMode) setRoundingMode(0);              // reset rounding mode
        break;

    case 6:   // double
        sumS.qs = -1;  nanS.q = 0;
        for (i = 0; i < 3; i++) {                          // loop through operands
            if (options & 1) parm[i].d = -parm[i].d;       // change sign
            // find the smallest of the three operands
            if ((parm[i].q << 1) < sumS.q) {
                sumS.q = (parm[i].q << 1);  j = i;
            }
            // find NANs and infs
            temp2 = parm[i].q & nsign_d;                   // ignore sign bit
            if (temp2 > nanS.q) nanS.q = temp2;            // find the biggest NAN
            if (temp2 == inf_d) parmInf = true;            // OR of all INFs
            options >>= 1;                                 // next option bit
        }
        if (nanS.q > inf_d) return nanS.q;                 // result is NAN

        // get the smallest operand last to minimize loss of precision if 
        // the two biggest operands have opposite signs
        temp2 = parm[j].q;
        parm[j].q = parm[2].q;
        parm[2].q = temp2;
        if (roundingMode) setRoundingMode(mask >> MSKI_ROUNDING);
        if (detectExceptions) clearExceptionFlags();       // clear previous exceptions

        // calculate sum
        sumU.d = (parm[0].d + parm[1].d) + parm[2].d;

        if (isnan_d(sumU.q)) {
            // the result is NAN but neither input is NAN. This must be INF-INF
            sumU.q = t->makeNan(nan_invalid_sub, operandType);
        }
        if (detectExceptions) {
            uint32_t x = getExceptionFlags();              // read exceptions
            if ((mask & MSK_OVERFLOW) && (x & 8)) sumU.q = t->makeNan(nan_overflow_add, operandType);
            else if ((mask & MSK_UNDERFLOW) && (x & 0x10)) sumU.q = t->makeNan(nan_underflow, operandType);
            else if ((mask & MSK_INEXACT) && (x & 0x20)) sumU.q = t->makeNan(nan_inexact, operandType);
        }
        if (roundingMode) setRoundingMode(0);              // reset rounding mode
        break;

    default:
        t->interrupt(INT_INST_ILLEGAL);
        sumU.i = 0;
    }
    return sumU.q;
}

uint64_t f_add_h(CThread * t) {
    // add two numbers, float16
    // (rounding mode not supported)
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    uint32_t mask = t->parm[3].i;
    uint16_t result;

    if (t->fInstr->immSize == 1) b.s = float2half(b.bs);  // convert 8-bit integer to float16
    if (t->operandType != 1) t->interrupt(INT_INST_ILLEGAL);
    if (isnan_h(a.s) && isnan_h(b.s)) {    // both are NAN
        result = (a.s << 1) > (b.s << 1) ? a.s : b.s; // return the biggest payload
    }
    if (mask & MSK_INEXACT) clearExceptionFlags();       // clear previous exceptions

    // the exact result is obtained with double precision. This makes sure we don't get double rounding errors
    double resultd = (double)half2float(a.s) + (double)half2float(b.s);  // calculate with single precision
    result = double2half(resultd);

    // check for exceptions
    if ((mask & MSK_OVERFLOW) && isinf_h(result) && !isinf_h(a.s) && !isinf_h(b.s)) {
        // overflow
        result = (uint16_t)t->makeNan(nan_overflow_add, 1);
        result |= (a.s ^ b.s) & 0x8000;  // get the sign
    }
    else if ((mask & MSK_UNDERFLOW) && is_zero_or_subnormal_h(result) && resultd != 0.0) {
        // underflow
        result = (uint16_t)t->makeNan(nan_underflow, 1) | (result & 0x8000); // signed NAN
    }
    else if ((mask & MSK_INEXACT) && (half2float(result) != resultd || (getExceptionFlags() & 0x20)) != 0) {
        // inexact
        result = (uint16_t)t->makeNan(nan_inexact, 1);
    }

    uint8_t roundingMode = mask >> MSKI_ROUNDING & 3;
    if (roundingMode != 0 && !isnan_or_inf_h(result)) {
        double r = half2float(result);
        // non-standard rounding mode
        switch (roundingMode) {
        case 1:  // down
            if (r > resultd && result != 0xFBFF) {
                if (result == 0) result = 0x8001;
                else if ((int16_t)result > 0) result--;
                else result++;
            }
            break;
        case 2:  // up
            if (r < resultd && result != 0x7BFF) {
                if ((int16_t)result > 0) result++;
                else result--;
            }
            break;
        case 3:  // towards zero
            if ((int16_t)result > 0 && r > resultd) result--;
            else if ((int16_t)result < 0 && r < resultd) result--;        
        }    
    }
    t->returnType =0x118;
    return result;
}

uint64_t f_sub_h(CThread * t) {
    // subtract two numbers, float16
    // (rounding mode not supported)
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    uint32_t mask = t->parm[3].i;
    uint16_t result;

    if (t->fInstr->immSize == 1) b.s = float2half(b.bs);  // convert 8-bit integer to float16
    if (t->operandType != 1) t->interrupt(INT_INST_ILLEGAL);
    if (isnan_h(a.s) && isnan_h(b.s)) {    // both are NAN
        result = (a.s << 1) > (b.s << 1) ? a.s : b.s; // return the biggest payload
    }
    if (mask & MSK_INEXACT) clearExceptionFlags();       // clear previous exceptions

    // the exact result is obtained with double precision. This makes sure we don't get double rounding errors
    double resultd = (double)half2float(a.s) - (double)half2float(b.s);  // calculate with single precision
    result = double2half(resultd);

    // check for exceptions
    if ((mask & MSK_OVERFLOW) && isinf_h(result) && !isinf_h(a.s) && !isinf_h(b.s)) {
        // overflow
        result = (uint16_t)t->makeNan(nan_overflow_add, 1);
        result |= (a.s ^ b.s) & 0x8000;  // get the sign
    }
    else if ((mask & MSK_UNDERFLOW) && is_zero_or_subnormal_h(result) && resultd != 0.0) {
        // underflow
        result = (uint16_t)t->makeNan(nan_underflow, 1) | (result & 0x8000); // signed NAN
    }
    else if ((mask & MSK_INEXACT) && (half2float(result) != resultd || (getExceptionFlags() & 0x20)) != 0) {
        // inexact
        result = (uint16_t)t->makeNan(nan_inexact, 1);
    }
    uint8_t roundingMode = mask >> MSKI_ROUNDING & 3;
    if (roundingMode != 0 && !isnan_or_inf_h(result)) {
        double r = half2float(result);
        // non-standard rounding mode
        switch (roundingMode) {
        case 1:  // down
            if (r > resultd && result != 0xFBFF) {
                if (result == 0) result = 0x8001;
                else if ((int16_t)result > 0) result--;
                else result++;
            }
            break;
        case 2:  // up
            if (r < resultd && result != 0x7BFF) {
                if ((int16_t)result > 0) result++;
                else result--;
            }
            break;
        case 3:  // towards zero
            if ((int16_t)result > 0 && r > resultd) result--;
            else if ((int16_t)result < 0 && r < resultd) result--;        
        }    
    }
    t->returnType =0x118;
    return result;
}

uint64_t f_mul_h(CThread * t) {
    // multiply two numbers, float16
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    uint32_t mask = t->parm[3].i;
    uint16_t result;

    if (t->fInstr->immSize == 1) b.s = float2half(b.bs);  // convert 8-bit integer to float16
    if (t->operandType != 1) t->interrupt(INT_INST_ILLEGAL);
    if (isnan_h(a.s) && isnan_h(b.s)) {    // both are NAN
        result = (a.s << 1) > (b.s << 1) ? a.s : b.s; // return the biggest payload
    }
    if (mask & MSK_INEXACT) clearExceptionFlags();       // clear previous exceptions

    // single precision is sufficient to get an exact multiplication result
    float resultf = half2float(a.s) * half2float(b.s);  // calculate with single precision
    result = float2half(resultf);

    // check for exceptions
    if ((mask & MSK_OVERFLOW) && isinf_h(result) && !isinf_h(a.s) && !isinf_h(b.s)) {
        // overflow
        result = (uint16_t)t->makeNan(nan_overflow_mul, 1);
        result |= (a.s ^ b.s) & 0x8000;  // get the sign
    }
    else if ((mask & MSK_UNDERFLOW) && is_zero_or_subnormal_h(result) && resultf != 0.0f) {
        // underflow
        result = (uint16_t)t->makeNan(nan_underflow, 1) | (result & 0x8000); // signed NAN
    }
    else if ((mask & MSK_INEXACT) && (half2float(result) != resultf || (getExceptionFlags() & 0x20)) != 0) {
        // inexact
        result = (uint16_t)t->makeNan(nan_inexact, 1);
    }
    uint8_t roundingMode = mask >> MSKI_ROUNDING & 3;
    if (roundingMode != 0 && !isnan_or_inf_h(result)) {
        // non-standard rounding mode
        float r = half2float(result);
        switch (roundingMode) {
        case 1:  // down
            if (r > resultf && result != 0xFBFF) {
                if (result == 0) result = 0x8001;
                else if ((int16_t)result > 0) result--;
                else result++;
            }
            break;
        case 2:  // up
            if (r < resultf && result != 0x7BFF) {
                if ((int16_t)result > 0) result++;
                else result--;
            }
            break;
        case 3:  // towards zero
            if ((int16_t)result > 0 && r > resultf) result--;
            else if ((int16_t)result < 0 && r < resultf) result--;        
        }    
    }
    t->returnType =0x118;
    return result;
}


uint64_t f_mul_add_h(CThread * t) {
    // a + b * c, float16
    SNum a = t->parm[0];
    SNum b = t->parm[1];
    SNum c = t->parm[2]; 
    uint32_t mask = t->parm[3].i;
    if (t->fInstr->imm2 & 4) c = t->parm[4];          // avoid immediate operand shifted by imm3
    if (t->fInstr->immSize == 1) c.s = float2half(c.bs);  // convert 8-bit integer to float16                                                          // get sign options
    uint8_t options = 0;
    if (t->fInstr->tmpl == 0xE) options = t->pInstr->a.im3;
    else if (t->fInstr->tmpl == 0xA) options = (mask >> MSKI_OPTIONS) & 0xF;
    if (t->vect == 2) { // odd vector element
        options >>= 1;
    }
    if (t->operandType != 1) t->interrupt(INT_INST_ILLEGAL);
    if (options & 1) a.s ^= 0x8000;                           // adjust sign
    if (options & 4) b.s ^= 0x8000;

    if (mask & MSK_INEXACT) clearExceptionFlags();         // clear previous exceptions

    double resultd = (double)half2float(a.s) + (double)half2float(b.s) * (double)half2float(c.s);

    uint16_t result = double2half(resultd);
    uint32_t nans = 0;  bool parmInf = false;

    if (isnan_or_inf_h(result)) {                          // check for overflow and nan
        for (int i = 0; i < 3; i++) {                      // loop through input operands
            uint32_t tmp = t->parm[i].s & 0x7FFF;          // ignore sign bit
            if (tmp > nans) nans = tmp;                    // get the biggest if there are multiple NANs
            if (tmp == inf_h) parmInf = true;              // OR of all INFs
        }
        if (nans > inf_h) return nans;                     // there is at least one NAN. return the biggest (sign bit is lost)
        else if (isnan_h(result)) {
            // result is NAN, but no input is NAN. This can be 0*INF or INF-INF
            if ((a.s << 1 == 0 || b.s << 1 == 0) && parmInf) result = (uint16_t)t->makeNan(nan_invalid_0mulinf, 1);
            else result = (uint16_t)t->makeNan(nan_invalid_sub, 1);
        }
    }
    else if ((mask & MSK_OVERFLOW) && isinf_h(result) && !parmInf) result = (uint16_t)t->makeNan(nan_overflow_mul, 1);
    else if ((mask & MSK_UNDERFLOW) && is_zero_or_subnormal_h(result) && resultd != 0.0) result = (uint16_t)t->makeNan(nan_underflow, 1);
    else if ((mask & MSK_INEXACT) && ((getExceptionFlags() & 0x20) != 0 || half2float(result) != resultd)) result = (uint16_t)t->makeNan(nan_inexact, 1);

    uint8_t roundingMode = mask >> MSKI_ROUNDING & 3;
    if (roundingMode != 0 && !isnan_or_inf_h(result)) {
        float r = half2float(result);
        // non-standard rounding mode
        switch (roundingMode) {
        case 1:  // down
            if (r > resultd && result != 0xFBFF) {
                if (result == 0) result = 0x8001;
                else if ((int16_t)result > 0) result--;
                else result++;
            }
            break;
        case 2:  // up
            if (r < resultd && result != 0x7BFF) {
                if ((int16_t)result > 0) result++;
                else result--;
            }
            break;
        case 3:  // towards zero
            if ((int16_t)result > 0 && r > resultd) result--;
            else if ((int16_t)result < 0 && r < resultd) result--;        
        }    
    }
    t->returnType =0x118;
    return result;
}


// Tables of function pointers
// tiny instructions
PFunc funcTab1[32] = {
    t_nop, t_move_iu, t_add, t_sub, t_shift_left, t_shift_right_u, 0, 0,                 // 0 - 7
    t_move_r, t_add_r, t_sub_r, t_and_r, t_or_r, t_xor_r, t_read_r, t_write_r,           // 8 - 15
    t_clear, t_move_v, t_move_uf, t_move_ud, t_add_f, t_add_d, t_sub_f, t_sub_d,         // 16 - 23
    t_mul_f, t_mul_d, 0, 0, t_add_cps, t_sub_cps, t_restore_cp, t_save_cp                // 24 - 31
}; 

// multiformat instructions
PFunc funcTab2[64] = {
    f_nop, f_store, f_move, f_prefetch, f_sign_extend, f_sign_extend_add, 0, f_compare,  // 0 - 7
    f_add, f_sub, f_sub_rev, f_mul, f_mul_hi, f_mul_hi_u, f_mul_ex, f_mul_ex_u,          // 8 - 15
    f_div, f_div_u, f_div_rev, 0, f_rem, f_rem_u, f_min, f_min_u,                        // 16 - 23
    f_max, f_max_u, 0, 0, f_and, f_and_not, f_or, f_xor,                                 // 24 - 31
    f_shift_left, f_rotate, f_shift_right_s, f_shift_right_u, f_set_bit, f_clear_bit, f_toggle_bit, f_and_bit, // 32 - 39
    f_test_bit, f_test_bits, f_test_bits_all1, 0, f_add_h, f_sub_h, f_mul_h, 0,          // 40 - 47
    f_mul_add_h, f_mul_add, f_mul_add2, f_add_add, 0, 0, 0, 0,                           // 48 - 55
    0, 0, 0, 0, 0, 0, 0, 0                                                               // 56 - 63
};

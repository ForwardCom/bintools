/****************************  emulator4.cpp  ********************************
* Author:        Agner Fog
* date created:  2018-02-18
* Last modified: 2018-03-30
* Version:       1.01
* Project:       Binary tools for ForwardCom instruction set
* Description:
* Emulator: Execution functions for tiny instructions and multiformat instructions
*
* Copyright 2018 GNU General Public License http://www.gnu.org/licenses
*****************************************************************************/

#include "stdafx.h"

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
    if (t->numContr & MSK_OVERFL_I) {  // check for overflow
        if ((t->numContr & MSK_OVERFL_UNSIGN) && result < a) t->interrupt(INT_OVERFL_UNSIGN);  // unsigned overflow
        if ((t->numContr & MSK_OVERFL_SIGN) && (result & ~a) >> 63) t->interrupt(INT_OVERFL_SIGN);  // signed overflow
    }
    t->returnType = 0x13;                        // debug return output
    return result;
}

static uint64_t t_sub(CThread * t) {
    // RD -= unsigned constant RS
    uint64_t a = t->registers[t->operands[4]];
    uint64_t b = t->parm[2].q;
    uint64_t result = a - b;
    if (t->numContr & MSK_OVERFL_I) {  // check for overflow
        if ((t->numContr & MSK_OVERFL_UNSIGN) && result > a) t->interrupt(INT_OVERFL_UNSIGN);  // unsigned overflow
        if ((t->numContr & MSK_OVERFL_SIGN) && (a & ~result) >> 63) t->interrupt(INT_OVERFL_SIGN);  // signed overflow
    }
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
    if (t->numContr & MSK_OVERFL_I) {  // check for overflow
        if ((t->numContr & MSK_OVERFL_UNSIGN) && result < a) t->interrupt(INT_OVERFL_UNSIGN);  // unsigned overflow
        if ((t->numContr & MSK_OVERFL_SIGN) && 
            int64_t(~(a ^ b) & (a ^ result)) < 0) t->interrupt(INT_OVERFL_SIGN);  // signed overflow
    }
    t->returnType = 0x13;                        // debug return output
    return result;
}

static uint64_t t_sub_r(CThread * t) {
    // RD -= register operand RS
    uint64_t a = t->registers[t->operands[4]];
    uint64_t b = t->registers[t->operands[5]];
    uint64_t result = a - b;
    if (t->numContr & MSK_OVERFL_I) {  // check for overflow
        if ((t->numContr & MSK_OVERFL_UNSIGN) && result < a) t->interrupt(INT_OVERFL_UNSIGN);  // unsigned overflow
        if ((t->numContr & MSK_OVERFL_SIGN) && 
            int64_t((a ^ b) & (a ^ result)) < 0) t->interrupt(INT_OVERFL_SIGN);  // signed overflow
    }
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


// Multi-format instructions
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
        if (t->parm[3].i & MSK_FLOAT_NAN_LOSS) t->interrupt(INT_FLOAT_NAN_LOSS); // mask bit 29: trap if NAN loss
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
    SNum mask = t->parm[3];
    SNum result;
    if (t->operandType < 4) {
        uint64_t sizeMask = dataSizeMask[t->operandType]; // mask for data size

        // integer
        result.q = a.q + b.q;
        if (mask.b & MSK_OVERFL_I) {
            // check for overflow
            if (mask.b & MSK_OVERFL_UNSIGN) {  // check for unsigned overflow
                bool unsignedOverflow = false;
                unsignedOverflow = (result.q & sizeMask) < (a.q & sizeMask);
                if (unsignedOverflow) t->interrupt(INT_OVERFL_UNSIGN);
            }
            if (mask.b & MSK_OVERFL_SIGN) {  // check for signed overflow
                bool signedOverflow = false;
                SNum ofl;
                // overflow if a and b have same sign and result has opposite sign
                ofl.q = ~(a.q ^ b.q) & (a.q ^ result.q);
                uint64_t signBit = (sizeMask >> 1) + 1;      // sign bit
                signedOverflow = (ofl.q & signBit) != 0;
                if (signedOverflow) t->interrupt(INT_OVERFL_SIGN);
            }
        }
    }
    else if (t->operandType == 5) {
        // float
        result.f = a.f + b.f;
        if (isnan_f(a.i) && isnan_f(b.i)) {    // both are NAN
            result.i = (a.i << 1) > (b.i << 1) ? a.i : b.i; // return the biggest payload. (current systems don't do this)
        }
        if (mask.i & MSK_OVERFL_FLOAT) {  // trap if overflow
            if (isinf_f(result.i) && !isinf_f(a.i) && !isinf_f(b.i)) {
                t->interrupt(INT_OVERFL_FLOAT);
            }
        }
    }
    else if (t->operandType == 6) {
        // double
        result.d = a.d + b.d;
        if (isnan_d(a.q) && isnan_d(b.q)) {    // both are NAN
            result.q = (a.q << 1) > (b.q << 1) ? a.q : b.q; // return the biggest payload. (current systems don't do this)
        }
        if (mask.i & MSK_OVERFL_FLOAT) {  // trap if overflow
            if (isinf_d(result.q) && !isinf_d(a.q) && !isinf_d(b.q)) {
                t->interrupt(INT_OVERFL_FLOAT);
            }
        }
    }
    else {
        // unsupported operand type
        t->interrupt(INT_INST_ILLEGAL);
    }
    return result.q;
}

uint64_t f_sub(CThread * t) {
    // subtract two numbers
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    SNum mask = t->parm[3];
    SNum result;
    uint64_t sizeMask = dataSizeMask[t->operandType]; // mask for data size

    if (t->operandType < 4) {
        // integer
        result.q = a.q - b.q;   // subtract
        if (mask.b & MSK_OVERFL_I) {
            // check for overflow
            if (mask.b & MSK_OVERFL_UNSIGN) {  // check for unsigned overflow
                bool unsignedOverflow = (result.q & sizeMask) > (a.q & sizeMask);
                if (unsignedOverflow) t->interrupt(INT_OVERFL_UNSIGN);
            }
            if (mask.b & MSK_OVERFL_SIGN) {  // check for signed overflow
                SNum ofl;
                // overflow if a and b have opposite sign and result has opposite sign of a
                ofl.q = (a.q ^ b.q) & (a.q ^ result.q);
                uint64_t signBit = (sizeMask >> 1) + 1;      // sign bit
                bool signedOverflow = (ofl.qs & signBit) != 0;
                if (signedOverflow) t->interrupt(INT_OVERFL_SIGN);
            }
        }
    }
    else if (t->operandType == 5) {
        // float
        result.f = a.f - b.f;
        if (isnan_f(a.i) && isnan_f(b.i)) {    // both are NAN
            result.i = (a.i << 1) > (b.i << 1) ? a.i : b.i; // return the biggest payload. (current systems don't do this)
        }
        if (mask.i & MSK_OVERFL_FLOAT) {  // trap if overflow
            if (isinf_f(result.i) && !isinf_f(a.i) && !isinf_f(b.i)) {
                t->interrupt(INT_OVERFL_FLOAT);
            }
        }
    }
    else if (t->operandType == 6) {
        // double
        result.d = a.d - b.d;
        if (isnan_d(a.q) && isnan_d(b.q)) {    // both are NAN
            result.q = (a.q << 1) > (b.q << 1) ? a.q : b.q; // return the biggest payload. (current systems don't do this)
        }
        if (mask.i & MSK_OVERFL_FLOAT) {  // trap if overflow
            if (isinf_d(result.q) && !isinf_d(a.q) && !isinf_d(b.q)) {
                t->interrupt(INT_OVERFL_FLOAT);
            }
        }
    }
    else {
        // unsupported operand type
        t->interrupt(INT_INST_ILLEGAL);
    }
    return result.q;
}

extern uint64_t f_sub_rev(CThread * t) {
    // subtract two numbers, b-a
    uint64_t temp = t->parm[1].q;  // swap operands
    t->parm[1].q = t->parm[2].q;
    t->parm[2].q = temp;
    return f_sub(t);
}

uint64_t f_mul(CThread * t) {
    // multiply two numbers
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    SNum mask = t->parm[3];
    SNum result;
    bool signedOverflow = false;
    bool unsignedOverflow = false;
    if (t->operandType < 4) {
        // integer
        result.q = a.q * b.q;
        if (mask.b & MSK_OVERFL_I) {
            // check for signed overflow
            if (mask.b & MSK_OVERFL_SIGN) {
                switch (t->operandType) {
                case 0:
                    signedOverflow = result.bs != result.is;  break;
                case 1:
                    signedOverflow = result.ss != result.is;  break;
                case 2:
                    signedOverflow = result.is != result.qs;  break;
                case 3:
                    signedOverflow = fabs((double)a.qs * (double)b.qs - (double)result.qs) > 1.E8; 
                    break;
                }
                if (signedOverflow) t->interrupt(INT_OVERFL_SIGN);
            }
            // check for unsigned overflow (signed overflow takes precedence if both occur)
            if ((mask.b & MSK_OVERFL_UNSIGN) && !signedOverflow) {
                switch (t->operandType) {
                case 0:
                    unsignedOverflow = result.b != result.i;  break;
                case 1:
                    unsignedOverflow = result.s != result.i;  break;
                case 2:
                    unsignedOverflow = result.i != result.q;  break;
                case 3:
                    unsignedOverflow = fabs((double)a.q * (double)b.q - (double)result.q) > 1.E8; 
                    break;
                }
                if (unsignedOverflow) t->interrupt(INT_OVERFL_UNSIGN);
            }
        }
    }
    else if (t->operandType == 5) {
        // float
        result.f = a.f * b.f;
        if (isnan_f(a.i) && isnan_f(b.i)) {    // both are NAN
            result.i = (a.i << 1) > (b.i << 1) ? a.i : b.i; // return the biggest payload. (current systems don't do this)
        }
        if (mask.i & MSK_OVERFL_FLOAT) {  // trap if overflow
            if (isinf_f(result.i) && !isinf_f(a.i) && !isinf_f(b.i)) {
                t->interrupt(INT_OVERFL_FLOAT);
            }
        }
    }
    else if (t->operandType == 6) {
        // double
        result.d = a.d * b.d;
        if (isnan_d(a.q) && isnan_d(b.q)) {    // both are NAN
            result.q = (a.q << 1) > (b.q << 1) ? a.q : b.q; // return the biggest payload. (current systems don't do this)
        }
        if (mask.i & MSK_OVERFL_FLOAT) {  // trap if overflow
            if (isinf_d(result.q) && !isinf_d(a.q) && !isinf_d(b.q)) {
                t->interrupt(INT_OVERFL_FLOAT);
            }
        }
    }
    else {
        // unsupported operand type
        t->interrupt(INT_INST_ILLEGAL);
    }
    return result.q;
}

uint64_t f_div(CThread * t) {
    // divide two signed numbers
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    SNum mask = t->parm[3];
    SNum result;
    bool overflow = false;

    // to do: rounding mode!!

    switch (t->operandType) {
    case 0:  // int8
        if (b.b == 0 || (a.b == 0x80 && b.bs == -1)) {
            result.i = 0x80; overflow = true;
        }
        else result.i = a.bs / b.bs;
        break;
    case 1:  // int16
        if (b.s == 0 || (a.s == 0x8000 && b.s == -1)) {
            result.i = 0x8000; overflow = true;
        }
        else result.i = a.ss / b.ss;
        break;
    case 2:  // int32
        if (b.i == 0 || (a.i == sign_f && b.is == -1)) {
            result.i = sign_f; overflow = true;
        }
        else result.i = a.is / b.is;
        break;
    case 3:  // int64
        if (b.q == 0 || (a.q == sign_d && b.qs == int64_t(-1))) {
            result.q = sign_d; overflow = true;
        }
        else result.qs = a.qs / b.qs;
        break;
    case 5:  // float
        if (b.i << 1 == 0) { // division by zero
            if (a.i << 1 == 0) { // 0./0. = nan
                result.i = nan_f;
            }
            else {
                result.i = inf_f; // a / 0. = infinity
                overflow = true;
            }
            result.i |= (a.i ^ b.i) & sign_f; // sign bit
        }
        else {
            result.f = a.f / b.f;           // normal division
        }
        if (isnan_f(a.i) && isnan_f(b.i)) {    // both are NAN
            result.i = (a.i << 1) > (b.i << 1) ? a.i : b.i; // return the biggest payload. (current systems don't do this)
        }
        break;
    case 6:  // double
        if (b.q << 1 == 0) { // division by zero
            if (a.q << 1 == 0) { // 0./0. = nan
                result.q = nan_d;
            }
            else {
                result.q = inf_d; // a / 0. = infinity
                overflow = true;
            }
            result.q |= (a.q ^ b.q) & sign_d; // sign bit
        }
        else {
            result.d = a.d / b.d;           // normal division
        }
        if (isnan_d(a.q) && isnan_d(b.q)) {    // both are NAN
            result.q = (a.q << 1) > (b.q << 1) ? a.q : b.q; // return the biggest payload. (current systems don't do this)
        }
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
    }
    if (overflow) {     // catch overflow
        if ((mask.i & MSK_OVERFL_FLOAT) && t->operandType >= 5) t->interrupt(INT_OVERFL_FLOAT); // floating point overflow
        if ((mask.i & MSK_OVERFL_SIGN) && t->operandType < 5) t->interrupt(INT_OVERFL_SIGN);    // signed integer overflow
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

    switch (t->operandType) {
    case 0:  // int8
        if (b.b == 0) {
            result.i = 0xFF; overflow = true;
        }
        else result.i = a.b / b.b;
        break;
    case 1:  // int16
        if (b.s == 0) {
            result.i = 0xFFFF; overflow = true;
        }
        else result.i = a.s / b.s;
        break;
    case 2:  // int32
        if (b.i == 0) {
            result.i = -1; overflow = true;
        }
        else result.i = a.i / b.i;
        break;
    case 3:  // int64
        if (b.q == 0) {
            result.q = -(int64_t)1; overflow = true;
        }
        else result.qs = a.q / b.q;
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
    }
    if (overflow && (mask.i & MSK_OVERFL_UNSIGN)) t->interrupt(INT_OVERFL_UNSIGN);    // unsigned integer overflow
    return result.q;
}

static uint64_t f_div_rev(CThread * t) {
    // divide two numbers, b/a
    uint64_t temp = t->parm[1].q;  // swap operands
    t->parm[1].q = t->parm[2].q;
    t->parm[2].q = temp;
    return f_div(t);
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
        result.q = 0;
        t->interrupt(INT_INST_ILLEGAL);
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
        if (b.s == 0 || (a.s == 0x8000 && b.s == -1)) {
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
        if (b.i << 1 == 0) { // division by zero
            result.i = nan_f;
        }
        else {
            result.f = fmodf(a.f, b.f);           // normal modulo
        }
        if (isnan_f(a.i) && isnan_f(b.i)) {    // both are NAN
            result.i = (a.i << 1) > (b.i << 1) ? a.i : b.i; // return the biggest payload. (current systems don't do this)
        }
        break;
    case 6:  // double
        if (b.q << 1 == 0) { // division by zero
            result.q = nan_d;
        }
        else {
            result.d = fmod(a.d, b.d);           // normal modulo
        }
        if (isnan_d(a.q) && isnan_d(b.q)) {    // both are NAN
            result.q = (a.q << 1) > (b.q << 1) ? a.q : b.q; // return the biggest payload. (current systems don't do this)
        }
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
    }
    if (overflow&& (mask.i & MSK_OVERFL_SIGN)) t->interrupt(INT_OVERFL_SIGN);    // signed integer overflow
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
    }
    if (overflow&& (mask.i & MSK_OVERFL_SIGN)) t->interrupt(INT_OVERFL_SIGN);    // signed integer overflow
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
        if (isnan) {
            if (true) {  // improved NAN propagation according to the forthcoming IEEE 754 standard revision
                if (isnan == 1) result.i = a.i;
                else if (isnan == 2) result.i = b.i;
                else result.i = (a.i << 1) > (b.i << 1) ? a.i : b.i; // return the biggest payload
            }
            else { // The unfortunate IEEE 754-2008 standard says return the non-nan operand!
                if (isnan == 1) result.i = b.i;
                else result.i = a.i;
            }
        }
        break;
    case 6:   // double
        result.d = a.d < b.d ? a.d : b.d;
        // check NANs
        isnan  = isnan_d(a.q);           // a is nan
        isnan |= isnan_d(b.q) << 1;      // b is nan
        if (isnan) {
            if (true) {  // improved NAN propagation according to the forthcoming IEEE 754 standard revision
                if (isnan == 1) result.q = a.q;
                else if (isnan == 2) result.q = b.q;
                else result.q = (a.q << 1) > (b.q << 1) ? a.q : b.q; // return the biggest payload
            }
            else { // The unfortunate IEEE 754-2008 standard says return the non-nan operand!
                if (isnan == 1) result.q = b.q;
                else result.q = a.q;
            }
        }
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
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
            if (true) {  // improved NAN propagation according to the forthcoming IEEE 754 standard revision
                if (isnan == 1) result.i = a.i;
                else if (isnan == 2) result.i = b.i;
                else result.i = (a.i << 1) > (b.i << 1) ? a.i : b.i; // return the biggest payload
            }
            else { // The unfortunate IEEE 754-2008 standard says return the non-nan operand!
                if (isnan == 1) result.i = b.i;
                else result.i = a.i;
            }
        }
        break;
    case 6:   // double
        result.d = a.d > b.d ? a.d : b.d;
        // check NANs
        isnan  = isnan_d(a.q);           // a is nan
        isnan |= isnan_d(b.q) << 1;      // b is nan
        if (isnan) {
            if (true) {  // improved NAN propagation according to the forthcoming IEEE 754 standard revision
                if (isnan == 1) result.q = a.q;
                else if (isnan == 2) result.q = b.q;
                else result.q = (a.q << 1) > (b.q << 1) ? a.q : b.q; // return the biggest payload
            }
            else { // The unfortunate IEEE 754-2008 standard says return the non-nan operand!
                if (isnan == 1) result.q = b.q;
                else result.q = a.q;
            }
        }
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
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
    if (t->fInstr->immSize) b = t->parm[4]; // avoid conversion of b to float
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
            if (mask.i & MSK_OVERFL_FLOAT) t->interrupt(INT_OVERFL_FLOAT);
        }
        else if ((int32_t)exponent <= 0) { // underflow
            result.i = 0;
            if (mask.i & MSK_FLOAT_UNDERFL) t->interrupt(INT_FLOAT_UNDERFL);
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
            if (mask.i & MSK_OVERFL_FLOAT) t->interrupt(INT_OVERFL_FLOAT);
        }
        else if ((int64_t)exponent <= 0) { // underflow
            result.q = 0;
            if (mask.i & MSK_FLOAT_UNDERFL) t->interrupt(INT_FLOAT_UNDERFL);
        }
        else {
            result.q = (a.q & 0x800FFFFFFFFFFFFF) | (exponent << 52); // insert new exponent
        }
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
    }
    return result.q;
}

static uint64_t f_rotate(CThread * t) {
    // rotate bits left
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    if (t->fInstr->immSize) b = t->parm[4]; // avoid conversion of b to float
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
    }
    return result.q;
}

static uint64_t f_shift_right_s(CThread * t) {
    // integer only: a >> b, with sign extension
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    if (t->fInstr->immSize) b = t->parm[4]; // avoid conversion of b to float
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
    }
    return result.q;
}

static uint64_t f_shift_right_u(CThread * t) {
    // integer only: a >> b, with zero extension
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    if (t->fInstr->immSize) b = t->parm[4]; // avoid conversion of b to float
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
    }
    return result.q;
}

static uint64_t f_set_bit(CThread * t) {
    // a | 1 << b
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    if (t->fInstr->immSize) b = t->parm[4]; // avoid conversion of b to float
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
    }
    return result.q;
}

static uint64_t f_clear_bit(CThread * t) {
    // a & ~ (1 << b)
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    if (t->fInstr->immSize) b = t->parm[4]; // avoid conversion of b to float
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
    }
    return result.q;
}

static uint64_t f_toggle_bit(CThread * t) {
    // a ^ (1 << b)
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    if (t->fInstr->immSize) b = t->parm[4]; // avoid conversion of b to float
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
    }
    return result.q;
}

static uint64_t f_and_bit(CThread * t) {
    // clear all bits except one
    // a & (1 << b)
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    if (t->fInstr->immSize) b = t->parm[4]; // avoid conversion of b to float
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
    }
    return result.q;
}

static uint64_t f_test_bit(CThread * t) {
    // test a single bit: a >> b & 1
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    if (t->fInstr->immSize) b = t->parm[4]; // avoid conversion of b to float
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
    if (t->fInstr->immSize) b = t->parm[4]; // avoid conversion of b to float
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
    if (t->fInstr->immSize) b = t->parm[4];      // avoid conversion of b to float
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
    // Note: you may replace this by inline assembly or intrinsic functions on
    // platforms that have FMA instructions
    return float((double)a * (double)b + (double)c);
}

double mul_add_d(double a, double b, double c) {
    // calculate a * b + c with extra precision on the intermediate product.
    // Note: you may replace this by inline assembly or intrinsic functions on
    // platforms that have FMA instructions
    SNum aa, bb, ahi, bhi, alo, blo;
    uint64_t upper_mask = 0xFFFFFFFFF8000000;
    aa.d = a;  bb.d = b;
    ahi.q = aa.q & upper_mask;                   // split into high and low parts
    alo.d = a - ahi.d;
    bhi.q = bb.q & upper_mask;
    blo.d = b - bhi.d;
    double r1 = ahi.d * bhi.d;                   // this product is exact
    double r2 = r1 + c;                          // add c to high product
    double r3 = r2 + (ahi.d * blo.d + bhi.d * alo.d) + alo.d * blo.d; // add rest of product
    return r3;
}

uint64_t f_mul_add(CThread * t) {
    // a + b * c, calculated with extra precision on the intermediate product
    SNum a = t->parm[0];
    SNum b = t->parm[1];
    SNum c = t->parm[2];
    SNum mask = t->parm[3];
    SNum result;
    if ((t->fInstr->imm2 & 4) && t->operandType < 5) {
        c.q = t->pInstr->a.im2;                  // avoid immediate operand shifted by imm3
    }
    // get sign options
    uint8_t options = 0;
    if (t->fInstr->tmpl == 0xE) options = t->pInstr->a.im3;
    else if (t->fInstr->tmpl == 0xA) options = (mask.i >> 10) & 0xF;
    if (t->vect == 2) { // odd vector element
        options >>= 1;
    }
    bool unsignedOverflow = false;
    bool signedOverflow = false;

    switch (t->operandType) {
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
        if (mask.b & MSK_OVERFL_UNSIGN) {                  // check for unsigned overflow
            if (fabs((double)a.q + (double)b.q * (double)c.q - (double)result.q) > 1.E8) unsignedOverflow = true;
        }
        if (mask.b & MSK_OVERFL_SIGN) {                    // check for signed overflow
            if (fabs((double)a.qs + (double)b.qs * (double)c.qs - (double)result.qs) > 1.E8) signedOverflow = true;
        }
        break;
    case 5:   // float
        if (options & 1) a.f = -a.f;
        if (options & 4) b.f = -b.f;
        result.f = mul_add_f(c.f, b.f, a.f);
        if (result.i << 1 >= inf2f) {                      // check for overflow and nan
            uint32_t nans = 0;  bool parmInf = false;
            for (int i = 0; i < 3; i++) {                  // loop through input operands
                uint32_t tmp = t->parm[i].i & nsign_f;     // ignore sign bit
                if (tmp > nans) nans = tmp;                // get the biggest if there are multiple NANs
                if (tmp == inf_f) parmInf = true;          // OR of all INFs
            }
            if (nans > inf_f) return nans;                 // there is at least one NAN. return the biggest (sign bit is lost)
            else if ((mask.i & MSK_OVERFL_FLOAT) && !parmInf) t->interrupt(INT_OVERFL_FLOAT);
        }
        break;
    case 6:   // double
        if (options & 1) a.d = -a.d;
        if (options & 4) b.d = -b.d;
        result.d = mul_add_d(c.d, b.d, a.d);
        if ((result.q & nsign_d) >= inf_d) {               // check for overflow and nan
            uint64_t nans = 0;  bool parmInf = false;
            for (int i = 0; i < 3; i++) {                  // loop through input operands
                uint64_t tmp = t->parm[i].q & nsign_d;     // ignore sign bit
                if (tmp > nans) nans = tmp;                // get the biggest if there are multiple NANs
                if (tmp == inf_d) parmInf = true;          // OR of all INFs
            }
            if (nans > inf_d) return nans;                 // there is at least one NAN. return the biggest (sign bit is lost)
            else if ((mask.i & MSK_OVERFL_FLOAT) && !parmInf) t->interrupt(INT_OVERFL_FLOAT);
        }
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
    }
    if ((mask.b & MSK_OVERFL_SIGN) && signedOverflow) t->interrupt(INT_OVERFL_SIGN);
    else if ((mask.b & MSK_OVERFL_UNSIGN) && unsignedOverflow) t->interrupt(INT_OVERFL_UNSIGN);
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
    SNum sumS, sumU;                             // signed and unsigned sums
    SNum nanS;                                   // combined nan's
    // get sign options
    uint8_t options = 0;
    if (t->fInstr->tmpl == 0xE) options = t->pInstr->a.im3;
    else if (t->fInstr->tmpl == 0xA) options = uint8_t(mask >> 10);
    
    uint8_t signedOverflow = 0;
    uint8_t unsignedOverflow = 0;
    bool parmInf = false;
    sumS.q = sumU.q = 0;
    uint32_t temp1;
    uint64_t temp2;

    switch (t->operandType) {
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
        // calculate sum
        sumU.f = (parm[0].f + parm[1].f) + parm[2].f;
        // check for overflow
        if (isinf_f(sumU.i) && !parmInf) t->interrupt(INT_OVERFL_FLOAT);
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
        // calculate sum
        sumU.d = (parm[0].d + parm[1].d) + parm[2].d;
        // check for overflow
        if (isinf_d(sumU.q) && !parmInf) t->interrupt(INT_OVERFL_FLOAT);
        break;
    default:
        t->interrupt(INT_INST_ILLEGAL);
    }
    return sumU.q;
}

uint64_t f_add_h(CThread * t) {
    // add two numbers, float16
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    SNum mask = t->parm[3];
    uint16_t result;
    if (t->fInstr->immSize == 1) b.s = float2half(b.bs);  // convert 8-bit integer to float16
    if (t->operandType != 1) t->interrupt(INT_INST_ILLEGAL);
    result = float2half(half2float(a.s) + half2float(b.s));
    if (isnan_h(a.s) && isnan_h(b.s)) {    // both are NAN
        result = (a.s << 1) > (b.s << 1) ? a.s : b.s; // return the biggest payload. (current systems don't do this)
    }
    if (mask.i & MSK_OVERFL_FLOAT) {  // trap if overflow
        if (isinf_h(result) && !isinf_h(a.s) && !isinf_h(b.s)) {
            t->interrupt(INT_OVERFL_FLOAT);
        }
    }
    t->returnType =0x118;
    return result;
}

uint64_t f_sub_h(CThread * t) {
    // subtract two numbers, float16
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    SNum mask = t->parm[3];
    if (t->fInstr->immSize == 1) b.s = float2half(b.bs);  // convert 8-bit integer to float16
    uint16_t result;
    if (t->operandType != 1) t->interrupt(INT_INST_ILLEGAL);
    result = float2half(half2float(a.s) - half2float(b.s));
    if (isnan_h(a.s) && isnan_h(b.s)) {    // both are NAN
        result = (a.s << 1) > (b.s << 1) ? a.s : b.s; // return the biggest payload. (current systems don't do this)
    }
    if (mask.i & MSK_OVERFL_FLOAT) {  // trap if overflow
        if (isinf_h(result) && !isinf_h(a.s) && !isinf_h(b.s)) {
            t->interrupt(INT_OVERFL_FLOAT);
        }
    }
    t->returnType =0x118;
    return result;
}

uint64_t f_mul_h(CThread * t) {
    // mulitply two numbers, float16
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    SNum mask = t->parm[3];
    uint16_t result;
    if (t->fInstr->immSize == 1) b.s = float2half(b.bs);  // convert 8-bit integer to float16
    if (t->operandType != 1) t->interrupt(INT_INST_ILLEGAL);
    result = float2half(half2float(a.s) * half2float(b.s));
    if (isnan_h(a.s) && isnan_h(b.s)) {    // both are NAN
        result = (a.s << 1) > (b.s << 1) ? a.s : b.s; // return the biggest payload. (current systems don't do this)
    }
    if (mask.i & MSK_OVERFL_FLOAT) {  // trap if overflow
        if (isinf_h(result) && !isinf_h(a.s) && !isinf_h(b.s)) {
            t->interrupt(INT_OVERFL_FLOAT);
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
    SNum mask = t->parm[3];
    uint16_t result;
    if (t->fInstr->imm2 & 4) c = t->parm[4];          // avoid immediate operand shifted by imm3
    if (t->fInstr->immSize == 1) c.s = float2half(c.bs);  // convert 8-bit integer to float16                                                          // get sign options
    uint8_t options = 0;
    if (t->fInstr->tmpl == 0xE) options = t->pInstr->a.im3;
    else if (t->fInstr->tmpl == 0xA) options = (mask.i >> 10) & 0xF;
    if (t->vect == 2) { // odd vector element
        options >>= 1;
    }
    if (t->operandType != 1) t->interrupt(INT_INST_ILLEGAL);
    if (options & 1) a.s ^= 0x8000;                           // adjust sign
    if (options & 4) b.s ^= 0x8000;
    result = float2half(half2float(a.s) + half2float(b.s) * half2float(c.s));
    if (isnan_or_inf_h(result)) {                          // check for overflow and nan
        uint32_t nans = 0;  bool parmInf = false;
        for (int i = 0; i < 3; i++) {                      // loop through input operands
            uint32_t tmp = t->parm[i].s & 0x7FFF;          // ignore sign bit
            if (tmp > nans) nans = tmp;                    // get the biggest if there are multiple NANs
            if (tmp == inf_h) parmInf = true;              // OR of all INFs
        }
        if (nans > inf_h) return nans;                     // there is at least one NAN. return the biggest (sign bit is lost)
        else if ((mask.i & MSK_OVERFL_FLOAT) && !parmInf) t->interrupt(INT_OVERFL_FLOAT);
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

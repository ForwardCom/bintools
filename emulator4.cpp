/****************************  emulator4.cpp  ********************************
* Author:        Agner Fog
* date created:  2018-02-18
* Last modified: 2024-07-31
* Version:       1.13
* Project:       Binary tools for ForwardCom instruction set
* Description:
* Emulator: Execution functions for single format instructions, part 1
*
* Copyright 2018-2024 GNU General Public License http://www.gnu.org/licenses
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
    // Shift 16-bit unsigned constant left by 16 and add.
    t->parm[2].q <<= 16;
    return f_add(t);
}

static uint64_t shifti1_move(CThread * t) {
    // RD = IM2 << IM1. Sign-extend IM2 to 32/64 bits and shift left by the unsigned value IM1
    return (t->parm[2].qs >> 8) << t->parm[2].b;
}

static uint64_t shifti1_add(CThread * t) {
    // RD += IM2 << IM1. Sign-extend IM2 to 32/64 bits and shift left by the unsigned value IM1 and add
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
        default: t->interrupt(INT_WRONG_PARAMETERS);
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
        a.qs = (IM1 & 0x10) ? -1 : 0;            // return 0 or -1 if intput is 0
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
    if (dataSizeTable[t->operandType] > 8) t->interrupt(INT_WRONG_PARAMETERS); // illegal operand type
    if (a.q == 0) {
        a.qs = IM1 & 0x10 ? -1 : 0;              // return 0 or -1 if the intput is 0
    }
    else if (!(a.q & (a.q-1))) {
        return a.q;                              // the number is a power of 2. Return unchanged
    }
    else if (IM1 & 1) {
        // round up to nearest power of 2
        uint32_t s = bitScanReverse(a.q);        // highest set bit
        if (s+1 >= dataSizeTableBits[t->operandType]) { // overflow
            a.qs = IM1 & 0x20 ? -1 : 0;          // return 0 or -1 on overflow
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

    switch (rs) {
    case REG_NUMCONTR & 0x1F:     // numcontr register
        retval = t->numContr;    
        break;

    case REG_THREADP & 0x1F:     // threadp register
        retval = t->threadp;
        break;

    case REG_DATAP & 0x1F:       // datap register     
        retval = t->datap;
        break;

    default:                     // other register not implemented
        t->interrupt(INT_WRONG_PARAMETERS);
    }
    return retval;
}

static uint64_t write_spec(CThread * t) {
    // Write g. p. register RS to special register RD
    uint8_t rd = t->operands[0];                 // destination register
    SNum a = t->parm[1];                         // value
    switch (rd) {
    case REG_NUMCONTR & 0x1F:     // numcontr register
        t->numContr = a.i | 1;                   // bit 0 must be set
        if (((t->numContr ^ t->lastMask) & (1<<MSK_SUBNORMAL)) != 0) {
            // subnormal status changed
            enableSubnormals(t->numContr & (1<<MSK_SUBNORMAL));
        }
        t->lastMask = t->numContr;
        break;

    case REG_THREADP & 0x1F:     // threadp register
        t->threadp = a.q;
        break;

    case REG_DATAP & 0x1F:       // datap register     
        t->datap = a.q;
        break;

    default:                     // other register not implemented
        t->interrupt(INT_WRONG_PARAMETERS);
    }

    t->returnType = 0;
    return 0;
}

static uint64_t read_capabilities(CThread * t) {
    // Read capabilities register into g. p. register RD
    uint8_t capabreg = t->operands[4];    // capabilities register number
    if (capabreg < number_of_capability_registers) {
        return t->capabilyReg[capabreg];
    }
    else {
        t->interrupt(INT_WRONG_PARAMETERS);
    }
    return 0;
}

static uint64_t write_capabilities(CThread * t) {
    // Write g. p. register to capabilities register RD
    uint8_t capabreg = t->operands[0];    // capabilities register number
    uint64_t value =  t->parm[1].q;
    if (capabreg < number_of_capability_registers) {
        t->capabilyReg[capabreg] = value;
    }
    else {
        t->interrupt(INT_WRONG_PARAMETERS);
    }
    t->returnType = 0;
    return 0;
}

static uint64_t read_perf(CThread * t) {
    // Read performance counter
    uint8_t parfreg = t->operands[4];    // performance register number
    uint8_t par2 = t->parm[2].b;         // second operand
    uint64_t result = 0;
    switch (parfreg) {
    case 0:  // reset all performance counters
        if (par2 & 1) {
            t->perfCounters[perf_cpu_clock_cycles] = 0;
        }
        if (par2 & 2) {
            t->perfCounters[perf_instructions] = 0;
            t->perfCounters[perf_2size_instructions] = 0;
            t->perfCounters[perf_3size_instructions] = 0;
            t->perfCounters[perf_gp_instructions] = 0;
            t->perfCounters[perf_gp_instructions_mask0] = 0;
        }
        if (par2 & 4) {
            t->perfCounters[perf_vector_instructions] = 0;
        }
        if (par2 & 8) {
            t->perfCounters[perf_control_transfer_instructions] = 0;
            t->perfCounters[perf_direct_jumps] = 0;
            t->perfCounters[perf_indirect_jumps] = 0;
            t->perfCounters[perf_cond_jumps] = 0;
        }
        break;

    case 1:  // CPU clock cycles
        result = t->perfCounters[perf_cpu_clock_cycles];
        if (par2 == 0) t->perfCounters[perf_cpu_clock_cycles] = 0;
        break;

    case 2:  // number of instructions
        switch (par2) {
        case 0:
            result = t->perfCounters[perf_instructions];
            t->perfCounters[perf_instructions] = 0;
            t->perfCounters[perf_2size_instructions] = 0;
            t->perfCounters[perf_3size_instructions] = 0;
            t->perfCounters[perf_gp_instructions] = 0;
            t->perfCounters[perf_gp_instructions_mask0] = 0;
            break;
        case 1: 
            result = t->perfCounters[perf_instructions];
            break;
        case 2:
            result = t->perfCounters[perf_2size_instructions];
            break;
        case 3:
            result = t->perfCounters[perf_3size_instructions];
            break;
        case 4:
            result = t->perfCounters[perf_gp_instructions];
            break;
        case 5:
            result = t->perfCounters[perf_gp_instructions_mask0];
            break;
        }
        break;

    case 3:  // number of vector instructions
        result = t->perfCounters[perf_vector_instructions];
        if (par2 == 0) t->perfCounters[perf_vector_instructions] = 0;
        break;

    case 4:  // vector registers in use
        for (int iv = 0; iv < 32; iv++) {
            if (t->vectorLength[iv] > 0) result |= (uint64_t)1 << iv;
        }
        break;

    case 5:  // jumps, calls, and returns
        switch (par2) {
        case 0:
            result = t->perfCounters[perf_control_transfer_instructions];
            t->perfCounters[perf_control_transfer_instructions] = 0;
            t->perfCounters[perf_direct_jumps] = 0;
            t->perfCounters[perf_indirect_jumps] = 0;
            t->perfCounters[perf_cond_jumps] = 0;
            break;
        case 1:    // all jumps, calls, returns
            result = t->perfCounters[perf_control_transfer_instructions];
            break;
        case 2:    // direct unconditional jumps, calls, returns
            result = t->perfCounters[perf_direct_jumps];
            break;
        case 3:
            result = t->perfCounters[perf_indirect_jumps];
            break;
        case 4:
            result = t->perfCounters[perf_cond_jumps];
            break;
        }
        break;
    case 16:  // errors counters
        switch (par2) {
        case 0:
            result = 0;
            t->perfCounters[perf_unknown_instruction] = 0;
            t->perfCounters[perf_wrong_operands] = 0;
            t->perfCounters[perf_array_overflow] = 0;
            t->perfCounters[perf_read_violation] = 0;
            t->perfCounters[perf_write_violation] = 0;
            t->perfCounters[perf_misaligned] = 0;
            t->perfCounters[perf_address_of_first_error] = 0;
            t->perfCounters[perf_type_of_first_error] = 0;
            break;
        case 1:    // unknown instructions
            result = t->perfCounters[perf_unknown_instruction];
            break; 
        case 2:    // wrong operands for instruction
            result = t->perfCounters[perf_wrong_operands];
            break;
        case 3:    // array index out of bounds
            result = t->perfCounters[perf_array_overflow];
            break;
        case 4:    // memory read access violation
            result = t->perfCounters[perf_read_violation];
            break;
        case 5:    // memory write access violation
            result = t->perfCounters[perf_write_violation];
            break;
        case 6:    // memory access misaligned
            result = t->perfCounters[perf_misaligned];
            break;
        case 62:   // address of first error
            result = t->perfCounters[perf_address_of_first_error];
            break;
        case 63:   // type of first error
            result = t->perfCounters[perf_type_of_first_error];
            break;
        }

        break;
    default:
        t->interrupt(INT_WRONG_PARAMETERS);
    }

    return result;
}

static uint64_t read_sys(CThread * t) {
    // Read system register RS into g. p. register RD
    t->interrupt(INT_WRONG_PARAMETERS); // not supported yet
    return 0;
}

static uint64_t write_sys(CThread * t) {
    // Write g. p. register RS to system register RD
    t->interrupt(INT_WRONG_PARAMETERS); // not supported yet
    t->returnType = 0;
    return 0;
}

static uint64_t push_r(CThread * t) {
    // push one or more g.p. registers on a stack pointed to by rd
    int32_t step = dataSizeTable[t->operandType];
    bool forward = (t->parm[4].i & 0x80) != 0; // false: pre-decrement, true: post-increment pointer
    uint8_t reg0 = t->operands[0] & 0x1F;   // pointer register
    uint8_t reg1 = t->operands[4] & 0x1F;   // first push register
    uint8_t reglast = t->parm[4].i & 0x1F;  // last push register
    uint8_t reg;
    // check for errors
    if (reglast < reg1 || (reg0 >= reg1 && reg0 <= reglast)) {
        t->interrupt(INT_WRONG_PARAMETERS);
    }

    uint64_t pointer = t->registers[reg0];
    // loop through registers to push
    for (reg = reg1; reg <= reglast; reg++) {
        if (!forward) pointer -= (int64_t)step;
        uint64_t value = t->registers[reg];
        t->writeMemoryOperand(value, pointer);    
        if (forward) pointer += (int64_t)step;
        t->listResult(value);
    }
    t->registers[reg0] = pointer;
    t->running = 2; // don't save stack pointer with reduced operand size
    return pointer;
}

static uint64_t pop_r(CThread * t) {
    // pop one or more g.p. registers from a stack pointed to by rd
    int32_t step = dataSizeTable[t->operandType];
    bool regorder = (t->parm[4].i & 0x40) != 0; // false means last register first, true means first register first
    uint8_t reg0 = t->operands[0] & 0x1F;   // pointer register
    uint8_t reg1 = t->operands[4] & 0x1F;   // first push register
    uint8_t reglast = t->parm[4].i & 0x1F;  // last push register
    uint8_t reg;
    // check for errors
    if (reglast < reg1 || (reg0 >= reg1 && reg0 <= reglast)) {
        t->interrupt(INT_WRONG_PARAMETERS);
    }

    uint64_t pointer = t->registers[reg0];
    if (regorder) {
        // loop through registers to pop in forward order
        for (reg = reg1; reg <= reglast; reg++) {
            uint64_t value = t->readMemoryOperand(pointer);
            t->registers[reg] = value;
            pointer += (int64_t)step;
            t->listResult(value);
        }
    }
    else {
        // loop through registers to pop in reverse order
        for (reg = reglast; reg >= reg1; reg--) {
            uint64_t value = t->readMemoryOperand(pointer);
            t->registers[reg] = value;
            pointer += (int64_t)step;
            t->listResult(value);
        }
    }
    t->registers[reg0] = pointer;
    t->running = 2; // don't save stack pointer with reduced operand size
    return pointer;
}


// Format 2.9 A. Three general purpose registers and a 32-bit immediate operand

static uint64_t move_hi32(CThread * t) {
    // Load 32-bit constant into the high part of a general purpose register. The low part is zero. RD = IM4 << 32.
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
    // Add 32-bit constant to high part of general purpose register. RD = RT + (IM6 << 32).
    t->parm[2].q <<= 32;
    return f_add(t);
}

static uint64_t and_hi32(CThread * t) {
    // AND high part of general purpose register with 32-bit constant. RD = RT & (IM6 << 32).
    return t->parm[1].q & t->parm[2].q << 32;
}

static uint64_t or_hi32(CThread * t) {
    // OR high part of general purpose register with 32-bit constant. RD = RT | (IM6 << 32).
    return t->parm[1].q | t->parm[2].q << 32;
}

static uint64_t xor_hi32(CThread * t) {
    // XOR high part of general purpose register with 32-bit constant. RD = RT ^ (IM6 << 32).
    return t->parm[1].q ^ t->parm[2].q << 32;
}

static uint64_t replace_bits(CThread * t) {
    // Replace a group of contiguous bits in RT by a specified constant
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    uint64_t val = b.s;                          // value to insert
    uint8_t  pos = uint8_t(b.i >> 16);           // start position
    uint8_t  num = uint8_t(b.i >> 24);           // number of bits to replace
    if (num > 32 || pos + num > 64) t->interrupt(INT_WRONG_PARAMETERS);
    uint64_t mask = ((uint64_t)1 << num) - 1;    // mask with 'num' 1-bits
    return (a.q & ~(mask << pos)) | ((val & mask) << pos);
}

static uint64_t address_(CThread * t) {
    // RD = RT + IM6, RS can be THREADP (28), DATAP (29) or IP (30)
    t->returnType = 0x13;
    return t->memAddress;
}

// Format 1.2 A. Three vector register operands

static uint64_t set_len(CThread * t) {
    // RD = vector register RS with length changed to value of g.p. register RT
    // set_len: the new length is indicated in bytes
    // set_num: the new length is indicated in elements
    uint8_t  rd = t->operands[0];
    uint8_t  rs = t->operands[4];
    uint8_t  rt = t->operands[5];
    uint32_t oldLength = t->vectorLength[rs];
    uint64_t newLength = t->registers[rt];
    if (t->op & 1) newLength *= dataSizeTable[t->operandType];  // set_num: multiply by operand size
    if (newLength > t->MaxVectorLength) newLength = t->MaxVectorLength;
    if (newLength > oldLength) { 
        memcpy(t->vectors.buf() + rd*t->MaxVectorLength, t->vectors.buf() + rs*t->MaxVectorLength, oldLength);  // copy first part from RT
        memset(t->vectors.buf() + rd*t->MaxVectorLength + oldLength, 0, size_t(newLength - oldLength));               // set the rest to zero
    }
    else {
        memcpy(t->vectors.buf() + rd*t->MaxVectorLength, t->vectors.buf() + rs*t->MaxVectorLength, size_t(newLength));  // copy newLength from RT
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
    if (t->op & 1) length >>= dataSizeTableLog[t->operandType];  // get_num: divide by operand size (round down)
    t->registers[rd] = length;                             // save in g.p. register, not vector register
    t->vect = 4;                                           // stop vector loop
    t->running = 2;                                        // don't save to vector register RD
    t->returnType = 0x12;                                  // debug return output
    return length;
}

uint64_t insert_(CThread * t) {
    // Replace one element in vector RD, starting at offset RT·OS, with scalar RS
    uint64_t pos;                         // position of element insert
    uint8_t  rd = t->operands[3];         // source and destination register
    uint8_t  operandType = t->operandType;       // operand type
    uint64_t returnval;
    uint8_t  dsizelog = dataSizeTableLog[operandType]; // log2(elementsize)
    t->vectorLengthR = t->vectorLength[rd];
    uint8_t sourceVector = t->operands[4];      // source register 

    if (t->fInstr->format2 == 0x120) {   //  format 1.2A  v1 = insert(v1, v2, r3)
        uint8_t  rt = t->operands[5];         // index register
        pos = t->registers[rt] << dsizelog;
    }
    else {   // format 1.3B     v1 = insert(v1, v2, imm)
        pos = t->parm[2].q << dsizelog;
    }
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
    // and broadcast into vector register RD.
    uint8_t  rd = t->operands[0];                          // destination register
    uint8_t  operandType = t->operandType;                 // operand type
    uint8_t  dsizelog = dataSizeTableLog[operandType];     // log2(elementsize)
    uint8_t  rsource = t->operands[4];                     // source vector
    uint64_t pos;                                          // position = index * OS
    if (t->fInstr->format2 == 0x120) {
        uint8_t  rt = t->operands[5];                      // index register
        pos = t->registers[rt] << dsizelog;
    }
    else {  // format 0x130
        pos = t->parm[4].q << dsizelog;
    }
    uint32_t sourceLength = t->vectorLength[rsource];      // length of source vector
    uint64_t result;
    if (pos >= sourceLength) {
        result = 0;                                        // beyond end of source vector
    }
    else {
        int8_t * source = t->vectors.buf() + (uint64_t)rsource * t->MaxVectorLength; // address of rsource data
        result = *(uint64_t*)(source+pos);                 // no problem reading too much, it will be cut off later if the operand size is < 64 bits
        if (dsizelog >= 4) {                               // 128 bits
            t->parm[5].q = *(uint64_t*)(source+pos+8);     // store high part of 128 bit element
        }
    }
    t->vectorLength[rd] = t->vectorLengthR = sourceLength; // length of destination vector
    return result;
}



static uint64_t compress_sparse(CThread * t) {
    // Compress sparse vector elements indicated by mask bits into contiguous vector. 
    uint8_t  rd = t->operands[0];         // destination vector
    //uint8_t  rt = t->operands[4];       // length of input vector not specified
    uint8_t  rt = t->operands[5];         // source vector
    uint8_t  rm = t->operands[1];         // mask vector
    uint32_t sourceLength = t->vectorLength[rt]; // length of source vector
    uint32_t maskLength = t->vectorLength[rm];   // length of mask vector
    //uint64_t newLength = t->registers[rt];       // length of destination
    uint64_t newLength = sourceLength;     // length of destination
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
    uint8_t  rs = t->operands[4];         // source vector
    uint8_t  rt = t->operands[5];         // length indicator
    uint8_t  rm = t->operands[1];         // mask vector
    uint32_t sourceLength = t->vectorLength[rs]; // length of source vector
    uint32_t maskLength = t->vectorLength[rm];   // length of mask vector
    uint64_t newLength = t->registers[rt];       // length of destination
    uint32_t elementSize = dataSizeTable[t->operandType & 7];        // size of each element
    int8_t * source = t->vectors.buf() + rs*t->MaxVectorLength;      // address of RS data
    int8_t * masksrc = t->vectors.buf() + rm*t->MaxVectorLength;     // address of mask data
    int8_t * destination = t->vectors.buf() + rd*t->MaxVectorLength; // address of RD data
    if (rd == rs) {
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
    // Broadcast first element of source vector into all elements of RD with specified length
    uint8_t  rlen;                               // g.p. register indicating length
    uint64_t value;                              // value to broadcast
    uint8_t  rd = t->operands[0];                // destination vector
    if (t->fInstr->format2 == 0x120) {
        rlen = t->operands[5];                   // RT = length
        uint8_t  rs = t->operands[4];            // source vector
        value = t->readVectorElement(rs, 0);     // first element of RS
    }
    else {
        rlen = t->operands[4];                   // first source operand = length
        value = t->parm[2].q;                    // immediate operand
    }
    uint64_t destinationLength = t->registers[rlen];  // value of length register
    if (destinationLength > t->MaxVectorLength) destinationLength = t->MaxVectorLength; // limit length
    // set length of destination register, let vector loop continue to this length
    t->vectorLength[rd] = t->vectorLengthR = (uint32_t)destinationLength;
    return value;
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


static uint64_t shift_expand(CThread * t) {
    // Shift vector RS up by RT bytes and extend the vector length by RT. 
    // The lower RT bytes of RD will be zero.
    uint8_t  rd = t->operands[0];         // destination vector
    uint8_t  rs = t->operands[4];         // RS = source vector
    uint8_t  rt = t->operands[5];         // RT indicates length
    uint8_t * source = (uint8_t*)t->vectors.buf() + rs*t->MaxVectorLength; // address of RS data
    uint8_t * destination = (uint8_t*)t->vectors.buf() + rd*t->MaxVectorLength; // address of RD data
    uint64_t shiftCount = t->registers[rt];      // value of RT = shift count
    if (shiftCount > t->MaxVectorLength) shiftCount = t->MaxVectorLength; // limit length
    uint32_t sourceLength = t->vectorLength[rs]; // length of source vector
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
    // Shift vector RS down RT bytes and reduce the length by RT. 
    // The lower RT bytes of RS are lost
    uint8_t  rd = t->operands[0];         // destination vector
    uint8_t  rs = t->operands[4];         // RS = source vector
    uint8_t  rt = t->operands[5];         // RT indicates length
    uint8_t * source = (uint8_t*)t->vectors.buf() + rs*t->MaxVectorLength; // address of RS data
    uint8_t * destination = (uint8_t*)t->vectors.buf() + rd*t->MaxVectorLength; // address of RD data
    uint32_t sourceLength = t->vectorLength[rs]; // length of source vector
    uint64_t shiftCount = t->registers[rt];      // value of RT = shift count
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
    // Shift elements of vector RS up RT elements.
    // The lower RT elements of RD will be zero, the upper RT elements of RS are lost.
    uint8_t  rd = t->operands[0];         // destination vector
    uint8_t  rs = t->operands[4];         // RS = source vector
    uint8_t  rt = t->operands[5];         // RT indicates length
    uint8_t * source = (uint8_t*)t->vectors.buf() + rs * t->MaxVectorLength; // address of RS data
    uint8_t * destination = (uint8_t*)t->vectors.buf() + rd * t->MaxVectorLength; // address of RD data
    uint8_t  dsizelog = dataSizeTableLog[t->operandType];  // log2(elementsize)
    uint64_t shiftCount = t->registers[rt] << dsizelog;      // value of TS = shift count, elements
    if (shiftCount > t->MaxVectorLength) shiftCount = t->MaxVectorLength; // limit length
    uint32_t sourceLength = t->vectorLength[rs]; // length of source vector
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
    // Shift elements of vector RS down RT elements.
    // The upper RT elements of RD will be zero, the lower RT elements of RS are lost.
    uint8_t  rd = t->operands[0];                   // destination vector
    uint8_t  rs = t->operands[4];                   // RS = source vector
    uint8_t  rt = t->operands[5];                   // RT indicates length
    uint8_t * source = (uint8_t*)t->vectors.buf() + rs*t->MaxVectorLength; // address of RS data
    uint8_t * destination = (uint8_t*)t->vectors.buf() + rd*t->MaxVectorLength; // address of RD data
    uint32_t sourceLength = t->vectorLength[rs];           // length of source vector
    uint8_t  dsizelog = dataSizeTableLog[t->operandType];  // log2(elementsize)
    uint64_t shiftCount = t->registers[rt] << dsizelog;    // value of RT = shift count, elements
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

/*
static uint64_t rotate_up (CThread * t) {
    // Rotate vector RT up one element. 
    uint8_t  rd = t->operands[0];         // destination vector
    uint8_t  rt = t->operands[5];         // RT = source vector
    //uint8_t  rs = t->operands[4];         // RS indicates length
    int8_t * source = t->vectors.buf() + rt*t->MaxVectorLength; // address of RT data
    int8_t * destination = t->vectors.buf() + rd*t->MaxVectorLength; // address of RD data
    //uint64_t length = t->registers[rs];          // value of RS = vector length
    //if (length > t->MaxVectorLength) length = t->MaxVectorLength; // limit length
    uint32_t sourceLength = t->vectorLength[rt]; // length of source vector
    uint32_t length = sourceLength;
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
    uint8_t  rd = t->operands[0];         // destination vector
    uint8_t  rt = t->operands[5];         // RT = source vector
    //uint8_t  rs = t->operands[4];         // RS indicates length
    int8_t * source = t->vectors.buf() + rt*t->MaxVectorLength; // address of RT data
    int8_t * destination = t->vectors.buf() + rd*t->MaxVectorLength; // address of RD data
    //uint64_t length = t->registers[rs];          // value of RS = vector length
    uint32_t sourceLength = t->vectorLength[rt]; // length of source vector
    uint32_t length = sourceLength;
    //if (length > t->MaxVectorLength) length = t->MaxVectorLength; // limit length
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
    t->vectorLength[rd] = (uint32_t)length;      // set length of destination vector
    memcpy(destination, source + elementSize, size_t(length - elementSize)); // copy down
    memcpy(destination + length - elementSize, source, elementSize); // copy the bottom element to top
    t->vect = 4;                                           // stop vector loop
    t->running = 2;                                        // don't save RD. It has already been saved
    return 0;
}*/

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
    //SNum mask = t->parm[3];                      // mask register value or NUMCONTR
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
            t->interrupt(INT_WRONG_PARAMETERS);
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

static uint64_t f_mul_ex(CThread * t) {
    // extended signed multiply. result uses two consecutive array elements
    if (!t->vect) {
        t->interrupt(INT_WRONG_PARAMETERS);  return 0;
    }
    SNum result;
    switch (t->operandType) {
    case 0:   // int8
        result.is = ((int32_t)t->parm[1].bs * (int32_t)t->parm[2].bs);
        t->parm[5].is = result.is >> 8;  // store high part in parm[q]
        break;
    case 1:   // int16
        result.is = ((int32_t)t->parm[1].ss * (int32_t)t->parm[2].ss);
        t->parm[5].is = result.is >> 16;  // store high part in parm[5]
        break;
    case 2:   // int32
        result.qs = ((int64_t)t->parm[1].is * (int64_t)t->parm[2].is);
        t->parm[5].qs = result.qs >> 32;  // store high part in parm[5]
        break;
    case 3:   // int64
        result.qs = mul64_128s(&t->parm[5].q, t->parm[1].qs, t->parm[2].qs);
        break;
    default:
        t->interrupt(INT_WRONG_PARAMETERS);
        result.i = 0;
    }
    return result.q;
}

static uint64_t f_mul_ex_u(CThread * t) {
    // extended unsigned multiply. result uses two consecutive array elements
    if (!t->vect) {
        t->interrupt(INT_WRONG_PARAMETERS);  return 0;
    }
    SNum result;
    switch (t->operandType) {
    case 0:   // int8
        result.i = ((uint32_t)t->parm[1].b * (uint32_t)t->parm[2].b);
        t->parm[5].i = result.i >> 8;  // store high part in parm[5]
        break;
    case 1:   // int16
        result.i = ((uint32_t)t->parm[1].s * (uint32_t)t->parm[2].s);
        t->parm[5].i = result.i >> 16;  // store high part in parm[5]
        break;
    case 2:   // int32
        result.q = ((uint64_t)t->parm[1].i * (uint64_t)t->parm[2].i);
        t->parm[5].q = result.q >> 32;  // store high part in parm[5]
        break;
    case 3:   // int64
        result.q = mul64_128u(&t->parm[5].q, t->parm[1].q, t->parm[2].q);
        break;
    default:
        t->interrupt(INT_WRONG_PARAMETERS);
        result.i = 0;
    }
    return result.q;
}

static uint64_t sqrt_ (CThread * t) {
    // square root
    SNum a = t->parm[2];                         // input operand
    SNum result;  result.q = 0;
    uint32_t mask = t->parm[3].i;
    uint8_t operandType = t->operandType;
    bool detectExceptions = (mask & (0xF << MSKI_EXCEPTIONS)) != 0;  // make NAN if exceptions
    uint32_t roundingMode = (mask >> MSKI_ROUNDING) & 7;
    bool error = false;
    switch (operandType) {
    /*case 0:   // int8
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
        break; */
    case 1:   // float16
        if (a.ss < 0) {
            result.q = t->makeNan(nan_invalid_sqrt, operandType);
        }
        else {
            float x = half2float(a.s);
            if (detectExceptions) clearExceptionFlags();   // clear previous exceptions
            float y = sqrtf(x);                            // calculate square root
            result.i = roundToHalfPrecision(y, t);         // round with specified rounding mode
            if (detectExceptions) {
                uint32_t x = getExceptionFlags();          // read exceptions
                if (result.i == 0 && y != 0) x = 0x10;     // underflow
                if ((mask & (1<<MSK_UNDERFLOW)) && (x & 0x10)) result.q = t->makeNan(nan_underflow, operandType);
                else if ((mask & (1<<MSK_INEXACT)) && (x & 0x20)) result.q = t->makeNan(nan_inexact, operandType);
            }
        }
        t->returnType = 0x118;                             // return type is float16
        break;
    case 5:   // float
        if (a.f < 0) {
            result.q = t->makeNan(nan_invalid_sqrt, operandType);
        }
        else {
            if (detectExceptions) clearExceptionFlags();   // clear previous exceptions
            if (roundingMode != 0) setRoundingMode(roundingMode);
            result.f = sqrtf(a.f);                         // calculate square root
            if (roundingMode == 4) {
                // special case for rounding mode 4 (odd if not exact)
                setRoundingMode(2);                            // try with both round up and round down
                SNum roundUpResult;
                roundUpResult.f = sqrtf(a.f);
                if (roundUpResult.i & 1) result = roundUpResult; // choose the odd result
            }
            if (roundingMode != 0) setRoundingMode(0);
            if (detectExceptions) {
                uint32_t x = getExceptionFlags();          // read exceptions
                if ((mask & (1<<MSK_UNDERFLOW)) && (x & 0x10)) result.q = t->makeNan(nan_underflow, operandType);
                else if ((mask & (1<<MSK_INEXACT)) && (x & 0x20)) result.q = t->makeNan(nan_inexact, operandType);
            }
        }
        break;
    case 6:   // double
        if (a.d < 0) {
            result.q = t->makeNan(nan_invalid_sqrt, operandType);
        }
        else {
            if (detectExceptions) clearExceptionFlags();   // clear previous exceptions
            if (roundingMode != 0) setRoundingMode(roundingMode);
            result.d = sqrt(a.d);                          // calculate square root
            if (roundingMode == 4) {
                // special case for rounding mode 4 (odd if not exact)
                setRoundingMode(2);                            // try with both round up and round down
                SNum roundUpResult;
                roundUpResult.d = sqrt(a.d);
                if (roundUpResult.i & 1) result = roundUpResult; // choose the odd result
            }
            if (roundingMode != 0) setRoundingMode(0);
            if (detectExceptions) {
                uint32_t x = getExceptionFlags();          // read exceptions
                if ((mask & (1<<MSK_UNDERFLOW)) && (x & 0x10)) result.q = t->makeNan(nan_underflow, operandType);
                else if ((mask & (1<<MSK_INEXACT)) && (x & 0x20)) result.q = t->makeNan(nan_inexact, operandType);
            }
        }
        break;
    default:
        t->interrupt(INT_WRONG_PARAMETERS);
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
        t->interrupt(INT_WRONG_PARAMETERS);
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
        t->interrupt(INT_WRONG_PARAMETERS);
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
        t->interrupt(INT_WRONG_PARAMETERS);  result.q = 0;
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
        t->interrupt(INT_WRONG_PARAMETERS);  result.q = 0;
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
        t->interrupt(INT_WRONG_PARAMETERS);
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

    // to do: rounding mode!

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
        t->interrupt(INT_WRONG_PARAMETERS);
    }
    t->parm[5].q = carry.q & 3;                  // return carry
    return result.q;                             // return result
}

static uint64_t read_spev (CThread * t) {
    // Read special register RS into vector register RD with length RT.
    // to do
    return 0;
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

/* Input ports to match soft core
Note: serial input from stdin in windows and Linux is messy. Emulation will have quirks. 

Input port 8. Serial input:
Read one byte from RS232 serial input. The value is
bit 0-7: Received data (zero if input buffer empty)
bit   8: Data valid. Will be 0 if the input buffer is empty. It will not wait for data if the system allows polling
bit   9: More data ready: The input buffer contains at least one more byte ready to read
bit  12: Buffer overflow error. Data has been lost due to input buffer overflow
bit  13: Frame error. Error detected in start bit or stop bit. May be due to noise or wrong BAUD rate

Input port 9. Serial input status:
bit 0-15: Number of bytes currently in input buffer
bit   16: Buffer overflow error. Data has been lost due to input buffer overflow
bit   17: Frame error. Error detected in start bit or stop bit. May be due to noise or wrong BAUD rate

Input port 11. Serial output status:
bit 0-15: Number of bytes currently in output buffer
bit   16: Buffer overflow error. Data has been lost due to output buffer overflow
bit   18: Ready. The output buffer has enough space to receive at least one more byte

*/

static uint64_t input_ (CThread * t) {
    // read from input port. 
    // vector version: RD = vector register, RS = port address, RT = vector length
    // g.p. version: RD = g.p. register, RS = port address, IM1 = port address
    using namespace std;  // some compilers have getchar and putchar in namespace std, some not
    if (t->vect) {   // vector version not implemented yet
        t->interrupt(INT_WRONG_PARAMETERS);
        return 0;
    }
    uint32_t port = t->parm[2].i;           // immediate operand contains port number
    if (port == 255) port = t->parm[1].i;   // register operand contains port number

    switch (port) {
#if defined (__WINDOWS__) || defined (_WIN32) || defined (_WIN64)
    case 8:    // port 8: read serial input
        if (_kbhit()) {
            //int res = getchar();          // read character from stdin. waits for enter
            int res = _getch();             // read character from stdin. does not wait for enter
            if (res < 0) return 0;          // error or end of file (EOF = -1)
            else return (res | 0x100);      // input valid
        }
        else return 0;
    case 9:    // port 9: read serial input status. Only in systems that allow polling
        return _kbhit();
#else   // Other operating systems
        // Why is there no portable way of non-blocking read or polling a serial input?
    //case 8: case 9:
    //    return 0;  // to do: implement for Linux using curses.h or something
#endif
    case 11:   // port 11: get serial output status.
        return 0;
    default:
        t->interrupt(INT_WRONG_PARAMETERS);
        break;
    }
    return 0;
}

/* Output ports to match soft core
Output port 9. Serial input control:
bit    0: Clear buffer. Delete all data currently in the input buffer, and clear error flags 
bit    1: Clear error flags but keep data. 
          The error bits remain high after an error condition until reset by this or by system reset 

Output port 10. Serial output:
Write one byte to RS232 serial output.
bit 0-7: Data to write
Other bits are reserved.

Output port 11. Serial output control:
bit    0: Clear buffer. Delete all data currently in the input buffer, and clear error flags 
bit    1: Clear error flags but keep data. 
          The error bits remain high after an error condition until reset by this or by system reset 
*/

static uint64_t output_ (CThread * t) {
    // write to output port. 
    // vector version: RD = vector register to write, RS = port address, RT = vector length
    // g.p. version: RD = g.p. register to wrote, RS = port address, IM1 = port address
    using namespace std;  // some compilers have getchar and putchar in namespace std::, some not
    if (t->vect) {   // vector version not implemented yet
        t->interrupt(INT_WRONG_PARAMETERS);
        return 0;
    }
    uint32_t port = t->parm[2].i;           // immediate operand contains port number
    if (port == 255) port = t->parm[1].i;   // register operand contains port number
    uint32_t value = t->parm[0].i;          // value to output
    switch (port) {
    case 9:   // clear input buffer
#if defined (__WINDOWS__) || defined (_WIN32) || defined (_WIN64)
        while (_kbhit()) (void)_getch();
#endif
        break;
    case 10:   // write character
        putchar(value);
        break;
    case 11:   // serial output control. not possible in most operating systems
        break;
    default:
        t->interrupt(INT_WRONG_PARAMETERS);
        break;
    }
    t->running = 2;  // don't save to register RD
    return 0;
}


// tables of single format instructions
// Format 1.0 A. Three general purpose registers
PFunc funcTab4[64] = {
    0, 0, 0, 0, 0, 0, 0, 0
};

// Format 1.1 C. One general purpose register and a 16 bit immediate operand. int64
PFunc funcTab5[64] = {
    move_16s, move_16s, 0, move_16u, shifti1_move, shifti1_move, f_add, 0,   // 0 - 7
    f_mul, 0, shifti1_add, shifti1_add, shifti1_and, shifti1_and, shifti1_or, shifti1_or,  // 8 - 15 
    shifti1_xor, shifti1_xor, shift16_add, 0, 0, 0, 0, // 16 -23    
};


// Format 1.2 A. Three vector register operands
PFunc funcTab6[64] = {
    get_len, get_len, set_len, set_len, insert_, extract_, broad_, 0,               // 0  - 7
    compress_sparse, expand_sparse, 0, 0, bits2bool, 0, 0, 0,                       // 8 - 15
    shift_expand, shift_reduce, shift_up, shift_down, 0, 0, 0, 0, // 16 - 23
    div_ex, div_ex, f_mul_ex, f_mul_ex_u, sqrt_, 0, 0, 0,                           // 24 - 31
    add_ss, add_us, sub_ss, sub_us, mul_ss, mul_us, add_oc, sub_oc,                 // 32 - 39
    mul_oc, div_oc, add_c, sub_b, 0, 0, 0, 0,                                       // 40 - 47
    0, 0, 0, 0, 0, 0, 0, 0,                                                         // 48 - 55
    read_spev, 0, read_call_stack, write_call_stack, read_memory_map, write_memory_map, input_, output_ // 56 - 63
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
    push_r, pop_r, 0, 0, 0, 0, input_, output_                   // 56 - 63
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

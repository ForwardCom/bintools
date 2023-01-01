/****************************  emulator2.cpp  ********************************
* Author:        Agner Fog
* date created:  2018-02-18
* Last modified: 2021-02-19
* Version:       1.11
* Project:       Binary tools for ForwardCom instruction set
* Description:
* Emulator: Execution functions for jump instructions
*
* Copyright 2018-2021 GNU General Public License http://www.gnu.org/licenses
*****************************************************************************/

#include "stdafx.h"

static uint64_t f_jump(CThread * t) {
    // simple self-relative jump
    t->ip += t->addrOperand * 4;                           // add relative offset to IP
    t->running = 2;  t->returnType = 0;                    // no return value to save
    return 0;
}

static uint64_t f_call(CThread * t) {
    // simple self-relative call
    t->callStack.push(t->ip);                              // push return address on call stack
    if (t->callStack.numEntries() > t->callDept) t->callDept = t->callStack.numEntries();
    t->ip += t->addrOperand * 4;                           // add relative offset to IP
    t->running = 2;  t->returnType = 0;                    // no return value to save
    return 0;
}

static uint64_t compare_jump_generic(CThread * t) {
    // compare operands, jump on contition
    SNum a = t->parm[1];                                   // first parameter
    SNum b = t->parm[2];                                   // second parameter
    uint8_t branch = 0;                                    // jump if 1
    if (t->operandType < 4) {
        // all integer types
        uint64_t sizeMask = dataSizeMask[t->operandType];  // mask for data size
        uint64_t signBit = (sizeMask >> 1) + 1;            // sign bit
        a.q &= sizeMask;  b.q &= sizeMask;                 // limit to right number of bits
        // select condition
        switch (t->op & 0xE) {                             // mask out constant bits and invert bit
        case 0:  // jump if equal
            branch = a.q == b.q;  break;
        case 2:  // jump if signed below
            branch = (a.q ^ signBit) < (b.q ^ signBit);  break;            
        case 4:  // jump if signed above
            branch = (a.q ^ signBit) > (b.q ^ signBit);  break;            
        case 6:  // jump if unsigned below
            branch = a.q < b.q;  break;            
        case 8:  // jump if unsigned above
            branch = a.q > b.q;  break;            
        default: 
            t->interrupt(INT_WRONG_PARAMETERS);
        }
        branch ^= t->op;                                   // invert branch condition if op odd
    }
    else {
        // vector registers
        if (t->operandType != 5 && t->operandType != 6) {
            t->interrupt(INT_WRONG_PARAMETERS);                // unsupported operand type
            return 0;
        }
        if ((t->op & 0xFE) == 24) {
            // fp_category test
            if (t->fInstr->immSize) b = t->parm[4];        // avoid conversion of b to float
            bool mant0;                                    // mantissa is zero
            bool exp0;                                     // exponent is all zeroes
            bool exp1;                                     // exponent is all ones
            bool sign;                                     // sign bit
            if (t->operandType == 5) {                     // float
                mant0 = (a.i & 0x007FFFFF) == 0;           // mantissa is zero
                exp0 = (a.i & 0x7F800000) == 0;            // exponent is all zeroes
                exp1 = (a.i & 0x7F800000) == 0x7F800000;   // exponent is all ones
                sign = a.i >> 31 != 0;                     // sign bit
            }
            else {                                         // double
                mant0 = a.q << 12 == 0;                    // mantissa is zero
                exp0 = a.q << 1 >> 53 == 0;                // exponent is all zeroes
                exp1 = a.q << 1 >> 53 == 0xFFFFFFFFFFFFF;  // exponent is all ones
                sign = a.q >> 63 != 0;                     // sign bit
            }
            if (exp1) {
                if (b.b & 1)    branch |= !mant0;          // NAN
                if (b.b & 0x40) branch |= mant0 && sign;   // -INF
                if (b.b & 0x80) branch |= mant0 && !sign;  // +INF
            }
            else if (exp0) {
                if (b.b & 2)    branch |= uint8_t(mant0);  // +/- 0.0
                if (b.b & 4)    branch |= !mant0 && sign;  // - subnormal
                if (b.b & 8)    branch |= !mant0 && !sign; // + subnormal
            }
            else {
                if (b.b & 0x10) branch |= uint8_t(sign);   // - normal
                if (b.b & 0x20) branch |= !sign;           // + normal
            }
            branch ^= t->op;                               // invert branch condition if op odd
        }
        else {
            bool unordered;
            uint8_t opj = t->op;
            if (t->operandType == 5) {
                // float
                unordered = isnan_f(a.i) || isnan_f(b.i);  // check if unordered
                if (unordered) {
                    // a or b is NAN. Don't check the condition. Branch only if unordered version of instruction.
                    branch = t->op < 32;
                }
                else {
                    if ((opj & 0xE) > 5) {
                        // compare absolute values
                        a.i &= 0x7FFFFFFF;  b.i &= 0x7FFFFFFF;
                        opj -= 4;
                    }
                    // select condition, float
                    switch (opj & 0xE) {   // mask out bits for ordered/unordered and invert
                    case 0:    // jump if equal
                        branch = a.f == b.f;  break;
                    case 2:    // jump if below
                        branch = a.f < b.f;  break;
                    case 4:    // jump if above
                        branch = a.f > b.f;  break;
                    default:
                        t->interrupt(INT_WRONG_PARAMETERS);
                    }
                    branch ^= t->op;                               // invert branch condition if op odd
                }
            }
            else {
                // double
                unordered = isnan_d(a.q) || isnan_d(b.q);  // check if unordered
                if (unordered) {
                    // a or b is NAN. Don't check the condition. Branch only if unordered version of instruction.
                    branch = t->op < 32;
                }
                else {
                    if ((opj & 0xE) > 5) {
                        // compare absolute values
                        a.q &= nsign_d;  b.q &= nsign_d;
                        opj -= 4;
                    }
                    // select condition, float
                    switch (opj & 0xE) {                 // mask out bits for ordered/unordered and invert
                    case 0:    // jump if equal
                        branch = a.d == b.d;  break;
                    case 2:    // jump if below
                        branch = a.d < b.d;  break;
                    case 4:    // jump if above
                        branch = a.d > b.d;  break;
                    default:
                        t->interrupt(INT_WRONG_PARAMETERS);
                    }
                    branch ^= t->op;                       // invert branch condition if op odd
                }
            }
        }
    }
    if (branch & 1) {
        t->ip += t->addrOperand * 4;                       // add relative offset to IP
        t->returnType = 0x2000;                            // debug output jump taken
    }
    else t->returnType = 0x1000;                           // debug output jump taken
    if (t->vect) t->vect = 4;                              // stop vector loop
    t->running = 2;                                        // don't save result
    return 0; 
}


static uint64_t sub_jump_generic(CThread * t) {
    // subtract and jump on some condition
    if (t->operandType > 4) {  // floating point types have no add/jump
        // the opcode is used for floating point compare unordered
        return compare_jump_generic(t);
    }
    SNum a = t->parm[1];                         // first parameter
    SNum b = t->parm[2];                         // second parameter
    SNum result;                                 // result
    int8_t branch = 0;                           // jump if 1
    int8_t unsignedOverflow = 0;                 // unsigned overflow detected
    int8_t signedOverflow = 0;                   // signed overflow detected
    int8_t op1 = t->op >> 1;                     // operation without toggle bit
    uint64_t sizeMask = dataSizeMask[t->operandType]; // mask for data size
    uint64_t signBit = (sizeMask >> 1) + 1;      // sign bit
    result.q = a.q - b.q;                        // subtract integers

    // detection of signed overflow 
    SNum overfl;  // overflow if a and b have opposite sign and result has opposite sign of a
    overfl.q = (a.q ^ b.q) & (a.q ^ result.q);
    signedOverflow = (overfl.q & signBit) != 0;
    // detection of borrow / unsigned overflow required
    unsignedOverflow = (result.q & sizeMask) > (a.q & sizeMask);

    // detect branch condition
    switch (op1) {
    case 0:  // jump if zero
        branch = (result.q & sizeMask) == 0;
        break;
    case 1:  // jump if negative
        branch = (result.q & signBit) != 0;
        break;
    case 2:  // jump if positive
        branch = (result.q & signBit) == 0 && (result.q & sizeMask) != 0;
        break;
    case 3:  // jump if signed overflow
        branch = signedOverflow;
        signedOverflow = unsignedOverflow = 0;   // no interrupt for this condition
        break;
    case 4:  // jump if borrow (unsigned overflow)
        branch = unsignedOverflow;
        signedOverflow = unsignedOverflow = 0;   // no interrupt for this condition
        break;
    default:
        err.submit(ERR_INTERNAL);
    }
    // only integer types in g.p. registers allowed
    if (t->operandType > 3) t->interrupt(INT_WRONG_PARAMETERS);
    // overflow interrupts
    //if (signedOverflow)   t->interrupt(INT_OVERFL_SIGN);
    //if (unsignedOverflow) t->interrupt(INT_OVERFL_UNSIGN);

    // invert condition if op odd
    branch ^= t->op;
    // conditional branch
    if (branch & 1) {
        t->ip += t->addrOperand * 4;             // add relative offset to IP
        t->returnType |= 0x2000;                 // debug output jump taken
    }
    return result.q;                             // return result
}

static uint64_t add_jump_generic(CThread * t) {
    // add and jump on some condition
    if (t->operandType > 4) {  // floating point types have no add/jump
        // the opcode is used for floating point compare unordered
        return compare_jump_generic(t);
    }
    SNum a = t->parm[1];                         // first parameter
    SNum b = t->parm[2];                         // second parameter
    SNum result;                                 // result
    int8_t branch = 0;                           // jump if 1
    int8_t unsignedOverflow = 0;                 // unsigned overflow detected
    int8_t signedOverflow = 0;                   // signed overflow detected
    int8_t op1 = t->op >> 1;                     // operation without toggle bit
    uint64_t sizeMask = dataSizeMask[t->operandType]; // mask for data size
    uint64_t signBit = (sizeMask >> 1) + 1;      // sign bit

    // add integers
    result.q = a.q + b.q;
    // detection of signed overflow required
    SNum overfl;  // overflow if a and b have same sign and result has opposite sign of a
    overfl.q = ~(a.q ^ b.q) & (a.q ^ result.q);
    signedOverflow = (overfl.q & signBit) != 0;
    // detection of carry / unsigned overflow required
    unsignedOverflow = (result.q & sizeMask) < (a.q & sizeMask);

    // detect branch condition
    switch (op1) {
    case 8:  // jump if zero
        branch = (result.q & sizeMask) == 0;
        break;
    case 9:  // jump if negative
        branch = (result.q & signBit) != 0;
        break;
    case 10:  // jump if positive
        branch = (result.q & signBit) == 0 && (result.q & sizeMask) != 0;
        break;
    case 11:  // jump if signed overflow
        branch = signedOverflow;
        signedOverflow = unsignedOverflow = 0;   // no interrupt for this condition
        break;
    case 12:  // jump if borrow (unsigned overflow)
        branch = unsignedOverflow;
        signedOverflow = unsignedOverflow = 0;   // no interrupts for these conditions
        break;
    default:
        err.submit(ERR_INTERNAL);
    }
    // only integer types in g.p. registers allowed for add/jump instructions
    if (t->operandType > 3 && op1 < 12) t->interrupt(INT_WRONG_PARAMETERS);
    // overflow interrupts
    //if (signedOverflow)   t->interrupt(INT_OVERFL_SIGN);
    //if (unsignedOverflow) t->interrupt(INT_OVERFL_UNSIGN);

    // invert condition if op odd
    branch ^= t->op;

    // conditional branch
    if (branch & 1) {
        t->ip += t->addrOperand * 4;             // add relative offset to IP
        t->returnType |= 0x2000;                 // debug output jump taken
    }
    return result.q;                             // return result
}
/*
static uint64_t shift_left_jump_zero(CThread * t) {
    // shift left and jump if zero
    SNum a = t->parm[1];                         // first parameter
    SNum b = t->parm[2];                         // second parameter
    if (t->fInstr->immSize) b = t->parm[4];      // avoid conversion of b to float
    SNum result;                                 // result
    int8_t branch = 0;                           // jump if 1
    result.q = 0;

    switch (t->operandType) {
    case 0:   // int8
        if (b.b < 8) result.b = a.b << b.b;
        branch = result.b == 0;
        break;
    case 1:   // int16
        if (b.s < 16) result.s = a.s << b.b;
        branch = result.s == 0;
        break;
    case 2: case 5:   // int32, float
        if (b.i < 32) result.i = a.i << b.b;
        branch = result.i == 0;
        break;
    case 3:   // int64, double
        if (b.q < 64) result.q = a.q << b.b;
        branch = result.q == 0;
        break;
    default:
        t->interrupt(INT_WRONG_PARAMETERS);
    }
    if (t->vect) { // vector registers. make result scalar and stop vector loop
        t->vectorLength[t->operands[0]] = t->vectorLengthR = dataSizeTable[t->operandType];        
    }
    // invert condition if op odd
    branch ^= t->op;

    // conditional branch
    if (branch & 1) {
        t->ip += t->addrOperand * 4;             // add relative offset to IP
        t->returnType |= 0x2000;                 // debug output jump taken
    }
    return result.q; 
}

static uint64_t shift_right_u_jump_zero(CThread * t) {
    // shift right unsigned and jump if zero
    SNum a = t->parm[1];                         // first parameter
    SNum b = t->parm[2];                         // second parameter
    if (t->fInstr->immSize) b = t->parm[4];      // avoid conversion of b to float
    SNum result;                                 // result
    int8_t branch = 0;                           // jump if 1
    result.q = 0;

    switch (t->operandType) {
    case 0:   // int8
        if (b.b < 8) result.b = a.b >> b.b;
        branch = result.b == 0;
        break;
    case 1:   // int16
        if (b.s < 16) result.s = a.s >> b.b;
        branch = result.s == 0;
        break;
    case 2: case 5:   // int32, float
        if (b.i < 32) result.i = a.i >> b.b;
        branch = result.i == 0;
        break;
    case 3:   // int64, double
        if (b.q < 64) result.q = a.q >> b.b;
        branch = result.q == 0;
        break;
    default:
        t->interrupt(INT_WRONG_PARAMETERS);
    }
    if (t->vect) { // vector registers. make result scalar and stop vector loop
        t->vectorLength[t->operands[0]] = t->vectorLengthR = dataSizeTable[t->operandType];        
    }
    // invert condition if op odd
    branch ^= t->op;

    // conditional branch
    if (branch & 1) {
        t->ip += t->addrOperand * 4;             // add relative offset to IP
        t->returnType |= 0x2000;                 // debug output jump taken
    }
    return result.q; 
}

static uint64_t rotate_jump_carry(CThread * t) {
    // rotate left and jump if the last rotated bit is 1
    SNum a = t->parm[1];                         // first parameter
    SNum b = t->parm[2];                         // second parameter
    if (t->fInstr->immSize) b = t->parm[4];      // avoid conversion of b to float
    SNum result;                                 // result
    int8_t branch = 0;                           // jump if 1
    result.q = 0;

    switch (t->operandType) {
    case 0:   // int8
        b.b &= 7;            
        result.b = (a.b << b.b) | (a.b >> (8 - b.b));
        branch = b.bs < 0 ? result.b >> 7 : result.b; // most or least significant bit
        break;
    case 1:   // int16
        b.b &= 15;            
        result.b = (a.s << b.b) | (a.s >> (16 - b.b));
        branch = uint8_t(b.ss < 0 ? result.s >> 15 : result.s); // most or least significant bit
        break;
    case 2: case 5:   // int32, float
        b.b &= 31;            
        result.i = (a.i << b.b) | (a.i >> (32 - b.b));
        branch = uint8_t(b.is < 0 ? result.i >> 31 : result.i); // most or least significant bit
        break;
    case 3:   // int64, double
        b.b &= 63;            
        result.q = (a.q << b.b) | (a.q >> (64 - b.b));
        branch = uint8_t(b.qs < 0 ? result.q >> 63 : result.q); // most or least significant bit
        break;
    default:
        t->interrupt(INT_WRONG_PARAMETERS);
    }
    if (t->vect) { // vector registers. make result scalar and stop vector loop
        t->vectorLength[t->operands[0]] = t->vectorLengthR = dataSizeTable[t->operandType];        
    }
    // invert condition if op odd
    branch ^= t->op;

    // conditional branch
    if (branch & 1) {
        t->ip += t->addrOperand * 4;             // add relative offset to IP
        t->returnType |= 0x2000;                 // debug output jump taken
    }
    return result.q; 
} */

static uint64_t and_jump_zero(CThread * t) {
    // bitwise AND, jump if zero
    SNum a = t->parm[1];                         // first parameter
    SNum b = t->parm[2];                         // second parameter
    if (t->fInstr->immSize) b = t->parm[4];      // avoid conversion of b to float
    SNum result;                                 // result
    int8_t branch = 0;                           // jump if 1
    result.q = a.q & b.q;                        // bitwise AND

    // branch condition
    switch (t->operandType) {
    case 0:   // int8
        branch = result.b == 0;  break;
    case 1:   // int16
        branch = result.s == 0;  break;
    case 2: case 5:   // int32, float
        branch = result.i == 0;  break;
    case 3: case 6:  // int64, double
        branch = result.q == 0;  break;
    default:
        t->interrupt(INT_WRONG_PARAMETERS);
    }
    if (t->vect) { // vector registers. make result scalar and stop vector loop
        t->vectorLength[t->operands[0]] = t->vectorLengthR = dataSizeTable[t->operandType];        
    }
    // invert condition if op odd
    branch ^= t->op;

    // conditional branch
    if (branch & 1) {
        t->ip += t->addrOperand * 4;             // add relative offset to IP
        t->returnType |= 0x2000;                 // debug output jump taken
    }
    return result.q; 
}

static uint64_t or_jump_zero(CThread * t) {
    // bitwise OR, jump if zero
    SNum a = t->parm[1];                         // first parameter
    SNum b = t->parm[2];                         // second parameter
    if (t->fInstr->immSize) b = t->parm[4];      // avoid conversion of b to float
    SNum result;                                 // result
    int8_t branch = 0;                           // jump if 1
    result.q = a.q | b.q;                        // bitwise AND

    // branch condition
    switch (t->operandType) {
    case 0:   // int8
        branch = result.b == 0;  break;
    case 1:   // int16
        branch = result.s == 0;  break;
    case 2: case 5:   // int32, float
        branch = result.i == 0;  break;
    case 3: case 6:   // int64, double
        branch = result.q == 0;  break;
    default:
        t->interrupt(INT_WRONG_PARAMETERS);
    }
    if (t->vect) { // vector registers. make result scalar and stop vector loop
        t->vectorLength[t->operands[0]] = t->vectorLengthR = dataSizeTable[t->operandType];        
    }
    // invert condition if op odd
    branch ^= t->op;

    // conditional branch
    if (branch & 1) {
        t->ip += t->addrOperand * 4;             // add relative offset to IP
        t->returnType |= 0x2000;                 // debug output jump taken
    }
    return result.q; 
}

static uint64_t xor_jump_zero(CThread * t) {
    // bitwise XOR, jump if zero
    SNum a = t->parm[1];                         // first parameter
    SNum b = t->parm[2];                         // second parameter
    if (t->fInstr->immSize) b = t->parm[4];      // avoid conversion of b to float
    SNum result;                                 // result
    int8_t branch = 0;                           // jump if 1
    result.q = a.q ^ b.q;                        // bitwise AND

    // branch condition
    switch (t->operandType) {
    case 0:   // int8
        branch = result.b == 0;  break;
    case 1:   // int16
        branch = result.s == 0;  break;
    case 2: case 5:   // int32, float
        branch = result.i == 0;  break;
    case 3: case 6:  // int64, double
        branch = result.q == 0;  break;
    default:
        t->interrupt(INT_WRONG_PARAMETERS);
    }
    if (t->vect) { // vector registers. make result scalar and stop vector loop
        t->vectorLength[t->operands[0]] = t->vectorLengthR = dataSizeTable[t->operandType];        
    }
    // invert condition if op odd
    branch ^= t->op;

    // conditional branch
    if (branch & 1) {
        t->ip += t->addrOperand * 4;             // add relative offset to IP
        t->returnType |= 0x2000;                 // debug output jump taken
    }
    return result.q; 
}

static uint64_t test_bit_jump_true(CThread * t) {
    // test bit number b in a, jump if zero
    // floating point operands are treated as integers with the same size
    SNum a = t->parm[1];                         // first parameter
    SNum b = t->parm[2];                         // second parameter
    if (t->fInstr->immSize) b = t->parm[4];      // avoid conversion of b to float
    uint8_t branch = 0;                          // treat bits out of range as zero

    // branch condition
    switch (t->operandType) {
    case 0:   // int8
        if (b.b < 8) branch = a.b >> b.b;
        break;
    case 1:   // int16
        if (b.s < 16) branch = uint8_t(a.s >> b.b);
        break;
    case 2:   // int32
    case 5:   // float
        if (b.i < 32) branch = uint8_t(a.i >> b.b);
        break;
    case 3:   // int64
    case 6:   // double
        if (b.q < 64) branch = uint8_t(a.q >> b.b);
        break;
    default:
        t->interrupt(INT_WRONG_PARAMETERS);
    }
    if (t->vect)  t->vect = 4;                             // stop vector loop

    // invert condition if op odd
    branch ^= t->op;
    // conditional branch
    if (branch & 1) {
        t->ip += t->addrOperand * 4;                       // add relative offset to IP
        t->returnType = 0x2000;                            // debug output jump taken
    }
    else t->returnType = 0x1000;                           // debug output jump taken
    t->running = 2;                                        // don't save result
    return 0; 
}

static uint64_t test_bits_and(CThread * t) {
    // jump if (a & b) == b
    SNum a = t->parm[1];                         // first parameter
    SNum b = t->parm[2];                         // second parameter
    if (t->fInstr->immSize) b = t->parm[4];      // avoid conversion of b to float
    int8_t branch = 0;                           // branch condition is inverted if op is odd

    // branch condition
    switch (t->operandType) {
    case 0:   // int8
        branch = (a.b & b.b) == b.b;  break;
    case 1:   // int16
        branch = (a.s & b.s) == b.s;  break;
    case 2:   // int32
    case 5:   // float
        branch = (a.i & b.i) == b.i;  break;
    case 3:   // int64
    case 6:   // double
        branch = (a.q & b.q) == b.q;  break;
    default:
        t->interrupt(INT_WRONG_PARAMETERS);
    }
    if (t->vect)  t->vect = 4;                             // stop vector loop

    // invert condition if op odd
    branch ^= t->op;
    // conditional branch 
    if (branch & 1) {
        t->ip += t->addrOperand * 4;                       // add relative offset to IP
        t->returnType = 0x2000;                            // debug output jump taken
    }
    else t->returnType = 0x1000;                           // debug output jump taken
    t->running = 2;                                        // don't save result
    return 0; 
}

static uint64_t test_bits_or(CThread * t) {
    // jump if (a & b) != 0
    SNum a = t->parm[1];                         // first parameter
    SNum b = t->parm[2];                         // second parameter
    if (t->fInstr->immSize) b = t->parm[4];      // avoid conversion of b to float
    int8_t branch = 0;                           // branch condition is inverted if op is odd
    // branch condition
    switch (t->operandType) {
    case 0:   // int8
        branch = (a.b & b.b) != 0;  break;
    case 1:   // int16
        branch = (a.s & b.s) != 0;  break;
    case 2:   // int32
    case 5:   // float
        branch = (a.i & b.i) != 0;  break;
    case 3:   // int64
    case 6:   // double
        branch = (a.q & b.q) != 0;  break;
    default:
        t->interrupt(INT_WRONG_PARAMETERS);
    }
    if (t->vect)  t->vect = 4;                             // stop vector loop
    // invert condition if op odd
    branch ^= t->op;
    // conditional branch 
    if (branch & 1) {
        t->ip += t->addrOperand * 4;                       // add relative offset to IP
        t->returnType = 0x2000;                            // debug output jump taken
    }
    else t->returnType = 0x1000;                           // debug output jump taken
    t->running = 2;                                        // don't save result
    return 0; 
} 


static uint64_t increment_compare_jump(CThread * t) {
    // result = a + 1. Jump if condition
    SNum a = t->parm[1];                         // first parameter
    SNum b = t->parm[2];                         // second parameter
    SNum result;                                 // result
    int8_t branch1 = 0;                          // jump if 1
    int8_t branch2 = 0;                          // jump if 1
    result.q = a.q + 1;                          // increment

    // branch condition
    switch (t->operandType) {
    case 0:   // int8
        branch1 = result.bs < b.bs;
        branch2 = result.bs > b.bs;
        break;
    case 1:   // int16
        branch1 = result.ss < b.ss;
        branch2 = result.ss > b.ss;
        break;
    case 2:   // int32
        branch1 = result.is < b.is;
        branch2 = result.is > b.is;
        break;
    case 3:   // int64
        branch1 = result.qs < b.qs;
        branch2 = result.qs > b.qs;
        break;
    default:
        t->interrupt(INT_WRONG_PARAMETERS);
    }
    // select instruction
    if ((t->op & 0x3E) != II_INCREMENT_COMPARE_JBELOW) {
        branch1 = branch2;       // increment_compare/jump_above
    }
    // invert condition if opj odd
    branch1 ^= t->op;
    // conditional branch
    if (branch1 & 1) {
        t->ip += t->addrOperand * 4;                       // add relative offset to IP
        t->returnType |= 0x2000;                           // debug output jump taken
    }
    return result.q; 
}

static uint64_t sub_maxlen_jump_pos(CThread * t) {
    // Subtract the maximum vector length (in bytes) from a general purpose register 
    // and jump if the result is positive. The 8-bit immediate operand indicates the 
    // operand type for which the maximum vector length is obtained.
    // The register operand must be a 64-bit general purpose register.
    SNum a = t->parm[1];                         // first parameter
    SNum b = t->parm[2];                         // second parameter
    SNum result;                                 // result
    int8_t branch = 0;                           // jump if 1
    // b indicates the operand type for which the maximum vector length is used.
    // to do: allow different maximum vector lengths for different operand types
    if (b.q > 7) t->interrupt(INT_WRONG_PARAMETERS);
    uint64_t maxlen = t->MaxVectorLength;

    result.q = a.q - maxlen;                     // subtract maximum length

    // branch condition
    switch (t->operandType) {
    case 0:   // int8
        branch = result.bs > 0;  break;
    case 1:   // int16
        branch = result.ss > 0;  break;
    case 2:   // int32
        branch = result.is > 0;  break;
    case 3:   // int64. This is the preferred operant types.
        branch = result.qs > 0;  break;
    default:
        t->interrupt(INT_WRONG_PARAMETERS);
    }
    // invert condition if op odd
    branch ^= t->op;
    // conditional branch
    if (branch & 1) {
        t->ip += t->addrOperand * 4;                       // add relative offset to IP
        t->returnType |= 0x2000;                           // debug output jump taken
    }
    return result.q; 
}
/*
static uint64_t sub_jump(CThread * t) {
    // subtract integers and jump unconditionally. optional
    SNum a = t->parm[1];                         // first parameter
    SNum b = t->parm[2];                         // second parameter
    SNum result;                                 // result
    int8_t unsignedOverflow = 0;                 // unsigned overflow detected
    int8_t signedOverflow = 0;                   // signed overflow detected
    result.q = a.q - b.q;                        // subtract integers
    // only integer types in g.p. registers allowed
    if (t->operandType > 3) t->interrupt(INT_WRONG_PARAMETERS);
    // overflow interrupts
    if (signedOverflow)   t->interrupt(INT_OVERFL_SIGN);
    if (unsignedOverflow) t->interrupt(INT_OVERFL_UNSIGN);

    // unconditional branch
    t->ip += t->addrOperand * 4;                 // add relative offset to IP
    t->returnType |= 0x2000;                     // debug output jump taken
    return result.q;                             // return result
}

static uint64_t add_jump(CThread * t) {
    // add integers and jump unconditionally. optional
    SNum a = t->parm[1];                         // first parameter
    SNum b = t->parm[2];                         // second parameter
    SNum result;                                 // result
    int8_t unsignedOverflow = 0;                 // unsigned overflow detected
    int8_t signedOverflow = 0;                   // signed overflow detected
    result.q = a.q + b.q;                        // add integers
    // only integer types in g.p. registers allowed
    if (t->operandType > 3) t->interrupt(INT_WRONG_PARAMETERS);
    // overflow interrupts
    if (signedOverflow)   t->interrupt(INT_OVERFL_SIGN);
    if (unsignedOverflow) t->interrupt(INT_OVERFL_UNSIGN);

    // unconditional branch
    t->ip += t->addrOperand * 4;                 // add relative offset to IP
    t->returnType |= 0x2000;                     // debug output jump taken
    return result.q;                             // return result
}*/

static uint64_t jump_call_58(CThread * t) {
    // op = 58: jump, 59: call
    // Format 1.6 and 2.5.0: Indirect jump or call with memory operand
    // Format 1.7 C, 2.5.4, and 3.1.0: Unconditional direct jump or call
    uint64_t target = 0;                         // target address

    // different instructions for different formats
    switch (t->fInstr->format2) {
    case 0x161: case 0x252: // Indirect jump or call with memory operand
        target = t->readMemoryOperand(t->memAddress);
        break;
    case 0x172: case 0x254: // Unconditional direct jump or call with relative address
        target = t->ip + t->addrOperand * 4;     // add relative offset to IP
        break;
    case 0x310: // Unconditional direct jump or call with absolute address
        target = t->addrOperand;                 // absolute address
        break;
    default: 
        t->interrupt(INT_WRONG_PARAMETERS);
        return 0;
    }
    if (target & 3) { // misaligned jump target
        t->interrupt(INT_MISALIGNED_JUMP);
        return 0;
    }

    if (t->op & 1) {
        // this is a call instruction. push return address
        t->callStack.push(t->ip);                // push return address on call stack
        if (t->callStack.numEntries() > t->callDept) t->callDept = t->callStack.numEntries();
    }
    t->ip = target;                              // jump to new address
    t->running = 2;                              // don't save result
    return 0;
}

static uint64_t multiway_and_indirect(CThread * t) {
    // op = 60: jump, 61: call
    // Format 1.6 and 2.5.2: Multiway jump or call with table of relative addresses
    // Format 1.7 C: Indirect jump or call to value of register 
    uint64_t target = 0;                         // target address
    uint64_t offset;                             // offset relative to reference point

    // different instructions for different formats
    switch (t->fInstr->format2) {
    case 0x162: case 0x252: // Indirect jump or call with memory operand
        offset = t->readMemoryOperand(t->memAddress);
        // sign extend table entry
        switch (t->operandType) {
        case 0:  // int8
            offset = (uint64_t)(int64_t)(int8_t)offset;  break;
        case 1:  // int16
            offset = (uint64_t)(int64_t)(int16_t)offset;  break;
        case 2:  // int32
            offset = (uint64_t)(int64_t)(int32_t)offset;  break;
        case 3:  // int64
            break;
        default:
            t->interrupt(INT_WRONG_PARAMETERS);
        }
        offset <<= 2;                            // scale by 4
        target = t->parm[1].q + offset;          // add reference point
        break;
    case 0x173: // Unconditional indirect jump or call to value of register
        target = t->registers[t->operands[0]];
        break;
    default: 
        t->interrupt(INT_WRONG_PARAMETERS);
        return 0;
    }
    if (target & 3) { // misaligned jump target
        t->interrupt(INT_MISALIGNED_JUMP);
        return 0;
    }

    if (t->op & 1) {
        // this is a call instruction. push return address
        t->callStack.push(t->ip);                // push return address on call stack
        if (t->callStack.numEntries() > t->callDept) t->callDept = t->callStack.numEntries();
    }
    t->ip = target;                              // jump to new address
    t->returnType = 0x2000;                      // debug output jump taken
    t->running = 2;                              // don't save result
    return 0;
}

static uint64_t return_62(CThread * t) {
    // Format 1.6: Normal function return
    // Format 1.7 C: system return
    uint64_t target = 0;                         // target address
    switch (t->fInstr->format2) {
    case 0x163: // return
        if (t->callStack.numEntries() == 0) {
            t->interrupt(INT_CALL_STACK);        // call stack empty
            target = t->entry_point;             // return to program start            
        }
        else {
            target = t->callStack.pop();         // pop return address
        }
        break;
    case 0x173: // system return
        // to do!
        target = t->ip;
        break;
    default:
        t->interrupt(INT_WRONG_PARAMETERS);
    }
    t->ip = target;                              // go to return address
    t->running = 2;                              // don't save result
    return 0;
}

static uint64_t syscall_63(CThread * t) {
    // Format 1.6: sys call. ID in register
    // Format 2.5.1, 2.5.7 and 3.1.0: sys call. ID in constants
    // Format 1.7 C: trap or filler
    // Format 2.5.5: conditional traps
    uint8_t rd = t->pInstr->a.rd;
    uint8_t rs = t->pInstr->a.rs;
    uint8_t rt = t->pInstr->a.rt;
    uint32_t mod;                                // module id
    uint32_t funcid;                             // function id
    switch (t->fInstr->format2) {
    case 0x163: // system call. ID in register
        if (t->operandType == 2) { // 16+16 bit
            mod = uint16_t(t->registers[rt] >> 16);
            funcid = uint16_t(t->registers[rt]);
        }
        else if (t->operandType == 3) {  // 32+32 bit
            mod = uint32_t(t->registers[rt] >> 32);
            funcid = uint32_t(t->registers[rt]);
        }
        else {t->interrupt(INT_WRONG_PARAMETERS); return 0;}
        t->systemCall(mod, funcid, rd, rs);
        break;
    case 0x251:  // system call. ID in constants
        mod = t->pInstr->s[3];
        funcid = t->pInstr->s[2];
        t->systemCall(mod, funcid, rd, rs);
        break;
    case 0x257:  // system call. ID in constants. no registers
        mod = t->pInstr->i[1];
        funcid = t->pInstr->s[0];
        t->systemCall(mod, funcid, 0, 0);
        break;    
    case 0x310: // system call. ID in constants
        mod = t->pInstr->i[2];
        funcid = t->pInstr->i[1];
        t->systemCall(mod, funcid, rd, rs);
        break;
    case 0x174: case 0x175: // trap or filler
        t->interrupt(t->pInstr->s[0]);
        break;
    case 0x255: // conditional traps
        if (t->pInstr->b[1] != 40) t->interrupt(INT_WRONG_PARAMETERS); // the only condition supported is unsigned above
        if (t->parm[1].i > t->parm[2].i) {       // check condition, unsigned compare
            t->interrupt(t->pInstr->b[0]);       // generate interrupt
        }
        break;
    default: 
        t->interrupt(INT_WRONG_PARAMETERS);
    }
    t->running = 2;                              // don't save result
    t->returnType = 0;                           // debug output written by system function
    return 0;
}

// Jump instructions, conditional and indirect
PFunc funcTab2[64] = {
    sub_jump_generic, sub_jump_generic, sub_jump_generic, sub_jump_generic,                        // 0 - 3
    sub_jump_generic, sub_jump_generic, sub_jump_generic, sub_jump_generic,                        // 4 - 7
    sub_jump_generic, sub_jump_generic, and_jump_zero, and_jump_zero,                              // 8 - 11
    or_jump_zero, or_jump_zero, xor_jump_zero, xor_jump_zero,                                      // 12 - 15                                                                   
    add_jump_generic, add_jump_generic, add_jump_generic, add_jump_generic,                        // 16 - 19
    add_jump_generic, add_jump_generic, add_jump_generic, add_jump_generic,                        // 20 - 23
    add_jump_generic, add_jump_generic, test_bit_jump_true, test_bit_jump_true,                    // 24 - 27
    test_bits_and, test_bits_and, test_bits_or, test_bits_or,                                      // 28 - 31
    compare_jump_generic, compare_jump_generic, compare_jump_generic, compare_jump_generic,        // 32 - 35
    compare_jump_generic, compare_jump_generic, compare_jump_generic, compare_jump_generic,        // 36 - 39
    compare_jump_generic, compare_jump_generic, 0, 0,                                              // 40 - 43
    0, 0, 0, 0,                                                                                    // 44 - 47
    increment_compare_jump, increment_compare_jump, increment_compare_jump, increment_compare_jump,// 48 - 51
    sub_maxlen_jump_pos, sub_maxlen_jump_pos, 0, 0,                                                // 52 - 55
    0, 0, jump_call_58, jump_call_58,                                                              // 56 - 59
    multiway_and_indirect, multiway_and_indirect, return_62, syscall_63                            // 60 - 63
};

// jump and call instructions with 24 bit offset
PFunc funcTab3[16] = {
    f_jump, f_jump, f_jump, f_jump, f_jump, f_jump, f_jump, f_jump, 
    f_call, f_call, f_call, f_call, f_call, f_call, f_call, f_call
};

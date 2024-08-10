/****************************  emulator3.cpp  ********************************
* Author:        Agner Fog
* date created:  2018-02-18
* Last modified: 2024-08-01
* Version:       1.13
* Project:       Binary tools for ForwardCom instruction set
* Description:
* Emulator: Execution functions for multiformat instructions
*
* Copyright 2018-2024 GNU General Public License http://www.gnu.org/licenses
*****************************************************************************/

#include "stdafx.h"

// get intrinsic functions for _mm_getcsr and _mm_setcsr to control floating point rounding and exceptions
#if defined(_M_X64) || defined(__x86_64__) || defined(__amd64) || defined(__SSE2__)
#if defined(__FMA__) || defined(__AVX2__)
#define FMA_AVAILABLE 1
#else 
#define FMA_AVAILABLE 0       // FMA instructions available
#endif
#if defined(_MSC_VER) && !FMA_AVAILABLE
#include <xmmintrin.h>
#else
#include <immintrin.h>
#endif
#define MCSCR_AVAILABLE 1      // MXCSR register available (x86-64 only)
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
        fprintf(stderr, "Warning: Emulator cannot control floating point exceptions and rounding mode on this platform");
        repeated = 1;
    }
}

void setRoundingMode(uint8_t r) {
    // change rounding mode
#if MCSCR_AVAILABLE
    // Rounding mode 4: "odd if not exact" is not supported on x86-64 platform.
    // Implement it by rounding up and down. If the two values are different then take the odd one.
    if (r == 4) r = 1;
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


uint32_t roundToHalfPrecision(float fresult, CThread * t) {
    // round a result to half precision, using the specified rounding mode
    uint32_t mask = t->parm[3].i;
    uint32_t roundingMode = (mask >> MSKI_ROUNDING) & 7;
    uint32_t hresult = float2half(fresult);                // convert to half
    uint32_t abshresult = hresult & 0x7FFF;                // hresult without sign bit
    float    bresult = half2float(hresult);                // convert back to float to check rounding
    if (isnan_or_inf_h(hresult)) 
        return hresult;

    switch (roundingMode) {
    case 0: default:   // ties to even
        break;
    case 1:            // down
        if (bresult > fresult && abshresult != 0) { // round down
            if (hresult & 0x8000) {  // negative
                if (abshresult < 0x7C00) hresult++;
            }
            else {  // positive
                hresult--;
            }
        }
        break;
    case 2:            // up
        if (bresult < fresult && abshresult != 0) {  // round up
            if (hresult & 0x8000) {  // negative
                hresult--;
            }
            else {  // positive
                if (abshresult < 0x7C00) hresult++;
            }
        }
        break;
    case 3:            // truncate
        if (hresult & 0x8000) {  // negative
            if (bresult < fresult && abshresult != 0) { 
                hresult--;       // round up towards 0
            }
        }
        else {                   // positive
            if (bresult > fresult && abshresult != 0) { 
                hresult--;       // round down towards 0
            }
        }
        break;
    case 4:                      // odd if not exact
        if (bresult != fresult && (hresult & 1) == 0) {
            // even and not exact. round to nearest odd
            int isNegative = (hresult & 0x8000) != 0;
            int isLow = bresult < fresult;
            if (isNegative ^ isLow) {  // fix the absense of logical xor operator
                if (abshresult < 0x7C00) hresult++;
            }
            else {
                if (abshresult != 0) hresult--;
            }
        }
        break;
    }
    return hresult;
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
    if (t->parm[3].b & 1) {
        uint64_t address = t->memAddress;                     // memory address
        if (t->vect) address += t->vectorOffset;
        t->writeMemoryOperand(value, address);
    }
    else {  // mask is 0. This instruction has no fallback. Don't write
        /*
        uint8_t fallback = t->operands[2];                 // mask is 0. get fallback
        if (fallback == 0x1F) value = 0;
        else if (t->vect) value = t->readVectorElement(fallback, t->vectorOffset);
        else value = t->registers[fallback];*/
    }
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
        t->interrupt(INT_WRONG_PARAMETERS);
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
    uint8_t options = 0;
    if (t->fInstr->tmplate == 0xE) options = t->pInstr->a.im5;

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
        t->interrupt(INT_WRONG_PARAMETERS);
        value = 0;
    }
    value <<= options;

    uint8_t r1 = t->operands[4];                           // first operand. g.p. register
    value += t->registers[r1];                             // read register with full size
    t->operandType = 3;                                    // change operand size of result    
    t->returnType = (t->returnType & ~7) | 3;              // debug return output
    if (t->vect) t->interrupt(INT_WRONG_PARAMETERS);
    return (uint64_t)value;
}

static uint64_t f_compare(CThread * t) {
    // compare two source operands and generate a boolean result
    // get condition code
    uint8_t cond = 0;
    uint32_t mask = t->parm[3].i;                          // mask register value or NUMCONTR
    if (t->fInstr->tmplate == 0xE && (t->fInstr->imm2 & 2)) {
        cond = t->pInstr->a.im5;                           // E template. get condition from IM5
    }
    // get operands
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    uint8_t operandType = t->operandType;
    if ((t->fInstr->imm2 & 4) && operandType < 5) {
        b = t->parm[4];                                    // avoid immediate operand shifted by imm3
    }
    if (t->pInstr->a.op1 == II_COMPARE_HH) {
        // float16 compare
        if (operandType != 1) t->interrupt(INT_WRONG_PARAMETERS);
        a.f = half2float(a.s);  // convert to float32
        b.f = half2float(b.s);
        operandType = 5;     // treat as float32
    }  
    uint64_t result = 0;
    uint8_t cond1 = cond >> 1 & 3;                         // bit 1 - 2 of condition
    bool isnan = false;

    // select operand type
    if (operandType < 5) {  // integer types
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
        case 3:   // abs(a) < abs(b). Not officially supported in version 1.11
            if (a.q & signBit) a.q = (~a.q + 1) & sizeMask;  // change sign. overflow allowed
            if (b.q & signBit) b.q = (~b.q + 1) & sizeMask;  // change sign. overflow allowed
            result = a.q < b.q;  
            break;
        }
    }
    else if (operandType == 5) {    // float
        //half2float(b.s)
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
    else if (operandType == 6) {   // double
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
    else t->interrupt(INT_WRONG_PARAMETERS);                   // unsupported type
    // invert result
    if (cond & 1) result ^= 1;

    // check for NAN
    if (isnan) {
        result = (cond >> 3) & 1;                          // bit 3 tells what to get if unordered
        //if (t->parm[3].i & MSK_FLOAT_NAN_LOSS) t->interrupt(INT_FLOAT_NAN_LOSS); // mask bit 29: trap if NAN loss
    }

    // mask and fallback
    uint8_t fallbackreg = t->operands[2];
    uint64_t fallback = (fallbackreg & 0x1F) != 0x1F ? t->readRegister(fallbackreg) : 0;
    switch (cond >> 4) {
    case 0:    // normal fallback
        if (!(mask & 1)) result = fallback;
        break;
    case 1:    // mask & result & fallback
        result &= mask & fallback;
        break;
    case 2:    // mask & (result | fallback)
        result = mask & (result | fallback);
        break;
    case 3:    // mask & (result ^ fallback)
        result = mask & (result ^ fallback);
        break;
    }
    if ((t->returnType & 7) >= 5) t->returnType -= 3;  // debug return output must be integer

    result &= 1;       // use only bit 0 of result
    if ((t->operands[1] & 0x1F) < 7) {
        // There is a mask. get remaining bits from mask
        result |= (t->parm[3].q & ~(uint64_t)1);
    }
    t->parm[3].b = 1;    // prevent normal mask operation
    return result;
}

uint64_t f_add(CThread * t) {
    // add two numbers
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    uint32_t mask = t->parm[3].i;
    SNum result;
    uint32_t roundingMode = (mask >> MSKI_ROUNDING) & 7;
    bool detectExceptions = (mask & (0xF << MSKI_EXCEPTIONS)) != 0;  // make NAN if exceptions
    uint8_t operandType = t->operandType;

    if (((mask ^ t->lastMask) & (1<<MSK_SUBNORMAL)) != 0) {
        // subnormal status changed
        enableSubnormals (mask & (1<<MSK_SUBNORMAL));
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
        if (roundingMode != 0) setRoundingMode(roundingMode);
        if (detectExceptions) clearExceptionFlags();       // clear previous exceptions
        result.f = a.f + b.f;                              // this is the actual addition
        if (roundingMode == 4) {
            // special case for rounding mode 4 (odd if not exact)
            setRoundingMode(2);                            // try with both round up and round down
            SNum roundUpResult;
            roundUpResult.f = a.f + b.f;
            if (roundUpResult.i & 1) result = roundUpResult; // choose the odd result
        }
        if (isnan_f(result.i)) {
            // the result is NAN but neither input is NAN. This must be INF-INF
            result.q = t->makeNan(nan_invalid_inf_sub_inf, operandType);
        }
        if (detectExceptions) {
            uint32_t x = getExceptionFlags();              // read exceptions
            if ((mask & (1<<MSK_OVERFLOW)) && (x & 8)) result.q = t->makeNan(nan_overflow_add, operandType);
            else if ((mask & (1<<MSK_UNDERFLOW)) && (x & 0x10)) result.q = t->makeNan(nan_underflow, operandType);
            else if ((mask & (1<<MSK_INEXACT)) && (x & 0x20)) result.q = t->makeNan(nan_inexact, operandType);
        }
        if (roundingMode != 0) setRoundingMode(0);              // reset rounding mode
    }

    else if (operandType == 6) {                           // double
        bool nana = isnan_d(a.q);                          // check for NAN input
        bool nanb = isnan_d(b.q);   
        if (nana && nanb) {                                // both are NAN
            return (a.q << 1) > (b.q << 1) ? a.q : b.q;    // return the biggest payload
        }
        else if (nana) return a.q;
        else if (nanb) return b.q;
        if (roundingMode != 0) setRoundingMode(roundingMode);
        if (detectExceptions) clearExceptionFlags();       // clear previous exceptions
        result.d = a.d + b.d;                              // this is the actual addition
        if (roundingMode == 4) {
            // special case for rounding mode 4 (odd if not exact)
            setRoundingMode(2);                            // try with both round up and round down
            SNum roundUpResult;
            roundUpResult.d = a.d + b.d;
            if (roundUpResult.q & 1) result = roundUpResult; // choose the odd result
        }
        if (isnan_d(result.q)) {
            // the result is NAN but neither input is NAN. This must be INF-INF
            result.q = t->makeNan(nan_invalid_inf_sub_inf, operandType);
        }
        if (detectExceptions) {
            uint32_t x = getExceptionFlags();              // read exceptions
            if ((mask & (1<<MSK_OVERFLOW)) && (x & 8)) result.q = t->makeNan(nan_overflow_add, operandType);
            else if ((mask & (1<<MSK_UNDERFLOW)) && (x & 0x10)) result.q = t->makeNan(nan_underflow, operandType);
            else if ((mask & (1<<MSK_INEXACT)) && (x & 0x20)) result.q = t->makeNan(nan_inexact, operandType);
        }
        if (roundingMode != 0) setRoundingMode(0);              // reset rounding mode
    }
    else {
        // unsupported operand type
        t->interrupt(INT_WRONG_PARAMETERS);
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
    uint32_t roundingMode = (mask >> MSKI_ROUNDING) & 7;
    bool detectExceptions = (mask & (0xF << MSKI_EXCEPTIONS)) != 0;  // make NAN if exceptions
    uint8_t operandType = t->operandType;
    if (((mask ^ t->lastMask) & (1<<MSK_SUBNORMAL)) != 0) {
        // subnormal status changed
        enableSubnormals (mask & (1<<MSK_SUBNORMAL));
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
        if (roundingMode != 0) setRoundingMode(roundingMode);
        if (detectExceptions) clearExceptionFlags();       // clear previous exceptions
        result.f = a.f - b.f;                              // this is the actual subtraction
        if (roundingMode == 4) {
            // special case for rounding mode 4 (odd if not exact)
            setRoundingMode(2);                            // try with both round up and round down
            SNum roundUpResult;
            roundUpResult.f = a.f - b.f;
            if (roundUpResult.i & 1) result = roundUpResult; // choose the odd result
        }
        if (isnan_f(result.i)) {
            // the result is NAN but neither input is NAN. This must be INF-INF
            result.q = t->makeNan(nan_invalid_inf_sub_inf, operandType);
        }
        if (detectExceptions) {
            uint32_t x = getExceptionFlags();              // read exceptions
            if ((mask & (1<<MSK_OVERFLOW)) && (x & 8)) result.q = t->makeNan(nan_overflow_add, operandType);
            else if ((mask & (1<<MSK_UNDERFLOW)) && (x & 0x10)) result.q = t->makeNan(nan_underflow, operandType);
            else if ((mask & (1<<MSK_INEXACT)) && (x & 0x20)) result.q = t->makeNan(nan_inexact, operandType);
        }
        if (roundingMode != 0) setRoundingMode(0);              // reset rounding mode
    }
    else if (operandType == 6) {// double
        bool nana = isnan_d(a.q);                          // check for NAN input
        bool nanb = isnan_d(b.q);   
        if (nana && nanb) {                                // both are NAN
            return (a.q << 1) > (b.q << 1) ? a.q : b.q;    // return the biggest payload
        }
        else if (nana) return a.q;
        else if (nanb) return b.q;
        if (roundingMode != 0) setRoundingMode(roundingMode);
        if (detectExceptions) clearExceptionFlags();       // clear previous exceptions
        result.d = a.d - b.d;                              // this is the actual subtraction
        if (roundingMode == 4) {
            // special case for rounding mode 4 (odd if not exact)
            setRoundingMode(2);                            // try with both round up and round down
            SNum roundUpResult;
            roundUpResult.d = a.d - b.d;
            if (roundUpResult.q & 1) result = roundUpResult; // choose the odd result
        }
        if (isnan_d(result.q)) {
            // the result is NAN but neither input is NAN. This must be INF-INF
            result.q = t->makeNan(nan_invalid_inf_sub_inf, operandType);
        }
        if (detectExceptions) {
            uint32_t x = getExceptionFlags();              // read exceptions
            if ((mask & (1<<MSK_OVERFLOW)) && (x & 8)) result.q = t->makeNan(nan_overflow_add, operandType);
            else if ((mask & (1<<MSK_UNDERFLOW)) && (x & 0x10)) result.q = t->makeNan(nan_underflow, operandType);
            else if ((mask & (1<<MSK_INEXACT)) && (x & 0x20)) result.q = t->makeNan(nan_inexact, operandType);
        }
        if (roundingMode != 0) setRoundingMode(0);              // reset rounding mode
    }
    else {
        // unsupported operand type
        t->interrupt(INT_WRONG_PARAMETERS);
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
    uint32_t roundingMode = (mask >> MSKI_ROUNDING) & 7;
    bool detectExceptions = (mask & (0xF << MSKI_EXCEPTIONS)) != 0;  // make NAN if exceptions
    uint8_t operandType = t->operandType;
    if (((mask ^ t->lastMask) & (1<<MSK_SUBNORMAL)) != 0) {
        // subnormal status changed
        enableSubnormals (mask & (1<<MSK_SUBNORMAL));
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
        if (roundingMode != 0) setRoundingMode(roundingMode);
        if (detectExceptions) clearExceptionFlags();       // clear previous exceptions
        result.f = a.f * b.f;                              // this is the actual multiplication
        if (roundingMode == 4) {
            // special case for rounding mode 4 (odd if not exact)
            setRoundingMode(2);                            // try with both round up and round down
            SNum roundUpResult;
            roundUpResult.f = a.f * b.f;
            if (roundUpResult.i & 1) result = roundUpResult; // choose the odd result
        }
        if (isnan_f(result.i)) {
            // the result is NAN but neither input is NAN. This must be 0*INF
            result.q = t->makeNan(nan_invalid_0mulinf, operandType);
        }
        if (detectExceptions) {
            uint32_t x = getExceptionFlags();              // read exceptions
            if ((mask & (1<<MSK_OVERFLOW)) && (x & 8)) result.q = t->makeNan(nan_overflow_mul, operandType);
            else if ((mask & (1<<MSK_UNDERFLOW)) && (x & 0x10)) result.q = t->makeNan(nan_underflow, operandType);
            else if ((mask & (1<<MSK_INEXACT)) && (x & 0x20)) result.q = t->makeNan(nan_inexact, operandType);
        }
        if (roundingMode != 0) setRoundingMode(0);         // reset rounding mode
    }

    else if (operandType == 6) { // double
        bool nana = isnan_d(a.q);                          // check for NAN input
        bool nanb = isnan_d(b.q);   
        if (nana && nanb) {                                // both are NAN
            return (a.q << 1) > (b.q << 1) ? a.q : b.q;    // return the biggest payload
        }
        else if (nana) return a.q;
        else if (nanb) return b.q;
        if (roundingMode != 0) setRoundingMode(roundingMode);
        if (detectExceptions) clearExceptionFlags();       // clear previous exceptions
        result.d = a.d * b.d;                              // this is the actual multiplication
        if (roundingMode == 4) {
            // special case for rounding mode 4 (odd if not exact)
            setRoundingMode(2);                            // try with both round up and round down
            SNum roundUpResult;
            roundUpResult.d = a.d * b.d;
            if (roundUpResult.q & 1) result = roundUpResult; // choose the odd result
        }
        if (isnan_d(result.q)) {
            // the result is NAN but neither input is NAN. This must be 0*INF
            result.q = t->makeNan(nan_invalid_0mulinf, operandType);
        }
        if (detectExceptions) {
            uint32_t x = getExceptionFlags();              // read exceptions
            if ((mask & (1<<MSK_OVERFLOW)) && (x & 8)) result.q = t->makeNan(nan_overflow_mul, operandType);
            else if ((mask & (1<<MSK_UNDERFLOW)) && (x & 0x10)) result.q = t->makeNan(nan_underflow, operandType);
            else if ((mask & (1<<MSK_INEXACT)) && (x & 0x20)) result.q = t->makeNan(nan_inexact, operandType);
        }
        if (roundingMode != 0) setRoundingMode(0);              // reset rounding mode
    }
    else {
        // unsupported operand type
        t->interrupt(INT_WRONG_PARAMETERS);
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
    uint32_t roundingMode = (mask >> MSKI_ROUNDING) & 7;   // floating point rounding mode
    bool detectExceptions = (mask & (0xF << MSKI_EXCEPTIONS)) != 0;  // make NAN if exceptions
    bool nana, nanb;                                       // inputs are NAN
    uint8_t operandType = t->operandType;
    uint8_t options = 0;
    uint8_t intRounding = 0;                              // integer rounding mode
    if (t->fInstr->tmplate == 0xE && (t->fInstr->imm2 & 2)) {
        options = t->pInstr->a.im5;
        intRounding = options & 3;
    }
    if (((mask ^ t->lastMask) & (1<<MSK_SUBNORMAL)) != 0) {
        // subnormal status changed
        enableSubnormals (mask & (1<<MSK_SUBNORMAL));
        t->lastMask = mask;
    }
    switch (operandType) {
    case 0:  // int8
        if (a.b == 0x80 && b.bs == -1) {  // division overflow
            result.i = 0x80; overflow = true;
        }
        else if (b.b == 0) { // signed division by zero
            result.i = a.bs < 0 ? 0x80 : 0x7F; 
            overflow = true;
        }
        else {
            result.i = a.bs / b.bs;
            if (intRounding != 0 && abs(b.bs) != 1) {
                int rem = a.bs % b.bs;
                switch (intRounding) {
                case 3: { // nearest or even
                    uint32_t r2 = 2*abs(rem);
                    uint32_t b2 = abs(b.bs);
                    int s = int8_t(a.i ^ b.i) < 0 ? -1 : 1;  // one with sign of result
                    if (r2 > b2 || (r2 == b2 && (result.b & 1))) result.i += s;
                    break;}
                case 1:  // down
                    if (rem != 0 && int8_t(a.i ^ b.i) < 0 && result.b != 0x80u) result.i--;
                    break;
                case 2:  // up
                    if (rem != 0 && int8_t(a.i ^ b.i) >= 0) result.i++;
                    break;
                }
            }
        }
        break;
    case 1:  // int16 or float16
        if ((options & 0x20) == 0) {  // int16
            if (a.s == 0x8000u && b.ss == -1) {  // division overflow
                result.i = 0x8000; overflow = true;
            }
            else if (b.s == 0) { // signed division by zero
                result.i = a.ss < 0 ? 0x8000 : 0x7FFF;
                overflow = true;
            }
            else {
                result.i = a.ss / b.ss;
                if (intRounding != 0 && abs(b.ss) != 1) {
                    int16_t rem = a.ss % b.ss;
                    switch (intRounding) {
                    case 3: { // nearest or even
                        uint16_t r2 = 2 * abs(rem);
                        uint16_t b2 = abs(b.is);
                        int16_t  s = int16_t(a.s ^ b.s) < 0 ? -1 : 1;  // one with sign of result
                        if (r2 > b2 || (r2 == b2 && (result.s & 1))) result.s += s;
                        break; }
                    case 1:  // down
                        if (rem != 0 && int16_t(a.s ^ b.s) < 0 && result.s != 0x8000u) result.s--;
                        break;
                    case 2:  // up
                        if (rem != 0 && int16_t(a.s ^ b.s) >= 0) result.s++;
                        break;
                    }
                }
            }
        }
        else {
            // float16
            float aa = half2float(a.s);
            float bb = half2float(b.s);
            nana = isnan_h(a.s);                                // check for NAN input
            nanb = isnan_h(b.s);
            if (nana && nanb) {                                 // both are NAN
                result.i = (a.s << 1) > (b.s << 1) ? a.s : b.s; // return the biggest payload
            }
            else if (nana) result.i = a.i;
            else if (nanb) result.i = b.i;
            else if (b.s << 1 == 0) { // division by zero
                if (a.s << 1 == 0) { // 0./0. = nan
                    result.q = t->makeNan(nan_invalid_0div0, operandType);
                }
                else {
                    // a / 0. = infinity
                    if (mask & (1 << MSK_DIVZERO)) result.q = t->makeNan(nan_div0, operandType);
                    else result.i = inf_h;
                }
                result.i |= (a.s ^ b.s) & 0x8000; // sign bit
            }
            else if (isinf_h(a.s) && isinf_h(b.s)) {
                result.i = (uint32_t)t->makeNan(nan_invalid_infdivinf, operandType); // INF/INF
                result.i |= (a.s ^ b.s) & 0x8000; // sign bit
            }
            else {
                if (detectExceptions) clearExceptionFlags();         // clear previous exceptions
                float r = aa / bb;                                   // normal division
                result.i = roundToHalfPrecision(r, t);               // round with specified rounding mode
                if (detectExceptions) {
                    uint32_t x = getExceptionFlags();                // read exceptions
                    if (isinf_h(result.s)) x = 8;                    // overflow
                    else if (result.s << 1 == 0 && r != 0) x = 0x10; // underflow
                    else if (r != half2float(result.s)) x = 0x20;    // inexact
                    if ((mask & (1 << MSK_OVERFLOW)) && (x & 8)) result.q = t->makeNan(nan_overflow_div, operandType);
                    else if ((mask & (1 << MSK_UNDERFLOW)) && (x & 0x10)) result.q = t->makeNan(nan_underflow, operandType);
                    else if ((mask & (1 << MSK_INEXACT)) && (x & 0x20)) result.q = t->makeNan(nan_inexact, operandType);
                }
            }
            t->returnType = 0x118;  // float16 return
        }
        break;
    case 2:  // int32
        if (a.i == 0x80000000 && b.is == -1) {  // division overflow
            result.i = 0x80000000; overflow = true;
        }
        else if (b.i == 0) { // signed division by zero
            result.i = a.is < 0 ? 0x80000000 : 0x7FFFFFFF; 
            overflow = true;
        }
        else {
            result.i = a.is / b.is;
            if (intRounding != 0 && abs(b.is) != 1) {
                int rem = a.is % b.is;
                switch (intRounding) {
                case 3: { // nearest or even
                    uint32_t r2 = 2*abs(rem);
                    uint32_t b2 = abs(b.is);
                    int s = int32_t(a.i ^ b.i) < 0 ? -1 : 1;  // one with sign of result
                    if (r2 > b2 || (r2 == b2 && (result.i & 1))) result.i += s;
                    break;}
                case 1:  // down
                    if (rem != 0 && int32_t(a.i ^ b.i) < 0 && result.i != 0x80000000u) result.i--;
                    break;
                case 2:  // up
                    if (rem != 0 && int32_t(a.i ^ b.i) >= 0) result.i++;
                    break;
                }
            }
        }
        break;
    case 3:  // int64
        if (a.q == sign_d && b.qs == int64_t(-1)) {  // division overflow
            result.q = sign_d; overflow = true;
        }
        else if (b.q == 0) { // signed division by zero
            result.q = a.qs < 0 ? sign_d : sign_d-1u; 
            overflow = true;
        }
        else {
            result.qs = a.qs / b.qs;
            if (intRounding != 0 && abs(b.qs) != 1) {
                int64_t rem = a.qs % b.qs;
                switch (intRounding) {
                case 3: { // nearest or even
                    uint64_t r2 = 2*abs(rem);
                    uint64_t b2 = abs(b.qs);
                    int64_t s = int64_t(a.q ^ b.q) < 0 ? -1 : 1;  // one with sign of result
                    if (r2 > b2 || (r2 == b2 && (result.i & 1))) result.q += s;
                    break;}
                case 1:  // down
                    if (rem != 0 && int64_t(a.q ^ b.q) < 0 && result.q != 0x8000000000000000u) result.q--;
                    break;
                case 2:  // up
                    if (rem != 0 && int64_t(a.q ^ b.q) >= 0) result.q++;
                    break;
                }
            }
        }
        break;
    case 5:  // float
        nana = isnan_f(a.i);                                // check for NAN input
        nanb = isnan_f(b.i);   
        if (nana && nanb) {                                 // both are NAN
            result.i = (a.i << 1) > (b.i << 1) ? a.i : b.i; // return the biggest payload
        }
        else if (nana) result.i = a.i;
        else if (nanb) result.i = b.i;
        else if (b.i << 1 == 0) { // division by zero
            if (a.i << 1 == 0) { // 0./0. = nan
                result.q = t->makeNan(nan_invalid_0div0, operandType);
            }
            else {
                // a / 0. = infinity
                if (mask & (1<<MSK_DIVZERO)) result.q = t->makeNan(nan_div0, operandType);
                else result.i = inf_f;
            }
            result.i |= (a.i ^ b.i) & sign_f; // sign bit
        }
        else if (isinf_f(a.i) && isinf_f(b.i)) {
            result.i = (uint32_t)t->makeNan(nan_invalid_infdivinf, operandType); // INF/INF
            result.i |= (a.i ^ b.i) & sign_f; // sign bit
        }
        else {
            if (roundingMode != 0) setRoundingMode(roundingMode);
            if (detectExceptions) clearExceptionFlags();       // clear previous exceptions
            result.f = a.f / b.f;                              // normal division
            if (roundingMode == 4) {
                // special case for rounding mode 4 (odd if not exact)
                setRoundingMode(2);                            // try with both round up and round down
                SNum roundUpResult;
                roundUpResult.f = a.f / b.f;
                if (roundUpResult.i & 1) result = roundUpResult; // choose the odd result
            }
            if (detectExceptions) {
                uint32_t x = getExceptionFlags();              // read exceptions
                if ((mask & (1<<MSK_OVERFLOW)) && (x & 8)) result.q = t->makeNan(nan_overflow_div, operandType);
                else if ((mask & (1<<MSK_UNDERFLOW)) && (x & 0x10)) result.q = t->makeNan(nan_underflow, operandType);
                else if ((mask & (1<<MSK_INEXACT)) && (x & 0x20)) result.q = t->makeNan(nan_inexact, operandType);
            }
            if (roundingMode != 0) setRoundingMode(0);              // reset rounding mode
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
                if (mask & (1<<MSK_DIVZERO)) result.q = t->makeNan(nan_div0, operandType);
                else result.q = inf_d;
            }
            result.q |= (a.q ^ b.q) & sign_d; // sign bit
        }
        else if (isinf_d(a.q) && isinf_d(b.q)) {
            result.q = t->makeNan(nan_invalid_infdivinf, operandType); // INF/INF
            result.q |= (a.q ^ b.q) & sign_d; // sign bit
        }
        else {
        if (roundingMode != 0) setRoundingMode(roundingMode);
            if (detectExceptions) clearExceptionFlags();       // clear previous exceptions
            result.d = a.d / b.d;                              // normal division
            if (roundingMode == 4) {
                // special case for rounding mode 4 (odd if not exact)
                setRoundingMode(2);                            // try with both round up and round down
                SNum roundUpResult;
                roundUpResult.d = a.d / b.d;
                if (roundUpResult.q & 1) result = roundUpResult; // choose the odd result
            }
            if (detectExceptions) {
                uint32_t x = getExceptionFlags();              // read exceptions

                //!!
                if (x & 0x10)
                    x += 0;


                if ((mask & (1<<MSK_OVERFLOW)) && (x & 8)) result.q = t->makeNan(nan_overflow_div, operandType);
                else if ((mask & (1<<MSK_UNDERFLOW)) && (x & 0x10)) result.q = t->makeNan(nan_underflow, operandType);
                else if ((mask & (1<<MSK_INEXACT)) && (x & 0x20)) result.q = t->makeNan(nan_inexact, operandType);
            }
            if (roundingMode != 0) setRoundingMode(0);         // reset rounding mode
        }
        break;
    default:
        t->interrupt(INT_WRONG_PARAMETERS);
        result.i = 0;
    }
    return result.q;
}

uint64_t f_div_u(CThread * t) {
    // divide two unsigned integer numbers

    if (t->operandType > 4) {
        return f_div(t);                         // floating point: same as f_div
    }
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    SNum result;
    bool overflow = false;
    uint32_t intRounding = 0;                              // integer rounding mode
    if (t->fInstr->tmplate == 0xE && (t->fInstr->imm2 & 2)) {
        intRounding = t->pInstr->a.im5 & 3;                // E template. get integer rounding mode from IM5
    }

    switch (t->operandType) {
    case 0:  // int8
        if (b.b == 0) {    // unsigned division by zero
            result.i = 0xFF; overflow = true;
        }
        else {
            result.i = a.b / b.b;
            if (intRounding >= 2 && b.b != 1) {
                uint32_t rem = a.b % b.b;
                switch (intRounding) {
                case 3:  // nearest or even
                    if (rem*2 > b.b || (rem*2 == b.b && (result.i & 1))) result.i++;
                    break;
                case 2:  // up
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
            if (intRounding >= 2 && b.s != 1) {
                uint32_t rem = a.s % b.s;
                switch (intRounding) {
                case 3:  // nearest or even
                    if (rem*2 > b.s || (rem*2 == b.s && (result.i & 1))) result.i++;
                    break;
                case 2:  // up
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
            if (intRounding >= 2 && b.i != 1) {
                uint32_t rem = a.i % b.i;
                switch (intRounding) {
                case 3:  // nearest or even
                    if (rem*2 > b.i || (rem*2 == b.i && (result.i & 1))) result.i++;
                    break;
                case 2:  // up
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
            if (intRounding >= 2 && b.q != 1) {
                uint64_t rem = a.q % b.q;
                switch (intRounding) {
                case 3:  // nearest or even
                    if (rem*2 > b.q || (rem*2 == b.q && (result.q & 1))) result.q++;
                    break;
                case 2:  // up
                    if (rem != 0) result.q++;
                    break;
                }
            }
        }
        break;
    default:
        t->interrupt(INT_WRONG_PARAMETERS);
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

static uint64_t f_div_rev_u(CThread * t) {
    // divide two numbers, b/a
    uint64_t temp = t->parm[2].q;  // swap operands
    t->parm[2].q = t->parm[1].q;
    t->parm[1].q = temp;
    uint64_t retval = f_div_u(t);
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
        t->interrupt(INT_WRONG_PARAMETERS);
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
        t->interrupt(INT_WRONG_PARAMETERS);
        result.q = 0;
    }
    return result.q;
}


static uint64_t f_rem(CThread * t) {
    // remainder/modulo of two signed numbers
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    //SNum mask = t->parm[3];
    SNum result;
    bool overflow = false;

    switch (t->operandType) {
    case 0:  // int8
        if (a.b == 0x80 && b.bs == -1) {
            result.i = 0x80; overflow = true;
        }
        else if (b.b == 0) {
            result.i = a.i; overflow = true;
        }
        else result.is = a.bs % b.bs;
        break;
    case 1:  // int16
        if (a.s == 0x8000 && b.ss == -1) {
            result.i = a.i; overflow = true;
        }
        else if (b.s == 0) {
            result.i = a.i; overflow = true;
        }
        else {
            result.is = int(a.ss) % int(b.ss);
        }
        break;
    case 2:  // int32
        if (a.i == sign_f && b.is == -1) {
            result.i = sign_f; overflow = true;
        }
        else if (b.i == 0) {
            result.i = a.i; overflow = true;
        }
        else result.is = a.is % b.is;
        break;
    case 3:  // int64
        if (a.q == sign_d && b.qs == int64_t(-1)) {
            result.q = sign_d; overflow = true;
        }
        else if (b.q == 0) {
            result.q = a.q; overflow = true;
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
            // the rounding mode is ignored as defined for the C++ standard function fmod
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
            // the rounding mode is ignored as defined for the C++ standard function fmod
        }
        break;
    default:
        t->interrupt(INT_WRONG_PARAMETERS);
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
    //SNum mask = t->parm[3];
    SNum result;
    bool overflow = false;

    switch (t->operandType) {
    case 0:  // int8
        if (b.b == 0) {
            result.i = a.i; overflow = true;
        }
        else result.i = a.b % b.b;
        break;
    case 1:  // int16
        if (b.s == 0) {
            result.i = a.i; overflow = true;
        }
        else result.i = a.s % b.s;
        break;
    case 2:  // int32
        if (b.i == 0) {
            result.i = a.i; overflow = true;
        }
        else result.i = a.i % b.i;
        break;
    case 3:  // int64
        if (b.q == 0) {
            result.q = a.q; overflow = true;
        }
        else result.q = a.q % b.q;
        break;
    default:
        t->interrupt(INT_WRONG_PARAMETERS);
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
    uint8_t options = 0;    // get options
    if (t->fInstr->tmplate == 0xE && (t->fInstr->imm2 & 2)) options = t->pInstr->a.im5;
    bool propagateNAN = (options & 1) == 0;
    bool absmin = (options & 2) != 0;
    bool zerolimit = (options & 4) != 0;
    bool isunsigned = (options & 8) != 0;

    switch (t->operandType) {
    case 0:   // int8
        if (isunsigned) result.i  = a.b  < b.b  ? a.b  : b.b;
        else if (zerolimit && (a.bs < 0 || b.bs < 0)) result.i = 0;
        else            result.is = a.bs < b.bs ? a.bs : b.bs;
        break;
    case 1:   // int16
        if ((options & 0x20) == 0) {   // int16
            if (isunsigned) result.i = a.s < b.s ? a.s : b.s;
            else if (zerolimit && (a.ss < 0 || b.ss < 0)) result.i = 0;
            else            result.is = a.ss < b.ss ? a.ss : b.ss;
        }
        else {  // float16
            if (absmin) {
                a.i &= 0x7FFF;  b.i &= 0x7FFF; // remove sign bits
            }
            result.i = half2float(a.s) < half2float(b.s) ? a.i : b.i;
            // check NANs
            isnan  = isnan_h(a.s);           // a is nan
            isnan |= isnan_h(b.s) << 1;      // b is nan
            if (isnan && propagateNAN) {
                // propagate NAN according to the 2019 revision of the IEEE-754 standard
                if (isnan == 1) result.i = a.i;
                else if (isnan == 2) result.i = b.i;
                else result.i = (a.s & 0x7FFF) > (b.s & 0x7FFF) ? a.i : b.i; // return the biggest payload
            }
            t->returnType = 0x118;  // float16 return
        }
        break;
    case 2:   // int32
        if (isunsigned) result.i = a.i < b.i ? a.i : b.i;
        else if (zerolimit && (a.is < 0 || b.is < 0)) result.i = 0;
        else            result.is = a.is < b.is ? a.is : b.is;
        break;
    case 3:   // int64
        if (isunsigned) result.q = a.q < b.q ? a.q : b.q;
        else if (zerolimit && (a.qs < 0 || b.qs < 0)) result.q = 0;
        else            result.qs = a.qs < b.qs ? a.qs : b.qs;
        break;
    case 5:   // float
        if (absmin) {
            a.i &= 0x7FFFFFFF;  b.i &= 0x7FFFFFFF; // remove sign bits
        }
        result.f = a.f < b.f ? a.f : b.f;
        // check NANs
        isnan  = isnan_f(a.i);           // a is nan
        isnan |= isnan_f(b.i) << 1;      // b is nan
        if (isnan && propagateNAN) { // propagate NAN according to the 2019 revision of the IEEE-754 standard
            if (isnan == 1) result.i = a.i;
            else if (isnan == 2) result.i = b.i;
            else result.i = (a.i << 1) > (b.i << 1) ? a.i : b.i; // return the biggest payload
        }
        break;
    case 6:   // double
        if (absmin) {
            a.q &= nsign_d;  b.q &= nsign_d; // remove sign bits
        }
        result.d = a.d < b.d ? a.d : b.d;
        // check NANs
        isnan  = isnan_d(a.q);           // a is nan
        isnan |= isnan_d(b.q) << 1;      // b is nan
        if (isnan && propagateNAN) { // propagate NAN according to the 2019 revision of the IEEE-754 standard
            if (isnan == 1) result.q = a.q;
            else if (isnan == 2) result.q = b.q;
            else result.q = (a.q << 1) > (b.q << 1) ? a.q : b.q; // return the biggest payload
        }
        break;
    default:
        t->interrupt(INT_WRONG_PARAMETERS);
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
    uint8_t options = 0;    // get options
    if (t->fInstr->tmplate == 0xE && (t->fInstr->imm2 & 2)) options = t->pInstr->a.im5;
    bool propagateNAN = (options & 1) == 0;
    bool absmax = (options & 2) != 0;
    bool isunsigned = (options & 8) != 0;

    switch (t->operandType) {
    case 0:   // int8
        if (isunsigned) result.i = a.b > b.b ? a.b : b.b;
        else            result.is = a.bs > b.bs ? a.bs : b.bs;
        break;
    case 1:   // int16
        if ((options & 0x20) == 0) {   // int16
            if (isunsigned) result.i = a.s > b.s ? a.s : b.s;
            else            result.is = a.ss > b.ss ? a.ss : b.ss;
        }
        else {  // float16
            if (absmax) {
                a.i &= 0x7FFF;  b.i &= 0x7FFF; // remove sign bits
            }
            result.i = half2float(a.s) > half2float(b.s) ? a.i : b.i;
            // check NANs
            isnan  = isnan_h(a.s);           // a is nan
            isnan |= isnan_h(b.s) << 1;      // b is nan
            if (isnan && propagateNAN) {
                // propagate NAN according to the 2019 revision of the IEEE-754 standard
                if (isnan == 1) result.i = a.i;
                else if (isnan == 2) result.i = b.i;
                else result.i = (a.s & 0x7FFF) > (b.s & 0x7FFF) ? a.i : b.i; // return the biggest payload
            }
            t->returnType = 0x118;  // float16 return
        }
        break;
    case 2:   // int32
        if (isunsigned) result.i = a.i > b.i ? a.i : b.i;
        else            result.is = a.is > b.is ? a.is : b.is;
        break;
    case 3:   // int64
        if (isunsigned) result.q = a.q > b.q ? a.q : b.q;
        else            result.qs = a.qs > b.qs ? a.qs : b.qs;
        break;
    case 5:   // float
        if (absmax) {
            a.i &= 0x7FFFFFFF;  b.i &= 0x7FFFFFFF; // remove sign bits
        }
        result.f = a.f > b.f ? a.f : b.f;
        // check NANs
        isnan  = isnan_f(a.i);           // a is nan
        isnan |= isnan_f(b.i) << 1;      // b is nan
        if (isnan && propagateNAN) {
            // propagate NAN according to the 2019 revision of the IEEE-754 standard
            if (isnan == 1) result.i = a.i;
            else if (isnan == 2) result.i = b.i;
            else result.i = (a.i << 1) > (b.i << 1) ? a.i : b.i; // return the biggest payload
        }
        break;
    case 6:   // double
        if (absmax) {
            a.q &= nsign_d;  b.q &= nsign_d; // remove sign bits
        }
        result.d = a.d > b.d ? a.d : b.d;
        // check NANs
        isnan  = isnan_d(a.q);           // a is nan
        isnan |= isnan_d(b.q) << 1;      // b is nan
        if (isnan && propagateNAN) {
            // propagate NAN according to the 2019 revision of the IEEE-754 standard
            if (isnan == 1) result.q = a.q;
            else if (isnan == 2) result.q = b.q;
            else result.q = (a.q << 1) > (b.q << 1) ? a.q : b.q; // return the biggest payload
        }
        break;
    default:
        t->interrupt(INT_WRONG_PARAMETERS);
        result.i = 0;
    }
    return result.q;
}


static uint64_t f_and(CThread * t) {
    // bitwise AND of two numbers
    return t->parm[1].q & t->parm[2].q;
}
/*
static uint64_t f_and_not(CThread * t) {
    // a & ~b
    return t->parm[1].q & ~ t->parm[2].q;
}*/

static uint64_t f_or(CThread * t) {
    // bitwise OR of two numbers
    return t->parm[1].q | t->parm[2].q;
}

static uint64_t f_xor(CThread * t) {
    // bitwise exclusive OR of two numbers
    return t->parm[1].q ^ t->parm[2].q;
}

static uint64_t f_select_bits(CThread * t) {
    // a & c | b & ~c
    return (t->parm[0].q & t->parm[2].q) | (t->parm[1].q & ~ t->parm[2].q);
}

static uint64_t f_shift_left(CThread * t) {
    // integer: a << b, float a * 2^b where b is interpreted as integer
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    //if (t->fInstr->immSize && t->operandType >= 5) b = t->parm[4]; // avoid conversion of b to float
    if ((t->operands[5] & 0x20) && t->operandType > 4) b = t->parm[4]; // avoid conversion of b to float

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
    case 5:   // float: mul_pow2
        if (isnan_f(a.i)) return a.q;  // a is nan
        exponent = a.i >> 23 & 0xFF;       // get exponent
        if (exponent == 0) return a.i & sign_f; // a is zero or subnormal. return zero
        exponent += b.i;                  // add integer to exponent
        if ((int32_t)exponent >= 0xFF) { // overflow
            if ((mask.i & (1<<MSK_OVERFLOW)) != 0) {  // make NAN if exception
                result.q = t->makeNan(nan_overflow_mul, 5);
            }
            else {
                result.i = inf_f;
            }
        }
        else if ((int32_t)exponent <= 0) { // underflow
            if ((mask.i & (1<<MSK_UNDERFLOW)) != 0) {  // make NAN if exception
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
    case 6:   // double: mul_pow2
        if (isnan_d(a.q)) return a.q;  // a is nan
        exponent = a.q >> 52 & 0x7FF;
        if (exponent == 0) return a.q & sign_d; // a is zero or subnormal. return zero
        exponent += b.q;                  // add integer to exponent
        if ((int64_t)exponent >= 0x7FF) { // overflow
            if ((mask.i & (1<<MSK_OVERFLOW)) != 0) {  // make NAN if exception
                result.q = t->makeNan(nan_overflow_mul, 6);
            }
            else {
                result.q = inf_d;
            }
        }
        else if ((int64_t)exponent <= 0) { // underflow
            if ((mask.i & (1<<MSK_UNDERFLOW)) != 0) {  // make NAN if exception
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
        t->interrupt(INT_WRONG_PARAMETERS);
        result.i = 0;
    }
    return result.q;
}

static uint64_t f_rotate(CThread * t) {
    // rotate bits left
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    //if (t->fInstr->immSize && t->operandType >= 5) b = t->parm[4]; // avoid conversion of b to float
    if ((t->operands[5] & 0x20) && t->operandType > 4) b = t->parm[4]; // avoid conversion of b to float

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
        t->interrupt(INT_WRONG_PARAMETERS);
        result.i = 0;
    }
    return result.q;
}

static uint64_t f_shift_right_s(CThread * t) {
    // integer only: a >> b, with sign extension
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    //if (t->fInstr->immSize && t->operandType >= 5) b = t->parm[4]; // avoid conversion of b to float
    if ((t->operands[5] & 0x20) && t->operandType > 4) b = t->parm[4]; // avoid conversion of b to float

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
        t->interrupt(INT_WRONG_PARAMETERS);
        result.i = 0;
    }
    return result.q;
}

static uint64_t f_shift_right_u(CThread * t) {
    // integer only: a >> b, with zero extension
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    //if (t->fInstr->immSize && t->operandType >= 5) b = t->parm[4]; // avoid conversion of b to float
    if ((t->operands[5] & 0x20) && t->operandType > 4) b = t->parm[4]; // avoid conversion of b to float

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
        t->interrupt(INT_WRONG_PARAMETERS);
        result.i = 0;
    }
    return result.q;
}

static uint64_t f_funnel_shift(CThread * t) {
    uint64_t shift_count = t->parm[2].q;
    if ((t->operands[5] & 0x20) && t->operandType > 4) shift_count = t->parm[4].q; // avoid conversion of c to float

    if (t->vect == 0) {     // g.p. registers. shift integers n bytes
        uint32_t dataSize = dataSizeTableBits[t->operandType]; //  operand size, bits
        uint64_t dataMask = dataSizeMask[t->operandType]; //  operand size mask, dataSize bits of 1        
        if ((shift_count & dataMask) >= dataSize) return 0;  // shift count out of range
        return ((t->parm[0].q & dataMask) >> shift_count | (t->parm[1].q & dataMask) << (dataSize - shift_count)) & dataMask;
    }
    else {  // vector registers. shift concatenated whole vectors n bytes down
       
        // The second operand may be a vector register of incomplete size or a broadcast memory operand.
        // Both input operands may be the same as the destination register.
        // The operand size may not match the shift count
        // The easiest way to handle all these cases is to copy both input vectors into temporary buffers
        switch (t->operandType) {
        case 0:
            *(t->tempBuffer + t->vectorOffset) =  t->parm[0].bs;
            *(t->tempBuffer + t->MaxVectorLength + t->vectorOffset) =  t->parm[1].bs;
            break;
        case 1:
            *(uint16_t*)(t->tempBuffer + t->vectorOffset) =  t->parm[0].s;
            *(uint16_t*)(t->tempBuffer + t->MaxVectorLength + t->vectorOffset) =  t->parm[1].s;
            break;
        case 2: case 5:
            *(uint32_t*)(t->tempBuffer + t->vectorOffset) =  t->parm[0].i;
            *(uint32_t*)(t->tempBuffer + t->MaxVectorLength + t->vectorOffset) =  t->parm[1].i;
            break;
        case 3: case 6:
            *(uint64_t*)(t->tempBuffer + t->vectorOffset) =  t->parm[0].q;
            *(uint64_t*)(t->tempBuffer + t->MaxVectorLength + t->vectorOffset) =  t->parm[1].q;
            break;
        case 4: case 7:  // to do: support 128 bits
            t->interrupt(INT_WRONG_PARAMETERS);
            break;
        }
        uint32_t dataSizeBytes = dataSizeTable[t->operandType]; //  operand size, bits
        if (t->vectorOffset + dataSizeBytes >= t->vectorLengthR) {
            // last iteration. Make the result
            uint8_t  rd = t->operands[0];         // destination vector
            shift_count *= dataSizeBytes;         // shift n elements
            if (shift_count >= t->vectorLengthR) {
                // shift count out of range. return 0
                memset(t->vectors.buf() + t->MaxVectorLength*rd, 0, t->vectorLengthR);
            }
            else {
                // copy upper part of first vector to lower part of destination vector
                memcpy(t->vectors.buf() + t->MaxVectorLength * rd, t->tempBuffer + shift_count, t->vectorLengthR - shift_count);
                // copy lower part of second vector to upper part of destination vector
                memcpy(t->vectors.buf() + t->MaxVectorLength * rd + (t->vectorLengthR - shift_count), t->tempBuffer + t->MaxVectorLength, shift_count);
            }
        }
        t->running = 2;        // don't save RD. It is saved by above code
        return 0;
    }
}    

static uint64_t f_set_bit(CThread * t) {
    // a | 1 << b
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    //if (t->fInstr->immSize && t->operandType >= 5) b = t->parm[4]; // avoid conversion of b to float
    if ((t->operands[5] & 0x20) && t->operandType > 4) b = t->parm[4]; // avoid conversion of b to float

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
        t->interrupt(INT_WRONG_PARAMETERS);
        result.i = 0;
    }
    return result.q;
}

static uint64_t f_clear_bit(CThread * t) {
    // a & ~ (1 << b)
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    //if (t->fInstr->immSize && t->operandType >= 5) b = t->parm[4]; // avoid conversion of b to float
    if ((t->operands[5] & 0x20) && t->operandType > 4) b = t->parm[4]; // avoid conversion of b to float
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
        t->interrupt(INT_WRONG_PARAMETERS);
        result.i = 0;
    }
    return result.q;
}

static uint64_t f_toggle_bit(CThread * t) {
    // a ^ (1 << b)
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    //if (t->fInstr->immSize && t->operandType >= 5) b = t->parm[4]; // avoid conversion of b to float
    if ((t->operands[5] & 0x20) && t->operandType > 4) b = t->parm[4]; // avoid conversion of b to float
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
        t->interrupt(INT_WRONG_PARAMETERS);
        result.i = 0;
    }
    return result.q;
}


static uint64_t f_test_bit(CThread * t) {
    // test a single bit: a >> b & 1
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    //if (t->fInstr->immSize && t->operandType >= 5) b = t->parm[4]; // avoid conversion of b to float
    if ((t->operands[5] & 0x20) && t->operandType > 4) b = t->parm[4]; // avoid conversion of b to float
    if (t->fInstr->imm2 & 4) {
        b = t->parm[4];                                    // avoid immediate operand shifted by imm3
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
        t->interrupt(INT_WRONG_PARAMETERS);
        result.i = 0;
    }
    // get additional options
    uint8_t options = 0;
    if (t->fInstr->tmplate == 0xE && (t->fInstr->imm2 & 2)) options = t->pInstr->a.im5;
    if (options & 4) result.b ^= 1;    // invert result
    if (options & 8) fallback.b ^= 1;  // invert fallback
    if (options & 0x10) mask.b ^= 1;   // invert mask
    switch (options & 3) {
    case 0:
        result.b = (mask.b & 1) ? result.b : fallback.b;
        break;
    case 1:
        result.b = mask.b  &  result.b & fallback.b;
        break;
    case 2:
        result.b = mask.b  & (result.b | fallback.b);
        break;
    case 3:
        result.b = mask.b  & (result.b ^ fallback.b);
    }
    // ignore other bits
    result.q &= 1;
    if (options & 0x20) {  // get remaining bits from flag or NUMCONTR
        result.q |= mask.q & ~(uint64_t)1;
    }
    // disable normal fallback process
    t->parm[3].b = 1;
    return result.q;
}

static uint64_t f_test_bits_and(CThread * t) {
    // Test if all the indicated bits are 1
    // result = (a & b) == b
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    //if (t->fInstr->immSize && t->operandType >= 5) b = t->parm[4]; // avoid conversion of b to float
    if ((t->operands[5] & 0x20) && t->operandType > 4) b = t->parm[4]; // avoid conversion of b to float
    if (t->fInstr->imm2 & 4) {
        b = t->parm[4];                                    // avoid immediate operand shifted by imm3
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
        t->interrupt(INT_WRONG_PARAMETERS);
        result.i = 0;
    }
    // get additional options
    uint8_t options = 0;
    if (t->fInstr->tmplate == 0xE && (t->fInstr->imm2 & 2)) options = t->pInstr->a.im5;
    if (options & 4) result.b ^= 1;    // invert result
    if (options & 8) fallback.b ^= 1;  // invert fallback
    if (options & 0x10) mask.b ^= 1;   // invert mask
    switch (options & 3) {
    case 0:
        result.b = (mask.b & 1) ? result.b : fallback.b;
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
    // ignore other bits
    result.q &= 1;
    if (options & 0x20) {  // get remaining bits from flag or NUMCONTR
        result.q |= mask.q & ~(uint64_t)1;
    }
    // disable normal fallback process
    t->parm[3].b = 1;
    return result.q;
}

static uint64_t f_test_bits_or(CThread * t) {
    // Test if at least one of the indicated bits is 1. 
    // result = (a & b) != 0
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    //if (t->fInstr->immSize && t->operandType >= 5) b = t->parm[4]; // avoid conversion of b to float
    if ((t->operands[5] & 0x20) && t->operandType > 4) b = t->parm[4]; // avoid conversion of b to float
    if (t->fInstr->imm2 & 4) {
        b = t->parm[4];                                    // avoid immediate operand shifted by imm3
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
        t->interrupt(INT_WRONG_PARAMETERS);
        result.i = 0;
    }
    // get additional options
    uint8_t options = 0;
    if (t->fInstr->tmplate == 0xE && (t->fInstr->imm2 & 2)) options = t->pInstr->a.im5;
    if (options & 4) result.b ^= 1;    // invert result
    if (options & 8) fallback.b ^= 1;  // invert fallback
    if (options & 0x10) mask.b ^= 1;   // invert mask
    switch (options & 3) {
    case 0:
        result.b = (mask.b & 1) ? result.b : fallback.b;
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
    // ignore other bits
    result.q &= 1;
    if (options & 0x20) {  // get remaining bits from flag or NUMCONTR
        result.q |= mask.q & ~(uint64_t)1;
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

uint64_t f_mul_add_h(CThread * t) {
    // a + b * c, float16
    SNum a = t->parm[0];
    SNum b = t->parm[1];
    SNum c = t->parm[2]; 
    uint32_t mask = t->parm[3].i;
    if (t->fInstr->imm2 & 4) c = t->parm[4];          // avoid immediate operand shifted by imm3
    if (t->fInstr->immSize == 1) c.s = float2half(c.bs);  // convert 8-bit integer to float16                                                          // get sign options
    uint8_t options = 0;
    if (t->fInstr->tmplate == 0xE && (t->fInstr->imm2 & 2)) options = t->pInstr->a.im5;
    if (t->vect == 2) { // odd vector element
        options >>= 1;
    }
    if (options & 1) a.s ^= 0x8000;                           // adjust sign
    if (options & 4) c.s ^= 0x8000;

    if (mask & (1<<MSK_INEXACT)) clearExceptionFlags();       // clear previous exceptions

    double resultd = (double)half2float(a.s) * (double)half2float(b.s) + (double)half2float(c.s);

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
            else result = (uint16_t)t->makeNan(nan_invalid_inf_sub_inf, 1);
        }
    }
    else if ((mask & (1<<MSK_OVERFLOW)) && isinf_h(result) && !parmInf) result = (uint16_t)t->makeNan(nan_overflow_fma, 1);
    else if ((mask & (1<<MSK_UNDERFLOW)) && is_zero_or_subnormal_h(result) && resultd != 0.0) result = (uint16_t)t->makeNan(nan_underflow, 1);
    else if ((mask & (1<<MSK_INEXACT)) && ((getExceptionFlags() & 0x20) != 0 || half2float(result) != resultd)) result = (uint16_t)t->makeNan(nan_inexact, 1);

    result = roundToHalfPrecision(float(resultd), t);
    t->returnType = 0x118;
    return result;
}

uint64_t f_mul_add(CThread * t) {
    // a * b + c, calculated with extra precision on the intermediate product

    // get options
    uint8_t options = 0;
    if (t->fInstr->tmplate == 0xE && (t->fInstr->imm2 & 2)) options = t->pInstr->a.im5;
    uint8_t operandType = t->operandType;

    if (options & 0x20) { // float16
        if (t->operandType != 1) t->interrupt(INT_WRONG_PARAMETERS);
        return f_mul_add_h(t);
    }

    SNum a = t->parm[0];
    SNum b = t->parm[1];
    SNum c = t->parm[2];
    if ((t->fInstr->imm2 & 4) && operandType < 5) {
        c = t->parm[4];                 // avoid immediate operand shifted by imm3
    }
    if (t->op == II_MUL_ADD2) {
        SNum t = b; b = c; c = t;       // swap last two operands
    }
    uint32_t mask = t->parm[3].i;
    SNum result;
    uint32_t roundingMode = (mask >> MSKI_ROUNDING) & 7;
    bool detectExceptions = (mask & (0xF << MSKI_EXCEPTIONS)) != 0;  // make NAN if exceptions

    bool unsignedOverflow = false;
    bool signedOverflow = false;
    if (t->vect == 2) { // odd vector element
        options >>= 1;
    }

    switch (operandType) {
    case 0:   // int8
        a.is = a.bs; b.is = b.bs;    // sign extend to avoid overflow during sign change
        if (options & 1) a.is = -a.is;
        if (options & 4) c.is = -c.is;
        result.is = a.is * b.is + c.bs;
        signedOverflow = result.is != result.bs;            
        unsignedOverflow = result.i != result.b;
        break;
    case 1:   // int16
        a.is = a.ss; b.is = b.ss;    // sign extend to avoid overflow during sign change
        if (options & 1) a.is = -a.is;
        if (options & 4) c.is = -c.is;
        result.is = a.is * b.is + c.ss;
        signedOverflow = result.is != result.ss;            
        unsignedOverflow = result.i != result.s;
        break;
    case 2:   // int32
        a.qs = a.is; b.qs = b.is;    // sign extend to avoid overflow during sign change
        if (options & 1) a.qs = -a.qs;
        if (options & 4) c.qs = -c.qs;
        result.qs = a.qs * b.qs + c.is;
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
            c.qs = -c.qs;
        }
        result.qs = a.qs * b.qs + c.qs;
        break;
    case 5:   // float
        if (options & 1) a.f = -a.f;
        if (options & 4) c.f = -c.f;
        if (roundingMode != 0) setRoundingMode(roundingMode);
        if (detectExceptions) clearExceptionFlags();       // clear previous exceptions
        result.f = mul_add_f(a.f, b.f, c.f);               // do the calculation
#if FMA_AVAILABLE 
        if (roundingMode == 4) {
            // special case for rounding mode 4 (odd if not exact)
            setRoundingMode(2);                            // try with both round up and round down
            SNum roundUpResult;
            roundUpResult.f = mul_add_f(a.f, b.f, c.f);    // repeat calculation with round up
            if (roundUpResult.i & 1) result = roundUpResult; // choose the odd result
        }
#else
        // rounding modes not supported
#endif
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
                else result.q = t->makeNan(nan_invalid_inf_sub_inf, operandType);
            }
        }
        else if (detectExceptions) {
            uint32_t x = getExceptionFlags();              // read exceptions
            if ((mask & (1<<MSK_OVERFLOW)) && (x & 8)) result.q = t->makeNan(nan_overflow_fma, operandType);
            else if ((mask & (1<<MSK_UNDERFLOW)) && (x & 0x10)) result.q = t->makeNan(nan_underflow, operandType);
            else if ((mask & (1<<MSK_INEXACT)) && (x & 0x20)) result.q = t->makeNan(nan_inexact, operandType);
        }
        if (roundingMode != 0) setRoundingMode(0);              // reset rounding mode
        break;

    case 6:   // double
        if (options & 1) a.d = -a.d;
        if (options & 4) c.d = -c.d;
        if (roundingMode != 0) setRoundingMode(roundingMode);
        if (detectExceptions) clearExceptionFlags();       // clear previous exceptions

        result.d = mul_add_d(a.d, b.d, c.d);               // do the calculation
#if FMA_AVAILABLE
        if (roundingMode == 4) {
            // special case for rounding mode 4 (odd if not exact)
            setRoundingMode(2);                            // try with both round up and round down
            SNum roundUpResult;
            roundUpResult.d = mul_add_d(a.d, b.d, c.d);    // repeat calculation with round up
            if (roundUpResult.i & 1) result = roundUpResult; // choose the odd result
        }
#else
        // rounding modes not supported
#endif
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
                else result.q = t->makeNan(nan_invalid_inf_sub_inf, operandType);
            }
        }
        else if (detectExceptions) {
            uint32_t x = getExceptionFlags();              // read exceptions
            if ((mask & (1<<MSK_OVERFLOW)) && (x & 8)) result.q = t->makeNan(nan_overflow_fma, operandType);
            else if ((mask & (1<<MSK_UNDERFLOW)) && (x & 0x10)) result.q = t->makeNan(nan_underflow, operandType);
            else if ((mask & (1<<MSK_INEXACT)) && (x & 0x20)) result.q = t->makeNan(nan_inexact, operandType);
        }
        if (roundingMode != 0) setRoundingMode(0);         // reset rounding mode
        break;

    default:
        t->interrupt(INT_WRONG_PARAMETERS);
        result.i = 0;
    }
    return result.q;
}

static uint64_t f_add_add(CThread * t) {
    // a + b + c, calculated with extra precision on the intermediate sum
    int i, j;
    SNum parm[3];
    // copy parameters so that we change sign and reorder them without changing original constant
    for (i = 0; i < 3; i++) parm[i] = t->parm[i];
    if ((t->fInstr->imm2 & 4) && t->operandType < 5) {
        parm[2] = t->parm[4];                                    // avoid immediate operand shifted by imm3
    }
    uint32_t mask = t->parm[3].i;
    uint32_t roundingMode = (mask >> MSKI_ROUNDING) & 7;
    bool detectExceptions = (mask & (0xF << MSKI_EXCEPTIONS)) != 0;  // make NAN if exceptions
    uint8_t operandType = t->operandType;
    SNum sumS, sumU;                             // signed and unsigned sums
    SNum nanS;                                   // combined nan's
    // get sign options
    uint8_t options = 0;
    if (t->fInstr->tmplate == 0xE && (t->fInstr->imm2 & 2)) options = t->pInstr->a.im5;
    //else if (t->fInstr->tmplate == 0xA) options = uint8_t(mask >> MSKI_OPTIONS);
    
    uint8_t signedOverflow = 0;
    uint8_t unsignedOverflow = 0;
    //bool parmInf = false;
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
            //if (temp1 == inf_f) parmInf = true;            // OR of all INFs
            options >>= 1;                                 // next option bit
        }
        if (nanS.i > inf_f) return nanS.i;                 // result is NAN
        // get the smallest operand last to minimize loss of precision if the two biggest operands have opposite signs
        temp1 = parm[j].i;
        parm[j].i = parm[2].i;
        parm[2].i = temp1;

        //if (roundingMode != 0) setRoundingMode(roundingMode);
        if (detectExceptions) clearExceptionFlags();       // clear previous exceptions

        // calculate sum
        sumU.f = (parm[0].f + parm[1].f) + parm[2].f;

        if (isnan_f(sumU.i)) {
            // the result is NAN but neither input is NAN. This must be INF-INF
            sumU.q = t->makeNan(nan_invalid_inf_sub_inf, operandType);
        }
        if (detectExceptions) {
            uint32_t x = getExceptionFlags();              // read exceptions
            if ((mask & (1<<MSK_OVERFLOW)) && (x & 8)) sumU.q = t->makeNan(nan_overflow_add, operandType);
            else if ((mask & (1<<MSK_UNDERFLOW)) && (x & 0x10)) sumU.q = t->makeNan(nan_underflow, operandType);
            else if ((mask & (1<<MSK_INEXACT)) && (x & 0x20)) sumU.q = t->makeNan(nan_inexact, operandType);
        }
        //if (roundingMode != 0) setRoundingMode(0);              // reset rounding mode
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
            //if (temp2 == inf_d) parmInf = true;            // OR of all INFs
            options >>= 1;                                 // next option bit
        }
        if (nanS.q > inf_d) return nanS.q;                 // result is NAN

        // get the smallest operand last to minimize loss of precision if 
        // the two biggest operands have opposite signs
        temp2 = parm[j].q;
        parm[j].q = parm[2].q;
        parm[2].q = temp2;
        // if (roundingMode != 0) setRoundingMode(roundingMode);
        if (detectExceptions) clearExceptionFlags();       // clear previous exceptions

        // calculate sum
        sumU.d = (parm[0].d + parm[1].d) + parm[2].d;

        if (isnan_d(sumU.q)) {
            // the result is NAN but neither input is NAN. This must be INF-INF
            sumU.q = t->makeNan(nan_invalid_inf_sub_inf, operandType);
        }
        if (detectExceptions) {
            uint32_t x = getExceptionFlags();              // read exceptions
            if ((mask & (1<<MSK_OVERFLOW)) && (x & 8)) sumU.q = t->makeNan(nan_overflow_add, operandType);
            else if ((mask & (1<<MSK_UNDERFLOW)) && (x & 0x10)) sumU.q = t->makeNan(nan_underflow, operandType);
            else if ((mask & (1<<MSK_INEXACT)) && (x & 0x20)) sumU.q = t->makeNan(nan_inexact, operandType);
        }
        if (roundingMode != 0) setRoundingMode(0);
        break;

    default:
        t->interrupt(INT_WRONG_PARAMETERS);
        sumU.i = 0;
    }
    return sumU.q;
}

uint64_t f_add_h(CThread * t) {
    // add two numbers, float16
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    uint32_t mask = t->parm[3].i;
    uint16_t result = 0;
    t->returnType =0x118;

    if (t->fInstr->immSize == 1) b.s = float2half(b.bs);  // convert 8-bit integer to float16
    if (t->operandType != 1) t->interrupt(INT_WRONG_PARAMETERS);
    //if (mask & (1<<MSK_INEXACT)) clearExceptionFlags();       // clear previous exceptions

    // may get double rounding errors
    float resultf = half2float(a.s) + half2float(b.s);  // calculate with single precision
    result = roundToHalfPrecision(resultf, t);

    // check for NaN
    if (isnan_h(result)) {
        if (isnan_h(a.s) && isnan_h(b.s)) {    // both are NAN
            result = (a.s << 1) > (b.s << 1) ? a.s : b.s; // return the biggest payload
        }
        else if (isnan_h(a.s)) result = a.s;
        else if (isnan_h(b.s)) result = b.s;
        else result = (uint16_t)t->makeNan(nan_invalid_inf_sub_inf, 1);
        return result;
    }
    // check for exceptions
    if ((mask & (1<<MSK_OVERFLOW)) && isinf_h(result) && !isinf_h(a.s) && !isinf_h(b.s)) {
        // overflow
        result = (uint16_t)t->makeNan(nan_overflow_add, 1);
        result |= (a.s ^ b.s) & 0x8000;  // get the sign
    }
    else if ((mask & (1<<MSK_UNDERFLOW)) && is_zero_or_subnormal_h(result) && resultf != 0.0f) {
        // underflow
        result = (uint16_t)t->makeNan(nan_underflow, 1) | (result & 0x8000); // signed NAN
    }
    //else if ((mask & (1<<MSK_INEXACT)) && (half2float(result) != resultf || (getExceptionFlags() & 0x20)) != 0) {
    else if ((mask & (1<<MSK_INEXACT)) && half2float(result) != resultf) {
        // inexact
        result = (uint16_t)t->makeNan(nan_inexact, 1);
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
    uint16_t result = 0;
    t->returnType =0x118;

    if (t->fInstr->immSize == 1) b.s = float2half(b.bs);  // convert 8-bit integer to float16
    if (t->operandType != 1) t->interrupt(INT_WRONG_PARAMETERS);
    //if (mask & (1<<MSK_INEXACT)) clearExceptionFlags();       // clear previous exceptions

    float resultf = half2float(a.s) - half2float(b.s);    // calculate with single precision
    result = roundToHalfPrecision(resultf, t);

    // check for NaN
    if (isnan_h(result)) {
        if (isnan_h(a.s) && isnan_h(b.s)) {    // both are NAN
            result = (a.s << 1) > (b.s << 1) ? a.s : b.s; // return the biggest payload
        }
        else if (isnan_h(a.s)) result = a.s;
        else if (isnan_h(b.s)) result = b.s;
        else result = (uint16_t)t->makeNan(nan_invalid_inf_sub_inf, 1);
        return result;
    }
    // check for exceptions
    if ((mask & (1<<MSK_OVERFLOW)) && isinf_h(result) && !isinf_h(a.s) && !isinf_h(b.s)) {
        // overflow
        result = (uint16_t)t->makeNan(nan_overflow_add, 1);
        result |= (a.s ^ b.s) & 0x8000;  // get the sign
    }
    else if ((mask & (1<<MSK_UNDERFLOW)) && is_zero_or_subnormal_h(result) && resultf != 0.0f) {
        // underflow
        result = (uint16_t)t->makeNan(nan_underflow, 1) | (result & 0x8000); // signed NAN
    }
    //else if ((mask & (1<<MSK_INEXACT)) && (half2float(result) != resultf || (getExceptionFlags() & 0x20)) != 0) {
    else if ((mask & (1<<MSK_INEXACT)) && half2float(result) != resultf) {
        // inexact
        result = (uint16_t)t->makeNan(nan_inexact, 1);
    }
    return result;
}

uint64_t f_mul_h(CThread * t) {
    // multiply two numbers, float16
    SNum a = t->parm[1];
    SNum b = t->parm[2];
    uint32_t mask = t->parm[3].i;
    uint16_t result = 0;
    t->returnType =0x118;

    if (t->fInstr->immSize == 1) b.s = float2half(b.bs);  // convert 8-bit integer to float16
    if (t->operandType != 1) t->interrupt(INT_WRONG_PARAMETERS);
    if (mask & (1<<MSK_INEXACT)) clearExceptionFlags();       // clear previous exceptions

    // single precision is sufficient to get an exact multiplication result
    float resultf = half2float(a.s) * half2float(b.s);  // calculate with single precision
    result = roundToHalfPrecision(resultf, t);

    // check for NaN
    if (isnan_h(result)) {
        if (isnan_h(a.s) && isnan_h(b.s)) {    // both are NAN
            result = (a.s << 1) > (b.s << 1) ? a.s : b.s; // return the biggest payload
        }
        else if (isnan_h(a.s)) result = a.s;
        else if (isnan_h(b.s)) result = b.s;
        else result = (uint16_t)t->makeNan(nan_invalid_inf_sub_inf, 1);
        return result;
    }
    // check for exceptions
    if ((mask & (1<<MSK_OVERFLOW)) && isinf_h(result) && !isinf_h(a.s) && !isinf_h(b.s)) {
        // overflow
        result = (uint16_t)t->makeNan(nan_overflow_mul, 1);
        result |= (a.s ^ b.s) & 0x8000;  // get the sign
    }
    else if ((mask & (1<<MSK_UNDERFLOW)) && is_zero_or_subnormal_h(result) && resultf != 0.0f) {
        // underflow
        result = (uint16_t)t->makeNan(nan_underflow, 1) | (result & 0x8000); // signed NAN
    }
    else if ((mask & (1<<MSK_INEXACT)) && (half2float(result) != resultf || (getExceptionFlags() & 0x20)) != 0) {
        // inexact
        result = (uint16_t)t->makeNan(nan_inexact, 1);
    }
    return result;
}

// Tables of function pointers

// multiformat instructions
PFunc funcTab1[64] = {
    f_nop, f_store, f_move, f_prefetch, f_sign_extend, f_sign_extend_add, f_compare, f_compare,  // 0 - 7
    f_add, f_sub, f_sub_rev, f_mul, f_mul_hi, f_mul_hi_u, f_div, f_div_u,                         // 8 - 15
    f_div_rev, f_div_rev_u, f_rem, f_rem_u, f_min, f_max, 0, 0,                       // 16 - 23
    0, 0, f_and, f_or, f_xor, 0, 0, 0,                             // 24 - 31
    f_shift_left, f_rotate, f_shift_right_s, f_shift_right_u, f_clear_bit, f_set_bit, f_toggle_bit, f_test_bit,  // 32 - 39
    f_test_bits_and, f_test_bits_or, 0, 0, f_add_h, f_sub_h, f_mul_h, 0,          // 40 - 47
    0, f_mul_add, f_mul_add, f_add_add, f_select_bits, f_funnel_shift, 0, 0,                           // 48 - 55
    0, 0, 0, 0, 0, 0, 0, 0                                                               // 56 - 63
};
    
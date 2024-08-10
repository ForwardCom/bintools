/****************************  main.cpp   *******************************
* Author:        Agner Fog
* Date created:  2017-04-17
* Last modified: 2024-07-29
* Version:       1.13
* Project:       Binary tools for ForwardCom instruction set
* Description:   This includes assembler, disassembler, linker, library
*                manager, and emulator in one program
*
* Instructions:
* Run with option -h for help
*
* For detailed instructions, see forwardcom.pdf
*
* (c) Copyright 2017-2024 GNU General Public License version 3
* https://www.gnu.org/licenses
*****************************************************************************/

#include "stdafx.h"

// Check if running on little endian system
static void CheckEndianness();

// Buffer for symbol names is made global in order to make it accessible to operators:
// bool operator < (ElfFWC_Sym2 const &, ElfFWC_Sym2 const &)
// bool operator < (SStringEntry const & a, SStringEntry const & b)
// bool operator < (SSymbolEntry const & a, SSymbolEntry const & b)
CTextFileBuffer symbolNameBuffer;      // Buffer for symbol names during assembly, linking, and library operations


                                       // Main. Program starts here
int main(int argc, char * argv[]) {
    CheckEndianness();                  // Check that machine is little-endian

#ifdef  _DEBUG
   // For debugging only: Read command line from file resp.txt
    if (argc == 1) {
        char commandline[] = "@resp.txt";
        char * dummyarg[] = { argv[0],  commandline};
        argc = 2; argv = dummyarg;
    }
#endif

    cmd.readCommandLine(argc, argv);             // Read command line parameters   
    if (cmd.job == CMDL_JOB_HELP) return 0;      // Help screen has been printed. Do nothing else

    CConverter maincvt;                          // This object takes care of all conversions etc.
    maincvt.go();                                // Do everything the command line says

    if (cmd.verbose && cmd.job != CMDL_JOB_EMU)  printf("\n"); // End with newline
    if (err.getWorstError()) cmd.mainReturnValue = err.getWorstError(); // Return with error code
    return cmd.mainReturnValue;
}


CConverter::CConverter() {
    // Constructor
}

void CConverter::go() {
    // Do whatever the command line parameters say

    switch (cmd.job) {
    case CMDL_JOB_DUMP:
        // File dump requested
        readInputFile();
        if (err.number()) return;
        switch (fileType) {
        case FILETYPE_FWC: case FILETYPE_ELF:
            dumpELF();  break;
        default:
            err.submit(ERR_DUMP_NOT_SUPPORTED, getFileFormatName(fileType));  // Dump of this file type not supported
        }
        printf("\n");                              // New line
        break;

    case CMDL_JOB_ASS:
        // assemble
        readInputFile();
        if (err.number()) return;
        assemble();
        break;

    case CMDL_JOB_DIS:
        // disassemble
        readInputFile();
        if (err.number()) return;
        disassemble();
        break;

    case CMDL_JOB_LINK:
    case CMDL_JOB_RELINK:        
        link();          // linker
        break;

    case CMDL_JOB_LIB:
        readInputFile();
        if (err.number()) return;
        lib();        // library manager
        break;

    case CMDL_JOB_EMU:
        emulate();    // emulator
        break;

    case 0: return; // no job. command line error

    default:
        err.submit(ERR_INTERNAL);
    }
}

// read input file
void CConverter::readInputFile() {
    // Ignore nonexisting filename when building library
    int IgnoreError = (cmd.fileOptions & CMDL_FILE_IN_IF_EXISTS);
    // Read input file
    read(cmd.getFilename(cmd.inputFile), IgnoreError);
    if (cmd.job == CMDL_JOB_ASS) fileType = FILETYPE_ASM;
    else getFileType();                 // Determine file type
    if (err.number()) return;           // Return if error
    cmd.inputType = fileType;           // Save input file type in cmd for access from other modules
    if (cmd.outputType == 0) {
        // desired type not specified
        cmd.outputType = fileType;
    }
}

void CConverter::dumpELF() {
    // Dump ELF file
    // Make object for interpreting 32 bit ELF file
    CELF elf;
    *this >> elf;                      // Give it my buffer
    elf.parseFile();                   // Parse file buffer
    if (err.number()) return;          // Return if error
    elf.dump(cmd.dumpOptions);         // Dump file
    *this << elf;                      // Take back my buffer
}

void CConverter::assemble() {
    // Aassemble to ELF file
    // Make instance of assembler
    CAssembler ass;
    if (err.number()) return;
    *this >> ass;                      // Give it my buffer
    ass.go();                          // run
} 

void CConverter::disassemble() {
    // Disassemble ELF file
    // Make instance of disassembler
    CDisassembler dis;
    if (err.number()) return;
    *this >> dis;                      // Give it my buffer
    dis.parseFile();                   // Parse file buffer
    if (err.number()) return;          // Return if error
    dis.getComponents1();              // Get components from ELF file
    dis.go();                          // Convert
}

void CConverter::lib() {
    // Library manager
    // Make instance of library manager
    CLibrary libmanager;
    if (err.number()) return;
    *this >> libmanager;               // Give it my buffer
    libmanager.go();                   // Do the job
}

void CConverter::link() {
    // Linker
    // Make instance of linker
    CLinker linker;
    linker.go();                   // Do the job
}

void CConverter::emulate() {
    // Emulator
    // Make instance of linker
    CEmulator emulator;
    emulator.go();                   // Do the job
}

// Convert half precision floating point number to single precision
// Optional support for subnormals
// NAN payload is right-justified for ForwardCom
float half2float(uint32_t half, bool supportSubnormal) {
    union {
        uint32_t hhh;
        float fff;
        struct {
            uint32_t mant: 23;
            uint32_t expo:  8;
            uint32_t sign:  1;
        };
    } u;

    u.hhh  = (half & 0x7fff) << 13;              // Exponent and mantissa
    u.hhh += 0x38000000;                         // Adjust exponent bias
    if ((half & 0x7C00) == 0) {// Subnormal
        if (supportSubnormal) {
            u.hhh = 0x3F800000 - (24 << 23);     // 2^-24
            u.fff *= int(half & 0x3FF);          // subnormal value = mantissa * 2^-24
        }
        else {        
            u.hhh = 0;                           // make zero
        }
    }
    if ((half & 0x7C00) == 0x7C00) {             // infinity or nan
        u.expo = 0xFF;
        if (half & 0x3FF) {  // nan
            u.mant = (half & 0x3FF) << 13;       // left-justify NaN payload
        }
    }
    u.hhh |= (half & 0x8000) << 16;              // sign bit
    return u.fff;
}

// Convert floating point number to half precision.
// Round to nearest or even. 
// Optional support for subnormals
// NAN payload is right-justified
uint16_t float2half(float x, bool supportSubnormal) {
    union {                                      // single precision float
        float f;
        struct {
            uint32_t mant: 23;
            uint32_t expo:  8;
            uint32_t sign:  1;
        };
    } u;
    union {                                      // half precision float
        uint16_t h;
        struct {
            uint16_t mant: 10;
            uint16_t expo:  5;
            uint16_t sign:  1;
        };
    } v;
    u.f = x;
    v.expo = u.expo - 0x70;                      // adjust exponent bias
    v.mant = u.mant >> 13;                       // get upper part of mantissa
    if (u.mant & (1 << 12)) {                    // round to nearest or even
        if ((u.mant & ((1 << 12) - 1)) || (v.mant & 1)) { // round up if odd or remaining bits are nonzero
            v.h++;                               // overflow here will carry into exponent
        }
    }
    v.sign = u.sign;                             // copy sign bit
    if (u.expo == 0xFF) {                        // infinity or NaN
        v.expo = 0x1F;
        if (u.mant != 0) {                       // NaN
            v.mant = u.mant >> 13;               // No rounding
            if (v.mant == 0) v.mant = 0x200;     // make sure it is still a NaN
        }
    }
    else if (u.expo > 0x8E) {
        v.expo = 0x1F;  v.mant = 0;              // overflow -> inf
    }
    else if (u.expo < 0x71) {
        v.expo = 0;
        if (supportSubnormal) {
            u.expo += 24;
            u.sign = 0;
            int mants = int(u.f);                // convert subnormal
            v.mant = mants & 0x3FF;
            if (mants == 0x400) v.expo = 1;      // rounded to normal
        }
        else {        
            v.mant = 0;                          // underflow -> 0
        }
    }
    return v.h;   
}

// Convert double precision floating point number to half precision. 
// subnormals optionally supported
// Nan payloads not preserved
uint16_t double2half(double x, bool supportSubnormal) {
    union {
        double d;
        struct {
            uint64_t mant: 52;
            uint64_t expo: 11;
            uint64_t sign:  1;
        };
    } u;
    union {
        uint16_t h;
        struct {
            uint16_t mant: 10;
            uint16_t expo:  5;
            uint16_t sign:  1;
        };
    } v;
    u.d = x;
    v.mant = u.mant >> 42;                       // get upper part of mantissa
    if (u.mant & ((uint64_t)1 << 41)) {          // round to nearest or even
        if ((u.mant & (((uint64_t)1 << 41) - 1)) || (v.mant & 1)) { // round up if odd or remaining bits are nonzero
            v.h++;                               // overflow here will give infinity
        }
    }
    v.expo = u.expo - 0x3F0;
    v.sign = u.sign;
    if (u.expo == 0x7FF) {
        v.expo = 0x1F;                           // infinity or nan
        if (u.mant != 0 && v.mant == 0) v.mant = 0x200;  // make sure output is a nan if input is nan
    }
    else if (u.expo > 0x40E) {
        v.expo = 0x1F;  v.mant = 0;              // overflow -> inf
    }
    else if (u.expo < 0x3F1) {                   // underflow
        v.expo = 0;
        if (supportSubnormal) {
            u.expo += 24;
            u.sign = 0;
            v.mant = int(u.d) & 0x3FF;
        }
        else {        
            v.mant = 0;                          // underflow -> 0
        }
    }
    return v.h;   
}


// Check that we are running on a machine with little-endian memory 
// organization and right data representation
static void CheckEndianness() {
    static uint8_t bytes[4] = { 1, 2, 3, 0xC0 };
    uint8_t * bb = bytes;
    if (*(uint32_t*)bb != 0xC0030201) {
        err.submit(ERR_BIG_ENDIAN);        // Big endian
    }
    if (*(int32_t*)bb != -1073544703) {
        err.submit(ERR_BIG_ENDIAN);        // not two's complement
    }
    *(float*)bb = 1.0f;
    if (*(uint32_t*)bb != 0x3F800000) {
        err.submit(ERR_BIG_ENDIAN);        // Not IEEE floating point format
    }
}


// Bit scan reverse. Returns floor(log2(x)), 0 if x = 0
uint32_t bitScanReverse(uint64_t x) {
    uint32_t s = 32;  // shift count
    uint32_t r = 0;   // return value
    uint64_t y;       // x >> s
    do {
        y = x >> s;
        if (y) {
            r += s;
            x = y;
        }
        s >>= 1;
    }
    while (s);
    return r;
}

// Bit scan forward. Returns index to the lowest set bit, 0 if x = 0
uint32_t bitScanForward(uint64_t x) {
    uint32_t s = 32;  // shift count
    uint32_t r = 0;   // return value
    if (x == 0) return 0;
    do {
        if ((x & (((uint64_t)1 << s) - 1)) == 0) {
            x >>= s;
            r += s;
        }
        s >>= 1;
    }
    while (s);
    return r;
}

const char * timestring(uint32_t t) {
    // Convert 32 bit time stamp to string
    // Fix the problem that time_t may be 32 bit or 64 bit
    union {
        time_t t;
        uint32_t t32;
    } utime;
    utime.t = 0;
    utime.t32 = t;
    const char * string = ctime(&utime.t);
    if (string == 0) string = "?";
    return string;
}

const char * exceptionCodeName(uint32_t code) {
    // get the name of an exception code from a NaN payload
    switch (code) {
    case nan_data_error:
        return "data unavailable";
    case nan_div0: 
        return "division by zero";
    case nan_overflow_div:
        return "division overflow";
    case nan_overflow_mul:
        return "multiplication overflow";
    case nan_overflow_fma:
        return "FMA overflow";        
    case nan_overflow_add:
        return "addition/subtraction overflow";
    case nan_overflow_conv:
        return "conversion overflow";
    case nan_overflow_other:
        return "other overflow";
    case nan_invalid_0div0:
        return "zero/zero";
    case nan_invalid_infdivinf:
        return "INF/INF";
    case nan_invalid_0mulinf:
        return "zero*INF";
    case nan_invalid_inf_sub_inf:
        return "INF-INF";
    case nan_underflow:
        return "underflow exception";
    case nan_inexact:
        return "inexact exception";
    case nan_invalid_sqrt:
        return "sqrt of negative";
    case nan_invalid_log:
        return "log of non-positive";
    case nan_invalid_pow:
        return "pow of invalid arguments";
    case nan_invalid_rem:
        return "remainder or modulo of invalid arguments";
    case nan_invalid_asin:
        return "asin of invalid argument";
    case nan_invalid_acos:
        return "acos of invalid argument";
    case nan_invalid_acosh:
        return "acosh of invalid argument";
    case nan_invalid_atanh:
        return "atanh of invalid argument";
    }
    // none of the above:
    if (code > nan_div0) return "unknown data error";
    if (code > nan_overflow_div) return "div 0 error";
    if (code > nan_invalid_0div0) return "overflow";
    if (code > nan_underflow) return "invalid calculation";
    if (code > nan_inexact) return "underflow";
    if (code > nan_invalid_sqrt) return "inexact";
    if (code >= 0b110000000) return "invalid argument to standard math function";
    if (code >= 0b101000000) return "invalid argument to math function";
    if (code >= 0b100000000) return "invalid operation in other function";
    if (code > 0) return "user-defined error code";
    return "no error code";
}
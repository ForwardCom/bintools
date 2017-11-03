/****************************  main.cpp   *******************************
* Author:        Agner Fog
* Date created:  2017-04-17
* Last modified: 2017-11-03
* Version:       1.00
* Project:       Binary tools for ForwardCom instruction set
* Description:   This includes assembler, disassembler, linker, library
*                manager, and emulator in one program
*
* Instructions:
* Run with option -h for help
*
* For detailed instructions, see forwardcom.pdf
*
* (c) Copyright 2017 GNU General Public License version 3
* http://www.gnu.org/licenses
*****************************************************************************/

#include "stdafx.h"

// Check that we are running on a machine with little-endian memory organization
static void CheckEndianness() {
    static uint8_t bytes[4] = { 1, 2, 3, 4 };
    uint8_t * bb = bytes;
    if (*(uint32_t*)bb != 0x04030201) {
        // Big endian
        err.submit(ERR_BIG_ENDIAN);
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

    cmd.readCommandLine(argc, argv);    // Read command line parameters   
    if (cmd.job == CMDL_JOB_HELP) return 0;         // Help screen has been printed. Do nothing else

    CConverter maincvt;                      // This object takes care of all conversions etc.
    maincvt.go();
    // Do everything the command line says

    if (cmd.verbose) printf("\n");      // End with newline
    return err.getWorstError();         // Return with error code
}


CConverter::CConverter() {
    // Constructor
}

void CConverter::go() {
    // Do whatever the command line parameters say
    fileName = cmd.inputFile;           // Get input file name from command line
                                        // Ignore nonexisting filename when building library
    int IgnoreError = (cmd.fileOptions & CMDL_FILE_IN_IF_EXISTS) && !cmd.outputFile;
    read(IgnoreError);                  // Read input file
    if (cmd.job == CMDL_JOB_ASS) fileType = FILETYPE_ASM;
    else getFileType();                 // Determine file type
    if (err.number()) return;           // Return if error
    cmd.inputType = fileType;           // Save input file type in cmd for access from other modules
    if (cmd.outputType == 0) {
        // desired type not specified
        cmd.outputType = fileType;
    }

    checkOutputFileName();              // Construct output file name with default extension
    if (err.number()) return;

    if (cmd.verbose && (cmd.job == CMDL_JOB_ASS || cmd.job == CMDL_JOB_DIS)) {
        // Tell what we are doing:
        printf("\nInput file: %s, output file: %s", fileName, outputFileName);
        if (fileType != cmd.outputType) {
            printf("\nConverting from %s to %s",
                getFileFormatName(fileType), getFileFormatName(cmd.outputType));
        }
        else {
            printf("\nModifying %s file", getFileFormatName(fileType));
        }
    }

    switch (cmd.job) {
    case CMDL_JOB_DUMP:
        // File dump requested
        switch (fileType) {
        case FILETYPE_ELF:
            dumpELF();  break;
        default:
            err.submit(ERR_DUMP_NOT_SUPPORTED, getFileFormatName(fileType));  // Dump of this file type not supported
        }
        printf("\n");                              // New line
        break;

    case CMDL_JOB_ASS:
        // assemble
        assemble();
        if (err.number()) return;  // Return if error
        break;

    case CMDL_JOB_DIS:
        // disassemble
        disassemble();
        if (err.number()) return;  // Return if error
        break;

    case CMDL_JOB_LINK:
        break;

    case CMDL_JOB_LIB:
        break;

    case CMDL_JOB_EMU:
        break;

    default:
        err.submit(ERR_INTERNAL);
    }
    /*
    if ((cmd.fileOptions & CMDL_FILE_OUTPUT) && outputFileName) {
        // There is an output file to write
        if (err.number()) return;        // Return if error
        fileName = outputFileName;       // Output file name
        write();                         // Write output file
        if (cmd.verbose) cmd.reportStatistics(); // Report statistics
    }*/
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
    // Make instance of converter, 64 bit template
    CAssembler ass;
    if (err.number()) return;
    *this >> ass;                      // Give it my buffer
    ass.go();                          // run
} 

void CConverter::disassemble() {
    // Disassemble ELF file
    // Make instance of converter, 64 bit template
    CDisassembler dis;
    if (err.number()) return;
    *this >> dis;                      // Give it my buffer
    dis.parseFile();                   // Parse file buffer
    if (err.number()) return;          // Return if error
    dis.getComponents1();              // Get components from ELF file
    dis.go();                          // Convert
}

// Convert half precision floating point number
float half2float(uint32_t half) {
    union {
        uint32_t hhh;
        float fff;
    } u;

    u.hhh  = (half & 0x7fff) << 13;              // Exponent and mantissa
    u.hhh += 0x38000000;                         // Adjust exponent bias
    if ((half & 0x7c00) == 0) {
        // Denormal. Make zero
        u.hhh = 0;
    }
    u.hhh |= (half & 0x8000) << 16;              // sign bit
    return u.fff;
}


// Convert double precision floating point number to half precision. subnormals give zero
uint16_t double2half(double x) {
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
    v.mant = u.mant >> 42;         // get upper part of mantissa
    if (u.mant & ((uint64_t)1 << 41)) {  // round to nearest or even
        if ((u.mant & (((uint64_t)1 << 41) - 1)) || (v.mant & 1)) { // round up if odd or remaining bits are nonzero
            v.h++;                       // overflow here will give infinity
        }
    }
    v.expo = u.expo - 0x3F0;
    v.sign = u.sign;
    if (u.expo == 0x7FF) {
        v.expo = 0x1F;  // infinity or nan
        if (u.mant != 0 && v.mant == 0) v.mant = 0x200;  // make sure output is a nan if input is nan
    }
    else if (u.expo > 0x40E) {
        v.expo = 0x1F;  v.mant = 0;  // overflow -> inf
    }
    else if (u.expo < 0x3F1) {
        v.expo = 0;  v.mant = 0;     // underflow -> 0
    }
    return v.h;   
}

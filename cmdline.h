/****************************  cmdline.h   ***********************************
* Author:        Agner Fog
* Date created:  2017-04-17
* Last modified: 2017-11-03
* Version:       1.00
* Project:       Binary tools for ForwardCom instruction set
* Module:        cmdline.h
* Description:
* Header file for command line interpreter cmdline.cpp
*
* Copyright 2006-2017 GNU General Public License http://www.gnu.org/licenses
*****************************************************************************/
#pragma once

/**************************  Define constants  ******************************/
// Max number of response files on command line
const int MAX_COMMAND_FILES = 10;

// Constants for output file type
const int CMDL_OUTPUT_DUMP =          0x80;       // No output file, just dump contents
const int CMDL_OUTPUT_ELF =   FILETYPE_ELF;      // ELF file
const int CMDL_OUTPUT_ASM =   FILETYPE_ASM;      // Assembly file

// Constants for job
const int CMDL_JOB_ASS =                1;       // Assemble
const int CMDL_JOB_DIS =                2;       // Disassemble
const int CMDL_JOB_DUMP =               3;       // Dump
const int CMDL_JOB_LINK =               4;       // Link
const int CMDL_JOB_LIB =                5;       // Library
const int CMDL_JOB_EMU =                8;       // Emulate/Debug
const int CMDL_JOB_HELP =          0x1000;       // Show help

// Constants for verbose or silent console output
const int CMDL_VERBOSE_NO =             0;     // Silent. No console output if no errors or warnings
const int CMDL_VERBOSE_YES =            1;     // Output messages about file names and types
const int CMDL_VERBOSE_DIAGNOSTICS =    2;     // Output more messages

// Constants for dump options
const int DUMP_NONE =              0x0000;     // Dump nothing
const int DUMP_FILEHDR =           0x0001;     // Dump file header
const int DUMP_SECTHDR =           0x0002;     // Dump section headers
const int DUMP_SYMTAB =            0x0010;     // Dump symbol table
const int DUMP_RELTAB =            0x0020;     // Dump relocation table
const int DUMP_STRINGTB =          0x0040;     // Dump string table
const int DUMP_COMMENT =           0x0080;     // Dump comment records

// Constants for file input/output options
const int CMDL_FILE_INPUT =             1;     // Input file required
const int CMDL_FILE_IN_IF_EXISTS =      2;     // Read input file if it exists
const int CMDL_FILE_OUTPUT =         0x10;     // Write output file required
const int CMDL_FILE_IN_OUT_SAME =    0x20;     // Input and output files may have the same name

// Constants for library options
const int CMDL_LIBRARY_DEFAULT =        0;     // No option specified
const int CMDL_LIBRARY_CONVERT =        1;     // Convert or modify library
const int CMDL_LIBRARY_ADDMEMBER =      2;     // Add object file to library
const int CMDL_LIBRARY_EXTRACTMEM = 0x100;     // Extract specified object file(s) from library
const int CMDL_LIBRARY_EXTRACTALL = 0x110;     // Extract all object files from library


// Class for interpreting command line
class CCommandLineInterpreter {
public:
    CCommandLineInterpreter();                // Default constructor
    void readCommandLine(int argc, char * argv[]);     // Read and interpret command line
    void reportStatistics();                  // Report statistics about name changes etc.
    char const * inputFile;                   // Input file name
    char const * outputFile;                  // Output file name
    char const * instructionListFile;         // File name of instruction list
    char const * outputListFile;              // File name of output list file (ass)
    int  job;                                 // Job to do: ass, dis, dump, link, lib, emu
    int  inputType;                           // Input file type (detected from file)
    int  outputType;                          // Output type (file type or dump)
    int  optiLevel;                           // Optimization level (asm)
    uint32_t maxErrors;                       // Maximum number of errors before assembler aborts
    uint32_t verbose;                         // How much diagnostics to print on screen
    uint32_t dumpOptions;                     // Options for dumping file
    uint32_t fileOptions;                     // Options for input and output files
    uint32_t libraryOptions;                  // Options for library operations
    uint32_t linkOptions;                     // Options for linking
    uint32_t debugOptions;                    // Options for debug info in assembly. not supported yet
    uint64_t codeSizeOption;                  // Option specifying max code size
    uint64_t dataSizeOption;                  // Option specifying max data size
    const char * programName;                 // Path and name of this program
protected:
    int  libmode;                             // -lib option has been encountered
    void readCommandItem(char *);             // Read one option from command line
    void readCommandFile(char *);             // Read commands from file
    void interpretFileName(char *);           // Interpret input or output filename from command line
    void interpretCommandOption(char *);      // Interpret one option from command line
    void interpretAssembleOption(char *);     // Interpret assemble option from command line
    void interpretDisassembleOption(char *);  // Interpret disassemble option from command line
    void interpretLibraryOption(char* string);// Interpret library option from command line
    void interpretLinkOption(char * string);  // Interpret linking option from command line
    void interpretMaxErrorsOption(char * string); // Interpret maxerrors option from command line    
    void interpretCodeSizeOption(char * string);  // Interpret codesize option from command line
    void interpretDataSizeOption(char * string);  // Interpret datasize option from command line
    void interpretEmulateOption(char *);      // Interpret emulate option from command line
    void help();                              // Print help message
    void interpretIlistOption(char *);        // Interpret instruction list file option
    void interpretListOption(char *);         // Interpret output list file option (assem)
    void interpretOptimizationOption(char *); // Interpret optimization option (assem)
    void interpretDumpOption(char *);         // Interpret dump option from command line
    void interpretErrorOption(char *);        // Interpret error option from command line
    CDynamicArray<CFileBuffer> responseFiles; // Array of up to 10 response file buffers
    int numBuffers;                           // Number of response file buffers
    uint32_t currentSymbol;                   // Pointer into SymbolList
    // Statistics counters
    int countDebugSectionsRemoved;            // Count number of debug sections removed
    int countExceptionSectionsRemoved;        // Count number of exception handler sections removed
};

extern CCommandLineInterpreter cmd;          // Command line interpreter

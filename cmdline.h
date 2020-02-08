/****************************  cmdline.h   ***********************************
* Author:        Agner Fog
* Date created:  2017-04-17
* Last modified: 2018-03-30
* Version:       1.01
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
const int CMDL_OUTPUT_DUMP =          0x80;      // No output file, just dump contents
const int CMDL_OUTPUT_ELF =   FILETYPE_ELF;      // ELF file
const int CMDL_OUTPUT_ASM =   FILETYPE_ASM;      // Assembly file

// Constants for job
const int CMDL_JOB_ASS =                1;       // Assemble
const int CMDL_JOB_DIS =                2;       // Disassemble
const int CMDL_JOB_DUMP =               3;       // Dump
const int CMDL_JOB_LINK =               4;       // Link
const int CMDL_JOB_RELINK =             5;       // Relink
const int CMDL_JOB_LIB =                6;       // Library
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
const int DUMP_RELINKABLE =        0x0100;     // Show names of relinkable modules and libraries

// Constants for file input/output options
const int CMDL_FILE_INPUT =             1;     // Input file required
const int CMDL_FILE_SEARCH_PATH =       2;     // Search for file in path
const int CMDL_FILE_IN_IF_EXISTS =      4;     // Read input file if it exists
const int CMDL_FILE_OUTPUT =         0x10;     // Write output file required
const int CMDL_FILE_IN_OUT_SAME =    0x20;     // Input and output files may have the same name
const int CMDL_FILE_INCOMPLETE =   0x1000;     // Allow output executable file to be incomplete, i.e. have unresolved references
const int CMDL_FILE_RELINKABLE =  0x10000;     // Output executable file is relinkable

// Constants for library options
const int CMDL_LIBRARY_PRESERVEMEMBER = 1;     // preserve object file in library
const int CMDL_LIBRARY_ADDMEMBER =      2;     // add object file to library
const int CMDL_LIBRARY_DELETEMEMBER =   4;     // delete object file to library
const int CMDL_LIBRARY_LISTMEMBERS =    8;     // list library members
const int CMDL_LIBRARY_EXTRACTMEM = 0x100;     // extract specified object file(s) from library
const int CMDL_LIBRARY_EXTRACTALL = 0x300;     // extract all object files from library

// Constants for linker options
const int CMDL_LINK_EXEFILE =            1;    // expecting executable file name
const int CMDL_LINK_ADDMODULE =          2;    // add the following object files or library files
const int CMDL_LINK_ADDLIBRARY =         4;    // add the following library files
const int CMDL_LINK_ADDLIBMODULE =       6;    // add a member of a previously specified library
const int CMDL_LINK_EXTRACT =            8;    // extract relinkable module
const int CMDL_LINK_REMOVE =          0x10;    // remove relinkable modules
const int CMDL_LINK_REPLACE =         0x20;    // replace relinkable modules
const int CMDL_LINK_LIST =            0x80;    // list relinkable modules
const int CMDL_LINK_EXTRACT_ALL =    0x100;    // extract all relinkable modules
const int CMDL_LINK_STACKSIZE =      0x200;    // size of call stack, data stack, additional vectors
const int CMDL_LINK_HEAPSIZE =       0x400;    // size of heap
const int CMDL_LINK_DYNLINKSIZE =    0x800;    // extra size for dynamic linking: readonly, executable, writeable data
const int CMDL_LINK_RELINKABLE =   0x10000;    // add as relinkable the following object files and library files
const int CMD_NAME_FOUND =       0x1000000;    // mark name found in rnames record


// Structure for storing library or linker commands from command line
struct SLCommand {
    uint32_t filename;                           // full file name, as index into cmd.fileNameBuffer
    uint32_t command;                            // library commands or linker commands
    uint64_t value;                              // value or library member name = filename without path
};


// Class for interpreting command line
class CCommandLineInterpreter {
public:
    CCommandLineInterpreter();                // Default constructor
    void readCommandLine(int argc, char * argv[]);     // Read and interpret command line
    void reportStatistics();                  // Report statistics about name changes etc.
    uint32_t inputFile;                       // Input file name. index into fileNameBuffer
    uint32_t outputFile;                      // Output file name. index into fileNameBuffer
    uint32_t instructionListFile;             // File name of instruction list. index into fileNameBuffer
    uint32_t outputListFile;                  // File name of assembler or emulator output list file. index into fileNameBuffer
    int  job;                                 // Job to do: ass, dis, dump, link, lib, emu
    int  inputType;                           // Input file type (detected from file)
    int  outputType;                          // Output type (file type or dump)
    int  optiLevel;                           // Optimization level (asm)
    uint32_t maxErrors;                       // Maximum number of errors before assembler or emulator aborts
    uint32_t maxLines;                        // Maximum number of lines in emulator output list
    uint32_t verbose;                         // How much diagnostics to print on screen
    uint32_t dumpOptions;                     // Options for dumping file
    uint32_t fileOptions;                     // Options for input and output files
    uint32_t libraryOptions;                  // Options for library operations
    uint32_t linkOptions;                     // Options for linking
    uint32_t debugOptions;                    // Options for debug info in assembly. not supported yet
    uint64_t codeSizeOption;                  // Option specifying max code size
    uint64_t dataSizeOption;                  // Option specifying max data size
    const char * programName;                 // Path and name of this program
    void checkExtractSuccess();               // Check if library members to extract were found
    const char * getFilename(uint32_t n);     // Get file name from index
    int mainReturnValue;                      // Return value for program main
protected:
    int  libmode;                             // lib option has been encountered
    int  linkmode;                            // link option has been encountered
    int  emumode;                             // emulation option has been encountered
    int  numBuffers;                          // Number of response file buffers
    void readCommandItem(char *);             // Read one option from command line
    void readCommandFile(char *);             // Read commands from file
    void interpretFileName(char *);           // Interpret input or output filename from command line
    void interpretCommandOption(char *);      // Interpret one option from command line
    void interpretAssembleOption(char *);     // Interpret assemble option from command line
    void interpretDisassembleOption(char *);  // Interpret disassemble option from command line
    void interpretLibraryCommand(char*string);// Interpret -lib command
    void interpretLibraryOption(char*string); // Interpret library option from command line
    void interpretLinkCommand(char * string); // Interpret -link command
    void interpretLinkOption(char * string);  // Interpret linking option from command line
    void interpretEmulateOption(char *);      // Interpret emulate option from command line
    void interpretMaxErrorsOption(char * string); // Interpret maxerrors option from command line    
    void interpretCodeSizeOption(char * string);  // Interpret codesize option from command line
    void interpretDataSizeOption(char * string);  // Interpret datasize option from command line
    void interpretIlistOption(char *);        // Interpret instruction list file option
    void interpretListOption(char *);         // Interpret output list file option for assembler
    void interpretOptimizationOption(char *); // Interpret optimization option for assembler
    void interpretStackOption(char *);        // Interpret stack size option for linker
    void interpretHeapOption(char *);         // Interpret heap size option for linker
    void interpretDynlinkOption(char *);      // Interpret dynamic link size option for linker
    void interpretVerboseOption(char * string);// Interpret silent/verbose option from command line
    void interpretDumpOption(char *);         // Interpret dump option from command line
    void interpretErrorOption(char *);        // Interpret error option from command line
    void interpretMaxLinesOption(char * string);// Interpret maxlines option from command line
    void checkOutputFileName();               // Make output file name or check that requested name is valid
    uint32_t setFileNameExtension(uint32_t fn, int filetype);   // Set file name extension according to FileType
    void help();                              // Print help message
public:
    CMemoryBuffer fileNameBuffer;             // Buffer containing names of files from command line
    CDynamicArray<SLCommand> lcommands;       // List of linker or library commands from command line
};

int strncasecmp_(const char *s1, const char *s2, uint32_t n); // compare strings, ignore case for a-z

extern CCommandLineInterpreter cmd;          // Command line interpreter

// operator < for sorting command line commands by name
static inline bool operator < (SLCommand const & a, SLCommand const & b) {
    return strcmp(cmd.getFilename((uint32_t)a.value), cmd.getFilename((uint32_t)b.value)) < 0;
}

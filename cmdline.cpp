/****************************  cmdline.cpp  **********************************
* Author:        Agner Fog
* Date created:  2017-04-17
* Last modified: 2017-11-03
* Version:       1.00
* Project:       Binary tools for ForwardCom instruction set
* Description:
* This module is for interpretation of command line options
* Also contains symbol change function
*
* Copyright 2017 GNU General Public License http://www.gnu.org/licenses
*****************************************************************************/

#include "stdafx.h"

// Command line interpreter
CCommandLineInterpreter cmd;                               // Instantiate command line interpreter

CCommandLineInterpreter::CCommandLineInterpreter() {
    // Default constructor
    memset(this, 0, sizeof(*this));                        // Set all to zero
    verbose   = CMDL_VERBOSE_YES;                          // How much diagnostics to print on screen
    optiLevel = 2;                                         // Optimization level
    maxErrors = 50;                                        // Maximum number of errors before assembler aborts
    instructionListFile = "instruction_list.csv";          // Filename of list of instructions (default name)
}


void CCommandLineInterpreter::readCommandLine(int argc, char * argv[]) {
    // Read command line
    if (argv[0]) programName = argv[0]; else programName = "";
    for (int i = 1; i < argc; i++) {
        readCommandItem(argv[i]);
    }
    if (job == CMDL_JOB_HELP || (inputFile == 0 && outputFile == 0)) {
        // No useful command found. Print help
        job = CMDL_JOB_HELP;
        help();
        return;
    }
    // Check file options
    fileOptions = CMDL_FILE_INPUT;
    if (libraryOptions == CMDL_LIBRARY_ADDMEMBER) {
        // Adding object files to library. Library may not exist
        fileOptions = CMDL_FILE_IN_IF_EXISTS;
    } 
    if (job == CMDL_JOB_DUMP) {
        // Dumping or extracting. Output file not used
        if (outputFile) err.submit(ERR_OUTFILE_IGNORED); // Output file name ignored
        outputFile = 0;
    }
    else {
        // Output file required
        fileOptions |= CMDL_FILE_OUTPUT;
    }
    if ((libraryOptions & CMDL_LIBRARY_ADDMEMBER) && !(libraryOptions & CMDL_LIBRARY_CONVERT)) {
        // Adding library members only. Output file may have same name as input file
        fileOptions |= CMDL_FILE_IN_OUT_SAME;
    }
    // Check output type
    if (!outputType) {
        // Output type not defined yet
        outputType = FILETYPE_FWC;
    }
}


void CCommandLineInterpreter::readCommandItem(char * string) {
    // Read one option from command line
    // Skip leading whitespace
    while (*string != 0 && *string <= ' ') string++;
    if (*string == 0) return;  // Empty string

    // Look for option prefix and response file prefix
    const char optionPrefix1 = '-';  // Option must begin with '-'
#if defined (_WIN32) || defined (__WINDOWS__)
    const char optionPrefix2 = '/';  // '/' allowed instead of '-' in Windows only
#else
    const char optionPrefix2 = '-';
#endif
    const char responseFilePrefix = '@';  // Response file name prefixed by '@'
    if (*string == optionPrefix1 || *string == optionPrefix2) {
        // Option prefix found. This is a command line option
        interpretCommandOption(string+1);
    }
    else if (*string == responseFilePrefix) {
        // Response file prefix found. Read more options from response file
        readCommandFile(string+1);
    }
    else {
        // No prefix found. This is an input or output file name
        interpretFileName(string);
    }
}


void CCommandLineInterpreter::readCommandFile(char * filename) {
    // Read commands from file
    if (*filename <= ' ') {
        err.submit(ERR_EMPTY_OPTION); return;    // Warning: empty filename
    }

    // Check if too many response file buffers (possibly because file includes itself)
    if (++numBuffers > MAX_COMMAND_FILES) {err.submit(ERR_TOO_MANY_RESP_FILES); return;}

    // Allocate buffer for response files.
    if (responseFiles.numEntries() == 0) {
        responseFiles.setNum(MAX_COMMAND_FILES);
    }

    // Read response file into new buffer
    responseFiles[numBuffers-1].fileName = filename;
    responseFiles[numBuffers-1].read();

    // Get buffer with file contents
    char * buffer = (char*)responseFiles[numBuffers-1].buf();
    char * itemBegin, * itemEnd;  // Mark begin and end of token in buffer

    // Check if buffer is allocated
    if (buffer) {

        // Parse contents of response file for tokens
        while (*buffer) {

            // Skip whitespace
            while (*buffer != 0 && uint8_t(*buffer) <= uint8_t(' ')) buffer++;
            if (*buffer == 0) break; // End of buffer found
            itemBegin = buffer;

            // Find end of token
            itemEnd = buffer+1;
            while (uint8_t(*itemEnd) > uint8_t(' ')) itemEnd++;
            if (*itemEnd == 0) {
                buffer = itemEnd;
            }
            else {
                buffer = itemEnd + 1;
                *itemEnd = 0;    // Mark end of token
            }
            // Found token. 
            // Check if it is a comment beginning with '#' or '//'
            if (itemBegin[0] == '#' || (itemBegin[0] == '/' && itemBegin[1] == '/' )) {
                // This is a comment. Skip to end of line
                itemEnd = buffer;
                while (*itemEnd != 0 && *itemEnd != '\n') {
                    itemEnd++;
                }
                if (*itemEnd == 0) {
                    buffer = itemEnd;
                }
                else {
                    buffer = itemEnd + 1;
                }
                continue;
            }
            // Not a comment. Interpret token
            readCommandItem(itemBegin);
        }
    }
}


void CCommandLineInterpreter::interpretFileName(char * string) {
    // Interpret input or output filename from command line

    switch (libmode) {
    case 1:            // First filename after -lib = inputfile and outputfile
        inputFile = string;
        libmode = 2;
        return;

    case 2:            // Second or later filename after -lib = object file to add to library
       // addObjectToLibrary(string, string);
        return;
    }
    // libmode = 0: Ordinary input or output file
    if (!inputFile) {
        // Input file not specified yet
        inputFile = string;
    }
    else if (!outputFile) {
        // Output file not specified yet
        outputFile = string;
    }
    else {
        // Both input and output files already specified
        err.submit(ERR_MULTIPLE_IO_FILES);
    }
}


void CCommandLineInterpreter::interpretCommandOption(char * string) {
    // Interpret one option from command line
    if ((uint8_t)*string <= (uint8_t)' ') {
        err.submit(ERR_EMPTY_OPTION); return;    // Warning: empty option
    }

    char stringlow[64];                          // convert to lower case
    for (int i=0; i<64; i++) {   
        stringlow[i] = tolower(string[i]);
        if (string[i] == 0) break;
    }

    // Detect option type
    switch(stringlow[0]) {
    case 'a':   // assemble option
        if (job) err.submit(ERR_MULTIPLE_COMMANDS, string);     // More than one job specified
        job = CMDL_JOB_ASS;
        if (strncmp(stringlow, "ass", 3) == 0) {
            interpretAssembleOption(string+3);
        }
        else err.submit(ERR_UNKNOWN_OPTION, string);     // Unknown option
        break;

    case 'c':   // codesize option
        if (strncmp(stringlow, "codesize", 8) == 0) {
            interpretCodeSizeOption(string+8);
        }
        else err.submit(ERR_UNKNOWN_OPTION, string);     // Unknown option
        break;


    case 'd':   // datasize, disassemble or dump option
        if (strncmp(stringlow, "dis", 3) == 0) {
            if (job) err.submit(ERR_MULTIPLE_COMMANDS, string);     // More than one job specified
            job = CMDL_JOB_DIS;
            interpretDisassembleOption(string+3);  break;
        }
        if (strncmp(stringlow, "dump", 4) == 0) {
            if (job) err.submit(ERR_MULTIPLE_COMMANDS, string);     // More than one job specified
            job = CMDL_JOB_DUMP;
            interpretDumpOption(string+4);  break;
        }
        if (strncmp(stringlow, "datasize", 8) == 0) {
            interpretDataSizeOption(string+8);
            break;
        }
        err.submit(ERR_UNKNOWN_OPTION, string);     // Unknown option
        break;

    case 'e':    // Emulation or error option
        if (strncmp(stringlow, "emu", 3) == 0) {
            if (job) err.submit(ERR_MULTIPLE_COMMANDS, string);     // More than one job specified
            job = CMDL_JOB_EMU;
            interpretEmulateOption(string+3);
        }
        else {
            interpretErrorOption(string);
        }
        break;

    case 'h': case '?':  // Help
        job = CMDL_JOB_HELP;
        break;

    case 'i':   // Instruction list
        if (strncmp(stringlow, "ilist=", 6) == 0) {
        interpretIlistOption(string+6);
        }
        else {        
            err.submit(ERR_UNKNOWN_OPTION, string);     // Unknown option
        }
        break;

    case 'l':   // Link, Library, and list file options
        if (strncmp(stringlow, "lib", 3) == 0) {
            if (job) err.submit(ERR_MULTIPLE_COMMANDS, string);     // More than one job specified
            job = CMDL_JOB_LIB;
            interpretLibraryOption(string+3);  break;
        }
        if (strncmp(stringlow, "link", 4) == 0) {
            if (job) err.submit(ERR_MULTIPLE_COMMANDS, string);     // More than one job specified
            job = CMDL_JOB_LINK;
            interpretLinkOption(string+4);  break;
        }
        if (strncmp(stringlow, "list=", 5) == 0) {
            interpretListOption(string+5);  break;
        }
        err.submit(ERR_UNKNOWN_OPTION, string);     // Unknown option
        break;

    case 'm':    // Maxerrors option
        if (strncmp(stringlow, "maxerrors", 9) == 0) {
            interpretMaxErrorsOption(string+9);  break;
        }
        err.submit(ERR_UNKNOWN_OPTION, string);     // Unknown option
        break;

    case 'o':    // Optimization option
        interpretOptimizationOption(string+1);
        break;
        //

    case 'w':    // Warning option
        interpretErrorOption(string);  break;

    default:    // Unknown option
        err.submit(ERR_UNKNOWN_OPTION, string);
    }
}

void CCommandLineInterpreter::interpretAssembleOption(char * string) {
    outputType = FILETYPE_FWC;
}
    
void CCommandLineInterpreter::interpretDisassembleOption(char * string) {
    outputType = CMDL_OUTPUT_ASM;
}

void CCommandLineInterpreter::interpretDumpOption(char * string) {
    // Interpret dump option from command line
    if (outputType || dumpOptions) err.submit(ERR_MULTIPLE_COMMANDS);   // Both dump and convert specified

    char * s1 = string;
    while (*s1) {
        switch (*(s1++)) {
        case 'f':   // dump file header
            dumpOptions |= DUMP_FILEHDR;  break;
        case 'h':   // dump section headers
            dumpOptions |= DUMP_SECTHDR;  break;
        case 's':   // dump symbol table
            dumpOptions |= DUMP_SYMTAB;  break;
        case 'r':   // dump relocations
            dumpOptions |= DUMP_RELTAB;  break;
        case 'n':   // dump string table
            dumpOptions |= DUMP_STRINGTB;  break;
        case 'c':   // dump comment records (currently only for OMF)
            dumpOptions |= DUMP_COMMENT;  break;         
        case '-': case '_':  // '-' may separate options
            break;
        default:
            err.submit(ERR_UNKNOWN_OPTION, string-1);  // Unknown option
        }
    }
    if (dumpOptions == 0) dumpOptions = DUMP_FILEHDR;
    outputType = CMDL_OUTPUT_DUMP;
    if (outputType && outputType != CMDL_OUTPUT_DUMP) err.submit(ERR_MULTIPLE_COMMANDS); // Both dump and convert specified
    outputType = CMDL_OUTPUT_DUMP;
}

void CCommandLineInterpreter::interpretLinkOption(char * string) {
    // Interpret linker options
}

void CCommandLineInterpreter::interpretEmulateOption(char * string) {
    // Interpret emulate options
}

void CCommandLineInterpreter::interpretLibraryOption(char * string) {
    // Interpret options for manipulating library/archive files

    // Check for -lib command
    if (/*stricmp(string, "lib") == 0*/ true) {  // Found -lib command
        if (inputFile) {
            libmode = 2;                  // Input file already specified. Remaining file names are object files to add
        }
        else {
            libmode = 1;                  // The rest of the command line must be interpreted as library name and object file names
        }
        return;
    }
    /*
    switch (string[1]) {
    case 'a':      // Add input file to library
        if (name1) {
            AddObjectToLibrary(name1, name2);
        }
        else err.submit(ERR_UNKNOWN_OPTION, string);
        break;

    case 'x':      // Extract member(s) from library
        if (name1) {
            // Extract specified member
            cmd.libraryOptions = CMDL_LIBRARY_EXTRACTMEM;
            sym.Action  = SYMA_EXTRACT_MEMBER;
            SymbolList.Push(&sym, sizeof(sym));
        }
        else {
            // Extract all members
            cmd.libraryOptions = CMDL_LIBRARY_EXTRACTALL;
        }
        break;

    case 'd':   // Delete member from library
        if (name1) {
            // Delete specified member
            cmd.libraryOptions = CMDL_LIBRARY_CONVERT;
            sym.Action  = SYMA_DELETE_MEMBER;
            SymbolList.Push(&sym, sizeof(sym));
        }
        else err.submit(ERR_UNKNOWN_OPTION, string);
        break;

    default:
        err.submit(ERR_UNKNOWN_OPTION, string);  // Unknown option
    } */
}

void CCommandLineInterpreter::interpretIlistOption(char * string) {
    // Interpret instruction list file option
    instructionListFile = string;
}

void CCommandLineInterpreter::interpretListOption(char * string) {
    // Interpret instruction list file option
    outputListFile = string;
}

void CCommandLineInterpreter::interpretOptimizationOption(char * string) {
    if (string[0] < '0' || string[0] > '9' || strlen(string) != 1) {
        err.submit(ERR_UNKNOWN_OPTION, string); return; // Unknown option
    }
    optiLevel = string[0] - '0';
}

void CCommandLineInterpreter::interpretMaxErrorsOption(char * string) {
    // Interpret maxerrors option from command line
    if (string[0] == '=') string++;
    uint32_t error = 0;
    maxErrors = (uint32_t)interpretNumber(string, 99, &error);
    if (error) err.submit(ERR_UNKNOWN_OPTION, string);
}

void CCommandLineInterpreter::interpretCodeSizeOption(char * string) {
    // Interpret codesize option from command line
    uint32_t error = 0;
    codeSizeOption = interpretNumber(string, 99, &error);
    if (error) err.submit(ERR_UNKNOWN_OPTION, string);
}
    
void CCommandLineInterpreter::interpretDataSizeOption(char * string) {
    // Interpret datasize option from command line
    uint32_t error = 0;
    dataSizeOption = interpretNumber(string+1, 99, &error);
    if (error) err.submit(ERR_UNKNOWN_OPTION, string);
}

void CCommandLineInterpreter::interpretErrorOption(char * string) {
    // Interpret warning/error option from command line
    if (strlen(string) < 3) {
        err.submit(ERR_UNKNOWN_OPTION, string); return; // Unknown option
    } 
    int newstatus;   // New status for this error number

    switch (string[1]) {
    case 'd':   // Disable
        newstatus = 0;  break;

    case 'w':   // Treat as warning
        newstatus = 1;  break;

    case 'e':   // Treat as error
        newstatus = 2;  break;

    default:
        err.submit(ERR_UNKNOWN_OPTION, string);  // Unknown option
        return;
    }
    if (string[2] == 'x' ) {
        // Apply new status to all non-fatal messages
        for (SErrorText * ep = errorTexts; ep->status < 9; ep++) {
            ep->status = newstatus;  // Change status of all errors
        }
    }
    else {
        int ErrNum = atoi(string+2);
        if (ErrNum == 0 && string[2] != '0') {
            err.submit(ERR_UNKNOWN_OPTION, string);  return; // Unknown option
        }
        // Search for this error number
        SErrorText * ep = err.FindError(ErrNum);
        if (ep->status & 0x100) {
            // Error number not found
            err.submit(ERR_UNKNOWN_ERROR_NUM, ErrNum);  return; // Unknown error number
        }
        // Change status of this error
        ep->status = newstatus;
    }
}


void CCommandLineInterpreter::reportStatistics() {
    // Report statistics about name changes etc.
}


void CCommandLineInterpreter::help() {
    // Print help message
    printf("\nBinary tools version %.2f beta for ForwardCom instruction set.", FORWARDCOM_VERSION);
    printf("\nCopyright (c) 2017 by Agner Fog. Gnu General Public License.");
    printf("\n\nUsage: forw command [options] inputfile [outputfile]");
    printf("\n\nCommand:");
    printf("\n-ass       Assemble\n");
    printf("\n-dis       Disassemble object or executable file\n");
    printf("\n-link      Link object files into executable file\n");
    printf("\n-lib       Build or manage library file\n");
    printf("\n-emu       Emulate and debug executable file\n");
    printf("\n-dump-XXX  Dump file contents to console.");
    printf("\n           Values of XXX (can be combined):");
    printf("\n           f: File header, h: section Headers, s: Symbol table,");
    printf("\n           r: Relocation table, n: string table.\n");
    printf("\n-help      Print this help screen.");

    printf("\n\nAssemble options:");
    printf("\n-list=filename Specify file for output listing.");
    printf("\n-ON        Optimization level. N = 0-2.");

    printf("\n\nGeneral options:");
    printf("\n-ilist=filename Specify instruction list file.");
    printf("\n-wdNNN     Disable Warning NNN.");
    printf("\n-weNNN     treat Warning NNN as Error. -wex: treat all warnings as errors.");
    printf("\n-edNNN     Disable Error number NNN.");
    printf("\n-ewNNN     treat Error number NNN as Warning.");
    printf("\n@RFILE     Read additional options from response file RFILE.");
    printf("\n\nExample:");
    printf("\nforw -ass test.as test.ob\n\n");
}

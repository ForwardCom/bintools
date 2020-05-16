/****************************  cmdline.cpp  **********************************
* Author:        Agner Fog
* Date created:  2017-04-17
* Last modified: 2020-05-12
* Version:       1.10
* Project:       Binary tools for ForwardCom instruction set
* Description:
* This module is for interpretation of command line options
* Also contains symbol change function
*
* Copyright 2017-2020 GNU General Public License http://www.gnu.org/licenses
*****************************************************************************/

#include "stdafx.h"

// Command line interpreter
CCommandLineInterpreter cmd;                               // Instantiate command line interpreter

CCommandLineInterpreter::CCommandLineInterpreter() {
    // Default constructor
    zeroAllMembers(*this);                                 // Set all to zero
    verbose   = CMDL_VERBOSE_YES;                          // How much diagnostics to print on screen
    optiLevel = 2;                                         // Optimization level
    maxErrors = 50;                                        // Maximum number of errors before assembler aborts
    fileNameBuffer.pushString("");                         // make first entry zero
    instructionListFile = fileNameBuffer.pushString("instruction_list.csv");  // Filename of list of instructions (default name)
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
    fileOptions |= CMDL_FILE_INPUT;
    if (libraryOptions == CMDL_LIBRARY_ADDMEMBER) {
        // Adding object files to library. Library may not exist
        fileOptions |= CMDL_FILE_IN_IF_EXISTS;
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
    if (libraryOptions & CMDL_LIBRARY_ADDMEMBER) {
        // Adding library members only. Output file may have same name as input file
        fileOptions |= CMDL_FILE_IN_OUT_SAME;
    }
    // Check output type
    if (!outputType) {
        // Output type not defined yet
        outputType = FILETYPE_FWC;
    }
    // check if output file name is valid
    checkOutputFileName();
}


void CCommandLineInterpreter::readCommandItem(char * string) {
    // Read one option from command line
    // Skip leading whitespace
    while (*string != 0 && *string <= ' ') string++;
    if (*string == 0) return;  // Empty string

    // Look for option prefix and response file prefix
    const char responseFilePrefix = '@';  // Response file name prefixed by '@'
    const char optionPrefix1 = '-';  // Option must begin with '-'
#if defined (_WIN32) || defined (__WINDOWS__)
    const char optionPrefix2 = '/';  // '/' allowed instead of '-' in Windows only
#else
    const char optionPrefix2 = '-';
#endif

    if (*string == optionPrefix1 || *string == optionPrefix2) {
        // Option prefix found. This is a command line option
        if (libmode) interpretLibraryOption(string+1);
        else if (linkmode) interpretLinkOption(string+1);
        else if (emumode) interpretEmulateOption(string+1);
        else if (job == CMDL_JOB_DUMP) interpretDumpOption(string+1);
        else if (job == CMDL_JOB_ASS) interpretAssembleOption(string+1);
        else interpretCommandOption(string+1);
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
    if (++numBuffers > MAX_COMMAND_FILES) {
        err.submit(ERR_TOO_MANY_RESP_FILES); return;
    }

    // Read response file into buffer
    CFileBuffer commandfile;
    commandfile.read(filename);
    if (err.number()) return;

    // Get buffer with file contents
    char * buffer = (char*)commandfile.buf();
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
    if (libmode) {
        // library command
        if (libmode == 1) {
            // first filename = library file
            inputFile = outputFile = cmd.fileNameBuffer.pushString(string);
            libmode = 2;
            return;
        }
        // object file to add, remove, or extract
        if (libraryOptions == CMDL_LIBRARY_LISTMEMBERS) {
            err.submit(ERR_LIBRARY_LIST_ONLY);  return;
        }
        if (libraryOptions == 0) {        
            libraryOptions = CMDL_LIBRARY_ADDMEMBER;  // default action is add member
        }
        SLCommand libcmd;
        libcmd.command = libraryOptions;
        libcmd.filename = fileNameBuffer.pushString(string);
        libcmd.value = libcmd.filename;
        lcommands.push(libcmd);
        return;
    }

    if (linkmode) {
        // linker command
        if (linkmode == CMDL_LINK_EXEFILE) {
            if (job == CMDL_JOB_LINK) {
                // first filename = executable file
                inputFile = cmd.fileNameBuffer.pushString(string);
                outputFile = inputFile;
                linkmode = CMDL_LINK_ADDMODULE;
            }
            else if (job == CMDL_JOB_RELINK) {
                if (inputFile == 0) {
                    // first filename = executable input file
                    inputFile = cmd.fileNameBuffer.pushString(string);
                }
                else {
                    // second filename = executalbe output file
                    outputFile = cmd.fileNameBuffer.pushString(string);
                    linkmode = CMDL_LINK_ADDMODULE;
                }
            }
            return;
        }
        // object file to add, remove, or extract
        if (linkOptions == CMDL_LINK_LIST) {
            err.submit(ERR_LINK_LIST_ONLY);  return;
        }
        if (linkOptions == 0) {        
            linkOptions = CMDL_LINK_ADDMODULE;  // default action is add modules
        }
        SLCommand linkcmd;
        linkcmd.command = linkOptions;
        linkcmd.filename = fileNameBuffer.pushString(string);
        linkcmd.value = linkcmd.filename;
        lcommands.push(linkcmd);
        return;
    }

    // no libmode or linkmode: Ordinary input or output file
    if (!inputFile) {
        // Input file not specified yet
        inputFile = fileNameBuffer.pushString(string);
    }
    else if (!outputFile) {
        // Output file not specified yet
        outputFile = fileNameBuffer.pushString(string);
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

    // Detect option type
    switch(string[0] | 0x20) {
    case 'a':   // assemble option
        if (job) err.submit(ERR_MULTIPLE_COMMANDS, string);     // More than one job specified
        job = CMDL_JOB_ASS;
        if (strncasecmp_(string, "ass", 3) == 0) {
            interpretAssembleOption(string+3);
        }
        else err.submit(ERR_UNKNOWN_OPTION, string);     // Unknown option
        break;

    case 'c':   // codesize option
        if (strncasecmp_(string, "codesize", 8) == 0) {
            interpretCodeSizeOption(string+8);
        }
        else err.submit(ERR_UNKNOWN_OPTION, string);     // Unknown option
        break;


    case 'd':   // datasize, disassemble or dump option
        if (strncasecmp_(string, "dis", 3) == 0) {
            if (job) err.submit(ERR_MULTIPLE_COMMANDS, string);     // More than one job specified
            job = CMDL_JOB_DIS;
            interpretDisassembleOption(string+3);  break;
        }
        if (strncasecmp_(string, "dump", 4) == 0) {
            if (job) err.submit(ERR_MULTIPLE_COMMANDS, string);     // More than one job specified
            job = CMDL_JOB_DUMP;
            interpretDumpOption(string+4);  break;
        }
        if (strncasecmp_(string, "datasize", 8) == 0) {
            interpretDataSizeOption(string+8);
            break;
        }
        err.submit(ERR_UNKNOWN_OPTION, string);     // Unknown option
        break;

    case 'e':    // Emulation or error option
        if (strncasecmp_(string, "emu", 3) == 0) {
            if (job) err.submit(ERR_MULTIPLE_COMMANDS, string);     // More than one job specified
            job = CMDL_JOB_EMU;
            emumode = 1;
        }
        else {
            interpretErrorOption(string);
        }
        break;

    case 'h': case '?':  // Help
        job = CMDL_JOB_HELP;
        break;

    case 'i':   // Instruction list
        if (strncasecmp_(string, "ilist=", 6) == 0) {
        interpretIlistOption(string+6);
        }
        else {        
            err.submit(ERR_UNKNOWN_OPTION, string);     // Unknown option
        }
        break;

    case 'l':   // Link, Library, and list file options
        if (strncasecmp_(string, "lib", 3) == 0) {
            if (job) err.submit(ERR_MULTIPLE_COMMANDS, string);     // More than one job specified
            job = CMDL_JOB_LIB;
            interpretLibraryCommand(string+3);  break;
        }
        if (strncasecmp_(string, "link", 4) == 0) {
            if (job) err.submit(ERR_MULTIPLE_COMMANDS, string);     // More than one job specified
            job = CMDL_JOB_LINK;  outputType = FILETYPE_FWC_EXE;
            interpretLinkCommand(string+4);  break;
        }
        if (strncasecmp_(string, "list=", 5) == 0) {
            interpretListOption(string+5);  break;
        }
        err.submit(ERR_UNKNOWN_OPTION, string);     // Unknown option
        break;

    case 'm':    // Maxerrors option
        if (strncasecmp_(string, "maxerrors", 9) == 0) {
            interpretMaxErrorsOption(string+9);  break;
        }
        err.submit(ERR_UNKNOWN_OPTION, string);     // Unknown option
        break;

    case 'o':    // Optimization option
        interpretOptimizationOption(string+1);
        break;

    case 'r':    // Relink command
        if (strncasecmp_(string, "relink", 6) == 0) {
            if (job) err.submit(ERR_MULTIPLE_COMMANDS, string);     // More than one job specified
            job = CMDL_JOB_RELINK;  outputType = FILETYPE_FWC_EXE;
            interpretLinkCommand(string+6);  break;
        }
        break;

    case 'v':    // verbose/silent
        interpretVerboseOption(string+1);
        break;

    case 'w':    // Warning option
        interpretErrorOption(string);  break;

    default:    // Unknown option
        err.submit(ERR_UNKNOWN_OPTION, string);
    }
}

void CCommandLineInterpreter::interpretAssembleOption(char * string) {
    if ((string[0] | 0x20) == 'b') dumpOptions = 2; // binary listing!
    else if (string[0] == 0 || (string[0] | 0x20) == 'e') outputType = FILETYPE_FWC;
    else interpretCommandOption(string);        
}
    
void CCommandLineInterpreter::interpretDisassembleOption(char * string) {
    outputType = CMDL_OUTPUT_ASM;
}

void CCommandLineInterpreter::interpretDumpOption(char * string) {
    // Interpret dump option from command line
    if (outputType /*&& outputType != outputType*/) {
        err.submit(ERR_MULTIPLE_COMMANDS, string);   // Both dump and convert specified
    }
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
        case 'm':   // show names of relinkable modules and libraries
            dumpOptions |= DUMP_RELINKABLE;  break;
        case 'c':   // dump comment records (currently only for OMF)
            dumpOptions |= DUMP_COMMENT;  break;         
        case '-': case '_':  // '-' may separate options
            break;
        default:
            err.submit(ERR_UNKNOWN_OPTION, string-1);  // Unknown option
        }
    }
    outputType = CMDL_OUTPUT_DUMP;
}

void CCommandLineInterpreter::interpretEmulateOption(char * string) {
    // Interpret emulate options
    if ((uint8_t)*string <= (uint8_t)' ') {
        err.submit(ERR_EMPTY_OPTION); return;    // Warning: empty option
    }
    // Detect option type
    switch(string[0] | 0x20) {
    case 'l':   // list option
        if (strncasecmp_(string, "list=", 5) == 0) {
            interpretListOption(string+5);  break;
        }
        err.submit(ERR_UNKNOWN_OPTION, string-1);  // Unknown option
        break;
    case 'm':
        if (strncasecmp_(string, "maxerrors", 9) == 0) {
            interpretMaxErrorsOption(string + 9);  break;
        }
        if (strncasecmp_(string, "maxlines", 8) == 0) {
            interpretMaxLinesOption(string + 8);  break;
        }        
        err.submit(ERR_UNKNOWN_OPTION, string);     // Unknown option
        break;
    }

}

void CCommandLineInterpreter::interpretLibraryCommand(char * string) {
    // Interpret options for manipulating library/archive files

    // meaning of libmode:
    // 0: not a library command
    // 1: after "-lib". expecting library name
    // 2: after library name or -a. expecting command or object files to add
    // 3: after "-d". expecting object files to delete
    // 4: after "-l". list all members
    // 5: after "-x". expecting object files to extract
    // 6: after "-xall". extract all members

    // Check for -lib command
    if (inputFile) {
        libmode = 2;                  // Input file already specified. Remaining file names are object files to add
    }
    else {
        libmode = 1;                  // The rest of the command line must be interpreted as library name and object file names
    }
    fileOptions |= CMDL_FILE_IN_IF_EXISTS | CMDL_FILE_IN_OUT_SAME;
}

// interpret library commands
void CCommandLineInterpreter::interpretLibraryOption(char * string) {
    switch (string[0] | 0x20) {
    case 'a':      // Add input file to library
        libmode = 2;
        libraryOptions = CMDL_LIBRARY_ADDMEMBER;
        break;

    case 'd':   // Delete member from library
        libmode = 3;
        libraryOptions = CMDL_LIBRARY_DELETEMEMBER;
        break;

    case 'l':
        libmode = 4;
        if (libraryOptions) { // cannot combine with other commands
            err.submit(ERR_LIBRARY_LIST_ONLY);  return;  
        }
        libraryOptions = CMDL_LIBRARY_LISTMEMBERS;
        if (string[1] > ' ') verbose = atoi(string+1);
        return;

    case 'v':    // verbose/silent
        interpretVerboseOption(string+1);
        return;

    case 'x':      // Extract member(s) from library
        libmode = 5;
        libraryOptions = CMDL_LIBRARY_EXTRACTMEM;
        if (strncasecmp_(string+2, "all", 3) == 0) {
            libmode = 6;
            cmd.libraryOptions = CMDL_LIBRARY_EXTRACTALL;
            if (string[5] > ' ') err.submit(ERR_UNKNOWN_OPTION, string);
            return;
        }    
        break;

    default:    
        err.submit(ERR_UNKNOWN_OPTION, string);  // Unknown option
    }
    // check if immediately followed by filename
    if (string[1] > ' ') interpretFileName(string+2);
}


// Interpret -link command
void CCommandLineInterpreter::interpretLinkCommand(char * string) {
    if (inputFile) {
        linkmode = CMDL_LINK_ADDMODULE;                  // executable file already specified. Remaining file names are object files and library files to add
    }
    else {
        linkmode = CMDL_LINK_EXEFILE;                  // The rest of the command line must be interpreted as executable name and object file names
    }
    if (job == CMDL_JOB_LINK) {    
        fileOptions |= CMDL_FILE_IN_IF_EXISTS | CMDL_FILE_IN_OUT_SAME;
    }
} 


// interpret linker options
void CCommandLineInterpreter::interpretLinkOption(char * string) {
    switch (string[0] | 0x20) {
    case 'a':      // Add input file to executable
        linkmode = CMDL_LINK_ADDMODULE;
        linkOptions = CMDL_LINK_ADDMODULE;
        break;

    case 'r':   // Add input files as relinkable
        linkmode = CMDL_LINK_ADDMODULE;
        linkOptions = CMDL_LINK_ADDMODULE | CMDL_LINK_RELINKABLE;
        fileOptions |= CMDL_FILE_RELINKABLE;
        break;

    case 'l':
        linkmode = CMDL_LINK_LIST;
        if (linkOptions) { // cannot combine with other commands
            err.submit(ERR_LINK_LIST_ONLY);  return;  
        }
        linkOptions = CMDL_LINK_LIST;
        if (string[1] > ' ') verbose = atoi(string+1);
        return;

    case 'm':      // Explicitly add specified module from previously specified library
        linkmode = CMDL_LINK_ADDLIBMODULE;
        linkOptions = CMDL_LINK_ADDLIBMODULE | (linkOptions & CMDL_LINK_RELINKABLE);
        break;

    case 'x':      // Extract module from relinkable executable file
        linkmode = CMDL_LINK_EXTRACT;
        linkOptions = CMDL_LINK_EXTRACT;
        libraryOptions = CMDL_LIBRARY_EXTRACTMEM;
        if (strncasecmp_(string+1, "all", 3) == 0) {
            libmode = CMDL_LINK_EXTRACT_ALL;
            libraryOptions = CMDL_LIBRARY_EXTRACTALL;
            if (string[5] > ' ') err.submit(ERR_UNKNOWN_OPTION, string);
            return;
        }    
        break;

    case 'u':
        fileOptions |= CMDL_FILE_INCOMPLETE | CMDL_FILE_RELINKABLE;
        if (string[1] > ' ') 
            err.submit(ERR_UNKNOWN_OPTION, string);
        return;

    case 'v':    // verbose/silent
        interpretVerboseOption(string+1);
        return;

    case 's':    // stacksize
        if (strncasecmp_(string, "stack=", 6) == 0) {
            interpretStackOption(string+6);
            return;
        }
        break;

    case 'h':    // heapsize
        if (strncasecmp_(string, "heap=", 5) == 0) {
            interpretHeapOption(string+5);
            return;
        }
        break;

    case 'd':
        if (strncasecmp_(string, "dynlink=", 8) == 0) {
            interpretDynlinkOption(string+8);  // space for dynamic linking
            return;
        }
        // Remove relinkable modules
        linkmode = CMDL_LINK_REMOVE;
        linkOptions = CMDL_LINK_REMOVE;
        break;

    case 'e':  case 'w':
        interpretErrorOption(string);  
        return;

    default:
        err.submit(ERR_UNKNOWN_OPTION, string);  // Unknown option
        return;
    }
    // check if immediately followed by filename
    if (string[1] > ' ') interpretFileName(string+1);
}

void CCommandLineInterpreter::interpretIlistOption(char * string) {
    // Interpret instruction list file option for assembler
    instructionListFile = fileNameBuffer.pushString(string);
}

void CCommandLineInterpreter::interpretListOption(char * string) {
    // Interpret instruction list file option for assembler
    outputListFile = fileNameBuffer.pushString(string);
    if (maxLines == 0) maxLines = 1000;
}

void CCommandLineInterpreter::interpretStackOption(char * string) {
    // Interpret stack size option for linker
    // stack=number1,number2,number3
    // number1 = call stack size, bytes
    // number2 = data stack size, bytes
    // number3 = additional size for vectors on data stack. value will be multiplied by maximum vector length
    SLCommand linkcmd;       // command record
    uint32_t e;              // return code from interpretNumber
    linkcmd.filename = 0;
    linkcmd.command = CMDL_LINK_STACKSIZE;
    linkcmd.value = interpretNumber(string, 32, &e);   // get first number = call stack size
    if (e && !(e & 0x1000)) {err.submit(ERR_UNKNOWN_OPTION, string);  return;}
    lcommands.push(linkcmd);                           // save first number
    if (e & 0x1000) { // second number specified
        string += (e & 0xFFF) + 1;
        linkcmd.command++;
        linkcmd.value = interpretNumber(string, 32, &e);   // get second number = data stack size
        if (e && !(e & 0x1000)) {err.submit(ERR_UNKNOWN_OPTION, string);  return;}
        lcommands.push(linkcmd);                           // save second number
        if (e & 0x1000) { // third number specified
            string += (e & 0xFFF) + 1;
            linkcmd.command++;
            linkcmd.value = interpretNumber(string, 32, &e);   // get third number = number of vectors on data stack
            if (e) {err.submit(ERR_UNKNOWN_OPTION, string);  return;}
            lcommands.push(linkcmd);                           // save third number
        }
    }
}

void CCommandLineInterpreter::interpretHeapOption(char * string) {
// Interpret heap size option for linker
// Interpret stack size option for linker
// heap=number
// number = heap size, bytes
    SLCommand linkcmd;       // command record
    uint32_t e;              // return code from interpretNumber
    linkcmd.filename = 0;
    linkcmd.command = CMDL_LINK_HEAPSIZE;
    linkcmd.value = interpretNumber(string, 32, &e);   // get first number = call stack size
    if (e) {err.submit(ERR_UNKNOWN_OPTION, string);  return;}
    lcommands.push(linkcmd);
}

void CCommandLineInterpreter::interpretDynlinkOption(char * string) {
// Interpret dynamic link size option for linker
// dynlink=number1,number2,number3
// number1 = size for read-only data, bytes
// number2 = size for executable section, bytes
// number3 = size for writeable static data, bytes
    SLCommand linkcmd;       // command record
    uint32_t e;              // return code from interpretNumber
    linkcmd.filename = 0;
    linkcmd.command = CMDL_LINK_DYNLINKSIZE;
    linkcmd.value = interpretNumber(string, 32, &e);   // get first number = read-only
    if (e && !(e & 0x1000)) {err.submit(ERR_UNKNOWN_OPTION, string);  return;}
    lcommands.push(linkcmd);                           // save first number
    if (e & 0x1000) { // second number specified
        string += (e & 0xFFF) + 1;
        linkcmd.command++;
        linkcmd.value = interpretNumber(string, 32, &e);   // get second number = executable section
        if (e && !(e & 0x1000)) {err.submit(ERR_UNKNOWN_OPTION, string);  return;}
        lcommands.push(linkcmd);                           // save second number
        if (e & 0x1000) { // third number specified
            string += (e & 0xFFF) + 1;
            linkcmd.command++;
            linkcmd.value = interpretNumber(string, 32, &e);   // get third number = writeable data
            if (e & ~0x1000) {err.submit(ERR_UNKNOWN_OPTION, string);  return;}
            lcommands.push(linkcmd);                           // save third number
        }
    }
}

void CCommandLineInterpreter::interpretOptimizationOption(char * string) {
    if (string[0] < '0' || string[0] > '9' || strlen(string) != 1) {
        err.submit(ERR_UNKNOWN_OPTION, string); return; // Unknown option
    }
    optiLevel = string[0] - '0';
}

void CCommandLineInterpreter::interpretVerboseOption(char * string) {
    // Interpret silent/verbose option from command line
    verbose = atoi(string);
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

void CCommandLineInterpreter::interpretMaxLinesOption(char * string) {
    // Interpret maxlines option from command line
    if (string[0] == '=') string++;
    uint32_t error = 0;
    maxLines = (uint32_t)interpretNumber(string, 99, &error);
    if (error) err.submit(ERR_UNKNOWN_OPTION, string);
}


void CCommandLineInterpreter::reportStatistics() {
    // Report statistics about name changes etc.
}


void CCommandLineInterpreter::checkExtractSuccess() {
    // Check if library members to extract were found
    //!!
}


void CCommandLineInterpreter::checkOutputFileName() {
    // Make output file name or check that requested name is valid
    if (!(fileOptions & CMDL_FILE_OUTPUT)) return;

    if (outputFile == 0) {    
        // Output file name not specified. Make filename    
        outputFile = setFileNameExtension(inputFile, outputType);
    }
    // Check if input and output files have same name
    if (strcmp(getFilename(inputFile), getFilename(outputFile)) == 0 && !(cmd.fileOptions & CMDL_FILE_IN_OUT_SAME)) {
        err.submit(ERR_FILES_SAME_NAME, getFilename(inputFile));
    }
}


uint32_t CCommandLineInterpreter::setFileNameExtension(uint32_t fn, int filetype) {
    // Set file name extension for output file according to FileType
    // Names are stored as indexes into cmd.fileNameBuffer

    // get old name
    const char * name1 = getFilename(fn);
    int i;
    uint32_t newname = 0;

    // Search for last '.' in file name
    for (i = (int)strlen(name1)-1; i > 0; i--) if (name1[i] == '.') break;
    if (i < 1) {
        // '.' not found. Append '.' to name
        i = (int)strlen(name1);
    }
    // Get default extension
    const char * defaultExtension;
    switch (filetype) {
    case FILETYPE_ASM:
        // don't give disassembly the same extension because it may overwrite the original assembly file
        defaultExtension = ".das"; break;
    case FILETYPE_FWC: case FILETYPE_ELF:
        defaultExtension = ".ob"; break;
    case FILETYPE_FWC_EXE:
        defaultExtension = ".ex"; break;
    case FILETYPE_FWC_LIB:
        defaultExtension = ".li"; break;
    default:
        defaultExtension = ".txt";
    }
    // generate new name in cmd.fileNameBuffer
    newname = fileNameBuffer.push(name1, i + (uint32_t)strlen(defaultExtension) + 1);
    strcpy((char*)cmd.fileNameBuffer.buf() + newname + i, defaultExtension);
    return newname;
}

// Get file name from index into fileNameBuffer
const char * CCommandLineInterpreter::getFilename(uint32_t n) {
    if (n >= fileNameBuffer.dataSize()) return "unknown?";
    return (const char *)fileNameBuffer.buf() + n;
}


void CCommandLineInterpreter::help() {
    // Print help message
    printf("\nBinary tools version %i.%02i beta for ForwardCom instruction set.", FORWARDCOM_VERSION, FORWARDCOM_SUBVERSION);
    printf("\nCopyright (c) 2020 by Agner Fog. Gnu General Public License.");
    printf("\n\nUsage: forw command [options] inputfile [outputfile] [options]");
    printf("\n\nCommand:");
    printf("\n-ass       Assemble\n");
    printf("\n-dis       Disassemble object or executable file\n");
    printf("\n-link      Link object files into executable file\n");
    printf("\n-relink    Relink and modify executable file\n");
    printf("\n-lib       Build or manage library file\n");
    printf("\n-emu       Emulate and debug executable file\n");
    printf("\n-dump-XXX  Dump file contents to console.");
    printf("\n           Values of XXX (can be combined):");
    printf("\n           f: File header, h: section Headers, s: Symbol table,");
    printf("\n           m: Relinkable modules, r: Relocation table, n: string table.\n");
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
    printf("\nforw -ass test.as test.ob");
    printf("\nforw -link test.ex test.ob libc.li");
    printf("\nforw -emu test.ex -list=debugout.txt\n\n");
}

// compare strings, ignore case for a-z
int strncasecmp_(const char *s1, const char *s2, uint32_t n) {
    //return strnicmp(s1, s2, n);          // MS
    //return strncasecmp(s1, s2, n);       // Linux
    for (uint32_t i = 0; i < n; i++) {     // loop through string
        char c1 = s1[i];
        char c2 = s2[i];
        if (uint8_t(c1-'A') <= uint8_t('Z' - 'A')) c1 |= 0x20; // convert A-Z to a-z
        if (uint8_t(c2-'A') <= uint8_t('Z' - 'A')) c2 |= 0x20; // convert A-Z to a-z
        if (c1 != c2) return int(c1) - int(c2);                // difference found
        if (c1 == 0) break;                                    // end of string
    }
    return 0;                                                  // no difference between strings
}

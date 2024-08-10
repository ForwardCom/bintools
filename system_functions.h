/*************************    system_functions.h    ***************************
* Author:        Agner Fog
* Date created:  2018-03-20
* Last modified: 2020-11-25
* Version:       1.13
* Project:       Binary tools for ForwardCom instruction set
* Module:        system_functions.h
* Description:
* Header file for ForwardCom system function ID numbers
*
* Note: id values are preliminary, they may change
*
* Copyright 2018-2024 GNU General Public License http://www.gnu.org/licenses
******************************************************************************/

// Event ID's. System-specific and user-defined ID numbers may be added
#define EVT_NONE                      0     // ignore
#define EVT_CONSTRUCT                 1     // call static constructors and initialization procedures before calling main
#define EVT_DESTRUCT                  2     // call static destructors and clean up after return from main
#define EVT_LOADLIB                   3     // a library has been loaded dynamically at runtime. initialize it
#define EVT_UNLOADLIB                 4     // a library is about to be unloaded dynamically
#define EVT_CLOSE                     5     // a request for closing the current process
#define EVT_PARENTNOTIFY              6     // notification from child process to parent process
#define EVT_ERROR                  0x10     // error handler
#define EVT_TIMER                  0x20     // timer event
#define EVT_COMMAND                0x30     // user command from keyboard, menu, or icon click
#define EVT_MESSAGE                0x31     // keyboard or mouse message or inter-process message
#define EVT_CALL                   0x32     // call to an add-on module

// Interrupt ID's. ID numbers for error interrupts
#define INT_BREAKPOINT             0x01     // debug breakpoint
#define INT_UNKNOWN_INST           0x80     // unknown instruction
#define INT_WRONG_PARAMETERS       0x81     // illegal or unsupported parameters for instruction
#define INT_ACCESS_READ            0x82     // memory access violation, read
#define INT_ACCESS_WRITE           0x83     // memory access violation, write
#define INT_ACCESS_EXE             0x84     // memory access violation, execute
#define INT_CALL_STACK             0x85     // call stack overflow or underflow
#define INT_ARRAY_BOUNDS           0x88     // array bounds overflow, unsigned
#define INT_MISALIGNED_MEM         0x89     // misaligned memory access
#define INT_MISALIGNED_JUMP        0x8A     // jump to an address not divisible by 4

// Interrupt ID's for software traps. Note that software traps are not necessarily supported
#define INT_OVERFL_UNSIGN          0x101     // integer overflow, unsigned
#define INT_OVERFL_SIGN            0x102     // integer overflow, signed
#define INT_OVERFL_FLOAT           0x103     // floating point overflow
#define INT_FLOAT_INVALID          0x104     // floating point invalid operation
#define INT_FLOAT_UNDERFL          0x105     // floating point underflow or precision loss
#define INT_FLOAT_NAN_LOSS         0x106     // floating point nan propagation loss (nan input to compare or conversion to integer)

// module ID
#define SYSM_SYSTEM               0x001  // system module id

// process function IDs
#define SYSF_EXIT                 0x010  // terminate program
#define SYSF_ABORT                0x011  // abort program
#define SYSF_TIME                 0x020  // time

// input/output functions
#define SYSF_PUTS                 0x101  // write string to stdout
#define SYSF_PUTCHAR              0x102  // write character to stdout
#define SYSF_PRINTF               0x103  // write formatted output to stdout
#define SYSF_FPRINTF              0x104  // write formatted output to file
#define SYSF_SNPRINTF             0x105  // write formatted output to string buffer 
#define SYSF_FOPEN                0x110  // open file
#define SYSF_FCLOSE               0x111  // close file
#define SYSF_FREAD                0x112  // read from file
#define SYSF_FWRITE               0x113  // write to file
#define SYSF_FFLUSH               0x114  // flush file
#define SYSF_FEOF                 0x115  // check if end of file
#define SYSF_FTELL                0x116  // get file position
#define SYSF_FSEEK                0x117  // set file position
#define SYSF_FERROR               0x118  // get file error
#define SYSF_GETCHAR              0x120  // read character from stdin
#define SYSF_FGETC                0x121  // read character from file
#define SYSF_FGETS                0x123  // read string from file
#define SYSF_GETS_S               0x124  // read string from stdin
#define SYSF_SCANF                0x130  // read formatted input from stdin
#define SYSF_FSCANF               0x131  // read formatted input from file
#define SYSF_SSCANF               0x132  // read formatted input from string buffer
#define SYSF_REMOVE               0x140  // delete file

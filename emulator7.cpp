/****************************  emulator7.cpp  ********************************
* Author:        Agner Fog
* date created:  2018-02-18
* Last modified: 2020-02-08
* Version:       1.02
* Project:       Binary tools for ForwardCom instruction set
* Description:
* Emulator: System functions
*
* Copyright 2018-2020 GNU General Public License http://www.gnu.org/licenses
*****************************************************************************/

#include "stdafx.h"

// Data encoding names
SIntTxt interruptNames[] = {
    {INT_UNKNOWN_INST,       "Unknown instruction"},
    {INT_INST_ILLEGAL,       "Illegal instruction code"},
    {INT_ACCESS_READ,        "Memory read access violation"},
    {INT_ACCESS_WRITE,       "Memory write access violation"},
    {INT_ACCESS_EXE,         "Memory execute access violation"},
    {INT_CALL_STACK,         "Call stack overflow or underflow"},
    {INT_ARRAY_BOUNDS,       "Array bounds violation"},
    {INT_OVERFL_UNSIGN,      "Unsigned integer overflow"},
    {INT_OVERFL_SIGN,        "Signed integer overflow"},
    {INT_OVERFL_FLOAT,       "Floating point overflow"},
    {INT_FLOAT_INVALID,      "Floating point invalid operation"},
    {INT_FLOAT_UNDERFL,      "Floating point underflow"},
    {INT_FLOAT_NAN_LOSS,     "Floating point NAN in compare or conversion to integer"},
    {0xFFFF,                 "Filler interrupt"},
};

// System function names
SIntTxt systemFunctionNames[] = {
    {SYSF_EXIT,              "exit"},       // terminate program
    {SYSF_ABORT,             "abort"},      // abort program
    {SYSF_TIME,              "time"},       // time in seconds since jan 1, 1970    

// input/output functions
    {SYSF_PUTS,              "puts"},       // write string to stdout
    {SYSF_PUTCHAR,           "putchar"},    // write character to stdout
    {SYSF_PRINTF,            "printf"},     // write formatted output to stdout
    {SYSF_FPRINTF,           "fprintf"},    // write formatted output to file
    {SYSF_SNPRINTF,          "snprintf"},    // write formatted output to string buffer 
    {SYSF_FOPEN,             "fopen"},      //  open file
    {SYSF_FCLOSE,            "fclose"},     // SYSF_FCLOSE
    {SYSF_FREAD,             "fread"},      // read from file
    {SYSF_FWRITE,            "fwrite"},     // write to file 
    {SYSF_FFLUSH,            "fflush"},     // flush file 
    {SYSF_FEOF,              "feof"},       // check if end of file 
    {SYSF_FTELL,             "ftell"},      // get file position 
    {SYSF_FSEEK,             "fseek"},      // set file position 
    {SYSF_FERROR,            "ferror"},     // get file error    
    {SYSF_GETCHAR,           "getchar"},    // read character from stdin 
    {SYSF_FGETC,             "fgetc"},      // read character from file 
    {SYSF_FGETS,             "fgets"},      // read string from file 
    {SYSF_SCANF,             "scanf"},      // read formatted input from stdio 
    {SYSF_FSCANF,            "fscanf"},     // read formatted input from file 
    {SYSF_SSCANF,            "sscanf"},     // read formatted input from string buffer 
    {SYSF_REMOVE,            "remove"},     // delete file 
};

// number of entries in list
const int numSystemFunctionNames = sizeof(systemFunctionNames) / sizeof(SIntTxt);


// interrupt or trap
void CThread::interrupt(uint32_t n) {
    if (n >= INT_UNKNOWN_INST) {  // unrecoverable error
        terminate = true;              // stop execution
        returnType = 0;
    }
    if (listFileName && cmd.maxLines != 0) {   // write interrupt to debug output
        listOut.tabulate(emulator->disassembler.asmTab0);
        const char * iname = Lookup(interruptNames, n);
        listOut.put(iname);
        if (terminate) listOut.put(". Terminating");
        listOut.newLine();
    }
}

/*
// give error message if compiled for 32 bit
void checkVa_listSize() {
    // C variable argument list va_list is not compatible with ForwardCom in 32 bit mode
    if (sizeof(void*) < 8) { // 32 bit host system. va_list has 32-bit entries except for %f 
        puts("\nError: forw must be compiled in 64 bit mode. printf function may fail\n");
    }
} */

// check if system function has access to a particular address
uint64_t CThread::checkSysMemAccess(uint64_t address, uint64_t size, uint8_t rd, uint8_t rs, uint8_t mode) {
    // rd = register pointing to beginning of shared memory area, rs = size of shared memory area
    // mode = SHF_READ or SHF_WRITE
    // return value is possibly reduced size, or zero if no access
    if ((rd | rs) == 0) return 0;                // no access if both are r0
    uint64_t base = registers[rd];               // beginning of shared area
    uint64_t bsize = registers[rs];              // size of shared area
    if (address + size < address) size = ~address; // avoid overflow
    if (base + bsize < base) bsize = ~base;      // avoid overflow
    if ((rd & rs & 0x1F) != 0x1F) {              // share all if both are r31
        // check if within shared area
        if (address < base) return 0;
        if (address + size > base + bsize) size = base + bsize - address;
    }
    // check application's memory map
    uint32_t index = mapIndex3;
    // find index
    while (address < memoryMap[index].startAddress) {
        if (index > 0) index--;
        else return 0;
    }
    while (address >= memoryMap[index+1].startAddress) {
        if (index+2 < memoryMap.numEntries()) index++;
        else return 0;
    }
    // check read/write permission
    if ((memoryMap[index].access_addend & mode) != mode) return 0;

    // check if multiple map entries covered
    uint32_t index2 = index;
    while (address + size >= memoryMap[index2+1].startAddress
        && index2+2 < memoryMap.numEntries() 
        && (memoryMap[index2+1].access_addend & mode) == mode) {
            index2++;
        }
    uint64_t size2 = memoryMap[index2+1].startAddress - address;  // maximum possible size
    if (size < size2) size = size2;
    return size;
}

// emulate fprintf with ForwardCom argument list
int CThread::fprintfEmulated(FILE * stream, const char * format, uint64_t * argumentList) {
    // a ForwardCom argument list is compatible with a va_list in 64-bit windows but not in Linux
    static CMemoryBuffer fstringbuf;             // buffer containing format string
    fstringbuf.setSize(0);                       // discard any previously stored string
    fstringbuf.pushString(format);               // copy format string
    // split the format string into substrings with a single format specifier in each
    uint32_t arg = 0;                            // argument index
    int returnValue;                             // return value;
    int returnSum = 0;                           // sum of return values;
    char * startp;                               // start of current substring in format string
    char * percentp1;                            // percent sign in current substring
    char * percentp2;                            // next percent sign starting next substring
    startp = fstringbuf.getString(0);            // start of string buffer
    percentp1 = startp;                          // search for first % sign
    while (true) {
        percentp1 = strchr(percentp1, '%');
        if (percentp1 && percentp1[1] == '%') percentp1 += 2; // skip "%%" which is not a format code
        else break;
    }
    // loop for substrings of format string containing only one format specifier each
    do {
        char c;                                      // format character
        int asterisks = 0;
        bool isString = false;
        if (percentp1) {
            percentp2 = percentp1 + 1;               // search for next % sign
            while (true) {
                percentp2 = strchr(percentp2, '%');
                if (percentp2 && percentp2[1] == '%') percentp2 += 2; // skip "%%" which is not a format code
                else break;
            }
            if (percentp2) *percentp2 = 0;           // put temporary end of string at next % sign
            // check if argument is a string, and count asterisks
            int i = 1;
            while (true) {
                c = percentp1[i++];                  // read character in format specifier
                if (c == 0) break;                   // end of string
                if (c == '*') asterisks++;           // count asterisks
                c |= 0x20;                           // lower case
                if (c == 's') isString = true;       // %s means string
                if (c >= 'a' && c <= 'z') break;     // a letter terminates the format specifier
            }
        }
        else {
            percentp2 = 0;
        }
        uint64_t argument = argumentList[arg];   // The argument list can contain any type of argument with size up to 64 bits
        union {
            uint64_t a;
            double d;
        } uu;
        if (isString) argument += (uint64_t)memory; // translate string address
        // Print current argument with format substring.
        if (asterisks) { // asterisks indicate extra arguments
            if (c == 'a' || c == 'e' || c == 'f' || c == 'g') {
                // floating point argument
                if (asterisks == 1) {
                    uu.a = argumentList[arg+1];
                    returnValue = fprintf(stream, startp, argument, uu.d, argumentList[arg+2]);
                }
                else { // asterisks = 2
                    uu.a = argumentList[arg+2];
                    returnValue = fprintf(stream, startp, argument, argumentList[arg+1], uu.d);
                }
            }
            else { // integer argument
                returnValue = fprintf(stream, startp, argument, argumentList[arg+1], argumentList[arg+2]);
            }
            arg += asterisks + 1;
        }
        else {
            if (c == 'a' || c == 'e' || c == 'f' || c == 'g') {
                // floating point argument
                uu.a = argument;
                returnValue = fprintf(stream, startp, uu.d);
            }
            else {            
                returnValue = fprintf(stream, startp, argument);
            }
            arg++;
        }
        if (returnValue < 0) return returnValue; // return error
        else returnSum += returnValue;           // sum of return values
        if (percentp2) *percentp2 = '%';         // re-insert next % sign
        startp = percentp1 = percentp2;
    }
    while (startp);                              // loop to next substring
    return returnSum;                            // return total number of characters written
}


// entry for system calls
void CThread::systemCall(uint32_t mod, uint32_t funcid, uint8_t rd, uint8_t rs) {
    if (listFileName) {    
        // debug listing
        listOut.tabulate(emulator->disassembler.asmTab0);
        listOut.put("system call: ");
        if (mod == SYSM_SYSTEM) { // search for function name
            for (int i = 0; i < numSystemFunctionNames; i++) {
                if (systemFunctionNames[i].a == funcid) { // name is in list
                    listOut.put(systemFunctionNames[i].b);
                    goto NAME_WRITTEN;
                }
            }
        }
        // name not found. write id
        listOut.putHex(mod);  listOut.put(":");  listOut.putHex(funcid);
        NAME_WRITTEN:
        listOut.newLine();
    }
    uint64_t temp;    // temporary
    uint64_t dsize;   // data size
    if (mod == SYSM_SYSTEM) {// system function
        // dispatch by function id
        switch (funcid) {
        case SYSF_EXIT:      // terminate program
            cmd.mainReturnValue = (int)registers[0];
            terminate = true;  break;
        case SYSF_ABORT:     // abort program
            cmd.mainReturnValue = (int)registers[0];
            terminate = true;  break;
        case SYSF_TIME:      // time
            temp = time(0);
            if (registers[0] && checkSysMemAccess(registers[0], 8, rd, rs, SHF_WRITE)) *(uint64_t*)(memory + registers[0]) = temp;
            registers[0] = temp;  break;
        case SYSF_PUTS:      // write string to stdout
            if (strlen((const char*)memory + registers[0]) > checkSysMemAccess(registers[0], -1, rd, rs, SHF_READ)) {
                interrupt(INT_ACCESS_READ);
            }
            else puts((const char*)memory + registers[0]);
            break;
        case SYSF_PUTCHAR:   // write character to stdout
            putchar((char)registers[0]); 
            break;
        case SYSF_PRINTF:    // write formatted output to stdout
            registers[0] = fprintfEmulated(stdout, (const char*)memory + registers[0], (uint64_t*)(memory + registers[1]));
            break; 
        case SYSF_FPRINTF:   // write formatted output to file
            registers[0] = fprintfEmulated((FILE *)(registers[0]), (const char*)memory + registers[1], (uint64_t*)(memory + registers[2]));
            break;
            /*
        case SYSF_SNPRINTF:   // write formatted output to string buffer 
            // this works only in 64 bit windows
            dsize = registers[1];  // size of data to read
            if (checkSysMemAccess(registers[0], dsize, rd, rs, SHF_WRITE) < dsize) {
                interrupt(INT_ACCESS_WRITE); // write access violation
                ret = 0;
            }
            else ret = snprintf((char*)memory + registers[0], registers[1], (const char*)memory + registers[2], (const char*)memory + registers[3]);
            registers[0] = ret;
            break;*/
        case SYSF_FOPEN:     //  open file
            registers[0] = (uint64_t)fopen((const char*)memory + registers[0], (const char*)memory + registers[1]);
            break;
        case SYSF_FCLOSE:    // SYSF_FCLOSE
            registers[0] = (uint64_t)fclose((FILE*)registers[0]);
            break;
        case SYSF_FREAD:     // read from file
            dsize = registers[1] * registers[2];  // size of data to read
            if (checkSysMemAccess(registers[0], dsize, rd, rs, SHF_WRITE) < dsize) {
                interrupt(INT_ACCESS_WRITE); // write access violation
                registers[0] = 0;
            }
            else registers[0] = (uint64_t)fread(memory + registers[0], (size_t)registers[1], (size_t)registers[2], (FILE *)(size_t)registers[3]);
            break;
        case SYSF_FWRITE:    // write to file 
            dsize = registers[1] * registers[2];  // size of data to write
            if (checkSysMemAccess(registers[0], dsize, rd, rs, SHF_READ) < dsize) {
                interrupt(INT_ACCESS_READ); // write access violation
                registers[0] = 0;
            }
            else registers[0] = (uint64_t)fwrite(memory + registers[0], (size_t)registers[1], (size_t)registers[2], (FILE *)(size_t)registers[3]);
            break;
        case SYSF_FFLUSH:    // flush file 
            registers[0] = (uint64_t)fflush((FILE *)registers[0]);
            break;
        case SYSF_FEOF:      // check if end of file 
            registers[0] = (uint64_t)feof((FILE *)registers[0]);
            break;
        case SYSF_FTELL:     // get file position 
            registers[0] = (uint64_t)ftell((FILE *)registers[0]);
            break;
        case SYSF_FSEEK:     // set file position 
            registers[0] = (uint64_t)fseek((FILE *)registers[0], (long int)registers[1], (int)registers[2]);
            break;
        case SYSF_FERROR:    // get file error
            registers[0] = (uint64_t)ferror((FILE *)registers[0]);
            break;
        case SYSF_GETCHAR:   // read character from stdin 
            registers[0] = (uint64_t)getchar();
            break;
        case SYSF_FGETC:     // read character from file 
            registers[0] = (uint64_t)fgetc((FILE *)registers[0]);
            break;
        case SYSF_FGETS:     // read string from file 
            dsize = registers[1];  // size of data to read
            if (checkSysMemAccess(registers[0], dsize, rd, rs, SHF_WRITE) < dsize) {
                interrupt(INT_ACCESS_WRITE); // write access violation
                registers[0] = 0;
            }
            else {
                registers[0] = (uint64_t)fgets((char *)(memory+registers[0]), (int)registers[1], (FILE *)registers[2]);
            }
            break;
        case SYSF_GETS_S:     // read string from stdin 
            dsize = registers[1];  // size of data to read
            if (checkSysMemAccess(registers[0], dsize, rd, rs, SHF_WRITE) < dsize) {
                interrupt(INT_ACCESS_WRITE); // write access violation
                registers[0] = 0;
            }
            else {
                char * r = fgets((char *)(memory+registers[0]), (int)registers[1], stdin);
                if (r == 0) registers[0] = 0;  // registers[0] unchanged if success
            }
            break;
            /*
        case SYSF_SCANF:     // read formatted input from stdio 
            ret = vscanf((char *)(memory+registers[0]), (va_list)(memory + registers[1]));
            if (checkSysMemAccess(registers[0], ret, rd, rs, SHF_WRITE) < ret) {
                interrupt(INT_ACCESS_WRITE); // write access violation
            }
            registers[0] = ret;
            break;
        case SYSF_FSCANF:    // read formatted input from file 
            ret = vfscanf((FILE *)registers[0], (char *)(memory+registers[1]), (va_list)(memory + registers[2]));
            if (checkSysMemAccess(registers[0], ret, rd, rs, SHF_WRITE) < ret) {
                interrupt(INT_ACCESS_WRITE); // write access violation
            }
            registers[0] = ret;
            break;
        case SYSF_SSCANF:    // read formatted input from string buffer 
            ret = vsscanf((char *)(memory+registers[0]), (char *)(memory+registers[1]), (va_list)(memory + registers[2]));
            if (checkSysMemAccess(registers[0], ret, rd, rs, SHF_WRITE) < ret) {
                interrupt(INT_ACCESS_WRITE); // write access violation
            }
            registers[0] = ret;
            break; */
        case SYSF_REMOVE:    // delete file 
            registers[0] = (uint64_t)remove((char *)(memory+registers[0]));
            break;
        }
    }
}

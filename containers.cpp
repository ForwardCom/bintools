/****************************  containers.cpp  **********************************
* Author:        Agner Fog
* Date created:  2006-07-15
* Last modified: 2017-11-03
* Version:       1.00
* Project:       Binary tools for ForwardCom instruction set
*
* This module contains container classes CMemoryBuffer and CFileBuffer for
* dynamic memory allocation and file read/write. See containers.h for
* further description.
*
* Copyright 2006-2017 GNU General Public License http://www.gnu.org/licenses
*****************************************************************************/

#include "stdafx.h"

// Names of file formats
SIntTxt FileFormatNames[] = {
    {FILETYPE_ELF,          "x86 ELF"},
    {FILETYPE_FWC,          "ForwardCom ELF"},
    {FILETYPE_ASM,          "Assembly"},
    {FILETYPE_FWC_EXE,      "ForwardCom executable"},
    {FILETYPE_FWC_LIB,      "ForwardCom library"}
};


// Members of class CMemoryBuffer

// Constructor
CMemoryBuffer::CMemoryBuffer() {  
    buffer = 0;
    num_entries = data_size = buffer_size = 0;
}

// Destructor
CMemoryBuffer::~CMemoryBuffer() {
    clear();                                  // De-allocate buffer
}

// De-allocate buffer
void CMemoryBuffer::clear() {
    if (buffer) delete[] buffer;
    buffer = 0;
    num_entries = data_size = buffer_size = 0;
}

void CMemoryBuffer::setSize(uint32_t size) {
    // Allocate, reallocate or deallocate buffer of specified size.
    // DataSize is initially zero. It is increased by push or pushString.
    // Setting size > dataSize will allocate more buffer and fill it with zeroes but not increase dataSize.
    // Setting size < dataSize will decrease dataSize so that some of the data are discarded.
    if (size < data_size) {
        // Request to delete some data
        data_size = size;
        return;
    }
    if (size <= buffer_size) {
        // Request to reduce size but not delete it
        return;                                  // Ignore
    }
    size = (size + buffer_size + 15) & uint32_t(-16);   // Double size and round up to value divisible by 16
    int8_t * buffer2 = 0;                        // New buffer
    buffer2 = new int8_t[size];                  // Allocate new buffer
    if (buffer2 == 0) {err.submit(ERR_MEMORY_ALLOCATION); return;} // Error can't allocate
    memset (buffer2, 0, size);                   // Initialize to all zeroes
    if (buffer) {
        // A smaller buffer is previously allocated
        memcpy (buffer2, buffer, buffer_size);   // Copy contents of old buffer into new
        delete[] buffer;                         // De-allocate old buffer
    }
    buffer = buffer2;                            // Save pointer to buffer
    buffer_size = size;                          // Save size
}

uint32_t CMemoryBuffer::push(void const * obj, uint32_t size) {
    // Add object to buffer, return offset
    // Parameters: 
    // obj = pointer to object, 0 if fill with zeroes
    // size = size of object to push

    // Old offset will be offset to new object
    uint32_t OldOffset = data_size;

    // New data size will be old data size plus size of new object
    uint32_t NewOffset = data_size + size;

    if (NewOffset > buffer_size) {
        // Buffer too small, allocate more space.
        // We can use SetSize for this only if it is certain that obj is not 
        // pointing to an object previously allocated in the old buffer
        // because it would be deallocated before copied into the new buffer:
        // SetSize (NewOffset + NewOffset / 2 + 1024);

        // Allocate more space without using SetSize:
        // Double the size + 1 kB, and round up size to value divisible by 16
        uint32_t NewSize = (NewOffset * 2 + 1024 + 15) & uint32_t(-16);
        int8_t * buffer2 = 0;                    // New buffer
        // Allocate new buffer
        buffer2 = new int8_t[NewSize];
        if (buffer2 == 0) {
            // Error can't allocate
            err.submit(ERR_MEMORY_ALLOCATION);  return 0;
        }
        // Initialize to all zeroes
        memset (buffer2, 0, NewSize);
        if (buffer) {
            // A smaller buffer is previously allocated
            // Copy contents of old buffer into new
            memcpy (buffer2, buffer, buffer_size);
        }
        buffer_size = NewSize;                   // Save size
        if (obj && size) {                         
            // Copy object to new buffer
            memcpy (buffer2 + OldOffset, obj, size);
            obj = 0;                             // Prevent copying once more
        }
        // Delete old buffer after copying object
        if (buffer) delete[] buffer;

        // Save pointer to new buffer
        buffer = buffer2;
    }
    // Copy object to buffer if nonzero
    if (obj && size) {
        memcpy (buffer + OldOffset, obj, size);
    }
    if (size) {
        // Adjust new offset
        data_size = NewOffset;
    }
    num_entries++;

    // Return offset to allocated object
    return OldOffset;
}

uint32_t CMemoryBuffer::pushString(char const * s) {
    // Add ASCIIZ string to buffer, return offset
    return push (s, uint32_t(strlen(s))+1);
}

uint32_t CMemoryBuffer::getLastIndex() const {
    // Index of last object pushed (zero-based)
    return num_entries - 1;
}

void CMemoryBuffer::align(uint32_t a) {
    // Align next entry to address divisible by a. must be a power of 2
    // uint32_t NewOffset = (data_size + a - 1) / a * a;   // use this if a is not a power of 2
    uint32_t NewOffset = (data_size + a - 1) & (-(int32_t)a);
    if (NewOffset > buffer_size) {
        // Allocate more space
        setSize (NewOffset + 2048);
    }
    // Set DataSize to after alignment space
    data_size = NewOffset;
}

// Make a copy of whole buffer
void CMemoryBuffer::copy(CMemoryBuffer const & b) {
    setSize(0);                                  // clear own buffer
    setSize(b.dataSize());                       // set new size
    memcpy(buffer, b.buf(), b.dataSize());       // copy data
    num_entries = b.numEntries();                // copy num_entries
    data_size = b.dataSize();                    // size used
}

// Members of class CFileBuffer
CFileBuffer::CFileBuffer() : CMemoryBuffer() {  
    // Default constructor
    fileName = 0;
    outputFileName = 0;
    fileType = wordSize = executable = 0;
}

CFileBuffer::CFileBuffer(char const * filename) : CMemoryBuffer() {  
    // Constructor
    fileName = filename;
    fileType = wordSize = 0;
}

void CFileBuffer::read(int ignoreError) {                   
    // Read file into buffer
    // InoreError: 0: abort on error, 1: ignore error, 2: search for file also in exe directory
    uint32_t status;                             // Error status

    const int MAXPATHL = 1024;                   // Buffer for constructing file path
    char name[MAXPATHL];
#if defined (_WIN32) || defined (__WINDOWS__)
    const char slash[2] = "\\";  // path separator depends on operating system
#else
    const char slash[2] = "/";
#endif

#ifdef _MSC_VER  // Microsoft compiler prefers this:

    int fh;                                      // File handle
    fh = _open(fileName, O_RDONLY | O_BINARY);   // Open file in binary mode
    if (fh == -1) {
        if (ignoreError == 2 && strlen(cmd.programName) + strlen(fileName) < MAXPATHL) {
            // Search for file in directory of executable file
            strcpy(name, cmd.programName);
            char *s1 = name, * s2;               // find last slash
            do {
                s2 = strchr(s1+1, slash[0]);
                if (s2) s1 = s2;  else *s1 = 0;
            } while(s2);
            strcat(name, slash);
            strcat(name, fileName);
            fh = _open(name, O_RDONLY | O_BINARY); // Open file in binary mode
        }
        if (fh == -1) {
            // Cannot read file
            if (ignoreError != 1) err.submit(ERR_INPUT_FILE, fileName); // Error. Input file must be read
            setSize(0); return;                  // Make empty file buffer
        }
    }
    data_size = _filelength(fh);                 // Get file size
    if (data_size <= 0) {
        if (ignoreError != 1) err.submit(ERR_FILE_SIZE, fileName); // Wrong size
        return;}
    setSize(data_size + 2048);                   // Allocate buffer, 2k extra
    status = _read(fh, buf(), data_size);        // Read from file
    if (status != data_size) err.submit(ERR_INPUT_FILE, fileName);
    status = _close(fh);                         // Close file
    if (status != 0) err.submit(ERR_INPUT_FILE, fileName);

#else              // Works with most compilers:

    FILE * fh = fopen(fileName, "rb");
    if (!fh) {
        // Cannot read file
        if (ignoreError == 2 && strlen(cmd.programName) + strlen(fileName) < MAXPATHL) {
            // Search for file in directory of executable file
            strcpy(name, cmd.programName);
            char *s1 = name, * s2;               // find last slash
            do {
                s2 = strchr(s1+1, slash[0]);
                if (s2) s1 = s2;  else *s1 = 0;
            } while(s2);
            strcat(name, slash);
            strcat(name, fileName);
            fh = fopen(name, "rb");
        }
        if (!fh) {
            // Cannot read file
            if (ignoreError != 1) err.submit(ERR_INPUT_FILE, fileName); // Error. Input file must be read
            setSize(0); return;                  // Make empty file buffer
        }
    }
    // Find file size
    fseek(fh, 0, SEEK_END);
    long int fsize = ftell(fh);
    if (fsize <= 0 || (unsigned long)fsize >= 0xFFFFFFFF) {
        // File too big or zero size
        err.submit(ERR_FILE_SIZE, fileName); fclose(fh); return;
    }
    data_size = (uint32_t)fsize;
    rewind(fh);
    // Allocate buffer
    setSize(data_size + 2048);                    // Allocate buffer, 2k extra
    // Read entire file
    status = (uint32_t)fread(buf(), 1, data_size, fh);
    if (status != data_size) err.submit(ERR_INPUT_FILE, fileName);
    status = fclose(fh);
    if (status != 0) err.submit(ERR_INPUT_FILE, fileName);

#endif
}

void CFileBuffer::write() {                  
    // Write buffer to file:
    if (outputFileName) fileName = outputFileName;
    else {
        err.submit(ERR_OUTPUT_FILE);
        return;
    }
    // Two alternative ways to write a file:

#ifdef _MSC_VER    // Microsoft compiler prefers this:

    int fh;                                      // File handle
    uint32_t status;                             // Error status
    // Open file in binary mode
    fh = _open(fileName, O_RDWR | O_BINARY | O_CREAT | O_TRUNC, _S_IREAD | _S_IWRITE); 
    // Check if error
    if (fh == -1) {err.submit(ERR_OUTPUT_FILE, fileName);  return;}
    // Write file
    status = _write(fh, buf(), data_size);
    // Check if error
    if (status != data_size) err.submit(ERR_OUTPUT_FILE, fileName);
    // Close file
    status = _close(fh);
    // Check if error
    if (status != 0) err.submit(ERR_OUTPUT_FILE, fileName);

#else              // Works with most compilers:

    // Open file in binary mode
    FILE * ff = fopen(fileName, "wb");
    // Check if error
    if (!ff) {err.submit(ERR_OUTPUT_FILE, fileName);  return;}
    // Write file
    uint32_t n = (uint32_t)fwrite(buf(), 1, data_size, ff);
    // Check if error
    if (n != data_size) err.submit(ERR_OUTPUT_FILE, fileName);
    // Close file
    n = fclose(ff);
    // Check if error
    if (n) {err.submit(ERR_OUTPUT_FILE, fileName);  return;}

#endif
}

int CFileBuffer::getFileType() {
    // Detect file type
    if (fileType) return fileType;               // File type already known
    if (!data_size) return 0;                    // No file
    if (!buf()) return 0;                        // No contents

    //uint32_t namelen = fileName ? (uint32_t)strlen(fileName) : 0;

    //if (strncmp((char*)Buf(),ELFMAG,4) == 0) {
    if (get<uint32_t>(0) == ELFMAG) { 
        // ELF file
        fileType = FILETYPE_ELF;
        executable = get<Elf64_Ehdr>(0).e_type != ET_REL;
        switch (buf()[EI_CLASS]) {
        case ELFCLASS32:
            wordSize = 32; break;
        case ELFCLASS64:
            wordSize = 64; break;
        }
        machineType = get<Elf64_Ehdr>(0).e_machine;   // Copy file header.e_machine;
    }
    else {
        // Unknown file type
        int utype = get<uint32_t>(0);        
        err.submit(ERR_UNKNOWN_FILE_TYPE, utype, fileName); 
        fileType = 0;
    }
    return fileType;
}


char const * CFileBuffer::getFileFormatName(int FileType) {
    // Get name of file format type
    return Lookup (FileFormatNames, FileType);
}


void CFileBuffer::setFileType(int type) {
    // Set file format type
    fileType = type;
}

void CFileBuffer::reset() {
    // Set all members to zero
    setSize(0);                                  // Deallocate memory buffer
    memset(this, 0, sizeof(*this));
}

char * CFileBuffer::setFileNameExtension(const char * f) {
    // Set file name extension according to FileType
    static char name[MAXFILENAMELENGTH+8];
    int i;

    if (strlen(f) > MAXFILENAMELENGTH) err.submit(ERR_FILE_NAME_LONG, f);
    strncpy(name, f, MAXFILENAMELENGTH);

    // Search for last '.' in file name
    for (i = (int)strlen(name)-1; i > 0; i--) if (name[i] == '.') break;
    if (i < 1) {
        // '.' not found. Append '.' to name
        i = (int)strlen(name); if (i > MAXFILENAMELENGTH-4) i = MAXFILENAMELENGTH-4;
    }
    // Get default extension
    const char * defaultExtension;
    switch (cmd.outputType) {
    case FILETYPE_ASM:
        defaultExtension = ".as"; break;
    case FILETYPE_FWC: case FILETYPE_ELF:
        defaultExtension = ".ob"; break;
    case FILETYPE_FWC_EXE:
        defaultExtension = ".ex"; break;
    case FILETYPE_FWC_LIB:
        defaultExtension = ".li"; break;
    default:
        defaultExtension = ".txt";
    }
    strcpy(name + i, defaultExtension);          // Add default extension
    return name;
}

void CFileBuffer::checkOutputFileName() {
    // Make output file name or check that requested name is valid
    if (!(cmd.fileOptions & CMDL_FILE_OUTPUT)) return;

    outputFileName = cmd.outputFile;
    if (outputFileName == 0) {
        // Output file name not specified. Make filename
        outputFileName = cmd.outputFile = setFileNameExtension(fileName);
    }
    if (strcmp(fileName,outputFileName) == 0 && !(cmd.fileOptions & CMDL_FILE_IN_OUT_SAME)) {
        // Input and output files have same name
        err.submit(ERR_FILES_SAME_NAME, fileName);
    }
}

void operator >> (CMemoryBuffer & a, CMemoryBuffer & b) {
    // Transfer ownership of buffer and other properties from a to b
    b.setSize(0);                                // De-allocate old buffer from target if it has one
    b.buffer = a.buffer;                         // Transfer buffer
    a.buffer = 0;                                // Remove buffer from source, so that buffer has only one owner

    // Copy properties
    b.data_size   = a.dataSize();                // Size of data, offset to vacant space
    b.buffer_size = a.bufferSize();              // Size of allocated buffer
    b.num_entries = a.numEntries();              // Number of objects pushed
    a.setSize(0);                                // Reset a's properties
}

void operator >> (CFileBuffer & a, CFileBuffer & b) {
    // Transfer ownership of buffer and other properties from a to b
    b.setSize(0);                                // De-allocate old buffer from target if it has one
    b.buffer = a.buffer;                         // Transfer buffer
    a.buffer = 0;                                // Remove buffer from source, so that buffer has only one owner

    // Copy properties
    b.data_size   = a.dataSize();                // Size of data, offset to vacant space
    b.buffer_size = a.bufferSize();              // Size of allocated buffer
    b.num_entries = a.numEntries();              // Number of objects pushed
    b.executable = a.executable;                 // File is executable
    b.machineType = a.machineType;               // Machine type
    if (a.wordSize) b.wordSize = a.wordSize;     // Segment word size (16, 32, 64)
    if (a.fileName) b.fileName = a.fileName;     // Name of input file
    if (a.outputFileName) b.outputFileName = a.outputFileName;// Name of output file
    if (a.getFileType())  b.fileType = a.getFileType();       // Object file type
    a.setSize(0);                                // Reset a's properties
}


// Class CTextFileBuffer is used for building text files
// Constructor
CTextFileBuffer::CTextFileBuffer() {
    column = 0;
#ifdef _WIN32
    lineType = 0;                                // DOS/Windows type linefeed
#else
    lineType = 1;                                // Unix type linefeed
#endif
}

uint32_t CTextFileBuffer::put(const char * text) {
    // Write text string to buffer
    uint32_t len = (uint32_t)strlen(text);       // Length of text
    uint32_t ret = push(text, len);              // Add to buffer without terminating zero
    column += len;                               // Update column
    return ret;                                  // Return index
}

void CTextFileBuffer::put(const char character) {
    // Write single character to buffer
    push(&character, 1);                         // Add to buffer
    column ++;                                   // Update column
}

uint32_t CTextFileBuffer::putStringN(const char * s, uint32_t len) {
    // Write string to buffer, add terminating zero
    static const int8_t nul = 0;
    uint32_t retval = push(s, len);  
    push(&nul, 1);
    num_entries--;                               // compensate for pushing twice
    column += len + 1;
    return retval;
}

void CTextFileBuffer::newLine() {
    // Add linefeed
    if (lineType == 0) {
        push("\r\n", 2);                         // DOS/Windows style linefeed
    }
    else {
        push("\n", 1);                           // UNIX style linefeed
    }
    column = 0;                                  // Reset column
}

void CTextFileBuffer::tabulate(uint32_t i) {
    // Insert spaces until column i
    uint32_t j;
    if (i > column) {                            // Only insert spaces if we are not already past i
        for (j = column; j < i; j++) push(" ", 1); // Insert i - column spaces
        column = i;                              // Update column
    }
}

void CTextFileBuffer::putDecimal(int32_t x, int IsSigned) {
    // Write decimal number to buffer, unsigned or signed
    char text[16];
    sprintf(text, IsSigned ? "%i" : "%u", x);
    put(text);
}

void CTextFileBuffer::putHex(uint8_t x, int ox) {
    // Write hexadecimal 8 bit number to buffer
    // ox meaning: 1 = put 0x prefix, 2 = prefix zeroes to fixed length
    char text[16];
    sprintf(text, ox & 2 ? "%s%02X" : "%s%X", ox & 1 ? "0x" : "", x);
    put(text);
}

void CTextFileBuffer::putHex(uint16_t x, int ox) {
    // Write hexadecimal 16 bit number to buffer
    // ox meaning: 1 = put 0x prefix, 2 = prefix zeroes to fixed length
    char text[16];
    sprintf(text, ox & 2 ? "%s%04X" : "%s%X", ox & 1 ? "0x" : "", x);
    put(text);
}

void CTextFileBuffer::putHex(uint32_t x, int ox) {
    // Write hexadecimal 32 bit number to buffer
    // ox meaning: 1 = put 0x prefix, 2 = prefix zeroes to fixed length
    char text[16];
    sprintf(text, ox & 2 ? "%s%08X" : "%s%X", ox & 1 ? "0x" : "", x);
    put(text);
}

void CTextFileBuffer::putHex(uint64_t x, int ox) {
    // Write unsigned hexadecimal 64 bit number to buffer
    // ox meaning: 1 = put 0x prefix, 2 = prefix zeroes to fixed length
    char text[32];
    if (ox & 2) {  // Print all digits
        sprintf(text, "%s%08X%08X", ox & 1 ? "0x" : "", highDWord(x), uint32_t(x));
    }
    else { // Skip leading zeroes
        if (highDWord(x)) {
            sprintf(text, "%s%X%08X", ox & 1 ? "0x" : "", highDWord(x), uint32_t(x));
        }
        else {
            sprintf(text, "%s%X", ox & 1 ? "0x" : "", uint32_t(x));
        }
    }
    put(text);
}

void CTextFileBuffer::putFloat(float x) {
    // Write floating point number to buffer
    char text[64];
    sprintf(text, "%.7G", x);
    put(text);
}

void CTextFileBuffer::putFloat(double x) {
    // Write floating point number to buffer
    char text[64];
    sprintf(text, "%.16G", x);
    put(text);
}

/****************************  converters.h   ********************************
* Author:        Agner Fog
* Date created:  2017-04-17
* Last modified: 2018-03-30
* Version:       1.10
* Project:       Binary tools for ForwardCom instruction set
* Module:        converters.h
* Description:
* Header file for file conversion classes.
*
* Copyright 2006-2020 GNU General Public License http://www.gnu.org/licenses
*****************************************************************************/

/*******************************   Classes   ********************************

This header file declares various classes for interpreting and converting
different types of object files. These classes are all derived from the 
container class CFileBuffer, declared in containers.h.

See containers.h for an explanation of the container classes and the 
operators >> and << which can transfer a data buffer from an object of one
class to an object of another class.

*****************************************************************************/

#pragma once

// Buffer for symbol names during assembly, linking, and library operations
// symbolNameBuffer is made global in order to make it accessible to bool operator < (ElfFWC_Sym2 const &, ElfFWC_Sym2 const &)
// It is defined in assem1.cpp
extern CTextFileBuffer symbolNameBuffer;      // Buffer for symbol names 


// Structure for string index entry in library
struct SSymbolEntry {
    uint32_t name;                        // name as offset into symbolNameBuffer
    uint32_t library;                     // library index (1-based) or 0 if object file
    uint32_t member;                      // module or library member offset
    uint32_t sectioni;                    // section index within module
    uint32_t symindex;                    // index into symbol table
    uint16_t st_other;                    // attributes: STV_SECT_ATTR
    uint8_t  st_type;                     // symbol type
    uint8_t  st_bind;                     // symbol binding
    uint8_t  status;                      // 1: has no value yet. 2: has been matched. 4: unresolved
};

// operator for sorting string entries
inline bool operator < (SSymbolEntry const & a, SSymbolEntry const & b) {
    int compare = strcmp(symbolNameBuffer.getString(a.name), symbolNameBuffer.getString(b.name));
    if (compare) return compare < 0;             // compare names
    if ((a.st_bind | b.st_bind) & STB_IGNORE) return false;    // ignore binding
    return (a.st_bind & STB_WEAK) < (b.st_bind & STB_WEAK);    // strong before weak
}

// operator for comparing string entries. Compares name only
inline bool operator == (SSymbolEntry const & a, SSymbolEntry const & b) {
    return strcmp(symbolNameBuffer.getString(a.name), symbolNameBuffer.getString(b.name)) == 0;
}


// Class CConverter contains the input file. 
// It redirects the task specified on the command line to some other converter class
class CConverter : public CFileBuffer {
public:
    CConverter();                       // Constructor
    void go();                          // Main action
protected:
    void readInputFile();               // read input file
    void dumpELF();                     // Dump x86 ELF file
    void disassemble();                 // Disassemble ForwardCom ELF file
    void assemble();                    // Assemble ForwardCom assembly file
    void link();                        // Link object files into executable file
    void emulate();                     // emulate and run executable file
    void lib();                         // Build or modify function libraries
};

// Class for interpreting and dumping ELF files
class CELF : public CFileBuffer {
public:
    CELF();                                       // Default constructor
    void parseFile();                             // Parse file buffer
    void dump(int options);                       // Dump file
    void makeLinkMap(FILE * stream);              // Write a link map
    void listSymbols(CMemoryBuffer * strings, CDynamicArray<SSymbolEntry> * index, uint32_t m, uint32_t l, int scope); // Make list of public and external symbols
    ElfFwcSym * getSymbol(uint32_t symindex);     // Get a symbol record
    int  split();                                 // Split ELF file into containers
    int  join(ElfFwcEhdr * header);               // Join containers into ELF file
    void reset();                                 // Reset everything
    uint32_t addSection(ElfFwcShdr & section, CMemoryBuffer const & strings, CMemoryBuffer const & data); // Add section header and section data
    void extendSection(ElfFwcShdr & section, CMemoryBuffer const & data);                 // Extend previously added section
    void insertFiller(uint64_t numBytes);         // Insert alignment fillers between sections
    void addModuleNames(CDynamicArray<uint32_t>& moduleNames, CDynamicArray<uint32_t>& libraryNames);
    void updateModuleNames(CDynamicArray<ElfFwcShdr> &newSectionHeaders, CMemoryBuffer &newShStrtab); // Put module names and library names into section string table for relinkable sections
    // Add module name and library name to relinkable sections
    void addProgHeader(ElfFwcPhdr & header);      // Add program header
    uint32_t addSymbol(ElfFwcSym & symbol, CMemoryBuffer const & strings);               // Add a symbol
    void addRelocation(ElfFwcReloc & relocation); // Add a relocation
    void removePrivateSymbols(int debugOptions);  // remove local symbols and adjust relocation records with new symbol indexes
    CDynamicArray<ElfFwcShdr> const & getSectionHeaders() const { return sectionHeaders; }  // get sectionHeaders for copying. Used by disassembly listing
    CDynamicArray<ElfFwcSym> const & getSymbols() const { return symbols; }                // get symbols buffer for copying
    CDynamicArray<ElfFwcReloc> const & getRelocations() const { return relocations; }      // get relocations buffer for copying
    CMemoryBuffer const & getStringBuffer() const { return stringBuffer; }                  // get string buffer for copying
    CMemoryBuffer const & getDataBuffer() const { return dataBuffer; }                      // get data buffer for copying
    CFileBuffer & makeHexBuffer();                // make hexadecimal code file
    CDynamicArray<ElfFwcSym> symbols;             // List of symbols
    uint32_t moduleName;                          // Name of file or module as index into cmd.fileNameBuffer
    uint32_t library;                             // Library index if this module is extracted from a library
    bool relinkable;                              // module can be replaced by relinking
    const char * symbolName(uint32_t index);      // Get name of symbol
protected:
    char * secStringTable;                        // Section header string table
    uint32_t secStringTableLen;                   // Length of section header string table
    uint32_t nSections;                           // Number of sections
    int sectionHeaderSize;                        // Size of each section header
    uint32_t symbolTableOffset;                   // Offset to symbol table
    uint32_t symbolTableEntrySize;                // Entry size of symbol table
    uint32_t symbolTableEntries;                  // Number of symbols
    uint32_t symbolStringTableOffset;             // Offset to symbol string table
    uint32_t symbolStringTableSize;               // Size of symbol string table
    ElfFwcEhdr fileHeader;                        // Copy of file header
    CDynamicArray<ElfFwcShdr> sectionHeaders;     // Copy of section headers
    CDynamicArray<ElfFwcPhdr> programHeaders;     // Copy of program headers   
    CDynamicArray<ElfFwcReloc> relocations;       // List of relocations
    CMemoryBuffer stringBuffer;                   // String buffer with names of symbols and sections
    CMemoryBuffer dataBuffer;                     // Raw data for sections
    CDynamicArray<uint32_t> moduleNames;          // module names as index into cmd.fileNameBuffer. used by join function
    CDynamicArray<uint32_t> libraryNames;         // library names as index into cmd.stringBuffer. used by join function
    friend class CLinker;                         // Give linker access to internal buffers
    friend void operator >> (CELF & a, CELF & b); // Transfer ownership of buffer
    friend void operator << (CELF & a, CELF & b); // Transfer ownership of buffer
};

// transfer ownership of data buffer and properties except what can be generated by the split function
inline void operator >> (CELF & a, CELF & b) {
    static_cast<CFileBuffer&>(a) >> static_cast<CFileBuffer&>(b);
    b.moduleName = a.moduleName;
    b.library = a.library;
    b.relinkable = a.relinkable;
}

inline void operator << (CELF & b, CELF & a) {
    a >> b;
}

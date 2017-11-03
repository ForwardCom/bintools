/****************************  converters.h   ********************************
* Author:        Agner Fog
* Date created:  2017-04-17
* Last modified: 2017-11-03
* Version:       1.00
* Project:       Binary tools for ForwardCom instruction set
* Module:        converters.h
* Description:
* Header file for file conversion classes.
*
* Copyright 2006-2017 GNU General Public License http://www.gnu.org/licenses
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

// Structure for string index entry in library
struct SStringEntry {
    uint32_t string;                      // Offset to string
    uint32_t member;                      // Library member
};

// Class CResponseFileBuffer is used for storage of a command line response file
class CResponseFileBuffer : public CFileBuffer {
public:
   CResponseFileBuffer(char const * filename);   // Constructor
   ~CResponseFileBuffer();                       // Destructor
   CResponseFileBuffer * next;                   // Linked list if more than one buffer
};


// Class CConverter contains the input file. 
// It redirects the task specified on the command line to some other converter class
class CConverter : public CFileBuffer {
public:
   CConverter();                       // Constructor
   void go();                          // Main action
protected:
   void dumpELF();                     // Dump x86 ELF file
   void disassemble();                 // Disassemble ForwardCom ELF file
   void assemble();                    // Assemble ForwardCom assembly file
   void link(){};                      // Link object files into executable file
   void lib(){};                       // Build or modify function libraries
};

// Class for interpreting and dumping ELF files
class CELF : public CFileBuffer {
public:
   CELF();                                       // Default constructor
   void parseFile();                             // Parse file buffer
   void dump(int options);                       // Dump file
   void publicNames(CMemoryBuffer * strings, CDynamicArray<SStringEntry> * index, int m); // Make list of public names
   int  split();                                 // Split ELF file into containers
   int  join(uint32_t e_type);                   // Join containers into ELF file
   uint32_t addSection(Elf64_Shdr & section, CMemoryBuffer const & strings, CMemoryBuffer const & data); // Add section header and section data
   void addProgHeader(Elf64_Phdr & header);      // Add program header
   uint32_t addSymbol(ElfFWC_Sym & symbol, CMemoryBuffer const & strings);   // Add a symbol
   void addRelocation(ElfFWC_Rela2 & relocation); // Add a relocation
   void removePrivateSymbols();                   // remove local symbols and adjust relocation records with new symbol indexes
   CDynamicArray<Elf64_Shdr> const & getSectionHeaders() const {return sectionHeaders;}  // get sectionHeaders for copying. Used by disassembly listing
   CDynamicArray<ElfFWC_Sym> const & getSymbols() const {return symbols;}                // get symbols buffer for copying
   CDynamicArray<ElfFWC_Rela2> const & getRelocations() const {return relocations;}       // get relocations buffer for copying
   CMemoryBuffer const & getStringBuffer() const {return stringBuffer;}                  // get string buffer for copying
   CMemoryBuffer const & getDataBuffer() const {return dataBuffer;}                      // get data buffer for copying
protected:
   const char * symbolName(uint32_t index);      // Get name of symbol
   char * secStringTable;                        // Section header string table
   uint32_t secStringTableLen;                   // Length of section header string table
   uint32_t nSections;                           // Number of sections
   int sectionHeaderSize;                        // Size of each section header
   uint32_t symbolTableOffset;                   // Offset to symbol table
   uint32_t symbolTableEntrySize;                // Entry size of symbol table
   uint32_t symbolTableEntries;                  // Number of symbols
   uint32_t symbolStringTableOffset;             // Offset to symbol string table
   uint32_t symbolStringTableSize;               // Size of symbol string table
   Elf64_Ehdr fileHeader;                        // Copy of file header
   CDynamicArray<Elf64_Shdr> sectionHeaders;     // Copy of section headers
   CDynamicArray<Elf64_Phdr> programHeaders;     // Copy of program headers   
   CDynamicArray<ElfFWC_Sym> symbols;            // List of symbols
   CDynamicArray<ElfFWC_Rela2> relocations;      // List of relocations
   CMemoryBuffer stringBuffer;                   // String buffer with names of symbols and sections
   CMemoryBuffer dataBuffer;                     // Raw data for sections
};

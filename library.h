/****************************  library.h   ********************************
* Author:        Agner Fog
* date created:  2017-11-08
* Last modified: 2018-03-30
* Version:       1.01
* Project:       Binary tools for ForwardCom instruction set
* Module:        library.h
* Description:
* header file for library manager
*
* Copyright 2017-2017 GNU General Public License http://www.gnu.org/licenses
*****************************************************************************/

// Structure for list of library members
struct SLibMember {
    uint32_t name;                     // member name as index into cmd.fileNameBuffer
    uint32_t oldOffset;                // offset in old library file
    uint32_t newOffset;                // offset in new library file
    uint32_t size;                     // size, not including library header
    uint32_t action;                   // 1: preserve, 2: add or replace, 3: delete, 5: extract
};


// operator < for sorting library member list by name
static inline bool operator < (SLibMember const & a, SLibMember const & b) {
    return strcmp(cmd.getFilename(a.name), cmd.getFilename(b.name)) < 0;
}

// remove path from file name
const char * removePath(const char * filename);

// Class for extracting members from library or building a library
class CLibrary : public CFileBuffer {
public:
    CLibrary();                                  // Constructor
    void go();                                   // Do whatever the command line says
    const char * getMemberName(uint32_t memberOffset); // Get name of a library member
    uint32_t getMemberSize(uint32_t memberOffset); // get size of a library member
    void findLongNames();                        // Find longNames record
    uint32_t findSymbol(const char * name);      // Find exported symbol in library
    bool isForwardCom();                         // check if this is a ForwardCom library
    void addELF(CELF & elf);                     // make library from CELF modules during relinking
    void makeInternalLibrary();                  // make a library for internal use during relinking
    uint32_t findMember(uint32_t name);          // Find a module. name is an index into cmd.fileNameBuffer
    uint32_t libraryName;                        // file name as index into cmd.fileNameBuffer
    bool relinkable;                             // library can be replaced by relinking
protected:
    void checkActionList();                      // Check action list for errors
    void makeMemberList();                       // Make list of library member names
    void runActionList();                        // Run through commands from command line
    void addMember(uint32_t filename, uint32_t membername); // Add object file to library member list
    void deleteMember(uint32_t membername);      // Add object file to library
    void extractMember(uint32_t filename, uint32_t membername); // Extract member from library
    void extractAllMembers();                    // Extract all members from library
    void listMembers();                          // List all library members
    void generateNewLibraryBody();               // Generate data contents of new library from old one and additional object files
    void makeBinaryFile();                       // Make library header, symbol table, longnames record, data
    void checkDuplicateSymbols(CDynamicArray<SSymbolEntry>& symbolList);// Check if symbollist contains duplicate names
    int32_t  alignBy;                            // member alignment = 4
    uint32_t longNames;                          // offset to long names record
    uint32_t longNamesSize;                      // size of long names record
    CDynamicArray<SLibMember> members;           // list of member names
    CFileBuffer outFile;                         // Buffer for building output file
    CELF memberBuffer;                           // Buffer containing single library member
    CMemoryBuffer dataBuffer;                    // Buffer containing raw members
    //friend class CLinker;
};

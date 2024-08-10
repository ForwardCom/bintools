/****************************  linker.h   ************************************
* Author:        Agner Fog
* date created:  2017-11-14
* Last modified: 2023-01-08
* Version:       1.13
* Project:       Binary tools for ForwardCom instruction set
* Module:        linker.h
* Description:
* header file for linker
*
* Copyright 2017-2024 GNU General Public License www.gnu.org/licenses
*****************************************************************************/

// Structure for list of imported library modules
struct SLibraryModule {
    uint32_t library;                  // library number. msb set if symbols have been registered
    uint32_t offset;                   // offset in executable file or library
    uint32_t modul;                    // index into modules2 buffer
};

// operator < for sorting library modules
static inline bool operator < (SLibraryModule const & a, SLibraryModule const & b) {
    if ((a.library << 1) != (b.library << 1)) return (a.library << 1) < (b.library << 1);
    return a.offset < b.offset;
}

// Structure for storing name of relinkable module from input file
struct SRelinkModule {
    uint32_t libraryName;              // name of library that the module comes from (as index into cmd.fileNameBuffer)
    uint32_t moduleName;               // name of module (as index into cmd.fileNameBuffer)
};

// operator < for sorting relink module list
static inline bool operator < (SRelinkModule const & a, SRelinkModule const & b) {
    // compare library names. (libraryName = 0 will give "")
    int j = strcmp(cmd.getFilename(a.libraryName), cmd.getFilename(b.libraryName));
    if (j) return j < 0;
    // compare module names
    j = strcmp(cmd.getFilename(a.moduleName), cmd.getFilename(b.moduleName));
    return j < 0;
}

// operator < for sorting event list
static inline bool operator < (ElfFwcEvent const & a, ElfFwcEvent const & b) {
    if (a.event_id != b.event_id) return a.event_id < b.event_id;
    if (a.key != b.key) return a.key < b.key;
    return a.priority > b.priority;
}


// Structure for list of sections. Sorted by the order in which they should be placed in the executable
struct SLinkSection {
    uint64_t sh_size;                  // Section size in bytes
    uint64_t sh_addr;                  // address in executable
    uint32_t sh_flags;                 // Section flags
    uint32_t sh_type;                  // Section type
    uint32_t name;                     // section name as index into cmd.fileNameBuffer
    uint32_t sh_module;                // module containing section (index into modules2)
    uint32_t sectioni;                 // section index within module
    uint32_t sectionx;                 // section index in final executable
    uint32_t order;                    // section must occur in this order in executable file
    uint8_t  sh_align;                 // alignment = 1 << sh_align
};

// Same structure, sorted by sh_module and sectioni
struct SLinkSection2 : public SLinkSection {
};

// operator < for sorting section list by the order in which they should be placed in the executable
static inline bool operator < (SLinkSection const & a, SLinkSection const & b) {
    if (a.order != b.order) return a.order < b.order;
    // same type and flags. sort by name
    int j = strcmp(cmd.getFilename(a.name), cmd.getFilename(b.name));
    if (j != 0) return j < 0;
    // same name. sort by module
    return a.sh_module < b.sh_module;
}

// operator < for sorting section list by sh_module and sectioni
static inline bool operator < (SLinkSection2 const & a, SLinkSection2 const & b) {
    if (a.sh_module != b.sh_module) return a.sh_module < b.sh_module;
    return a.sectioni < b.sectioni;
}

// extended relocation record used temporarily during linking
struct SReloc2 : public ElfFwcReloc {
    uint32_t modul;                    // module containing specified r_section
    bool symLocal;                     // r_sym is in same module. reference by number rather than by name 
    bool refSymLocal;                  // r_refsym is in same module. reference by number rather than by name 
};

// symbol cross reference record for connecting symbol records in local modules to records in executable file
struct SSymbolXref {
    uint32_t  name;                    // symbol name as index into global symbolNameBuffer
    uint32_t  modul;                   // module containing symbol
    uint32_t  symi;                    // index into symbols buffer within module
    uint32_t  symx;                    // index into symbols buffer in outFile
    bool      isPublic;                // symbol is public or exported
    bool      isWeak;                  // symbol is weak
};

struct SSymbolXref2 : public SSymbolXref {
};

// operator < for sorting symbol cross reference records by modul and symi
static inline bool operator < (SSymbolXref const & a, SSymbolXref const & b) {
    if (a.modul != b.modul) return a.modul < b.modul;
    return a.symi < b.symi;
}

// operator < for sorting symbol cross reference records by name
static inline bool operator < (SSymbolXref2 const & a, SSymbolXref2 const & b) {
    return strcmp(symbolNameBuffer.getString(a.name), symbolNameBuffer.getString(b.name)) < 0;
}

// symbol record, sorted by name
struct SSymbol2 : public ElfFwcSym {
    SSymbol2() {}                                // default constructor
    SSymbol2(ElfFwcSym const & s) {             // constructor to convert from normal symbol record
        *static_cast<ElfFwcSym*>(this) = s;
    }
};
    
// operator < for sorting symbol recordsby name
static inline bool operator < (SSymbol2 const & a, SSymbol2 const & b) {
    return strcmp(symbolNameBuffer.getString(a.st_name), symbolNameBuffer.getString(b.st_name)) < 0;
}


// Class for linking or re-linking executable file
class CLinker {
public:
    CLinker();                                   // Constructor
    void go();                                   // Do whatever the command line says
protected:
    void feedBackText1();                         // write feedback text on stdout
    void loadExeFile();                          // load executable file to be relinked
    void getReplaceNames();                      // get names of modules and libraries to remove or replace
    void markSectionsInInputFile();              // check which sections to keep or remove in executable input file
    void extractModule(CELF & modul, uint32_t libname, uint32_t name); // extract a module from executable input file
    void countReusedModules();                   // count number of modules and libraries to reuse when relinking
    void getRelinkObjects();                     // get all reused objects into modules1 metabuffer
    void extractModuleToFile(CELF & modu);       // extract a module from relinkable file if requested
    void getRelinkLibraries();                   // recover relinkable library modules
    void feedBackText2();                        // write feedback to console
    uint64_t ip_base;                            // pointer to end of const, begin of code
    uint64_t datap_base;                         // pointer to end of data, begin of bss
    uint64_t threadp_base;                       // pointer to begin of thread local memory
    uint64_t entry_point;                        // entry point for executable startup code
    uint32_t event_table;                        // address of event table
    uint32_t event_table_num;                    // number of entries in event table
    uint32_t unresolvedWeak;                     // there are unresolved weak imports
    uint32_t unresolvedWeakNum;                  // number of unresolved weak imports for writeable data
    uint32_t dummyConst;                         // address of dummy constant symbol for unresolved weak imports
    uint32_t dummyData;                          // address of dummy data symbol for unresolved weak imports
    uint32_t dummyThreadData;                    // address of dummy thread data symbol for unresolved weak imports
    uint32_t dummyFunc;                          // address of dummy function for unresolved weak imports
    uint32_t eventDataSize;                      // total size of all event data sections
    uint32_t numObjects;                         // number of object files to add
    uint32_t numLibraries;                       // number of library files to add
    uint32_t numRelinkObjects;                   // number of relinkable object modules to reuse
    uint32_t numRelinkLibraries;                 // number of relinkable library files to reuse
    void fillBuffers();                          // load specified object files and library files into buffers
    void countModules();                         // count number of modules and libraries to add
    void makeSymbolList();                       // make list of imported and exported symbols
    void matchSymbols();                         // match lists of imported and exported symbols
    void librarySearch();                        // search libraries for imported symbols
    void checkDuplicateSymbols();                // check for duplicate symbols
    void readLibraryModules();                   // get imported library modules into modules2 buffer
    void makeSectionList();                      // make list of all sections
    void sortSections();                         // sort sections in the order in which they should occur in the executable file
    void joinCommunalSections();                 // join communal sections with same name
    void makeDummySections();                    // make dummy segments for unresolved weak externals
    void makeEventList();                        // make sorted list of events
    void makeProgramHeaders();                   // make program headers and assign addresses to sections
    void specialSymbolsOverride();               // check if automatic symbols have been overridden
    int32_t findModule(uint32_t library, uint32_t memberos);// find a module from a record in symbolExports    
    void relocate();                             // put values into all cross references
    void checkRegisterUse(ElfFwcSym * sym1, ElfFwcSym * sym2, uint32_t modul);// Check if external function call has compatible register use
    ElfFwcSym * findSymbolAddress(uint64_t * a, uint32_t * targetModule, ElfFwcSym * sym, uint32_t modul); // find a symbol and its address
    uint64_t findSymbolAddress(const char * name); // find the address of a symbol from its name
    void copySections();                         // copy sections to output file
    void copySymbols();                          // copy symbols to output file
    void copyRelocations();                      // copy relocation records to output file if needed
    uint32_t resolveRelocationTarget(uint32_t modul, uint32_t symi);// resolve relocation target for executable file record
    void makeFileHeader();                       // make file header
    CELF inputFile;                              // input file if relinking
    CELF outFile;                                // output file
    CMetaBuffer<CELF> modules1;                  // object files and modules from input exe file
    CMetaBuffer<CELF> modules2;                  // same + object files from libraries
    CMetaBuffer<CLibrary> libraries;             // library files
    CDynamicArray<SSymbolEntry> symbolExports;   // list of exported symbols
    CDynamicArray<SSymbolEntry> symbolImports;   // list of imported symbols
    CDynamicArray<SLinkSection> sections;        // list of sections, sorted in memory order
    CDynamicArray<SLinkSection> communalSections;// list of communal sections
    CDynamicArray<SLinkSection2> sections2;      // list of sections, sorted by module and section index
    CDynamicArray<SLibraryModule> libmodules;    // list of imported library modules
    CDynamicArray<SReloc2> relocations2;         // extended relocation records to be copied to executable file
    CDynamicArray<SSymbolXref> symbolXref;       // symbol index cross reference
    CDynamicArray<SSymbolXref2> unresWeakSym;    // list of unresolved weak symbols
    CDynamicArray<SRelinkModule> relinkModules;  // list of modules and libraries in input file to relink
    CDynamicArray<SLCommand> rnames;             // List of modules and libraries to delete or replace
    CDynamicArray<ElfFwcEvent> eventData;        // container for all event records
    CELF memberBuffer;                           // buffer containing temporary copy of single library member
    ElfFwcEhdr fileHeader;                       // file header for executable file
    bool relinkable;                             // output file is relinkable
    bool relinking;                              // relinking existing file
};

const uint32_t fillerInstruction = 0x7FFFFFFF;   // binary representation of filler instruction

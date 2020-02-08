/****************************  linker.cpp  ***********************************
* Author:        Agner Fog
* date created:  2017-11-14
* Last modified: 2018-03-30
* Version:       1.01
* Project:       Binary tools for ForwardCom instruction set
* Description:
* This module contains the linker.
*
* Copyright 2017-2018 GNU General Public License http://www.gnu.org/licenses
*****************************************************************************/

/*     Overview of data structures used during linking process
       -------------------------------------------------------
symbolImports:    List of imported symbols that need to be resolved.
                  Includes symbol name and source module
symbolExports:    List of public symbols that can be targets for symbolImports.
                  Includes symbol name and module or library
libraries:        Library files to include in symbol search
libmodules:       List of library modules that will be extracted as object files
modules1:         Metabuffer containing all the object files to add
modules2:         Same. Also includes object files extracted from libraries
sections:         Index to sections to be extracted from object files and library modules.
                  Sorted in the order in which they should occur in the executable file
sections2:        Same as sections. Sorted by module and section index. Used for re-finding a section
communalSections: List of communal sections. Some of these will be copied to sections and 
                  sections2 when needed
symbolXref:       Cross reference between module-local symbol indexes and indexes in relinkable executable file
unresWeakSym:     List of unresolved weak symbols. Includes indexes in relinkable executable file
eventData:        List of event records

Each of the elements in modules1/2 is a complete CELF object containing its own data structures,
including sectionHeaders, symbols, stringBuffer, and relocations.

outFile is also a complete CELF object containing its own data structures, including 
programHeaders, sectionHeaders, symbols, stringBuffer, and relocations.

*/

#include "stdafx.h"

// define code of dummy function for unresolved weak externals 
// and unresolved functions of incomplete executable file:
static const uint32_t unresolvedFunctionN = 2;
static const uint32_t unresolvedFunction[unresolvedFunctionN] = {
    0x79800200,         // tiny instructions: int64 r0 = 0; double v0 = 0
//  0x78000200,         // tiny instructions: int64 r0 = 0; v0 = clear()
    0x67C00000          // instruction: return
};
static const uint32_t unresolvedReguse1 = 1;
static const uint32_t unresolvedReguse2 = 1;

// run the linker
void CLinker::go() {
    // write text on stdout
    feedBackText1();

    if (cmd.job == CMDL_JOB_RELINK) {
        // read pre-existing executable file
        loadExeFile();  
        relinkable = true;  relinking = true;
        if (err.number()) return;
    }

    // read specified object files and library files
    fillBuffers();
    if (err.number()) return;

    // make list of imported and exported symbols
    makeSymbolList();
    if (err.number()) return;

    // match lists of imported and exported symbols
    matchSymbols();
    if (err.number()) return;

    // search libraries for imported symbols
    librarySearch();
    if (err.number()) return;

    // write feedback to console
    feedBackText2();

    // check for duplicate symbols
    checkDuplicateSymbols();
    if (err.number()) return;

    // get imported library modules into modules2 buffer
    readLibraryModules();
    if (err.number()) return;

    // make list of all sections
    makeSectionList();
    if (err.number()) return;

    // make program headers and assign addresses to sections
    makeProgramHeaders();
    if (err.number()) return;

    // put values into all cross references
    relocate();
    if (err.number()) return;

    // make sorted event list
    makeEventList();

    // copy sections to output file
    copySections();

    // copy symbols to output file
    copySymbols();

    // copy relocation records to output file if needed
    copyRelocations();
    if (err.number()) return;

    // make executable file header
    makeFileHeader();

    // join sections into executable file
    outFile.join(&fileHeader);
    if (err.number()) return;

    // write output file
    outFile.write(cmd.getFilename(cmd.outputFile));
}

CLinker::CLinker() {
    // Constructor
    zeroAllMembers(fileHeader); // initialize file header
    relinking = false;
    relinkable = (cmd.fileOptions & CMDL_FILE_RELINKABLE) != 0;
    symbolNameBuffer.pushString("");  // make sure name = 0 gives empty string
}

// write feedback text on stdout
void CLinker::feedBackText1() {
    if (cmd.verbose) {  // tell what we are doing
        if (cmd.verbose > 1) printf("\nForwardCom linker v. %i.%02i", FORWARDCOM_VERSION, FORWARDCOM_SUBVERSION);
        if (cmd.job == CMDL_JOB_LINK) {
            printf("\nLinking file %s", cmd.getFilename(cmd.outputFile));
        }
        else {
            printf("\nRelinking file %s to file %s", cmd.getFilename(cmd.inputFile), cmd.getFilename(cmd.outputFile));            
        }
    }
}

// load specified object files and library files into buffers
void CLinker::fillBuffers() {
    uint32_t i;                                  // loop counter
    const char * fname;                          // file name

    // count number of modules and libraries on command line, and number of relinkable modules and libraries
    countModules();

    // allocate metabuffers
    modules1.setSize(numRelinkObjects + numObjects);
    libraries.setSize(numLibraries + numRelinkLibraries + 1); // libraries[0] is not used

    // get preserved modules if relinking
    if (cmd.job == CMDL_JOB_RELINK) getRelinkObjects();

    // read files into these buffers
    uint32_t iObject = numRelinkObjects;         // object file index
    uint32_t iLibrary = 0;                       // library file index

    if (cmd.verbose && numObjects) printf("\nAdding object files:");

    // loop through commands. get object files and libraries
    for (i = 0; i < cmd.lcommands.numEntries(); i++) {
        if ((cmd.lcommands[i].command & 0xFF) == CMDL_LINK_ADDMODULE) {
            // name of object file
            fname = cmd.getFilename(cmd.lcommands[i].filename);
            // write name
            if (cmd.verbose) printf(" %s", fname);
            // read object file
            modules1[iObject].read(fname);
            modules1[iObject].moduleName = cmd.fileNameBuffer.pushString(removePath(fname));
            modules1[iObject].library = 0;
            modules1[iObject].relinkable = (cmd.lcommands[i].command & CMDL_LINK_RELINKABLE) != 0;

            // remove colons from name
            char *nm = &cmd.fileNameBuffer.get<char>(modules1[iObject].moduleName);
            for (int s = 0; s < (int)strlen(nm); s++) {
                if (nm[s] == ':' || nm[s] <= ' ') nm[s] = '_';
            }
            if (err.number()) continue;
            // check type
            if (modules1[iObject].getFileType() != FILETYPE_FWC) {
                err.submit(ERR_LINK_FILE_TYPE, fname);
                return;
            }
            iObject++;
        }
        else if ((cmd.lcommands[i].command & 0xFF) == CMDL_LINK_ADDLIBRARY) {
            iLibrary++;
            // name of library file
            fname = cmd.getFilename(cmd.lcommands[i].filename);
            // read library file
            libraries[iLibrary].read(fname);
            libraries[iLibrary].relinkable = (cmd.lcommands[i].command & CMDL_LINK_RELINKABLE) != 0;
            libraries[iLibrary].libraryName = cmd.fileNameBuffer.pushString(removePath(fname));

            // remove colons and whitespace from name
            char *nm = &cmd.fileNameBuffer.get<char>(libraries[iLibrary].libraryName);
            for (int s = 0; s < (int)strlen(nm); s++) {
                if (nm[s] == ':' || nm[s] <= ' ') nm[s] = '_';
            }
            if (err.number()) continue;
            // check type
            uint32_t ftype = libraries[iLibrary].getFileType();
            if ((ftype != FILETYPE_LIBRARY && ftype != FILETYPE_FWC_LIB) || !libraries[iLibrary].isForwardCom()) {
                err.submit(ERR_LINK_FILE_TYPE_LIB, fname);
                return;
            }
        }
        else if ((cmd.lcommands[i].command & 0xFF) == CMDL_LINK_ADDLIBMODULE) {
            // add module explicitly from library

            // name of module
            fname = cmd.getFilename(cmd.lcommands[i].filename);

            // extract module from last library
            if (iLibrary == 0) {  // no library specified
                err.submit(ERR_LINK_MODULE_NOT_FOUND, fname, "none");
                continue;
            }
            // library name
            const char * libName = cmd.getFilename(libraries[iLibrary].libraryName);
            // find module
            uint32_t moduleOs = libraries[iLibrary].findMember(cmd.lcommands[i].filename);
            if (moduleOs == 0) { // module not found in library
                err.submit(ERR_LINK_MODULE_NOT_FOUND, fname, libName);
                continue;
            }
            // write name
            if (cmd.verbose) printf(" %s:%s", libName, fname);

            // read object file
            modules1[iObject].push(libraries[iLibrary].buf() + moduleOs + (uint32_t)sizeof(SUNIXLibraryHeader),
                libraries[iLibrary].getMemberSize(moduleOs));
            modules1[iObject].moduleName = cmd.lcommands[i].filename;
            modules1[iObject].library = iLibrary;
            modules1[iObject].relinkable = (cmd.lcommands[i].command & CMDL_LINK_RELINKABLE) != 0;
            iObject++;
        }
    }

    // get recovered libraries if relinking
    if (numRelinkLibraries) getRelinkLibraries();
}

// count number of modules and libraries to add
void CLinker::countModules() {
    uint32_t i;                                  // loop counter
    int32_t  j;                                  // loop counter
    const char * fname;                          // file name
    numObjects = 0;                              // number of object files
    numLibraries = 0;                            // number of libraries

    // count number of object files and library files on command line
    for (i = 0; i < cmd.lcommands.numEntries(); i++) {
        if ((uint8_t)cmd.lcommands[i].command == CMDL_LINK_ADDMODULE || (uint8_t)cmd.lcommands[i].command == CMDL_LINK_ADDLIBRARY) {
            // name of module
            fname = cmd.getFilename(cmd.lcommands[i].filename);
            // is it a library?
            for (j = (int32_t)strlen(fname) - 1; j > 0; j--) {
                if (fname[j] == '.') break;
            }
            if ((j > 0 && strncasecmp_(fname + j, ".li", 3) == 0 ) || (fname[j+1] == 'a' && fname[j+2] == 0)) {
                // this is a library
                numLibraries++;
                cmd.lcommands[i].command = CMDL_LINK_ADDLIBRARY | (cmd.lcommands[i].command & CMDL_LINK_RELINKABLE);
            }
            else {
                // assume that this is an object file
                numObjects++;
            }
        }
        if ((cmd.lcommands[i].command & 0xFF) == CMDL_LINK_ADDLIBMODULE) {
            // object module from library file
            numObjects++;
        }
        if (cmd.lcommands[i].command & CMDL_LINK_RELINKABLE) {
            // output file is relinkable
            relinkable = true;
        }
    }
    // count number of object files and libraries to reuse if relinking
    countReusedModules();
}

// make list of imported and exported symbols
void CLinker::makeSymbolList() {
    uint32_t modul;                              // module index
    SSymbolEntry sym;                            // symbol record
    zeroAllMembers(sym);
    unresolvedWeak = 0;           // unresolved weak imports: 1: constant, 2: readonly ip data, 4: writeable datap data, 8: function
    unresolvedWeakNum = 0;        // number of unresolved weak imports for writeable data

    // loop through modules
    for (modul = 0; modul < modules1.numEntries(); modul++) {
        if (modules1[modul].dataSize() == 0) continue;
        // get exported symbols
        modules1[modul].listSymbols(&symbolNameBuffer, &symbolExports, modul, 0, 1);
        // get imported symbols
        modules1[modul].listSymbols(&symbolNameBuffer, &symbolImports, modul, 0, 2);
    }
    // add special symbols as weak. value will be set later
    sym.name = symbolNameBuffer.pushString("__ip_base");
    sym.st_bind = STB_WEAK;
    sym.library = 0xFFFFFFFE;
    sym.st_other = SHF_IP;
    sym.symindex = 1;
    sym.member = 0;
    sym.status = 3;
    symbolExports.push(sym);
    symbolImports.push(sym);
    sym.name = symbolNameBuffer.pushString("__datap_base");
    sym.st_other = SHF_DATAP;
    sym.symindex = 2;
    symbolExports.push(sym);
    symbolImports.push(sym);
    sym.name = symbolNameBuffer.pushString("__threadp_base");
    sym.st_other = SHF_THREADP;
    sym.symindex = 3;
    symbolExports.push(sym);
    symbolImports.push(sym);
    sym.name = symbolNameBuffer.pushString("__event_table");
    sym.st_other = SHF_IP;
    sym.symindex = 4;
    symbolExports.push(sym);
    symbolImports.push(sym);
    sym.name = symbolNameBuffer.pushString("__event_table_num");
    sym.st_other = 0;
    sym.symindex = 5;
    symbolExports.push(sym);
    symbolImports.push(sym);
    // make import symbol __entry_point
    sym.name = symbolNameBuffer.pushString("__entry_point");
    sym.st_other = 0;
    sym.symindex = 6;
    sym.status = 0;
    sym.st_bind = STB_GLOBAL;
    symbolImports.push(sym);
    // sort symbols by name for easy search
    symbolExports.sort();
#if 0  // debug: list exported symbols
    for (uint32_t s = 0; s < symbolExports.numEntries(); s++) {
        printf("\n>%s", symbolNameBuffer.buf() + symbolExports[s].name);
    }
#endif
}

// match lists of imported and exported symbols
void CLinker::matchSymbols() {
    uint32_t sym;                               // symbol index
    int32_t found;
    for (sym = 0; sym < symbolImports.numEntries(); sym++) {
        // imported symbol name
        if (!(symbolImports[sym].status & 2)) {
            // symbol name not already resolved
            // search for this name in list of exported symbols
            SSymbolEntry sym1 = symbolImports[sym];
            sym1.st_bind = STB_IGNORE;    // ignore weak/strong difference
            found = symbolExports.findFirst(sym1);
            if (found >= 0) symbolImports[sym].status |= 2; // symbol has been matched
        }
    }
}

// search libraries for imported symbols
void CLinker::librarySearch() {
    bool newImports = true;                      // new modules have additional imports to resolve
    uint32_t sym;                                // symbol index
    uint32_t lib;                                // library index
    uint32_t m;                                  // module index
    const char * symname = 0;                    // name of symbol to find
    uint32_t moduleOs;                           // offset to module in library
    SLibraryModule modul;                        // identifyer of library module to add
    // repeat search as long as new modules have additional imports to resolve
    while (newImports) {
        // loop through symbols
        for (sym = 0; sym < symbolImports.numEntries(); sym++) {
            if ((symbolImports[sym].status & 6) == 0 && !(symbolImports[sym].st_bind & STB_WEAK)) {
                // symbol name
                symname = symbolNameBuffer.getString(symbolImports[sym].name);
                // symbol is unresolved and not weak. search for it in all libraries
                for (lib = 1; lib < libraries.numEntries(); lib++) {
                    moduleOs = libraries[lib].findSymbol(symname);
                    if (moduleOs) {
                        // symbol found. add module to list if it is not already there
                        symbolImports[sym].status = 2;
                        modul.library = lib;
                        modul.offset = moduleOs;
                        libmodules.addUnique(modul);
                        break;
                    }
                }
                if (lib == libraries.numEntries()) {
                    // strong symbol not found. make error message
                    // get module name
                    const char * moduleName = "[fixed]";
                    uint32_t modul = symbolImports[sym].member;
                    if (modul > 0 && modul < modules1.numEntries()) {
                        uint32_t mn = modules1[modul].moduleName;
                        moduleName = cmd.getFilename(mn);
                    }
                    symbolImports[sym].status |= 4;  // avoid reporting same unresolved symbol more than once
                    symbolImports[sym].st_bind = STB_UNRESOLVED;
                    fileHeader.e_flags |= EF_INCOMPLETE;  // file is incomplete when there are unresolved symbols
                    if (cmd.fileOptions & CMDL_FILE_INCOMPLETE) { //incomplete file allowed. warn only
                        err.submit(ERR_LINK_UNRESOLVED_WARN, symname, moduleName);
                    }
                    else {  //incomplete file not allowed. fatal error
                        err.submit(ERR_LINK_UNRESOLVED, symname, moduleName);
                    }
                }
            }
        }

        // loop through new library modules
        newImports = false;
        for (m = 0; m < libmodules.numEntries(); m++) {
            if (!(libmodules[m].library & 0x80000000)) {
                // this module has not been added before                
                libmodules[m].library |= 0x80000000;
                // library and offset
                lib = libmodules[m].library & 0x7FFFFFFF;
                moduleOs = libmodules[m].offset;
                // put member into buffer in order to extract symbols
                memberBuffer.setSize(0);
                memberBuffer.push(libraries[lib].buf() + moduleOs + (uint32_t)sizeof(SUNIXLibraryHeader),
                    libraries[lib].getMemberSize(moduleOs));
                // check if this is a ForwardCom object file
                int fileType = memberBuffer.getFileType();
                if (fileType != FILETYPE_FWC) {
                    err.submit(ERR_LIBRARY_MEMBER_TYPE,
                        libraries[lib].getMemberName(moduleOs),
                        CFileBuffer::getFileFormatName(fileType));
                    return;
                }
                memberBuffer.relinkable = libraries[lib].relinkable;
                // get names of exported symbols from ELF file            
                memberBuffer.listSymbols(&symbolNameBuffer, &symbolExports, moduleOs, lib, 1);
                uint32_t numImports = symbolImports.numEntries();
                // get names of imported symbols from ELF file            
                memberBuffer.listSymbols(&symbolNameBuffer, &symbolImports, moduleOs, lib, 2);
                if (symbolImports.numEntries() > numImports) {
                    // this library module has new imports to resolve
                    newImports = true;
                }
            }
        }
        if (err.number()) return;
        // new symbols have been added. sort list again
        symbolExports.sort();
        // match all new symbol exports to imports
        matchSymbols();
    }
    // search for unresolved weak imports
    for (sym = 0; sym < symbolImports.numEntries(); sym++) {
        if ((symbolImports[sym].status & 3) == 0 && (symbolImports[sym].st_bind & STB_WEAK)) {
            // weak symbol not resolved. make a zero dummy for it
            symbolImports[sym].status |= 1;  // avoid counting same unresolved symbol more than once
            // unresolved weak imports: 
            // 1: constant, 2: readonly ip data, 4: writeable datap data, 
            // 8: threadp, 0x10: function
            switch (symbolImports[sym].st_other & (SHF_BASEPOINTER | STV_EXEC)) {
            case 0:                   // constant
                unresolvedWeak |= 1;  break;
            case STV_IP:
                unresolvedWeak |= 2;  break;
            case STV_DATAP:
                unresolvedWeak |= 4;  unresolvedWeakNum++;
                break;
            case STV_THREADP:
                unresolvedWeak |= 8;  break;
            case STV_IP | STV_EXEC:
                unresolvedWeak |= 0x10;  break;
            }
        }
    }
    // remove check bit
    for (m = 0; m < libmodules.numEntries(); m++) {
        libmodules[m].library &= 0x7FFFFFFF;
    }
    symbolImports.sort();
}

// check for duplicate public symbols, except weak symbols
void CLinker::checkDuplicateSymbols() {
    uint32_t sym1, sym2;                         // index into symbolExports
    uint32_t text;                               // index to text in cmd.fileNameBuffer
    const char * name1, * name2;                 // library and module names
    for (sym1 = 0; sym1 < symbolExports.numEntries(); sym1++) {
        if (!(symbolExports[sym1].st_bind & STB_WEAK)) {
            sym2 = sym1 + 1;
            while (sym2 < symbolExports.numEntries() && symbolExports[sym2] == symbolExports[sym1]) {
                // symbol 2 has same name
                if (!(symbolExports[sym2].st_bind & STB_WEAK)) {
                    // name clash. make complete list of modules containing this symbol name
                    text = cmd.fileNameBuffer.dataSize();
                    uint32_t num = symbolExports.findAll(0, symbolExports[sym1]);
                    for (sym2 = sym1; sym2 < sym1 + num; sym2++) {
                        if (!(symbolExports[sym2].st_bind & STB_WEAK)) {
                            if (sym2 != sym1) {
                                cmd.fileNameBuffer.push(", ", 2);  // insert comma, except before first name
                            }
                            if (symbolExports[sym2].library) {
                                // symbol is in a library. get library name
                                uint32_t lib = symbolExports[sym2].library; // library number
                                name1 = cmd.getFilename(libraries[lib].libraryName);
                                cmd.fileNameBuffer.push(name1, (uint32_t)strlen(name1));
                                cmd.fileNameBuffer.push(":", 1);
                                // get module name
                                name2 = libraries[lib].getMemberName(symbolExports[sym2].member);
                                cmd.fileNameBuffer.push(name2, (uint32_t)strlen(name2));
                            }
                            else {
                                // object module. get name
                                uint32_t m = symbolExports[sym2].member;
                                if (m < modules2.numEntries()) {
                                    name2 = cmd.getFilename(modules2[m].moduleName);
                                    cmd.fileNameBuffer.push(name2, (uint32_t)strlen(name2));
                                }
                                else if (m < modules1.numEntries()) {
                                    name2 = cmd.getFilename(modules1[m].moduleName);
                                    cmd.fileNameBuffer.push(name2, (uint32_t)strlen(name2));
                                }
                            }
                        }
                    }
                    const char * symname = symbolNameBuffer.getString(symbolExports[sym1].name);
                    err.submit(ERR_LINK_DUPLICATE_SYMBOL, symname, cmd.getFilename(text));
                    // we are finished with this symbol name
                    sym1 += num - 1;             // skip the rest in the for loop
                    break;                       // skip while sym2 loop
                }
                sym2++;                          // while sym2
            }
        }
    }
}


// get imported library modules into modules2 buffer
void CLinker::readLibraryModules() {
    uint32_t m1;                                 // object file index
    uint32_t m2;                                 // library module index
    uint32_t lib;                                // library index
    uint32_t moduleOs;                           // offset to library module

    // modules1 contains object files, libmodules contains index to library modules.
    // we want to join these into the same buffer named modules2.
    // The total number of object files and library modules is
    uint32_t numModules = modules1.numEntries() + libmodules.numEntries();
    // we cannot change the size of a metabuffer, so we will make a new 
    // bigger metabuffer and transfer everything from modules1 to modules2:
    modules2.setSize(numModules);
    for (m1 = 0; m1 < modules1.numEntries(); m1++) {
        modules2[m1] << modules1[m1];
    }
    // now get the library modules
    for (m2 = 0; m2 < libmodules.numEntries(); m2++) {
        // library and offset
        lib = libmodules[m2].library & 0x7FFFFFFF;
        moduleOs = libmodules[m2].offset;
        // put member into its own buffer
        modules2[m1+m2].push(libraries[lib].buf() + moduleOs + (uint32_t)sizeof(SUNIXLibraryHeader),
            libraries[lib].getMemberSize(moduleOs));
        modules2[m1+m2].moduleName = cmd.fileNameBuffer.pushString(libraries[lib].getMemberName(moduleOs));
        modules2[m1+m2].library = lib;
        modules2[m1+m2].relinkable = libraries[lib].relinkable;
        
        // put new module index into libmodules record
        libmodules[m2].modul = m1 + m2;
    }
}

// make list of all sections
void CLinker::makeSectionList() {
    uint32_t m;                                  // module index
    uint32_t sh;                                 // section header index
    uint32_t sh_type;                            // section type
    uint32_t secStringTableLen = 0;              // length of section string table
    const char * secStringTable = 0;             // section string table in ELF module
    const char * secName = 0;                    // section name
    SLinkSection section;                        // section record
    zeroAllMembers(section);                     // initialize
    eventDataSize = 0;                           // total size of all event data sections
    sections.push(section);

    // loop through all modules to get all sections
    for (m = 0; m < modules2.numEntries(); m++) {
        if (modules2[m].dataSize() == 0) continue;
        modules2[m].split();                     // split module into components
        secStringTable = (char*)modules2[m].stringBuffer.buf();
        secStringTableLen = modules2[m].stringBuffer.dataSize();
        for (sh = 0; sh < modules2[m].sectionHeaders.numEntries(); sh++) {
            sh_type = modules2[m].sectionHeaders[sh].sh_type;
            if (sh_type & (SHT_ALLOCATED | SHT_LIST)) {
                section.sh_type = sh_type;
                section.sh_flags = modules2[m].sectionHeaders[sh].sh_flags;
                section.sh_size = modules2[m].sectionHeaders[sh].sh_size;
                section.sh_align = modules2[m].sectionHeaders[sh].sh_align;
                uint32_t namei = modules2[m].sectionHeaders[sh].sh_name;
                if (namei >= secStringTableLen) secName = "?";
                else secName = secStringTable + namei;
                section.name = cmd.fileNameBuffer.pushString(secName);
                section.sh_module = m;
                section.sectioni = sh;
                if (modules2[m].relinkable) section.sh_flags |= SHF_RELINK;
                if (section.sh_flags & SHF_EVENT_HND) {
                    // check event data sections
                    eventDataSize += (uint32_t)section.sh_size;
                    // unsorted lists are preserved in executable file but not loaded into memory:
                    section.sh_type = SHT_LIST;  
                }
                if (sh_type == SHT_COMDAT) {
                    communalSections.push(section);  // communal section. sections with same name joined
                }
                else {
                    sections.push(section);          // normal code, data, or bss section
                }
            }
        }
    }
    // join communal sections with same name and add them to the sections list
    joinCommunalSections();

    // make dummy sections for unresolved weak external symbols
    makeDummySections();

    // sort the two section lists by the order in which it should occur in the executable
    sortSections();

    // add final index
    for (uint32_t ix = 0; ix < sections.numEntries(); ix++) {
        sections[ix].sectionx = ix + 1;
    }
    // copy the list
    sections2.copy(sections);
    // 'sections2' is sorted by module and section index for the purpose of finding back to the original
    sections2.sort();
}

// sort sections in the order in which they should occur in the executable file
void CLinker::sortSections() {
    uint32_t s;                                  // section index
    uint32_t order;                              // section sort order
    uint32_t flags;                              // section flags
    uint32_t type;                               // section type

    /* The order is as listed below. 
       The base pointers are set to the limits where order changes from even to odd.
                  SHF_ALLOC:
    0x02000002       SHT_ALLOCATED:
    0x02000002          SHF_IP:
    0x02101002             SHF_EVENT_HND
    0x02202002             SHF_EXCEPTION_HND
    0x02303002             SHF_DEBUG_INFO
    0x02404002             SHF_COMMENT
    0x02500002             SHF_WRITE
    0x02600002             SHF_READ only !SHF_WRITE !SHF_EXEC  (const)
    0x02601002                SHF_AUTOGEN
    0x02602002                SHF_RELINK
    0x02603002                !SHF_RELINK !SHF_FIXED
    0x02604002                SHF_FIXED
                           SHF_EXEC  (code)  (set ip_base)
    0x02701003                SHF_FIXED !SHF_RELINK
    0x02702003                !SHF_RELINK
    0x02703003                SHF_RELINK
    0x02704003                SHF_AUTOGEN
    0x02800004          SHF_DATAP
                           SHT_PROGBITS  (data)
    0x02801004                SHF_RELINK                                 
    0x02802004                !SHF_FIXED
    0x02803004                SHF_FIXED
                           SHT_NOBITS   (bss)  (set datap_base)
    0x02806005                SHF_FIXED
    0x02807005                !SHF_RELINK
    0x02808005                SHF_RELINK
    0x02809005                SHF_AUTOGEN
    0x02A00006          SHF_THREADP
                           SHT_PROGBITS  (data)
    0x02A01006                SHF_RELINK                                 
    0x02A02006                !SHF_FIXED
    0x02A03006                SHF_FIXED
                           SHT_NOBITS   (bss)  (set threadp_base)
    0x02A06007                SHF_FIXED
    0x02A07007                !SHF_RELINK
    0x02A08007                SHF_RELINK
    0x08000000       !SHT_ALLOCATED:
    0x08100000    !SHF_ALLOC:
    0x08110000       SHT_RELA
    0x08120000       SHT_SYMTAB
    0x08130000       SHT_STRTAB
    0x08160000       other
    */

    for (s = 0; s < sections.numEntries(); s++) {
        flags = sections[s].sh_flags;
        type  = sections[s].sh_type;
        if (flags & SHF_ALLOC) {
            if (type & SHT_ALLOCATED) {
                order = 0x02000000;
                if (flags & SHF_IP) {
                    order = 0x02000002;
                    if (flags & SHF_EVENT_HND) order = 0x02101002;
                    else if (flags & SHF_EXCEPTION_HND) order = 0x02202002;
                    else if (flags & SHF_DEBUG_INFO) order = 0x02303002;
                    else if (flags & SHF_COMMENT) order = 0x02404002;
                    else if (flags & SHF_WRITE) order = 0x02500002;
                    else if ((flags & SHF_READ) && !(flags & SHF_EXEC)) {
                        order = 0x02600002;
                        if (flags & SHF_AUTOGEN) order = 0x02601002;
                        else if (flags & SHF_RELINK) order = 0x02602002;
                        else if (!(flags & SHF_FIXED)) order = 0x02603002;
                        else order = 0x02604002;
                    }
                    else if (flags & SHF_EXEC) {
                        if (!(flags & SHF_AUTOGEN)) {
                            if ((flags & SHF_FIXED) || !(flags & SHF_RELINK)) order = 0x02701003;
                            else if (!(flags & SHF_RELINK)) order = 0x02702003;
                            else order = 0x02703003;
                        }
                        else {
                            order = 0x02704003; // SHF_AUTOGEN
                        }
                    }
                }
                else if (flags & (SHF_DATAP | SHF_THREADP)) {
                    order = 0x02800004;
                    if (flags & SHF_THREADP) order = 0x02A00006;
                    if (type != SHT_NOBITS) {
                        if (flags & SHF_RELINK) order |= 0x1000;
                        else if (!(flags & SHF_FIXED)) order |= 0x2000;
                        else  order |= 0x3000;
                    }
                    else {  // SHT_NOBITS
                        order |= 1;
                        if (!(flags & SHF_AUTOGEN)) {
                            if (flags & SHF_FIXED) order |= 0x6000;
                            else if (!(flags & SHF_RELINK)) order |= 0x7000;
                            else  order |= 0x8000;
                        }
                        else {  // SHF_AUTOGEN 
                            order |= 0x9000;
                        }
                    }
                }
            }
            else { // !SHT_ALLOCATED
                order = 0x08000000;
            }
        }
        else {  // !SHF_ALLOC
            switch (type) {
            case SHT_RELA:
                order = 0x08110000;  break;
            case SHT_SYMTAB:
                order = 0x08120000;  break;
            case SHT_STRTAB:
                order = 0x08130000;  break;
            default:
                order = 0x08160000;  break;
            }
        }
        sections[s].order = order;
    }
    sections.sort();

#if 0  // debug: list sections
    for (s = 0; s < sections.numEntries(); s++) {
        printf("\n* %8X  %s", sections[s].order, cmd.getFilename(sections[s].name));
    }
#endif
}

// join communal sections with same name
void CLinker::joinCommunalSections() {
    uint32_t m;                                  // module index
    uint32_t s1 = 0, s2, s3, s4;                 // index into communalSections
    uint32_t sym;                                // symbol index in module
    uint32_t rel;                                // relocation index in module
    const char * comname;                        // name of communal section
    bool symbolsRemoved = false;                 // symbols in removed communal sections

    communalSections.sort();
    while (s1 < communalSections.numEntries()) {
        comname = cmd.getFilename(communalSections[s1].name);
        // find last entry with same name
        s4 = s2 = s1;
        while (s2 + 1 < communalSections.numEntries()
            && strcmp(comname, cmd.getFilename(communalSections[s2+1].name)) == 0) {
            s2++;
        }

        // check that communal sections with same name have same size
        bool differentSize = false;
        for (s3 = s1+1; s3 <= s2; s3++) {
            // a non-linkable communal section takes precedence
            if (!(communalSections[s3].sh_flags & SHF_RELINK) && (communalSections[s4].sh_flags & SHF_RELINK)) {
                s4 = s3;
            }
            else if (communalSections[s3].sh_size != communalSections[s1].sh_size) {
                differentSize = true;
                // find the biggest
                if (communalSections[s3].sh_size > communalSections[s4].sh_size) s4 = s3;
            }
        }
        if (differentSize) {
            // make error message
            CMemoryBuffer joinNames;                     // join section names for error message
            joinNames.setSize(0);
            m = communalSections[s1].sh_module;
            const char * mname = cmd.getFilename(modules2[m].moduleName);            
            joinNames.push(mname, (uint32_t)strlen(mname));
            for (s3 = s1 + 1; s3 <= s2; s3++) {
                m = communalSections[s3].sh_module;
                mname = cmd.getFilename(modules2[m].moduleName);
                joinNames.push(", ", 2);
                joinNames.push(mname, (uint32_t)strlen(mname));
            }
            err.submit(ERR_LINK_COMMUNAL, comname, (char*)joinNames.buf());
        }
        // check if there is any reference to this section. if not, purge it, except when debug level 2
        bool keepSection = true;
        if (cmd.debugOptions < 2) {
            keepSection = false;
            m = communalSections[s4].sh_module;
            CELF * modul = &modules2[m];
            // find symbols in this section
            for (sym = 0; sym < modul->symbols.numEntries(); sym++) {
                if (modul->symbols[sym].st_section == communalSections[s4].sectioni) {
                    const char * symname = (char*)modul->stringBuffer.buf() + modul->symbols[sym].st_name;
                    // search for this symbol name in symbolImports
                    SSymbolEntry symsearch;
                    symsearch.name = symbolNameBuffer.pushString(symname);
                    symsearch.st_bind = STB_IGNORE;
                    int32_t s = symbolImports.findFirst(symsearch);
                    if (s >= 0) {
                        keepSection = true;      // there is a reference to this section. keep it
                        if (!(communalSections[s4].sh_flags & SHF_RELINK)) {
                            // communal section is not relinkable. Make the symbol non-weak
                            if (modul->symbols[sym].st_bind & STB_WEAK) {
                                modul->symbols[sym].st_bind = STB_GLOBAL;
                            }
                        }
                        break;
                    }
                }
            }
        }
        if (keepSection) {
            // save one instance of the communal section
            sections.push(communalSections[s4]);
        }
        // remove symbols and relocations from removed sections
        for (s3 = s1; s3 <= s2; s3++) {
            if (s3 != s4 || !keepSection) {
                // this section is removed
                m = communalSections[s3].sh_module;
                CELF * modul = &modules2[m];
                for (sym = 0; sym < modul->symbols.numEntries(); sym++) {
                    if (modul->symbols[sym].st_section == communalSections[s3].sectioni) {
                        const char * symname = (char*)modul->stringBuffer.buf() + modul->symbols[sym].st_name;
                        // search for this symbol name in symbolExports
                        SSymbolEntry symsearch;
                        symsearch.name = symbolNameBuffer.pushString(symname);
                        symsearch.st_bind = STB_IGNORE;
                        uint32_t firstMatch = 0;
                        uint32_t n = symbolExports.findAll(&firstMatch, symsearch);
                        // search through all symbols with this name
                        for (uint32_t i = firstMatch; i < firstMatch + n; i++) {
                            if (symbolExports[i].library == 0) {
                                if (symbolExports[i].member == m 
                                && symbolExports[i].sectioni == communalSections[s3].sectioni) {
                                    // removed symbol found
                                    symbolExports[i].name = 0;
                                    symbolExports[i].st_bind = 0;
                                    symbolsRemoved = true;
                                    break;
                                }
                            }
                            else {
                                uint32_t m2 = findModule(symbolExports[i].library, symbolExports[i].member);
                                if (m2 == m && symbolExports[i].sectioni == communalSections[s4].sectioni) {
                                    symbolExports[i].library = 0;
                                    symbolExports[i].name = 0;
                                    symbolExports[i].st_bind = 0;
                                    symbolsRemoved = true;
                                    break;
                                }
                            }
                        }
                    }
                }
                // search for relocations in removed section
                for (rel = 0; rel < modul->relocations.numEntries(); rel++) {
                    if (modul->relocations[rel].r_section == communalSections[s3].sectioni) {
                        modul->relocations[rel].r_type = 0;
                    }
                }
            }
        }
        // continue with next communal name
        s1 = s2 + 1;
    }
    if (symbolsRemoved) {
        // entries have been removed from symbolExports. sort it again
        symbolExports.sort();
    }
}

// make dummy segments for event handler table and for unresolved weak externals
void CLinker::makeDummySections() {
    SLinkSection section;
    zeroAllMembers(section);
    section.sh_type = SHT_PROGBITS;
    section.sh_align = 3;

    if (eventDataSize) {
        section.sh_size = eventDataSize;
        section.sh_flags = SHF_READ | SHF_IP | SHF_ALLOC | SHF_EVENT_HND | SHF_RELINK | SHF_AUTOGEN;
        section.name = cmd.fileNameBuffer.pushString("eventhandlers_sorted");
        section.sh_module = 0xFFFFFFF8;
        sections.push(section);
    }

    // unresolved weak imports indicated by unresolvedWeak: 
    // 1: constant, 2: readonly ip data, 4: writeable datap data, 
    // 8: threadp, 0x10: function
    if (unresolvedWeak & 2) {
        section.sh_size = 8;
        section.sh_flags = SHF_READ | SHF_IP | SHF_ALLOC | SHF_RELINK | SHF_AUTOGEN;
        section.name = cmd.fileNameBuffer.pushString("zdummyconst");
        section.sh_module = 0xFFFFFFF1;
        sections.push(section);
    }
    if (unresolvedWeak & 4) {
        section.sh_size = 8 * unresolvedWeakNum;
        section.sh_flags = SHF_READ | SHF_WRITE | SHF_DATAP | SHF_ALLOC | SHF_RELINK | SHF_AUTOGEN;
        section.name = cmd.fileNameBuffer.pushString("zdummydata");
        section.sh_module = 0xFFFFFFF2;
        sections.push(section);
    }
    if (unresolvedWeak & 8) {
        section.sh_size = 8;
        section.sh_flags = SHF_READ | SHF_WRITE | SHF_THREADP | SHF_ALLOC | SHF_RELINK | SHF_AUTOGEN;
        section.name = cmd.fileNameBuffer.pushString("zdummythreaddata");
        section.sh_module = 0xFFFFFFF3;
        sections.push(section);
    }
    if (unresolvedWeak & 0x10) {
        section.sh_size = 8;
        section.sh_flags = SHF_EXEC | SHF_IP | SHF_ALLOC | SHF_RELINK | SHF_AUTOGEN;
        section.name = cmd.fileNameBuffer.pushString("zdummyfunc");
        section.sh_module = 0xFFFFFFF4;
        sections.push(section);
    }
}

// make sorted list of events
void CLinker::makeEventList() {
    uint32_t sec;                                // section

    // find event handler sections
    for (sec = 0; sec < sections.numEntries(); sec++) {
        if (sections[sec].sh_flags & SHF_EVENT_HND) {
            uint32_t m = sections[sec].sh_module;
            if (m < modules2.numEntries()) {
                CELF * modul = &modules2[sections[sec].sh_module]; // find module
                uint32_t offset = uint32_t(modul->sectionHeaders[sections[sec].sectioni].sh_offset);
                uint32_t size = uint32_t(modul->sectionHeaders[sections[sec].sectioni].sh_size);
                if (size & (sizeof(ElfFwcEvent)-1)) {
                    // event section size not divisible by event record size
                    err.submit(ERR_EVENT_SIZE, cmd.getFilename(modul->moduleName));
                    return;
                }
                // copy all event records
                for (uint32_t index = 0; index < size; index += sizeof(ElfFwcEvent)) {                
                    eventData.push(modul->dataBuffer.get<ElfFwcEvent>(offset + index));
                }
            }
        }
    }
    // sort event list
    eventData.sort();
}


// make program headers and assign addresses to sections
void CLinker::makeProgramHeaders() {
    // Each program header can cover multiple sections with the same base pointer and
    // the same read/write/execute permissions
    uint32_t sec;                      // section index
    uint32_t ph;                       // program header index
    uint32_t lastFlags = 0;            // p_flags of last program header
    uint64_t offset = 0;               // address relative to begin of section group
    uint64_t * pBasePonter = 0;        // pointer to base pointer
    uint32_t secOrder;                 // indicates 'order' as defined in sortSections()
                                       // secOrder & 0xF00000 indicates program header
                                       // secOrder & 0x0E indicates base pointer
                                       // Even values may have negative index relative to the base pointer,
                                       // odd values have positive index relative to the base pointer
    uint32_t lastSecOrder = 0;         // secOrder of previous section
    uint64_t align;                    // section alignment
    uint8_t  maxAlign = 0;             // maximum alignment of all sections in group = (1 << maxAlign)
    bool basePointerAssigned = false;  // a base pointer has been assigned for this group
    ElfFwcPhdr pHeader;               // program header = segment definition
    zeroAllMembers(pHeader);           // initialize

    // initialize pointer bases. may change later
    ip_base = datap_base = threadp_base = 0; 
    event_table = event_table_num = 0;

    // loop through sections to assign sections to program headers, and 
    // find the maximum alignment for each program header
    for (sec = 0; sec < sections.numEntries(); sec++) {
        // section order as defined by sortSections()
        secOrder = sections[sec].order;
        if (secOrder == 0 || !(sections[sec].sh_type & SHT_ALLOCATED)) {
            // relocation tables, symbol tables, string tables, etc. need no program header.
            // set address to zero
            sections[sec].sh_addr = 0;
            uint32_t mod = sections[sec].sh_module;
            uint32_t seci = sections[sec].sectioni;
            if (mod < modules2.numEntries() && seci < modules2[mod].sectionHeaders.numEntries()) {
                // find section header
                ElfFwcShdr & sectionHeader = modules2[mod].sectionHeaders[seci];
                sectionHeader.sh_addr = 0;
            } 
            continue;  // don't put in program header
        }
        if ((secOrder & 0xF00000) != (lastSecOrder & 0xF00000)) {
            // new program header. save last program header
            if (pHeader.p_type != 0) {
                // finished with previous section group
                // check if alignment needs to be increased
                if (maxAlign > pHeader.p_align) {
                    pHeader.p_align = maxAlign;
                }
                outFile.programHeaders.push(pHeader);
            }
            // start making new program header
            zeroAllMembers(pHeader);
            pHeader.p_type = PT_LOAD;
            pHeader.p_flags = sections[sec].sh_flags;
            maxAlign = sections[sec].sh_align;
            if (secOrder >> 1 != lastSecOrder >> 1) {
                // new pointer base. reset maxAlign. must align by at least 1 << MEMORY_MAP_ALIGN
                maxAlign = MEMORY_MAP_ALIGN;
            }
            else if ((sections[sec].sh_flags ^ lastFlags) & SHF_PERMISSIONS) {
                // different permissions. must align by at least 1 << MEMORY_MAP_ALIGN
                if (maxAlign < MEMORY_MAP_ALIGN) maxAlign = MEMORY_MAP_ALIGN;
            }
            // use low 32 bits of p_paddr to store index into sections and 
            // high 32 bits to store number of sections
            pHeader.p_paddr = sec;
        }
        lastSecOrder = secOrder;
        lastFlags = sections[sec].sh_flags;
        // find the section with the highest alignment
        if (maxAlign < sections[sec].sh_align) maxAlign = sections[sec].sh_align;
        // count sections covered by this header
        pHeader.p_paddr += (uint64_t)1 << 32;   
    }
    // finish last program header
    if (pHeader.p_type != 0) {
        // check if alignment needs to be increased
        if (maxAlign > pHeader.p_align) {
            pHeader.p_align = maxAlign;
        }
        // save last program header
        outFile.programHeaders.push(pHeader);
    }

    // loop through sections covered by each program header and assign addresses
    lastFlags = 0;  offset = 0;
    for (ph = 0; ph < outFile.programHeaders.numEntries(); ph++) {
        ElfFwcPhdr & rHeader = outFile.programHeaders[ph];  // reference to current program header
        uint32_t fistSection = (uint32_t)rHeader.p_paddr;
        uint32_t numSections = (uint32_t)(rHeader.p_paddr >> 32);

        if ((rHeader.p_flags ^ lastFlags) & SHF_BASEPOINTER) {
            // base pointer is different from last header. restart addressing
            offset = 0;  basePointerAssigned = false;
            // get base pointer
            switch (rHeader.p_flags & SHF_BASEPOINTER) {
            case SHF_IP: // ip
                pBasePonter = &ip_base;
                break;
            case SHF_DATAP: // datap
                pBasePonter = &datap_base;
                break;
            case SHF_THREADP: // threadp
                pBasePonter = &threadp_base;
                break;
            default:
                pBasePonter = 0;
            }
        }
        // align start of segment
        align = (uint64_t)1 << rHeader.p_align;
        offset = (offset + align - 1) & -(int64_t)align;
        rHeader.p_vaddr = offset;

        // find event_table
        if ((outFile.programHeaders[ph].p_flags & SHF_EVENT_HND) && !(lastFlags & SHF_EVENT_HND)) {
            event_table = (uint32_t)offset;
            event_table_num = uint32_t(sections[fistSection].sh_size / sizeof(ElfFwcEvent));
        }

        // loop through sections covered by this program header
        for (sec = fistSection; sec < fistSection + numSections; sec++) {
            // get section start address
            if (relinking 
                && (sections[sec].sh_flags & SHF_FIXED)
                && basePointerAssigned) {
                // this section belongs to the non-relinkable part of a relinkable file.
                // the address must be the same as in the input file, relative to the base pointer
                uint64_t offset2 = sections[sec].sh_addr + *pBasePonter;
                if (offset2 - offset > MAX_ALIGN) {
                    err.submit(ERR_INDEX_OUT_OF_RANGE);
                    return;
                }
                offset = offset2;
            }
            else {
                // align start of section
                align = (uint64_t)1 << sections[sec].sh_align;
                offset = (offset + align - 1) & -(int64_t)align;
            }
            // find base pointer
            if (!basePointerAssigned && pBasePonter) {
                if (relinking && (sections[sec].sh_flags & SHF_FIXED)) {
                    // this section is the first in a the non-relinkable part of a relinkable file.
                    // Place base pointer at the same position relative to this section as in the original
                    *pBasePonter = offset - sections[sec].sh_addr;
                    basePointerAssigned = true;
                    if (int64_t(*pBasePonter) < 0) {
                        err.submit(ERR_INDEX_OUT_OF_RANGE);
                        return;
                    }
                }
                else if (sections[sec].order & 1) {
                    // changing from const to executable or from data to bss. place base pointer here
                    offset = (offset + MEMORY_MAP_ALIGN - 1) & int64_t(-MEMORY_MAP_ALIGN);
                    *pBasePonter = offset;
                    basePointerAssigned = true;
                }
                else if (sec + 1 >= sections.numEntries() //fistSection + numSections
                || uint8_t(sections[sec+1].order) >> 1 != uint8_t(sections[sec].order) >> 1) {
                    // last section with this base pointer. place base pointer here
                    // (alternatively, place base pointer at the end of this section)
                    offset = (offset + MEMORY_MAP_ALIGN - 1) & int64_t(-MEMORY_MAP_ALIGN);
                    *pBasePonter = offset;
                    basePointerAssigned = true;
                }
            }
            // save address
            sections[sec].sh_addr = offset;

            if (sections[sec].sh_module < 0xFFFFFFF0) {
                // find section header
                ElfFwcShdr & sectionHeader = modules2[sections[sec].sh_module].sectionHeaders[sections[sec].sectioni];
                sectionHeader.sh_addr = offset;
                offset += sectionHeader.sh_size;
            }
            else {
                // dummy section for unresolved weak externals
                switch (sections[sec].sh_module) {
                case 0xFFFFFFF1:
                    dummyConst = (uint32_t)offset;  break;
                case 0xFFFFFFF2:
                    dummyData = (uint32_t)offset;  break;
                case 0xFFFFFFF3:
                    dummyThreadData = (uint32_t)offset;  break;
                case 0xFFFFFFF4:
                    dummyFunc = (uint32_t)offset;  break;
                }
                offset += sections[sec].sh_size;
            }

            if ((rHeader.p_flags & SHF_READ) && ph+1 < outFile.programHeaders.numEntries()
                && !(outFile.programHeaders[ph+1].p_flags & SHF_READ)
                && rHeader.p_memsz <= rHeader.p_filesz) {
                // readable section followed by non-readable section. Add empty space
                offset += DATA_EXTRA_SPACE;
            }
            // update program header
            rHeader.p_memsz = offset - rHeader.p_vaddr;
            if (sections[sec].sh_type != SHT_NOBITS) rHeader.p_filesz = rHeader.p_memsz;
        }
        lastFlags = rHeader.p_flags;
    }

    // check if special symbols have been overridden
    specialSymbolsOverride();
}


// check if automatic symbols have been overridden
void CLinker::specialSymbolsOverride() {
    uint64_t addr;
    bool basePointerChanged = false;
    addr = findSymbolAddress("__ip_base");
    if ((int64_t)addr >= 0) {
        if (ip_base != addr) basePointerChanged = true;
        ip_base = addr;
    }
    addr = findSymbolAddress("__datap_base");
    if ((int64_t)addr >= 0) {
        if (datap_base != addr) basePointerChanged = true;
        datap_base = addr;
    }
    addr = findSymbolAddress("__threadp_base");
    if ((int64_t)addr >= 0) {
        if (threadp_base != addr) basePointerChanged = true;
        threadp_base = addr;
    }
    if (relinking && basePointerChanged && modules2[0].sectionHeaders.numEntries()) {
        // base pointer has been changed during relinking and there are fixed sections that 
        // may contain addresses relative to the old value of the base pointers
        err.submit(ERR_RELINK_BASE_POINTER_MOD);
    }

    // find entry point
    addr = findSymbolAddress("__entry_point");
    if ((int64_t)addr >= 0) entry_point = addr;
    else entry_point = ip_base;
}

// find a module from a record in symbolExports.
// the return value is an index into modules2
int32_t CLinker::findModule(uint32_t library, uint32_t memberos) {
    if (library == 0) return memberos;           // module not in a library
    if (library == 0xFFFFFFFE) return -2;        // special symbol, not in any module
    SLibraryModule modu;                         // module is in a library
    modu.library = library;
    modu.offset = memberos;
    int32_t i = libmodules.findFirst(modu);
    if (i >= 0) return libmodules[i].modul;
    return -1;
}


// put values into all cross references
void CLinker::relocate() {
    uint32_t modu;                               // module index
    uint32_t r;                                  // relocation loop counter
    ElfFwcReloc * reloc;                        // relocation record
    uint32_t sourcePos;                          // relocation source position in file
    ElfFwcSym * targetSym;                      // target symbol record
    ElfFwcSym * externTargetSym;                // external target symbol record
    ElfFwcSym * refSym;                         // reference symbol record
    uint64_t targetAddress;                      // address of target symbol
    uint64_t referenceAddress;                   // address of reference symbol
    int64_t  value;                              // value of relocation
    uint32_t targetModule;                       // module containing target symbol
    uint32_t refsymModule;                       // module containing reference symbol
    SReloc2  rel2;                               // relocation record for executable file
    bool     relink;                             // copy relocation to relinkable executable file

    // loop through all modules to get all relocation records
    for (modu = 0; modu < modules2.numEntries(); modu++) {
        if (modules2[modu].dataSize() == 0) continue;
        relink = modules2[modu].relinkable;
        for (r = 0; r < modules2[modu].relocations.numEntries(); r++) {
            // loop through relocations
            reloc = &modules2[modu].relocations[r];
            if (reloc->r_type == 0) continue;              // removed relocation
            // find source address
            if (reloc->r_section > modules2[modu].nSections) {
                err.submit(ERR_ELF_INDEX_RANGE); continue;
            }
            // source address in executable file
            // uint64_t sourceAddr = modules2[modu].sectionHeaders[reloc->r_section].sh_addr + reloc->r_offset;
            // source address in local module. This is where the binary data are currently stored
            sourcePos = uint32_t(modules2[modu].sectionHeaders[reloc->r_section].sh_offset + reloc->r_offset);
            if (sourcePos >= modules2[modu].dataBuffer.dataSize()) {
                err.submit(ERR_ELF_INDEX_RANGE); continue;
            }
            // find target symbol
            targetSym = &modules2[modu].symbols[reloc->r_sym];
            externTargetSym = findSymbolAddress(&targetAddress, &targetModule, targetSym, modu);
            if (externTargetSym == 0) {
                err.submit(ERR_ELF_INDEX_RANGE); continue;
            }
            // check if target symbol is in relinkable section
            if (externTargetSym->st_other & STV_RELINK) relink = true;
            if (relink) {
                // copy symbol records to executable file if necessary
                if (targetSym->st_section || (targetSym->st_bind & STB_WEAK)) {
                    targetSym->st_bind |= STB_EXE;
                }
            }

            // check register use
            checkRegisterUse(targetSym, externTargetSym, modu);

            // find reference symbol
            if (reloc->r_refsym && (reloc->r_type & R_FORW_RELTYPEMASK) == R_FORW_REFP) {
                refSym = &modules2[modu].symbols[reloc->r_refsym];
                refSym = findSymbolAddress(&referenceAddress, &refsymModule, refSym, modu);
                if (refSym->st_other & STV_RELINK) relink = true;
            }
            else {
                refSym = 0;
                referenceAddress = 0;
                refsymModule = 0;
            }
            value = int64_t(targetAddress - referenceAddress);

            // select relocation type
            switch (reloc->r_type >> 16 & 0xFF) {
            case R_FORW_ABS >> 16:  // absolute symbol or absolute address
                if (externTargetSym->st_type != STT_CONSTANT && externTargetSym->st_type != 0) {
                    // this is an absolute address to insert at load time. the code is not position-independent
                    // char * nm =  (char*)modules2[modu].stringBuffer.buf() + targetSym->st_name;
                    reloc->r_type |= R_FORW_LOADTIME;
                    fileHeader.e_flags |= EF_RELOCATE | EF_POSITION_DEPENDENT;
                }
                break;
            case R_FORW_SELFREL >> 16:
                value = int64_t(targetAddress - reloc->r_offset - modules2[modu].sectionHeaders[reloc->r_section].sh_addr);
                if ((modules2[modu].sectionHeaders[reloc->r_section].sh_flags
                    ^ externTargetSym->st_other) & SHF_BASEPOINTER) {
                    // different base pointers
                DIFFERENTBASEPOINTERS:
                    err.submit(ERR_LINK_DIFFERENT_BASE,
                        cmd.getFilename(modules2[modu].moduleName),
                        (char*)modules2[modu].stringBuffer.buf() + externTargetSym->st_name,
                        cmd.getFilename(modules2[targetModule].moduleName));
                }
                break;
            case R_FORW_IP_BASE >> 16:
                value = int64_t(targetAddress - ip_base);
                if (!(externTargetSym->st_other & STV_IP)) goto DIFFERENTBASEPOINTERS;
                break;
            case R_FORW_DATAP >> 16:
                value = int64_t(targetAddress - datap_base);
                if (!(externTargetSym->st_other & STV_DATAP)) goto DIFFERENTBASEPOINTERS;
                break;
            case R_FORW_THREADP >> 16:
                if (!(externTargetSym->st_other & STV_THREADP)) goto DIFFERENTBASEPOINTERS;
                break;
            case R_FORW_REFP >> 16:
                if (refSym == 0 || ((externTargetSym->st_other ^ refSym->st_other) & SHF_BASEPOINTER)) {
                    goto DIFFERENTBASEPOINTERS;
                }
                break;
            case R_FORW_SYSFUNC:
            case R_FORW_SYSMODUL:
            case R_FORW_SYSCALL:
                // system function ID inserted at load time
                reloc->r_type |= R_FORW_LOADTIME;
                fileHeader.e_flags |= EF_RELOCATE;
                break;
            }
            // add addend (sign extended)
            value += reloc->r_addend;
            // scale
            uint32_t scale = reloc->r_type & R_FORW_RELSCALEMASK;
            // check if divisible by scale
            if (value & ((1 << scale) - 1)) {
                // misaligned target. scaling of reference failed
                err.submit(ERR_LINK_MISALIGNED_TARGET,
                    cmd.getFilename(modules2[modu].moduleName),
                    (char*)modules2[modu].stringBuffer.buf() + externTargetSym->st_name,
                    cmd.getFilename(modules2[targetModule].moduleName));
            }
            value >>= scale;

            // check if overflow and insert value
            switch ((reloc->r_type >> 8) & 0xFF) {
            case R_FORW_8 >> 8:
                modules2[modu].dataBuffer.get<int8_t>((uint32_t)sourcePos) = (int8_t)value;
                if (value > 0x7F || value < -0x80) {
                RELOCATIONOVERFLOW:
                    err.submit(ERR_LINK_OVERFLOW,
                        cmd.getFilename(modules2[modu].moduleName),
                        (char*)modules2[modu].stringBuffer.buf() + externTargetSym->st_name,
                        cmd.getFilename(modules2[targetModule].moduleName));
                }
                break;
            case R_FORW_16 >> 8:
                modules2[modu].dataBuffer.get<int16_t>((uint32_t)sourcePos) = (int16_t)value;
                if (value > 0x7FFF || value < -0x8000) goto RELOCATIONOVERFLOW;
                break;
            case R_FORW_24 >> 8:
                modules2[modu].dataBuffer.get<int16_t>((uint32_t)sourcePos) = (int16_t)value;
                modules2[modu].dataBuffer.get<int8_t>((uint32_t)sourcePos + 2) = (int8_t)(value >> 16);
                if (value > 0x7FFFFF || value < -0x800000)  goto RELOCATIONOVERFLOW;
                break;
            case R_FORW_32 >> 8:
                modules2[modu].dataBuffer.get<int32_t>((uint32_t)sourcePos) = (int32_t)value;
                if (value > 0x7FFFFFFF || value < -((int64_t)1 << 31))  goto RELOCATIONOVERFLOW;
                break;
            case R_FORW_32LO >> 8:
                modules2[modu].dataBuffer.get<int16_t>((uint32_t)sourcePos) = (int16_t)value;
                if (value > 0x7FFFFFFF || value < -((int64_t)1 << 31))  goto RELOCATIONOVERFLOW;
                break;
            case R_FORW_32HI >> 8:
                if (value > 0x7FFFFFFF || value < -((int64_t)1 << 31))  goto RELOCATIONOVERFLOW;
                modules2[modu].dataBuffer.get<int16_t>((uint32_t)sourcePos) = (int16_t)(value >> 16);
                if (value > 0x7FFFFFFF || value < -((int64_t)1 << 31))  goto RELOCATIONOVERFLOW;
                break;
            case R_FORW_64 >> 8:
                modules2[modu].dataBuffer.get<int64_t>((uint32_t)sourcePos) = value;
                break;
            case R_FORW_64LO >> 8:
                modules2[modu].dataBuffer.get<int32_t>((uint32_t)sourcePos) = (int32_t)value;
                break;
            case R_FORW_64HI >> 8:
                modules2[modu].dataBuffer.get<int32_t>((uint32_t)sourcePos) = (int32_t)(value >> 32);
                break;
            }
            // mark reference to unresolved and autogenerated symbols for copy to executable
            if (relinkable) {
                if (externTargetSym->st_section == 0 && (externTargetSym->st_bind & STB_WEAK)) relink = true;
                if (refSym && refSym->st_section == 0 && (refSym->st_bind & STB_WEAK)) relink = true;
                if (externTargetSym->st_other & STV_AUTOGEN) relink = true;
                if (refSym && refSym->st_other & STV_AUTOGEN) relink = true;
            }
            // copy symbols and relocation record to executable file if target symbol or reference symbol are in relinkable sections
            if (relink || (reloc->r_type & R_FORW_LOADTIME)) {
                externTargetSym->st_bind |= STB_EXE;
                if (refSym) refSym->st_bind |= STB_EXE;
                memcpy(&rel2, reloc, sizeof(ElfFwcReloc));
                rel2.modul = modu;
                rel2.symLocal = (targetModule == modu)        // symbol is local
                    || ((targetSym->st_bind & STB_EXE) && targetSym->st_section == 0); // keep local record for weak external so that it can be replaced by relinking
                rel2.refSymLocal = (refsymModule == modu);
                relocations2.push(rel2);
            }
        }
    }
}

// Check if external function call has compatible register use
void CLinker::checkRegisterUse(ElfFwcSym * sym1, ElfFwcSym * sym2, uint32_t modul) {
    if ((sym1->st_other | sym1->st_other) & STV_REGUSE) {
        // register use specified for source or target or both
        uint32_t tregusea1 = sym1->st_reguse1;
        uint32_t tregusea2 = sym1->st_reguse2;                
        uint32_t treguseb1 = sym2->st_reguse1;
        uint32_t treguseb2 = sym2->st_reguse2;
        if (!(sym1->st_other & STV_REGUSE)) { 
            tregusea1 = tregusea2 = 0x0000FFFF; // register use not specified for source. assume default
        }
        if (sym1 == sym2 && sym1->st_section == 0 && (sym1->st_bind & STB_WEAK)) {
            // unresolved weak. will set r0 = 0 and v0 = 0
            treguseb1 = unresolvedReguse1;                
            treguseb2 = unresolvedReguse2;
        }
        else if (!(sym2->st_other & STV_REGUSE)) {
            // register use not specified for external target. assume default
            treguseb1 = treguseb2 = 0x0000FFFF;
        }
        uint32_t tregusem1 = treguseb1 & ~tregusea1;  // registers in target and not in source
        uint32_t tregusem2 = treguseb2 & ~tregusea2;
        if (tregusem1 | tregusem2) {
            // mismatched register use
            const char * symname = modules2[modul].stringBuffer.getString(sym2->st_name);
            char text[30];
            sprintf(text, "0x%X, 0x%X", tregusem1, tregusem2);
            err.submit(ERR_LINK_REGUSE, cmd.getFilename(modules2[modul].moduleName), symname,text);
            // avoid reporting multiple times if there are multiple references from a module to the same symbol
            sym1->st_reguse1 = treguseb1;
            sym1->st_reguse2 = treguseb2;
        }
    }
}

// find a symbol and its address
// the return value is a pointer to a remote symbol record. The address is returned in 'a'
ElfFwcSym * CLinker::findSymbolAddress(uint64_t * a, uint32_t * targetMod, ElfFwcSym * sym, uint32_t modul) {
    if (targetMod) *targetMod = modul;
    if (sym->st_section && (sym->st_bind & ~STB_EXE) != STB_WEAK2) {
        // target is in same module
        if (sym->st_type == STT_CONSTANT) {
            // absolute symbol
            *a = sym->st_value;
        }
        else if (sym->st_section >= modules2[modul].nSections) {
            err.submit(ERR_ELF_INDEX_RANGE); return sym;
        }
        else { // section address + offset into section
            // check if section is included in exe file. 
            // This will fail if there is a reference to a non-weak symbol in a replaced local communal section
            SLinkSection2 secSearch;
            secSearch.sh_module = modul;
            secSearch.sectioni = sym->st_section;
            int32_t x = sections2.findFirst(secSearch);
            if (x < 0) {
                const char * symname = (char*)modules2[modul].stringBuffer.buf() + sym->st_name;
                err.submit(ERR_LINK_UNRESOLVED, symname, "(relocation)");
                return sym;
            }
            *a = modules2[modul].sectionHeaders[sym->st_section].sh_addr + sym->st_value;
        }
        return sym;
    }
    else {
        // target is external. find it in symbolExports
        SSymbolEntry symSearch;                      // record for searching for symbol
        zeroAllMembers(symSearch);                   // initialize
        if (sym->st_name > modules2[modul].stringBuffer.dataSize()) {
            err.submit(ERR_ELF_INDEX_RANGE); return sym;
        }
        const char * symname = (char*)modules2[modul].stringBuffer.buf() + sym->st_name;
        symSearch.name = symbolNameBuffer.pushString(symname);
        symSearch.st_bind = STB_IGNORE;          // find both strong and weak symbols
        uint32_t firstMatch = 0;
        uint32_t numMatch = symbolExports.findAll(&firstMatch, symSearch);
        if (numMatch == 0) {
            // symbol name not found
            if (!(sym->st_bind & STB_WEAK)) {
                sym->st_bind = STB_UNRESOLVED;   // not weak. mark as unresolved
                if (sym->st_type == STT_FUNC) sym->st_other |= SHF_EXEC;
            }
            // give it a dummy
            *targetMod = 0;
            switch (sym->st_other & (SHF_BASEPOINTER | SHF_EXEC)) {
            case 0:                   // constant
                *a = 0;  break;
            case STV_IP:              // read-only data
                *a = dummyConst;  break;
            case STV_DATAP:           // writeable data. Make one address for each unresolved reference
                *a = dummyData + (--unresolvedWeakNum) * 8;
                break;
            case STV_THREADP:         // thread-local. this is rare
                *a = dummyThreadData;  break;
            case STV_IP | STV_EXEC:   // unresolved function
                *a = dummyFunc;  break;
            }
            return sym;
        }
   
        // one or more matching symbols found
        int32_t targetModule = findModule(symbolExports[firstMatch].library, symbolExports[firstMatch].member);
        if (targetModule == -2) {
            // special symbol
            switch (symbolExports[firstMatch].symindex) {
            case 1:
                *a = ip_base;  break;
            case 2:
                *a = datap_base;  break;
            case 3:
                *a = threadp_base;  break;
            case 4:
                *a = event_table;  break;
            case 5:
                *a = event_table_num;  break;
            default:
                err.submit(ERR_LINK_UNRESOLVED, symname, "relocation");
            }
            sym->st_other |= STV_AUTOGEN;        // symbol is autogenerated
            return sym;
        }
        if (targetMod) *targetMod = targetModule;
        if (targetModule < 0) { 
            // unexpected error 
            err.submit(ERR_LINK_UNRESOLVED, symname, "relocation");
            return sym;
        }
        // find external target symbol
        ElfFwcSym * targetSym = &modules2[targetModule].symbols[symbolExports[firstMatch].symindex];
        if (modules2[targetModule].relinkable) {
            targetSym->st_other |= STV_RELINK;
        }
        if (targetSym->st_type == STT_CONSTANT) {
            // absolute symbol
            *a = targetSym->st_value;
        }
        else if (targetSym->st_section >= modules2[targetModule].nSections) {
            err.submit(ERR_ELF_INDEX_RANGE); return sym;
        }
        else { // section address + offset into section
            // check if target section is included in exe file. This will fail only if there is a reference to a non-weak symbol in a replaced local communal section
            SLinkSection2 secSearch;
            secSearch.sh_module = targetModule;
            secSearch.sectioni = targetSym->st_section;
            int32_t x = sections2.findFirst(secSearch);
            if (x < 0) {
                const char * symname = (char*)modules2[modul].stringBuffer.buf() + sym->st_name;
                err.submit(ERR_LINK_UNRESOLVED, symname, "(removed)");
                return sym;
            }
            *a = modules2[targetModule].sectionHeaders[targetSym->st_section].sh_addr + targetSym->st_value;
        }
        return targetSym;
    }
}

// find the final address of a symbol from its name
uint64_t CLinker::findSymbolAddress(const char * name) {
    SSymbolEntry symSearch;                      // record for symbol search
    int32_t symi;                                // symbol index
    int32_t modul;                               // module containing symbol
    ElfFwcSym * sym;                            // pointer to symbol record
    uint64_t addr = 0xFFFFFFFFFFFFFFFF;          // return value
    symSearch.name = symbolNameBuffer.pushString(name);
    symSearch.st_bind = STB_GLOBAL;              // search for strong symbols only
    symi = symbolExports.findFirst(symSearch);
    if (symi >= 0) {                             // strong symbol found
        modul = findModule(symbolExports[symi].library, symbolExports[symi].member);
        if (modul >= 0) {
            sym = &modules2[modul].symbols[symbolExports[symi].symindex];
            findSymbolAddress(&addr, 0, sym, modul);
        }
    }
    return addr;
}

// copy sections to output file
void CLinker::copySections() {
    ElfFwcShdr header;                          // section header
    zeroAllMembers(header);                      // initialize
    uint32_t s;                                  // section index
    CELF * modul;                                // module containing section
    uint32_t sectionx = 0;                       // section index in executable file
    uint32_t progheadi = 0;                      // program header index
    uint32_t lastprogheadi = 0xFFFFFFFF;         // program header index of previous section
    CMemoryBuffer dummyBuffer;                   // buffer for dummy symbols
    CMemoryBuffer * dataBuf;                     // pointer to data buffer
    uint64_t dummyValue;                         // value of unresolved weak external symbols
    uint32_t lastFlags = 0;                      // previous section flags
    uint8_t type, lastType = 0;                  // section type
    uint32_t pHfistSection = 0;                  // first section covered by program header
    uint32_t pHlastSection = 0;                  // last section covered by program header
    uint32_t pHnumSections = 0;                  // number of sections covered by program header
    ElfFwcPhdr * pPHead = 0;                     // pointer to program header

    // find program header
    if (outFile.programHeaders.numEntries()) {
        pPHead = &outFile.programHeaders[progheadi];
        pHfistSection = (uint32_t)pPHead->p_paddr;
        pHnumSections = (uint32_t)(pPHead->p_paddr >> 32);
    }

    // loop through sections
    for (s = 0; s < sections.numEntries(); s++) {
        // make section header
        header.sh_type = sections[s].sh_type;
        if (header.sh_type == 0) continue;
        header.sh_name = sections[s].name;
        header.sh_flags = sections[s].sh_flags;
        header.sh_size = sections[s].sh_size;
        header.sh_align = sections[s].sh_align;
        header.sh_module = sections[s].sh_module;
        if (header.sh_module < modules2.numEntries()) {
            modul = &modules2[sections[s].sh_module]; // find module
            header.sh_library = modul->library;
            header.sh_offset = modul->sectionHeaders[sections[s].sectioni].sh_offset;
            header.sh_addr = modul->sectionHeaders[sections[s].sectioni].sh_addr;
            dataBuf = &modul->dataBuffer;
        }
        else {
            header.sh_library = 0;
            // make section for dummy symbol
            switch (sections[s].sh_module) {
            case 0xFFFFFFF1: default:            // read only data
                dummyValue = 0;
                header.sh_offset = dummyBuffer.push(&dummyValue, 8);
                header.sh_addr = dummyConst;
                break;
            case 0xFFFFFFF2:                     // writeable data
                dummyValue = 0;
                header.sh_offset = dummyBuffer.dataSize();
                header.sh_addr = dummyData;
                for (uint32_t i = 0; i < unresolvedWeakNum; i++) dummyBuffer.push(&dummyValue, 8);
                break;
            case 0xFFFFFFF3:                     // thread-local data 
                dummyValue = 0;
                header.sh_offset = dummyBuffer.push(&dummyValue, 8);
                header.sh_addr = dummyThreadData;
                break;
            case 0xFFFFFFF4:                     // unresolved weak function. return zero
                header.sh_addr = dummyFunc;
                header.sh_offset = dummyBuffer.dataSize();
                for (uint32_t i = 0; i < unresolvedFunctionN; i++) {
                    dummyBuffer.push(&unresolvedFunction[i], 4);
                }
                break;
            case 0xFFFFFFF8:                     // event list
                header.sh_offset = dummyBuffer.push(eventData.buf(), eventData.dataSize());
                break;
            }
            dataBuf = &dummyBuffer;
        }
        // find correcponding program header, if any
        while (s >= pHfistSection + pHnumSections && progheadi+1 < outFile.programHeaders.numEntries()) {
            progheadi++;
            pPHead = &outFile.programHeaders[progheadi];
            pHfistSection = (uint32_t)pPHead->p_paddr;
            pHnumSections = (uint32_t)(pPHead->p_paddr >> 32);
        }
        // is this section covered by a program header?
        bool hasProgHead = s >= pHfistSection && s < pHfistSection + pHnumSections;

        if (hasProgHead && progheadi == lastprogheadi && s > 0 && sections[s].sh_type != SHT_NOBITS) {
            // this section is covered by same program header as last section
            // insert any necessary filler
            uint64_t fill = sections[s].sh_addr - (sections[s-1].sh_addr + sections[s-1].sh_size);
            if (fill > MAX_ALIGN) err.submit(ERR_LINK_OVERFLOW, "","","");
            if (fill > 0) {
                // insert alignment filler in dataBuffer               
                outFile.insertFiller(fill);
            }
        }
        type = header.sh_type;
        if (type == SHT_COMDAT) type = SHT_PROGBITS;  // communal and normal data can be joined together

        // add section to outFile
        if (hasProgHead 
        && progheadi == lastprogheadi 
        && type == lastType
        && !cmd.debugOptions 
        && !(header.sh_flags & SHF_RELINK)
        && !(lastFlags & SHF_RELINK)
        && sections[s].sh_module < 0xFFFFFFF0) {
            outFile.extendSection(header, *dataBuf);
        }
        else {        
            sectionx = outFile.addSection(header, cmd.fileNameBuffer, *dataBuf);
        }
        // remember new section index
        sections[s].sectionx = sectionx;
        lastprogheadi = progheadi;
        lastType = type;
        lastFlags = header.sh_flags;

#if 0   // testing only: list sections
        printf("\n%2i %X  %s", outFile.sectionHeaders.numEntries(), header.sh_type, cmd.getFilename(header.sh_name));
#endif
    }

    // update section indexes in segment headers.
    // indexes may have changed if some sections are joined together.
    // p_paddr contains first section index and number of sections
    for (uint32_t ph = 0; ph < outFile.programHeaders.numEntries(); ph++) {
        pHfistSection = (uint32_t)outFile.programHeaders[ph].p_paddr;
        pHnumSections = (uint32_t)(outFile.programHeaders[ph].p_paddr >> 32);
        pHlastSection = pHfistSection + pHnumSections - 1;
        if (pHlastSection < sections.numEntries()) {
            uint32_t sx1 = sections[pHfistSection].sectionx;  // first new section index
            uint32_t sx2 = sections[pHlastSection].sectionx;  // last new section index
            uint32_t numsx = sx2 - sx1 + 1;                   // number of new sections
            outFile.programHeaders[ph].p_paddr = sx1 | (uint64_t)numsx << 32;
        }
    }

    // sections list has been modified. update sections2
    sections2.copy(sections);
    sections2.sort();

    // make lists of module names and library names
    CDynamicArray<uint32_t> moduleNames, libraryNames;
    moduleNames.setNum(modules2.numEntries());
    for (uint32_t m = 0; m < modules2.numEntries(); m++) {
        moduleNames[m] = modules2[m].moduleName;
    }
    libraryNames.setNum(libraries.numEntries());
    for (uint32_t lib = 0; lib < libraries.numEntries(); lib++) {
        libraryNames[lib] = libraries[lib].libraryName;
    }

    // copy module names and library names to relinkable sections
    outFile.addModuleNames(moduleNames, libraryNames);
}

// copy symbols to output file
void CLinker::copySymbols() {
    uint32_t s;                                  // symbol index
    ElfFwcSym sym;                               // symbol record
    uint32_t modul;                              // module containing symbol
    SSymbolXref2 xref;                           // symbol cross reference record
    SLinkSection2 searchSection;                 // record to search for section
    char const * name;                           // symbol name
    int32_t sx;                                  // section index in sections2
    char text[12];                               // temporary text
    CDynamicArray<SSymbolXref2> xreflist;        // list of cross reference records, sorted by name
    // make symbol number 0 empty
    zeroAllMembers(sym);
    outFile.addSymbol(sym, cmd.fileNameBuffer);

    for (s = 0; s < symbolExports.numEntries(); s++) {
        // skip weak public symbols if overridden and not relinkable
        while (s+1 < symbolExports.numEntries() && symbolExports[s] == symbolExports[s+1]) {
            // next symbol has same name
            modul = findModule(symbolExports[s].library, symbolExports[s].member);
            if (modules2[modul].relinkable) break;  // relinkable. preserve both symbols
            if (symbolExports[s+1].st_bind & STB_WEAK) {
                modul = findModule(symbolExports[s+1].library, symbolExports[s+1].member);
                modules2[modul].symbols[symbolExports[s+1].symindex].st_bind |= STB_IGNORE;
            }
            s++;
        }
        // if (symbolExports[s].library == 0xFFFFFFFE) 
        // The special symbols __ip_base, etc are not copied to the executable file.        
        // If we want them then we need to find the corresponding sections        
    }

    // loop through all modules to get all symbols
    for (modul = 0; modul < modules2.numEntries(); modul++) {
        for (s = 0; s < modules2[modul].symbols.numEntries(); s++) {
            sym = modules2[modul].symbols[s];
            if (sym.st_section || (sym.st_bind & STB_EXE)) {
                if ((sym.st_bind & (STB_EXE | STB_IGNORE)) == STB_EXE
                || ((sym.st_bind & (STB_GLOBAL | STB_WEAK)))
                || (cmd.debugOptions && sym.st_bind != STB_LOCAL)) {
                    name = (char*)modules2[modul].stringBuffer.buf() + modules2[modul].symbols[s].st_name;
                    xref.modul = modul;
                    xref.name = symbolNameBuffer.pushString(name);
                    xref.symi = s;
                    xref.symx = 0;
                    xref.isPublic = sym.st_section != 0;
                    xref.isWeak = (sym.st_bind & STB_WEAK) != 0;
                    xreflist.push(xref);
                }
            }
        }
    }
    // sort by name
    xreflist.sort();
    bool changed = false;
    // remove any $$number and subsequent text from all symbol names
    for (s = 0; s < xreflist.numEntries(); s++) {
        char * name1 = (char*)symbolNameBuffer.buf() + xreflist[s].name;
        char * p = strchr(name1, '$');
        if (p && p[1] == '$' && p[2] >= '0' && p[2] <= '9') {
            *p = 0;
            changed = true;
        }
    }
    // sort again
    if (changed) xreflist.sort();

    // search for duplicate names
    for (s = 0; s < xreflist.numEntries(); s++) {
        uint32_t num = 0;
        name = symbolNameBuffer.getString(xreflist[s].name);
        if (xreflist[s].isPublic && !xreflist[s].isWeak) {  
            // local or public non-weak symbol. check if duplicate names
            while (s+1 < xreflist.numEntries() && !(xreflist[s] < xreflist[s+1])) {
                // next symbol has same name
                s++;
                if (xreflist[s].isPublic && !xreflist[s].isWeak) {
                    // this symbol is local or public and non-weak. there is a name clash
                    // change duplicate name to name$$number
                    xreflist[s].name = symbolNameBuffer.push(name, (uint32_t)strlen(name));
                    sprintf(text, "$$%u", ++num);
                    symbolNameBuffer.pushString(text);
                    const char * name2 = symbolNameBuffer.getString(xreflist[s].name);
                    // also change name of original symbol
                    SSymbolXref2 & x2 = xreflist[s];
                    ElfFwcSym & s2 = modules2[x2.modul].symbols[x2.symi];
                    s2.st_name = modules2[x2.modul].stringBuffer.pushString(name2);
                }
            }
        }
    }
    // sort cross references by module
    symbolXref << xreflist;
    symbolXref.sort();

    // copy symbols to outFile
    for (s = 0; s < symbolXref.numEntries(); s++) {
        modul = symbolXref[s].modul;
        sym = modules2[modul].symbols[symbolXref[s].symi];
        if (sym.st_section != 0) {
            // translate local section index to final section index
            searchSection.sh_module = modul;
            searchSection.sectioni = sym.st_section;
            sx = sections2.findFirst(searchSection);
            if (sx < 0) {
                continue;  // symbol is in a discarded communal section. drop it
            }
            // adjust address
            uint32_t newsection = sections2[sx].sectionx;
            sym.st_value += sections2[sx].sh_addr - outFile.sectionHeaders[newsection].sh_addr;
            sym.st_section = newsection;
        }
        sym.st_bind &= ~ STB_EXE;
        symbolXref[s].symx = outFile.addSymbol(sym, modules2[modul].stringBuffer);
    }
    // make records for unresolved weak symbols
    if (relinkable) {
        zeroAllMembers(sym);
        for (s = 0; s < symbolImports.numEntries(); s++) {
            if ((symbolImports[s].status & 5) && (symbolImports[s].st_bind & STB_WEAK)) {
                // unresolved weak. make a symbol record
                sym.st_name = symbolImports[s].name;
                sym.st_type = symbolImports[s].st_type;
                sym.st_bind = symbolImports[s].st_bind;
                sym.st_other = symbolImports[s].st_other;
                // skip any additional unresolved symbols with same name
                while (s+1 < symbolImports.numEntries() && symbolImports[s] == symbolImports[s+1]) s++;
                // put record in output file
                xref.symx = outFile.addSymbol(sym, symbolNameBuffer);
                xref.name = sym.st_name;
                xref.modul = symbolImports[s].library;
                xref.symi = symbolImports[s].symindex;
                // put new index into list of unresolved weak symbols
                unresWeakSym.push(xref);  // this list will be sorted by name because symbolImports is sorted by name
            }
        }
    }
}

// copy relocation records to output file if needed
void CLinker::copyRelocations() {
    uint32_t r;                                  // relocation index
    int32_t s;                                   // symbol index
    SReloc2  rel2;                               // extended relocation record
    SSymbolXref symx;                            // record for searching for symbol in symbolXref
    CDynamicArray<SReloc2> relocations3;         // extended relocation records. load-time relocations first
    relocations3.setSize(relocations2.dataSize());

    // get load-time relocations first
    for (r = 0; r < relocations2.numEntries(); r++) {
        if (relocations2[r].r_type & R_FORW_LOADTIME) {
            relocations3.push(relocations2[r]);
        }
    }
    // get remaining relocations, used only for relinking
    for (r = 0; r < relocations2.numEntries(); r++) {
        if (!(relocations2[r].r_type & R_FORW_LOADTIME)) {
            relocations3.push(relocations2[r]);
        }
    }

    // relocations3 contains list of relocations that need to be copied to executable file
    for (r = 0; r < relocations3.numEntries(); r++) {
        rel2 = relocations3[r];
        if (rel2.r_type == 0) continue;          // removed
        if (rel2.modul >= modules2.numEntries()) {
            err.submit(ERR_ELF_INDEX_RANGE);  continue;
        }
        // translate section index
        SLinkSection2 secSearch;
        secSearch.sh_module = rel2.modul;
        secSearch.sectioni = rel2.r_section;
        int32_t x = sections2.findFirst(secSearch);
        if (x < 0) continue;                     // section not found. ignore
        rel2.r_section = sections2[x].sectionx;
        // adjust offset
        rel2.r_offset += sections2[x].sh_addr - outFile.sectionHeaders[rel2.r_section].sh_addr;

        // translate symbol index
        if (rel2.symLocal) {
            // symbol is local. reference by ID
            symx.modul = rel2.modul;
            symx.symi = rel2.r_sym;
            s = symbolXref.findFirst(symx);
            if (s < 0) {
                // unresolved weak
                rel2.r_sym = resolveRelocationTarget(rel2.modul, rel2.r_sym);
            }
            else  rel2.r_sym = symbolXref[s].symx;
        }
        else {
            // symbol is remote. search by name
            rel2.r_sym = resolveRelocationTarget(rel2.modul, rel2.r_sym);
        }

        // translate reference symbol index
        if (rel2.r_refsym) {
            if (rel2.refSymLocal) {
                // reference symbol is local. reference by ID
                symx.modul = rel2.modul;
                symx.symi = rel2.r_refsym;
                s = symbolXref.findFirst(symx);
                if (s < 0) {
                    rel2.r_refsym = resolveRelocationTarget(rel2.modul, rel2.r_refsym);
                }                
                else rel2.r_refsym = symbolXref[s].symx;
            }
            else {
                // reference symbol is remote. search by name
                rel2.r_refsym = resolveRelocationTarget(rel2.modul, rel2.r_refsym);
            }
        }
        // put relocation in outFile
        outFile.addRelocation(rel2);
    }
}

// resolve relocation target for executable file record
uint32_t CLinker::resolveRelocationTarget(uint32_t modul, uint32_t symi) {
    CELF * modulp;                               // pointer to module
    const char * symname;                        // symbol name
    int32_t ie;                                  // index into symbolExports 
    int32_t iu;                                  // index into unresWeakSym 
    int32_t is;                                  // index into symbolXref 
    uint32_t modt;                               // target module
    SSymbolEntry syms;                           // record for searching for symbol in symbolExports
    SSymbolXref2 symu;                           // record for searching for symbol in unresWeakSym
    SSymbolXref symx;                            // record for searching for symbol in symbolXref

    modulp = &modules2[modul];                   // module
    // search by name
    if (symi >= modulp->symbols.numEntries()) {
        err.submit(ERR_ELF_INDEX_RANGE);  return 0;
    }
    symname = (char*)modulp->stringBuffer.buf() + modulp->symbols[symi].st_name;
    syms.name = symbolNameBuffer.pushString(symname);
    syms.st_bind = STB_IGNORE;          // find both strong and weak symbols
    ie = symbolExports.findFirst(syms);
    if (ie < 0) { 
        // symbol name not found
        if (modulp->symbols[symi].st_bind & STB_WEAK) {
            // weak symbol not found
            symu.name = symbolNameBuffer.pushString(symname);
            iu = unresWeakSym.findFirst(symu);
            if (iu >= 0) {
                return unresWeakSym[iu].symx;
            }
            // strong symbol not found
            err.submit(ERR_REL_SYMBOL_NOT_FOUND);  return 0;  // should not occur
        } 
    }
    if (symbolExports[ie].library > 0xFFFFFFF0) { 
        symu.name = symbolNameBuffer.pushString(symname);
        iu = unresWeakSym.findFirst(symu);
        if (iu >= 0) {
            return unresWeakSym[iu].symx;
        }
    }
    // module containing target symbol
    modt = symbolExports[ie].member;
    uint32_t symlib = symbolExports[ie].library;
    if (symlib != 0 && symlib < 0xFFFFFFF0) {
        modt = (uint32_t)findModule(symbolExports[ie].library, modt);
        if ((int32_t)modt < 0) {
            err.submit(ERR_REL_SYMBOL_NOT_FOUND);  return 0;  // should not occur
        }                
    }
    else if (symlib) {
        modt = symlib;
    }
    symx.modul = modt;
    symx.symi = symbolExports[ie].symindex;
    // find new index for this symbol
    is = symbolXref.findFirst(symx);
    if (is < 0) {
        err.submit(ERR_REL_SYMBOL_NOT_FOUND);  return 0;  // should not occur
    }
    return symbolXref[is].symx;
}

// make executable file header
void CLinker::makeFileHeader() {
    fileHeader.e_type = ET_EXEC;                 // executable file
    fileHeader.e_ip_base = ip_base;              // __ip_base relative to first ip based segment
    fileHeader.e_datap_base = datap_base;        // __datap_base relative to first datap based segment
    fileHeader.e_threadp_base = 0;               // __threadp_base relative to first threadp based segment
    fileHeader.e_entry = entry_point;            // entry point for startup code
    if (relinkable) fileHeader.e_flags |= EF_RELINKABLE; // relinking allowed
}

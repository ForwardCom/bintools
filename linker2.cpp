/****************************  relink.cpp  ***********************************
* Author:        Agner Fog
* date created:  2017-12-07
* Last modified: 2018-03-30
* Version:       1.13
* Project:       Binary tools for ForwardCom instruction set
* Description:
* This module contains the relinking feature of the linker.
*
* Copyright 2017-2024 GNU General Public License http://www.gnu.org/licenses
*****************************************************************************/

#include "stdafx.h"


// load executable file to be relinked
void CLinker::loadExeFile() {

    // Read input file
    const char * inputFileName = cmd.getFilename(cmd.inputFile);
    inputFile.read(inputFileName);
    if (err.number()) return;
    inputFile.split();
    if (!(inputFile.fileHeader.e_flags & EF_RELINKABLE)) {
        err.submit(ERR_INPUT_NOT_RELINKABLE, inputFileName);
        return;
    }

    // get names of modules and libraries to remove or replace
    getReplaceNames();

#if 0   
    for (int i = 0; i < rnames.numEntries(); i++) {
        printf("\n%4X %s", rnames[i].command, cmd.getFilename(rnames[i].filename));
    }
#endif

    markSectionsInInputFile();
}

// get names of modules and libraries to remove or replace
void CLinker::getReplaceNames() {
    uint32_t i, j;                               // loop counter. command index
    const char * fname;                          // file name or module name
    SLCommand cmd2;                              // copy of command line item
    numObjects = 0;                              // number of object files to add
    numLibraries = 0;                            // number of library files to add
    bool isLib;                                  // file name indicates a library (.li or .a)

    // make a list of removed and replaced modules and libraries
    // and count number of object files and library files
    for (i = 0; i < cmd.lcommands.numEntries(); i++) {
        cmd2.command = 0;
        // name of module
        fname = cmd.getFilename(cmd.lcommands[i].filename);

        // is it a library?
        isLib = false;
        // find last '.'
        for (j = (int32_t)strlen(fname) - 1; j > 0; j--) {
            if (fname[j] == '.') break;
        }
        if ((j > 0 && strncasecmp_(fname + j, ".li", 3) == 0) || fname[j+1] == 'a') {
            isLib = true;
        }

        if ((cmd.lcommands[i].command & 0xFF) == CMDL_LINK_ADDMODULE) {
            // remove path
            cmd.lcommands[i].value = cmd.fileNameBuffer.pushString(removePath(fname));

            if (isLib) {                // this is a library
                numLibraries++;
                cmd.lcommands[i].command = CMDL_LINK_ADDLIBRARY | (cmd.lcommands[i].command & CMDL_LINK_RELINKABLE);
            }
            else {                  // assume that this is an object file
                numObjects++;
            }
            cmd2 = cmd.lcommands[i];
            cmd2.command |= CMDL_LINK_REPLACE;
        }
        if ((cmd.lcommands[i].command & 0xFF) == CMDL_LINK_ADDLIBMODULE) {
            // object module from library file
            // remove path
            cmd.lcommands[i].value = cmd.fileNameBuffer.pushString(removePath(fname));
            numObjects++;
            cmd2 = cmd.lcommands[i];
            cmd2.command |= CMDL_LINK_REPLACE;
        }
        if ((uint8_t)cmd.lcommands[i].command == CMDL_LINK_REMOVE) {
            // remove module from relinkable file
            // remove path
            cmd.lcommands[i].value = cmd.fileNameBuffer.pushString(removePath(fname));
            cmd2 = cmd.lcommands[i];
            if (isLib) cmd2.command |= CMDL_LINK_ADDLIBRARY;
            else cmd2.command |= CMDL_LINK_ADDMODULE;
        }
        // add command to rnames
        if (cmd2.command) {
            int32_t r = rnames.findFirst(cmd2);
            if (r >= 0) {
                // already in list. combine commands
                rnames[r].command |= cmd2.command;
            }
            else {
                // new name. add to list
                rnames.addUnique(cmd2);
            }
        }
    }
}

// check which sections to keep or remove in executable input file,
// and make list of modules and libraries to relink
void CLinker::markSectionsInInputFile() {
    uint32_t sec;                      // section index
    uint32_t rec;                      // rnames index
    const char * modName;              // module name
    const char * libName;              // library name
    SLCommand cmdrec;                  // command record used only for name search
    SRelinkModule relModul;            // name of relinked module
    zeroAllMembers(cmdrec);

    for (sec = 0; sec < inputFile.sectionHeaders.numEntries(); sec++) {
        ElfFwcShdr secHdr = inputFile.sectionHeaders[sec];
        if (secHdr.sh_type == 0) continue;
        inputFile.sectionHeaders[sec].sh_relink = 0;

        if (secHdr.sh_module && secHdr.sh_module < inputFile.secStringTableLen) {
            modName = inputFile.secStringTable + secHdr.sh_module;
        }
        else modName = "";

        if (secHdr.sh_library && secHdr.sh_library < inputFile.secStringTableLen) {
            libName = inputFile.secStringTable + secHdr.sh_library;
        }
        else libName = "";

        // search for module name and library name in rnames
        zeroAllMembers(cmdrec);
        zeroAllMembers(relModul);
        if (modName[0]) {        
            cmdrec.value = relModul.moduleName = cmd.fileNameBuffer.pushString(modName);
            int32_t f1 = rnames.findFirst(cmdrec);
            if (f1 >= 0) {
                // module name is in list
                rnames[f1].command |= CMD_NAME_FOUND;  // mark name found
                cmdrec.command = rnames[f1].command & ~CMDL_LINK_ADDLIBRARY;
                if (inputFile.sectionHeaders[sec].sh_flags & SHF_RELINK) {
                    inputFile.sectionHeaders[sec].sh_relink = (uint8_t)rnames[f1].command; // mark section for replace or delete
                }
                else {
                    err.submit(ERR_CANT_RELINK_MODULE, modName);
                }
            }
        }
        if (libName[0]) {        
            cmdrec.value = relModul.libraryName = cmd.fileNameBuffer.pushString(libName);
            int32_t f2 = rnames.findFirst(cmdrec);
            if (f2 >= 0) {
                // library name is in list
                rnames[f2].command |= CMD_NAME_FOUND;  // mark name found
                cmdrec.command = rnames[f2].command | CMDL_LINK_ADDLIBRARY;
                if (inputFile.sectionHeaders[sec].sh_flags & SHF_RELINK) {                
                    inputFile.sectionHeaders[sec].sh_relink |= (uint8_t)rnames[f2].command; // mark section for replace or delete
                }
                else {
                    err.submit(ERR_CANT_RELINK_LIBRARY, modName);
                }
            }
        }

        // add module or library to relinkModules list unless it is removed or replaced
        if (cmdrec.value && !(cmdrec.command & (CMDL_LINK_REMOVE | CMDL_LINK_REPLACE))) {
            relinkModules.addUnique(relModul);
        }

#if 0   // testing only: list sections
        const char * secName;              // section name
        if (secHdr.sh_name < inputFile.stringBuffer.dataSize()) {
            secName = inputFile.stringBuffer.getString(secHdr.sh_name);
        }
        else secName = "";
        printf("\n: %2i >> %s %s %s %2X", sec, secName, libName, modName, inputFile.sectionHeaders[sec].sh_relink);
#endif
    }
#if 0   // testing only: list kept modules and libraries
    for (rec = 0; rec < relinkModules.numEntries(); rec++) {
        printf("\n# %s:%s", cmd.getFilename(relinkModules[rec].libraryName), cmd.getFilename(relinkModules[rec].moduleName));
    }
#endif

    // check if all remove/replace records in rnames have been matched by sections in input file
    for (rec = 0; rec < rnames.numEntries(); rec++) {
        if ((rnames[rec].command & CMDL_LINK_REMOVE) && !(rnames[rec].command & CMD_NAME_FOUND)) {
            // unmatched name
            modName = cmd.getFilename(uint32_t(rnames[rec].value));
            if (rnames[rec].command & CMDL_LINK_ADDMODULE) {
                err.submit(ERR_RELINK_MODULE_NOT_FOUND, modName);
            }
            else {
                err.submit(ERR_RELINK_LIBRARY_NOT_FOUND, modName);
            }
        }
    }
}

// extract a module from executable input file
void CLinker::extractModule(CELF & modul, uint32_t libname, uint32_t name) {
    // libname: library name as index into cmd.fileNameBuffer. Zero if not a library member
    // name: module name as index into cmd.fileNameBuffer. Zero to build a module of all non-relinkable sections

    uint32_t sec;                                // section index in inputFile
    uint32_t seci;                               // section index in modul
    uint32_t sym;                                // symbol index in inputFile
    uint32_t symi;                               // symbol index in modul
    uint32_t rel;                                // relocation index in inputFile
    const char * modName;                        // module name of section
    const char * libName;                        // library name of library
    const char * modName1;                       // module name to search for
    const char * libName1;                       // library name to search for
    uint32_t * symp;                             // pointer to symbol index
    modName1 = cmd.getFilename(name);            // will be "" if name = 0
    libName1 = cmd.getFilename(libname);         // will be "" if libname = 0
    CDynamicArray<uint32_t> symbolTranslate;     // list for translating symbol indexes from inputFile to modul
    CDynamicArray<uint32_t> sectionTranslate;    // list for translating section indexes from inputFile to modul
    CDynamicArray<SSymbol2> externalSymbols;     // list of external symbols referenced by current module    
    CDynamicArray<uint32_t> symbolTranslate2;    // list for translating symbol indexes from externalSymbols to modul
    ElfFwcSym symrec;                           // symbol record

    // prepare translation of symbol indexes
    symbolTranslate.setNum(inputFile.symbols.numEntries());
    sectionTranslate.setNum(inputFile.sectionHeaders.numEntries());
    zeroAllMembers(symrec);
    modul.addSymbol(symrec, inputFile.stringBuffer);  // make symbol zero empty

    // loop through sections of input file
    for (sec = 0; sec < inputFile.sectionHeaders.numEntries(); sec++) {
        ElfFwcShdr secHdr = inputFile.sectionHeaders[sec];
        if (secHdr.sh_type == 0) continue;     // skip first empty section
        if (secHdr.sh_flags & SHF_RELINK) {
            // relinkable section. check name match
            if (secHdr.sh_module && secHdr.sh_module < inputFile.secStringTableLen) {
                modName = inputFile.secStringTable + secHdr.sh_module;
            }
            else continue;
            if (strcmp(modName, modName1) != 0) continue; // name doesn't match
            // check library name
            if (secHdr.sh_library && secHdr.sh_library < inputFile.secStringTableLen) {
                libName = inputFile.secStringTable + secHdr.sh_library;
                if (libname == 0) continue;  // library member not requested
                if (strcmp(libName, libName1) != 0) continue; // library name doesn't match
            }
            else if (libname) continue;  // not a library member
        }
        else {
            // non-relinkable section
            if (name || libname) continue;      // name = 0 for collecting non-relinkable sections
            // non-relinkable sections must have fixed addresses relative to each other because the
            // corresponding relocation records are not preserved.
            // Insert address relative to ip_base, datap_base, or threadp_base:
            switch (secHdr.sh_flags & SHF_BASEPOINTER) {
            case SHF_IP:
                secHdr.sh_addr = secHdr.sh_addr - inputFile.fileHeader.e_ip_base;
                break;
            case SHF_DATAP:
                secHdr.sh_addr = secHdr.sh_addr - inputFile.fileHeader.e_datap_base;
                break;
            case SHF_THREADP:
                secHdr.sh_addr = secHdr.sh_addr - inputFile.fileHeader.e_threadp_base;
                break;
            }
        }
        // all sections that do not match name and libname have been skipped now
        if (secHdr.sh_flags & SHF_AUTOGEN) continue; // auto-generated section. will be re-made

        // add this section to the module
        seci = modul.addSection(secHdr, inputFile.stringBuffer, inputFile.dataBuffer);
        sectionTranslate[sec] = seci;

        // find symbols in this section
        for (sym = 0; sym < inputFile.symbols.numEntries(); sym++) {
            if (inputFile.symbols[sym].st_section == sec) {
                // save symbol
                symrec = inputFile.symbols[sym];
                symrec.st_section = seci;
                symi = modul.addSymbol(symrec, inputFile.stringBuffer);
                symbolTranslate[sym] = symi;                
            }
        }
    }

    // find relocations in any section belonging to this module
    for (rel = 0; rel < inputFile.relocations.numEntries(); rel++) {
        sec = inputFile.relocations[rel].r_section;
        if (sec < sectionTranslate.numEntries()) {
            seci = sectionTranslate[sec];
            if (seci) {
                ElfFwcReloc reloc = inputFile.relocations[rel];
                reloc.r_section = seci;
                // loop to cover both symbol and reference symbol in relocation record
                for (int i = 0; i < 2; i++) {
                    symp = i ? &reloc.r_refsym : &reloc.r_sym;
                    if (*symp) {
                        // there is a symbol index
                        if (*symp < symbolTranslate.numEntries() && symbolTranslate[*symp]) {
                            // symbol is in same module. Translate to index in modul
                            *symp = symbolTranslate[*symp];
                        }
                        else if (*symp < inputFile.symbols.numEntries()) {
                            // symbol is external. make external symbol record
                            SSymbol2 symbol2 = inputFile.symbols[*symp];
                            symbol2.st_section = 0;      // make symbol external
                            symbol2.st_value = 0;
                            // put symbol name in global symbolNameBuffer for the purpose of sorting
                            if (symbol2.st_name >= inputFile.stringBuffer.dataSize()) {
                                err.submit(ERR_ELF_INDEX_RANGE);  return;
                            }
                            const char * symname = (char*)inputFile.stringBuffer.buf() + symbol2.st_name;
                            symbol2.st_name = symbolNameBuffer.pushString(symname);
                            // add to list of external symbols, avoid duplicates
                            externalSymbols.addUnique(symbol2);
                            // remember that symbol index is not resolved yet
                            *symp |= 0x80000000;
                        }
                        else err.submit(ERR_ELF_INDEX_RANGE);
                    }
                }
                // save relocation record
                modul.addRelocation(reloc);
            }
        }
    }
    // add external symbols to modul and remember new indexes
    symbolTranslate2.setNum(externalSymbols.numEntries());
    for (sym = 0; sym < externalSymbols.numEntries(); sym++) {
        symrec = externalSymbols[sym];
        if (symrec.st_bind == STB_UNRESOLVED) {
            symrec.st_bind = STB_GLOBAL;     // unresolved symbol from incomplete executable. attempt to resolve it again
        }
        symbolTranslate2[sym] = modul.addSymbol(symrec, symbolNameBuffer);
    }

    // resolve external symbol indexes in relocation records in new module
    for (rel = 0; rel < modul.relocations.numEntries(); rel++) {
        ElfFwcReloc & modulReloc = modul.relocations[rel];
        // loop to cover both symbol and reference symbol in relocation record
        for (int i = 0; i < 2; i++) {
            uint32_t * symp = i ? &modulReloc.r_refsym : &modulReloc.r_sym;
            if (*symp & 0x80000000) {
                // find symbol index in externalSymbols
                SSymbol2 sym2 = inputFile.symbols[*symp & 0x7FFFFFFF];
                const char * symname = (char*)inputFile.stringBuffer.buf() + sym2.st_name;
                sym2.st_name = symbolNameBuffer.pushString(symname);
                int32_t eindex = externalSymbols.findFirst(sym2);
                if (eindex < 0) {
                    err.submit(ERR_INDEX_OUT_OF_RANGE); // should not occur
                    return;
                }
                *symp = symbolTranslate2[(uint32_t)eindex];
            }
        }
    }
    ElfFwcEhdr head;
    zeroAllMembers(head);
    modul.join(&head);
}

// count number of modules and libraries to reuse when relinking
void CLinker::countReusedModules() {
    uint32_t rec;                                          // record index
    const char * libname;                                  // library name
    const char * lastlibname = "";                         // library name of preceding record
    
    numRelinkObjects = 1;                                  // including modules1[0] which contains all reused non-relinkable modules
    numRelinkLibraries = 0;                                // number of relinkable modules
    // this is counted as one, even if unused
    if (cmd.job != CMDL_JOB_RELINK) return;                // not relinking

    // loop through relinkModules list
    for (rec = 0; rec < relinkModules.numEntries(); rec++) {
        if (relinkModules[rec].libraryName) {
            libname = cmd.getFilename(relinkModules[rec].libraryName);
            if (rec > 0 && strcmp(libname, lastlibname) == 0) {
                continue;  // multiple modules from same library: count only once
            }
            lastlibname = libname;
            numRelinkLibraries++;
        }
        else if (relinkModules[rec].moduleName) numRelinkObjects++;
    }
}

// get all relinked objects into modules1 metabuffer
void CLinker::getRelinkObjects() {
    uint32_t rec;                                          // record index
    uint32_t mod = 0;                                      // module index
    uint32_t sec = 0;                                      // section index
    const char * modname;                                  // module name
    // join all non-relinkable sections into first entry
    extractModule(modules1[0], 0, 0);
    mod++;
#if 1 // testing: write module file
    modules1[0].write("ff.ob");
#endif

    // mark all sections for fixed position because they have already been relocated
    for (sec = 0; sec < modules1[0].sectionHeaders.numEntries(); sec++) {
        modules1[0].sectionHeaders[sec].sh_flags |= SHF_FIXED;
    }
    if (cmd.verbose && numRelinkObjects > 1) {
        printf("\nReusing object modules:");
    }

    // loop through relinkModules list to search for non-library modules
    for (rec = 0; rec < relinkModules.numEntries(); rec++) {
        if (relinkModules[rec].libraryName == 0 && relinkModules[rec].moduleName != 0) {
            extractModule(modules1[mod], 0, relinkModules[rec].moduleName);
            modules1[mod].moduleName = relinkModules[rec].moduleName;
            modules1[mod].relinkable = true;
            extractModuleToFile(modules1[mod]);  // possibly extract to file
            mod++;
            // write name
            if (cmd.verbose) {
                modname = cmd.getFilename(relinkModules[rec].moduleName);
                printf(" %s", modname);
            }
        }
    }
}

// extract a module from relinkable file if requested
void CLinker::extractModuleToFile(CELF & modu) {
    // search for extract command
    if (!(cmd.libraryOptions & CMDL_LIBRARY_EXTRACTMEM)) return;

    uint32_t i;                                  // loop counter
    const char * modname1;                       // module name
    const char * modname2;                       // module name on command line
    bool extract = false;
    modname1 = cmd.getFilename(modu.moduleName);
    if (modname1[0] == 0) return;

    if (cmd.libraryOptions == CMDL_LIBRARY_EXTRACTALL) extract = true;
    for (i = 0; i < cmd.lcommands.numEntries(); i++) {
        if (cmd.lcommands[i].command == CMDL_LINK_EXTRACT) {        
            // name of module
            modname2 = cmd.getFilename(cmd.lcommands[i].filename);
            if (strcmp(modname1, modname2) == 0) {
                extract = true;
                break;
            }
        }
    }
    if (!extract) return;                        // no request for extracting this module
    // make new name = old name prefixed by "x_"
    uint32_t newname = cmd.fileNameBuffer.dataSize();
    cmd.fileNameBuffer.push("x_", 2);
    cmd.fileNameBuffer.pushString(modname1);
    modname2 = cmd.getFilename(newname);
    modu.write(modname2);
}

// recover relinkable library modules
void CLinker::getRelinkLibraries() {
    if (cmd.job != CMDL_JOB_RELINK) return;                // not relinking
    uint32_t rec;                                          // record index
    const char * libname;                                  // library name
    const char * nextlibname = "";                         // library name of next record
    const char * modname;                                  // module name
    uint32_t iLibrary = numLibraries + 1;                  // library index
    CELF modul;                                            // recovered library module

    if (cmd.verbose && numRelinkLibraries) {
        printf("\nRecovering library modules:");
    }

    // loop through relinkModules list, looking for library modules
    for (rec = 0; rec < relinkModules.numEntries(); rec++) {
        if (relinkModules[rec].libraryName && relinkModules[rec].moduleName) {
            libname = cmd.getFilename(relinkModules[rec].libraryName);
            modname = cmd.getFilename(relinkModules[rec].moduleName);
            if (cmd.verbose) {
                printf(" %s:%s", libname, modname);
            }
            // extract library module from relinkable input file
            modul.reset();
            extractModule(modul, relinkModules[rec].libraryName, relinkModules[rec].moduleName);
            modul.moduleName = relinkModules[rec].moduleName;
            
            modul.library = iLibrary; //??

            extractModuleToFile(modul);          // extract to file if requested
            // build internal library
            libraries[iLibrary].addELF(modul);
            if (rec + 1 < relinkModules.numEntries()) {
                nextlibname = cmd.getFilename(relinkModules[rec+1].libraryName);
            }
            else nextlibname = "?/";
            if (strcmp(libname, nextlibname) != 0) {
                // last module for this library. Finish internal library
                libraries[iLibrary].makeInternalLibrary();
                libraries[iLibrary].libraryName = relinkModules[rec].libraryName;
                libraries[iLibrary].relinkable = true;
                iLibrary++;
            }
        }
    }
}

// write feedback to console
void CLinker::feedBackText2() {
    if (!(cmd.verbose)) return;                  // write feedback only if verbose
    uint32_t i;                                  // loop counter
    const char * name;                           // name of file or module
    const char * libname;                        // name of library
    bool written = false;                        // message has been written
    
    // search for removed objects
    for (i = 0; i < rnames.numEntries(); i++) {
        if (((uint8_t)rnames[i].command & CMDL_LINK_REMOVE) && !((uint8_t)rnames[i].command & CMDL_LINK_ADDLIBRARY)) {
            if (!written) printf("\nRemoving object files:");
            written = true;
            name = cmd.getFilename(uint32_t(rnames[i].value));
            printf(" %s", name);
            if (!(rnames[i].command & CMD_NAME_FOUND)) printf(" failed!");
        }
    }
    // list of added objects have already been written to console.
    // write replaced and removed objects
    written = false;
    // search for replaced objects
    for (i = 0; i < rnames.numEntries(); i++) {
        if ((rnames[i].command & CMDL_LINK_ADDMODULE) && (rnames[i].command & CMD_NAME_FOUND)
            &&!(rnames[i].command & CMDL_LINK_REMOVE)) {
            if (!written) printf("\nReplacing object files:");
            written = true;
            name = cmd.getFilename(uint32_t(rnames[i].value));
            printf(" %s", name);
        }
    }
    written = false;
    // search for removed libraries
    for (i = 0; i < rnames.numEntries(); i++) {
        if ((uint8_t)rnames[i].command == (CMDL_LINK_REMOVE | CMDL_LINK_ADDLIBRARY)) {
            if (!written) printf("\nRemoving library files:");
            written = true;
            name = cmd.getFilename(rnames[i].filename);
            printf(" %s", name);
            if (!(rnames[i].command & CMD_NAME_FOUND)) printf(" failed!");
        }
    }
    written = false;
    // search for added libraries
    for (i = 0; i < rnames.numEntries(); i++) {
        if ((uint8_t)rnames[i].command == (CMDL_LINK_REPLACE | CMDL_LINK_ADDLIBRARY)
            && !(rnames[i].command & CMD_NAME_FOUND)) {
            if (!written) printf("\nAdding library files:");
            written = true;
            name = cmd.getFilename(rnames[i].filename);
            printf(" %s", name);
        }
    }
    written = false;
    // search for replaced libraries
    for (i = 0; i < rnames.numEntries(); i++) {
        if ((rnames[i].command & CMDL_LINK_REPLACE)
            && (rnames[i].command & CMDL_LINK_ADDLIBRARY)
            && (rnames[i].command & CMD_NAME_FOUND)) {
            if (!written) printf("\nReplacing library files:");
            written = true;
            name = cmd.getFilename(rnames[i].filename);
            printf(" %s", name);
        }
    }
    written = false;  
    // search for added library members
    for (i = 0; i < libmodules.numEntries(); i++) {
        uint32_t lib = libmodules[i].library & 0x7FFFFFFF;
        if (!written) printf("\nUsing library members:");
        written = true;
        libname = cmd.getFilename(libraries[lib].libraryName);
        name = libraries[lib].getMemberName(libmodules[i].offset);
        printf(" %s:%s", libname, name);
    }
}
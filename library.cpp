/****************************  library.cpp  **********************************
* Author:        Agner Fog
* date created:  2017-11-08
* Last modified: 2018-03-30
* Version:       1.10
* Project:       Binary tools for ForwardCom instruction set
* Description:
* This module contains code for reading, writing and manipulating function
* libraries (archives) of the UNIX type
*
* Copyright 2017-2020 GNU General Public License http://www.gnu.org/licenses
*****************************************************************************/

#include "stdafx.h" 

CLibrary::CLibrary() {
    // Constructor
    longNames = 0;
    alignBy = 8;
}

void CLibrary::go() {
    // Do to library whatever the command line says

    // check libraryOptions and whether an output file is specified
    if (cmd.fileOptions & CMDL_FILE_OUTPUT) {
        // Output is a library file
        if ((cmd.outputFile == 0) && !(cmd.libraryOptions & CMDL_LIBRARY_ADDMEMBER)) {
            err.submit(2503); // Output file name missing
            return;
        }
        // Check extension
        const char * outputFile = cmd.getFilename(cmd.outputFile);
        uint32_t len = (uint32_t)strlen(outputFile);
        if (len < 4 || strncasecmp_(outputFile + len - 3, ".li", 3) != 0) {
            err.submit(1101, outputFile); // Warning wrong extension
        }
    }
    if (err.number()) return;

    // strip path from member names and search for duplicates
    checkActionList();
    if (err.number()) return;

    if (dataSize()) {
        // library exists
        // check file type
        if (getFileType() != FILETYPE_LIBRARY) {
            err.submit(ERR_LIBRARY_FILE_TYPE, getFileFormatName(getFileType()));
            return;
        }
        // make list of member names and offsets
        makeMemberList();
        if (err.number()) return;
    }

    if (cmd.libraryOptions == CMDL_LIBRARY_LISTMEMBERS) {
        // list members. do nothing else
        listMembers();
        return;
    }

    // do all commands
    runActionList();
    if (err.number()) return;

    // collect contents of new library
    generateNewLibraryBody();
    if (err.number()) return;

    // Make library header, symbol table, longnames record, data
    makeBinaryFile();
    if (err.number()) return;

    if (cmd.fileOptions & CMDL_FILE_OUTPUT) {
        // Write output file
        const char * outputFileName = cmd.getFilename(cmd.outputFile);
        outFile.write(outputFileName);
    }
}

// Check action list for errors
void CLibrary::checkActionList() {
    uint32_t numCommands = cmd.lcommands.numEntries();   // number of commands on command line
    uint32_t i, j;                                         // loop counters
    const char * fname;                                    // name of object file
    for (i = 0; i < numCommands; i++) {                    // loop through commands
        if (cmd.lcommands[i].filename) {        
            fname = cmd.getFilename(cmd.lcommands[i].filename);
            // strip path from name
            const char * fname2 = removePath(fname);
            if (fname2 != fname) {
                // filename contains path. strip path
                cmd.lcommands[i].value = cmd.fileNameBuffer.pushString(fname2);
            }
            else {
                // filename does not contain path. membername is same as filename
                cmd.lcommands[i].value = cmd.lcommands[i].filename;
            }

            // search for duplicate names
            for (j = 0; j < i; j++) {
                if (strcmp(fname, cmd.getFilename((uint32_t)cmd.lcommands[j].value)) == 0) {
                    // duplicate name found
                    if (cmd.lcommands[j].command == CMDL_LIBRARY_DELETEMEMBER && cmd.lcommands[i].command == CMDL_LIBRARY_ADDMEMBER) {
                        // delete member before adding new member with same name
                        cmd.lcommands[j].command = 0;  // ignore delete
                    }
                    else {  // duplicate name not allowed
                        err.submit(ERR_DUPLICATE_NAME_COMMANDL, fname);  break;
                    } 
                }
            }
        }
    }
}

// Make list of library member names
void CLibrary::makeMemberList() {
    // Unix style library. First header at offset 8 = strlen(archiveSignature)
    uint32_t offset = 8;
    SUNIXLibraryHeader * header;       // member header
    uint32_t memberSize;               // size of member
    const char * memberName = 0;       // name of member
    longNames = 0;                     // no longnames record
    const char * namebuffer = (const char *)cmd.fileNameBuffer.buf(); // filenames buffer

    // Loop through library headers.
    while (offset + sizeof(SUNIXLibraryHeader) < dataSize()) {
        // Find header
        header = &get<SUNIXLibraryHeader>(offset);
        // Size of member
        memberSize = atoi(header->fileSize);
        if (memberSize < 0 || memberSize + offset + sizeof(SUNIXLibraryHeader) > dataSize()) {
            err.submit(ERR_LIBRARY_FILE_CORRUPT);  // Points outside file
            return;
        }
        // member name
        memberName = header->name;
        if (strncmp(memberName, "// ", 3) == 0) {
            // This is the long names member. Remember its position
            longNames = offset + sizeof(SUNIXLibraryHeader);
            longNamesSize = memberSize;
        }
        else if (strncasecmp_(memberName, "/SYMDEF SORTED/", 15) == 0) {
            // This is the symbol list
            //symDef = offset;
        }
        else {
            // Normal member. get name 
            if (memberName[0] == '/' && memberName[1] >= '0' && memberName[1] <= '9' && longNames) {
                // name contains index into longNames record
                uint32_t nameIndex = atoi(memberName+1);
                if (nameIndex < longNamesSize) {
                    memberName = (char*)buf() + longNames + nameIndex;
                }
                else {
                    memberName = "NoName!";
                }
            }
            else {
                // ordinary short name
                // name is terminated by '/'. Replace termination char by 0
                for (int i = 15; i >= 0; i--) {
                    if (header->name[i] == '/') {
                        header->name[i] = 0;  break;
                    }
                }
                // Terminate name with max length by overwriting date field, which we are not using
                header->date[0] = 0;
                memberName = header->name;
            }
            // check if name is valid
            if (memberName[0] == 0) memberName = "NoName!";
            
            // store member name and offset
            SLibMember libmem;
            libmem.name = cmd.fileNameBuffer.pushString(memberName);
            libmem.oldOffset = offset;
            libmem.newOffset = 0;
            libmem.size = memberSize;
            libmem.action = CMDL_LIBRARY_PRESERVEMEMBER;
            members.push(libmem);
        }
        // get next offset
        offset += sizeof(SUNIXLibraryHeader) + memberSize;
        
        // Round up for alignment. Nominal alignment = 8, max alignment = 256
        while ((offset & 0xFF) 
            && offset + sizeof(SUNIXLibraryHeader) < dataSize() 
            && get<uint8_t>(offset) <= ' ') offset++;
        //offset = (offset + alignBy - 1) & ~ alignBy;
    }
    // sort member list alphabetically
    members.sort();
    // check for duplicate names
    for (uint32_t j = 1; j < members.numEntries(); j++) {
        if (strcmp(namebuffer + members[j-1].name, namebuffer + members[j].name) == 0) {
            err.submit(ERR_DUPLICATE_NAME_IN_LIB, namebuffer + members[j].name);
            members[j].action = CMDL_LIBRARY_DELETEMEMBER;  // delete duplicate member
        }
    }
}

// Find a module. Return offset
uint32_t CLibrary::findMember(uint32_t name) {
    // make list of members
    if (members.numEntries() == 0) makeMemberList();
    // make record to search for
    SLibMember memberSearch;
    memberSearch.name = name;
    int32_t i = members.findFirst(memberSearch);
    if (i < 0) return 0;               // not found
    return members[i].oldOffset;       // return offset
}


// Run through commands from command line
void CLibrary::runActionList() {
    uint32_t i;                        // loop counter
    if (cmd.verbose) {
        // Tell name of library
        uint32_t name = cmd.inputFile;
        if (name == 0) name = cmd.outputFile;
        if (dataSize() == 0) {
            printf("\nBuilding ForwardCom library %s", cmd.getFilename(name));
        }
        else if (cmd.libraryOptions & (CMDL_LIBRARY_ADDMEMBER | CMDL_LIBRARY_DELETEMEMBER)) {
            printf("\nModifying ForwardCom library %s", cmd.getFilename(name));
        }
        else {        
            printf("\nForwardCom library %s", cmd.getFilename(name));
        }
    }
   
    // loop through commands
    for (i = 0; i < cmd.lcommands.numEntries(); i++) {
        uint32_t command = cmd.lcommands[i].command;
        switch (command) {
        case CMDL_LIBRARY_ADDMEMBER: // add object file to library
            addMember(cmd.lcommands[i].filename, (uint32_t)cmd.lcommands[i].value);
            break;
        case CMDL_LIBRARY_DELETEMEMBER:  // delete object file to library
            deleteMember((uint32_t)cmd.lcommands[i].value);
            break;
        case CMDL_LIBRARY_LISTMEMBERS:   // list library members
            listMembers();
            break;
        case CMDL_LIBRARY_EXTRACTMEM:  // Extract specified object file(s) from library
            extractMember(cmd.lcommands[i].filename, (uint32_t)cmd.lcommands[i].value);
            break;
        case CMDL_LIBRARY_EXTRACTALL:  // Extract all object files from library
            extractAllMembers();
            break;
        case 0:
            break;
        default:
            err.submit(ERR_UNKNOWN_OPTION, "?"); // should not occur
        }
    }
}

// Add object file to library member list
void CLibrary::addMember(uint32_t filename, uint32_t membername) {
    SLibMember libmem;
    libmem.name = membername;
    libmem.oldOffset = 0;
    libmem.newOffset = filename;   // store filename temporarily here
    libmem.action = CMDL_LIBRARY_ADDMEMBER;
    libmem.size = 0; 

    // replace existing member with same name
    int32_t m = members.findFirst(libmem);
    if (m >= 0) {
        // existing member with same name found
        members[m] = libmem;
        members[m].action = CMDL_LIBRARY_DELETEMEMBER | CMDL_LIBRARY_ADDMEMBER;
        if (cmd.verbose) {
            printf("\n  replacing member %s", cmd.getFilename(membername));
        }
    }
    else {
        if (cmd.verbose) {
            printf("\n  adding member %s", cmd.getFilename(membername));
        }
        members.addUnique(libmem);     // add to list. keep alphabetical order
    }
}

// Add delete member from library
void CLibrary::deleteMember(uint32_t membername) {
    SLibMember libmem;
    libmem.name = membername;
    int32_t m = members.findFirst(libmem);
    if (m < 0) {
        err.submit(ERR_MEMBER_NOT_FOUND_DEL, cmd.getFilename(membername));
        return;
    }
    members[m].action = CMDL_LIBRARY_DELETEMEMBER;
    if (cmd.verbose) {
        printf("\n  deleting member %s", cmd.getFilename(membername));
    }
}

// Extract member from library
void CLibrary::extractMember(uint32_t filename, uint32_t membername) {
    // find member
    SLibMember libmem;
    libmem.name = membername;
    int32_t m = members.findFirst(libmem);
    if (m < 0) {
        err.submit(ERR_MEMBER_NOT_FOUND_EXTRACT, cmd.getFilename(membername));
        return;
    }
    uint32_t headerOffset = members[m].oldOffset;
    //uint32_t memberSize = (uint32_t)atoi(header->fileSize);
    uint32_t memberSize = members[m].size;
    if (memberSize + headerOffset + sizeof(SUNIXLibraryHeader) > dataSize()) {
        err.submit(ERR_LIBRARY_FILE_CORRUPT);  // Points outside file
        return;
    }
    // file name
    if (filename == 0) filename = membername;
    const char * filenm = cmd.getFilename(filename);

    // Tell what we are doing
    if (cmd.verbose) {
        if (filename == membername) {
            printf("\nExtracting file %s from library", filenm);
        }
        else {
            printf("\nExtracting library member %s to file %s", 
                cmd.getFilename(membername), filenm);
        }
    }
    // extract file
    CFileBuffer memberbuf;
    memberbuf.push(buf() + headerOffset + sizeof(SUNIXLibraryHeader), memberSize);
    // write file
    memberbuf.write(filenm);
}

// Extract all members from library
void CLibrary::extractAllMembers() {
    uint32_t num = members.numEntries();
    if (num == 0) {
        err.submit(ERR_MEMBER_NOT_FOUND_EXTRACT, "");
    }
    for (uint32_t i = 0; i < num; i++) {
        extractMember(members[i].name, members[i].name);
    }
}

// List all library members
void CLibrary::listMembers() {
    CDynamicArray<SSymbolEntry> symbolList;      // symbol list

    printf("\nMembers of library %s:", cmd.getFilename(cmd.inputFile));
    uint32_t m, i;                        // loop counters
    for (m = 0; m < members.numEntries(); m++) {
        if (members[m].name == 0) continue;  // has been deleted
        // print member name
        if (cmd.verbose < 2) {
            printf("\n  %s", cmd.getFilename(members[m].name));
        }
        else if (cmd.verbose >= 2) {
            printf("\n  %s export:", cmd.getFilename(members[m].name));
            // print exported symbol names for this member
            // clear buffers
            memberBuffer.setSize(0);  symbolNameBuffer.setSize(0);  symbolList.setSize(0);
            // put member into buffer in order to extract symbols
            memberBuffer.push(buf() + members[m].oldOffset + (uint32_t)sizeof(SUNIXLibraryHeader), members[m].size);
            // extract symbols from ELF file            
            memberBuffer.listSymbols(&symbolNameBuffer, &symbolList, m, 0, 1);
            // sort alphabetically
            symbolList.sort();
            // list names
            for (i = 0; i < symbolList.numEntries(); i++) {
                printf("\n      %s", symbolNameBuffer.getString(symbolList[i].name));
            }
        }
        if (cmd.verbose >= 3) {
            // print imported symbol names for this member
            printf("\n    import:");
            // clear buffers
            memberBuffer.setSize(0);  symbolNameBuffer.setSize(0);  symbolList.setSize(0);
            // put member into buffer in order to extract symbols
            memberBuffer.push(buf() + members[m].oldOffset + (uint32_t)sizeof(SUNIXLibraryHeader), members[m].size);
            // extract symbols from ELF file            
            memberBuffer.listSymbols(&symbolNameBuffer, &symbolList, m, 0, 2);
            // sort alphabetically
            symbolList.sort();
            // list names
            for (i = 0; i < symbolList.numEntries(); i++) {
                printf("\n      %s", symbolNameBuffer.getString(symbolList[i].name));
            }
        }
    }
}


// Generate data contents of new library from old one with possible additions and deletions
void CLibrary::generateNewLibraryBody() {
    uint32_t m;                        // member number
    SUNIXLibraryHeader header;         // new member header

    // loop through member list
    for (m = 0; m < members.numEntries(); m++) {
        if (members[m].name && members[m].action != 0 && members[m].action != CMDL_LIBRARY_DELETEMEMBER) {
            if (members[m].oldOffset && members[m].action == CMDL_LIBRARY_PRESERVEMEMBER) {
                // preserve existing member
                SUNIXLibraryHeader * phead = &get<SUNIXLibraryHeader>(members[m].oldOffset);
                uint32_t size = atoi(phead->fileSize);
                if (sizeof(SUNIXLibraryHeader) + size + members[m].oldOffset > dataSize()) {
                    err.submit(ERR_LIBRARY_FILE_CORRUPT);  return;
                }
                // put header and file into buffer
                members[m].newOffset = dataBuffer.push(buf() + members[m].oldOffset, size + (uint32_t)sizeof(SUNIXLibraryHeader));
            }
            else if (members[m].action & CMDL_LIBRARY_ADDMEMBER) {
                // get member from file
                // filename is temporarily stored in newOffset
                if (members[m].newOffset == 0 || (members[m].newOffset >= cmd.fileNameBuffer.dataSize())) {
                    members[m].newOffset = members[m].name;
                }                
                const char * filename = cmd.getFilename(members[m].newOffset);
                memberBuffer.setSize(0);  // clear memberBuffer first
                memberBuffer.read(filename);
                if (err.number()) return;
                // check file type
                if (memberBuffer.getFileType() != FILETYPE_FWC) {
                    err.submit(ERR_LIBRARY_MEMBER_TYPE, filename, getFileFormatName(memberBuffer.getFileType()));
                    return;
                }
                // remove path from file name
                const char * membername = removePath(filename);
                // make member header
                memset(&header, ' ', sizeof(header));
                // date
                sprintf(header.date, "%llu ", (unsigned long long)time(0));
                // User and group id
                header.userID[0] = '0';
                header.groupID[0] = '0';
                // File mode
                memcpy(header.fileMode, "100666", 6);
                // Name
                uint32_t namelength = (uint32_t)strlen(membername);
                if (namelength < 16) {
                    memcpy(header.name, membername, namelength);                        
                    header.name[namelength] = '/';  // terminate with '/'
                }
                else {
                    // cannot save name now, wait until longnames record is made
                    members[m].name = cmd.fileNameBuffer.pushString(membername);
                }
                // Size
                sprintf(header.fileSize, "%u", memberBuffer.dataSize());
                members[m].size = memberBuffer.dataSize();
                // End
                header.headerEnd[0] = '`';  header.headerEnd[1] = '\n';  
                // remove terminating zeroes left by sprintf
                char * p = (char *)&header;
                for (uint32_t i = 0; i < sizeof(header); i++) {
                    if (p[i] == 0) p[i] = ' ';
                }
                // put header and file into buffer
                members[m].newOffset = dataBuffer.push(&header, (uint32_t)sizeof(header));
                dataBuffer.push(memberBuffer.buf(), memberBuffer.dataSize());
            }
        }
        // align next member
        dataBuffer.align(alignBy);
    }
}

// Make library header, symbol table, longnames record, data
void CLibrary::makeBinaryFile() {
    // make signature
    outFile.push(archiveSignature, 8);

    // make symbol list and longnames record
    CMemoryBuffer longNamesBuf;                  // list of long member names
    CDynamicArray<SSymbolEntry> symbolList;      // symbol list, part of symdefSorted
    uint32_t m;                                  // member number
    uint32_t i;                                  // loop counter

    // loop through members to generate longnames and symbol list
    for (m = 0; m < members.numEntries(); m++) {
        if (members[m].action & (CMDL_LIBRARY_PRESERVEMEMBER | CMDL_LIBRARY_ADDMEMBER)) {
            // get member header
            SUNIXLibraryHeader * phead = &dataBuffer.get<SUNIXLibraryHeader>(members[m].newOffset);
            // member name
            const char * name = cmd.getFilename(members[m].name);
            if (strlen(name) > 15) {
                // put name in longnames record
                uint32_t longnameos = longNamesBuf.pushString(name);
                sprintf(phead->name, "/%u", longnameos);
                phead->name[strlen(phead->name)] = ' '; // remove terminating zero
            }
            // put member into buffer in order to extract symbols
            memberBuffer.setSize(0);
            memberBuffer.push(dataBuffer.buf() + members[m].newOffset + (uint32_t)sizeof(SUNIXLibraryHeader), members[m].size);
            // extract public symbols from ELF file
            memberBuffer.listSymbols(&symbolNameBuffer, &symbolList, m, 0, 1);
        }
    }

    // sort symbol list and check for duplicate symbol names
    checkDuplicateSymbols(symbolList);

    // calculate size of symbol list
    uint32_t symbolListSize = (uint32_t)sizeof(SUNIXLibraryHeader) + symbolList.numEntries()*8 + 8 + symbolNameBuffer.dataSize();
    symbolListSize = (symbolListSize + alignBy - 1) & - alignBy;  // align
    // calculate size of longnames record
    uint32_t longnamesSize = 0;
    if (longNamesBuf.dataSize() > 1) {              // longnames record needed
        longnamesSize = sizeof(SUNIXLibraryHeader) + longNamesBuf.dataSize();
        longnamesSize = (longnamesSize + alignBy - 1) & - alignBy;  // align
    }
    // offset to first normal member
    uint32_t firstMemberOffset = 8 + symbolListSize + longnamesSize;

    // make Mach-O style symbol list
    // put member addresses into symbol list
    for (i = 0; i < symbolList.numEntries(); i++) {
        m = symbolList[i].member;
        if (m < members.numEntries()) {
            symbolList[i].member = members[m].newOffset + firstMemberOffset;
        }
        else symbolList[i].member = 0;  // should not occur
    }
    // make header
    SUNIXLibraryHeader header;
    memset(&header, ' ', sizeof(header));
    memcpy(header.name, "/SYMDEF SORTED/", 15); 
    sprintf(header.date, "%llu ", (unsigned long long)time(0));
    header.userID[0] = '0';
    header.groupID[0] = '0';
    memcpy(header.fileMode, "100666", 6);
    sprintf(header.fileSize, "%u", symbolListSize - (uint32_t)sizeof(SUNIXLibraryHeader));
    header.headerEnd[0] = '`';  header.headerEnd[1] = '\n';
    // remove terminating zeroes
    char * p = (char*)&header;
    for (i = 0; i < sizeof(header); i++) {
        if (p[i] == 0) p[i] = ' ';
    }
    // save header
    outFile.push(&header, (uint32_t)sizeof(header));
    uint32_t n = symbolList.numEntries()*8;  // length of list
    outFile.push(&n, 4);
    // symbol list
    for (i = 0; i < symbolList.numEntries(); i++) {
        outFile.push(&symbolList[i].name, 4);            
        outFile.push(&symbolList[i].member, 4);
    }
    // length of string table
    n = symbolNameBuffer.dataSize();
    outFile.push(&n, 4);
    // string table
    outFile.push(symbolNameBuffer.buf(), n);
    // align
    outFile.align(alignBy);

    // Make longnames record if needed
    if (longnamesSize) {
        memcpy(header.name, "//              ", 16);
        sprintf(header.fileSize, "%u", longNamesBuf.dataSize());
        header.fileSize[strlen(header.fileSize)] = ' ';  // remove terminating zero
        outFile.push(&header, (uint32_t)sizeof(header));
        outFile.push(longNamesBuf.buf(), longNamesBuf.dataSize());
        outFile.align(alignBy);
    }

    // Insert all regular members
    outFile.push(dataBuffer.buf(), dataBuffer.dataSize());
}

// Check if symbollist contains duplicate names
void CLibrary::checkDuplicateSymbols(CDynamicArray<SSymbolEntry> & symbolList) {
    uint32_t i, j;                     // loop counters

    // sort symbol list
    symbolList.sort();

    // check symbol list for duplicates
    for (i = 1; i < symbolList.numEntries(); i++) {
        if (symbolList[i-1] == symbolList[i] && !(symbolList[i-1].st_bind & STB_WEAK) && !(symbolList[i].st_bind & STB_WEAK)) {
            // make a list of modules containing this symbol name
            CMemoryBuffer moduleNames;
            j = i - 1;
            while (j < symbolList.numEntries() && symbolList[j] == symbolList[i]) {
                if (j >= i) moduleNames.push(", ", 2);
                uint32_t m = symbolList[j].member;
                const char * mname = "?";
                if (m < members.numEntries()) {
                    mname = cmd.getFilename(members[m].name);
                }
                moduleNames.push(mname, (uint32_t)strlen(mname));
                j++;
            }
            moduleNames.pushString("");
            const char * symbolname = symbolNameBuffer.getString(symbolList[i].name);
            err.submit(ERR_DUPLICATE_SYMBOL_IN_LIB, symbolname, (char*)moduleNames.buf());
            i = j - 1;
        }
    }
}

// get name of a library member
const char * CLibrary::getMemberName(uint32_t memberOffset) {
    if (memberOffset >= dataSize()) return "unknown?";
    char * namebuf = (char *)cmd.fileNameBuffer.buf();
    SUNIXLibraryHeader * phead = (SUNIXLibraryHeader *)(buf() + memberOffset);
    if (phead->name[0] == '/') {
        // long name
        uint32_t namei = atoi(phead->name+1);
        if (longNames == 0) findLongNames();
        // offset into longNames
        uint32_t os = longNames + namei;
        if (longNames == 0 || namei >= longNamesSize) return "unknown?";
        return (char*)buf() + os;
    }
    // short name. replace terminating '/' by zero
    uint32_t nm = cmd.fileNameBuffer.push(phead->name, 16);
    namebuf += nm;
    for (int i = 0; i < 16; i++) {
        if (namebuf[i] == '/') namebuf[i] = 0;
    }
    namebuf[15] = 0;   // make sure string ends even if '/' is missing
    return namebuf;
}

// get size of a library member
uint32_t CLibrary::getMemberSize(uint32_t memberOffset) {
    if (memberOffset >= dataSize()) return 0;
    SUNIXLibraryHeader * phead = (SUNIXLibraryHeader *)(buf() + memberOffset);
    uint32_t size = atoi(phead->fileSize);
    return size;
}    

// Find longNames record
void CLibrary::findLongNames() {
    // find longNames record
    uint32_t offset = 8;
    SUNIXLibraryHeader * header;
    uint32_t memberSize;
    // Loop through library headers.
    while (offset + sizeof(SUNIXLibraryHeader) < dataSize()) {
        // Find header
        header = &get<SUNIXLibraryHeader>(offset);
        // Size of member
        memberSize = atoi(header->fileSize);
        if (memberSize < 0 || memberSize + offset + sizeof(SUNIXLibraryHeader) > dataSize()) {
            err.submit(ERR_LIBRARY_FILE_CORRUPT);  // Points outside file
            return;
        }
        // member name
        if (strncmp(header->name, "// ", 3) == 0) {
            // This is the long names member. Remember its position
            longNames = offset + sizeof(SUNIXLibraryHeader);
            longNamesSize = memberSize;
            return;
        }
        // get next offset
        offset += sizeof(SUNIXLibraryHeader) + memberSize;

        // Round up for alignment. Nominal alignment = 4, max alignment = 256
        while ((offset & 0xFF) 
            && offset + sizeof(SUNIXLibraryHeader) < dataSize() 
            && get<uint8_t>(offset) <= ' ') offset++;
    }
}


// Find exported symbol in library
// The return value is a file offset to the library member containing the symbol, 
// or zero if not found
uint32_t CLibrary::findSymbol(const char * name) {
    uint32_t memberSize;
    uint32_t offset = 8;
    SUNIXLibraryHeader * symdefHead = (SUNIXLibraryHeader *)(buf() + offset);
    // expect symbol table as first record    
    while (strncasecmp_(symdefHead->name, "/SYMDEF SORTED/", 15) != 0) {
        // not found. search whole library
        memberSize = atoi(symdefHead->fileSize);
        offset += memberSize + (uint32_t)sizeof(SUNIXLibraryHeader);
        if (offset + (uint32_t)sizeof(SUNIXLibraryHeader) >= dataSize()) {
            err.submit(ERR_NO_SYMTAB_IN_LIB); // library has no correct symbol table
            return 0;
        }
        symdefHead = (SUNIXLibraryHeader *)(buf() + offset);
    }
    memberSize = atoi(symdefHead->fileSize);
    // pointer to start of list
    offset += (uint32_t)sizeof(SUNIXLibraryHeader);
    uint32_t * listp = (uint32_t *)(buf() + offset);
    uint32_t symlistlen = listp[0];    // length of symbol list
    // string table
    char * stringtab = (char *)(buf() + offset + symlistlen + 8);
    // string table length
    uint32_t stringtablen = *(uint32_t *)(stringtab-4);
    // check integrity
    if ((uint64_t)symlistlen + stringtablen + 8 > memberSize) {
        err.submit(ERR_ELF_STRING_TABLE);  return 0;
    }
    // binary search for name 
    uint32_t a = 0;                                    // start of search interval
    uint32_t b = symlistlen >> 3;                      // number of symbols
    uint32_t c = 0;                                    // middle of search interval                                                     
    uint32_t nameindex;                                // index into string table
    while (a < b) {                                    // binary search loop:
        c = (a + b) / 2;
        nameindex = listp[1+2*c];                      // index to name of symbol number c
        if (nameindex >= stringtablen) {               // check integrity
            err.submit(ERR_ELF_STRING_TABLE);  return 0;
        }
        if (strcmp(stringtab + nameindex, name) < 0) { // compare strings       
            a = c + 1;
        }
        else {
            b = c;
        }
    }
    nameindex = listp[1+2*a];                          // index to name of symbol
    if (a == symlistlen >> 3 || strcmp(stringtab + nameindex, name)) {
        return 0;                                      // not found
    }
    uint32_t memberIndex = listp[2+2*a];               // index to member containing symbol
    if (memberIndex + (uint32_t)sizeof(SUNIXLibraryHeader) > dataSize()) {
        err.submit(ERR_LIBRARY_FILE_CORRUPT);          // out of range
        return 0;
    }
    return memberIndex;
}

// check if this is a ForwardCom library
bool CLibrary::isForwardCom() {
    uint32_t offset = 8;
    SUNIXLibraryHeader * symdefHead = (SUNIXLibraryHeader *)(buf() + offset);
    // expect symbol table as first record    
    return (strncasecmp_(symdefHead->name, "/SYMDEF SORTED/", 15) == 0);
}


// remove path from file name
const char * removePath(const char * filename) {
    int p;
    const char pathsep1 = '/';                     // separator in file path
#if defined (_WIN32) || defined (__WINDOWS__)
    const char pathsep2 = '\\', pathsep3 = ':';    // additional separators in Windows
#else
    const char pathsep2 = pathsep1, pathsep3 = pathsep1;
#endif
    // find last '/' or other path separator in object file name
    for (p = (int)strlen(filename) - 1; p >= 0; p--) {
        if (filename[p] == pathsep1 || filename[p] == pathsep2 || filename[p] == pathsep3) break;
    }
    if (p >= 0) {
        // filename contains path. strip path
        filename += p + 1;
    }
    if (filename[0] == 0) filename = "unknown?";
    return filename;
}

// make library from CELF modules during relinking
void CLibrary::addELF(CELF & elf) {
    SLibMember member;                           // entry into members list
    SUNIXLibraryHeader header;                   // new member header
    zeroAllMembers(member);
    zeroAllMembers(header);
    member.name = elf.moduleName;
    member.action = CMDL_LIBRARY_ADDMEMBER;
    member.size = elf.dataSize();
    sprintf(header.fileSize, "%u", elf.dataSize());
    const char * membername = cmd.getFilename(elf.moduleName);
    uint32_t namelength = (uint32_t)strlen(membername);
    if (namelength < 16) {
        memcpy(header.name, membername, namelength);                        
        header.name[namelength] = '/';  // terminate with '/'
    }
    // put header and file into buffer
    member.newOffset = dataBuffer.push(&header, (uint32_t)sizeof(header));
    dataBuffer.push(elf.buf(), elf.dataSize());
    members.push(member);
}

// make a library for internal use during relinking
void CLibrary::makeInternalLibrary() {
    makeBinaryFile();                            // make library file, but don't write to disk
    *this << outFile;                            // transfer to own object as if it had been read from disk
    members.setSize(0);                          // reset members list
    makeMemberList();                            // update internal list
}

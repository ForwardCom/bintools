/****************************  disasm1.cpp   ********************************
* Author:        Agner Fog
* Date created:  2017-04-26
* Last modified: 2021-03-30
* Version:       1.11
* Project:       Binary tools for ForwardCom instruction set
* Module:        disassem.h
* Description:   Disassembler
* Disassembler for ForwardCom
*
* Copyright 2007-2021 GNU General Public License http://www.gnu.org/licenses
*****************************************************************************/
#include "stdafx.h"


uint64_t interpretTemplateVariants(const char * s) {
    // Interpret template variants in instruction record
    // The return value is a combination of bits for each variant option
    // These bits are defined as constants VARIANT_D0, etc., in disassem.h
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) {          // Loop through string
        char c = toupper(s[i]), d = toupper(s[i+1]);
        switch (c) {
        case 0:
            return v;                      // End of string
        case 'D': 
            if (d == '0') v |= VARIANT_D0; // D0
            if (d == '1') v |= VARIANT_D1; // D1
            if (d == '2') v |= VARIANT_D2; // D2
            if (d == '3') v |= VARIANT_D3; // D3
            continue;
        case 'F': 
            if (d == '0') v |= VARIANT_F0; // F0
            if (d == '1') v |= VARIANT_F1; // F1
            continue;            
        case 'M':
            if (d == '0') v |= VARIANT_M0; // M0
            //if (d == '1') v |= VARIANT_M1; // M1. No longer used
            continue;
        case 'R':
            if (d == '0') v |= VARIANT_R0; // R0
            if (d == '1') v |= VARIANT_R1; // R1
            if (d == '2') v |= VARIANT_R2; // R2
            if (d == '3') v |= VARIANT_R3; // R3
            if (d == 'L') v |= VARIANT_RL; // RL
            i++;
            continue;
        case 'I':
            if (d == '2') v |= VARIANT_I2; // I2
            continue;

        case 'O':
            if (d > '0' && d < '7') v |= (d - '0') << 24;    // O1 - O6
            continue;
        case 'U':
            if (d == '0') v |= VARIANT_U0; // U0
            if (d == '3') v |= VARIANT_U3; // U3
            continue;
        case 'H':
            if (d == '0') v |= VARIANT_H0; // H0
            continue;
        case 'X':
            v |= uint64_t(((d-'0') & 0xF) | 0x10) << 32; // X0 - X9
            continue;
        case 'Y':
            v |= uint64_t(((d-'0') & 0xF) | 0x20) << 32; // Y0 - Y9
            continue;
        }
    }
    return v;
}


void CDisassembler::sortSymbolsAndRelocations() {
    // Sort symbols by address. This is useful when symbol labels are written out
    uint32_t i;                                            // loop counter
    // The values of st_reguse1 and st_reguse2 are no longer needed after these values have been written out.
    // Save old index in st_reguse1. 
    // Set st_reguse2 to zero, it is used later for data type

    for (i = 0; i < symbols.numEntries(); i++) {
        symbols[i].st_reguse1 = i;
        symbols[i].st_reguse2 = 0;
        // symbols are grouped by section in object files, by base pointer in executable files
        if (isExecutable) symbolExeAddress(symbols[i]);
    }
    // Sort symbols by address
    symbols.sort();

    // Add dummy empty symbol number 0
    ElfFwcSym nulsymbol = {0,0,0,0,0,0,0,0,0};
    symbols.addUnique(nulsymbol);

    // Update all relocations to the new symbol indexes
    // Translate old to new symbol index in all relocation records
    // Allocate array for translating  old to new symbol index
    CDynamicArray<uint32_t> old2newSymbolIndex;
    old2newSymbolIndex.setNum(symbols.numEntries());

    // Make translation table
    for (i = 0; i < symbols.numEntries(); i++) {
        uint32_t oldindex = symbols[i].st_reguse1;
        if (oldindex < symbols.numEntries()) {
            old2newSymbolIndex[oldindex] = i;
        }
    }

    // Translate all symbol indices in relocation records
    for (i = 0; i < relocations.numEntries(); i++) {
        if (relocations[i].r_sym < old2newSymbolIndex.numEntries()) {
            relocations[i].r_sym = old2newSymbolIndex[relocations[i].r_sym];            
        }
        else relocations[i].r_sym = 0; // index out of range!
        if ((relocations[i].r_type & R_FORW_RELTYPEMASK) == R_FORW_REFP) {
            // relocation record has an additional reference point
            // bit 30 indicates relocation used OK
            uint32_t refsym = relocations[i].r_refsym & ~0x40000000;
            if (refsym < old2newSymbolIndex.numEntries()) {
                relocations[i].r_refsym = old2newSymbolIndex[refsym] | (relocations[i].r_refsym & 0x40000000);
            }
            else relocations[i].r_refsym = 0; // index out of range
        }
    }

    // Sort relocations by address
    relocations.sort();
}

// Translate symbol address from section:offset to pointerbase:address
void CDisassembler::symbolExeAddress(ElfFwcSym & sym) {
    // use this translation only when disassembling executable files
    if (!isExecutable) return;

    // section
    uint32_t sec =  sym.st_section;
    if (sec && sec < sectionHeaders.numEntries()) {
        uint32_t flags = (uint32_t)sectionHeaders[sec].sh_flags;
        // get base pointer
        switch (flags & SHF_BASEPOINTER) {
        case SHF_IP:
            sym.st_section = 1;  break;
        case SHF_DATAP:
            sym.st_section = 2;  break;
        case SHF_THREADP:
            sym.st_section = 3;  break;
        default:
            sym.st_section = 0;  break;
        }
        sym.st_value += sectionHeaders[sec].sh_addr;
    }
}


// Join the tables: symbols and newSymbols
void CDisassembler::joinSymbolTables() {
    /* There are two symbol tables: 'symbols' and 'newSymbols'.
    'symbols' contains the symbols that were in the original file. This table is sorted
    by address in sortSymbolsAndRelocations() in order to make it easy to find a symbol
    at a given address.
    'newSymbols' contains new symbols that were created during pass 1. It is not sorted.
    The reason why we have two symbol tables is that the symbol indexes would change if 
    we add to the 'symbols' table during pass 1 and keep it sorted. We need to have 
    consistent indexes during pass 1 in order to access symbols by their index. Likewise,
    'newSymbols' is not sorted because indexes would change when new symbols are added to it.
    'newSymbols' may contain dublets because it is not sorted so dublets are not detected
    when new symbols are added.
    'joinSymbolTables()' is called after pass 1 when we are finished making new symbols.
    This function joins the two tables together, removes any dublets, updates symbol indexes
    in all relocation records, and tranfers data type information from relocation records
    to symbol records.
    */
    uint32_t r;                                            // Relocation index
    uint32_t s;                                            // Symbol index
    uint32_t newsymi;                                      // Symbol index in newSymbols
    uint32_t newsymi2;                                     // Index of new symbol after transfer to symbols table
    uint32_t symTempIndex = symbols.numEntries();       // Temporary index of symbol after transfer

    // Remember index of each symbol before adding new symbols and reordering
    for (s = 0; s < symbols.numEntries(); s++) {
        symbols[s].st_reguse1 = s;
    }

    // Loop through relocations to find references to new symbols
    for (r = 0; r < relocations.numEntries(); r++) {
        if (relocations[r].r_sym & 0x80000000) {           // Refers to newSymbols table
            newsymi = relocations[r].r_sym & ~0x80000000;
            if (newsymi < newSymbols.numEntries()) {            
                // Put symbol into old table if no equivalent symbol exists here
                newsymi2 = symbols.addUnique(newSymbols[newsymi]);
                // Give it a temporary index if it doesn't have one
                if (symbols[newsymi2].st_reguse1 == 0) symbols[newsymi2].st_reguse1 = symTempIndex++;
                // update reference in relocation record to temporary index
                relocations[r].r_sym = symbols[newsymi2].st_reguse1;
            }
        }
        // Do the same with any reference point
        if ((relocations[r].r_type & R_FORW_RELTYPEMASK) == R_FORW_REFP && relocations[r].r_refsym & 0x80000000) {
            newsymi = relocations[r].r_refsym & ~0xC0000000;
            if (newsymi < newSymbols.numEntries()) {            
                // Put symbol into old table if no equivalent symbol exists here
                newsymi2 = symbols.addUnique(newSymbols[newsymi]);
                // Give it a temporary index if it doesn't have one
                if (symbols[newsymi2].st_reguse1 == 0) symbols[newsymi2].st_reguse1 = symTempIndex++;
                // update reference in relocation record to temporary index
                relocations[r].r_refsym = symbols[newsymi2].st_reguse1 | (relocations[r].r_refsym & 0x40000000);
            }
        }
    }
    // Make symbol index translation table
    CDynamicArray<uint32_t> old2newSymbolIndex;
    old2newSymbolIndex.setNum(symbols.numEntries());
    for (s = 0; s < symbols.numEntries(); s++) {
        uint32_t oldsymi = symbols[s].st_reguse1;
        if (oldsymi < old2newSymbolIndex.numEntries()) {
            old2newSymbolIndex[oldsymi] = s;
        }
    }
    // Update indexes in relocation records
    for (r = 0; r < relocations.numEntries(); r++) {
        if (relocations[r].r_sym < old2newSymbolIndex.numEntries()) { // Refers to newSymbols table
            relocations[r].r_sym = old2newSymbolIndex[relocations[r].r_sym];
            // Give the symbol a data type from relocation record if it doesn't have one
            if (symbols[relocations[r].r_sym].st_reguse2 == 0) {
                symbols[relocations[r].r_sym].st_reguse2 = relocations[r].r_type >> 8;
            }
        }
        // Do the same with any reference point
        uint32_t refsym = relocations[r].r_refsym & ~0xC0000000;
        if ((relocations[r].r_type & R_FORW_RELTYPEMASK) == R_FORW_REFP && refsym < old2newSymbolIndex.numEntries()) {
            relocations[r].r_refsym = old2newSymbolIndex[refsym] | (relocations[r].r_refsym & 0x40000000);
        }
    }
}


void CDisassembler::assignSymbolNames() {
    // Assign names to symbols that do not have a name
    uint32_t i;                                            // New symbol index
    uint32_t numDigits;                                    // Number of digits in new symbol names
    char name[64];                                         // sectionBuffer for making symbol name
    static char format[64];
    uint32_t unnamedNum = 0;                               // Number of unnamed symbols
    //uint32_t addMoreSymbols = 0;                           // More symbols need to be added

    // Find necessary number of digits
    numDigits = 3; i = symbols.numEntries();
    while (i >= 1000) {
        i /= 10;
        numDigits++;
    }

    // format string for symbol names
    sprintf(format, "%s%c0%i%c", "@_", '%', numDigits, 'i');

    // Loop through symbols
    for (i = 1; i < symbols.numEntries(); i++) {
        if (symbols[i].st_name == 0 ) {
            // Symbol has no name. Make one
            sprintf(name, format, ++unnamedNum);
            // Store new name
            symbols[i].st_name = stringBuffer.pushString(name);
        }
    }

#if 0  //!!
    // For debugging: list all symbols
    printf("\n\nSymbols:");
    for (i = 0; i < symbols.numEntries(); i++) {
        printf("\n%3X %3X %s sect %i offset %X type %X size %i Scope %i", 
            i, symbols[i].st_name, stringBuffer.buf() +  symbols[i].st_name,
            symbols[i].st_section, (uint32_t)symbols[i].st_value, symbols[i].st_type, 
            (uint32_t)symbols[i].st_unitsize, symbols[i].st_other);
        if (symbols[i].st_reguse2) printf(" Type %X", symbols[i].st_reguse2);
    }
#endif
#if 0
    // For debugging: list all relocations
    printf("\n\nRelocations:");
    for (uint32_t i = 0; i < relocations.numEntries(); i++) {
        printf("\nsect %i, os %X, type %X, sym %i, add %X, refsym %X",
            (uint32_t)(relocations[i].r_section), (uint32_t)relocations[i].r_offset, relocations[i].r_type, 
            relocations[i].r_sym, relocations[i].r_addend, relocations[i].r_refsym);
    }
#endif
}



/**************************  class CDisassembler  *****************************
Members of class CDisassembler
Members that relate to file output are in disasm2.cpp
******************************************************************************/

CDisassembler::CDisassembler() {
    // Constructor. Initialize variables
    pass = 0;
    nextSymbol = 0;
    currentFunction = 0;
    currentFunctionEnd = 0;
    debugMode = 0;
    outputFile = cmd.outputFile;
    checkFormatListIntegrity();
};

void CDisassembler::initializeInstructionList() {
    // Read and initialize instruction list and sort it by category, format, and op1
    CCSVFile instructionListFile;
    instructionListFile.read(cmd.getFilename(cmd.instructionListFile), CMDL_FILE_SEARCH_PATH);  // Filename of list of instructions
    instructionListFile.parse();                 // Read and interpret instruction list file
    instructionlist << instructionListFile.instructionlist; // Transfer instruction list to my own container
    instructionlist.sort();                      // Sort list, using sort order defined by SInstruction2
}

// Read instruction list, split ELF file into components
void CDisassembler::getComponents1() {
    // Check code integrity
    checkFormatListIntegrity();

    // Read instruction list
    initializeInstructionList();

    // Split ELF file into containers
    split();
}

// Read instruction list, get ELF components for assembler output listing
void CDisassembler::getComponents2(CELF const & assembler, CMemoryBuffer const & instructList) {
    // This function replaces getComponents1() when making an output listing for the assembler
    // list file name from command line

    // copy containers from assembler outFile
    sectionHeaders.copy(assembler.getSectionHeaders());
    symbols.copy(assembler.getSymbols());
    relocations.copy(assembler.getRelocations());
    stringBuffer.copy(assembler.getStringBuffer());
    dataBuffer.copy(assembler.getDataBuffer());
    // Copy instruction list from assembler to avoid reading the csv file again.
    // Use the unsorted list to make sure the preferred name for an instuction comes first, in case there are alias names
    instructionlist.copy(instructList);
    instructionlist.sort();  // Sort list, using the sort order needed by the disassembler as defined by SInstruction2
}


// Do the disassembly
void CDisassembler::go() {
    // set tabulator stops
    setTabStops();

    // write feedback to console
    feedBackText1();

    // is this an executable or object file
    isExecutable = fileHeader.e_type == ET_EXEC;

    // Begin writing output file
    writeFileBegin();

    // Sort symbols by address
    sortSymbolsAndRelocations();

    // pass 1: Find symbols types and unnamed symbols
    pass = 1;
    pass1();

    if (pass & 0x10) {
        // Repetition of pass 1 requested
        pass = 2;
        pass1();
    }

    // Join the tables: symbols and newSymbols;
    joinSymbolTables();

    // put names on unnamed symbols
    assignSymbolNames();

    // pass 2: Write all sections to output file
    pass = 0x100;
    pass2();

    // Check for illegal entries in symbol table and relocations table
    finalErrorCheck();

    // Finish writing output file
    writeFileEnd();

    // write output file
    if (outputFile && !debugMode) outFile.write(cmd.getFilename(outputFile));
}

// write feedback text on stdout
void CDisassembler::feedBackText1() {
    if (cmd.verbose && cmd.job == CMDL_JOB_DIS) {
        // Tell what we are doing:
        printf("\nDisassembling %s to %s", cmd.getFilename(cmd.inputFile), cmd.getFilename(outputFile));
    }
}


void CDisassembler::pass1() {

    /*             pass 1: does the following jobs:
    --------------------------------

    * Scans all code sections, instruction by instruction.

    * Follows all references to data in order to determine data type for 
    each data symbol.

    * Assigns symbol table entries for all jump and call targets that do not
    allready have a name.

    * Identifies and analyzes tables of jump addresses and call addresses,
    e.g. switch/case tables and virtual function tables. (to do !)

    * Tries to identify any data in the code section.

    */
    //uint32_t sectionType;

    // Loop through sections, pass 1
    for (section = 1; section < sectionHeaders.numEntries(); section++) {

        // Get section type
        //sectionType = sectionHeaders[section].sh_type;
        codeMode = (sectionHeaders[section].sh_flags & SHF_EXEC) ? 1 : 4;

        sectionBuffer = dataBuffer.buf() + sectionHeaders[section].sh_offset;
        sectionEnd = (uint32_t)sectionHeaders[section].sh_size;

        if (codeMode < 4) {
            // This is a code section

            sectionAddress = sectionHeaders[section].sh_addr;
            if (sectionEnd == 0) continue;

            iInstr = 0;

            // Loop through instructions
            while (iInstr < sectionEnd) {

                // Check if code not dubious
                if (codeMode == 1) {

                    parseInstruction();                    // Parse instruction

                    updateSymbols();                       // Detect symbol types for operands of this instruction

                    updateTracer();                        // Trace register values

                    iInstr += instrLength * 4;             // Next instruction
                }
                else {
                  //  iEnd = labelEnd;
                }
            }
        }
    }
}


void CDisassembler::pass2() {

    /*             pass 2: does the following jobs:
    --------------------------------

    * Scans through all sections, code and data.

    * Outputs warnings for suboptimal instruction codes and error messages
    for erroneous code and erroneous relocations.

    * Outputs disassembly of all instructions, operands and relocations,
    followed by the binary code listing as comment.

    * Outputs disassembly of all data, followed by alternative representations
    as comment.
    */

    //uint32_t sectionType;

    // Loop through sections, pass 2
    for (section = 1; section < sectionHeaders.numEntries(); section++) {

        // Get section type
        //sectionType = sectionHeaders[section].sh_type;
        codeMode = (sectionHeaders[section].sh_flags & SHF_EXEC) ? 1 : 4;

        // Initialize code parser
        sectionBuffer = dataBuffer.buf() + sectionHeaders[section].sh_offset;
        sectionEnd = (uint32_t)sectionHeaders[section].sh_size;
        sectionAddress = sectionHeaders[section].sh_addr;

        writeSectionBegin();                               // Write segment directive

        if (codeMode < 4) {
            // This is a code section
            if (sectionEnd == 0) continue;
            iInstr = 0;

            // Loop through instructions
            while (iInstr < sectionEnd) {

                if (debugMode) {
                    // save cross reference
                    SLineRef xref = { iInstr + sectionAddress, 1, outFile.dataSize() };
                    lineList.push(xref);
                    writeAddress();
                }
                writeLabels();                             // Find any label here

                // Check if code not dubious
                if (codeMode == 1) {
                    
                    parseInstruction();                    // Parse instruction

                    writeInstruction();                    // Write instruction

                    iInstr += instrLength * 4;             // Next instruction

                }
                else {
                    // This is data Skip to next label                                            
                }
            }
            writeSectionEnd();                             // Write segment directive
        }
        else {
            // This is a data section
            pInstr = 0; iRecord = 0; fInstr = 0;           // Set invalid pointers to zero
            operandType = 2;                               // Default data type is int32
            instrLength = 4;                               // Default data size is 4 bytes
            iInstr = 0;                                    // Instruction position
            nextSymbol = 0;
            
            writeDataItems();                              // Loop through data. Write data

            writeSectionEnd();                             // Write segment directive
        }
    }
}



/********************  Explanation of tracer:  ***************************

This is a machine which can trace the contents of each register in certain
situations. It is currently used for recognizing pointers to jump tables
in order to identify jump tables (to do!)
*/
void CDisassembler::updateTracer() {
    // Trace register values. See explanation above
}


void CDisassembler::updateSymbols() {
    // Find unnamed symbols, determine symbol types,
    // update symbol list, call checkJumpTarget if jump/call.
    // This function is called during pass 1 for every instruction
    uint32_t relSource = 0; // Position of relocated field

    if (fInstr->category == 4 && fInstr->jumpSize) {
        // Self-relative jump instruction. Check OPJ
        // uint32_t opj = (instrLength == 1) ? pInstr->a.op1 : pInstr->b[0]; // Jump instruction opcode
        // Check if there is a relocation here
        relSource = iInstr + (fInstr->jumpPos); // Position of relocated field
        ElfFwcReloc rel;
        rel.r_offset = relSource;
        rel.r_section = section;
        rel.r_addend = 0;
        if (relocations.findFirst(rel) < 0) {
            // There is no relocation. Target must be in the same section. Find target
            int32_t offset = 0;
            switch (fInstr->jumpSize) {                // Read offset of correct size
            case 1:      // 8 bit
                offset = *(int8_t*)(sectionBuffer + relSource);
                rel.r_type = R_FORW_8 | 0x80000000;  //  add 0x80000000 to remember that this is not a real relocation
                break;
            case 2:      // 16 bit
                offset = *(int16_t*)(sectionBuffer + relSource);
                rel.r_type = R_FORW_16 | 0x80000000;
                break;
            case 3:      // 24 bit. Sign extend to 32 bits
                offset = *(int32_t*)(sectionBuffer + relSource) << 8 >> 8;
                rel.r_type = R_FORW_24 | 0x80000000;
                break;
            case 4:      // 32 bit
                offset = *(int32_t*)(sectionBuffer + relSource);
                rel.r_type = R_FORW_32 | 0x80000000;
                break;
            }
            // Scale offset by 4 and add offset to end of instruction
            int32_t target = iInstr + instrLength * 4 + offset * 4;

            // Add a symbol at target address if none exists
            ElfFwcSym sym;
            zeroAllMembers(sym);
            sym.st_bind = STB_LOCAL;
            sym.st_other = STV_EXEC;
            sym.st_section = section;
            sym.st_value = (uint64_t)(int64_t)target;
            symbolExeAddress(sym);
            int32_t symi = symbols.findFirst(sym);
            if (symi < 0) {
                symi = newSymbols.push(sym);           // Add symbol to new symbols table
                symi |= 0x80000000;                    // Upper bit means index refers to newSymbols
            }
            // Add a dummy relocation record for this symbol. 
            // This relocation does not need type, scale, or addend because the only purpose is to identify the symbol.
            // It does have a size, though, because this is checked later in writeRelocationTarget()
            rel.r_sym = (uint32_t)symi;
            relocations.addUnique(rel);
        }
    }

    // Check if instruction has a memory reference relative to IP, DATAP, or THREADP
    uint32_t basePointer = 0;
    if (fInstr->mem & 2) basePointer = pInstr->a.rs;
    relSource = iInstr + fInstr->addrPos; // Position of relocated field

    if (fInstr->addrSize > 1 && basePointer >= 28 && basePointer <= 30 && !(fInstr->mem & 0x20)) {
        // Memory operand is relative to THREADP, DATAP or IP
        // Check if there is a relocation here
        uint32_t relpos = iInstr + fInstr->addrPos;
        ElfFwcReloc rel;
        rel.r_offset = relpos;
        rel.r_section = section;
        rel.r_type = (operandType | 0x80) << 24;
        uint32_t nrel, irel = 0;
        nrel = relocations.findAll(&irel, rel);
        if (nrel > 1) writeWarning("Overlapping relocations here");
        if (nrel) {
            // Relocation found. Put the data type into the relocation record. 
            // The data type will later be transferred to the symbol record in joinSymbolTables()
            if (!(relocations[irel].r_type & 0x80000000)) {
                // Save target data type in upper 8 bits of r_type
                relocations[irel].r_type = (relocations[irel].r_type & 0x00FFFFFF) | (operandType /*| 0x80*/) << 24;
            }
            // Check if the target is a section + offset
            uint32_t symi = relocations[irel].r_sym;
            if (symi < symbols.numEntries() && symbols[symi].st_type == STT_SECTION && relocations[irel].r_addend > 0) {
                // Add a new symbol at this address
                ElfFwcSym sym;
                zeroAllMembers(sym);
                sym.st_bind = STB_LOCAL;
                sym.st_other = STT_OBJECT;
                sym.st_section = symbols[symi].st_section;
                sym.st_value = symbols[symi].st_value + (int64_t)relocations[irel].r_addend;
                symbolExeAddress(sym);
                uint32_t symi2 = newSymbols.push(sym);
                relocations[irel].r_sym = symi2 | 0x80000000;  // Upper bit means index refers to newSymbols
                relocations[irel].r_addend = 0;
            }
        }
        else if (basePointer == REG_IP >> 16 && fInstr->addrSize > 1 && !(fInstr->mem & 0x20)) {
            // No relocation found. Insert new relocation and new symbol
            // This fits the address instruction with a local IP target.
            // to do: Make it work for other cases

            // Add a symbol at target address if none exists
            int32_t target = iInstr + instrLength * 4;
            switch (fInstr->addrSize) {                // Read offset of correct size
            /* case 1:      // 8 bit. cannot use IP
                target += *(int8_t*)(sectionBuffer + relSource) << (operandType & 7);
                rel.r_type = R_FORW_8 | R_FORW_SELFREL | 0x80000000;
                break;*/
            case 2:      // 16 bit
                target += *(int16_t*)(sectionBuffer + relSource);
                rel.r_type = R_FORW_16 | R_FORW_SELFREL | 0x80000000;
                break;
            case 4:      // 32 bit
                target += *(int32_t*)(sectionBuffer + relSource);
                rel.r_type = R_FORW_32 | R_FORW_SELFREL | 0x80000000;
                break;
            }
            ElfFwcSym sym;
            zeroAllMembers(sym);
            sym.st_bind = STB_LOCAL;
            sym.st_other = STV_EXEC;
            sym.st_section = section;
            sym.st_value = (uint64_t)(int64_t)target;

            symbolExeAddress(sym);
            int32_t symi = symbols.findFirst(sym);
            if (symi < 0) {
                symi = newSymbols.push(sym);           // Add symbol to new symbols table
                symi |= 0x80000000;                    // Upper bit means index refers to newSymbols
            }
            // Add a dummy relocation record for this symbol. 
            // This relocation does not need type, scale, or addend because the only purpose is to identify the symbol.
            // It does have a size, though, because this is checked later in writeRelocationTarget()
            rel.r_offset = (uint64_t)iInstr + fInstr->addrPos; // Position of relocated field
            rel.r_section = section;
            rel.r_addend = -4;
            rel.r_sym = (uint32_t)symi;
            relocations.addUnique(rel);
        }
        else if ((basePointer == REG_DATAP >> 16 || basePointer == REG_THREADP >> 16)
            && fInstr->addrSize > 1 && !(fInstr->mem & 0x20) && isExecutable) {
            // No relocation found. Insert new relocation and new symbol. datap or threadp based

            // Add a symbol at target address if none exists
            int64_t target = fileHeader.e_datap_base;
            rel.r_type = R_FORW_DATAP;
            uint32_t dom = 2;
            uint32_t st_other = STV_DATAP;
            if (basePointer == REG_THREADP >> 16) {
                target = fileHeader.e_threadp_base;
                rel.r_type = R_FORW_THREADP;
                dom = 3;
                st_other = STV_THREADP;
            }
            switch (fInstr->addrSize) {                // Read offset of correct size
            case 1:      // 8 bit
                target += *(int8_t*)(sectionBuffer + relSource);
                rel.r_type |= R_FORW_8 | 0x80000000;
                break;
            case 2:      // 16 bit
                target += *(int16_t*)(sectionBuffer + relSource);
                rel.r_type |= R_FORW_16 | 0x80000000;
                break;
            case 4:      // 32 bit
                target += *(int32_t*)(sectionBuffer + relSource);
                rel.r_type |= R_FORW_32 | 0x80000000;
                break;
            }
            ElfFwcSym sym;
            zeroAllMembers(sym);
            sym.st_type = STT_OBJECT;
            sym.st_bind = STB_WEAK;
            sym.st_other = st_other;
            sym.st_section = dom;
            sym.st_value = (uint64_t)target;

            int32_t symi = symbols.findFirst(sym);
            if (symi < 0) {
                symi = newSymbols.push(sym);           // Add symbol to new symbols table
                symi |= 0x80000000;                    // Upper bit means index refers to newSymbols
            }
            // Add a dummy relocation record for this symbol. 
            // This relocation does not need type, scale, or addend because the only purpose is to identify the symbol.
            // It does have a size, though, because this is checked later in writeRelocationTarget()
            rel.r_offset = iInstr + fInstr->addrPos; // Position of relocated field
            rel.r_section = section;
            rel.r_addend = 0;
            rel.r_sym = (uint32_t)symi;
            relocations.addUnique(rel);
        }
    }
}


void CDisassembler::followJumpTable(uint32_t symi, uint32_t RelType) {
    // Check jump/call table and its targets
    // to do !
}


void CDisassembler::markCodeAsDubious() {
    // Remember that this may be data in a code segment
}


// List of instructionlengths, used in parseInstruction
static const uint8_t lengthList[8] = {1,1,1,1,2,2,3,4};


void CDisassembler::parseInstruction() {
    // Parse one opcode at position iInstr
    instructionWarning = 0;

    // Get instruction
    pInstr = (STemplate*)(sectionBuffer + iInstr);

    // Get op1
    uint8_t op = pInstr->a.op1;

    // Get format
    format = (pInstr->a.il << 8) + (pInstr->a.mode << 4); // Construct format = (il,mode,submode)
                                                          
    // Get submode
    switch (format) {
    case 0x200: case 0x220: case 0x300: case 0x320:  // submode in mode2
        format += pInstr->a.mode2;
        break;
    case 0x250: case 0x310: // Submode for jump instructions etc.
        if (op < 8) {
            format += op;  op = pInstr->b[0] & 0x3F;
        }
        else {
            format += 8;
        }
        break;
    }

    // Look up format details
    static SFormat form;
    fInstr = &formatList[lookupFormat(pInstr->q)];     // lookupFormat is in emulator2.cpp
    format = fInstr->format2;                          // Include subformat depending on op1
    if (fInstr->tmplate == 0xE && pInstr->a.op2 && !(fInstr->imm2 & 0x100)) {
        // Single format instruction if op2 != 0 and op2 not used as immediate operand
        form = *fInstr;
        form.category = 1;
        fInstr = &form;
    }

    // Get operand type
    if (fInstr->ot == 0) {                                 // Operand type determined by OT field
        operandType = pInstr->a.ot;                        // Operand type
        if (!(pInstr->a.mode & 6) && !(fInstr->vect & 0x11)) {
            // Check use of M bit
            format |= (operandType & 4) << 5;              // Add M bit to format
            operandType &= ~4;                             // Remove M bit from operand type
        }
    }
    else if ((fInstr->ot & 0xF0) == 0x10) {                // Operand type fixed. Value in formatList
        operandType = fInstr->ot & 7;
    }
    else if (fInstr->ot == 0x32) {                         // int32 for even op1, int64 for odd op1
        operandType = 2 + (pInstr->a.op1 & 1);
    }
    else if (fInstr->ot == 0x35) {                         // Float for even op1, double for odd op1
        operandType = 5 + (pInstr->a.op1 & 1);
    }
    else {
        operandType = 0;                                   // Error in formatList. Should not occur
    }

    // Find instruction length
    instrLength = lengthList[pInstr->i[0] >> 29];           // Length up to 3 determined by il. Length 4 by upper bit of mode

    // Find any reasons for warnings
    //findWarnings(p);

    // Find any errors
    //findErrors(p);
}



/*****************************************************************************
Functions for reading instruction list from comma-separated file,
sorting, and searching
*****************************************************************************/

// Members of class CCSVFile for reading comma-separated file

// Read and parse file
void CCSVFile::parse() {
    // Sorry for the ugly code!

    const char * fields[numInstructionColumns];  // pointer to each field in line
    int fi = 0;                                  // field index
    uint32_t i, j;                               // loop counters
    char * s, * t = 0;                           // point to begin and end of field
    char c;
    char separator = 0;                          // separator character, preferably comma
    int line = 1;                                // line number
    SInstruction record;                         // record constructed from line
    zeroAllMembers(fields);
                                                 
    if (data_size==0) read(cmd.getFilename(cmd.instructionListFile), 2);                    // read file if it has not already been read
    if (err.number()) return;

    // loop through file
    for (i = 0; i < data_size; i++) {
        // find begin of field, quoted or not
        s = (char*)buf() + i;
        c = *s;
        if (c == ' ') continue;                  // skip leading spaces

        if (c == '"' || c == 0x27) {             // single or double quote
            fields[fi] = s+1;                    // begin of quoted string
            for (i++; i < data_size; i++) {      // search for matching end quote
                t = (char*)buf() + i;
                if (*t == c) {
                    *t = 0; i++;                 // End quote found. Put end of string here
                    goto SEARCHFORCOMMA;
                }
                if (*t == '\n') break;           // end of line found before end quote
            }
            // end quote not found
            err.submit(ERR_INSTRUCTION_LIST_QUOTE, line);
            return;
        }
        if (c == '\r' || c == '\n') 
            goto NEXTLINE;  // end of line found
        if (c == separator || c == ',') {
            // empty field
            fields[fi] = "";            
            goto SEARCHFORCOMMA;
        }

        // Anything else: begin of unquoted string
        fields[fi] = s;
        // search for end of field

    SEARCHFORCOMMA:
        for (; i < data_size; i++) {  // search for comma after field
            t = (char*)buf() + i;
            if (*t == separator || (separator == 0 && (*t == ',' || *t == ';' || *t == '\t'))) {
                separator = *t;               // separator set to the first comma, semicolon or tabulator
                *t = 0;                       // put end of string here
                goto NEXTFIELD;
            }
            if (*t == '\n') break;        // end of line found before comma
        }
        fi++; 
        goto NEXTLINE;

    NEXTFIELD:
        // next field
        fi++;
        if (fi != numInstructionColumns) continue;
        // end of last field

    NEXTLINE:            
        for (; i < data_size; i++) {  // search for end. of line
            t = (char*)buf() + i;
            // accept newlines as "\r", "\n", or "\r\n"
            if (*t == '\r' || *t == '\n') break;
        }         
        if (*t == '\r' && *(t+1) == '\n') i++;  // end of line is two characters
        *t = 0;  // terminate line

        // make any remaining fields blank
        for (; fi < numInstructionColumns; fi++) {
            fields[fi] = "";
        }
        // Begin next line
        line++;
        fi = 0;

        // Check if blank or heading record
        if (fields[2][0] < '0' || fields[2][0] > '9') continue;

        // save values to record
        // most fields are decimal or hexadecimal numbers
        record.id = (uint32_t)interpretNumber(fields[1]);
        record.category = (uint32_t)interpretNumber(fields[2]);
        record.format = interpretNumber(fields[3]);
        record.templt = (uint32_t)interpretNumber(fields[4]);
        record.sourceoperands = (uint32_t)interpretNumber(fields[6]);
        record.op1 = (uint32_t)interpretNumber(fields[7]);
        record.op2 = (uint32_t)interpretNumber(fields[8]);
        record.optypesgp = (uint32_t)interpretNumber(fields[9]);
        record.optypesscalar = (uint32_t)interpretNumber(fields[10]);
        record.optypesvector = (uint32_t)interpretNumber(fields[11]);
        // interpret immediate operand
        if (tolower(fields[12][0]) == 'i') {
            // implicit immediate operand. value is prefixed by 'i'. Get value
            record.implicit_imm = (uint32_t)interpretNumber(fields[12]+1);
            record.opimmediate = OPI_IMPLICIT;
        }
        else {
            // immediate operand type
            record.opimmediate = (uint8_t)interpretNumber(fields[12]);
        }
        // interpret template variant
        record.variant = interpretTemplateVariants(fields[5]);
        // copy instruction name
        for (j = 0; j < sizeof(record.name)-1; j++) {
            c = fields[0][j];
            if (c == 0) break;
            record.name[j] = tolower(c);
        }
        record.name[j] = 0;

        // add record to list
        instructionlist.push(record); 
    }
}

// Interpret number in instruction list
uint64_t CCSVFile::interpretNumber(const char * text) {
    uint32_t error = 0;
    uint64_t result = uint64_t(::interpretNumber(text, 64, &error));
    if (error)  err.submit(ERR_INSTRUCTION_LIST_SYNTAX, text);
    return result;
} 


// Interpret a string with a decimal, binary, octal, or hexadecimal number
int64_t interpretNumber(const char * text, uint32_t maxLength, uint32_t * error) {
    int state = 0;           // 0: begin, 1: after 0, 
                             // 2: after 0x, 3: after 0b, 4: after 0o
                             // 5: after decimal digit, 6: trailing space
    uint64_t number = 0;
    uint8_t c, clower, digit;
    bool sign = false;
    uint32_t i;
    *error = 0;
    if (text == 0) {
        *error = 1; return number;
    }

    for (i = 0; i < maxLength; i++) {
        c = text[i];                    // read character
        clower = c | 0x20;              // convert to lower case
        if (clower == 'x') {
            if (state != 1) {
                *error = 1;  return 0;
            }
            state = 2;
        }
        else if (clower == 'o') {
            if (state != 1) { 
                *error = 1;  return 0;
            }
            state = 4;
        }
        else if (clower == 'b' && state == 1) {
            state = 3;
        }
        else if (c >= '0' && c <= '9') {
            // digit 0 - 9
            digit = c - '0'; 
            switch (state) {
            case 0:
                state = (digit == 0) ? 1 : 5;
                number = digit;
                break;
            case 1:
                state = 5;
                // continue in case 5:
            case 5:
                // decimal
                number = number * 10 + digit;
                break;
            case 2:
                // hexadecimal
                number = number * 16 + digit;
                break;
            case 3:
                // binary
                if (digit > 1) {
                    *error = 1;  return 0;
                }
                number = number * 2 + digit;
                break;
            case 4:
                // octal
                if (digit > 7) {
                    *error = 1;  return 0;
                }
                number = number * 8 + digit;
                break;
            default:
                *error = 1; 
                return 0;
            }
        }
        else if (clower >= 'a' && clower <= 'f') {
            // hexadecimal digit
            digit = clower - ('a' - 10);
            if (state != 2)  {
                *error = 1;  return 0;
            }
            number = number * 16 + digit;
        }
        else if (c == ' ' || c == '+') {
            // ignore leading or trailing blank or plus
            if (state > 0) state = 6;
        }
        else if (c == '-') {
            // change sign
            if (state != 0) {
                *error = 1;  return 0;
            }
            sign = ! sign;
        }
        else if (c == 0) break;  // end of string
        else if (c == ',') {
            *error = i | 0x1000;          // end with comma. return position in error
            break;
        }
        else {
            // illegal character
            *error = 1;  return 0;
        }        
    }
    if (sign) number = uint64_t(-int64_t(number));
    return (int64_t)number;
}

void CDisassembler::getLineList(CDynamicArray<SLineRef> & list) {
    // transfer lineList to debugger
    list << lineList;
}

void CDisassembler::getOutFile(CTextFileBuffer & buffer) {
    // transfer outFile to debugger
    buffer.copy(outFile);
}


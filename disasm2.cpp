/****************************  disasm2.cpp   ********************************
* Author:        Agner Fog
* Date created:  2017-04-26
* Last modified: 2024-08-02
* Version:       1.13
* Project:       Binary tools for ForwardCom instruction set
* Module:        disassem.h
* Description:
* Disassembler for ForwardCom
*
* Copyright 2007-2024 GNU General Public License http://www.gnu.org/licenses
*****************************************************************************/
#include "stdafx.h"

static const char * commentSeparator = "//";       // Comment separator in output assembly file


/**************************  class CDisassembler  *****************************
Most member functions of CDisassembler are defined in disasm1.cpp

Only the functions that produce output are defined here:
******************************************************************************/


void CDisassembler::writeSymbolName(uint32_t symi) {
    // Write symbol name. symi = symbol index
    uint32_t sname = symbols[symi].st_name;
    if (sname == 0) outFile.put("no_name");
    else if (sname >= stringBuffer.dataSize()) outFile.put("(illegal name index)");
    else outFile.put((char*)stringBuffer.buf() + sname);
}


void CDisassembler::writeSectionName(int32_t SegIndex) {
    // Write name of section, segment or group from section index
    const char * name = "noname";
    if (sectionHeaders[SegIndex].sh_name < stringBuffer.dataSize()) {
        name = (char*)stringBuffer.buf() + sectionHeaders[SegIndex].sh_name;
    }
    outFile.put(name);
} 


// Find any labels at current position and next
void CDisassembler::writeLabels() {
    // check section type
    uint8_t sectionType = sectionHeaders[section].sh_type;
    if (!(sectionType & SHT_ALLOCATED)) {
        return;   // section is not allocated
    }
    // start at new line
    if (outFile.getColumn() && !debugMode) outFile.newLine();

    if (iInstr == currentFunctionEnd && currentFunction) {
        // Current function is ending here
        writeSymbolName(currentFunction); 
        outFile.put(' '); outFile.tabulate(asmTab2); // at least one space
        outFile.put("end");
        outFile.newLine();
        currentFunction = 0; currentFunctionEnd = 0;
    }
    //bool isFunction = false;

    // Make dummy symbol to search for
    ElfFwcSym currentPosition;
    currentPosition.st_section = section;
    currentPosition.st_value = iInstr;
    symbolExeAddress(currentPosition);

    // Search for any symbol here. Look for any misplaced symbols we might have skipped before last output
    uint32_t numSymbols = 0; // Check if multiple symbols at same place
    while (nextSymbol < symbols.numEntries() 
        && symbols[nextSymbol] < currentPosition) {
        if (symbols[nextSymbol].st_section == currentPosition.st_section && iInstr
            && symbols[nextSymbol].st_type != STT_CONSTANT) {
            outFile.put(commentSeparator);
            outFile.put(" Warning: Misplaced symbol: ");
            writeSymbolName(nextSymbol);
            outFile.put(" at offset ");
            outFile.putHex(symbols[nextSymbol].st_value);
            outFile.newLine();
            symbols[nextSymbol].st_other |= 0x80000000;    // Remember symbol has been written
        }
        nextSymbol++;
    }
    // Write all symbols at current position
    while (nextSymbol < symbols.numEntries() && symbols[nextSymbol] == currentPosition) {
        if (symbols[nextSymbol].st_type != STT_CONSTANT) {
            if (numSymbols++) {           // Multiple symbols at same position
                if (debugMode) {
                    outFile.put(";  ");  // cannot show multiple lines at same address in debug mode
                }
                else {
                    outFile.put("\n");   // put on separate lines
                }
            }
            writeSymbolName(nextSymbol);
            if (symbols[nextSymbol].st_type == STT_FUNC && symbols[nextSymbol].st_bind != STB_LOCAL) {
                // This is a function            
                if (debugMode) {
                    outFile.put(": ");
                }
                else outFile.put(": function");
                //isFunction = true;
                currentFunction = nextSymbol;                  // Remember which function we are in
                if (symbols[nextSymbol].st_unitsize) {         // Calculate end of current function
                    if (symbols[nextSymbol].st_unitnum == 0) symbols[nextSymbol].st_unitnum = 1;
                    currentFunctionEnd = iInstr + symbols[nextSymbol].st_unitsize * symbols[nextSymbol].st_unitnum;
                }
                else currentFunctionEnd = 0;                   // Function size is not known
            }
            else if (codeMode & 1) { // local label        
                outFile.put(": ");
            }
            symbols[nextSymbol].st_other |= 0x80000000;        // Remember symbol has been written
        }
        nextSymbol++;
    }
    if (numSymbols) {
        if (codeMode == 1) {
            // if (!isFunction) outFile.put(':');          // has already been written
            if (!debugMode) outFile.newLine();             // Code. Put label on separate line
        }
        else {
            outFile.put(':');                              // Data. Make space after last label
        }
    }
}


void CDisassembler::writeDataItems() {
    // Write contents of data section to output file
    uint32_t nextLabel = 0;
    uint32_t nextRelocation = 0;
    uint32_t dataSize = 4;
    uint32_t currentSymbol;
    uint32_t sequenceEnd;
    ElfFwcReloc rel;                            // relocation record for searching
    uint32_t irel;                               // index to relocation record
    uint32_t numRel;                             // number of relocations found at current position

    operandType = 2;
    bool isFloat = false;

    // translate addresses if executable
    ElfFwcSym currentPosition;
    currentPosition.st_section = section;
    currentPosition.st_value = iInstr;
    symbolExeAddress(currentPosition);

    // find first relocation
    rel.r_offset = iInstr;
    rel.r_section = section;
    irel = relocations.findFirst(rel);
    irel &= 0x7FFFFFFF;
    if (irel < relocations.numEntries() && relocations[irel].r_section == section) {
        nextRelocation = (uint32_t)relocations[irel].r_offset;
    }
    else nextRelocation = sectionEnd;

    // Loop through section
    while (iInstr < sectionEnd) {
        // Search for symbol labels
        writeLabels();
        if (nextSymbol > 1) {
            // Get data size from current symbol
            currentSymbol = nextSymbol - 1;
            if (symbols[currentSymbol].st_section == currentPosition.st_section) {
                dataSize = symbols[currentSymbol].st_unitsize;
                if (dataSize > 8) dataSize = 8;
                if (dataSize == 0) dataSize = 4;
                isFloat = (symbols[currentSymbol].st_other & STV_FLOAT) != 0;
            }
        }
        // Get position of next symbol
        if (nextSymbol < symbols.numEntries()) {
            nextLabel = (uint32_t)symbols[nextSymbol].st_value;
            // translate to local offset
            if (isExecutable) nextLabel -= (uint32_t)sectionHeaders[section].sh_addr;
        }
        else nextLabel = sectionEnd;
        
        // Search for relocations
        rel.r_offset = iInstr;
        numRel = relocations.findAll(&irel, rel);
        if (numRel) {
            // Relocation found. Find size
            // Relocation size overrides any symbol size
            switch (relocations[irel].r_type & R_FORW_RELSIZEMASK) {
            case R_FORW_8:
                dataSize = 1;
                break;
            case R_FORW_16: case R_FORW_32LO: case R_FORW_32HI:
                dataSize = 2;
                break;
            case R_FORW_24:
                dataSize = 4;    // 3 bytes. Round up to 4
                break;
            case R_FORW_32: case R_FORW_64LO: case R_FORW_64HI:
                dataSize = 4;
                break;
            case R_FORW_64:
                dataSize = 8;
                break;
            default:
                writeError("Unknown data size for relocation");
                dataSize = 4;
                break;
            }
            isFloat = false;
            if (numRel > 1) writeError("Overlapping relocations");
            // Find position of next relocation
            if (irel+1 < relocations.numEntries() && relocations[irel+1].r_section == section) {
                nextRelocation = (uint32_t)relocations[irel+1].r_offset;
            }
            else nextRelocation = sectionEnd;
        }

        if (numRel) {
            // There is a relocation here. Write only one data item
            // Write type
            outFile.tabulate(asmTab1);
            switch (dataSize) {
            case 1: outFile.put("int8 "); break;
            case 2: outFile.put("int16 "); break;
            case 4: outFile.put("int32 "); break;
            case 8: outFile.put("int64 "); break;
            }
            outFile.tabulate(asmTab2);
            writeRelocationTarget(iInstr, dataSize);

            // Write comment with relocation type
            outFile.put(' '); outFile.tabulate(asmTab3);
            outFile.put(commentSeparator); outFile.put(' '); 
            if (sectionEnd + sectionAddress> 0xFFFF) outFile.putHex((uint32_t)(iInstr + sectionAddress), 2); 
            else outFile.putHex((uint16_t)(iInstr + sectionAddress), 2); 
            outFile.put(" _ ");
            switch(relocations[irel].r_type & R_FORW_RELTYPEMASK) {
            case R_FORW_ABS:
                outFile.put("absolute address"); break;
            case R_FORW_SELFREL:
                outFile.put("self-relative"); break;
            case R_FORW_IP_BASE:
                outFile.put("relative to __ip_base"); break;
            case R_FORW_DATAP:
                outFile.put("relative to __datap_base"); break;
            case R_FORW_THREADP:
                outFile.put("relative to __threadp_base"); break;
            case R_FORW_REFP:
                outFile.put("relative to ");
                writeSymbolName(relocations[irel].r_refsym & 0x7FFFFFFF); break;
            case R_FORW_SYSFUNC:
                outFile.put("system function ID"); break;
            case R_FORW_SYSMODUL:
                outFile.put("system module ID"); break;
            case R_FORW_SYSCALL:
                outFile.put("system module and function ID"); break;
            case R_FORW_DATASTACK:
                outFile.put("data stack size"); break;
            case R_FORW_CALLSTACK:
                outFile.put("call stack size"); break;
            case R_FORW_REGUSE:
                outFile.put("register use"); break;
            default:
                outFile.put("unknown relocation type"); break;
            }
            iInstr += dataSize;
        }
        else {
            // Write multiple data items. Find where sequence ends
            sequenceEnd = sectionEnd;
            if (nextLabel < sequenceEnd && nextLabel > iInstr) sequenceEnd = nextLabel;
            if (nextRelocation < sequenceEnd && nextRelocation > iInstr) sequenceEnd = nextRelocation;
            // Number of data items in this sequence
            uint32_t num = (sequenceEnd - iInstr) / dataSize;
            if (num == 0) {
                dataSize = sequenceEnd - iInstr;  // Reduce data size to avoid going past sequenceEnd
                while (dataSize & (dataSize-1)) dataSize--; // Round down to nearest power of 2                
                num = 1;
            }
            // Number of data items per line
            uint32_t itemsPerLine = 4;
            if (dataSize > 4) itemsPerLine = 2;
            if (dataSize < 2) itemsPerLine = 8;
            uint32_t lineEnd = iInstr + itemsPerLine * dataSize;
            if (lineEnd > sequenceEnd) {
                // Round down to multiple of dataSize
                itemsPerLine = (sequenceEnd - iInstr) / dataSize;  
                lineEnd = iInstr + itemsPerLine * dataSize;
            }
            // Write type
            outFile.tabulate(asmTab1);
            switch (dataSize) {
            case 1: outFile.put("int8 "); break;
            case 2: outFile.put("int16 "); break;
            case 4: outFile.put("int32 "); break;
            case 8: outFile.put("int64 "); break;
            }
            outFile.tabulate(asmTab2);

            // Write items
            uint32_t lineBegin = iInstr;
            while (iInstr < lineEnd) {
                if (sectionHeaders[section].sh_type == SHT_NOBITS) {
                    // BSS section has no data in buffer
                    outFile.put('0');
                }
                else {
                    // Write item
                    switch (dataSize) {
                    case 1:
                        outFile.putHex(*(uint8_t*)(sectionBuffer + iInstr));
                        break;
                    case 2:
                        outFile.putHex(*(uint16_t*)(sectionBuffer + iInstr));
                        break;
                    case 4:
                        outFile.putHex(*(uint32_t*)(sectionBuffer + iInstr));
                        break;
                    case 8:
                        outFile.putHex(*(uint64_t*)(sectionBuffer + iInstr));
                        break;
                    }
                }
                iInstr += dataSize;
                if (iInstr < lineEnd) outFile.put(", ");  // comma if not the last item on line
            }
            // Write data comment
            outFile.put(' '); outFile.tabulate(asmTab3);
            outFile.put(commentSeparator); outFile.put(' '); 

            // write address
            uint64_t address = lineBegin + sectionAddress;
            if (sectionHeaders[section].sh_flags & SHF_IP) {
                // IP based section. subtract ip_base for continuity with code section
                address -= fileHeader.e_ip_base;
            }
            if (sectionEnd + sectionAddress > 0xFFFF) outFile.putHex(uint32_t(address), 2); 
            else outFile.putHex(uint16_t(address), 2);

            if (sectionHeaders[section].sh_type != SHT_NOBITS) { // skip data if BSS section
                outFile.put(" _ ");
                // Write data in alternative form
                for (uint32_t i = lineBegin; i < lineEnd; i += dataSize) {
                    switch (dataSize) {
                    case 1: {  // bytes. Write as characters
                        char c = *(char*)(sectionBuffer + i);
                        outFile.put((uint8_t)c < ' ' ? '.' : c);
                        break; }
                    case 2:
                        if (isFloat) { // half precision float
                            outFile.putFloat(half2float(*(uint16_t*)(sectionBuffer + i)));
                        }
                        else { // 16 bit integer. Write as signed decimal
                            outFile.putDecimal(*(int16_t*)(sectionBuffer + i), 1);
                        }
                        if (i + dataSize < lineEnd) outFile.put(", "); // Comma except before last item
                        break;
                    case 4:
                        if (isFloat) { // single precision float
                            outFile.putFloat(*(float*)(sectionBuffer + i));
                        }
                        else { // 16 bit integer. Write as signed decimal
                            outFile.putDecimal(*(int32_t*)(sectionBuffer + i), 1);
                        }
                        if (i + dataSize < lineEnd) outFile.put(", "); // Comma except before last item
                        break;
                    case 8:
                        if (isFloat) { // double precision float
                            outFile.putFloat(*(double*)(sectionBuffer + i));
                        }
                        else {  // 64 bit integer. Write as signed decimal if not huge
                            int64_t x = *(int64_t*)(sectionBuffer + i);
                            if (x == (int32_t)x) outFile.putDecimal((int32_t)x, 1);
                        }
                        if (i + dataSize < lineEnd) outFile.put(", "); // Comma except before last item
                        break;
                    default:;
                    }
                }
            }
        }
        if (iInstr < sectionEnd) outFile.newLine();
    }
    // write label at end of data section, if any
    if ((section + 1 == sectionHeaders.numEntries() ||
    ((sectionHeaders[section].sh_flags ^ sectionHeaders[section + 1].sh_flags) & SHF_BASEPOINTER))
    && nextSymbol < symbols.numEntries()) {
        writeLabels();
    }
}

static const uint32_t relocationSizes[16] = {0, 1, 2, 3, 4, 4, 4, 8, 8, 8, 0, 0, 0, 0, 0, 0};

void CDisassembler::writeRelocationTarget(uint32_t src, uint32_t size) {
    // Write relocation target for this source position
    // Find relocation
    ElfFwcReloc rel;
    rel.r_offset = src;
    rel.r_section = section;
    //uint32_t irel;  // index to relocation record
    uint32_t n = relocations.findAll(&relocation, rel);
    if (n == 0) return;
    if (n > 1) {
        writeWarning(n ? "Overlapping relocations" : "No relocation found here");
        return;
    }
    relocation++;    // add 1 to avoid zero
    relocations[relocation-1].r_refsym |= 0x80000000;              // Remember relocation has been used OK
    // write scale factor if scale factor != 1 and not a jump target
    bool writeScale = relocations[relocation-1].r_type & R_FORW_RELSCALEMASK;
    if (writeScale || codeMode > 1) outFile.put('(');
    uint32_t isym = relocations[relocation-1].r_sym;
    writeSymbolName(isym);
    // Find any addend
    int32_t expectedAddend = 0;
    int32_t addend = relocations[relocation-1].r_addend;
    if ((relocations[relocation-1].r_type & R_FORW_RELTYPEMASK) == R_FORW_SELFREL) {
        if (fInstr) {                                      // Expected addend for self-relative address
            if (fInstr->addrSize) {                        // Jump instruction or memory operand
                expectedAddend = fInstr->addrPos - instrLength * 4;
            }
            else {                                         // Relocation of immediate operand
                expectedAddend = fInstr->immPos - instrLength * 4;
            }
        }
    }
    addend -= expectedAddend;
    if ((relocations[relocation-1].r_type & R_FORW_RELTYPEMASK) == R_FORW_REFP) {
        // has reference point
        outFile.put('-');
        uint32_t isym2 = relocations[relocation-1].r_refsym & 0x7FFFFFFF; // remove 0x80000000 flag
        writeSymbolName(isym2);
    }
    if (writeScale) {
        // write scale factor
        outFile.put(")/");
        outFile.putDecimal(1 << relocations[relocation-1].r_type & R_FORW_RELSCALEMASK);
    }

    // Check size of relocation
    if (addend > 0) {
        outFile.put('+'); outFile.putHex((uint32_t)addend);
    }
    else if (addend < 0) {
        outFile.put('-'); outFile.putHex(uint32_t(-addend));
    }
    if (codeMode > 1 && !writeScale) outFile.put(')');

    // Check for errors
    if (n > 1) writeError("Overlapping relocations here");
    uint32_t relSize = relocationSizes[relocations[relocation-1].r_type >> 8 & 0x0F];
    if (relSize < size) writeWarning("Relocation size less than data field");
    if (relSize > size) writeError("Relocation size bigger than data field");
}

void CDisassembler::writeJumpTarget(uint32_t src, uint32_t size) {
    // Write relocation jump target for this source position
    // Find relocation
    ElfFwcReloc rel;
    rel.r_offset = src;
    rel.r_section = section;
    //uint32_t irel;  // index to relocation record
    uint32_t n = relocations.findAll(&relocation, rel);
    if (n == 0) return;
    if (n > 1) {
        writeWarning(n ? "Overlapping relocations" : "No relocation found here");
        return;
    }
    relocation++;    // add 1 to avoid zero
    relocations[relocation-1].r_refsym |= 0x80000000;              // Remember relocation has been used OK
                                                                   // write scale factor if scale factor != 1 and not a jump target
    if (codeMode > 1) outFile.put('(');
    uint32_t isym = relocations[relocation-1].r_sym;
    writeSymbolName(isym);
    // Find any addend
    int32_t expectedAddend = 0;
    int32_t addend = relocations[relocation-1].r_addend;
    if ((relocations[relocation-1].r_type & R_FORW_RELTYPEMASK) == R_FORW_SELFREL && fInstr) {
        expectedAddend = fInstr->jumpPos - instrLength * 4;
    }
    addend -= expectedAddend;

    // Check size of relocation
    uint32_t expectedRelSize = size;                              // Expected size of relocation
    if (fInstr) {
        expectedRelSize = fInstr->jumpSize;
    }
    if (addend > 0) {
        outFile.put('+'); outFile.putHex((uint32_t)addend);
    }
    else if (addend < 0) {
        outFile.put('-'); outFile.putHex(uint32_t(-addend));
    }
    if (codeMode > 1) outFile.put(')');

    // Check for errors
    if (n > 1) writeError("Overlapping relocations here");
    uint32_t relSize = relocationSizes[relocations[relocation-1].r_type >> 8 & 0x0F];
    if (relSize < expectedRelSize) writeWarning("Relocation size less than data field");
    if (relSize > expectedRelSize) writeError("Relocation size bigger than data field");
}


/*
int CDisassembler::writeFillers() {
    return 1;  
}

void CDisassembler::writeAlign(uint32_t a) {
    // Write alignment directive
    outFile.put("ALIGN");
    outFile.tabulate(asmTab1);
    outFile.putDecimal(a);
    outFile.newLine();
} */

void CDisassembler::writeFileBegin() {
    outFile.setFileType(FILETYPE_ASM);
    if (debugMode) return;

    // Initial comment
    outFile.put(commentSeparator);
    if (outputFile == cmd.outputListFile) {
        outFile.put(" Assembly listing of file: ");
    }
    else {    
        outFile.put(" Disassembly of file: ");
    }
    outFile.put(cmd.getFilename(cmd.inputFile));
    outFile.newLine();
    // Date and time. 
    // Note: will fail after year 2038 on computers that use 32-bit time_t
    time_t time1 = time(0);
    char * timestring = ctime(&time1);
    if (timestring) {
        // Remove terminating '\n' in timestring
        for (char *c = timestring; *c; c++) {
            if (*c < ' ') *c = 0;
        }
        // Write date and time as comment
        outFile.put(commentSeparator); outFile.put(' ');
        outFile.put(timestring);
        outFile.newLine();
    }
    // Write special symbols and addresses if executable file
    if (isExecutable) {
        outFile.newLine();  outFile.put(commentSeparator);
        outFile.put(" __ip_base = ");  outFile.putHex(fileHeader.e_ip_base); 
        outFile.newLine();  outFile.put(commentSeparator);
        outFile.put(" __datap_base = ");  outFile.putHex(fileHeader.e_datap_base); 
        outFile.newLine();  outFile.put(commentSeparator);
        outFile.put(" __threadp_base = ");  outFile.putHex(fileHeader.e_threadp_base); 
        outFile.newLine();  outFile.put(commentSeparator);
        outFile.put(" __entry_point = ");  outFile.putHex(fileHeader.e_entry); 
        outFile.newLine();
    }

    // Write imported and exported symbols
    outFile.newLine(); 
    writePublicsAndExternals();
}


void CDisassembler::writePublicsAndExternals() {
    // Write public and external symbol definitions
    if (debugMode) return;
    uint32_t i;                                     // Loop counter
    uint32_t linesWritten = 0;                      // Count lines written

    // Loop through public symbols
    for (i = 0; i < symbols.numEntries(); i++) {
        if (symbols[i].st_bind && symbols[i].st_section) {
            // Symbol is public
            outFile.put("public ");
            // Write name
            writeSymbolName(i);
            // Code or data
            if (symbols[i].st_type == STT_FUNC) {
                outFile.put(": function");
                if (symbols[i].st_other & STV_REGUSE) {
                    // Write register use
                    outFile.put(", registeruse = ");  outFile.putHex(symbols[i].st_reguse1);
                    outFile.put(", ");  outFile.putHex(symbols[i].st_reguse2);
                }
            }
            else if (symbols[i].st_other & STV_EXEC) outFile.put(": function");
            else if (symbols[i].st_type == STT_OBJECT || symbols[i].st_type == STT_SECTION) {
                // data object. get base pointer
                if (symbols[i].st_other & (STV_IP | STV_EXEC)) outFile.put(": ip");
                else if (symbols[i].st_other & STV_DATAP) outFile.put(": datap");
                else if (symbols[i].st_other & STV_THREADP) outFile.put(": threadp");
                else if (symbols[i].st_other & STV_WRITE) outFile.put(": datap");
            }
            //else if (symbols[i].st_type == STT_FILE) outFile.put(": filename");
            //else if (symbols[i].st_type == STT_SECTION) outFile.put(": section");
            else if (symbols[i].st_type == STT_CONSTANT) {
                outFile.put(": constant"); outFile.newLine();  // write value
                outFile.put("% "); writeSymbolName(i); outFile.put(" = ");                 
                outFile.putHex(symbols[i].st_value);
            }
            else if (symbols[i].st_type == 0) {
                outFile.put(": absolute"); outFile.newLine(); 
            }
            else {
                outFile.put(": unknown type. type="); outFile.putHex(symbols[i].st_type);
                outFile.put(", bind="); outFile.putHex(symbols[i].st_bind);
                outFile.put(", other="); outFile.putHex(symbols[i].st_other);
            }
            // Check if weak or communal
            if (symbols[i].st_bind & STB_WEAK) {
                outFile.put(" weak");
            }
            if (symbols[i].st_type == STT_COMMON || (symbols[i].st_other & STV_COMMON)) outFile.put(", communal");
            outFile.newLine();  linesWritten++;
        }
    }
    // Blank line if anything written
    if (linesWritten) {
        outFile.newLine();
        linesWritten = 0;
    }
    // Loop through external symbols
    for (i = 0; i < symbols.numEntries(); i++) {
        if (symbols[i].st_bind && !symbols[i].st_section) {
            // Symbol is external
            outFile.put("extern ");
            // Write name
            writeSymbolName(i);
            // Code or data
            if (symbols[i].st_type == STT_FUNC) {
                outFile.put(": function");
                if (symbols[i].st_other & STV_REGUSE) {
                    // Write register use
                    outFile.put(", registeruse = ");  outFile.putHex(symbols[i].st_reguse1);
                    outFile.put(", ");  outFile.putHex(symbols[i].st_reguse2);
                }
            }
            else if (symbols[i].st_other & STV_EXEC) outFile.put(": function");
            //else if (symbols[i].st_other & (STV_READ | SHF_WRITE)) outFile.put(": data");
            else if (symbols[i].st_other & STV_IP) outFile.put(": ip");
            else if (symbols[i].st_other & STV_DATAP) outFile.put(": datap");
            else if (symbols[i].st_other & STV_THREADP) outFile.put(": threadp");
            else if (symbols[i].st_type == STT_OBJECT) outFile.put(": datap");
            else if (symbols[i].st_type == STT_CONSTANT) outFile.put(": constant");
            else if (symbols[i].st_type == 0) outFile.put(": absolute");
            else {
                outFile.put(": unknown type. type="); outFile.putHex(symbols[i].st_type);
                outFile.put(", other="); outFile.putHex(symbols[i].st_other);
            }
            // Check if weak or communal
            if (symbols[i].st_bind & STB_WEAK) {
                if (symbols[i].st_bind == STB_UNRESOLVED) outFile.put(" // unresolved!");
                else outFile.put(", weak");
            }
            if (symbols[i].st_type == STT_COMMON) outFile.put(", communal");
            // Finished line
            outFile.newLine();  linesWritten++;
        }
    }
    // Blank line if anything written
    if (linesWritten) {
        outFile.newLine();
        linesWritten = 0;
    }
}


void CDisassembler::writeFileEnd() {
    // Write end of file
}

void CDisassembler::writeSectionBegin() {
    // Write begin of section
    outFile.newLine();                            // Blank line

    // Check if section is valid
    if (section == 0 || section >= sectionHeaders.numEntries()) {
        // Illegal segment entry
        outFile.put("UNKNOWN SEGMENT");  outFile.newLine(); 
        return;
    }

    // Write segment name
    writeSectionName(section); outFile.put(" ");
    // tabulate
    outFile.tabulate(asmTab1);
    // Write "segment"
    outFile.put("section");

    // Write type
    if (sectionHeaders[section].sh_flags & SHF_READ) outFile.put(" read");
    if (sectionHeaders[section].sh_flags & SHF_WRITE) outFile.put(" write");
    if (sectionHeaders[section].sh_flags & SHF_EXEC) outFile.put(" execute");
    else if (sectionHeaders[section].sh_flags & SHF_IP) outFile.put(" ip");
    if (sectionHeaders[section].sh_flags & SHF_DATAP) outFile.put(" datap");
    if (sectionHeaders[section].sh_flags & SHF_THREADP) outFile.put(" threadp");
    if (sectionHeaders[section].sh_flags & SHF_EXCEPTION_HND) outFile.put(" exception_hand");
    if (sectionHeaders[section].sh_flags & SHF_EVENT_HND) outFile.put(" event_hand");
    if (sectionHeaders[section].sh_flags & SHF_DEBUG_INFO) outFile.put(" debug_info");
    if (sectionHeaders[section].sh_flags & SHF_COMMENT) outFile.put(" comment_info");
    if (sectionHeaders[section].sh_type == SHT_NOBITS) outFile.put(" uninitialized");
    if (sectionHeaders[section].sh_type == SHT_COMDAT) outFile.put(" communal");    

    // Write alignment
    uint32_t align = 1 << sectionHeaders[section].sh_align;
    outFile.put(" align="); 
    if (align < 16) outFile.putDecimal(align); else outFile.putHex(align);

    // tabulate to comment
    outFile.put(" ");  outFile.tabulate(asmTab3);
    outFile.put(commentSeparator);
    if (codeMode == 1) {  // code section
        outFile.put(" address/4. ");
    }
    else {
        outFile.put(" address.   ");
    }
    // Write section number
    outFile.put("section ");  
    outFile.putDecimal(section);
    // Write library and module, if available
    if (sectionHeaders[section].sh_module && sectionHeaders[section].sh_module < secStringTableLen) {
        outFile.put(". ");
        if (sectionHeaders[section].sh_library) {
            outFile.put(secStringTable + sectionHeaders[section].sh_library);
            outFile.put(':');
        }
        outFile.put(secStringTable + sectionHeaders[section].sh_module);
    }

    // New line
    outFile.newLine();
}


void CDisassembler::writeSectionEnd() {
    // Write end of section
   outFile.newLine();

   // Write segment name
   writeSectionName(section);  outFile.put(" ");  outFile.tabulate(asmTab1);
   // Write "segment"
   outFile.put("end");  outFile.newLine();
}


void CDisassembler::writeInstruction() {
    // Write instruction and operands
    // Check if instruction crosses section boundary
    if (iInstr + instrLength * 4 > sectionEnd) writeError("Instruction crosses section boundary");

    // Find instruction in instruction_list
    SInstruction2 iRecSearch;

    iRecSearch.format = format;
    iRecSearch.category = fInstr->category;
    iRecSearch.op1 = pInstr->a.op1;
    relocation = 0;

    if (iRecSearch.category == 4) {                        // jump instruction
        // Set op1 = opj for jump instructions in format 2.5.x and 3.1.0
        if (fInstr->imm2 & 0x80) {
            iRecSearch.op1 = pInstr->b[0];                 // OPJ is in IM1
            if (fInstr->imm2 & 0x40) iRecSearch.op1 = 63;  // OPJ has fixed value
            if (fInstr->imm2 & 0x10) iRecSearch.op1 = pInstr->b[7];  // OPJ is in upper part of IM6
        }
        // Set op1 for template D
        if (fInstr->tmplate == 0xD) iRecSearch.op1 &= 0xF8;
    }

    // Insert op2 only if template E
    if (instrLength > 1 && fInstr->tmplate == 0xE && !(fInstr->imm2 & 0x100)) {
        iRecSearch.op2 = pInstr->a.op2;
    }
    else iRecSearch.op2 = 0;

    uint32_t index, n, i;
    n = instructionlist.findAll(&index, iRecSearch);
    if (n == 0) {    // Instruction not found in list
        writeWarning("Unknown instruction: ");
        for (i = 0; i < instrLength; i++) {
            outFile.putHex(pInstr->i[i]);
            if (i + 1 < instrLength) outFile.put(" ");
        }
        writeCodeComment();  outFile.newLine();
        return;
    }
    // One or more matches in instruction table. Check if one of these fits the operand type and format
    uint32_t otMask = 0x101 << operandType; // operand type mask for supported + optional
    bool otFits = true;          // Check if operand type fits
    bool formatFits = true;      // Check if format fits
    for (i = 0; i < n; i++) {    // search through matching instruction table entries
        if (operandType < 4 && !(fInstr->vect & 1)) {   // general purpose register            
            otFits = (instructionlist[index + i].optypesgp & otMask) != 0;
        }
        else { // vector register
            otFits = ((instructionlist[index + i].optypesscalar | instructionlist[index + i].optypesvector) & otMask) != 0;
        }
        if (fInstr->category >= 3) {
            // Multi format or jump instruction. Check if format allowed
            formatFits = (instructionlist[index+i].format & ((uint64_t)1 << fInstr->formatIndex)) != 0;
        }
        if (instructionlist[index+i].opimmediate == OPI_IMPLICIT) {
            // check if implicit operand fits
            const uint8_t * bb = pInstr->b;
            uint32_t x = 0; // get value of immediate operand
            switch (fInstr->immSize) {
            case 1:   // 8 bits
                x = *(int8_t*)(bb + fInstr->immPos);
                break;
            case 2:    // 16 bits
                x = *(int16_t*)(bb + fInstr->immPos);
                break;
            case 4: default:  // 32 bits
                x = *(int32_t*)(bb + fInstr->immPos);
                break;
            }
            if (instructionlist[index+i].implicit_imm != x) formatFits = false;            
        }
        if (otFits && formatFits) {
            index += i;          // match found
            break;
        }
    }
    if (!otFits) {
        writeWarning("No instruction fits the operand type");
    }
    else if (!formatFits) {
        writeWarning("Error in instruction format");
    }
    // Save pointer to record
    iRecord = &instructionlist[index];

    // Template C or D has no OT field. Get operand type from instruction list if template C or D
    if (((iRecord->templt) & 0xFE) == 0xC) {
        uint32_t i, optypeSuppport = iRecord->optypesgp;
        if (fInstr->vect) optypeSuppport = iRecord->optypesscalar | iRecord->optypesvector;
        for (i = 0; i < 16; i++) {                     // Search for supported operand type
            if (optypeSuppport & (1 << i)) break;
        }
        operandType = i & 7;
    }
    // Get variant and options
    variant = iRecord->variant;

    // Write jump instruction or normal instruction
    if (fInstr->category == 4 && fInstr->jumpSize) {
        writeJumpInstruction();
    }
    else {
        writeNormalInstruction();
    }
    // Write comment    
    writeCodeComment();

    // End instruction
    outFile.newLine();
}

// Select a register from template
uint8_t getRegister(const STemplate * pInstr, int i) {
    // i = 5: RT, 6: RS, 7: RU, 8: RD
    uint8_t r = 0xFF;
    switch (i) {
    case 5: r = pInstr->a.rt;  break;
    case 6: r = pInstr->a.rs;  break;
    case 7: r = pInstr->a.ru;  break;
    case 8: r = pInstr->a.rd;  break;
    }
    return r;
}

uint8_t findFallback(SFormat const * fInstr, STemplate const * pInstr, int nOperands) {
    // Find the fallback register for an instruction code.
    // The return value is the register that is used for fallback
    // The return value is 0xFF if the fallback is zero or there is no fallback
    if (fInstr->tmplate != 0xA && fInstr->tmplate != 0xE) {
        return 0xFF;                                       // cannot have fallback
    }

    uint8_t operands[6] = {0,0,0,0,0,0};                   // Make list of operands

    int j = 5;
    if (fInstr->opAvail & 0x01) operands[j--] = 1;         // immediate operand
    if (fInstr->opAvail & 0x02) operands[j--] = 2;         // memory operand
    if (fInstr->opAvail & 0x10) operands[j--] = 5;         // register RT
    if (fInstr->opAvail & 0x20) operands[j--] = 6;         // register RS
    if (fInstr->opAvail & 0x40) operands[j--] = 7;         // register RU
    //if (fInstr->opAvail & 0x80) operands[j--] = 8;       // don't include register RD yet
    uint8_t fallback;                                      // fallback register
    bool fallbackSeparate = false;                         // fallback register is not first source register

    if (nOperands >= 3 && j < 3) {
        fallback = operands[3];                            // first of three source operands
    }
    else if (5-j-nOperands > 1) {                          // more than one vacant register field
        fallback = operands[3];                            // first of three possible source operands
        fallbackSeparate = true;
    }
    else if (5-j-nOperands == 1) {                         // one vacant register field used for fallback
        fallback = operands[j+1];
        fallbackSeparate = true;
    }
    else if (5-j-nOperands == 0) {                         // no vacant register field. RD not used for source operand
        fallback = operands[j+1];                          // first source operand
    }
    else if (fInstr->opAvail & 0x80) {                     // RD is first source operand
        fallback = 8;                                      // fallback is RD
    }
    else {
        fallback = 0xFF;
    }
    fallback = getRegister(pInstr, fallback);              // find register in specified register field
    if (fallback == 0x1F && fallbackSeparate) {
        return 0xFF;                                       // fallback is zero if register 31 is specified and not also a source register
    }
    return fallback;
}


void CDisassembler::writeNormalInstruction() {
    // Write operand type
    if (!((variant & VARIANT_D0) /*|| iRecord->sourceoperands == 0*/)) { // skip if no operand type
        if ((variant & VARIANT_U0) && operandType < 5 && !debugMode) outFile.put('u');   // Unsigned
        else if (variant & VARIANT_U3 && operandType < 5) {                
            // Unsigned if option bit 5 is set. 
            // Option bit is in IM5 in E formats
            if (fInstr->tmplate == 0xE && (fInstr->imm2 & 2) && (pInstr->a.im5 & 0x8) && !debugMode) {
                outFile.put('u');
            }
        }
        // special cases
        if (iRecord->id == II_COMPRESS) {
            switch (operandType & 0xFF) {  // change OT of input ot OT of output
            case TYP_FLOAT16 & 0xFF:
                operandType = 0;
                break;
            case 5:
                operandType = TYP_FLOAT16 & 0xFF;
                break;
            case 6:
                operandType = TYP_FLOAT32 & 0xFF;
                break;
            case 7:
                operandType = TYP_FLOAT64 & 0xFF;
                break;
            case 0:
                operandType = TYP_INT128 & 0xFF;  // actually INT4
                break;
            case 1:
                operandType = TYP_INT8 & 0xFF;
                break;
            case 2:
                operandType = TYP_INT16 & 0xFF;
                break;
            case 3:
                operandType = TYP_INT32 & 0xFF;
                break;
            case 4:
                operandType = TYP_INT64 & 0xFF;
                break; 
            }
        }

        outFile.tabulate(asmTab0);
        writeOperandType(operandType); outFile.put(' ');
    }
    outFile.tabulate(asmTab1);

    // Write destination operand
    if (!(variant & (VARIANT_D0 | VARIANT_D1 | VARIANT_D3))) {          // skip if no destination operands        
        if (variant & VARIANT_M0) {
            writeMemoryOperand();                          // Memory destination operand
        }
        else {
            if (variant & VARIANT_SPECD) writeSpecialRegister(pInstr->a.rd, variant >> VARIANT_SPECB);
            else if (fInstr->vect == 0 || (variant & VARIANT_R0)) writeGPRegister(pInstr->a.rd);
            else writeVectorRegister(pInstr->a.rd);
        }
        outFile.put(" = ");
    }

    // Write instruction name
    outFile.put(iRecord->name);

    /*  Source operands are selected according to the following algorithm:
    1.  Read nOp = number of operands from instruction list.
    2.  Select nOp operands from the following list, in order of priority:
        immediate, memory, RT, RS, RU, RD
        If one in the list is not available, go to the next
    3.  The selected operands are used as source operands in the reversed order
    */
    int nOperands = (int)iRecord->sourceoperands;                // Number of source operands

    // Make list of operands from available operands. 0=none, 1=immediate, 2=memory, 5=RT, 6=RS, 7=RU, 8=RD
    uint8_t opAvail = fInstr->opAvail;    // Bit index of available operands
                                          // opAvail bits: 1 = immediate, 2 = memory,
                                          // 0x10 = RT, 0x20 = RS, 0x40 = RU, 0x80 = RD 
    if (fInstr->category != 3) {                           // Single format instruction. Immediate operand determined by instruction table
        if (iRecord->opimmediate) opAvail |= 1;
        else opAvail &= ~1;
    }
    if (variant & VARIANT_M0) opAvail &= ~2;               // Memory operand already written as destination

    // (simular to emulator1.cpp:)
    // Make list of operands from available operands.
    // The operands[] array must have 6 elements to avoid overflow here,
    // even if some elements are later overwritten and used for other purposes
    uint8_t operands[6] = {0,0,0,0,0,0};                   // Make list of operands

    int j = 5;
    if (opAvail & 0x01) operands[j--] = 1;                 // immediate operand
    if (opAvail & 0x02) operands[j--] = 2;                 // memory operand
    if (opAvail & 0x10) operands[j--] = 5;                 // register RT
    if (opAvail & 0x20) operands[j--] = 6;                 // register RS
    if (opAvail & 0x40) operands[j--] = 7;                 // register RU
    if (opAvail & 0x80) operands[j--] = 8;                 // register RD
    operands[0] = 8;                                       // destination

    // Write source operands
    if (nOperands) {                                   // Skip if no source operands
        outFile.put("(");
        // Loop through operands
        int iop = 0;                                      // operand number
        for (j = 6 - nOperands; j < 6; j++, iop++) {
            uint8_t reg = getRegister(pInstr, operands[j]);// select register
            //uint8_t reg = operands[j];// select register
            switch (operands[j]) {
            case 1:  // Immediate operand
                writeImmediateOperand();
                break;
            case 2:  // Memory operand
                writeMemoryOperand();
                break;
            case 5:  // RT
                if (variant & VARIANT_SPECS) writeSpecialRegister(reg, variant >> VARIANT_SPECB);
                else if (fInstr->vect == 0 || (variant & VARIANT_RL) || ((uint32_t)variant & VARIANT_R123 & (1 << (VARIANT_R1B + iop)))) writeGPRegister(reg);
                else writeVectorRegister(reg);
                break;
            case 6:  // RS
            case 7:  // RU
                if (variant & VARIANT_SPECS) writeSpecialRegister(reg, variant >> VARIANT_SPECB);
                else if (fInstr->vect == 0 || ((uint32_t)variant & VARIANT_R123 & (1 << (VARIANT_R1B + iop)))) writeGPRegister(reg);
                else writeVectorRegister(reg);
                break;
            case 8:  // RD
                if (variant & VARIANT_SPECS) writeSpecialRegister(reg, variant >> VARIANT_SPECB);
                else if (fInstr->vect == 0 || ((uint32_t)variant & VARIANT_R123 & (1 << (VARIANT_R1B + iop))) || (variant & VARIANT_D3R0) == VARIANT_D3R0) {
                    writeGPRegister(reg);
                }
                else writeVectorRegister(reg);
                break;
            }
            if (operands[j] && j < 5 &&
            (iRecord->opimmediate != OPI_IMPLICIT || operands[j+1] != 1)) {
                outFile.put(", ");              // Comma if not the last operand
            }
        }
        // end parameter list
        outFile.put(")");    // we prefer to end the parenthesis before the mask and options

        // write mask register
        if ((fInstr->tmplate == 0xA || fInstr->tmplate == 0xE) && (pInstr->a.mask != 7 || (variant & VARIANT_F1))) {
            if (pInstr->a.mask != 7) {
                outFile.put(", mask=");
                if (fInstr->vect) writeVectorRegister(pInstr->a.mask); else writeGPRegister(pInstr->a.mask);
            }
            // write fallback
            if (!(variant & VARIANT_F0)) {
                uint8_t fb = findFallback(fInstr, pInstr, nOperands);       // find fallback register
                if (fb == 0xFF) {
                    outFile.put(", fallback=0");
                }
                else if (!(variant & VARIANT_F1) || getRegister(pInstr, operands[6-nOperands]) != fb)  {
                    outFile.put(", fallback=");
                    if (fInstr->vect) writeVectorRegister(fb & 0x1F); else writeGPRegister(fb & 0x1F);
                }
            }
        }
        // write options = IM5, if IM5 is used and not already written by writeImmediateOperand
        if ((variant & VARIANT_On) && (fInstr->imm2 & 2) 
        && (fInstr->category == 3 || (iRecord->opimmediate != 0 && iRecord->opimmediate != OPI_INT886))
            ) {
            outFile.put(", options="); 
            outFile.putHex(pInstr->a.im5); 
        }
    }
}


void CDisassembler::writeJumpInstruction(){
    // Write operand type
    if (!(variant & VARIANT_D0 || iRecord->sourceoperands == 1)) { // skip if no operands other than target
        outFile.tabulate(asmTab0);
        if ((variant & VARIANT_U0) && operandType < 5) outFile.put("u"); // unsigned
        writeOperandType(operandType);
    }
    outFile.tabulate(asmTab1);

    // Split instruction name into arithmetic operation and jump condition
    char iname[maxINameLen+1];
    char * jname;
    strncpy(iname, iRecord->name, maxINameLen);  iname[maxINameLen] = 0;
    jname = strchr(iname, '/');
    if (jname) {
        *jname = 0; // end first part of name
        jname++;    // point to second part of name
    }
    else jname = iname;

    if (iRecord->sourceoperands > 1) {
        // Instruction has arithmetic operands

        if (!(variant & (VARIANT_D0 | VARIANT_D1 | VARIANT_D3))) {
            // Write destination operand
            writeRegister(pInstr->a.rd, operandType);
            outFile.put(" = ");
        }

        // Write first part of instruction name
        outFile.put(iname);  outFile.put("(");

        // Write arithmetic operands
        if (iRecord->sourceoperands > 2) {
            if ((fInstr->opAvail & 0x30) == 0x30) {  // RS and RT
                writeRegister(pInstr->a.rs, operandType); outFile.put(", ");
                writeRegister(pInstr->a.rt, operandType);             
            }
            else {
                uint32_t r1 = pInstr->a.rd;                              // First source operand
                if ((fInstr->opAvail & 0x21) == 0x21) r1 = pInstr->a.rs; // Two registers and an immediate operand        
                writeRegister(r1, operandType);                          // Write operand
                outFile.put(", ");

                // Second source operand
                if (fInstr->opAvail & 2) {
                    writeMemoryOperand();
                    if (fInstr->opAvail & 1) {
                        outFile.put(", "); writeImmediateOperand();
                    }
                }
                else if (fInstr->opAvail & 1) {
                    writeImmediateOperand();
                }
                else {
                    writeRegister(pInstr->a.rs, operandType);
                }
            }
        }
        else {
            writeRegister(pInstr->a.rs, operandType);                // the only operand is rs
        }

        // End operand list
        if (fInstr->opAvail & 0x80) outFile.put("), ");

    }
    // Write second part of instruction name
    outFile.put(jname);

    // Write jump target
    outFile.put(' ');
    writeJumpTarget(iInstr + fInstr->jumpPos, fInstr->jumpSize);
}

void CDisassembler::writeCodeComment() {
    // Write hex listing of instruction as comment after single-format or multi-format instruction
    //    uint32_t i;                                     // Index to current byte
    //    uint32_t fieldSize;                             // Number of bytes in field
    //    const char * spacer;                          // Space between fields

    outFile.tabulate(asmTab3);                    // tabulate to comment field
    if (debugMode) return;
    outFile.put(commentSeparator); outFile.put(' '); // Start comment
                                                  
    writeAddress();            // Write address

    if (cmd.dumpOptions & 2) { // option "-b": binary listing
        outFile.putHex(pInstr->i[0], 2);
        if (instrLength > 1) {
            outFile.put(" "); outFile.putHex(pInstr->i[1], 2);
        }
        if (instrLength > 2) {
            outFile.put(" "); outFile.putHex(pInstr->i[2], 2);
        }
        outFile.put(" | ");
    }

    if (fInstr->tmplate == 0xE && instrLength > 1) {                       // format E
        // Write format_template op1.op2 ot rd.rs.rt.ru mask IM4 IM5
        outFile.putHex((format >> 8) & 0xF, 0); outFile.putHex(uint8_t(format), 2); outFile.put('_'); // format
        outFile.putHex(uint8_t(fInstr->tmplate), 0); outFile.put(' '); 
        outFile.putHex(uint8_t(pInstr->a.op1), 2); outFile.put('.'); // op1.op2
        if (!(fInstr->imm2 & 0x100)) {
            outFile.putHex(uint8_t(pInstr->a.op2), 0); outFile.put(' '); 
        }
        outFile.putHex(operandType, 0); outFile.put(' ');
        outFile.putHex(uint8_t(pInstr->a.rd), 2);  outFile.put('.'); // registers rd,rs,rt,ru
        outFile.putHex(uint8_t(pInstr->a.rs), 2);  outFile.put('.');
        outFile.putHex(uint8_t(pInstr->a.rt), 2);  outFile.put('.');
        outFile.putHex(uint8_t(pInstr->a.ru), 2);  outFile.put(' ');
        if (pInstr->a.mask != 7) outFile.putHex(pInstr->a.mask, 0); // mask
        else outFile.put('_'); // no mask
        outFile.put(' ');
        outFile.putHex(pInstr->s[2], 2);  outFile.put(' ');  // IM4
        outFile.putHex(uint8_t(pInstr->a.im5), 2);           // IM5
        if (instrLength == 3) {
            outFile.put(' ');
            outFile.putHex(pInstr->i[2], 2);                 // IM7
        }
    }
    else if (fInstr->tmplate == 0xD) {
        // Write format_template op1 data
        outFile.putHex((format >> 8) & 0xF, 0); outFile.putHex(uint8_t(format), 2); outFile.put('_');
        outFile.putHex(uint8_t(fInstr->tmplate), 0); outFile.put(' ');
        outFile.putHex(uint8_t(pInstr->a.op1), 2); outFile.put(' ');
        outFile.putHex(uint32_t(pInstr->d.im3 & 0xFFFFFF), 0);
    }
    else {
        // Write format_template op1 ot rd.rs.rt mask
        outFile.putHex((format >> 8) & 0xF, 0); outFile.putHex(uint8_t(format), 2); outFile.put('_');
        outFile.putHex(uint8_t(fInstr->tmplate), 0); outFile.put(' ');
        outFile.putHex(uint8_t(pInstr->a.op1), 2); outFile.put(' ');
        if (fInstr->tmplate == 0xC) {                           // Format C has 16 bit immediate
            outFile.putHex(uint8_t(pInstr->a.rd), 2); outFile.put(' ');
            outFile.putHex(pInstr->s[0], 2);
        }
        else { // not format C
            outFile.putHex(operandType, 0); outFile.put(' ');
            outFile.putHex(uint8_t(pInstr->a.rd), 2); outFile.put('.');
            outFile.putHex(uint8_t(pInstr->a.rs), 2);
            if (fInstr->tmplate == 0xB) {                       // Format B has 8 bit immediate
                outFile.put(' '); outFile.putHex(pInstr->b[0], 2);
            }
            else {                                             // format A or E
                outFile.put('.'); outFile.putHex(uint8_t(pInstr->a.rt), 2); outFile.put(' ');
                if (pInstr->a.mask != 7) outFile.putHex(pInstr->a.mask, 0);
                else outFile.put('_'); // no mask            
            }
        }
        if (instrLength > 1) {
            outFile.put(' ');
            if (instrLength == 2) {                       // format A2, B2, C2
                outFile.putHex(pInstr->i[1], 2);
            }
            else if (instrLength == 3) {                       // format A3, B3
                uint8_t const * bb = pInstr->b;
                uint64_t q = *(uint64_t*)(bb + 4);
                outFile.putHex(q, 2);
            }
            else { // unsupported formats longer than 1
                for (uint32_t j = 1; j < instrLength; j++) {
                    outFile.putHex(pInstr->i[j], 2); outFile.put(' ');
                }
            }
        }
    }
    // Write relocation comment
    if (relocation && !(relocations[relocation-1].r_type & 0x80000000)) { // 0x80000000 indicates no real relocation
        uint32_t reltype = relocations[relocation-1].r_type;
        outFile.put(". Rel: ");
        const char * rtyp = "", * rsize = "";
        switch ((reltype >> 16) & 0xFF) {
        case R_FORW_ABS >> 16: 
            rtyp = "abs "; break;
        case R_FORW_SELFREL >> 16:
            rtyp = "ip "; break;
        case R_FORW_DATAP >> 16:
            rtyp = "datap "; break;
        case R_FORW_THREADP >> 16:
            rtyp = "threadp "; break;
        case R_FORW_REFP >> 16:
            rtyp = "refpt "; break;
        default: 
            rtyp = "other "; break;
        }
        switch ((reltype >> 8) & 0xFF) {
        case R_FORW_8 >> 8:
            rsize = "8 bit"; break;
        case R_FORW_16 >> 8: 
            rsize = "16 bit"; break;
        case R_FORW_32 >> 8: 
            rsize = "32 bit"; break;
        case R_FORW_64 >> 8: 
            rsize = "64 bit"; break;
        case R_FORW_32LO >> 8: 
            rsize = "32 low bits"; break;
        case R_FORW_32HI >> 8: 
            rsize = "32 high bits"; break;
        case R_FORW_64LO >> 8: 
            rsize = "64 low bits"; break;
        case R_FORW_64HI >> 8: 
            rsize = "64 high bits"; break;
        }
        int scale = 1 << (reltype & 0xF);
        outFile.put(rtyp);
        outFile.put(rsize);
        if (scale > 1) {
            outFile.put(" * ");
            outFile.putDecimal(scale);
        }
    }
    
    // Write warnings and errors detected after we started writing instruction
    if (instructionWarning) {
        if (instructionWarning & 0x100) {
            outFile.put(". Unsupported format for this instruction");
            instructionWarning = 0; // Suppress further warnings
        }
        if (instructionWarning & 0x200) {
            outFile.put(". Unsupported operand type for this instruction");
            instructionWarning = 0; // Suppress further warnings
        }
        if (instructionWarning & 4)  outFile.put(". Warning: float in double size field");
        if (instructionWarning & 2)  outFile.put(". Warning: unused immediate operand");
        if (instructionWarning & 1)  outFile.put(". Optional");
    }
}


const char * baseRegisterNames[4] = {"thread", "datap", "ip", "sp"};


void CDisassembler::writeMemoryOperand() {
    // Check if there is a memory operand
    if (fInstr->mem == 0) {
        writeWarning("No memory operand");
        return;
    }
    int itemsWritten = 0;                 // items inside []
    bool symbolFound = false;             // address corresponds to symbol

    // Check if there is a relocation here
    relocation = 0;  // index to relocation record
    if (fInstr->addrSize) {
        ElfFwcReloc rel;
        rel.r_offset = iInstr + fInstr->addrPos;
        rel.r_section = section;
        uint32_t nrel = relocations.findAll(&relocation, rel);
        if (nrel) relocation++;  // add 1 to avoid zero
    }
    // Enclose in square bracket
    outFile.put('[');
    uint32_t baseP = pInstr->a.rs;       // Base pointer is RS

    if (fInstr->mem & 0x10) {   // has relocated symbol
        if (relocation) {        
            writeRelocationTarget(iInstr + fInstr->addrPos, fInstr->addrSize);
            itemsWritten++;
        }
        else if (isExecutable) {
            // executable file has no relocation record. Find nearest symbol
            ElfFwcSym needle;  // symbol address to search for
            needle.st_section = 0;
            needle.st_value = 0;
            if (fInstr->addrSize > 1 && baseP >= 28 && baseP <= 30) {
                needle.st_section = 31 - baseP; // 1: IP, 2: datap, 3: threadp

                int64_t offset = 0;
                switch (fInstr->addrSize) {  // Read offset of correct size
                case 2:
                    offset = *(int16_t*)(sectionBuffer + iInstr + fInstr->addrPos);
                    break;
                case 4:
                    offset = *(int32_t*)(sectionBuffer + iInstr + fInstr->addrPos);
                    break;
                }
                switch (baseP) {
                case 28: // threadp
                    offset += fileHeader.e_threadp_base;
                    break;

                case 29: // datap
                    offset += fileHeader.e_datap_base;
                    break;

                case 30: // ip, self-relative
                    offset += sectionAddress + int64_t(iInstr) + instrLength * 4;
                    break;
                }
                needle.st_value = offset;     // Symbol position
                int32_t isym = symbols.findFirst(needle);
                if (isym >= 0) {   // symbol found at target address
                    writeSymbolName(isym);
                    symbolFound = true; itemsWritten++;
                }
                else { // find nearest preceding symbol
                    isym &= 0x7FFFFFFF;  // remove not-found bit
                    if (uint32_t(isym) < symbols.numEntries()) {  // near symbol found                    
                        if (isym > 0 && symbols[isym-1].st_section == needle.st_section) {
                            isym--;  // use nearest preceding symbol if in same section
                        }
                        if (symbols[isym].st_section == needle.st_section) { // write nearest symbol
                            writeSymbolName(isym);  outFile.put('+');
                            outFile.putHex((uint32_t)(offset - symbols[isym].st_value), 1); // write offset relative to symbol
                            symbolFound = true;  itemsWritten++;
                        }
                    }
                }
            }
        }
    }
    if (!symbolFound) {
        if (fInstr->addrSize > 1 && baseP >= 28 && !(fInstr->mem & 0x20)) {  // Special pointers used if at least 16 bit offset
            if (baseP == 31 || !relocation) {  // Do not write base pointer if implicit in relocated symbol
                if (itemsWritten) outFile.put('+');
                outFile.put(baseRegisterNames[baseP - 28]);  itemsWritten++;
            }
        }
        else {
            if (itemsWritten) outFile.put('+');
            writeGPRegister(baseP);  itemsWritten++;
        }
    }

    if ((fInstr->mem & 4) && pInstr->a.rt != 31) { // Has index in RT
        if (fInstr->scale & 4) { // Negative index
            outFile.put('-');  writeGPRegister(pInstr->a.rt);
        }
        else {  // Positive, scaled index
            if (itemsWritten) outFile.put('+');
            writeGPRegister(pInstr->a.rt);
            if ((fInstr->scale & 2) && operandType > 0) { // Index is scaled
                outFile.put('*');
                outFile.putDecimal(dataSizeTable[operandType & 7]);
            }
        }
        itemsWritten++;
    }
    if (fInstr->mem & 0x10) { // Has offset
        if (relocation || symbolFound) {   // has relocated symbol
            // has been written above
            //writeRelocationTarget(iInstr + fInstr->addrPos, fInstr->addrSize);
        }
        else {
            int32_t offset = 0;
            switch (fInstr->addrSize) {  // Read offset of correct size
            case 1:
                offset = *(int8_t*)(sectionBuffer + iInstr + fInstr->addrPos);
                break;
            case 2:
                offset = *(int16_t*)(sectionBuffer + iInstr + fInstr->addrPos);
                break;
            case 4:
                offset = *(int32_t*)(sectionBuffer + iInstr + fInstr->addrPos);
                break;
            }
            if (offset > 0) {    // Write positive offset
                outFile.put('+');
                outFile.putHex((uint32_t)offset, 1);
            }
            else if (offset < 0) {    // Write negative offset
                outFile.put('-');
                outFile.putHex((uint32_t)(-offset), 1);
            }
            if ((fInstr->scale & 1) && offset != 0) { // Offset is scaled
                outFile.put('*');
                outFile.putDecimal(dataSizeTable[operandType & 7]);
            }
            itemsWritten++;
        }
    }
    if (fInstr->mem & 0x20) {  // Has limit
        outFile.put(", limit=");
        if (fInstr->addrSize == 4) {   // 32 bit limit
            outFile.putHex(*(uint32_t*)(sectionBuffer + iInstr + fInstr->addrPos));
        }
        else {                       // 16 bit limit
            outFile.putHex(*(uint16_t*)(sectionBuffer + iInstr + fInstr->addrPos));
        }
    }
    if ((fInstr->vect & 2) && pInstr->a.rt != 31) {  // Has vector length
        outFile.put(", length=");
        writeGPRegister(pInstr->a.rt);
    }
    else if ((fInstr->vect & 4) && pInstr->a.rt != 31) {  // Has broadcast
        outFile.put(", broadcast=");
        writeGPRegister(pInstr->a.rt);
    }
    else if (fInstr->vect & 7 || ((fInstr->vect & 0x10) && (pInstr->a.ot & 4))) {  //  Scalar
        outFile.put(", scalar");        
    }

    outFile.put(']');  // End square bracket
}


void CDisassembler::writeImmediateOperand() {
    // Write immediate operand depending on type in instruction list
    // Check if there is a relocation here
    ElfFwcReloc rel;
    rel.r_offset = (uint64_t)iInstr + fInstr->immPos;
    rel.r_section = section;
    uint32_t irel;  // index to relocation record
    uint32_t numRel = relocations.findAll(&irel, rel);
    if (numRel) {  // Immediate value is relocated
        writeRelocationTarget(iInstr + fInstr->immPos, fInstr->immSize);
        return;
    }
    // Value is not relocated
    
    /*if ((variant & VARIANT_M1) && (fInstr->tmplate) == 0xE && (fInstr->opAvail & 2)) {
        // VARIANT_M1: immediate operand is in IM5
        outFile.putDecimal(pInstr->a.im5);
        return;
    } */
    const uint8_t * bb = pInstr->b;  // use this for avoiding pedantic warnings from Gnu compiler when type casting
    if (operandType == 1 && (variant & VARIANT_H0)) operandType = 8;  // half precision float
    if (operandType < 5 || iRecord->opimmediate || (variant & VARIANT_I2)) { 
        // integer, or type specified in instruction list
        // Get value of right size
        int64_t x = 0;
        switch (fInstr->immSize) {
        case 1:   // 8 bits
            x = *(int8_t*)(bb + fInstr->immPos);
            break;
        case 2:    // 16 bits
            x = *(int16_t*)(bb + fInstr->immPos);
            break;
        case 3:   // 24 bits, sign extend to 32 bits
            x = *(int32_t*)(bb + fInstr->immPos) << 8 >> 8;
            break;
        case 4:   // 32 bits
            x = *(int32_t*)(bb + fInstr->immPos);
            break;
        case 8:
            x = *(int64_t*)(bb + fInstr->immPos);
            break;
        case 0:
            if (fInstr->tmplate == 0xE) {
                x = (pInstr->s[2]);
            }
            break;
            // else continue in default:
        default:
            writeError("Unknown immediate size");
        }
        // Write in the form specified in instruction list
        switch (iRecord->opimmediate) {
        case 0:   // No form specified
        case OPI_OT:  // same as operand type
            if (fInstr->category == 1 && iRecord->opimmediate == 0 && x != 0) instructionWarning |= 2; // Immediate field not used in this instruction. Write nothing
            switch (fInstr->immSize) {  // Output as hexadecimal
            case 1:  
                if (operandType > 0) outFile.putDecimal((int32_t)x, 1);  // sign extend to larger size
                else outFile.putHex(uint8_t(x), 1);   
                break;
            case 2: 
                if ((fInstr->imm2 & 4) && pInstr->a.im5 && !(variant & VARIANT_On)) {  // constant is IM4 << IM5
                    if ((int16_t)x < 0) {
                        outFile.put('-');  x = -x;
                    }
                    outFile.putHex(uint16_t(x), 1);
                    outFile.put(" << ");
                    outFile.putDecimal(pInstr->a.im5);
                }
                else if (operandType > 1) {
                    outFile.putDecimal((int32_t)x, 1);  // sign extend to larger size
                }
                else {                
                    outFile.putHex(uint16_t(x), 1);
                }
                break;
            default:
            case 4:  
                if ((fInstr->imm2 & 8) && pInstr->a.im4) {  // constant is IM7 << IM4
                    if ((int32_t)x < 0) {
                        outFile.put('-');  x = -x;
                    }
                    outFile.putHex(uint32_t(x), 1);
                    outFile.put(" << ");
                    outFile.putDecimal(pInstr->a.im4);
                }
                else if (operandType <= 2) outFile.putHex(uint32_t(x), 1);  
                else if (operandType == 5 || operandType == 6) {
                    outFile.putFloat(*(float*)(bb + fInstr->immPos));
                }
                else {
                    outFile.putDecimal((int32_t)x, 1);  // sign extend to larger size
                }
                break;
            case 8:
                if (operandType == 6) outFile.putFloat(*(double*)(bb + fInstr->immPos));
                else outFile.putHex(uint64_t(x), 1);  
                break;
            }
            break;
        case OPI_INT8:
            outFile.putDecimal(int8_t(x), 1);
            break;
        case OPI_INT16:
            outFile.putDecimal(int16_t(x), 1);
            break;
        case OPI_INT32:
            outFile.putDecimal(int32_t(x), 1);
            break;
        case OPI_INT8SH:
            if (int8_t(x >> 8) < 0) {
                outFile.put('-');
                outFile.putHex(uint8_t(-int8_t(x >> 8)), 1);
            }
            else outFile.putHex(uint8_t(x >> 8), 1);
            outFile.put(" << ");
            outFile.putDecimal(uint8_t(x));
            break;
        case OPI_INT16SH16:
            if (x < 0) {
                outFile.put('-');
                x = -x;
            }
            outFile.putHex(uint16_t(x), 1);
            outFile.put(" << 16");
            break;
        case OPI_INT32SH32:
            outFile.putHex(uint32_t(x), 1);
            outFile.put(" << 32");
            break;
        case OPI_UINT8:
            outFile.putHex(uint8_t(x), 1);
            break;
        case OPI_UINT16:
            outFile.putHex(uint16_t(x), 1);
            break;
        case OPI_UINT32:
            outFile.putHex(uint32_t(x), 1);
            break;
        case OPI_INT64: case OPI_UINT64: 
            outFile.putHex(uint64_t(x), 1);
            break;
        case OPI_2INT8:                     // Two unsigned integers
            outFile.putHex(uint8_t(x), 1);  outFile.put(", ");
            outFile.putHex(uint8_t(x >> 8), 1);
            break;
        case OPI_INT886:                     // Three unsigned integers, including IM5
            outFile.putDecimal(uint8_t(x));  outFile.put(", ");
            outFile.putDecimal(uint8_t(x >> 8));  outFile.put(", ");
            outFile.putDecimal(uint8_t(pInstr->a.im5));
            break;
        case OPI_2INT16:                     // Two 16-bit unsigned integers
            outFile.putHex(uint16_t(x >> 16), 1);  outFile.put(", ");
            outFile.putHex(uint16_t(x), 1);
            break;
        case OPI_INT1632:                     // One 16-bit and one 32-bit unsigned integer
            outFile.putHex(uint32_t(pInstr->i[1]), 1);  outFile.put(", ");
            outFile.putHex(uint16_t(x), 1);            
            break;
        case OPI_2INT32:                     // Two 32-bit unsigned integer
            outFile.putHex(uint32_t(x >> 32), 1);  outFile.put(", ");
            outFile.putHex(uint32_t(x), 1);
            break;
        case OPI_INT1688:                     // 16 + 8 + 8 bits
            outFile.putHex(uint16_t(x), 1);  outFile.put(", ");
            outFile.putHex(uint8_t(x >> 16), 1);  outFile.put(", ");
            outFile.putHex(uint8_t(x >> 24), 1);
            break;
        case OPI_FLOAT16:                     // Half precision float
            outFile.putFloat(half2float(uint16_t(x)));
            break;
        case OPI_IMPLICIT:
            if (x != iRecord->implicit_imm) { // Does not match implicit value. Make value explicit
                if (iRecord->sourceoperands > 1) outFile.put(", ");
                outFile.putHex(uint8_t(x), 1);
            }
            break;
        default:
            writeWarning("Unknown immediate operand type");
        }        
    }
    else {  // floating point
        uint32_t immSize = fInstr->immSize;      // Size of immediate field
        if (immSize == 8 && operandType == 5)  {
            immSize = 4;  instructionWarning |= 4;  // float in double size field
        }
        switch (immSize) {
        case 1:    // 8 bits. float as integer
            outFile.putFloat((float)*(int8_t*)(bb + fInstr->immPos));
            break;
        case 2:  { // 16 bits
            uint16_t x = *(uint16_t*)(bb + fInstr->immPos);
            outFile.putFloat(half2float(x));
            break;}
        case 4:  { // float
            float x = *(float*)(bb + fInstr->immPos);
            outFile.putFloat(x);
            break;}
        case 8:  { // double
            double x = *(double*)(bb + fInstr->immPos);
            outFile.putFloat(x);
            break;}
        default: 
            writeError("unknown size for float operand");
        }
    }
}


void CDisassembler::writeRegister(uint32_t r, uint32_t ot) {
    if (r == 31 && !(ot & 4)) outFile.put("sp");
    else {    
        outFile.put(ot & 4 ? "v" : "r");  outFile.putDecimal(r);
    }
}


void CDisassembler::writeGPRegister(uint32_t r) {
    // Write name of general purpose register
    if (r == 31) outFile.put("sp");
    else {    
        outFile.put("r");  outFile.putDecimal(r);
    }
}


void CDisassembler::writeVectorRegister(uint32_t v) {
    // Write name of vector register
    outFile.put("v");  outFile.putDecimal(v);
}

// Special register types according to Xn and Yn in 'variant' field in instruction list
static const char * specialRegNamesPrefix[8] = {"?", "spec", "capab", "perf", "sys", "?", "?", "?"};
static const char * pointerRegNames[4] = {"threadp", "datap", "ip", "sp"};
static const char * specialRegNames[] = {"numcontr", "threadp", "datap", "?", "?", "?"};
void CDisassembler::writeSpecialRegister(uint32_t r, uint32_t type) {
    // Write name of other type of register
    if ((type & 0xF) == 0) {
        // May be special pointer
        if (r < 28) {
            writeGPRegister(r);
        }
        else {
            outFile.put(pointerRegNames[(r-28) & 3]);
        }
    }
    else if ((type & 0xF) == 1 && r <= 2) {
        // special registers with unique names
        outFile.put(specialRegNames[r]);
    }
    else {    
        outFile.put(specialRegNamesPrefix[type & 7]);    
        outFile.putDecimal(r);
    }
}


// Write name of operand type
static const char * operandTypeNames[8] = {
    "int8", "int16", "int32", "int64", "int128", "float", "double", "float128 "};

void CDisassembler::writeOperandType(uint32_t ot) {
    if ((variant & VARIANT_H0) && ot == 1) {
        outFile.put("float16");
    }
    else if ((variant & VARIANT_H5) && ot == 1 && fInstr->tmplate == 0xE && (pInstr->a.im5 & 0x20)) {
        outFile.put("float16");
    }
    else if (ot == (TYP_FLOAT16 & 0xFF)){
        outFile.put("float16");
    }
    else {
        outFile.put(operandTypeNames[ot & 7]);
    }
}


void CDisassembler::writeWarning(const char * w) {
    // Write warning to output file
    outFile.put(commentSeparator);
    outFile.put(" Warning: ");
    outFile.put(w);
    outFile.newLine();
}


void CDisassembler::writeError(const char * w) {
    // Write warning to output file
    outFile.put(commentSeparator);
    outFile.put(" Error: ");
    outFile.put(w);
    outFile.newLine();
}


void CDisassembler::finalErrorCheck() {
    // Check for illegal entries in symbol table and relocations table
    // Check for orphaned symbols
    uint32_t i;                                  // Loop counter
    uint32_t linesWritten = 0;                   // Count lines written
    // Check for orphaned symbols
    for (i = 0; i < symbols.numEntries(); i++) {
        if ((symbols[i].st_other & 0x80000000) == 0 
            && (symbols[i].st_section || symbols[i].st_value) 
            && symbols[i].st_type != STT_CONSTANT 
            && symbols[i].st_type != STT_FILE) {
            // This symbol has not been written out
            if (linesWritten == 0) {
                // First orphaned symbol. Write text            
                outFile.newLine();  outFile.newLine();  outFile.put(commentSeparator);
                outFile.put(" Warning: Symbols outside address range:");
                outFile.newLine();
            }
            outFile.put(commentSeparator); outFile.put(' ');
            writeSymbolName(i); outFile.put(" = "); 
            outFile.putHex(symbols[i].st_section, 0); outFile.put(':'); outFile.putHex(symbols[i].st_value, 0);
            outFile.newLine();  linesWritten++;
        }
    }
    // Check for orphaned relocations
    linesWritten = 0;
    for (i = 0; i < relocations.numEntries(); i++) {
        if (relocations[i].r_type == 0) continue;  // ignore empty relocation 0        
        if ((relocations[i].r_refsym & 0x80000000) == 0) {
            // This relocation has not been used
            if (linesWritten == 0) {
                // First orphaned symbol. Write text            
                outFile.newLine();  outFile.newLine();  outFile.put(commentSeparator);
                outFile.put(" Warning: Unused or misplaced relocations:");
                outFile.newLine();
            }
            outFile.put(commentSeparator); outFile.put(" at ");
            outFile.putHex(uint32_t(relocations[i].r_section)); outFile.put(':');  // Section
            outFile.putHex(uint32_t(relocations[i].r_offset));                     // Offset
            outFile.put(" to symbol ");
            writeSymbolName(relocations[i].r_sym & 0x7FFFFFFF);
            outFile.newLine();  linesWritten++;
        }
    }
}

void CDisassembler::writeAddress() {
    // write code address >> 2. Subtract ip_base to put code section at address 0
    uint64_t address = (iInstr + sectionAddress - fileHeader.e_ip_base) >> 2;

    if (fileHeader.e_ip_base + sectionEnd + sectionAddress > 0xFFFF * 4) {
        // Write 32 bit address
        outFile.putHex(uint32_t(address), 2);
    }
    else {
        // Write 16 bit address
        outFile.putHex(uint16_t(address), 2);
    }
    if (debugMode) outFile.put(" ");
    else outFile.put(" _ ");    // Space after address
}

void CDisassembler::setTabStops() {
    // set tab stops for output
    if (debugMode) {
        asmTab0 = 18;                        // Column for operand type
        asmTab1 = 26;                        // Column for opcode
        asmTab2 = 40;                        // Column for first operand
        asmTab3 = 64;                        // Column for destination value
    }
    else {
        asmTab0 =  0;                        // unused
        asmTab1 =  8;                        // Column for opcode
        asmTab2 = 16;                        // Column for first operand
        asmTab3 = 56;                        // Column for comment
    }
}

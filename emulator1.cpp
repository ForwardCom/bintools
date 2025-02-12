/****************************  emulator1.cpp  ********************************
* Author:        Agner Fog
* date created:  2018-02-18
* Last modified: 2025-01-21
* Version:       1.14
* Project:       Binary tools for ForwardCom instruction set
* Description:
* Basic functionality of the emulator
*
* Copyright 2018-2025 GNU General Public License http://www.gnu.org/licenses
*****************************************************************************/

#include "stdafx.h"


///////////////////
// CEmulator class
///////////////////

// constructor
CEmulator::CEmulator() {
    memory = 0;                                  // initialize
    memsize = 0;
    stackp = 0;
    // set defaults. may be changed by command line or file header:
    MaxVectorLength = 0x80;                      // 128 bytes = 1024 bits
    maxNumThreads = 1;                           // multithreading not supported yet
    stackSize = 0x100000;                        // 1 MB. data stack size for main thread
    callStackSize = 0x800;                       // call stack size for main thread
    heapSize = 0;                                // heap size for main thread
    environmentSize = 0x100;                     // maximum size of environment and command line data
}

// destructor
CEmulator::~CEmulator() {
    if (memory) delete[] memory;                 // free allocated program memory
}

// start
void CEmulator::go() {
    threads.setSize(maxNumThreads);              // initialize threads
    load();                                      // load executable file
    if (err.number()) return;
    if (fileHeader.e_flags & EF_RELOCATE) relocate();
    if (err.number()) return;

    // set up disassembler for output list
    if (cmd.outputListFile) disassemble();

    // update list of number of operands and other attributes of instructions
    updateNumOperands();

    // prepare main thread
    threads[0].setRegisters(this);
    // run main thread
    threads[0].run();
}

// load executable file into memory
void CEmulator::load() {
    const char * filename = cmd.getFilename(cmd.inputFile);
    read(filename);                              // read executable file
    if (err.number()) return;
    split();                                     // extract components
    if (getFileType() != FILETYPE_FWC || fileHeader.e_type != ET_EXEC) {
        err.submit(ERR_LINK_FILE_TYPE_EXE, filename);
        return;
    }
    // calculate necessary memory size
    uint64_t blocksize = 0;                      // size of block of segments with same base pointer
    uint32_t ph;                                 // program header index
    uint64_t align;                              // program header alignment
    uint64_t address;                            // current address
    uint32_t flags, lastflags;                   // flags of program header
    bool hasDataSegment = false;                 // check if there is a data segment header
    const uint32_t dataflags = SHF_READ | SHF_WRITE | SHF_ALLOC | SHF_DATAP; // expected flags for data segment

    memsize = environmentSize;                   // reserve space for environment in the beginning
    for (ph = 0; ph < programHeaders.numEntries(); ph++) {
        if (programHeaders[ph].p_vaddr == 0) {
            // start of a new block
            memsize += blocksize;

            if ((programHeaders[ph].p_flags & SHF_READ) && (ph+1 == programHeaders.numEntries())) {
                // This is the last data section
                // Make space for reading a vector beyond the end
                // Note: we cannot do this after the const section because the space there must have fixed size, provided by the linker
                uint32_t extraSpace = MaxVectorLength;
                if (extraSpace < DATA_EXTRA_SPACE) extraSpace = DATA_EXTRA_SPACE;
                programHeaders[ph].p_memsz += extraSpace;
            }
            align = (uint64_t)1 << programHeaders[ph].p_align;
            memsize = (memsize + align - 1) & -(int64_t)align;
            blocksize = programHeaders[ph].p_memsz;
        }
        else {
            // continuation of previous block
            blocksize += programHeaders[ph].p_vaddr + programHeaders[ph].p_memsz;
        }
        if ((programHeaders[ph].p_flags & dataflags) == dataflags) hasDataSegment = true;
    }
    if (!hasDataSegment) { // there is no data segment. make one for the stack
        ElfFwcPhdr dataSegment;
        zeroAllMembers(dataSegment);
        dataSegment.p_type = PT_LOAD;
        dataSegment.p_flags = dataflags;
        dataSegment.p_align = 3;
        programHeaders.push(dataSegment);
    }

    // end of last block
    memsize += blocksize;
    align = (uint64_t)1 << MEMORY_MAP_ALIGN;
    memsize = (memsize + align - 1) & -(int64_t)align;
    // add stack and heap
    memsize += stackSize + heapSize;
    // allocate memory
    memory = new int8_t[size_t(memsize)];
    if (!memory) {
        err.submit(ERR_MEMORY_ALLOCATION); 
        return;
    }
    memset(memory, 0, size_t(memsize));
    // start making memory map
    address = 0;  
    flags = SHF_READ | SHF_IP;  lastflags = flags;
    SMemoryMap mapentry = {address, flags};
    memoryMap.push(mapentry);
    // make space for environment
    address = environmentSize;
    for (ph = 0; ph < programHeaders.numEntries(); ph++) {
        flags = programHeaders[ph].p_flags & (SHF_PERMISSIONS | SHF_BASEPOINTER);
        if (flags != lastflags && (lastflags & SHF_IP) && (!(flags & SHF_IP))) {
            // insert stack here
            align = 8;
            address = (address + align - 1) & -(int64_t)align;
            flags = SHF_DATAP | SHF_READ | SHF_WRITE;
            mapentry.startAddress = address;
            mapentry.access_addend = flags;
            memoryMap.push(mapentry);
            address += stackSize;
            stackp = address;
            lastflags = flags;
            flags = programHeaders[ph].p_flags & (SHF_PERMISSIONS | SHF_BASEPOINTER);
        }
        if ((flags & SHF_PERMISSIONS) != (lastflags & SHF_PERMISSIONS)) {
            // start new map entry
            align = (uint64_t)1 << programHeaders[ph].p_align;
            address = (address + align - 1) & -(int64_t)align;
            mapentry.startAddress = address;
            mapentry.access_addend = flags;
            memoryMap.push(mapentry);
        }
        if (programHeaders[ph].p_vaddr == 0) {
            switch (flags & SHF_BASEPOINTER) {
            case SHF_IP:
                ip0 = address;  break;
            case SHF_DATAP:
                datap0 = address;  break;
            case SHF_THREADP:
                threadp0 = address;  break;
            }
        }
        // check integrity before copying data
        if (address + programHeaders[ph].p_filesz > memsize
            || programHeaders[ph].p_filesz > programHeaders[ph].p_memsz
            || programHeaders[ph].p_offset + programHeaders[ph].p_filesz > dataSize()) {
            err.submit(ERR_ELF_INDEX_RANGE);
            return;
        }
        // store address in program header
        programHeaders[ph].p_vaddr = address;
        // copy data
        memcpy(memory + address, dataBuffer.buf() + programHeaders[ph].p_offset, size_t(programHeaders[ph].p_filesz));
        address += programHeaders[ph].p_memsz;
        lastflags = flags;
    }
    // make terminating entry
    mapentry.startAddress = address;
    mapentry.access_addend = 0;
    memoryMap.push(mapentry);
}

// relocate any absolute addresses and system function id's
void CEmulator::relocate() {
    uint32_t r;                                  // relocation index
    uint32_t ph;                                 // program header index
    uint32_t rsection;                           // relocated section
    uint32_t phFistSection;                      // first section covered by program header
    uint32_t phNumSections;                      // number of sections covered by program header
    uint64_t sourceAddress;                      // address of relocation source
    uint64_t targetAddress;                      // address of relocation target
    const char * symbolname;                     // name of target symbol
    bool found;                                  // program header found
    for (r = 0; r < relocations.numEntries(); r++) {
        // loadtime relocations are listed first. stop at first non-loadtime record
        if (!(relocations[r].r_type & R_FORW_LOADTIME)) break;
        // find program header containing relocated section
        rsection = relocations[r].r_section; 
        found = false;
        for (ph = 0; ph < programHeaders.numEntries(); ph++) {
            phFistSection = (uint32_t)programHeaders[ph].p_paddr;
            phNumSections = (uint32_t)(programHeaders[ph].p_paddr >> 32);
            if (rsection >= phFistSection && rsection < phFistSection + phNumSections) {
                found = true;  break;
            }
        }
        if (!found) {
            err.submit(ERR_REL_SYMBOL_NOT_FOUND);  continue;
        }
        // calculate address of relocation source
        sourceAddress = programHeaders[ph].p_vaddr + sectionHeaders[rsection].sh_addr - sectionHeaders[phFistSection].sh_addr + relocations[r].r_offset;
        if (sourceAddress >= memsize) {
            err.submit(ERR_ELF_INDEX_RANGE);  continue;
        } 
        if ((relocations[r].r_type & R_FORW_RELTYPEMASK) == R_FORW_ABS) {
            // needs absolute address of target
            uint32_t symi = relocations[r].r_sym;
            if (symi >= symbols.numEntries()) {
                err.submit(ERR_ELF_INDEX_RANGE);  return;
            }
            ElfFwcSym & targetSym = symbols[symi];
            uint32_t tsec = targetSym.st_section;  // section of target symbol
            // find program header containing target section
            found = false;
            for (ph = 0; ph < programHeaders.numEntries(); ph++) {
                phFistSection = (uint32_t)programHeaders[ph].p_paddr;
                phNumSections = (uint32_t)(programHeaders[ph].p_paddr >> 32);
                if (tsec >= phFistSection && tsec < phFistSection + phNumSections) {
                    found = true;  break;
                }
            }
            if (!found) {
                err.submit(ERR_REL_SYMBOL_NOT_FOUND);  continue;
            }
            // calculate target address
            targetAddress = programHeaders[ph].p_vaddr + sectionHeaders[rsection].sh_addr - sectionHeaders[phFistSection].sh_addr + targetSym.st_value;
            if (targetAddress >= memsize) {
                err.submit(ERR_ELF_INDEX_RANGE);  continue;
            }
            // scale (scaling of absolute addresses is rarely used, but allowed)
            targetAddress >>= (relocations[r].r_type & R_FORW_RELSCALEMASK);
            // insert relocation of desired size
            switch (relocations[r].r_type & R_FORW_RELSIZEMASK) {
            case R_FORW_8:     // 8  bit relocation size
                if (targetAddress >> 8) goto OVERFLW;
                *(memory + sourceAddress) = int8_t(targetAddress);
                break;
            case R_FORW_16:    // 16 bit relocation size
                if (targetAddress >> 16) goto OVERFLW;
                *(uint16_t*)(memory + sourceAddress) = uint16_t(targetAddress);
                break;
            case R_FORW_32:    // 32 bit relocation size
                if (targetAddress >> 32) goto OVERFLW;
                *(uint32_t*)(memory + sourceAddress) = uint32_t(targetAddress);
                break;
            case R_FORW_32LO:  // Low  16 of 32 bits relocation
                *(uint16_t*)(memory + sourceAddress) = uint16_t(targetAddress);
                break;
            case R_FORW_32HI:  // High 16 of 32 bits relocation
                if (targetAddress >> 32) goto OVERFLW;
                *(uint16_t*)(memory + sourceAddress) = uint16_t(targetAddress >> 16);
                break;
            case R_FORW_64:    // 64 bit relocation size
                *(uint64_t*)(memory + sourceAddress) = uint64_t(targetAddress);
                break;
            case R_FORW_64LO:  // Low  32 of 64 bits relocation
                *(uint32_t*)(memory + sourceAddress) = uint32_t(targetAddress);
                break;
            case R_FORW_64HI:  // High 32 of 64 bits relocation
                *(uint32_t*)(memory + sourceAddress) = uint32_t(targetAddress >> 32);
                break;
            default:
            OVERFLW:
                symbolname = symbolNameBuffer.getString(targetSym.st_name);
                err.submit(ERR_LINK_RELOCATION_OVERFLOW, symbolname);
            }
        }
        else {
            // to do: get system function id from name
        }
    }
}

void CEmulator::disassemble() {              // make disassembly listing for debug output
    disassembler.copy(*this);                // copy ELF file
    disassembler.getComponents1();           // set up instruction list, etc.
    if (err.number()) return;
    //disassembler.outputFile = cmd.fileNameBuffer.pushString("ddd.txt");
    disassembler.debugMode = 1;              // produce disassembly for debug display/list
    disassembler.go();                       // disassemble
    if (err.number()) return;
    disassembler.getLineList(lineList);      // get cross reference list from address to disassembly output file
    lineList.sort();                         // only needed if multiple segments in lineList
    disassembler.getOutFile(disassemOut);    // get disassembly output file
    // replace all linefeeds by end of string
    for (uint32_t i = 0; i < disassemOut.dataSize(); i++) {
        if ((uint8_t)disassemOut.buf()[i] < ' ') disassemOut.buf()[i] = 0;
    }
}

void CEmulator::updateNumOperands() {        
    // Update numOperands table in format_tables.cpp from instruction_list.csv.
    // The tables in format_tables.cpp are supposed to be correct, but for the sake
    // of easy changes, the number of operands and option bits in the file 
    // instruction_list.csv may override these values.

    CDynamicArray<SInstruction2> &instructionlist = disassembler.getInstructionList();
    SInstruction2 const * iRecord;           // Pointer to instruction table entry
    for (uint32_t i = 0; i < instructionlist.numEntries(); i++) {
        iRecord = &(instructionlist[i]);
        uint8_t category = iRecord->category;
        //uint32_t id = iRecord->id;
        uint64_t format = iRecord->format;
        uint8_t op1 = iRecord->op1 & 0x3F;
        uint8_t op2 = iRecord->op2;
        uint64_t variant = iRecord->variant;
        uint8_t  sourceoperands = iRecord->sourceoperands;
        uint32_t tablei = 0;  // row index in numOperands matrix in format_tables.cpp
        uint16_t * tableEntry = 0;

        if (category == 3) {     // multi-format instruction
            tablei = 1;
            tableEntry = &numOperands[tablei][op1];
        }
        else if (category == 1 && op2 == 0) {  // single format instruction
            switch (format >> 4) {
            case 0x10: tablei = 4; break;
            case 0x11: tablei = 5; break;
            case 0x12: tablei = 6; break;
            case 0x13: tablei = 7; break;
            case 0x14: tablei = 8; break;
            case 0x18: tablei = 9; break;
            case 0x25: tablei = 10; break;
            case 0x26: tablei = 11; break;
            case 0x29: tablei = 12; break;
            case 0x31: tablei = 13; break;
            }
            tableEntry = &numOperands[tablei][op1];
        }
        else if (category == 1 && op2 == 1) {
            switch (format) {
            case 0x207:
                tableEntry = &numOperands2071[op1];  break;
            case 0x226:
                tableEntry = &numOperands2261[op1];  break;
            case 0x227:
                tableEntry = &numOperands2271[op1];  break;
            }
        }
        uint16_t oldn = 0;  // old table entry
        uint16_t newn = 0;  // new table entry
        if (tableEntry) {
            oldn = *tableEntry;                       // read from table in format_tables.cpp
            // override old table with new information from external table instruction_list.csv
            newn = sourceoperands & 7;                // number of source operands
            if (variant & VARIANT_On) newn |= 1 << 8; // IM5 used for option bits
            // replace old table entry bits by new ones from external table instruction_list.csv
            *tableEntry = (oldn & 0xFFF8) | newn;
            /* // write any discrepancies between tables
            if ((oldn ^ newn) & 0x107) {
                printf("\n%4X  %4X  %4i  %4X  %4X  %s", category, format, op1, oldn, newn, iRecord->name);
            }*/
        }
    }
}



/////////////////
// CThread class
/////////////////

// constructor
CThread::CThread() {
    numContr = 1 | (1<<MSK_SUBNORMAL);                          // default numContr. Bit 0 must be 1;
    enableSubnormals (numContr & (1<<MSK_SUBNORMAL));           // enable or disable subnormal numbers
    lastMask = numContr;
    ninstructions = 0;
    mapIndex1 = mapIndex2 = mapIndex3 = 0;                 // indexes into memory map
    callDept = 0;
    listLines = 0;
    tempBuffer = 0;
}

// destructor
CThread::~CThread() {
    if (tempBuffer != 0) {
        delete[] tempBuffer;                               // free temporary buffer
    }
}

// initialize registers etc. from values in emulator
void CThread::setRegisters(CEmulator * emulator) {
    this->emulator = emulator;
    this->memory = emulator->memory;                       // program memory
    memoryMap.copy(emulator->memoryMap);                   // memory map
    // ip_base = emulator->ip_base;                        // reference point for code and read-only data
    ip0 = emulator->ip0;                                   // reference point for code and read-only data
    datap = emulator->datap0 + emulator->fileHeader.e_datap_base;  // base pointer for writeable data
    threadp = emulator->threadp0 + emulator->fileHeader.e_threadp_base; // base pointer for thread-local data
    ip = entry_point = emulator->fileHeader.e_entry + ip0; // start value of instruction pointer
    MaxVectorLength = emulator->MaxVectorLength;
    tempBuffer = new int8_t[MaxVectorLength * 2];          // temporary buffer for vector operands
    memset(registers, 0, sizeof(registers));               // clear all registers
    memset(vectorLength, 0, sizeof(vectorLength));
    vectors.setDataSize(32*MaxVectorLength);
    registers[31] = emulator->stackp;                      // stack pointer
    memset(perfCounters, 0, sizeof(perfCounters));         // reset performance counters
    // initialize capability registers
    memset(capabilyReg, 0, sizeof(capabilyReg));           // reset capability registers
    capabilyReg[0] = 'E';                                  // brand ID. E = emulator
    capabilyReg[1] = FORWARDCOM_VERSION * 0x10000 + FORWARDCOM_SUBVERSION * 0x100; // ForwardCom version
    capabilyReg[8] = 0b1111;                               // support for operand sizes in g.p. registers
    capabilyReg[9] = 0b101101111;                          // support for operand sizes in vector registers
    capabilyReg[12] = MaxVectorLength;                     // maximum vector length
    capabilyReg[13] = MaxVectorLength;                     // maximum vector length for permute
    capabilyReg[14] = MaxVectorLength;                     // maximum block size for permute??
    capabilyReg[15] = MaxVectorLength;                     // maximum vector length compress_sparse and expand_sparse    
    listFileName = cmd.outputListFile;                     // name for output list file. to do: add thread number to list file name if multiple threads
}

// start running
void CThread::run() {
    listStart();                                 // start writing debug output list
    running = 1;  terminate = false;
    while (running && !terminate) {

        fetch();                                 // fetch next instruction
        if (terminate) break;
        decode();                                // decode instruction
        if (terminate) break;
        execute();                               // execute instruction
    }
    // write debug output
    if (listFileName) {
        // write number of instructions executed
        listOut.newLine();
        listOut.tabulate(emulator->disassembler.asmTab0);
        listOut.putDecimal(uint32_t(perfCounters[perf_instructions])); // Write number of instructions executed
        listOut.put(" instructions executed.");
        listOut.newLine();

        // write output buffer to file
        listOut.write(cmd.getFilename(listFileName));
    }
}

// fetch next instruction
void CThread::fetch() {
    // find memory map entry
    while (ip < memoryMap[mapIndex1].startAddress) {
        if (mapIndex1 > 0) mapIndex1--;
        else {
            interrupt(INT_ACCESS_EXE);  return;
        }
    }
    while (ip >= memoryMap[mapIndex1 + 1].startAddress) {
        if (mapIndex1 + 2 < memoryMap.numEntries()) mapIndex1++;
        else {
            interrupt(INT_ACCESS_EXE);  return;
        }
    }
    // check execute permission
    if (!(memoryMap[mapIndex1].access_addend & SHF_EXEC)) interrupt(INT_ACCESS_EXE);
    // get instruction
    pInstr = (STemplate const *)(memory + ip);
}

// List of instructionlengths, used in decode()
static const uint8_t lengthList[8] = {1,1,1,1,2,2,3,4};

// decode current instruction
void CThread::decode() {
    uint32_t operandOptions = 0;  // number of operands and options from numOperands tables in format_tables.cpp

    listInstruction(ip - ip0);            // make debug listing
    // decoding similar to CDisassembler::parseInstruction()
    op = pInstr->a.op1;

    // Get format
    uint32_t format = (pInstr->a.il << 8) + (pInstr->a.mode << 4); // Construct format = (il,mode,submode)

    // Get submode
    switch (format) {
    case 0x200: case 0x220: case 0x300: case 0x320:        // submode in mode2
        format += pInstr->a.mode2;
        break;
    case 0x250: case 0x310:                                // Submode for jump instructions etc.
        if (op < 8) {
            format += op;                                  // op1 defines sub-format
            op = pInstr->b[0] & 0x3F;                      // OPJ is in IM1 (other positions for opj fixed below
        }
        else {
            format += 8;
        }
        break;
    }
    // Look up format details (lookupFormat() is in emulator2.cpp)
    fInstr = &formatList[lookupFormat(pInstr->q)];
    format = fInstr->format2;                              // Include subformat depending on op1

    if (fInstr->imm2 & 0x80) {                             // alternative position of opj
        if (fInstr->imm2 & 0x40) {                         // no opj
            op = 63;
        }
        else if (fInstr->imm2 & 0x10) {
            op = pInstr->b[7] & 0x3F;                      // OPJ is in high part of IM6 in format A2
        }
    }
    if (fInstr->tmplate == 0xE && pInstr->a.op2 && !(fInstr->imm2 & 0x100)) {
        // Single format instruction if op2 != 0 in E template and op2 not used as immediate operand
        static SFormat form;                               // don't initialize static object.
        form = *fInstr;                                    // copy format record
        form.category = 1;                                 // change category
        fInstr = &form;                                    // point to static object
        // operand tables for single-format instructions
        if (format == 0x207 && pInstr->a.op2 == 1) operandOptions = numOperands2071[op]; // table for format 2.0.7
        else if (format == 0x226 && pInstr->a.op2 == 1) operandOptions = numOperands2261[op]; // table for format 2.2.6
        else if (format == 0x227 && pInstr->a.op2 == 1) operandOptions = numOperands2271[op]; // table for format 2.2.7
        else operandOptions = 0xB;                              // default value when there is no table
    }
    else {    
        // operand tables for multi-format instructions
        operandOptions = numOperands[fInstr->exeTable][op];     // number of source operands (see bit definitions in format_tables.cpp)
    }

    ignoreMask     = (operandOptions & 0x08) != 0;              // bit 3: ignore mask
    noVectorLength = (operandOptions & 0x10) != 0;              // bit 4: vector length determined by execution function
    doubleStep     = (operandOptions & 0x20) != 0;              // bit 5: take double steps
    dontRead       = (operandOptions & 0x40) != 0;              // bit 6: don't read source operand
    unchangedRd    = (operandOptions & 0x80) != 0;              // bit 7: RD is unchanged, not destination
    nOperands      = operandOptions  & 0x7;                     // bit 0-2: number of operands
    bool hasOptions = operandOptions & 0x100;                   // has option bits in format E for integer operands

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
    uint8_t instrLength = lengthList[pInstr->i[0] >> 29];  // Length up to 3 determined by il. Length 4 by upper bit of mode
    ip += instrLength * 4;  // next ip

    // get address of memory operand
    if (fInstr->mem) memAddress = getMemoryAddress();

    // find operands
    if (fInstr->category == 4 && fInstr->jumpSize) { 
        // jump instruction with self-relative jump address
        // check if it uses vector registers
        vect = (fInstr->vect & 0x10) && fInstr->tmplate != 0xC && (pInstr->a.ot & 4);
        // pointer to address field
        const uint8_t * pa = &pInstr->b[0] + fInstr->jumpPos;
        // store relative address in addrOperand
        switch (fInstr->jumpSize) {
        case 1:    // sign extend 8-bit offset
            addrOperand = *(int8_t*)pa;
            break;
        case 2:    // sign extend 16-bit offset
            addrOperand = *(int16_t*)pa;
            break;
        case 3:    // sign extend 24-bit offset
            addrOperand = *(int32_t*)pa << 8 >> 8;
            break;
        case 4:    // sign extend 32-bit offset
            addrOperand = *(int32_t*)pa;
            break;
        case 8:    // 64-bit offset
            addrOperand = *(int64_t*)pa;
            break;
        default:
            addrOperand = 0;
            err.submit(ERR_INTERNAL);
        }
        // pointer to immediate field
        const uint8_t * pi = &pInstr->b[0] + fInstr->immPos;
        // get immediate operand or last register operand
        if (fInstr->opAvail & 1) {
            // last operand is immediate. sign extend or convert it into parm[2]
            switch (fInstr->immSize) {
            case 1:
                parm[2].qs = parm[4].qs = *(int8_t*)pi;              // sign extend
                if (operandType == 5) parm[2].f = parm[4].bs;        // convert to float
                if (operandType == 6) parm[2].d = parm[4].bs;        // convert to double
                break;
            case 2:
                parm[2].qs = parm[4].qs = *(int16_t*)pi;             // sign extend
                if (operandType == 5) parm[2].f = half2float(*(uint16_t*)pi); // convert from half precision
                if (operandType == 6) parm[2].d = half2float(*(uint16_t*)pi); // convert from half precision
                break;
            case 4:
                parm[2].qs = parm[4].qs = *(int32_t*)pi;             // sign extend
                if (operandType == 6) parm[2].d = *(float*)pi;       // convert to double
                break;
            case 8:
                parm[2].qs = parm[4].qs = *(int64_t*)pi;  break;     // just copy
            default:
                err.submit(ERR_INTERNAL);
            }
            operands[5] = 0x20;
            // first source operand
            if (fInstr->opAvail & 0x20) operands[4] = pInstr->a.rs;
            else operands[4] = pInstr->a.rd;
        }
        else if (fInstr->opAvail & 2) {
            // last operand is memory
            parm[2].q = readMemoryOperand(memAddress);
            operands[5] = 0x40;
            // first source operand
            if (fInstr->opAvail & 0x20) operands[4] = pInstr->a.rs;
            else operands[4] = pInstr->a.rd;
        }
        else {
            // last source operand is a register
            operands[4] = pInstr->a.rd;
            if ((fInstr->opAvail & 0x30) == 0x30) {
                // three registers
                operands[4] = pInstr->a.rs;
                operands[5] = pInstr->a.rt;
            }
            else if (fInstr->opAvail & 0x20) operands[5] = pInstr->a.rs;
            else operands[5] = pInstr->a.rd;
            // read register containing last operand
            parm[2].q = readRegister(operands[5]);
        }
        operands[0] = pInstr->a.rd;                         // destination
        operands[1] = 0xFF;                                 // no mask
        // read register containing first source operand
        parm[1].q = readRegister(operands[4]);
        // return type for debug output. may be changed by execution function
        returnType = operandType | 0x1010;
        return;
    }

    // single format, multi-format, and indirect jump instructions:

    // Make list of operands from available operands.
    // The operands[] array must have 6 elements to avoid overflow here,
    // even if some elements are later overwritten and used for other purposes
    uint8_t opAvail = fInstr->opAvail;    // Bit index of available operands
    // opAvail bits: 1 = immediate, 2 = memory,
    // 0x10 = RT, 0x20 = RS, 0x40 = RU, 0x80 = RD 
    int j = 5;
    if (opAvail & 0x01) operands[j--] = 0x20;              // immediate operand
    if (opAvail & 0x02) operands[j--] = 0x40;              // memory operand
    if (opAvail & 0x10) operands[j--] = pInstr->a.rt;      // register RT
    if (opAvail & 0x20) operands[j--] = pInstr->a.rs;      // register RS
    if (opAvail & 0x40) operands[j--] = pInstr->a.ru;      // register RU
    if (opAvail & 0x80) operands[j--] = pInstr->a.rd;      // register RD
    operands[0] = pInstr->a.rd;                            // destination

    // find mask register
    if (fInstr->tmplate == 0xA || fInstr->tmplate == 0xE) {
        operands[1] = pInstr->a.mask;
        // find fallback register
        uint8_t fb = findFallback(fInstr,  pInstr, nOperands);
        operands[2] = fb;                                  // fallback register, or 0xFF if zero fallback
    }
    else {
        operands[1] = operands[2] = 0xFF;                  // no mask, no fallback
    }

    // determine if vector registers are used
    vect = (fInstr->vect & 1) || ((fInstr->vect & 0x10) && (pInstr->a.ot & 4));

    // return type for debug output. may be changed by execution function
    returnType = operandType | 0x10 | vect << 8;

    // get value of last operand if not a vector
    if (opAvail & 0x01) {
        // pointer to immediate field
        const uint8_t * pi = &pInstr->b[0] + fInstr->immPos;
        // get value, sign extended
        switch (fInstr->immSize) {
        case 1:
            parm[2].qs = *(int8_t*)pi;
            break;
        case 2:
            parm[2].qs = *(int16_t*)pi;
            break;
        case 4:
            parm[2].qs = *(int32_t*)pi;
            break;
        case 8:
            parm[2].qs = *(uint64_t*)pi;
            break;
        case 14:  // 4 bits
            parm[2].q = *(uint8_t*)pi & 0xF;
            break;
        default:
            err.submit(ERR_INTERNAL);
        }
        // extend, shift, or convert
        parm[4].q = parm[2].q;                             // preserve original value
        switch (operandType) {
        case 5:  // float
            if (fInstr->immSize == 1) { // convert integer
                parm[2].f = (float)(int8_t)parm[2].b;
            }
            else if (fInstr->immSize == 2) { // convert half precision
                parm[2].f = half2float(parm[2].i);
            }
            break;
        case 6:  // double precision
            if (fInstr->immSize == 1) { // convert integer
                parm[2].d = (double)(int8_t)parm[2].b;
            }
            else if (fInstr->immSize == 2) { // convert half precision
                parm[2].d = half2float(parm[2].i);
            }
            else if (fInstr->immSize == 4) { // convert single precision
                parm[2].d = parm[2].f;
            }
            break;
        case 7:  // quadruple precision
            // to do
            break;
        default: // all integer types. shift value if needed

            if ((fInstr->imm2 & 4) && !hasOptions) {
                parm[2].q <<= pInstr->a.im5;  // shift IM4 value left by IM5 if IM5 is not used for options
            }
            else if (fInstr->imm2 & 8) parm[2].q <<= pInstr->a.im4;
        }
        if (opAvail & 2) {
            // both memory and immediate operand
            if ((!vect || (fInstr->vect & 4)) && !dontRead) {
                // scalar or broadcast memory operand
                parm[1].q = readMemoryOperand(memAddress);
            }
            if (nOperands > 2) parm[0].q = readRegister(operands[3] & 0x1F);
            return;
        }
    }
    else if ((!vect || (fInstr->vect & 4)) && (opAvail & 0x02) && !dontRead) {
        // scalar or broadcast memory operand and no immediate operand
        parm[2].q = readMemoryOperand(memAddress);
    }
    else if (!vect) {
        // general purpose register
        parm[2].q = readRegister(operands[5] & 0x1F);
    }
    // get values of remaining operands
    if (nOperands > 1) parm[1].q = readRegister(operands[4] & 0x1F);
    if (nOperands > 2) parm[0].q = readRegister(operands[3] & 0x1F);
}


// execute current instruction
void CThread::execute() {
    uint64_t result = 0;                         // destination value
    PFunc functionPointer = 0;                   // pointer to execution function
    running = 1;

    // find function pionter
    if (fInstr->exeTable == 0) {
        interrupt(INT_UNKNOWN_INST);  return;
    }
    if (fInstr->tmplate == 0xE && pInstr->a.op2 != 0 && !(fInstr->imm2 & 0x100)) {  
        // single format instruction with E template
        uint8_t index; // index into EDispatchTable
        // bit 0-2 = mode2
        // bit   3 = mode bit 1
        // bit   4 = il bit 0
        // bit 5-6 = op2 - 1
        index = pInstr->a.mode2 | (pInstr->a.mode << 2 & 8) | (pInstr->a.il << 4 & 0x10) | (pInstr->a.op2 - 1) << 5;
        functionPointer = EDispatchTable[index];
    }
    else {  // all other instructions. fInstr->exeTable indicates which function table to look into  
        functionPointer = metaFunctionTable[fInstr->exeTable][op];
    }
    if (!functionPointer || !fInstr->exeTable) {
        interrupt(INT_UNKNOWN_INST);
        return;
    }
    if (vect) { // vector instruction
        // length of each element
        uint32_t elementSize = dataSizeTable[operandType];
        // get vector length
        // vector length of result = length of first source operand register
        switch (nOperands) {
        case 0:  // no source operands. vector length will be set by instruction
            vectorLengthR = 8;  break;
        case 1:  // one source operand
            if (operands[5] & 0x20) {  // source operand is immediate. 
                vectorLengthR = dataSizeTable[operandType]; // vector length may be modified by instruction
            }
            else if (operands[5] & 0x40) {  // source operand is memory                
                vectorLengthR = vectorLengthM;
            }
            else { // source operand is register
                vectorLengthR = vectorLength[operands[5]];
            }
            break;
        case 2:  // two source operands
            if (operands[4] & 0x40) {  // first source operand is memory                
                vectorLengthR = vectorLengthM;
            }
            else {   // first source operand is register
                vectorLengthR = vectorLength[operands[4]];
            }
            break;
        case 3: default:  // three source operands. first source operand must be register
            vectorLengthR = vectorLength[operands[3]];
            break;
        }
        if (noVectorLength                       // vector length determined by execution function
            || fInstr->category == 4) {          // call compare/jump function even if vector is empty
            vectorLengthR = elementSize;         // make sure it is called at least once
        }
        // set vector length of destination
        if (!noVectorLength && !unchangedRd) {
            vectorLength[operands[0]] = vectorLengthR;
        }

        // loop through vector
        vect = 1;
        for (vectorOffset = 0; vectorOffset < vectorLengthR; vectorOffset += elementSize) {
            if (vect & 4) break;  // stop loop

            // read nOperands operands
            int iOp = 3 - nOperands;
            if (iOp < 0) iOp = 0; // limit if nOperands > 3
            for (; iOp <= 2; iOp++) {
                if (operands[iOp+3] & 0x20) { // immediate
                    // has already been read into parm[2]
                }
                else if (operands[iOp+3] & 0x40) { // memory
                    if (fInstr->vect & 4) { // broadcast memory operand
                        if (vectorOffset + elementSize > vectorLengthM) {
                            parm[iOp].q = 0; // beyond broadcast length
                        }
                        else { // read broadcast memory operand
                            parm[iOp].q = readMemoryOperand(memAddress);
                        }
                    }
                    else {  // memory vector 
                        if (!dontRead) {
                            if (vectorOffset + elementSize > vectorLengthM) {
                                parm[iOp].q = 0; // beyond memory operand length
                            }
                            else {  // read memory vector                          
                                parm[iOp].q = readMemoryOperand(memAddress + vectorOffset);                        
                            }
                        }
                    }
                }
                else { // vector register
                    parm[iOp].q = readVectorElement(operands[iOp+3], vectorOffset);                    
                }            
            }
            
            // get mask
            if ((operands[1] & 7) != 7) {
                parm[3].q = readVectorElement(operands[1], vectorOffset);
            }
            else {
                parm[3].q = numContr;
            }
            // skip instruction if mask = 0, except for certain instructions
            if ((parm[3].q & 1) == 0 && !ignoreMask) {
                // result is masked off. find fallback
                if (operands[2] == 0xFF) result = 0;               // fallback = 0
                else result = readVectorElement(operands[2], vectorOffset); // fallback register          
                if (doubleStep) {
                    if (operands[2] == 0xFF) result = 0;
                    else result = readVectorElement(operands[2], vectorOffset + elementSize);
                }
            }
            else {
                // normal operation. execute instruction
                result = (*functionPointer)(this);
            }
            // store in destination register
            if ((running & 1) && !(returnType & 0x20)) {
                vectorLength[operands[0]] = vectorLengthR;
                // get mask for operand size (operandType may have been changed by function)
                //uint64_t opmask = dataSizeMask[operandType];
                // write result to vector
                writeVectorElement(operands[0], result, vectorOffset);
                if (dataSizeTable[operandType] >= 16) {  // 128 bits
                    writeVectorElement(operands[0], parm[5].q, vectorOffset + (elementSize>>1)); // high part of double size result
                }
                if (doubleStep) {  // double step
                    writeVectorElement(operands[0], parm[5].q, vectorOffset + elementSize); // high part of double size result
                }
            }
            vect ^= 3;                                     // toggle between 1 for even elements, 2 for odd
            if (doubleStep) vectorOffset += elementSize;   // skip next element if instruction takes two elements at a time            
        }
        listResult(result);                                // debug output
    }
    else {
        // general purpose registers
        // get mask
        if ((operands[1] & 7) != 7) {
            parm[3].q = readRegister(operands[1]);
        }
        else {
            parm[3].q = numContr;
        }
        // skip instruction if mask = 0, except for certain instructions
        if ((parm[3].q & 1) == 0 && !ignoreMask) {
            // result is masked off. find fallback
            if (operands[2] == 0xFF) result = 0;
            else result = readRegister(operands[2]);            
        }
        else {
            // normal operation. 
            // execute instruction
            result = (*functionPointer)(this);
        }
        // get mask for operand size (operandType may have been changed by function)
        // store in destination register, zero extended from operand size
        if (running & 1) registers[operands[0]] = result & dataSizeMask[operandType];
        listResult(result);                                // debug output
    }
    performanceCounters();  // update performance counters
}

// update performance counters
void CThread::performanceCounters() {
    perfCounters[perf_cpu_clock_cycles]++;       // clock cycles
    perfCounters[perf_instructions]++;       // instructions
    if ((fInstr->format2 & 0xF00) == 0x200)  perfCounters[perf_2size_instructions]++;  // double size instructions
    if ((fInstr->format2 & 0xF00) == 0x300)  perfCounters[perf_3size_instructions]++;  // triple size instructions
    if (vect) {
        perfCounters[perf_vector_instructions]++;  // vector instructions
    }
    else {
        perfCounters[perf_gp_instructions]++;  // g.p. instructions
        if ((parm[3].q & 1) == 0 && !ignoreMask) perfCounters[perf_gp_instructions_mask0]++;  // g.p. instructions masked off
    }
    if (fInstr->category == 4) {  // jump instructions
        perfCounters[perf_control_transfer_instructions]++;  // all jumps, calls, returns
        if (fInstr->tmplate == 0xD) { // direct jump/call
            perfCounters[perf_direct_jumps]++;  // g.p. instructions
        }
        else if (fInstr->exeTable == 2) {
            if (op == 62 && fInstr->format2 >> 4 == 0x16) {
                perfCounters[perf_direct_jumps]++; // simple return
            }
            else if (op >= 56) perfCounters[perf_indirect_jumps]++; // indirect jumps and calls
            else perfCounters[perf_cond_jumps]++; // conditional jumps 
        }
    }
}

// read vector element
uint64_t CThread::readVectorElement(uint32_t v, uint32_t vectorOffset) {
    uint32_t size;   // element size
    uint64_t returnval = 0;
    if (operandType == 8) size = 2;
    else size = dataSizeTableMax8[operandType];
    v &= 0x1F;  // protect against array overflow
    //if (vectorOffset < vectorLength[v]) {
    if (vectorOffset + size <= vectorLength[v]) {
        switch (size) {  // zero-extend from element size
        case 1:
            returnval = *(uint8_t*)(vectors.buf() + MaxVectorLength*v + vectorOffset);
            break;
        case 2:
            returnval = *(uint16_t*)(vectors.buf() + MaxVectorLength*v + vectorOffset);
            break;
        case 4:
            returnval = *(uint32_t*)(vectors.buf() + MaxVectorLength*v + vectorOffset);
            break;
        case 8:
            returnval = *(uint64_t*)(vectors.buf() + MaxVectorLength*v + vectorOffset);
            break;
        }
        uint32_t sizemax = vectorLength[v] - vectorOffset;            
        if (size > sizemax) {  // reading beyond end of vector. cut off element to max size
            returnval &= (uint64_t(1) << sizemax*8) - 1;
        }
    }
    return returnval;
}

// write vector element
void CThread::writeVectorElement(uint32_t v, uint64_t value, uint32_t vectorOffset) {
    uint32_t size = dataSizeTableMax8[operandType];
    v &= 0x1F;  // protect against array overflow
    if (vectorOffset + size <= vectorLength[v]) {
        switch (size) {  // zero-extend from element size
        case 1:
            *(uint8_t*)(vectors.buf() + MaxVectorLength*v + vectorOffset) = (uint8_t)value;
            break;
        case 2:
            *(uint16_t*)(vectors.buf() + MaxVectorLength*v + vectorOffset) = (uint16_t)value;
            break;
        case 4:
            *(uint32_t*)(vectors.buf() + MaxVectorLength*v + vectorOffset) = (uint32_t)value;
            break;
        case 8:
            *(uint64_t*)(vectors.buf() + MaxVectorLength*v + vectorOffset) = value;
            break;
        }
    }
}

// get address of a memory operand
uint64_t CThread::getMemoryAddress() {
    // find base register
    if ((fInstr->mem & 3) == 0) err.submit(ERR_INTERNAL);
    //uint8_t basereg = (fInstr->mem & 1) ? pInstr->a.rt : pInstr->a.rs;
    uint8_t basereg = pInstr->a.rs;
    readonly = false;
    memory_error = false;
    // base register value
    uint64_t baseval = registers[basereg];
    if (fInstr->addrSize > 1 && !(fInstr->mem & 0x20)) {
        // special registers
        switch (basereg) {
        case 28:   // threadp
            baseval = threadp;  break;
        case 29:   // datap
            baseval = datap;  break;
        case 30:   // ip
            baseval = ip;  readonly = true;
            break;
        }
    }
    // pointer to memory field
    const uint8_t * pa = &pInstr->b[0] + fInstr->addrPos;

    // find index register
    uint64_t indexval = 0;
    if ((fInstr->mem & 4) && (pInstr->a.rt != 0x1F)) {
        // rt is index register
        indexval = registers[pInstr->a.rt & 0x1F];
        // check limit
        if (fInstr->mem & 0x20) {
            const uint8_t * pi = &pInstr->b[0] + fInstr->addrPos; // pointer to immediate field
            uint64_t limit = *(uint64_t*)pi;
            limit &= (uint64_t(1) << (fInstr->addrSize * 8)) - 1;
            if (indexval > limit) {
                interrupt(INT_ARRAY_BOUNDS);
                memory_error = true;
                //return 0;
            }
        }
    }
    // get offset, sign-extended
    int64_t offset = 0;
    if (fInstr->mem & 0x10) {
        switch (fInstr->addrSize) {
        case 0:
            break;
        case 1:
            offset = *(int8_t*)pa;
            break;
        case 2:
            offset = *(int16_t*)pa;
            break;
        case 4:
            offset = *(int32_t*)pa;
            break;
        case 8:
            offset = *(int64_t*)pa;
            break;
        default:
            err.submit(ERR_INTERNAL);
        }
    }
    // scale
    switch (fInstr->scale) {
    case 1: // offset is scaled
        offset <<= dataSizeTableLog[operandType];
        break;
    case 2: // index is scaled by OS
        indexval <<= dataSizeTableLog[operandType];
        break;
    case 4: // 4 = scale factor is -1
        indexval = uint64_t(-(int64_t)indexval);
        break;
    }
    // get length
    if ((fInstr->vect & 6) && pInstr->a.rt < 0x1F) { // vector length or broadcast length is in RT        
        if (registers[pInstr->a.rt] > MaxVectorLength) vectorLengthM = MaxVectorLength;
        else vectorLengthM = (uint32_t)registers[pInstr->a.rt];
    }
    else { // scalar
        vectorLengthM = dataSizeTable[operandType & 7];
    }
    // offset and index may be negative, but the result must be positive
    return baseval + indexval + (uint64_t)offset;
}

// read a memory operand
uint64_t CThread::readMemoryOperand(uint64_t address) {
    // get most likely memory map index
    uint32_t * indexp = readonly ? &mapIndex2 : &mapIndex3;
    uint32_t index = * indexp;

    // find memory map entry
    while (address < memoryMap[index].startAddress) {
        if (index > 0) index--;
        else {
            interrupt(INT_ACCESS_READ);  return 0;
        }
    }
    while (address >= memoryMap[index + 1].startAddress) {
        if (index + 2 < memoryMap.numEntries()) index++;
        else {
            interrupt(INT_ACCESS_READ);  return 0;
        }
    }
    // check read permission
    if (!(memoryMap[index].access_addend & SHF_READ)) {
        interrupt(INT_ACCESS_READ);  return 0;
    }

    // check if map boundary crossed
    if (address + dataSizeTable[operandType] > memoryMap[index+1].startAddress
    && !(memoryMap[index+1].access_addend & SHF_READ)) {
        interrupt(INT_ACCESS_READ);
    }

    // check alignment ?

    // save index for next time
    *indexp = index;

    // get value, zero extended    
    const int8_t * p = memory + address;  // pointer to data
    switch (dataSizeTableMax8[operandType]) {
    case 0:
        break;
    case 1:
        return *(uint8_t*)p;
    case 2:
        if (address & 1) interrupt(INT_MISALIGNED_MEM);
        return *(uint16_t*)p;
    case 4:
        if (address & 3) interrupt(INT_MISALIGNED_MEM);
        return *(uint32_t*)p;
    case 8:
        if (address & 7) interrupt(INT_MISALIGNED_MEM);
        return *(uint64_t*)p;
    }
    return 0;
}

// write a memory operand
void CThread::writeMemoryOperand(uint64_t val, uint64_t address) {
    // most likely memory map index is saved in mapIndex3
    // find memory map entry
    while (address < memoryMap[mapIndex3].startAddress) {
        if (mapIndex3 > 0) mapIndex3--;
        else {
            interrupt(INT_ACCESS_WRITE);  return;
        }
    }
    while (address >= memoryMap[mapIndex3+1].startAddress) {
        if (mapIndex3 + 2 < memoryMap.numEntries()) mapIndex3++;
        else {
            interrupt(INT_ACCESS_WRITE);  return;
        }
    }
    // check write permission
    if (!(memoryMap[mapIndex3].access_addend & SHF_WRITE)) {
        interrupt(INT_ACCESS_WRITE);  return;
    }

    // check if map boundary crossed
    if (address + dataSizeTable[operandType] > memoryMap[mapIndex3+1].startAddress
    && !(memoryMap[mapIndex3+1].access_addend & SHF_WRITE)) {
        interrupt(INT_ACCESS_WRITE);
    }

    // write value
    // get value, zero extended    
    int8_t * p = memory + address;  // pointer to data
    switch (dataSizeTableMax8[operandType]) {
    case 0:
        break;
    case 1:
        *(uint8_t*)p = (uint8_t)val;
        break;
    case 2:
        if (address & 1) interrupt(INT_MISALIGNED_MEM);
        *(uint16_t*)p = (uint16_t)val;
        break;
    case 4:
        if (address & 3) interrupt(INT_MISALIGNED_MEM);
        *(uint32_t*)p = (uint32_t)val;
        break;
    case 8:
        if (address & 7) interrupt(INT_MISALIGNED_MEM);
        *(uint64_t*)p = val;
        break;
    }
}

// start writing debug list
void CThread::listStart() {
    if (!listFileName) return;                   // nothing if no list file
    listOut.put("Debug listing of ");
    listOut.put(cmd.getFilename(cmd.inputFile));
    listOut.newLine();
    // Date and time. (Will fail after year 2038 on computers that use 32-bit time_t)
    time_t time1 = time(0);
    char * timestring = ctime(&time1);
    if (timestring) {
        for (char *c = timestring; *c; c++) {            // Remove terminating '\n' in timestring
            if (*c < ' ') *c = 0;
        }        
        listOut.put(timestring);
        listOut.newLine(); listOut.newLine();
    }
}

static uint32_t listIndex = 0;                   // index into lineList
// write current instruction to debug list
void CThread::listInstruction(uint64_t address) {
    if (listFileName == 0 || cmd.maxLines == 0) return;    // stop listing
    SLineRef rec = {address, 1, 0};
    const char * text = 0;
    if (listIndex + 1 < emulator->lineList.numEntries() && emulator->lineList[listIndex+1] == rec) {
        // just the next record. no need to search
        listIndex = listIndex+1;
    }
    else {  // we may have jumped. Find address in list
        listIndex = (uint32_t)emulator->lineList.findFirst(rec);
    }
    if (listIndex < emulator->lineList.numEntries()) {
        text = emulator->disassemOut.getString(emulator->lineList[listIndex].textPos); // get line from disassembly
        listOut.put(text);
    }
    else {  // corresponding disassembly not found
        listOut.putHex((uint32_t)address, 2);
        listOut.tabulate(emulator->disassembler.asmTab0);
        listOut.put("???");
    }
    listOut.newLine();
}

// write result of current instruction to debug list
void CThread::listResult(uint64_t result) {
    if (++listLines >= cmd.maxLines) cmd.maxLines = 0;  // stop listing 
    if (listFileName == 0 || returnType == 0 || cmd.maxLines == 0) return;      // nothing if no list file or no return value
    listOut.tabulate(emulator->disassembler.asmTab0);
    if (!(returnType & 0x100)) { // general purpose register
        if (returnType & 0x20) { // memory destination
            result = readMemoryOperand(getMemoryAddress());
        }
        if (returnType & 0x30) { // register or memory
            switch (returnType & 0xF) {
            case 0:  // int8
                listOut.putHex((uint8_t)result); break;
            case 1:  // int16
                listOut.putHex((uint16_t)result); break;
            case 2: case 5:  // int32
                listOut.putHex((uint32_t)result); break;
            case 3: case 6:  // int64
                listOut.putHex(result); break;
            case 4:  // int128
                listOut.putHex(parm[5].q, 2); listOut.putHex(result, 2); break;
            default:
                listOut.put("?");
            }
        }
    }
    else if (returnType & 0x30) { // vector
        uint8_t destinationReg = operands[0] & 0x1F;
        //uint32_t vectorLengthR = vectorLength[destinationReg];
        if (!(returnType & 0x20)) vectorLengthR = vectorLength[destinationReg];
        uint8_t type = returnType & 0xF;
        operandType = type;
        uint32_t elementSize = dataSizeTable[type & 7];
        if (type == 8) elementSize = 2;          // half precision
        if (elementSize > 8) elementSize = 8;    // int128 and float128 listed as two int64
        union {                                  // union to convert types
            uint64_t q;
            double d;
            float f;
        } u;
        if (vectorLengthR == 0) listOut.put("Empty");
        //if (returnType & 0x40) vectorLengthR += elementSize;  // one extra element (save_cp instruction)
        for (uint32_t vectorOffset = 0; vectorOffset < vectorLengthR; vectorOffset += elementSize) {
            if (returnType & 0x20) { // memory destination
                result = readMemoryOperand(getMemoryAddress() + vectorOffset);
            }
            else {            
                result = readVectorElement(destinationReg, vectorOffset);
            }
            switch (returnType & 0xF) {
            case 0:  // int8
                listOut.putHex((uint8_t)result); break;
            case 1:  // int16
                listOut.putHex((uint16_t)result); break;
            case 2:  // int32
                listOut.putHex((uint32_t)result); break;
            case 3: case 4: case 7: // int64
                listOut.putHex(result); break;
            case 5:  // float
                u.q = result;
                listOut.putFloat(u.f); break;
            case 6:  // double
                u.q = result;
                listOut.putFloat(u.d); break;
            case 8:  // float16
                listOut.putFloat16((uint16_t)result); break;
            default:
                listOut.put("???");
            }
            listOut.put(' ');
        }
    }
    if (returnType & 0x3000) {
        // conditional jump instruction
        if (returnType & 0x30) listOut.put(",  ");    // space after value
        listOut.put((returnType & 0x2000) ? "jump" : "no jump"); // tell if jump or not
    }
    listOut.newLine();
}

// make a quiet NaN with exception code and address in payload
uint64_t CThread::makeNan(uint32_t code, uint32_t operandTyp) {
    uint64_t retval = 0;
    uint8_t  instrLength = lengthList[pInstr->a.il];       // instruction length
    uint64_t iaddress = ((ip - ip0) >> 2) - instrLength;   // instruction address
    uint32_t exception_code = code & 0x1FF;                // exception code

    switch (operandTyp) {
    case 1:  // half precision
        retval = 0x7E00 | exception_code;
        break;
    case 5:  // single precision
        retval = 0x7FC00000 | exception_code << 13 | (iaddress & 0x1FFF);
        break;
    case 6:  // double precision
        retval = 0x7FF8000000000000              // exponent and quiet bit
            | uint64_t(exception_code) << 42     // error code
            | (iaddress & 0x1FFF) << 29          // address low
            | (iaddress >> 13 & 0x7FFFF);        // address high
        break;
    }
    return retval;
}

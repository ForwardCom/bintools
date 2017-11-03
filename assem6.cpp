/****************************    assem6.cpp    ********************************
* Author:        Agner Fog
* Date created:  2017-08-07
* Last modified: 2017-11-03
* Version:       1.00
* Project:       Binary tools for ForwardCom instruction set
* Module:        assem.cpp
* Description:
* Module for assembling ForwardCom .as files. 
* This module contains:
* - pass4(): Resolve internal cross references, optimize forward references
* - pass5(): Make binary file
* Copyright 2017 GNU General Public License http://www.gnu.org/licenses
******************************************************************************/
#include "stdafx.h"


// Resolve symbol addresses and internal cross references, optimize forward references
void CAssembler::pass4() {
    uint32_t addr = 0;                 // address relative to current section begin
    //uint32_t instructId;               // instruction id
    uint32_t i;                        // loop counter
    uint32_t symi;                     // symbol index
    uint32_t numUncertain;             // number of instructions with unresolved size in current section
    uint32_t totUncertain;             // number of instructions with unresolved size in all sections
    uint32_t changes = 1;              // number of size changes during each optimization pass
    uint32_t optiPass = 0;             // count optimization passes
    uint32_t nSections = sectionHeaders.numEntries(); // number of sections
    uint32_t const maxOptiPass = 10;   // maximum number of optimization passes

    // multiple optimization passes until size is certain or no changes
    for (optiPass = 1; optiPass <= maxOptiPass; optiPass++) {
        if (changes == 0 && (totUncertain == 0 || optiPass > 2)) break;
        changes = 0;                   // count instructions with changed size
        section = 0;
        numUncertain = totUncertain = 0;
        for (i = 1; i < nSections; i++) {
            sectionHeaders[i].sh_link = 0;  // reset count of uncertain instruction sizes
            sectionHeaders[i].sh_size = 0;
        }
        // loop through code objects
        for (i = 0; i < codeBuffer.numEntries(); i++) {
            //instructId = codeBuffer[i].instr1;
            if (codeBuffer[i].section == 0 || codeBuffer[i].section >= nSections)
                continue;
            if (codeBuffer[i].section != section) {
                if (section) {
                    // save results of previous section
                    sectionHeaders[section].sh_size = addr;
                    sectionHeaders[section].sh_link = numUncertain;  // sh_link is temporarily used for indicating number of instructions with uncertain size                
                    totUncertain += numUncertain;
                }
                // restore status for current section
                section = codeBuffer[i].section;
                addr = (uint32_t)sectionHeaders[section].sh_size;
                numUncertain = sectionHeaders[section].sh_link;
            }
            codeBuffer[i].address = addr;
            if (codeBuffer[i].label) {
                // there is a label here. put the address into the symbol record
                symi = findSymbol(codeBuffer[i].label);
                if (symi > 0 && symi < symbols.numEntries()) {
                    // the upper half of st_value is temporarily used for indicating if address is not yet precise
                    symbols[symi].st_value = addr | (uint64_t)numUncertain << 32;
                    symbols[symi].st_unitsize = 1;     // set an arbitrary size to indicate that a value has been assigned
                }
            }
            if (codeBuffer[i].sizeUnknown) {
                // update the size of this instruction
                uint8_t lastSize = codeBuffer[i].size;
                if (codeBuffer[i].instr1) {  // update normal instruction
                    if (optiPass >= maxOptiPass - 1) {
                        // rare case. optimization has slow convergence. choose larger instruction size if uncertain
                        codeBuffer[i].fitAddr |= IFIT_LARGE;
                    }
                    sectionHeaders[section].sh_link = numUncertain;
                    fitConstant(codeBuffer[i]);                     // recalculate necessary size of immediate constant
                    fitAddress(codeBuffer[i]);                      // recalculate necessary size of address
                    fitCode(codeBuffer[i]);                         // fit instruction to new size
                    if (codeBuffer[i].size != lastSize) changes++;  // count changes if size changed
                }
                else {  // not an instruction
                    if (codeBuffer[i].instruction == II_ALIGN) {
                        // align directive. round up address to nearest multiple of alignment value
                        uint32_t newAddress = (addr + codeBuffer[i].value.w - 1) & uint32_t(-codeBuffer[i].value.i);
                        codeBuffer[i].size = (newAddress - addr) >> 2;    // size of alignment fillers
                        if (codeBuffer[i].size != lastSize) changes++;    // count changes if size changed
                        if (numUncertain) numUncertain += (codeBuffer[i].value.w >> 2) - 1 - codeBuffer[i].size; // maximum additional size if size of previous instructions change
                        if (section && sectionHeaders[section].sh_addralign < codeBuffer[i].value.w) {
                            sectionHeaders[section].sh_addralign = codeBuffer[i].value.w; // adjust alignment of this section
                        }
                    }
                }
            }
            if (codeBuffer[i].category == 2) {
                // tiny instruction. can it be paired?
                if (i && codeBuffer[i-1].category == 2 && codeBuffer[i-1].section == codeBuffer[i].section 
                && codeBuffer[i-1].size == 1 && codeBuffer[i].label == 0) {
                    codeBuffer[i].size = 0;  // paired
                }
                else {
                    codeBuffer[i].size = 1;  // unpaired
                }
            }

            addr += codeBuffer[i].size * 4;  // update address
            numUncertain += codeBuffer[i].sizeUnknown & 0x7F;  // update uncertainty
        }
        // update last section
        if (section) {
            // save results of previous section
            sectionHeaders[section].sh_size = addr;
            sectionHeaders[section].sh_link = numUncertain;
            totUncertain += numUncertain;
        }
    } 
    // remove temporary uncertainty information from symbol records
    for (symi = 1; symi < symbols.numEntries(); symi++) {
        if (symbols[symi].st_type == STT_OBJECT || symbols[symi].st_type == STT_FUNC) {        
            symbols[symi].st_value &= 0xFFFFFFFFU;
        }
    }

    // make public symbol definitions
    for (linei = 1; linei < lines.numEntries(); linei++) {
        if (lines[linei].type == LINE_PUBLICDEF) {
            interpretPublicDirective();
        }
    }
}


// interpret public name: options {, name: options}
void CAssembler::interpretPublicDirective() {
    int state = 0;           // 0: start
                             // 1: after 'public' or ','
                             // 2: after name
                             // 3: after ':'
                             // 4: after attribute

    uint32_t symi = 0;       // symbol index
    uint32_t symn;           // symbol name index
    uint32_t tok;            // token index
    uint32_t symtok = 0;     // symbol token
    SToken token;            // current token

    tokenB = lines[linei].firstToken;      // first token in line        
    tokenN = lines[linei].numTokens;       // number of tokens in line 
    // loop through tokens on this line
    for (tok = tokenB; tok < tokenB + tokenN; tok++) {
        token = tokens[tok];
        switch (state) {
        case 0:  // start
            if (token.id == DIR_PUBLIC) state = 1; else return;
            break;
        case 1:  // expect symbol name
            if (token.type == TOK_SYM) {
                symtok = tok;
                symn = token.id;
                symi = findSymbol(symn);
                if ((int32_t)symi < 1) {
                    errors.report(token.pos, token.stringLength, ERR_SYMBOL_UNDEFINED);  return;
                }
                state = 2;
            }
            else if (token.type == TOK_NAM) {
                // name found. find symbol
                symi = findSymbol((char*)buf() + tokens[tok].pos, tokens[tok].stringLength);
                if ((int32_t)symi < 1) {
                    errors.report(token.pos, token.stringLength, ERR_SYMBOL_UNDEFINED);  return;
                }
                symtok = tok;
                symn = symbols[symi].st_name;
                state = 2;
            }
            else errors.report(token);
            break;
        case 2:  // after name. expect ':' or ','
            if (token.type == TOK_OPR && token.id == ':') state = 3;
            else if (token.type == TOK_OPR && token.id == ',') {
            EXPORT_SYMBOL:
                // check if external
                if (symbols[symi].st_shndx == 0) {
                    errors.report(tokens[symtok].pos, tokens[symtok].stringLength, ERR_CANNOT_EXPORT);
                    state = 1;
                    continue;                    
                }
                // check symbol type
                switch (symbols[symi].st_type) {
                case STT_NOTYPE:  // type missing. set type
                    symbols[symi].st_type = (symbols[symi].st_other & STV_EXEC) ? STT_FUNC : STT_OBJECT;
                    break;
                case STT_OBJECT:
                case STT_FUNC:
                case STT_CONSTANT:
                    break;  // ok
                case STT_VARIABLE:  // meta-variable has been assigned multiple values
                    errors.report(tokens[symtok].pos, tokens[symtok].stringLength, ERR_SYMBOL_REDEFINED);
                    state = 1;
                    continue; 
                case STT_EXPRESSION:  // cannot export expression
                    errors.report(tokens[symtok].pos, tokens[symtok].stringLength, ERR_EXPORT_EXPRESSION);
                    state = 1;
                    continue;
                default:
                    errors.report(tokens[symtok].pos, tokens[symtok].stringLength, ERR_CANNOT_EXPORT);
                    state = 1;
                    continue;                    
                }
                // make symbol global or weak
                if (symbols[symi].st_bind != STB_WEAK) symbols[symi].st_bind = STB_GLOBAL;
                state = 1;
            }
            else {
                errors.report(token);  return;
            }
            break;
        case 3:  // after ':'. expect attribute
            SET_ATTRIBUTE:
            if (token.id == ATT_WEAK) {
                symbols[symi].st_bind = STB_WEAK;
            }
            else if (token.id == ATT_CONSTANT && symbols[symi].st_type != STT_OBJECT && symbols[symi].st_type != STT_FUNC) {
                symbols[symi].st_type = STT_CONSTANT;
            }            
            else if (token.id == DIR_FUNCTION) {
                symbols[symi].st_type = STT_FUNC;
            }
            else if (token.id == REG_IP) {
                symbols[symi].st_other = (symbols[symi].st_other & ~ (SHF_DATAP | SHF_THREADP)) | STV_IP;
            }
            else if (token.id == REG_DATAP) {
                symbols[symi].st_other = (symbols[symi].st_other & ~ (STV_IP | SHF_THREADP)) | SHF_DATAP;
            }
            else if (token.id == REG_THREADP) {
                symbols[symi].st_other = (symbols[symi].st_other & ~ (STV_IP | SHF_DATAP)) | SHF_THREADP;
            }
            else errors.report(token);
            state = 4;
            break;
        case 4:  // after attribute. expect ',' or more attributes
            if (token.type == TOK_OPR && token.id == ',') goto EXPORT_SYMBOL;
            if (token.type == TOK_ATT || token.type == TOK_DIR || token.type == TOK_REG) goto SET_ATTRIBUTE;
            errors.report(token);
            return;
        }
    }
    if (state > 1) goto EXPORT_SYMBOL;  // unfinished symbol
}


// Make binary file
void CAssembler::pass5() {
    uint32_t i;                // loop counter

    // make a databuffer for each section
    uint32_t nSections = sectionHeaders.numEntries();
    dataBuffers.setSize(nSections);
    section = 0;

    // make binary code from code records
    makeBinaryCode();

    // make binary data for data sections
    makeBinaryData();

    // make sections
    for (i = 1; i < nSections; i++) {
        if (dataBuffers[i].dataSize() > sectionHeaders[i].sh_size) {  // dataSize() is zero for uninitialized data sections
            sectionHeaders[i].sh_size = dataBuffers[i].dataSize();    // this should never be necessary
        }
        sectionHeaders[i].sh_link = 0;  // remove temporary information used during optimization passes
        outFile.addSection(sectionHeaders[i], symbolNameBuffer, dataBuffers[i]);
    }

    // copy symbols
    for (i = 0; i < symbols.numEntries(); i++) {
        if (symbols[i].st_type != STT_SECTION && symbols[i].st_type < STT_VARIABLE) {
            uint32_t newSymi;                                  // symbol index in outFile
            newSymi = outFile.addSymbol(symbols[i], symbolNameBuffer);
            // save new symbol index for use in relocation records
            symbols[i].st_unitnum = newSymi;
        }
    }

    // copy relocations
    makeBinaryRelocations();

    // make output list file
    if (cmd.outputListFile) makeListFile();

    if (cmd.debugOptions == 0) {
        // remove local symbols if not debug output and no relocation reference to them, 
        // and adjust relocation records with new symbol indexes, after making list file
        outFile.removePrivateSymbols();
    }

    // write assembly output file
    outFile.join(ET_REL);  // make ELF file from sections, etc.
}

// make binary data for code sections
void CAssembler::makeBinaryCode() {
    uint32_t i;                // loop counter
    STemplate instr;           // instruction template
    uint32_t format;           // format 
    uint32_t templ;            // format template
    uint32_t instructId;       // instruction as index into instructionlistId
    SFormat const * formatp = 0; // record in formatList
    //ElfFWC_Rela relocation = {0,0,0,0,0}; // relocation record
    uint32_t nSections = sectionHeaders.numEntries();

    // loop through code objects
    for (i = 0; i < codeBuffer.numEntries(); i++) {
        instructId = codeBuffer[i].instr1;
        if (instructId == 0) {
            // not an instruction. possibly label or directive
            if (codeBuffer[i].instruction == II_ALIGN && section) {
                // alignment directive. size has been calculated in pass 4
                int32_t asize = codeBuffer[i].size;
                instr.q = 0;  // nop instruction
                if (asize & 1) {
                    dataBuffers[section].push(&instr, 4);  // single size nop
                    asize -= 1;
                }
                instr.a.il = 2;  // double size nop
                while (asize >= 2) {
                    dataBuffers[section].push(&instr, 8);  // add double size nop
                    asize -= 2;
                }
            }
            continue;  // skip the rest
        }
        section = codeBuffer[i].section;
        if (section == 0 || section >= nSections) continue;

        instr.q = 0;           // reset template
        formatp = codeBuffer[i].formatp;
        templ = formatp->tmpl;
        format = formatp->format2;

        // assign registers
        uint8_t opAvail = formatp->opAvail;  // registers available in this format

        int nOp = instructionlistId[instructId].sourceoperands;

        if (templ == 0xA || templ == 0xE) nOp++;  // make one more register for fallback, even if it is unused

        uint8_t operands[4] = {0,0,0,0};
        int a = 0;                              // bit index to opAvail
        int j = 3;                              // Index into operands
                                                // Loop through the bits in opAvail in reverse order to pick operands according to priority
        while (j >= 0 && a < 8) {     
            if (opAvail & (1 << a)) {
                //opAvail &= ~(1 << a);
                operands[j--] = 1 << a;
            }
            a++;
        }

        // List register operands
        uint8_t  registers[4] = {0,0,0,0};
        a = 3;
        if (codeBuffer[i].etype & XPR_REG3) registers[a--] = codeBuffer[i].reg3;
        if (codeBuffer[i].etype & XPR_REG2) registers[a--] = codeBuffer[i].reg2;
        if (codeBuffer[i].etype & XPR_REG1) registers[a--] = codeBuffer[i].reg1;
        if (codeBuffer[i].etype & (XPR_MASK | XPR_FALLBACK)) registers[a--] = codeBuffer[i].fallback;
        // Make any remaining registers equal to first source for possible fallback 
        // or to avoid false dependence on unused register in superscalar processor
        while (a >= 0) registers[a--] = codeBuffer[i].reg1;

        if (codeBuffer[i].category != 2) {  // all instructions except tiny
            // Loop through operands to assign registers
            for (j = 3, a = 3; j >= 0; j--) {
                // put next operand in the sequence reg3, reg2, reg1, fallback into rt, rs, ru, or rd
                // these may be overwritten below in template B, C, and D.
                switch (operands[j]) {
                case 0x10:  // rt
                    instr.a.rt = registers[a--] & 0x1F;
                    break;
                case 0x20:  // rs
                    instr.a.rs = registers[a--] & 0x1F;
                    break;
                case 0x40:  // ru
                    instr.a.ru = registers[a--] & 0x1F;
                    break;
                case 0x80:  // rd
                    instr.a.rd = registers[a--] & 0x1F;
                    break;
                default:;  // memory and immediate operands or nothing
                }
            }

            // insert other fields
            instr.a.il = (format >> 8) & 3;  // il = instruction length
            instr.a.mode = (format >> 4) & 7;  // mode
            instr.a.op1 = instructionlistId[instructId].op1;  // operation
            if (templ != 0xD) {
                if (codeBuffer[i].dest != 2 && codeBuffer[i].dest != 0) instr.a.rd = codeBuffer[i].dest & 0x1F;  // destination register        
                if (templ != 0xC) {
                    instr.a.ot = codeBuffer[i].dtype & 7;  // operand type
                    if (format & 0x80) instr.a.ot |= 4;    // M bit
                    if (templ != 0xB) {
                        if (codeBuffer[i].etype & XPR_MASK) {
                            instr.a.mask = codeBuffer[i].mask;  // mask register
                        }
                        else {
                            instr.a.mask = 7;        // no mask
                        }
                    }
                }
            }

            uint8_t * instr_b = instr.b;  // avoid pedantic warnings from Gnu compiler
            // memory operand
            if (formatp->mem) {
                if (formatp->mem & 1) instr.a.rt = codeBuffer[i].base & 0x1F;      // base in rt
                else if (formatp->mem & 2) instr.a.rs = codeBuffer[i].base & 0x1F; // base in rs
                if (formatp->mem & 4) instr.a.rs = codeBuffer[i].index & 0x1F;     // index in rs
                uint8_t oldBase = codeBuffer[i].base;                  // save base pointer

                // calculate offset, possibly involving symbols. make relocation if necessary
                int64_t offset = calculateMemoryOffset(codeBuffer[i]);

                if (codeBuffer[i].base != oldBase) {
                    // base pointer changed by calculateMemoryOffset
                    switch (codeBuffer[i].formatp->mem & 3) {
                    case 1:  // base in RT
                        instr.a.rt = codeBuffer[i].base;  break;
                    case 2:  // base in RS
                        instr.a.rs = codeBuffer[i].base;  break;
                    }
                }

                uint32_t addrPos = formatp->addrPos;  // position of offset field
                switch (formatp->addrSize) { // size of offset
                case 0:    // no offset
                    break;
                case 1:    // 8 bits offset
                    instr.b[addrPos] = uint8_t(offset);
                    break;
                case 2:    // 16 bits offset
                    *(int16_t *)(instr_b + addrPos) = int16_t(offset);
                    break;
                case 3:   // 24 bits offset
                    *(int16_t *)(instr_b + addrPos) = int16_t(offset);          // first 16 of 24 bits
                    *(int8_t *)(instr_b + addrPos + 2) = int8_t(offset >> 16);  // last 8 bits
                    break;
                case 4:    // 32 bits offset
                    *(int32_t *)(instr_b + addrPos) = int32_t(offset);
                    break;
                case 8:    // 64 bits offset
                    *(int64_t *)(instr_b + addrPos) = offset;
                }
                // memory length or broadcast
                if (formatp->vect & 6) instr.a.rs = codeBuffer[i].length;
            }

            // immediate operand
            if (formatp->immSize) {
                int64_t value = codeBuffer[i].value.i;  //value of operand
                if (codeBuffer[i].sym1 && !(codeBuffer[i].etype & XPR_JUMPOS)) { // assume that symbol applies to jump address, not immediate constant, if instruction has both                
                    // calculation of symbol address. add relocation if needed
                    value = calculateConstantOperand(codeBuffer[i], codeBuffer[i].address + codeBuffer[i].formatp->immPos, codeBuffer[i].formatp->immSize);
                    if (codeBuffer[i].etype & XPR_ERROR) {
                        linei = codeBuffer[i].line;
                        errors.reportLine(codeBuffer[i].value.w); // report error
                    }
                }

                uint32_t immPos = formatp->immPos;  // position of immediate field
                switch (formatp->immSize) { // size of immediate field
                case 1:    // 8 bits immediate
                    if ((codeBuffer[i].etype & XPR_IMMEDIATE) == XPR_FLT) {
                        *(int8_t *)(instr_b + immPos) = (int8_t)(int)(codeBuffer[i].value.d);  // convert double to float16
                    }
                    else {
                        instr.b[immPos] = uint8_t(value);
                    }
                    break;
                case 2:    // 16 bits immediate
                    if ((codeBuffer[i].etype & XPR_IMMEDIATE) == XPR_FLT) {
                        *(int16_t *)(instr_b + immPos) = double2half(codeBuffer[i].value.d);  // convert double to float16
                    }
                    else {
                        *(int16_t *)(instr_b + immPos) = int16_t(value);
                    }
                    break;
                case 4:    // 32 bits immediate
                    if ((codeBuffer[i].etype & XPR_IMMEDIATE) == XPR_FLT) {   // convert double to float
                        *(float *)(instr_b + immPos) = float(codeBuffer[i].value.d);
                    }
                    else {
                        *(int32_t *)(instr_b + immPos) = int32_t(value);
                        if (formatp->imm2 & 8) instr.a.im2 = uint16_t((uint64_t)value >> 32);
                    }
                    break;
                case 8:    // 64 bits immediate
                    *(int64_t *)(instr_b + immPos) = value;
                }
            }

            if (formatp->imm2 & 0x80) {
                // opj is in IM1
                instr.b[0] = instructionlistId[instructId].op1;
                instr.a.op1 = format & 7;
            }

            // additional fields for format E
            if (templ == 0xE) {
                instr.a.im3 = codeBuffer[i].optionbits;
                instr.a.mode2 = format & 7;
                instr.a.op2 = instructionlistId[instructId].op2;
                // variant M1 has immediate operand in IM3
                uint64_t variant = interpretTemplateVariants(instructionlistId[instructId].template_variant);  // instruction-specific variants
                if ((variant & VARIANT_M1) && formatp->mem) instr.a.im3 = codeBuffer[i].value.w & 0x3F;
            }

            // save code
            uint32_t ilen = instr.a.il;
            if (ilen == 0) ilen = 1;
            dataBuffers[section].push(&instr, ilen * 4);
        }
        else {
            // tiny instruction
            STinyTemplate instrTiny = {0};                           // template for tiny instruction
            instrTiny.t.op1 = instructionlistId[instructId].op1;     // opcode
            instrTiny.t.rd = codeBuffer[i].dest & 0x1F;              // destination
            uint8_t rs = codeBuffer[i].reg1;                         // source register = reg1 or reg2
            if (codeBuffer[i].etype & XPR_REG2) rs = codeBuffer[i].reg2;
            instrTiny.t.rs = rs & 0xF;
            if (formatp->mem) {
                instrTiny.t.rs = codeBuffer[i].base & 0xF;           // memory pointer in rs
                if (rs == 0) rs = codeBuffer[i].dest;                // source or destination in rd
                instrTiny.t.rd = rs & 0x1F; 
            }
            if (formatp->immSize) {
                instrTiny.t.rs = codeBuffer[i].value.w & 0xF; // immediate constant in rs
                if ((codeBuffer[i].etype & XPR_IMMEDIATE) == XPR_FLT) instrTiny.t.rs = (int)codeBuffer[i].value.d & 0xF;
            }
            if (instructionlistId[instructId].format == 11) {        // swap source and destination
                instrTiny.t.rs = codeBuffer[i].dest & 0xF;
                instrTiny.t.rd = rs & 0x1F;
            }
            // check if there is a preceding unpaired tiny instruction
            uint32_t n = dataBuffers[section].dataSize();
            //if (codeBuffer[i].label == 0 && i && codeBuffer[i-1].category == 2
            //&& codeBuffer[i-1].section == codeBuffer[i].section && (dataBuffers[sectioni].get<uint32_t>(n-4) & 0x0FFFC000) == 0) {
            if (codeBuffer[i].size == 0 && n) {
                // second tiny instruction in a pair. insert into preceding unpaired tiny instruction
                dataBuffers[section].get<uint32_t>(n-4) |= (instrTiny.i & 0x3FFF) << 14;
            }
            else { // make new unpaired tiny instruction
                instr.t.ilmd = 7;
                instr.t.tiny1 = instrTiny.i & 0x3FFF;
                dataBuffers[section].push(&instr, 4);
            }
        }
    }
}

// make binary data for data sections
void CAssembler::makeBinaryData() {
    // similar to pass2, but data lines only
    section = 0;

    // lines loop
    for (linei = 1; linei < lines.numEntries(); linei++) {
        tokenB = lines[linei].firstToken;      // first token in line        
        tokenN = lines[linei].numTokens; // number of tokens in line 
        if (lines[linei].type == LINE_SECTION && tokens[tokenB+1].type == TOK_DIR) {
            switch (tokens[tokenB+1].id) {
            case DIR_SECTION:   // section starts here
                interpretSectionDirective();
                break;
            case DIR_END:    // section or function end
                interpretEndDirective();
                break;
            default:
                errors.report(tokens[tokenB + 1]);
            }
        }
        else if (lines[linei].type == LINE_DATADEF) {
            lineError = 0;
            tokenB = lines[linei].firstToken;      // first token in line        
            tokenN = lines[linei].numTokens; // number of tokens in line
            if (tokens[tokenB].type == TOK_DIR) continue;  // ignore directives here
            if (tokenN > 1) {               // lines with a single token cannot legally define a symbol name
                if (tokens[tokenB].type == TOK_TYP && tokens[tokenB + 1].type == TOK_SYM) {
                    interpretVariableDefinition2();
                }
                else if (tokens[tokenB].type == TOK_ATT && tokens[tokenB].id == ATT_ALIGN) {  
                    interpretAlign();
                }
                else {
                    interpretVariableDefinition1();
                }
            }
        }
    }
}


// put relocation records in output file
void CAssembler::makeBinaryRelocations() {
    uint32_t i;                                  // loop counter
    // copy relocation records
    for (i = 0; i < relocations.numEntries(); i++) {
        // translate symbol indexes in relocation records
        int32_t symi1 = 0, symi2 = 0;                                  // symbol index
        if (relocations[i].r_sym) {
            symi1 = findSymbol(relocations[i].r_sym);
            if (symi1 > 0) {
                relocations[i].r_sym = symbols[symi1].st_unitnum;  // replace by symbol index in outFile
            }
            else relocations[i].r_sym = 0;  // should not occur
        }
        if (relocations[i].r_refsym) {                 // reference symbol
            symi2 = findSymbol(relocations[i].r_refsym);
            if (symi2 > 0) {
                relocations[i].r_refsym = symbols[symi2].st_unitnum;  // replace by symbol index in outFile
            }
            else relocations[i].r_refsym = 0;  // should not occur
        }
        outFile.addRelocation(relocations[i]);  // put relocation in outFile
    }
}

// make output listing
void CAssembler::makeListFile() {
    // Use the disassembler for making output listing
    CDisassembler disassembler;       // make an instance of CDisassembler
    // give all my tables to the disassembler
    disassembler.getComponents2(outFile, instructionlist);
    // do the disassembly
    disassembler.go();
}

// calculate memory address possibly involving symbol. generate relocation if necessary
int64_t CAssembler::calculateMemoryOffset(SCode & code) {
    int64_t value = 0;
    int32_t symi1 = 0, symi2 = 0;
    if (code.sym1) symi1 = findSymbol(code.sym1); // target symbol, if any
    if (code.sym2) symi2 = findSymbol(code.sym2); // reference symbol, if any
    ElfFWC_Rela2 relocation;                      // relocation, if needed
    bool needsRelocation = false;                 // relocation needed

    uint8_t fieldPos = code.formatp -> addrPos;          // position of address or immediate field
    uint8_t fieldSize = code.formatp -> addrSize;         // size of address or immediate field

    uint32_t scale = 0;                           // log2 scale factor to address, not including explicit symbol scale
    if (code.etype & XPR_JUMPOS) scale = 2;       // jumps always scaled by 1 << 2 = 4
    else if (fieldSize == 1) {
        // scale factor determined by type
        uint32_t type = code.dtype;
        scale = type & 0xF;
        if (type & 0x40) scale -= 3;
    }

    // check target symbol
    if (symi1) {
        if (symi2) {
            // difference between two symbols
            if (code.symscale == 0) code.symscale = 1;
            if (symbols[symi1].st_shndx == symbols[symi2].st_shndx && symbols[symi1].st_bind == STB_LOCAL && symbols[symi2].st_bind == STB_LOCAL) {
                // both symbols are local in same section. final value can be calculated
                value = (int64_t)(symbols[symi1].st_value - symbols[symi2].st_value) / code.symscale;
                value = (value + code.offset) >> scale;
            }
            else {
                // symbols are in different section or external. relocation needed
                relocation.r_type = R_FORW_REFP;          // relative to arbitrary reference point
                relocation.r_type |= bitScanReverse(code.symscale) + scale;  // scale factor
                relocation.r_sym = code.sym1;              // Symbol index
                relocation.r_refsym = code.sym2;           // Reference symbol
                relocation.r_addend = uint32_t(code.offset);      // Addend
                needsRelocation = true;
            }
        }
        else {
            // a single symbol
            // is symbol relative to IP, DATAP, THREADP or constant?
            //uint8_t basepointer = 0;
            uint32_t symsection = symbols[symi1].st_shndx;
            if (symbols[symi1].st_type == STT_CONSTANT) {
                // constant
                relocation.r_type = R_FORW_ABS | scale;
                relocation.r_sym = code.sym1;              // Symbol index
                relocation.r_refsym = 0;           // Reference symbol
                relocation.r_addend = uint32_t(code.offset);      // Addend
                needsRelocation = true;
            }
            else if (symsection > 0 && symsection < sectionHeaders.numEntries()) {  
                // local symbol relative to IP or DATAP
                if (sectionHeaders[symsection].sh_flags & (SHF_IP | SHF_EXEC)) {
                    if (symsection == section) {
                        // symbol in same section relative to IP. calculate address
                        code.base = 30;
                        value = (int64_t)(symbols[symi1].st_value - (code.address + code.size * 4));
                        value = (value + code.offset) >> scale;    // scale jump offset by 4                          
                        // address size must be at least 2
                    }
                    else {
                        // local symbol in different IP section. needs relocation
                        code.base = 30;
                        relocation.r_type = R_FORW_SELFREL;
                        relocation.r_addend = fieldPos - code.size * 4;  // position of relocated field relative to instruction end
                        relocation.r_sym = code.sym1;          // temporary symbol index. resolve when symbol table created
                        relocation.r_refsym = 0;
                        relocation.r_addend += (int32_t)code.offset;
                        needsRelocation = true;
                    }
                }
                else {
                    // relative to DATAP or TRHEADP. needs relocation
                    if (sectionHeaders[symsection].sh_flags & SHF_THREADP) {
                        code.base = (uint8_t)REG_THREADP;
                        relocation.r_type = R_FORW_THREADP;   // relocation relative to THREADP
                    }
                    else {
                        code.base = (uint8_t)REG_DATAP;                    
                        relocation.r_type = R_FORW_DATAP;     // relocation relative to DATAP
                    }
                    relocation.r_type |= scale;         // scale factor only if 8-bit offset allowed
                    relocation.r_sym = code.sym1;       // temporary symbol index. resolve when symbol table created
                    relocation.r_refsym = 0;
                    relocation.r_addend = uint32_t(code.offset);
                    needsRelocation = true;
                }
            }
            else {  
                // remote symbol relative to IP or DATAP
                if (symbols[symi1].st_other & (STV_IP | STV_EXEC)) {
                    // relative to IP
                    code.base = (uint8_t)REG_IP;
                    relocation.r_type = R_FORW_SELFREL;
                    relocation.r_addend = fieldPos - code.size * 4;  // position of relocated field relative to instruction end
                }
                else if (symbols[symi1].st_other & STV_THREADP) {
                    // relative to THREADP
                    code.base = (uint8_t)REG_THREADP;
                    relocation.r_type = R_FORW_THREADP;
                    relocation.r_addend = 0;
                }
                else {
                    // relative to DATAP
                    code.base = (uint8_t)REG_DATAP;
                    relocation.r_type = R_FORW_DATAP;
                    relocation.r_addend = 0;
                }
                relocation.r_sym = code.sym1;          // temporary symbol index. resolve when symbol table created
                relocation.r_refsym = 0;
                relocation.r_addend += (int32_t)code.offset;
                needsRelocation = true;
            }
        }
    }
    else {
        // no symbol
        value = code.offset >> scale;
    }

    if (needsRelocation) {
        // relocation needed. insert source address
        relocation.r_type |= fieldSize << 8;      // relocation size
        relocation.r_offset = (uint64_t)code.address + fieldPos;
        relocation.r_section = code.section;
        value = 0;   // value included in relocation addend
        relocations.push(relocation);  // save relocation
    }
    return value;
}

// calculate constant or immediate operand possibly involving symbol. generate relocation if necessary
int64_t CAssembler::calculateConstantOperand(SExpression & expr, uint64_t address, uint32_t fieldSize) {
    int64_t value = 0;
    int32_t symi1 = 0, symi2 = 0;
    if (expr.sym1) {
        symi1 = findSymbol(expr.sym1); // target symbol, if any
        if (symi1 < 1) {errors.reportLine(ERR_SYMBOL_UNDEFINED);  return 0;}
    }
    if (expr.sym2) {
        symi2 = findSymbol(expr.sym2); // reference symbol, if any
        if (symi2 < 1) {errors.reportLine(ERR_SYMBOL_UNDEFINED);  return 0;}
    }

    ElfFWC_Rela2 relocation;                       // relocation, if needed
    bool needsRelocation = false;           
    // relocation needed

    if (symi1) {
        // there is a symbol
        if (symi2) {
            // difference between two symbols
            if (symbols[symi1].st_shndx == symbols[symi2].st_shndx && symbols[symi1].st_bind == STB_LOCAL && symbols[symi2].st_bind == STB_LOCAL) {
                // both symbols are local in same section. final value can be calculated
                value = (int64_t)(symbols[symi1].st_value - symbols[symi2].st_value);
                if (expr.symscale > 1) value /= expr.symscale;
            }
            else {
                // symbols are in different section or external. relocation needed
                relocation.r_type = R_FORW_REFP;          // relative to arbitrary reference point
                if (expr.symscale > 1) relocation.r_type |= bitScanReverse(expr.symscale);  // scale factor
                relocation.r_sym = expr.sym1;              // Symbol index
                relocation.r_refsym = expr.sym2;           // Reference symbol
                relocation.r_addend = int32_t(expr.value.w);      // Addend
                needsRelocation = true;
            }
        }
        else {
            // single symbol
            if (symbols[symi1].st_type & STT_CONSTANT) {
                // symbol is an external constant
                relocation.r_type = R_FORW_ABS;          // absolute value
                if (expr.symscale > 1) relocation.r_type |= bitScanReverse(expr.symscale);  // scale factor
                relocation.r_sym = expr.sym1;              // Symbol index
                relocation.r_refsym = 0;           // Reference symbol
                relocation.r_addend = int32_t(expr.value.w);      // Addend
                needsRelocation = true;
            }
            else if ((sectionHeaders[section].sh_flags & (SHF_WRITE | SHF_DATAP)) && fieldSize >= 4) {
                // other symbol. absolute address allowed only in writeable data section
                relocation.r_type = R_FORW_ABS;            // absolute value, 64 bits, no scale
                relocation.r_sym = expr.sym1;              // Symbol index
                relocation.r_refsym = 0;           // Reference symbol
                if (expr.symscale > 1) relocation.r_type |= bitScanReverse(expr.symscale);  // scale factor
                relocation.r_addend = int32_t(expr.value.w);      // Addend
                if (symbols[symi1].st_shndx && fieldSize < 4) {
                    expr.etype = XPR_ERROR;
                    value = ERR_ABS_RELOCATION;
                }
                needsRelocation = true;
            }
            else {
                // symbol without reference point not allowed here
                expr.etype = XPR_ERROR;
                value = ERR_ABS_RELOCATION;
            }
        }
    }
    else {
        // no symbol
        value = expr.value.i;
    }
    if (needsRelocation) {
        // relocation needed. insert source address
        relocation.r_offset = address;
        relocation.r_section = section;
        relocation.r_type |= fieldSize << 8;      // relocation size
        value = 0;   // value included in relocation addend
        relocations.push(relocation);  // save relocation
    }
    return value;
}

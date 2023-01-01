/****************************    assem4.cpp    ********************************
* Author:        Agner Fog
* Date created:  2017-04-17
* Last modified: 2022-12-28
* Version:       1.12
* Project:       Binary tools for ForwardCom instruction set
* Module:        assem.cpp
* Description:
* Module for assembling ForwardCom .as files.
* This module contains:
* pass3(): Interpretation of code lines.
* Copyright 2017-2022 GNU General Public License http://www.gnu.org/licenses
******************************************************************************/
#include "stdafx.h"


// Interpret lines. Generate code and data
void CAssembler::pass3() {
    uint16_t last_line_type = 0;       // type of preceding line
    makeFormatLists();                 // make formatList3 and formatList4
    code_size = cmd.codeSizeOption;    // initialize options
    data_size = cmd.dataSizeOption;
    section = 0;
    iLoop = iIf = iSwitch = 0;         // index of current high level statements
    
    // lines loop
    for (linei = 1; linei < lines.numEntries()-1; linei++) {
        tokenB = lines[linei].firstToken;      // first token in line        
        tokenN = lines[linei].numTokens; // number of tokens in line
        if (tokenN == 0 || lines[linei].type == LINE_ERROR || lines[linei].type == LINE_METADEF) continue;
        lineError = false;

        switch (lines[linei].type) {
        case LINE_DATADEF:
            if (last_line_type == LINE_CODEDEF && (lines[linei].sectionType & SHF_EXEC)) {
                /* currently, the assembler cannot mix code and data because they are put in different buffers.
                The only way to hard-code instructions is to put them into a separate section. */
                errors.reportLine(ERR_MIX_DATA_AND_CODE);   // data definition in code section
            }
            break;
        case LINE_CODEDEF:
            interpretCodeLine();
            if (last_line_type == LINE_DATADEF && !(lines[linei].sectionType & SHF_EXEC)) {
                errors.reportLine(ERR_MIX_DATA_AND_CODE);   // code definition in data section
            }
            break;
        case LINE_METADEF: case LINE_ERROR:
            continue;
        case LINE_FUNCTION:
            interpretFunctionDirective();
            break;
        case LINE_SECTION:
            interpretSectionDirective();
            break;
        case LINE_ENDDIR:
            interpretEndDirective();
            break;
        case LINE_OPTIONS:
            interpretOptionsLine();
            break;
        }

        last_line_type = lines[linei].type;
    }
    while (hllBlocks.numEntries()) {
        // unfinished block
        SBlock block = hllBlocks.pop();
        errors.report(tokens[block.startBracket].pos, tokens[block.startBracket].stringLength, ERR_BRACKET_BEGIN);
    }
}

// extract subsets of formatList (in disasm1.cpp) for multiformat instructions and jump instructions
void CAssembler::makeFormatLists() {
    uint32_t i;
    for (i = 0; i < formatListSize; i++) {
        if (formatList[i].category == 3) formatList3.push(formatList[i]);
        if (formatList[i].category == 4) formatList4.push(formatList[i]);
    }
}

// Interpret a line defining code. This covers both assembly style and high level style code
void CAssembler::interpretCodeLine() {
    uint32_t tok;                                // token index
    dataType = 0;                                // data type for current instruction
    uint32_t nReg = 0;                           // number of register source operands
    uint32_t state = 0;  /* state during interpretation of line. example:
        L1: int32 r1 = compare(r2, 5), option = 2   // assembly style
        L1: int32 r1 = r2 < 5                       // same in high level style
            0:  begin
            1:  after label
            2:  after label:
            3:  after type
            4:  after destination
            5:  after destination = (expecting expression or instruction)
            6:  after expression or instruction()
            7:  after instruction
            8:  after instruction(
            9:  after operand
           10:  after instruction(),
           11:  after jump instruction
    */  
    SExpression expr;                            // evaluated expression
    SCode code;                                  // current instruction code
    zeroAllMembers(code);                        // reset code structure

    if (section == 0) {
        errors.reportLine(ERR_CODE_WO_SECTION);
    }

    // high level instructions with nothing before can be caught already here
    if (tokens[tokenB].type == TOK_HLL) {
        interpretHighLevelStatement();    // if, else, switch, for, do, while (){} statements
        return;
    }
    if (tokens[tokenB].type == TOK_OPR && tokens[tokenB].id == '}') {
        interpretEndBracket();            // end of {} block
        return;
    }

    // interpret line by state machine looping through tokens
    for (tok = tokenB; tok < tokenB + tokenN; tok++) {
        SToken token = tokens[tok];
        if (token.type == TOK_XPR && expressions[token.value.w].etype & XPR_REG) {
            // this is an alias for a register. Translate to register
            token.type = TOK_REG;
            token.id = expressions[token.value.w].reg1;
        }

        if (lineError) break;
        code.section = section;

        if (state == 5) {  // after '='
            if (token.type == TOK_INS) {  // instruction
                if (code.instruction) errors.report(token);  // instruction after += etc.
                code.instruction = token.id;
                state = 7;
            }
            else {  // expression after equal sign
                // interpret expression representing operands and operator
                expr = expression(tok, tokenB + tokenN - tok, 0);
                if (lineError) return;
                if (code.instruction) {
                    // += operator etc. already encountered. combine the operands
                    uint32_t op = code.instruction;  code.instruction = 0;
                    code.reg1 = code.dest;  // first source operand is same as destination
                    code.etype |= XPR_REG1;  code.tokens = 0;
                    expr = op2(op, code, expr);  // operation '+' for '+=', etc.
                    code.instruction = 0;  code.reg1 = 0;
                }
                if (code.etype & XPR_ERROR) {
                    errors.reportLine(code.value.w); // report error
                }
                // ordinary '=' goes here
                if (lineError) return;
                insertAll(code, expr);
                tok += expr.tokens - 1;
                state = 6;
            }
        }
        else if (state == 11) {
            // interpret jump target
            expr = expression(tok, tokenB + tokenN - tok, 0);
            state = 6;
            if (expr.etype & XPR_REG) {
                code = code | expr;
                tok += expr.tokens - 1;
            }
            else if (expr.etype & (XPR_INT | XPR_SYM1)) {
                code.sym5 = expr.sym3 ? expr.sym3 : expr.sym1;
                code.offset_jump = expr.value.w;
                if (expr.value.w & 3) errors.report(token.pos, token.stringLength, ERR_JUMP_TARGET_MISALIGN);
                tok += expr.tokens - 1;
                code.etype |= XPR_JUMPOS | (expr.etype & ~XPR_IMMEDIATE);
            }
            else {
                errors.report(token.pos, token.stringLength, ERR_EXPECT_JUMP_TARGET);
                break;
            }
        }
        else if (state == 8 && token.type != TOK_OPT && token.type != TOK_REG) {
            // expression in parameter list
            if (token.type == TOK_OPR && token.id == ')') {
                state = 6; break;  // end of parameter list
            }
            // interpret any expression, except register or option
            expr = expression(tok, tokenB + tokenN - tok, 0);
            tok += expr.tokens - 1;
            if (code.etype & expr.etype & XPR_INT) {
                // multiple immediate integer constants
                if (code.etype & XPR_INT2) {
                    // three integer operands
                    if (code.etype & XPR_OPTIONS) errors.report(token.pos, token.stringLength, ERR_TOO_MANY_OPERANDS);
                    code.optionbits = uint8_t(expr.value.w);
                    code.etype |= XPR_OPTIONS;
                    expr.value.u = 0;
                }
                else {
                    // two integer operands
                    if (code.value.u >> 32 != 0) errors.report(token.pos, token.stringLength, ERR_TOO_MANY_OPERANDS);
                    code.value.u = code.value.w | expr.value.u << 32;
                    code.etype |= XPR_INT2;
                    expr.value.u = 0;
                }
            }
            else if (expr.etype & XPR_MEM) {
                if (expr.etype & XPR_OFFSET) code.offset_mem += expr.offset_mem;
                //else code.offset += expr.value.i;
                if (expr.etype & XPR_IMMEDIATE) {  // both memory and immediate operands
                    code.value.i = expr.value.i;
                }
            }
            else if (expr.etype & XPR_IMMEDIATE) {
                code.value.i = expr.value.i;
            }
            expr.value.i = 0;
            code = code | expr;
            state = 9;
        }
        else {
            switch (token.type) {
            case TOK_LAB:  case TOK_SYM:
                if (state == 0) {
                    //code.label = token.value.w;
                    code.label = token.id;
                    if (code.label) {
                        int32_t symi = findSymbol(code.label);
                        if (symi > 0) symbols[symi].st_section = section;
                    }
                    state = 1;
                }
                else goto ST_ERROR;
                break;
            case TOK_OPR:
                if (token.id == ':' && state == 1) {
                    state = 2;
                }
                else if (token.id == '+' && state == 3) {
                    code.dtype |= TYP_PLUS;
                }
                else if (token.priority == 15 && state == 4) {
                    // assignment operator
                    state = 5;
                    if (token.id & EQ) { // combined operator and assignment: += -= *= etc.
                        code.reg1 = code.dest;
                        code.etype |= XPR_REG | XPR_REG1;
                        code.instruction = token.id & ~EQ;  // temporarily store operator in .instruction
                    }
                    else if (token.id != '=') errors.report(token);
                }
                else if (token.id == '=' && state == 11) {
                    state = 12;
                }
                else if (token.id == ',' && state == 6) {
                    state = 10;
                }
                else if (token.id == ',' && state == 9) {
                    state = 8;
                }
                else if (token.id == '(' && state == 7) {
                    state = 8;
                }
                else if (token.id == ')' && (state == 8 || state == 9)) {
                    state = 6;
                }
                else if (token.id == '[' && (state == 0 || state == 2 || state == 3)) {
                    // interpret memory destination
                    expr = expression(tok, tokenB + tokenN - tok, 0);
                    tok += expr.tokens - 1;
                    insertMem(code, expr);
                    code.dest = 2;
                    state = 4;
                }
                else if (token.id == '[' && state == 7 && code.instruction == II_ADDRESS) {
                    // address []. expect memory operand
                    expr = expression(tok, tokenB + tokenN - tok, 0);
                    tok += expr.tokens - 1;
                    insertMem(code, expr);
                    state = 6;
                }
                else if ((token.id == '+' + D2 || token.id == '-' + D2) && (state == 3 || state == 4)) {
                    // ++ and -- operators
                    code.instruction = (token.id == '+' + D2) ? II_ADD : II_SUB;
                    // operand is 1, integer or float
                    if (dataType & TYP_FLOAT) {
                        code.value.d = 1.0;
                        code.etype |= XPR_FLT;
                    }
                    else {
                        code.value.i = 1;
                        code.etype |= XPR_INT;
                    }
                    if (state == 3) { // prefix operator. expect register
                        tok++;
                        if (token.type != TOK_REG) errors.report(token);
                        code.dest = token.id;
                    }
                    code.reg1 = code.dest;
                    code.etype |= XPR_REG1;
                    state = 6;
                }
                else if (token.id == ';') {} // ignore terminating ';'
                else goto ST_ERROR;
                break;
            case TOK_TYP:
                if (state == 0 || state == 2) {
                    dataType = code.dtype = token.id;
                    state = 3;
                }
                else goto ST_ERROR;
                break;
            case TOK_REG:
                if (state == 0 || state == 2 || state == 3) {
                    code.dest = uint8_t(token.id);
                    state = 4;
                }
                else if (state == 8) {
                    if (nReg < 3) {
                        (&code.reg1)[nReg] = (uint8_t)token.id;  // insert register in expression
                        code.etype |= XPR_REG1 << nReg++;
                        if ((code.etype & (XPR_INT | XPR_FLT | XPR_MEM)) && code.dest != 2)  errors.report(token.pos, token.stringLength, ERR_OPERANDS_WRONG_ORDER);
                    }
                    else errors.report(token.pos, token.stringLength, ERR_TOO_MANY_OPERANDS);
                    state = 9;
                }
                else goto ST_ERROR;
                break;
            case TOK_XPR: 
                if (token.value.u >= expressions.numEntries())  goto ST_ERROR; // expression not found
                if (expressions[token.value.w].etype & XPR_MEM) {  // this is an alias for a memory operand
                    insertMem(code, expressions[token.value.w]);
                    code.dest = 2;
                    state = 4;
                }
                else goto ST_ERROR;
                break;
            case TOK_INS:
                if (state == 0 || state == 2 || state == 3) {
                    // interpret instruction name
                    code.instruction = token.id;
                    state = 7;                              // expect parenthesis and parameters
                    if (code.instruction & II_JUMP_INSTR) {
                        // Jump or call instruction. The next may be a jump target, a register or a memory operand
                        state = 11;  // expect jump target
                        // Check if there is a memory operand
                        for (uint32_t tok2 = tok+1; tok2 < tokenB + tokenN; tok2++) {
                            if (tokens[tok2].type == TOK_OPR && tokens[tok2].id == '[') {
                                // a jump instruction with memory operand is treated as a normal instruction
                                state = 7;  break;
                            }
                        }
                    }
                }
                else if ((state == 6 || state == 10) && (token.id & II_JUMP_INSTR)) {
                    // second half of jump instruction
                    code.instruction |= token.id;    // combine two partial instruction names
                    state = 11;                            // expect jump target
                }
                else goto ST_ERROR;
                break;
            case TOK_OPT:  // option keyword
                expr = expression(tok, tokenB + tokenN - tok, 4);  // this will read option = value
                tok += expr.tokens - 1;
                code.etype |= expr.etype;
                if (expr.etype & XPR_LIMIT) {
                    code.value.i = expr.value.i;
                    if (expr.value.u >= 0x100000000U) { // limit too high
                        errors.report(tokens[tok - 1].pos, tokens[tok - 1].stringLength, ERR_LIMIT_TOO_HIGH);
                    }
                }
                if (expr.etype & (XPR_LENGTH | XPR_BROADC)) code.length = expr.length;
                if (expr.etype & XPR_MASK) code.mask = expr.mask;
                if (expr.etype & XPR_FALLBACK) code.fallback = expr.fallback;
                if (expr.etype & XPR_OPTIONS) code.optionbits = expr.optionbits;
                if (state == 8) state = 9;
                else if (state == 6 || state == 10) state = 6;
                else goto ST_ERROR;
                break;
            case TOK_ATT:
                if (token.id == ATT_ALIGN && state == 0 && tokenN >= 2) {  
                    // align n directive
                    code.instruction = II_ALIGN;
                    expr = expression(tok + 1, tokenB + tokenN - tok - 1, 0);
                    tok = tokenB + tokenN;
                    code.value.u = expr.value.u;
                    code.sizeUnknown = 0x80;
                    if ((code.value.u & (code.value.u - 1)) || code.value.u > MAX_ALIGN 
                    || (expr.etype & XPR_IMMEDIATE) != XPR_INT || (expr.etype & (XPR_REG|XPR_OPTION|XPR_MEM))) {
                        errors.reportLine(ERR_ALIGNMENT);
                    }
                }
                else goto ST_ERROR;
                break;
            case TOK_HLL:  // high level directive: if, else, while, for, etc.
                interpretHighLevelStatement();
                return;
            default:;
            ST_ERROR:
                errors.report(token);
                break;
            }
        }
    }
    if (lineError) return;
    // check if state machine ends with a finished instruction
    if (state != 0 && state != 2 && state != 6 && state != 7) {  
        errors.report(tokens[tok-1].pos, tokens[tok-1].stringLength, ERR_UNFINISHED_INSTRUCTION);
        return;
    }

    // move and store instruction has no operator yet
    if (code.instruction == 0 && code.etype) {
        if (code.dest == 2) code.instruction = II_STORE;  // store to memory
        else {
            code.instruction = II_MOVE;                   // move constant to register
            if (cmd.optiLevel && (code.etype & XPR_INT) && code.value.i >= 0 && !code.sym3 && (code.dtype & TYP_INT) && (code.dest & REG_R)) {
                code.dtype |= TYP_PLUS;                   // optimize to larger type for positive constant because it is zero-extended anyway
            }
        }
    }

    if (code.instruction) { // a code record with no instruction represents a label only
        // code record contains instruction
        if (code.etype & XPR_JUMPOS) mergeJump(code);

        checkCode1(code);
        if (lineError) return;

        // find an instruction variant that fits
        fitCode(code);
        if (lineError) return;
    }

    // save code structure
    codeBuffer.push(code);
}


// Check how many bits are needed to contain immediate constant of an instruction.
// The result is returned as bit-flags in code.fitNumX.
// The return value is nonzero if the size cannot be resolved yet.
int CAssembler::fitConstant(SCode & code) {
    int64_t value = 0;                           // the constant or address to fit
    int64_t valueScaled;                         // value divided by scale factor
    double dvalue = 0;                           // floating point value if needed
    bool floatType = false;                      // a floating point type is needed
    bool floatConst = false;                     // a floating point constant is provided
    uint32_t fitNum = 0;                         // return value
    uint32_t sym3 = 0, sym4 = 0;                 // symbols
    int32_t isym3 = 0, isym4 = 0;                // symbol index
    int32_t uncertainty;                         // maximum deviance if the value is uncertain
    int  uncertain = 0;                          // return value
    int symscale;                                // scaling of difference between symbols

    if (code.instruction == II_ALIGN) return 0;  // not an instruction
    if (!(code.etype & (XPR_IMMEDIATE | XPR_SYM1))) return 0; // has no immediate

    value = value0 = code.value.i;               // immediate constant
    floatType  = uint8_t(code.dtype) >= uint8_t(TYP_FLOAT16);  // floating point needed
    floatConst = (code.etype & XPR_FLT) != 0;    // floating point provided
    if (floatType) {
        if (floatConst) dvalue = code.value.d; 
        else {
            // Note: We are converting the immediate constant to floating point here in order to find
            // the optimal representation. We have not identified the instruction yet so we don't know
            // if it actually needs a floating point constant or an integer. We have saved the original
            // integer value in value0 so that we can undo the conversion in case an instruction with
            // floating point type needs an integer operand.
            dvalue = (double)value;  // value as float
            if (code.etype & XPR_INT) {
                // convert integer constant to float
                code.value.d = dvalue;
                code.etype = (code.etype & ~XPR_IMMEDIATE) | XPR_FLT;
                floatConst = true;
            }
        }
        if ((code.etype & XPR_FLT) && uint8_t(code.dtype) == uint8_t(TYP_FLOAT32)) {
            union {            // check for overflow in single precision float
                float f;
                uint32_t i;
            } u;
            u.f = float(code.value.d);
            if (isinf_f(u.i) && u.f > code.value.d) errors.reportLine(ERR_CONSTANT_TOO_LARGE);
        }
        if ((code.etype & XPR_FLT) && uint8_t(code.dtype) == uint8_t(TYP_FLOAT16)) {
            // check for overflow in half precision float
            if (isinf_h(double2half(code.value.d) && !isinf_d(code.value.i))) errors.reportLine(ERR_CONSTANT_TOO_LARGE);
        }
    }

    // check for symbols
    if (code.sym3) {
        sym3 = code.sym3; sym4 = code.sym4;
        symscale = code.symscale3;
        isym3 = findSymbol(sym3);
        if (isym3 < 1) {
            code.sizeUnknown = 2; return 2;              // should not occur
        }
    }

    if (code.sym3 && !code.sym4 && int32_t(symbols[isym3].st_section) == SECTION_LOCAL_VAR && symbols[isym3].st_type == STT_CONSTANT) {
        // convert local symbol to constant
        value = symbols[isym3].st_value;
        code.value.i = value;
        code.sym3 = 0;
        if (cmd.optiLevel && value >= 0 && (code.dtype & TYP_INT) && (code.dest & REG_R)) {
            code.dtype |= TYP_PLUS;        // optimize to larger type for positive constant because it is zero-extended anyway
        }
    }
    else if (sym3) {
        // there is a symbol
        if (symbols[isym3].st_unitsize == 0) uncertain = 2;  // symbol value is not known yet
        uint32_t sym3section = symbols[isym3].st_section; // symbol section
        // determine necessary relocation size if relocation needed
        uint64_t relSize;                       // maximum size of relocated address
        if (symbols[isym3].st_type == STT_CONSTANT) {
            relSize = 0x10000000;               // there is no command line option for the size of absolute symbols. assume 32 bit
            code.etype |= XPR_INT;
        }
        else if (sym3section && symbols[isym3].st_type != STT_CONSTANT) {   // local symbol with known section 
            relSize = (sectionHeaders[sym3section].sh_flags & (SHF_EXEC | SHF_IP)) ? code_size : data_size;
        }
        else { // external symbol with unknown section. look at symbol attributes
            relSize = (symbols[isym3].st_other & (STV_EXEC | STV_IP)) ? code_size : data_size;
            if (!(code.etype & (XPR_MEM | XPR_SYM2))) {
                errors.reportLine(ERR_CONFLICT_TYPE);  // must be memory operand
            }
        }
        if (sym4) {
            // value is (sym3 - sym4) / scale factor
            isym4 = findSymbol(sym4);
            if (isym4 <= 0) {
                code.sizeUnknown = 2; return 2;              // should not occur
            }
            code.etype |= XPR_INT;                           // symbol difference gives an integer
            if (symbols[isym3].st_unitsize == 0) uncertain = 2;  // symbol value is not known yet
            if (symbols[isym3].st_section != symbols[isym4].st_section || symbols[isym3].st_bind != STB_LOCAL || symbols[isym4].st_bind != STB_LOCAL) {
                // different sections or not local. relocation needed
                fitNum = IFIT_RELOC;
                if (code.symscale1 > 1) relSize /= code.symscale1;  // value is scaled
                if (relSize <= 1 << 7)  fitNum |= IFIT_I8;
                if (relSize <= 1 << 15) fitNum |= IFIT_I16;
                if (relSize <= (uint64_t)1 << 31) fitNum |= IFIT_I32;
                code.fitNum = fitNum;
                code.sizeUnknown = uncertain;
                return uncertain;
            }
            // difference between two local symbols
            if (pass < 4) {
                code.fitNum = IFIT_I8 | IFIT_I16 | IFIT_I32;  // symbol values are not available yet
                code.sizeUnknown = 1;
                return 1;
            }
            value += int32_t(uint32_t(symbols[isym3].st_value) - uint32_t(symbols[isym4].st_value));
            if (symscale < 1) symscale = 1;
            valueScaled = value / symscale + code.offset_mem;
            if (valueScaled >= -(1 << 7)  && valueScaled < (1 << 7))  fitNum |= IFIT_I8;
            if (valueScaled >= -(1 << 15) && valueScaled < (1 << 15)) fitNum |= IFIT_I16;
            if (valueScaled >= -((int64_t)1 << 31) && valueScaled < ((int64_t)1 << 31)) fitNum |= IFIT_I32;
            // check if value is certain. uncertainty is stored in high part of st_value
            uncertainty = (symbols[isym3].st_value >> 32) - (symbols[isym4].st_value >> 32);
            valueScaled = value / symscale + code.offset_mem + uncertainty;
            if (symscale > 1) valueScaled /= symscale;  // value is scaled
            if ((valueScaled < -(1 << 7)  || valueScaled >= (1 << 7))  && (fitNum & IFIT_I8))  uncertain |= 1;
            if ((valueScaled < -(1 << 15) || valueScaled >= (1 << 15)) && (fitNum & IFIT_I16)) uncertain |= 1;
            if ((valueScaled < -((int64_t)1 << 31) || valueScaled >= ((int64_t)1 << 31)) && (fitNum & IFIT_I32)) uncertain |= 1;
       
            if (uncertain && (code.fitNum & IFIT_LARGE)) {
                // choose the larger version if optimization process has convergence problems
                fitNum  = (fitNum & (fitNum - 1)) | IFIT_I32;  // remove the lowest set bit
                uncertain &= ~1;
            }
            code.fitNum = fitNum;
            code.sizeUnknown = uncertain;
            return uncertain;
        }
        // one symbol. must be constant
        if (sym3section != 0 && symbols[isym3].st_type != STT_CONSTANT && !(code.etype & XPR_MEM)) {
            errors.reportLine(ERR_MEM_WO_BRACKET);
            return 1;
        }

        if (sym3section && symbols[isym3].st_type != STT_CONSTANT && (sectionHeaders[sym3section].sh_flags & SHF_IP)) {
            // relative to instruction pointer
            if (sym3section != code.section || symbols[isym3].st_bind != STB_LOCAL) {
                // symbol is in different section or not local. relocation needed
                fitNum = IFIT_RELOC;
                if (relSize <= 1 << 7)  fitNum |= IFIT_I8;   // necessary relocation size
                if (relSize <= 1 << 15) fitNum |= IFIT_I16;
                if (relSize <= (uint64_t)1 << 31) fitNum |= IFIT_I32;
                code.fitNum = fitNum;
                code.sizeUnknown = uncertain;
                return uncertain;
            }
            if (pass < 4) {
                code.fitNum = IFIT_I8 | IFIT_I16 | IFIT_I32;  // symbol values are not available yet
                code.sizeUnknown = 1;
                return 1;
            }
            // self-relative address to local symbol
            value = int32_t((uint32_t)symbols[isym3].st_value - (code.address + code.size * 4));
            valueScaled = value + code.offset_mem;
            if (valueScaled >= -(1 << 7)  && valueScaled < (1 << 7))  fitNum |= IFIT_I8;
            if (valueScaled >= -(1 << 15) && valueScaled < (1 << 15)) fitNum |= IFIT_I16;
            if (valueScaled >= -((int64_t)1 << 31) && valueScaled < ((int64_t)1 << 31)) fitNum |= IFIT_I32;
            code.fitNum = fitNum;
            // check if value is certain. uncertainty is stored in high part of st_value and sh_link
            uncertainty = int32_t((symbols[isym3].st_value >> 32) - sectionHeaders[code.section].sh_link);
            valueScaled += uncertainty;
            if ((valueScaled < -(1 << 7)  || valueScaled >= (1 << 7))  && (fitNum & IFIT_I8))  uncertain |= 1;
            if ((valueScaled < -(1 << 15) || valueScaled >= (1 << 15)) && (fitNum & IFIT_I16)) uncertain |= 1;
            if ((valueScaled < -((int64_t)1 << 31) || valueScaled >= ((int64_t)1 << 31)) && (fitNum & IFIT_I32)) uncertain |= 1;
            if (uncertain && (code.fitNum & IFIT_LARGE)) {
                // choose the larger version if optimization process has convergence problems
                fitNum  = (fitNum & (fitNum - 1)) | IFIT_I32;  // remove the lowest set bit
                uncertain &= ~1;
            }
            code.fitNum = fitNum;
            code.sizeUnknown = uncertain;
            return uncertain;
        }

        // symbol is relative to data pointer or external constant. relocation needed
        fitNum = IFIT_RELOC;
        if (relSize <= 1 << 7)  fitNum |= IFIT_I8;
        if (relSize <= 1 << 15) fitNum |= IFIT_I16;
        if (relSize <= (uint64_t)1 << 31) fitNum |= IFIT_I32;
        code.fitNum = fitNum;
        code.sizeUnknown = uncertain;
        return uncertain;
    }
    // no symbol. only a constant
    if (floatType) {
        // floating point constant
        code.fitNum = fitFloat(dvalue);
        if (uint8_t(code.dtype) < uint8_t(TYP_FLOAT64)) code.fitNum |= FFIT_32;
        code.sizeUnknown = 0;
        return 0;
    }
    // integer constant
    uint32_t low;     // index of lowest set bit
    uint32_t high;    // index of highest set bit
    fitNum = 0;
    int nbits;
    if (value == int64_t(0x8000000000000000)) {  // prevent overflow of -value
        fitNum = 0;
    }
    else if (value >= 0) {
        low   = bitScanForward((uint64_t)value);    // lowest set bit
        high  = bitScanReverse((uint64_t)value);    // highest set bit
        //if (value < 8)       fitNum |= IFIT_I4;
        //if (value == 8)      fitNum |= IFIT_J4;
        //if (value < 0x10)    fitNum |= IFIT_U4;
        if (value < 0x80)    fitNum |= IFIT_I8 | IFIT_I8SHIFT;
        if (value == 0x80)   fitNum |= IFIT_J8;
        if (value <= 0xFF)   fitNum |= IFIT_U8;        
        if (value < 0x8000)  fitNum |= IFIT_I16 | IFIT_I16SH16;
        if (value == 0x8000) fitNum |= IFIT_J16;
        if (value <= 0xFFFF) fitNum |= IFIT_U16;
        if (high < 31) fitNum |= IFIT_I32;
        if (high < 32) fitNum |= IFIT_U32;
        if (value == 0x80000000U) fitNum |= IFIT_J32;
        nbits = high - low + 1;
        if (nbits < 8) fitNum |= IFIT_I8SHIFT;
        if (nbits < 16) {
            fitNum |= IFIT_I16SHIFT;
            if (low >= 16 && high < 31) fitNum |= IFIT_I16SH16;
        }
        if (nbits < 32) fitNum |= IFIT_I32SHIFT;
        if (low >= 32)  fitNum |= IFIT_I32SH32;
    }
    else {  // x < 0
        value = -value;
        low   = bitScanForward(value);    // lowest set bit
        high  = bitScanReverse(value);    // highest set bit
        //if (value <= 8)           fitNum |= IFIT_I4;
        if (value <= 0x80)        fitNum |= IFIT_I8 | IFIT_I8SHIFT;
        if (value <= 0x8000)      fitNum |= IFIT_I16 |IFIT_I16SH16 ;
        if (value <= 0x80000000U) fitNum |= IFIT_I32;
        nbits = high - low + 1;
        if (nbits < 8) fitNum |= IFIT_I8SHIFT;
        if (nbits < 16) {
            fitNum |= IFIT_I16SHIFT;
            if (low >= 16 && high <= 31) fitNum |= IFIT_I16SH16;
        }
        if (nbits < 32) fitNum |= IFIT_I32SHIFT;
        if (low >= 32)  fitNum |= IFIT_I32SH32;
    }
    code.fitNum = fitNum;
    code.sizeUnknown = 0;
    return 0;
}


// Check how many bits are needed to a relative address or jump offset of an instruction.
// This result is returned as bit-flags in codefitAddr, code.fitJump, and code.fitNum
// The return value is nonzero if the size cannot be resolved yet.
int CAssembler::fitAddress(SCode & code) {
    int64_t value = 0;                           // the constant or address to fit
    int64_t valueScaled;                         // value divided by scale factor
    uint32_t fitBits = 0;                        // bit flags indicating fit
    int32_t isym1 = 0, isym2 = 0;                // symbol index
    int32_t uncertainty;                         // maximum deviance if the value is uncertain
    int  uncertain = 0;                          // return value

    if (code.instruction == II_ALIGN) return 0;              // not an instruction
    if (!(code.etype & (XPR_OFFSET | XPR_JUMPOS | XPR_MEM))) return 0; // has no address

    // check address of memory operand
    if (code.sym1) {
        // there is a memory operand symbol
        code.etype |= XPR_OFFSET;

        value = code.offset_mem;                                 // memory offset
        isym1 = findSymbol(code.sym1);
        if (isym1 <= 0) {
            code.sizeUnknown = 2; return 2;              // should not occur
        }
        if (symbols[isym1].st_unitsize == 0) uncertain = 2;  // symbol value is not known yet
        uint32_t sym1section = symbols[isym1].st_section; // symbol section
        if (sym1section < sectionHeaders.numEntries()) {
            // determine necessary relocation size if relocation needed
            uint64_t relSize;                       // maximum size of relocated address
            if (symbols[isym1].st_type == STT_CONSTANT) {
                // assume that constant offset is limited by dataSizeOption
                relSize = data_size;       // relocation size for code and constant data                
            }
            else if (sym1section 
                && !(sectionHeaders[sym1section].sh_flags & (SHF_WRITE | SHF_DATAP | SHF_THREADP))) {
                relSize = code_size;       // relocation size for code and constant data
            }
            else if (sym1section) {   // local symbol with known section 
                relSize = (sectionHeaders[sym1section].sh_flags & (SHF_EXEC | SHF_IP)) ? code_size : data_size;
            }
            else { // external symbol with unknown section. look at symbol attributes
                relSize = (symbols[isym1].st_other & (STV_EXEC | STV_IP)) ? code_size : data_size;
            }
            if (code.sym2) {
                // value is (sym1 - sym2) / scale factor
                isym2 = findSymbol(code.sym2);
                if (isym2 <= 0) {
                    code.sizeUnknown = 2; return 2;              // should not occur
                }
                if (symbols[isym1].st_unitsize == 0) uncertain = 2;  // symbol value is not known yet
                if (symbols[isym1].st_section != symbols[isym2].st_section || symbols[isym1].st_bind != STB_LOCAL || symbols[isym2].st_bind != STB_LOCAL) {
                    // different sections or not local. relocation needed
                    fitBits = IFIT_RELOC;
                    if (code.symscale1 > 1) relSize /= code.symscale1;  // value is scaled
                    if (relSize <= 1 << 7)  fitBits |= IFIT_I8;
                    if (relSize <= 1 << 15) fitBits |= IFIT_I16;
                    //if (relSize <= 1 << 23) fitBits |= IFIT_I24;
                    if (relSize <= (uint64_t)1 << 31) fitBits |= IFIT_I32;
                    code.fitAddr = fitBits;
                    code.sizeUnknown += uncertain;
                    //return uncertain;
                }
                // difference between two local symbols
                else if (pass < 4) {
                    code.fitAddr = IFIT_I8 | IFIT_I16 | IFIT_I32;  // symbol values are not available yet
                    code.sizeUnknown += 1;
                    uncertain += 1;
                    //return 1;
                }
                else {
                    value += int32_t(uint32_t(symbols[isym1].st_value) - uint32_t(symbols[isym2].st_value));
                    int scale = code.symscale1;
                    if (scale < 1) scale = 1;
                    valueScaled = value / scale + code.offset_mem;
                    if (valueScaled >= -(1 << 7) && valueScaled < (1 << 7))  fitBits |= IFIT_I8;
                    if (valueScaled >= -(1 << 15) && valueScaled < (1 << 15)) fitBits |= IFIT_I16;
                    if (valueScaled >= -((int64_t)1 << 31) && valueScaled < ((int64_t)1 << 31)) fitBits |= IFIT_I32;
                    // check if value is certain. uncertainty is stored in high part of st_value
                    uncertainty = (symbols[isym1].st_value >> 32) - (symbols[isym2].st_value >> 32);
                    valueScaled = value / scale + code.offset_mem + uncertainty;
                    if (code.symscale1 > 1) valueScaled /= code.symscale1;  // value is scaled
                    if ((valueScaled < -(1 << 7) || valueScaled >= (1 << 7)) && (fitBits & IFIT_I8))  uncertain |= 1;
                    if ((valueScaled < -(1 << 15) || valueScaled >= (1 << 15)) && (fitBits & IFIT_I16)) uncertain |= 1;
                    if ((valueScaled < -((int64_t)1 << 31) || valueScaled >= ((int64_t)1 << 31)) && (fitBits & IFIT_I32)) uncertain |= 1;
                    if (uncertain && (code.fitAddr & IFIT_LARGE)) {
                        // choose the larger version if optimization process has convergence problems
                        fitBits = (fitBits & (fitBits - 1)) | IFIT_I32;  // remove the lowest set bit
                        uncertain &= ~1;
                    }
                    code.fitAddr = fitBits;
                    code.sizeUnknown += uncertain;
                    //return uncertain;
                }
            }
            // one symbol
            else if (sectionHeaders[sym1section].sh_flags & SHF_IP) {
                // relative to instruction pointer
                if (sym1section != code.section || symbols[isym1].st_bind != STB_LOCAL) {
                    // symbol is in different section or not local. relocation needed
                    fitBits = IFIT_RELOC;
                    if (code.etype & XPR_JUMPOS) relSize >>= 2;  // value is scaled by 4
                    if (relSize <= 1 << 7)  fitBits |= IFIT_I8;   // necessary relocation size
                    if (relSize <= 1 << 15) fitBits |= IFIT_I16;
                    if (relSize <= 1 << 23) fitBits |= IFIT_I24;
                    if (relSize <= (uint64_t)1 << 31) fitBits |= IFIT_I32;
                    code.fitAddr = fitBits;
                    code.sizeUnknown += uncertain;
                    //return uncertain;
                }
                else if (pass < 4) {
                    // code.fitBits = IFIT_I16 | IFIT_I32;  // symbol values are not available yet
                    code.fitAddr = IFIT_I16 | IFIT_I24 | IFIT_I32;  // symbol values are not available yet
                    code.sizeUnknown += 1;
                    uncertain |= 1;
                    //return 1;
                }
                else {  // self-relative address to local symbol
                    value = int32_t((uint32_t)symbols[isym1].st_value - (code.address + code.size * 4));
                    valueScaled = value;
                    valueScaled += code.offset_mem;
                    if (valueScaled >= -(1 << 15) && valueScaled < (1 << 15)) fitBits |= IFIT_I16;
                    if (valueScaled >= -(1 << 23) && valueScaled < (1 << 23)) fitBits |= IFIT_I24;
                    if (valueScaled >= -((int64_t)1 << 31) && valueScaled < ((int64_t)1 << 31)) fitBits |= IFIT_I32;
                    code.fitAddr = fitBits;
                    // check if value is certain. uncertainty is stored in high part of st_value and sh_link
                    uncertainty = int32_t((symbols[isym1].st_value >> 32) - sectionHeaders[code.section].sh_link);
                    valueScaled += uncertainty;
                    if ((valueScaled < -(1 << 7) || valueScaled >= (1 << 7)) && (fitBits & IFIT_I8))  uncertain |= 1;
                    if ((valueScaled < -(1 << 15) || valueScaled >= (1 << 15)) && (fitBits & IFIT_I16)) uncertain |= 1;
                    if ((valueScaled < -(1 << 23) || valueScaled >= (1 << 23)) && (fitBits & IFIT_I24)) uncertain |= 1;
                    if ((valueScaled < -((int64_t)1 << 31) || valueScaled >= ((int64_t)1 << 31)) && (fitBits & IFIT_I32)) uncertain |= 1;
                    if (uncertain && (code.fitAddr & IFIT_LARGE)) {
                        // choose the larger version if optimization process has convergence problems
                        fitBits = (fitBits & (fitBits - 1)) | IFIT_I32;  // remove the lowest set bit
                        uncertain &= ~1;
                    }
                    code.fitAddr = fitBits;
                    code.sizeUnknown += uncertain;
                    //return uncertain;
                }
            }
            else {
                // symbol is relative to data pointer. relocation needed
                fitBits = IFIT_RELOC;
                if (relSize <= 1 << 7)  fitBits |= IFIT_I8;
                if (relSize <= 1 << 15) fitBits |= IFIT_I16;
                if (relSize <= (uint64_t)1 << 31) fitBits |= IFIT_I32;
                code.fitAddr = fitBits;
                code.sizeUnknown += uncertain;
            }
        }
    }
    else {
        // no symbol. only a signed integer constant
        value = code.offset_mem;
        fitBits = 0;
        if (value >= -(int64_t)0x80 && value < 0x80) fitBits |= IFIT_I8;
        if (value >= -(int64_t)0x8000 && value < 0x8000) fitBits |= IFIT_I16;
        if (value >= -(int64_t)0x80000000 && value < 0x80000000) fitBits |= IFIT_I32;
        code.fitAddr = fitBits;
    }
    
    // check jump offset symbol
    if (code.sym5) {
        // there is a jump offset symbol
        value = code.offset_jump;                     // jump offset
        fitBits = 0;

        isym1 = findSymbol(code.sym5);
        if (isym1 <= 0) {
            code.sizeUnknown = 2; return 2;              // should not occur
        }
        // one symbol relative to instruction pointer
        if (symbols[isym1].st_unitsize == 0) uncertain = 2;  // symbol value is not known yet
        uint32_t sym1section = symbols[isym1].st_section; // symbol section
        if (sym1section < sectionHeaders.numEntries()) {
            // determine necessary relocation size if relocation needed
            uint64_t relSize;                       // maximum size of relocated address
            relSize = code_size >> 2;               // relocation size for code and constant data, scaled by 4
            
            if (sym1section != code.section || symbols[isym1].st_bind != STB_LOCAL) {
                // symbol is in different section or not local. relocation needed
                fitBits = IFIT_RELOC;
                if (relSize <= 1 << 7)  fitBits |= IFIT_I8;   // necessary relocation size
                if (relSize <= 1 << 15) fitBits |= IFIT_I16;
                if (relSize <= 1 << 23) fitBits |= IFIT_I24;
                if (relSize <= (uint64_t)1 << 31) fitBits |= IFIT_I32;
                code.fitJump = fitBits;
                code.sizeUnknown += uncertain;
                //return uncertain;
            }
            else if (pass < 4) {
                code.fitJump = IFIT_I16 | IFIT_I24 | IFIT_I32;  // symbol values are not available yet
                code.sizeUnknown += 1;
                uncertain = 1;
                //return 1;
            }
            else {
                // self-relative address to local symbol
                value = int32_t((uint32_t)symbols[isym1].st_value - (code.address + code.size * 4));
                valueScaled = value >> 2;  // jump address is scaled
                valueScaled += code.offset_jump;
                if (valueScaled >= -(1 << 7)  && valueScaled < (1 << 7))  fitBits |= IFIT_I8;
                if (valueScaled >= -(1 << 15) && valueScaled < (1 << 15)) fitBits |= IFIT_I16;
                if (valueScaled >= -(1 << 23) && valueScaled < (1 << 23)) fitBits |= IFIT_I24;
                if (valueScaled >= -((int64_t)1 << 31) && valueScaled < ((int64_t)1 << 31)) fitBits |= IFIT_I32;
                code.fitJump = fitBits;
                // check if value is certain. uncertainty is stored in high part of st_value and sh_link
                uncertainty = int32_t((symbols[isym1].st_value >> 32) - sectionHeaders[code.section].sh_link);
                valueScaled += uncertainty;
                if ((valueScaled < -(1 << 7)  || valueScaled >= (1 << 7)) && (fitBits & IFIT_I8))   uncertain |= 1;
                if ((valueScaled < -(1 << 15) || valueScaled >= (1 << 15)) && (fitBits & IFIT_I16)) uncertain |= 1;
                if ((valueScaled < -(1 << 23) || valueScaled >= (1 << 23)) && (fitBits & IFIT_I24)) uncertain |= 1;
                if ((valueScaled < -((int64_t)1 << 31) || valueScaled >= ((int64_t)1 << 31)) && (fitBits & IFIT_I32)) uncertain |= 1;
                if (uncertain && (code.fitAddr & IFIT_LARGE)) {
                    // choose the larger version if optimization process has convergence problems
                    fitBits = (fitBits & (fitBits - 1)) | IFIT_I32;  // remove the lowest set bit
                    uncertain &= ~1;
                    code.fitJump = fitBits;
                    //code.sizeUnknown += uncertain;
                }
                code.sizeUnknown += uncertain;
            }
        }
    }
    return uncertain;
}


// find format details in formatList from entry in instructionlist
uint32_t findFormat(SInstruction const & listentry, uint32_t imm) {
    // listentry: record in instructionlist or instructionlistId
    // imm: immediate operand, if any

    // make model instruction for lookupFormat
    STemplate instrModel;
    instrModel.a.il = listentry.format >> 8;
    instrModel.a.mode = (listentry.format >> 4) & 7;
    instrModel.a.ot = (listentry.format >> 5) & 4;
    if ((listentry.format & ~ 0x12F) == 0x200) {  // format 0x200, 0x220, 0x300, 0x320
        instrModel.a.mode2 = listentry.format & 7;
    }
    else if ((listentry.format & 0xFF0) == 0x270 && listentry.op1 < 8) {
        instrModel.a.mode2 = listentry.op1 & 7;
    }
    else instrModel.a.mode2 = 0;
    instrModel.a.op1 = listentry.op1;
    instrModel.b[0] = imm & 0xFF;
    // look op details for this format (from emulator2.cpp)
    return lookupFormat(instrModel.q);
}

// find the smallest representation that the floating point operand fits into
int fitFloat(double x) {
    if (x == 0.) return IFIT_I8 | FFIT_16 | FFIT_32 | FFIT_64;
    union {
        double d;
        struct {
            uint64_t mantissa: 52;
            uint64_t exponent: 11;
            uint64_t sign:      1;
        } f;
    } u;
    u.d = x;
    int fit = FFIT_64;
    // check if mantissa fits
    if ((u.f.mantissa & (((uint64_t)1 << 42) - 1)) == 0) fit |= FFIT_16;
    if ((u.f.mantissa & (((uint64_t)1 << 29) - 1)) == 0) fit |= FFIT_32;
    // check if exponent fits, except for infinity or nan
    if (u.f.exponent != 0x7FF) {
        int ex = int(u.f.exponent - 0x3FF);
        if (ex < -14 || ex > 15) fit &= ~FFIT_16;
        if (ex < -126 || ex > 127) fit &= ~FFIT_32;       
    }
    // check if x fits into a small integer
    if (fit & FFIT_16) {
        int i = int(x);
        if (i == x && i >= -128 && i < 128) {
            fit |= IFIT_I8;
        }
    }
    return fit;
}

// find an instruction variant that fits the code
int CAssembler::fitCode(SCode & code) {
    // return value:
    // 0: does not fit
    // 1: fits
    uint32_t bestInstr = 0;                      // best fitting instruction variant, index into instructionlistId
    uint32_t bestSize  = 99;                     // size of best fitting instruction variant
    SCode    codeTemp;                           // fitted code
    SCode    codeBest;                           // best fitted code
    uint32_t instrIndex = 0, ii;                 // index into instructionlistId
    uint32_t formatIx = 0;                       // index into formatList
    uint32_t isize;                              // il bits
    codeBest.category = 0;

    // find instruction by id    
    SInstruction3 sinstr;                        // make dummy record with instruction id as parameter to findAll
    if (code.instruction == II_ALIGN) {
        return 1;                                // alignment directive
    }
    sinstr.id = code.instruction; 
    int32_t nInstr = instructionlistId.findAll(&instrIndex, sinstr);

    if (code.etype & (XPR_IMMEDIATE | XPR_OFFSET | XPR_LIMIT | XPR_JUMPOS)) {
        // there is an immediate constant, offset, or limit.
        // generate specific error message if large constant cannot fit
        if ((code.etype & XPR_OFFSET) && !(code.etype & XPR_IMMEDIATE) && !(code.fitAddr & IFIT_I32))  {
            errors.reportLine(ERR_OFFSET_TOO_LARGE);
        }
        //else if ((code.etype & XPR_LIMIT) && !(code.fitBits & (IFIT_U16 | IFIT_U32)))  errors.reportLine(ERR_LIMIT_TOO_LARGE);
        else if ((code.etype & XPR_IMMEDIATE) && !(code.etype & XPR_INT2)) {
            if (!(code.fitNum & (IFIT_I16 | IFIT_I16SHIFT | IFIT_I32 | IFIT_I32SHIFT | FFIT_16 | FFIT_32)) && (code.etype & XPR_OPTIONS) && code.optionbits) {
                errors.reportLine(ERR_IMMEDIATE_TOO_LARGE);
            }
        } 
    }
    if (lineError) return 0;

    // loop through all instruction definitions with same id
    for (ii = instrIndex; ii < instrIndex + nInstr; ii++) {
        // category
        code.instr1 = ii;
        code.category = instructionlistId[ii].category;
        // get variant bits from instruction list
        variant = instructionlistId[ii].variant;  // instruction-specific variants

        if ((variant & VARIANT_U3) && (code.dtype & TYP_UNS) && ii != II_COMPARE) {
            code.optionbits |= 8;  // unsigned min, max
            code.etype |= XPR_OPTIONS;
        }

        switch (instructionlistId[ii].category) {
        case 1:   // single format. find entry in formatList
            formatIx = findFormat(instructionlistId[ii], code.value.w);
            code.formatp = formatList + formatIx;
            if (instructionFits(code, codeTemp, ii)) {
                // check if smaller than previously found.
                isize = codeTemp.size;
                if (isize < bestSize) {
                    bestSize = isize;
                    bestInstr = ii;
                    codeBest = codeTemp;
                }
            }
            break;

        case 3:  // multi-format instructions. search all formats for the best one
            for (formatIx = 0; formatIx < formatList3.numEntries(); formatIx++) {
                code.formatp = &formatList3[formatIx];

                if (((uint64_t)1 << code.formatp->formatIndex) & instructionlistId[ii].format) {
                    if (instructionFits(code, codeTemp, ii)) {
                        // check if smaller than previously found. category 3 = multiformat preferred
                        isize = codeTemp.size;
                        if (isize < bestSize || (isize == bestSize && codeBest.category != 3)) {
                            bestSize = isize;
                            bestInstr = ii;
                            codeBest = codeTemp;
                        }
                    }                    
                }
            }
            break;

        case 4:  // jump instructions. search all formats for the best one
            for (formatIx = 0; formatIx < formatList4.numEntries(); formatIx++) {
                code.formatp = &formatList4[formatIx];
                if (((uint64_t)1 << code.formatp->formatIndex) & instructionlistId[ii].format) {
                    if (jumpInstructionFits(code, codeTemp, ii)) {
                        // check if smaller than previously found. category 3 = multiformat preferred
                        isize = codeTemp.size;
                        if (isize < bestSize) {
                            bestSize = isize;
                            bestInstr = ii;
                            codeBest = codeTemp;
                        }
                    }                    
                }
            }
            break;

        default:
            return 0;        // error in list
        }
    }

    if (bestSize > 4) {
        errors.reportLine(checkCodeE(code));         // find reason why no format fits, and report error
        return 0;
    }

    code = codeBest;          // get the best fitting code
    variant = instructionlistId[bestInstr].variant;  // instruction-specific variants
        
    checkCode2(code);         // check if operands are correct

    if (lineError) return 0;
    return 1;
}


// check if instruction fits into specified format
bool CAssembler::instructionFits(SCode const & code, SCode & codeTemp, uint32_t ii) {
    // code: structure defining all operands and options
    // codeTemp: fitted code
    // ii: index into instructionlistId
    // formatIndex: index into formatList

    uint32_t shiftCount;                         // shift count for shifted constant
    // copy code structure and add details
    codeTemp = code;
    codeTemp.category = code.formatp->category;
    codeTemp.size = (code.formatp->format2 >> 8) & 3;
    if (codeTemp.size == 0) codeTemp.size = 1;
    codeTemp.instr1 = ii;

    if (instructionlistId[ii].opimmediate == OPI_IMPLICIT && !(code.etype & XPR_IMMEDIATE)) {
        // There is no immediate operand. instructionlistId[ii] has an implicit immediate operand.
        // Insert implicit operand and see if it fits
        codeTemp.value.u = instructionlistId[ii].implicit_imm;
        codeTemp.etype |= XPR_INT;
        codeTemp.fitNum = 0xFFFFFFFF;
    }

    // check vector use
    bool useVectors = (code.dtype & TYP_FLOAT) 
        || (code.dest & 0xE0) == REG_V
        || (code.reg1 & 0xE0) == REG_V
        || (code.reg2 & 0xE0) == REG_V;

    if (useVectors) {
        if (!(code.formatp->vect)) return false;  // vectors not supported
    }
    else if (code.formatp->vect & ~0x10) return false;    // vectors provided but not used

    // requested operand type
    uint32_t requestOT = code.dtype & 7;
    if (uint8_t(code.dtype) == uint8_t(TYP_FLOAT16)) {
        requestOT = TYP_INT16 & 7;                // replace pseudo-type TYP_FLOAT16 with TYP_INT16
        codeTemp.dtype = TYP_INT16;
    }

    // operand type provided by this format
    uint32_t formatOT = code.formatp->ot;
    if (formatOT == 0x32) formatOT = 0x12 + (instructionlistId[ii].op1 & 1);  // int32 for even op1, int64 for odd op1
    if (formatOT == 0x35) formatOT = 0x15 + (instructionlistId[ii].op1 & 1);  // float for even op1, double for odd op1
    if (formatOT == 0) formatOT = requestOT;  // operand type determined by OT field
    formatOT &= 7;
    uint32_t scale2 = formatOT;
    if (scale2 > 4) scale2 -= 3;  // operand size = 1 << scale2

    if (variant & (VARIANT_D0 | VARIANT_D2)) {  // no operand type
        if (code.dtype == 0 && code.instruction != II_NOP) codeTemp.dtype = formatOT ? formatOT : 3;
    }
    else {
        // check requested operand type 
        if (formatOT <= 3 && requestOT < formatOT && (code.dtype & TYP_PLUS)) {
            requestOT = formatOT;  // request allows bigger type
            // codeTemp.dtype = formatOT;  // prevents merging with subsequent jump with smaller type than formatOT
        }
        if (requestOT != formatOT && code.dtype) return false;  // requested format type not supported

        // check if operand type supported by instruction
        uint32_t optypessupport = useVectors ? (instructionlistId[ii].optypesscalar | instructionlistId[ii].optypesvector) : instructionlistId[ii].optypesgp;
        optypessupport |= optypessupport >> 8;  // include types with optional support
        if (!(optypessupport & (1 << requestOT))) return false;
    }

    // check if there are enough register operands in this format
    uint8_t opAvail = code.formatp->opAvail;
    uint8_t numReg = ((opAvail >> 4) & 1) + ((opAvail >> 5) & 1) + ((opAvail >> 6) & 1) + ((opAvail >> 7) & 1); // number of registers available
    uint8_t numReq = instructionlistId[ii].sourceoperands;  // number of registers required for this instruction
    codeTemp.numOp = numReq;
    if ((codeTemp.etype & XPR_IMMEDIATE) && numReq) numReq--;
    if ((codeTemp.etype & XPR_MEM) && numReq) numReq--;
    if ((codeTemp.etype & (XPR_MASK | XPR_FALLBACK)) && ((code.fallback & 0x1F) != (code.reg1 & 0x1F) || (code.reg1 & 0x1F) == 0x1F)) {
        numReq += 2;  // fallback different from reg1, implies reg1 != destination
    }
    else if ((code.etype & XPR_REG1) && code.dest && code.reg1 != code.dest && !(variant & VARIANT_D3)) {
        numReq++;     // reg1 != destination
    }
    if (numReq > numReg) return false;  // not enough registers in this format

    // check if mask available
    if ((code.etype & XPR_MASK) && !(code.formatp->tmplate == 0xA || code.formatp->tmplate == 0xE)) return false;

    // check option bits 
    if ((code.etype & XPR_OPTIONS) && code.optionbits != 0 
        && (code.formatp->tmplate != 0xE || !(code.formatp->imm2 & 2))
        && (variant & VARIANT_On) && instructionlistId[ii].opimmediate != OPI_INT1688) return false; // only template E has option bits

    // check memory operand
    if (code.etype & XPR_MEM) {
        if (code.formatp->mem == 0) return false;  // memory operand requested but not supported
        if (code.etype & XPR_SYM1) {  // has data symbol
            if (code.etype & XPR_SYM2) {  // has difference between two symbols
                codeTemp.sizeUnknown = 1;
            }
            //if (!(code.fitNumX & IFIT_I32)) return false;  // assume symbol address requires 32 bits. local symbol difference resolved later when sizeUnknown = 1
        }
        // check index and scale factor
        if (code.etype & XPR_INDEX) {
            if (!(code.formatp->mem & 4)) return false;  // index not supported
            if ((code.formatp->scale & 4) && code.scale != -1) return false;  // scale factor must be -1
            if ((code.formatp->scale & 2) && code.scale != 1 << scale2) return false;  // scale factor must match operand type
            if (!(code.formatp->scale & 6) && code.scale != 1) return false;  // scale factor must be 1
        }
        else {  // no index requested
            if (code.formatp->mem & 4) {
                codeTemp.index = 0x1F;  // RT = 0x1F means no index
                codeTemp.scale = 1 << scale2;
            }
        }

        // check address offset size
        if (code.etype & (XPR_OFFSET | XPR_SYM1)) {
            if (!(code.formatp->mem & 0x10)) return false;  // format does not support memory offset
            switch (code.formatp->addrSize) {
            case 1:
                if (code.sym1 && !(code.fitAddr & IFIT_I8)) return false;
                if ((code.base & 0x1F) >= 0x1C && (code.base & 0x1F) != 0x1F) return false; // ip, datap, threadp must have 16 bit offset
                // no relocation. scale factor depends on operand size
                if (code.offset_mem & ((1 << scale2) - 1)) return false;  // offset is not a multiple of the scale factor
                if ((code.offset_mem >> scale2) < -0x80 || (code.offset_mem >> scale2) > 0x7F) return false;
                break;
            case 2:
                if (!(code.fitAddr & IFIT_I16)) return false;
                break;
            case 4:
                if (!(code.fitAddr & IFIT_I32)) return false;
                break;
            default:
                return false;
            }
        }
        else if ((code.formatp->addrSize) < 2 && (code.base & 0x1F) >= 0x1C && (code.base & 0x1F) != 0x1F) return false;

        // fail if limit required and not supported, or supported and not required
        if (code.etype & XPR_LIMIT) {
            if (!(code.formatp->mem & 0x20)) return false;     // limit not supported by format
            switch (code.formatp->addrSize) {
            case 1: if (code.value.u >= 0x100) return false;
                break;
            case 2: if (code.value.u >= 0x10000) return false;
                break;
            case 4: if (uint64_t(code.value.u) >= 0x100000000U) return false;
                break;
            }
        }
        else {
            if (code.formatp->mem & 0x20) return false;     // limit provided but not requested
        }

        // check length/broadcast/scalar
        if (code.etype & XPR_SCALAR) {                            // scalar operand requested
            if ((code.formatp->vect & 6) != 0) {
                codeTemp.length = 31;                            // disable length or broadcast option
            }
        }
        else if (code.etype & XPR_LENGTH) {              // vector length specified
            if ((code.formatp->vect & 2) == 0) return false;  // vector length not in this format
        }
        else if (code.etype & XPR_BROADC) {              // vector broadcast specified
            if ((code.formatp->vect & 4) == 0) return false;  // vector broadcasst not in this format
        }
    }
    else if (code.formatp->mem) return false;  // memory operand supported by not requested

    // check immediate operand
    //bool isFloat = (code.dtype & TYP_FLOAT32 & 0xF0) != 0; // specified type is float or double or float128 
    bool hasImmediate = (code.etype & XPR_IMMEDIATE) != 0; // && !(code.etype & (XPR_OFFSET | XPR_LIMIT)));

    /*if ((variant & VARIANT_M1) && code.formatp->mem && code.formatp->tmplate == 0xE) {
        // variant M1: immediate operand is in IM5. No further check needed
        // to do: fail if relocation on immediate
        return hasImmediate;  // succeed if there is an immediate
    } */

    if (hasImmediate) {
        if (code.formatp->immSize == 0 && instructionlistId[ii].sourceoperands < 4) return false;  // immediate not supported

        // to do: check if relocation

        // check if size fits. special cases in instruction list
        switch (instructionlistId[ii].opimmediate) { 
        case OPI_IMPLICIT:  // implicit value of immediate operand. Accept explicit value only if same
            if (codeTemp.value.u != instructionlistId[ii].implicit_imm) return false;
            break;

        case OPI_INT8SH:  // im2 << im1
            if (code.fitNum & (IFIT_I8 | IFIT_I8SHIFT)) {   // fits im2 << im1
                shiftCount = bitScanForward(codeTemp.value.u);
                codeTemp.value.u = (codeTemp.value.u >> shiftCount << 8) | shiftCount;
                codeTemp.fitNum |= IFIT_I16;  // make it accepted below
                break;
            }
            return false;
        case OPI_INT16SH16: // im12 << 16
            if (code.fitNum & (IFIT_I16 | IFIT_I16SH16)) {   // fits im12 << 16
                codeTemp.value.u = codeTemp.value.u >> 16;
                codeTemp.fitNum |= IFIT_I16;  // make it accepted below
                break;
            }
            return false;
        case OPI_INT32SH32: // im6 << 32
            if (code.fitNum & (IFIT_I32 | IFIT_I32SH32)) {   // fits im6 << 32
                codeTemp.value.u = codeTemp.value.u >> 32;
                codeTemp.fitNum |= IFIT_I32;  // make it accepted below
                break;
            }
            return false;
        case OPI_UINT8: // 8 bit unsigned integer
            if (value0 < 0x100 && value0 > -(int64_t)0x80U) return true;
            return false;
        case OPI_UINT16: // 16 bit unsigned integer
            if (value0 < 0x10000 && value0 > -(int64_t)0x8000U) return true;
            return false;
        case OPI_UINT32: // 32 bit unsigned integer
            //if (code.fitNum & IFIT_U32) return true; // this does not work if a float type is specified
            if (value0 < 0x100000000 && value0 > -(int64_t)0x80000000U) return true;
            return false;
        case OPI_INT886:  // three integers
            codeTemp.value.u = (codeTemp.value.w & 0xFF) | (codeTemp.value.u >> 24);
            return true;
        case OPI_INT1688:  // three integers: 16 + 8 + 8 bits
            codeTemp.value.u = (codeTemp.value.w & 0xFFFF) | (codeTemp.value.u >> 16 & 0xFF0000) | codeTemp.optionbits << 24;
            return true;
        case OPI_OT:  // constant of same type as operand type
            if ((uint8_t(code.dtype) & ~TYP_UNS) <= uint8_t(TYP_INT32) && code.formatp->immSize >= 4) return true;
        }
        // check if size fits. general cases
        switch (code.formatp->immSize) {
        case 1:
            if (codeTemp.fitNum & IFIT_I8) break;  // fits
            if ((variant & VARIANT_U0) && (codeTemp.fitNum & IFIT_U8)) break; // unsigned fits
            if ((codeTemp.dtype & 0x1F) == (TYP_INT8 & 0x1F) && (codeTemp.fitNum & IFIT_U8)) break;  // 8 bit size fits unsigned with no sign extension
            return false;
        case 2:
            if (codeTemp.fitNum & (IFIT_I16 | FFIT_16)) break;  // fits
            if ((variant & VARIANT_U0) && (codeTemp.fitNum & IFIT_U16)) break; // unsigned fits
            if ((codeTemp.dtype & 0x1F) == (TYP_INT16 & 0x1F) && code.formatp->tmplate != 0xC && (codeTemp.fitNum & IFIT_U16)) break;  // 16 bit size fits unsigned with no sign extension
            if ((code.formatp->imm2 & 4) && !(variant & VARIANT_On) && (codeTemp.fitNum & IFIT_I16SHIFT)) {
                // fits with im4 << im5
                shiftCount = bitScanForward(codeTemp.value.u);
                codeTemp.value.u >>= shiftCount;
                codeTemp.optionbits = shiftCount;
                break;
            }
            if (variant & VARIANT_H0) break; // half precision fits
            return false;
        case 4:
            if (code.formatp->imm2 & 8) {   // 32 bits with shift
                if (codeTemp.fitNum & IFIT_I32SHIFT) { // fits IM7 << IM4
                    shiftCount = bitScanForward(codeTemp.value.u);
                    if (codeTemp.fitNum & IFIT_I32) shiftCount = 0; // don't use shift if it fits without
                    codeTemp.value.u = ((codeTemp.value.u >> shiftCount) & 0xFFFFFFFF) | ((uint64_t)shiftCount << 32); // store shift count in upper half
                    break;
                }
            }
            else {  // set shift count to zero
                codeTemp.value.u &= 0xFFFFFFFF;
            }
            if ((code.dtype & 0xFF) == (TYP_FLOAT32 & 0xFF))  break;  // float32 must be rounded to fit
            if (codeTemp.fitNum & (IFIT_I32 | FFIT_32)) break;  // fits
            if ((codeTemp.fitNum & IFIT_U32) && (code.dtype & 0xFF) == (TYP_INT32 & 0xFF)) break;  // fits
            if ((variant & VARIANT_U0) && (codeTemp.fitNum & IFIT_U32)) break; // unsigned fits
            if (variant & VARIANT_H0) break; // half precision fits
            if ((codeTemp.dtype & 0x1F) == (TYP_INT32 & 0x1F) && (codeTemp.fitNum & IFIT_U32)) break;  // 32 bit size fits unsigned with no sign extension
            return false;
        case 8:
            break;            
        default:; // other values should not occur in table
        }
    }
    else if ((code.formatp->immSize != 0) && !(code.etype & (XPR_OFFSET | XPR_LIMIT)) 
        && instructionlistId[ii].sourceoperands && code.category != 1) {
        return false;  // immediate operand provided but not required
    }
    return true;
}

// check if instruction fits into specified format
bool CAssembler::jumpInstructionFits(SCode const & code, SCode & codeTemp, uint32_t ii) {
    // code: structure defining all operands and options
    // codeTemp: fitted code
    // ii: index into instructionlistId
    // formatIndex: index into formatList4

    //uint8_t offsetSize = 0;              // number of bytes to use in relative address
    //uint8_t immediateSize = 0;           // number of bytes to use in immediate operand
    bool offsetRelocated = false;        // relative offset needs relocation
    //bool immediateRelocated = false;     // immediate operand needs relocation

    codeTemp = code;
    codeTemp.category = code.formatp->category;
    codeTemp.size = (code.formatp->format2 >> 8) & 3;
    codeTemp.instr1 = ii;

    // check vector use
    bool useVectors = (code.dtype & TYP_FLOAT) || (code.dest & 0xE0) == REG_V || (code.reg1 & 0xE0) == REG_V;
    if (useVectors) {
        if (!(code.formatp->vect)) return false;  // vectors not supported
    }

    // operand type provided by this format
    uint32_t formatOT = code.formatp->ot;
    if (formatOT == 0) formatOT = code.dtype;  // operand type determined by OT field
    formatOT &= 7;

    // check requested operand type
    uint32_t requestOT = code.dtype & 7;
    if (formatOT <= 3 && requestOT < formatOT && (code.dtype & TYP_PLUS)) {
        requestOT = formatOT;  // request allows bigger type
        codeTemp.dtype = formatOT;
    }
    if (requestOT != formatOT && code.dtype) return false;  // requested format type not supported

    // check if operand type supported by instruction
    uint32_t optypessupport = useVectors ? (instructionlistId[ii].optypesscalar | instructionlistId[ii].optypesvector) : instructionlistId[ii].optypesgp;
    optypessupport |= optypessupport >> 8;  // include types with optional support
    if (!(optypessupport & (1 << requestOT))) return false;

    // check if there are enough register operands in this format
    uint8_t opAvail = code.formatp->opAvail;
    uint8_t numReg = ((opAvail >> 4) & 1) + ((opAvail >> 5) & 1) + ((opAvail >> 7) & 1); // number of registers available
    uint8_t numReq = instructionlistId[ii].sourceoperands;  // number of registers required for this instruction
    if ((code.etype & XPR_REG1) && code.dest && code.reg1 != code.dest && numReq > 2) {
        numReq++;     // reg1 != destination, except if no reg2
    }
    if (code.formatp->jumpSize) numReq--;
    if ((code.etype & (XPR_IMMEDIATE | XPR_MEM)) && numReq) numReq--;
    if ((code.etype & XPR_INT2) && numReq) numReq--;
    if (numReq > numReg) return false;  // not enough registers in this format

    // check if correct number of registers specified
    uint8_t nReg = 0;
    for (int j = 0; j < 3; j++) nReg += (code.etype & (XPR_REG1 << j)) != 0;
    if (code.dest && code.dest != code.reg1) nReg++;
    if (nReg != numReq) return false;

    // check if mask available
    if ((code.etype & XPR_MASK) && !(fInstr->tmplate == 0xA || fInstr->tmplate == 0xE)) return false;

    // self-relative jump offset
    if (code.etype & XPR_JUMPOS) {
        if (!(code.formatp->jumpSize)) return false;
        switch (code.formatp->jumpSize) {
        case 0:  // no offset
            if (code.offset_jump || offsetRelocated) return false;
            break;
        case 1:  // 1 byte
            if (!(code.fitJump & IFIT_I8)) return false;
            break;
        case 2:  // 2 bytes
            if (!(code.fitJump & IFIT_I16)) return false;
            break;
        case 3:  // 3 bytes
            if (!(code.fitJump & IFIT_I24)) return false;
            break;
        case 4:  // 4 bytes
            if (!(code.fitJump & IFIT_I32)) return false;
            break;
        }
    }
    else { // no self-relative jump offset
        if (code.formatp->jumpSize) return false;
    }

    if (instructionlistId[ii].opimmediate == OPI_IMPLICIT && !(code.etype & XPR_IMMEDIATE)) {
        // There is no immediate operand. instructionlistId[ii] has an implicit immediate operand.
        // Insert implicit operand and see if it fits
        codeTemp.value.u = instructionlistId[ii].implicit_imm;
        codeTemp.etype |= XPR_INT;
        codeTemp.fitNum = 0xFFFFFFFF;
    }

    // immediate operand
    if (codeTemp.etype & XPR_IMMEDIATE) {
        if (code.dtype & TYP_FLOAT) {
            if (variant & VARIANT_I2) {
                // immediate should be integer
                codeTemp.etype = (code.etype & ~XPR_FLT) | XPR_INT;
                codeTemp.value.i = (int64_t)code.value.d;
                switch (code.formatp->immSize) {
                case 0:  // no immediate
                    return false;
                case 1:  // 1 byte
                    if (codeTemp.value.i < -0x80 || codeTemp.value.i > 0x7F) return false;
                    break;
                case 2:  // 2 bytes
                    if (codeTemp.value.i < -0x8000 || codeTemp.value.i > 0x7FFF) return false;
                    break;
                case 4:  // 4 bytes
                    if (-codeTemp.value.i > 0x80000000u || codeTemp.value.i > 0x7FFFFFFF) return false;
                    break;
                }
            }
            else {
                // immediate is floating point or small integer converted to floating point
                int fit = code.fitNum;
                if ((code.dtype & 0xFF) <= (TYP_FLOAT32 & 0xFF)) fit |= FFIT_32;
                switch (code.formatp->immSize) {
                case 0:  // no immediate
                    return false;
                case 1:  // 1 byte
                    if (!(fit & IFIT_I8)) return false;
                    break;
                case 2:  // 2 bytes
                    if (!(fit & FFIT_16)) return false;
                    break;
                case 4:  // 4 bytes
                    if (!(fit & FFIT_32)) return false;
                    break;
                case 8:  // 8 bytes., currently not supported
                    ;
                }
            }
        }
        else {
            // immediate integer operand
            switch (code.formatp->immSize) {
            case 0:  // no immediate
                return false;
            case 1:
                if (codeTemp.fitNum & IFIT_I8) break;  // fits
                if ((codeTemp.dtype & 0x1F) == (TYP_INT8 & 0x1F) && (codeTemp.fitNum & IFIT_U8)) break;  // 8 bit size fits unsigned with no sign extension
                return false;
            case 2:  // 2 bytes
                if (instructionlistId[ii].opimmediate == OPI_INT1632) { // 16+32 bits
                    if ((codeTemp.value.u >> 32) <= 0xFFFF) break;
                    return false;
                }
                if (codeTemp.fitNum & IFIT_I16) break;  // fits
                if ((codeTemp.dtype & 0x1F) == (TYP_INT16 & 0x1F) && (codeTemp.fitNum & IFIT_U16)) break;  // 16 bit size fits unsigned with no sign extension
                return false;
            case 4:  // 4 bytes
                if (instructionlistId[ii].opimmediate == OPI_2INT16) { // 16+16 bits
                    if (codeTemp.value.w <= 0xFFFF && (codeTemp.value.u >> 32) <= 0xFFFF) break;
                    return false;
                }
                if (codeTemp.fitNum & IFIT_I32) break;  // fits
                if ((codeTemp.dtype & 0x1F) == (TYP_INT32 & 0x1F) && (codeTemp.fitNum & IFIT_U32)) break;  // 32 bit size fits unsigned with no sign extension
                return false;
            case 8:  // 8 bytes
                break;
            default:  // does not fit other sizes
                return false;
            }
        }
    }
    else {
        // no explicit immediate
        if (code.formatp->immSize && code.instruction != II_JUMP && code.instruction != II_CALL) return false;
    }

    // memory operand
    if (code.etype & XPR_MEM) {
        if (code.formatp->mem == 0) return false;  // memory operand requested but not supported
        uint32_t scale2 = formatOT;
        if (scale2 > 4) scale2 -= 3;  // operand size = 1 << scale2
        if (code.etype & XPR_SYM1) {  // has data symbol
            if (code.etype & XPR_SYM2) {  // has difference between two symbols
                codeTemp.sizeUnknown = 1;
            }
            if (!(code.fitAddr & IFIT_I32)) return false;  // assume symbol address requires 32 bits. local symbol difference resolved later when sizeUnknown = 1
        }
        // check index and scale factor
        if (code.etype & XPR_INDEX) {
            if (!(code.formatp->mem & 4)) return false;  // index not supported
        }
        else {  // no index requested
            if (code.formatp->mem & 4) {
                codeTemp.index = 0x1F;  // RT = 0x1F means no index
                codeTemp.scale = 1 << scale2;
            }
        }

        // check address offset size
        if (code.etype & XPR_OFFSET) {
            if (!(code.formatp->mem & 0x10)) return false;  // format does not support memory offset
            switch (code.formatp->addrSize) {
            case 1:  // scale factor depends on operand size
                if (code.offset_mem & ((1 << scale2) - 1)) return false;  // offset is not a multiple of the scale factor
                if ((code.offset_mem >> scale2) < -0x80 || (code.offset_mem >> scale2) > 0x7F) return false;
                break;
            case 2:
                if (!(code.fitAddr & IFIT_I16)) return false;
                break;
            case 4:
                if (!(code.fitAddr & IFIT_I32)) return false;
                break;
            default:
                return false;
            }
        }
    }
    else if (code.formatp->mem) return false;  // memory operand supported by not requested

    return true;
}


// Check code for correctness before fitting a format, and fix some code details
void CAssembler::checkCode1(SCode & code) {

    // check code for correctness
    if (code.etype & XPR_MEM) {
        // check memory operand
        bool useVectors = (code.dtype & TYP_FLOAT) != 0 || (code.dest & 0xE0) == REG_V || (code.reg1 & 0xE0) == REG_V;
        if (useVectors && code.scale == -1) {
            code.etype |= XPR_LENGTH;  code.length = code.index;  // index register is also length
        }
        int numOpt = ((code.etype & XPR_SCALAR) != 0) + ((code.etype & XPR_LENGTH) != 0) + ((code.etype & XPR_BROADC) != 0);
        if (numOpt > 1) {errors.reportLine(ERR_CONFLICT_OPTIONS);  return;}  // conflicting options
        if (numOpt && !useVectors && !(code.etype & XPR_SCALAR)) {errors.reportLine(ERR_VECTOR_OPTION);  return;}  // vector option on non-vector operands

        if (code.etype & XPR_INDEX) {
            // check scale factor
            const int dataSizeTable[8] = {1, 2, 4, 8, 16, 4, 8, 16}; // data size for each operant type
            int8_t scale = code.scale;
            if (scale != 1 && scale != -1 && scale != dataSizeTable[code.dtype & 7]) errors.reportLine(ERR_SCALE_FACTOR);
            if (code.scale == -1 && code.length && code.length != code.index) {
                errors.reportLine(ERR_NEG_INDEX_LENGTH);  return;
            }
        }
        if (!(code.etype & XPR_BASE)) {
            // no base pointer. check if there is a symbol with an implicit base pointer
            int32_t symi1 = 0;
            if (code.etype & XPR_SYM1) symi1 = findSymbol(code.sym1);
            if ((code.etype & XPR_SYM2) || symi1 < 1 || !(symbols[symi1].st_other & STV_SECT_ATTR)) {
                errors.reportLine(ERR_NO_BASE);
            }
        }
    }
    // check mask
    if ((code.etype & XPR_MASK) && (code.mask & 0x1F) > 6) errors.reportLine(ERR_MASK_REGISTER);

    // check fallback
    if (code.etype & XPR_MASK) {
        if (code.fallback == 0) code.fallback = code.reg1 ? code.reg1 : 0x1F;  // default fallback is reg1, or 0 if no reg1
        if ((code.fallback & 0xE0) == 0) code.fallback |= code.dest & 0xE0;    // get type of dest if fallback has no type
    }

    // details for unsigned variants
    if (code.dtype & TYP_UNS) {  // an unsigned type is specified  
        switch (code.instruction) {
        case II_DIV:  case II_DIV_REV:
        case II_DIV_EX:  
        case II_MUL_HI:  case II_MUL_EX: 
        case II_REM:  case II_SHIFT_RIGHT_S:  
            code.instruction |= 1;  // change to unsigned version
            break;
        default:;  // other instructions: do nothing
        }
    }

    // handle half precision
    if (uint8_t(code.dtype) == uint8_t(TYP_FLOAT16)) {
        switch (code.instruction) {
        case II_MUL_ADD: case II_DIV: case II_MAX: case II_MIN:
            code.optionbits |= 0x20;                   // add option bit 5
            code.etype |= XPR_OPTIONS;
            break;
        case II_ADD: case II_MUL: case II_COMPARE:
            code.instruction |= II_ADD_H & 0xFF000;    // change to half precision instruction code
            break;
        case II_SUB: 
            if ((code.etype & XPR_IMMEDIATE) && !(code.etype & (XPR_MEM | XPR_REG2))) {
                code.instruction = II_ADD_H; code.value.d = - code.value.d; // subtract constant changed to add -constant
            }
            else code.instruction = II_SUB_H;
            break;
        case II_SUB_REV:
            if (code.value.i == 0) {   // -x
                code.instruction = II_TOGGLE_BIT;
                code.value.u = 15;
            }
            else errors.reportLine(ERR_WRONG_OPERANDS);
            break;
        case II_MOVE: case II_REPLACE: case II_REPLACE_EVEN: case II_REPLACE_ODD:
            if (code.etype & XPR_INT) {   // convert integer to float16
                if (abs(code.value.i) > 65504) errors.reportLine(ERR_OVERFLOW);
                code.value.u = double2half(double(code.value.i));
            }
            else if (code.etype & XPR_FLT) {  // convert double to float16
                if (code.value.d > 65504. || code.value.d < -65504.) errors.reportLine(ERR_OVERFLOW);
                code.value.u = double2half(code.value.d);
                code.etype = (code.etype & ~ XPR_IMMEDIATE) | XPR_INT;
            }
            if (code.instruction == II_SUB_H && (code.etype & XPR_IMMEDIATE)) {
                code.value.w ^= 0x8000;
                code.instruction &= ~1;  // convert sub_h constant to add_h -constant
            }
            code.dtype = TYP_INT16;
            code.fitNum = IFIT_I16 | IFIT_I32;
            break;
        case II_STORE:
            if (code.etype & XPR_INT) code.value.u = double2half(double(code.value.i));
            else code.value.u = double2half(code.value.d);
            code.dtype = TYP_INT16;
            code.etype = (code.etype & ~ XPR_FLT) | XPR_INT;
            break;
        case II_ADD_H: case II_SUB_H: case II_MUL_H: case II_DIV_H: case II_SQRT:
        case II_FLOAT2INT: case II_INT2FLOAT:
        case II_COMPARE_H: case II_FP_CATEGORY: case II_FP_CATEGORY_REDUCE:
            break;
        default:
            // no other instructions support half precision
            errors.reportLine(ERR_WRONG_OPERANDS);
        }
    }

    // special case instructions 
    switch (code.instruction) {
    case II_STORE:
        if ((code.dtype & TYP_FLOAT) && (code.etype & XPR_FLT) && !(code.reg1)) {
            // store float constant
          //  code.dtype = code.dtype + (TYP_INT32 - TYP_FLOAT32) | TYP_UNS;
        }
    }

    // check size needed for immediate operand and address
    fitConstant(code);
    fitAddress(code);

    if (code.instruction & II_JUMP_INSTR) {
        // jump instruction 
        code.category = 4;
        // check register type
        if (code.dtype && code.reg1) {
            if ((code.dtype & 0xFF) <= (TYP_FLOAT16 & 0xFF)) { // must use g.p. registers
                if (code.reg1 & REG_V) errors.reportLine(ERR_WRONG_REG_TYPE);
            }
            else {  // must use vector registers
                if (code.reg1 & REG_R) errors.reportLine(ERR_WRONG_REG_TYPE);
            }
        }
        // check if immediate operand too big
        if (code.etype & XPR_IMMEDIATE) {
            if (code.dtype & TYP_FLOAT) {
                if ((code.dtype & 0xFF) >= (TYP_FLOAT64 & 0xFF) && !(code.fitNum & FFIT_32)) errors.reportLine(ERR_TOO_LARGE_FOR_JUMP);
            }
            else if (code.dtype & TYP_UNS) {
                if ((code.dtype & 0x1F) >= (TYP_INT64 & 0x1F) && !(code.fitNum & IFIT_U32)) errors.reportLine(ERR_TOO_LARGE_FOR_JUMP);
            }
            else if ((code.dtype & 0x1F) >= (TYP_INT64 & 0x1F) && !(code.fitNum & IFIT_I32)) errors.reportLine(ERR_TOO_LARGE_FOR_JUMP);
        }
    }

    // optimize instruction
    if (cmd.optiLevel) optimizeCode(code);
}


// Check register types etc. after fitting a format, and finish code details
void CAssembler::checkCode2(SCode & code) {
    if (code.instruction >= II_ALIGN) return;  // not an instruction

    // check type
    if (code.dtype == 0) {
        if ((code.etype & (XPR_INT | XPR_FLT | XPR_REG | XPR_REG1 | XPR_MEM)) && !(variant & (VARIANT_D0 | VARIANT_D2))) { // type not specified        
            if (code.instruction == II_MOVE && code.category == 3 && !(code.etype & (XPR_IMMEDIATE | XPR_MEM))) {
                // register-to-register move. find appropriate operand type
                code.dtype = TYP_INT64;        // g.p. register. copy whole register ??
                if (code.dest & REG_V) code.dtype = TYP_INT8;  // vector register. length must be divisible by tpe
            }
            else {
                errors.reportLine(ERR_TYPE_MISSING);       // type must be specified
                return;
            }
        }
    } 

    if (code.etype & XPR_MEM) {
        // check memory operand
        if (variant & VARIANT_M0) { // memory destination
            if (code.etype & XPR_BROADC) {
                errors.reportLine(ERR_DEST_BROADCAST); return;
            }
        }
        if (code.base >= REG_R + 28 && code.base <= REG_R + 30 && (code.formatp->addrSize) > 1 && pass < 4) {
            // cannot use r28 - r30 as base pointer with more than 8 bits offset
            // (we don't get an error message here for a symbol address because the base pointer has not been assigned yet)
            errors.reportLine(ERR_R28_30_BASE);
        }
        // check M1 option
        /*if (variant & VARIANT_M1) {
            if (code.formatp->tmplate == 0xE && (code.etype & XPR_MEM) && (code.etype & XPR_INT)
                && (code.value.i > 63 || code.value.i < -63)) {
                errors.reportLine(ERR_CONSTANT_TOO_LARGE);  return;
            }
            if (code.optionbits && (code.etype & XPR_MEM)) {
                errors.reportLine(ERR_BOTH_MEM_AND_OPTIONS);  return;
            }
        }*/
    }

    if (lineError) return;  // skip additional errors

    // Make list of operands from available operands. 0=none, 1=immediate, 2=memory, 5=RT, 6=RS, 7=RU, 8=RD
    uint8_t opAvail = code.formatp->opAvail;    // Bit index of available operands
    int j;                                        // loop counter

    // check if correct number of registers
    uint32_t numReq = instructionlistId[code.instr1].sourceoperands;  // number of registers required for this instruction
    if (code.category == 4 && (code.instruction & II_JUMP_INSTR) && (code.etype & XPR_JUMPOS) && numReq) numReq--;
    if ((code.etype & XPR_IMMEDIATE) && numReq) numReq--;
    if ((code.etype & XPR_INT2) && numReq) numReq--;
    if ((code.etype & XPR_MEM) && !(variant & VARIANT_M0) && numReq) numReq--;

    uint32_t nReg = 0;
    for (j = 0; j < 3; j++) nReg += (code.etype & (XPR_REG1 << j)) != 0;
    if (nReg < numReq && !(variant & VARIANT_D3)) 
        errors.reportLine(ERR_TOO_FEW_OPERANDS);
    else if (nReg > numReq && instructionlistId[code.instr1].opimmediate != 25) {
        errors.reportLine(ERR_TOO_MANY_OPERANDS);
    }

    // count number of available registers in format
    uint32_t regAvail = 0;                   
    opAvail >>= 4;                            // register operands
    while (opAvail) {
        regAvail += opAvail & 1;
        opAvail >>= 1;
    }

    // expected register types
    uint8_t regType = REG_R;  
    if ((code.formatp->vect & 1) || ((code.formatp->vect & 0x10) && (code.dtype & 4))) regType = REG_V;

    // check each of up to three source registers
    for (j = 0; j < 3; j++) {
        if (code.etype & (XPR_REG1 << j)) {  // register j used
            if (variant & VARIANT_SPECS) {    // must be special register
                if (((&code.reg1)[j] & 0xE0) <= REG_V) errors.reportLine(ERR_WRONG_REG_TYPE);
            }
            else if ((variant & (VARIANT_R1 << j)) 
                || ((variant & VARIANT_RL) && (j == 2 || (&code.reg1)[j+1] == 0))) {
                if (((&code.reg1)[j] & 0xE0) != REG_R) {  // this operand must be general purpose register
                    errors.reportLine(ERR_WRONG_REG_TYPE);
                }
            }
            else if (((&code.reg1)[j] & 0xE0) != regType) {  // wrong register type
                errors.reportLine(ERR_WRONG_REG_TYPE);
            }
        }
        if (lineError) return;  // skip additional errors
    }
    // check destination register
    if (code.dest) {
        if (variant & VARIANT_SPECD) {    // must be special register
            if ((code.dest & 0xE0) <= REG_V) errors.reportLine(ERR_WRONG_REG_TYPE);
        }
        else if (variant & VARIANT_R0) {
            if ((code.dest & 0xE0) != REG_R) {  // destination must be general purpose register
                errors.reportLine(ERR_WRONG_REG_TYPE);
            }
        }
        else if ((code.dest & 0xE0) != regType && code.dest != 2) {  // wrong register type
            errors.reportLine(ERR_WRONG_REG_TYPE);
        }
        else if ((code.dest == 2) ^ ((variant & VARIANT_M0) != 0)) {  // operands in wrong order
            errors.reportLine(ERR_OPERANDS_WRONG_ORDER);
        }

        if (lineError) return;  // skip additional errors
    }
    if ((variant & (VARIANT_D0 | VARIANT_D1 | VARIANT_D2)) != 0 && code.dest != 0) {  // should not have destination        
        errors.reportLine(ERR_NO_DESTINATION);
    }
    if ((variant & (VARIANT_D0 | VARIANT_D1)) == 0 && code.dest == 0) {  // should have destination
        errors.reportLine(ERR_MISSING_DESTINATION);
    }

    // check mask register
    if ((code.etype & XPR_FALLBACK) && !(code.etype & XPR_MASK)) {   // fallback but no mask
        code.mask = 7;                                               // no mask
    }
    if ((code.etype & (XPR_MASK | XPR_FALLBACK)) && (code.mask & 7) != 7) {  // mask used
        if ((code.mask & 0xE0) != regType) {  // wrong type for mask register
            errors.reportLine(ERR_WRONG_REG_TYPE);
        }
        else if ((code.fallback & 0xE0) != regType && (code.fallback & 0x1F) != 0x1F) {  // wrong type for fallback registser
            if ((variant & VARIANT_RL) && code.fallback == code.reg1) {
                // fallback has been assigned to reg1 in CAssembler::checkCode1, but reg1 is g.p. register
                code.fallback = 0x5F;
            }
            else errors.reportLine(ERR_WRONG_REG_TYPE);
        }
        if ((code.etype & XPR_FALLBACK) && (variant & VARIANT_F0)) {  // cannot have fallback register
            errors.reportLine(ERR_CANNOT_HAVEFALLBACK1);
        }
        // check if fallback is the right register
        if (code.etype & XPR_FALLBACK) {
            if (code.numOp >= 3 && code.fallback != code.reg1) {
                errors.reportLine(ERR_3OP_AND_FALLBACK);
            }
        }        
    }

    // check scale factor
    const int dataSizeTable[8] = { 1, 2, 4, 8, 16, 4, 8, 16 }; // data size for each operant type
    int8_t scale = code.scale;
    if (scale == 0) scale = 1;
    if (((code.formatp->scale & 4) && scale != -1)     // scale must be -1
        || (((code.formatp->scale & 6) == 2) && scale != dataSizeTable[code.dtype & 7]) // scale must match operand type
        || (((code.formatp->scale & 6) == 0 && scale != 1 && (code.index & 0x1F) != 0x1F))) {                          // scale must be 1
        errors.reportLine(ERR_SCALE_FACTOR);
    }
    // check vector length
    int numOpt = ((code.etype & XPR_SCALAR) != 0) + ((code.etype & XPR_LENGTH) != 0) + ((code.etype & XPR_BROADC) != 0);
    if (numOpt == 0 && (code.etype & XPR_MEM) && (code.formatp->vect & ~0x10) && !(code.etype & XPR_LIMIT) && !(code.formatp->vect & 0x80)) {
        errors.reportLine(ERR_LENGTH_OPTION_MISS);  return;  // missing length option
    }

    // check immediate type
    if ((code.etype & XPR_FLT) && (variant & VARIANT_I2)) {
        // immediate should be integer
        code.etype = (code.etype & ~XPR_FLT) | XPR_INT;
        //code.value.i = (int64_t)code.value.d;
        code.value.i = value0;
        // reinterpret constant as integer
        fitConstant(code);
    }
    if ((code.etype & XPR_INT) && !(code.etype & (XPR_LIMIT | XPR_INT2))) {
        // check if value fits specified operand type
        int ok = 1;
        switch (code.dtype & 0x1F) {
        case TYP_INT8 & 0x1F:
            ok = code.fitNum & (IFIT_I8 | IFIT_U8);  break;
        case TYP_INT16 & 0x1F:
            ok = code.fitNum & (IFIT_I16 | IFIT_U16);  break;
        case TYP_INT32 & 0x1F:
            ok = code.fitNum & (IFIT_I32 | IFIT_U32);  break;
        case TYP_INT64 & 0x1F:
            break;
        }
        if (!ok && (instructionlistId[code.instr1].opimmediate & ~0x10) != OPI_INT32) {
            errors.reportLine(ERR_CONSTANT_TOO_LARGE);
        }
    }

    // check options
    if ((code.etype & XPR_OPTIONS) && !(variant & VARIANT_On) && code.formatp->category != 4) {
        errors.reportLine(ERR_CANNOT_HAVE_OPTION);
    }

    // details for unsigned compare
    if (code.dtype & TYP_UNS) {  // an unsigned type is specified  
        if ((variant & VARIANT_U3) && code.optionbits && code.instruction == II_COMPARE) {
            code.optionbits |= 8;  // compare needs unsigned bit only if there is at least one other option bit
            code.etype |= XPR_OPTIONS;
        }
    }

    if (section) code.section = section;  // insert section
}


// find reason why no format fits, and return error number
uint32_t CAssembler::checkCodeE(SCode & code) {
    // check fallback
    if ((code.etype & XPR_FALLBACK) && code.fallback != code.dest) {
        if (((code.etype & XPR_MEM) && (code.dest & REG_V)) || code.index) return ERR_CANNOT_HAVEFALLBACK2;
        if (instructionlistId[code.instr1].sourceoperands >= 3) return ERR_3OP_AND_FALLBACK;
    }
    // check three-operand instructions
    if (instructionlistId[code.instr1].sourceoperands >= 3 && code.reg1 != code.dest && (code.etype & XPR_MEM) && ((code.dest & REG_V) || code.index)) {
        return ERR_3OP_AND_MEM;
    }
    return ERR_NO_INSTRUCTION_FIT;  // any other reason
}


// optimize instruction. replace by more efficient instruction if possible
void CAssembler::optimizeCode(SCode & code) {

    // is it a vector instruction?
    bool hasVector = ((code.dest | code.reg1) & REG_V) != 0;

    // is it a floating point instruction?
    bool isFloat = (code.dtype & TYP_FLOAT) != 0;

    if (code.instruction & II_JUMP_INSTR) {
        // jump instruction
        // optimize immediate jump offset operand
        if ((code.instruction & 0xFF) == II_SUB && (code.etype & XPR_IMMEDIATE) == XPR_INT 
            && code.value.i >= -0x7F && code.value.i <= 0x80  && cmd.optiLevel 
            && ((code.dtype & 0xFF) == (TYP_INT32 & 0xFF) || ((code.dtype & 0xFF) <= (TYP_INT32 & 0xFF) && (code.dtype & TYP_PLUS)))) {
            // subtract with conditional jump with 8-bit immediate and 8-bit address 
            // should be replaced by addition of the negative constant
            int32_t isym = 0;
            if (code.etype & XPR_SYM1) isym = findSymbol(code.sym1);
            if (isym <= 0 || symbols[isym].st_section == section || code_size <= (1 << 9)) {
                // we are not sure yet, but chances are good that the address fits an 8-bit field. Replace sub by add
                code.value.i = -code.value.i;       // change sign of immediate constant
                code.instruction ^= (II_SUB ^ II_ADD);  // replace sub with add
                if ((code.instruction & 0xFFFF00) == II_JUMP_CARRY) code.instruction ^= 0x100;  // carry condition is inverted
            }
        }
        if ((code.fitNum & (IFIT_J16 | IFIT_J32) && (code.etype & XPR_IMMEDIATE) == XPR_INT && (code.instruction & 0xFE) == II_ADD)) {
            // replace add with sub or vice versa
            code.value.i = -code.value.i;       // change sign of immediate constant
            code.instruction ^= (II_SUB ^ II_ADD);
            if ((code.instruction & 0xFFFF00) == II_JUMP_CARRY) code.instruction ^= 0x100;  // carry condition is inverted
            code.fitNum |= (code.fitNum & IFIT_J) >> 1;  // signal that it fits
        }
    }
    else { // other instruction. optimize immediate operand
        if ((code.etype & XPR_INT) /* && !(code.etype & (XPR_OFFSET | XPR_LIMIT | XPR_SYM1))*/ ) {
            if ((code.instruction & 0xFFFFFFFE) == II_ADD && (code.fitNum & IFIT_J8) != 0) {
                // we can make the instruction smaller by changing the sign of the constant and exchange add and sub 
                // (we don't have to do this for 0x8000 and 0x80000000 because the can be fitted as 1 << x)
                code.instruction ^= (II_ADD ^ II_SUB);  // replace add with sub or vice versa
                code.value.i = -code.value.i;       // change sign of immediate constant
                code.fitNum |= (code.fitNum & IFIT_J) >> 1;  // signal that it fits
            }
            else if (code.instruction == II_SUB && (code.fitNum & (IFIT_I16SH16 | IFIT_I16)) && !(code.fitNum & IFIT_I8)
                && code.value.w != 0x80000000U && code.value.w != 0xFFFF8000U && code.dest == code.reg1 && !hasVector
                && (((uint8_t)code.dtype == (uint8_t)TYP_INT32) || (((uint8_t)code.dtype < (uint8_t)TYP_INT32) && (code.dtype & TYP_PLUS)))) {
                code.instruction = II_ADD;          // replace sub with add
                code.value.i = -code.value.i;       // change sign of immediate constant
            }
            else if (code.instruction == II_SUB && (code.fitNum & IFIT_I8SHIFT) && !(code.fitNum & IFIT_I8) && !isFloat
                && code.dest == code.reg1 
                && (((uint8_t)code.dtype >= (uint8_t)TYP_INT32) || (code.dtype & TYP_PLUS))) {
                code.instruction = II_ADD;          // replace sub with add
                code.value.i = -code.value.i;       // change sign of immediate constant
                code.fitNum &= ~(IFIT_I16 | IFIT_I16SH16 | IFIT_I32SH32);
            }
            else if (code.instruction == II_SUB && (code.fitNum & IFIT_I32SH32) && !(code.fitNum & (IFIT_I16SHIFT | IFIT_I32))                
                && (((uint8_t)code.dtype == (uint8_t)TYP_INT64) || (code.dtype & TYP_PLUS)) && !isFloat) {
                code.instruction = II_ADD;          // replace sub with add
                code.value.i = -code.value.i;       // change sign of immediate constant
            }
            else if ((code.instruction == II_MOVE || code.instruction == II_AND) 
                && (code.fitNum & IFIT_U32) && !(code.fitNum & (IFIT_I32 | IFIT_I16SHIFT))                
                && ((uint8_t)code.dtype == (uint8_t)TYP_INT64) && !hasVector) {
                code.dtype = TYP_INT32;             // changing type to int32 will zero extend
            }
            /*else if (code.instruction == II_MOVE 
                && (code.fitNum & IFIT_U16) && !(code.fitNum & IFIT_I16)
                && ((uint8_t)code.dtype >= (uint8_t)TYP_INT32) && !hasVector
                && !(code.etype & (XPR_REG | XPR_MEM | XPR_OPTION | XPR_SYM1))) {
                    code.instruction = II_MOVE_U;
                    code.dtype = TYP_INT64;
            } */
            else if (code.instruction == II_OR && (code.value.u & (code.value.u-1)) == 0 && !(code.fitNum & IFIT_I8)) {
                code.instruction = II_SET_BIT;                  // OR with a power of 2
                code.value.u = bitScanReverse(code.value.u);
                code.fitNum = IFIT_I8 | IFIT_I16 | IFIT_I32;
            }
            else if (code.instruction == II_AND && (~code.value.u & (~code.value.u-1)) == 0 && !(code.fitNum & IFIT_I8)) {
                code.instruction = II_CLEAR_BIT;                // AND with ~(a power of 2)
                code.value.u = bitScanReverse(~code.value.u);
                code.fitNum = IFIT_I8 | IFIT_I16 | IFIT_I32;
            }
            else if (code.instruction == II_XOR && (code.value.u & (code.value.u-1)) == 0 && !(code.fitNum & IFIT_I8)) {
                code.instruction = II_TOGGLE_BIT;               // XOR with a power of 2
                code.value.u = bitScanReverse(code.value.u);
                code.fitNum = IFIT_I8 | IFIT_I16 | IFIT_I32;
            }
        }
        if ((code.etype & XPR_FLT) && !(code.etype & (XPR_OFFSET | XPR_LIMIT | XPR_SYM1))) {
            if (code.instruction == II_SUB && (code.fitNum & FFIT_16) && (uint8_t)code.dtype >= (uint8_t)TYP_FLOAT16) {
                code.instruction = II_ADD;          // replace sub with add
                code.value.d = -code.value.d;       // change sign of immediate constant
            }
        }
    }

    // optimize -float as toggle_bit
    if (code.instruction == II_SUB_REV && (code.etype & XPR_IMMEDIATE) && (code.dtype & TYP_FLOAT) 
    && code.value.i == 0 && (code.etype & XPR_REG1) && !(code.etype & XPR_REG2)) {
        // code is -v represented as 0-v. replace by flipping bit
        uint32_t bits = 1 << (code.dtype & 7);  // number of bits in floating point type
        code.instruction = II_TOGGLE_BIT;
        code.value.u = bits - 1;
        code.etype = ((code.etype & ~XPR_IMMEDIATE) | XPR_INT);
    }

    // optimize multiply and divide instructions
    if ((code.instruction == II_MUL || code.instruction == II_DIV) && (code.etype & XPR_IMMEDIATE)) {
        if (code.dtype & TYP_INT) { // integer multiplication
            // check if constant is positive and a power of 2
            if (code.value.i <= 0 || (code.value.u & (code.value.u - 1))) return;
            if (code.instruction == II_MUL) {
                // integer multiplication by power of 2. replace by left shift
                code.instruction = II_SHIFT_LEFT;
                code.value.u = bitScanReverse(code.value.u);
            }
            else if (code.dtype & TYP_UNS) {
                // unsigned division by power of 2. replace by right shift
                // We are not optimizing signed division because this requires multiple instructions and registers
                code.instruction = II_SHIFT_RIGHT_U;
                code.value.u = bitScanReverse(code.value.u);
            }
        }
        else if (code.dtype & TYP_FLOAT) {
            // floating point multiplication or division
            // check if constant is a power of 2
            int shiftCount = 0xFFFFFFFF;          // shift count to replace multiplication by power of 2
            if ((code.etype & XPR_INT) && code.value.i > 0 && (code.value.u & (code.value.u-1)) == 0) {
                // positive integer power of 2
                shiftCount = bitScanReverse(code.value.u);
                if (code.instruction == II_DIV) shiftCount = -shiftCount;
            }
            else if ((code.etype & XPR_FLT) && code.value.d != 0.) {
                int32_t exponent = (code.value.u >> 52) & 0x7FF;  // exponent field of double
                if ((code.value.u & ((uint64_t(1) << 52) - 1)) == 0 && exponent != 0 && exponent != 0x7FF) {
                    // value is a power of 2, and not inf, nan, or subnormal
                    shiftCount = exponent - 0x3FF;
                    if (code.instruction == II_DIV) shiftCount = -shiftCount;
                }
            }
            if (shiftCount == (int)0xFFFFFFFF) return;  // not a power of 2. cannot optimize
            if (shiftCount >= 0 || cmd.optiLevel >= 3) {
                // replace by mul_2pow instruction
                // use negative powers of 2 only in optimization level 3, because subnormals are ignored
                code.instruction = II_MUL_2POW;
                code.value.i = shiftCount;
                code.etype = (code.etype & ~XPR_IMMEDIATE) | XPR_INT;
            }
            else if (code.instruction == II_DIV) {
                // replace division by power of 2 to multiplication by the reciprocal
                code.instruction = II_MUL;
                if (code.etype & XPR_FLT) code.value.d = 1. / code.value.d;
                else {
                    code.value.d = 1. / double((uint64_t)1 << (-shiftCount));
                    code.etype = (code.etype & ~XPR_IMMEDIATE) | XPR_FLT;
                }
            }
        }
    }
}


void insertMem(SCode & code, SExpression & expr) {
    // insert memory operand into code structure
    if (code.value.i && expr.value.i) code.etype |= XPR_ERROR; // both have constants
    if (expr.etype & XPR_OFFSET) code.offset_mem = expr.offset_mem;
    else code.offset_mem = expr.value.w;
    code.etype |= expr.etype;
    code.tokens += expr.tokens;
    code.sym1 = expr.sym1;
    code.sym2 = expr.sym2;
    code.base = expr.base;
    code.index = expr.index;
    code.length = expr.length;
    code.scale = expr.scale;
    code.symscale1 = expr.symscale1;
    code.mask |= expr.mask;
    code.fallback |= expr.fallback;
}

void insertAll(SCode & code, SExpression & expr) {
    // insert everything from expression to code structure, OR'ing all bits
    for (uint32_t i = 0; i < sizeof(SExpression) / sizeof(uint64_t); i++) {
        (&code.value.u)[i] |= (&expr.value.u)[i];
    } 
}

/****************************    assem5.cpp    ********************************
* Author:        Agner Fog
* Date created:  2017-09-19
* Last modified: 2017-11-03
* Version:       1.00
* Project:       Binary tools for ForwardCom instruction set
* Module:        assem.cpp
* Description:
* Module for assembling ForwardCom .as files. 
* This module contains functions for interpreting high level language constructs:
* functions, branches, and loops
*
* Copyright 2017 GNU General Public License http://www.gnu.org/licenses
******************************************************************************/
#include "stdafx.h"

// Define high level block types
const int HL_SECTION     =  1;  // section
const int HL_FUNC        =  2;  // function
const int HL_IF          =  3;  // if branch
const int HL_ELSE        =  4;  // else branch
const int HL_SWITCH      =  5;  // switch-case branch
const int HL_FOR         =  6;  // for loop
const int HL_FOR_IN      =  7;  // vector loop. for (v1 in [r2-r3]) {}
const int HL_WHILE       =  8;  // while loop
const int HL_DO_WHILE    =  9;  // do-while loop


// if, else, switch, for, do, while statements
void CAssembler::interpretHighLevelStatement() {
    //uint32_t label = 0;
    if (tokenN > 2 && tokens[tokenB].type == TOK_SYM && tokens[tokenB+1].id == ':') {
        // line starts with a label. insert label
        // (this will prevent merging of jump instruction. merging is not allowed when there is a label between the two instructions)
        SCode codeL;
        memset (&codeL, 0, sizeof(codeL));
        //codeL.label = tokens[tokenB].value.w; //?
        codeL.label = tokens[tokenB].id;
        codeL.section = section;
        codeBuffer.push(codeL);
        // interpret directive after label
        tokenB += 2;
        tokenN -= 2;
    }
    uint32_t tok = tokenB;
    if (tokenN > 1 && tokens[tok].type == TOK_TYP) {
        tok++;  // skip type keyword
        if (tok+1 < tokenB+tokenN && tokens[tok].type == TOK_OPR && tokens[tok].id == '+') tok++;  // skip '+' after type
    }
    // expect HLL keyword here. dispatch to the corresponding function
    switch (tokens[tok].id) {
    case HLL_IF:
        codeIf();  break;
    case HLL_SWITCH:
        codeSwitch();  break;
    case HLL_CASE:
        codeCase();  break;
    case HLL_FOR:
        codeFor();  break;
    case HLL_WHILE:
        codeWhile();  break;
    case HLL_DO:
        codeDo();  break;
    case HLL_BREAK: case HLL_CONTINUE:
        if (tok != tokenB) {
            errors.report(tokens[tok]);   // cannot have type token before break or continue
            break;
        }
        codeBreak();  break;
    case HLL_PUSH:                        // may be replaced by macro later
        codePush();  break;
    case HLL_POP:                         // may be replaced by macro later
        codePop();  break;
    default:
        errors.report(tokens[tok]);
    }
}


// finish {} block
void CAssembler::interpretEndBracket() {
    uint32_t n = hllBlocks.numEntries();
    if (n == 0) {
        errors.reportLine(ERR_BRACKET_END);  // unmatched end bracket
        return;
    }
    // dispatch depending on type of block
    switch (hllBlocks[n-1].blockType) {
    case HL_FUNC: // function
        break;
    case HL_IF: case HL_ELSE: // if branch
        codeIf2();
        break;
    case HL_FOR: // for loop
        codeFor2();
        break;
    case HL_FOR_IN: // vector loop. for (v1 in [r2-r3]) {}
        codeForIn2();
        break;
    case HL_WHILE:    // while loop
        codeWhile2();
        break;
    case HL_DO_WHILE: // do-while loop
        codeDo2();
        break;
    case HL_SWITCH: // switch-case branch
        break;
    default:
        errors.reportLine(ERR_BRACKET_END);  // should not occur
    }
}


// Interpret if statement in assembly code
void CAssembler::codeIf() {
    uint32_t state = 0;                // 0: start, 1: after type, 2: after if, 3: after (, 4: after (type, 
                                       // 5: after expression, 6: after ')', 7: after {
    uint32_t tok;                      // current token index
    SBlock block;                      // block descriptor to save
    memset (&block, 0, sizeof(block)); // reset
    block.blockType = HL_IF;           // if block
    SToken token;                      // current token
    SExpression expr;                  // expression in ()
    SCode code;                        // instruction code
    memset (&code, 0, sizeof(code));   // reset

    // interpret line by state machine looping through tokens
    for (tok = tokenB; tok < tokenB + tokenN; tok++) {
        if (lineError) break;
        token = tokens[tok];

        switch (state) {
        case 0:  // start. expect type or 'if'
            if (token.type == TOK_TYP) {
                code.dtype = token.id & 0xFF;
                state = 1;
            }
            else if (token.id == HLL_IF) state = 2;
            else errors.report(token);
            break;
        case 1:  // after type. expect '+' or 'if'
            if (token.type == TOK_OPR && token.id == '+') {
                code.dtype |= TYP_PLUS;
            }
            else if (token.id == HLL_IF) state = 2;
            else errors.report(token);
            break;
        case 2: // after if. expect '('
            if (token.type == TOK_OPR && token.id == '(') state = 3;
            else errors.report(token.pos, token.stringLength, ERR_EXPECT_PARENTHESIS);
            break;
        case 3: // after '('. expect type or logical expression
            if (token.type == TOK_TYP && !code.dtype) {
                code.dtype = token.id & 0xFF;
                state = 4;
                break;
            }
            EXPRESSION:
            expr = expression(tok, tokenB + tokenN-tok, (code.dtype & TYP_UNS) != 0);
            if (lineError) return; 

            // insert logical expression into block
            insertAll(code, expr);
            tok += expr.tokens - 1;
            state = 5;
            break;
        case 4: // after "if (type". expect '+' or expression
            if (token.type == TOK_OPR && token.id == '+') {
                code.dtype |= TYP_PLUS;
                break;
            }
            // not a '+'. expect expression
            goto EXPRESSION;
        case 5: // after expression. expect ')'
            if (token.type == TOK_OPR && token.id == ')') state = 6;
            else {
                errors.report(token);  return;
            }
            break;
        }
    }
    // should end at state 6 because '{' should be on next pseudo-line
    if (state != 6) errors.report(token);
    if (lineError) return;

    if (linei == lines.numEntries()-1) {  // no more lines
        errors.reportLine(ERR_UNFINISHED_INSTRUCTION);
        return;
    } 
    if (code.etype & (XPR_MEM | XPR_OPTION)) {
        errors.reportLine(ERR_MEM_NOT_ALLOWED);
        code.etype = XPR_INT | XPR_JUMPOS;
    } 

    // get next line
    if (linei == lines.numEntries()-1) {    // no more lines
        errors.reportLine(ERR_UNFINISHED_INSTRUCTION);
        return;
    }
    linei++;
    tokenB = lines[linei].firstToken;       // first token in line        
    tokenN = lines[linei].numTokens;        // number of tokens in line
    lineError = false;

    // expect '{'
    if (tokens[tokenB].id != '{') {
        errors.reportLine(ERR_EXPECT_BRACKET);
        return;
    }
    // interpret the condition expression
    linei--;                                // make any error message apply to previous line
    interpretCondition(code);
    linei++;
    // make instruction code
    code.etype |= XPR_JUMPOS | XPR_SYM1;
    code.section = section;

    // check if {} contains a jump only
    uint32_t target2 = hasJump(linei+1);
    if (target2) {
        if (linei + 2 < lines.numEntries() && lines[linei+2].numTokens == 1) {
            tok = lines[linei+2].firstToken;
            if (tokens[tok].type == TOK_OPR && tokens[tok].id == '}') {
                // the {} block contains a jump and nothing else
                // make conditional jump to target2 instead
                code.sym1 = target2;
                linei += 2;                          // finished processing these two lines
                // finish code and fit it
                checkCode1(code);
                if (lineError) return;    
                fitCode(code);       // find an instruction variant that fits
                if (lineError) return;     
                codeBuffer.push(code);// save code structure

                // check if there is an 'else' after if(){}
                if (linei + 2 < lines.numEntries() && lines[linei+1].numTokens == 1 && lines[linei+2].numTokens == 1) {
                    tok = lines[linei+1].firstToken;
                    if (tokens[tok].type == TOK_HLL && tokens[tok].id == HLL_ELSE) {
                        tok = lines[linei+2].firstToken;
                        if (tokens[tok].type == TOK_OPR && tokens[tok].id == '{') {
                            // make the 'else' ignored
                            linei += 2;
                            // make block record with no label
                            block.blockNumber = ++iIf;
                            block.startBracket = tok;
                            block.jumpLabel = 0;
                            // store block in hllBlocks stack. will be retrieved at matching '}'
                            hllBlocks.push(block);
                        }
                    }
                }
                return;
            }
        }
    }
    code.instruction ^= II_JUMP_INVERT;  // invert condition. jump to else block if logical expression false

    // make block record with label name
    block.blockNumber = ++iIf;
    block.startBracket = tokenB;
    char name[32];
    sprintf(name, "@if_%u_a", iIf);
    uint32_t symi = makeLabelSymbol(name);
    block.jumpLabel = symbols[symi].st_name;

    // store block in hllBlocks stack. will be retrieved at matching '}'
    hllBlocks.push(block);

    // store jump instruction
    code.sym1 = block.jumpLabel;

    // check if it can be merged with previous instruction
    mergeJump(code);

    // finish code and fit it
    checkCode1(code);
    if (lineError) return;    
    fitCode(code);       // find an instruction variant that fits
    if (lineError) return;     
    codeBuffer.push(code);// save code structure
}


// Finish if statement at end bracket
void CAssembler::codeIf2() {
    SCode code;                             // code record for jump label
    memset(&code, 0, sizeof(code));         // reset
    SBlock block = hllBlocks.pop();        // pop the stack of {} blocks
    uint32_t labelA = block.jumpLabel;           // jump target label to place here
    // check if there is an 'else' following the if(){}
    if (block.blockType == HL_IF && linei+2 < lines.numEntries() && tokens[lines[linei+1].firstToken].id == HLL_ELSE) {
        // there is an else. get next line with the else
        linei++;
        if (lines[linei].numTokens > 1) errors.report(tokens[lines[linei].firstToken+1]);  // something other than '{' after else
        // check if there is a '{' following the 'else'
        linei++;
        uint32_t tokenB = lines[linei].firstToken;
        if (lines[linei].numTokens > 1 || tokens[tokenB].type != TOK_OPR || tokens[tokenB].id != '{') {
            errors.reportLine(ERR_EXPECT_BRACKET); // expecting '{'
            return;
        }
        // make block record for jump to label b
        block.blockType = HL_ELSE;           // if-else block
        block.blockNumber = iIf;
        block.startBracket = tokenB;
        // make label name
        char name[32];
        sprintf(name, "@if_%u_b", iIf);
        uint32_t symi = makeLabelSymbol(name);
        block.jumpLabel = symbols[symi].st_name;
        hllBlocks.push(block);     // store block in hllBlocks stack. will be retrieved at matching '}'

        // make jump instruction
        code.section = section;
        code.instruction = II_JUMP;
        code.etype = XPR_JUMPOS | XPR_SYM1;
        code.sym1 = block.jumpLabel;

        // check if it can be merged with previous instruction
        mergeJump(code);

        // finish code and save it
        checkCode1(code);
        if (lineError) return;    
        fitCode(code);       // find an instruction variant that fits
        if (lineError) return;     
        codeBuffer.push(code);// save code structure
    }
    // make target label here
    if (labelA) {
        memset(&code, 0, sizeof(code));
        code.section = section;
        code.label = labelA;       // jump target label
        codeBuffer.push(code);    // save code structure
    }
}
 

// Interpret while loop in assembly code
void CAssembler::codeWhile() {
    uint32_t state = 0;                // 0: start, 1: after type, 2: after while, 3: after (, 4: after (type, 
                                       // 5: after expression, 6: after ')', 7: after {
    uint32_t tok;                      // current token index
    SBlock block;                      // block descriptor to save
    memset (&block, 0, sizeof(block)); // reset
    SToken token;                      // current token
    SExpression expr;                  // expression in ()
    SCode code;                        // instruction code
    memset (&code, 0, sizeof(code));   // reset

    // interpret line by state machine looping through tokens (same as for codeIf)
    for (tok = tokenB; tok < tokenB + tokenN; tok++) {
        if (lineError) break;
        token = tokens[tok];

        switch (state) {
        case 0:  // start. expect type or 'while'
            if (token.type == TOK_TYP) {
                code.dtype = token.id & 0xFF;
                state = 1;
            }
            else if (token.id == HLL_WHILE) state = 2;
            else errors.report(token);
            break;
        case 1:  // after type. expect '+' or 'while'
            if (token.type == TOK_OPR && token.id == '+') {
                code.dtype |= TYP_PLUS;
            }
            else if (token.id == HLL_WHILE) state = 2;
            else errors.report(token);
            break;
        case 2: // after if. expect '('
            if (token.type == TOK_OPR && token.id == '(') state = 3;
            else errors.report(token.pos, token.stringLength, ERR_EXPECT_PARENTHESIS);
            break;
        case 3: // after '('. expect type or logical expression
            if (token.type == TOK_TYP && !code.dtype) {
                code.dtype = token.id & 0xFF;
                state = 4;
                break;
            }
        EXPRESSION:
            expr = expression(tok, tokenB + tokenN-tok, (code.dtype & TYP_UNS) != 0);
            if (lineError) return; 

            // insert logical expression into block
            insertAll(code, expr);
            tok += expr.tokens - 1;
            state = 5;
            break;
        case 4: // after "while (type". expect '+' or expression
            if (token.type == TOK_OPR && token.id == '+') {
                code.dtype |= TYP_PLUS;
                break;
            }
            // not a '+'. expect expression
            goto EXPRESSION;
        case 5: // after expression. expect ')'
            if (token.type == TOK_OPR && token.id == ')') state = 6;
            else {
                errors.report(token);  return;
            }
            break;
        }
    }
    // should end at state 6 because '{' should be on next pseudo-line
    if (state != 6) errors.report(token);
    if (lineError) return;

    if (linei == lines.numEntries()-1) {  // no more lines
        errors.reportLine(ERR_UNFINISHED_INSTRUCTION);
        return;
    } 
    if (code.etype & (XPR_MEM | XPR_OPTION)) {
        errors.reportLine(ERR_MEM_NOT_ALLOWED);
        code.etype = XPR_INT | XPR_JUMPOS;
    } 

    // get next line
    // get next line
    if (linei == lines.numEntries()-1) {    // no more lines
        errors.reportLine(ERR_UNFINISHED_INSTRUCTION);
        return;
    }
    linei++;
    tokenB = lines[linei].firstToken;       // first token in line        
    tokenN = lines[linei].numTokens;        // number of tokens in line
    lineError = false;

    // expect '{'
    if (tokens[tokenB].id != '{') {
        errors.reportLine(ERR_EXPECT_BRACKET);
        return;
    }

    // interpret the condition expression
    linei--;                                // make any error message apply to previous line
    interpretCondition(code);
    linei++;

    // make instruction code
    code.etype |= XPR_JUMPOS | XPR_SYM1;
    code.section = section;

    // make block record with label names
    block.blockType = HL_WHILE;        // while-block
    block.blockNumber = ++iLoop;
    block.startBracket = tokenB;
    char name[32];
    sprintf(name, "@while_%u_a", iLoop);
    uint32_t symi1 = makeLabelSymbol(name);
    block.jumpLabel = symbols[symi1].st_name;  // label to jump back to
    sprintf(name, "@while_%u_b", iLoop);
    uint32_t symi2 = makeLabelSymbol(name);
    block.breakLabel = symbols[symi2].st_name;  // label after loop. used if condition is false first time and for break statements
    block.continueLabel = 0xFFFFFFFF;          // this label will only be made if there is a continue statement

    // make code to check condition before first iteration
    SCode code1 = code;
    code1.instruction ^= II_JUMP_INVERT;  // invert condition to jump if false
    code1.sym1 = block.breakLabel;
    // check if it can be merged with previous instruction
    mergeJump(code1);
    // finish code and store it
    checkCode1(code1);
    if (lineError) return;    
    fitCode(code1);       // find an instruction variant that fits
    if (lineError) return;     
    codeBuffer.push(code1);// save code structure

    // make loop label
    memset(&code1, 0, sizeof(code1));
    code1.label = block.jumpLabel;
    code1.section = section;
    codeBuffer.push(code1);// save code structure

    // make instruction to place at end of loop
    code.sym1 = block.jumpLabel;
    checkCode1(code);
    // store in codeBuffer2 for later insertion at the end of the loop
    block.codeBuffer2index = codeBuffer2.push(code);
    block.codeBuffer2num = 1;

    // store block in hllBlocks stack. will be retrieved at matching '}'
    hllBlocks.push(block);
}

// Finish while-loop at end bracket
void CAssembler::codeWhile2() {
    SCode code;                            // code record for jump back
    SBlock block = hllBlocks.pop();        // pop the stack of {} blocks
    if (block.continueLabel != 0xFFFFFFFF) {
        // place label here as jump target for continue statements
        memset(&code, 0, sizeof(code));
        code.label = block.continueLabel;
        code.section = section;
        codeBuffer.push(code);
    }

    uint32_t codebuf2num = codeBuffer2.numEntries();
    if (block.codeBuffer2num && block.codeBuffer2index < codebuf2num) {
        // retrieve jumpback instruction
        code = codeBuffer2[block.codeBuffer2index];
        // check if it can be merged with previous instruction
        mergeJump(code);
        // finish code and store it
        checkCode1(code);
        if (lineError) return;
        fitCode(code);       // find an instruction variant that fits
        if (lineError) return;
        codeBuffer.push(code);  // save code
        // remove from temporary buffer
        if (block.codeBuffer2index + 1 == codebuf2num) codeBuffer2.pop();
        // place label for breaking out
        memset(&code, 0, sizeof(code));
        code.label = block.breakLabel;
        code.section = section;
        codeBuffer.push(code);  // save label
        return;
    }
}

// Interpret do-while loop in assembly code
void CAssembler::codeDo() {
    SBlock block;                          // block record
    SCode code;                            // code record for label
    memset(&block, 0, sizeof(block));
    memset(&code, 0, sizeof(code));

    // make block record with label names
    block.blockType = HL_DO_WHILE;        // do-while-block
    block.blockNumber = ++iLoop;
    char name[32];
    sprintf(name, "@do_%u_a", iLoop);
    uint32_t symi1 = makeLabelSymbol(name);
    block.jumpLabel = symbols[symi1].st_name;  // label to jump back to
    block.breakLabel = 0xFFFFFFFF;             // this label will only be made if there is a break statement
    block.continueLabel = 0xFFFFFFFF;          // this label will only be made if there is a continue statement
    // make loop label
    code.label = block.jumpLabel;
    code.section = section;
    codeBuffer.push(code);                     // save label

    // get next line with '{'
    if (linei == lines.numEntries()-1) {    // no more lines
        errors.reportLine(ERR_UNFINISHED_INSTRUCTION);
        return;
    }
    linei++;
    tokenB = lines[linei].firstToken;       // first token in line        
    tokenN = lines[linei].numTokens;        // number of tokens in line
    lineError = false; 
    // expect '{'
    if (tokens[tokenB].id != '{') {
        errors.report(tokens[tokenB]);
        return;
    }
    block.startBracket = tokenB;
    hllBlocks.push(block);                     // store block in hllBlocks stack. will be retrieved at matching '}'
}

// Finish do-while loop at end bracket
void CAssembler::codeDo2() {
    SCode code;                            // code record 

    SBlock block = hllBlocks.pop();        // pop the stack of {} blocks
    if (block.continueLabel != 0xFFFFFFFF) {
        // place label here as jump target for continue statements
        memset(&code, 0, sizeof(code));
        code.label = block.continueLabel;
        code.section = section;
        codeBuffer.push(code);
    }
    // find 'while' keyword in next pseudo-line after '}'
    if (linei+1 >= lines.numEntries()) {                  // no more lines
        errors.reportLine(ERR_WHILE_EXPECTED); return;
    }
    linei++;
    tokenB = lines[linei].firstToken;       // first token in line        
    tokenN = lines[linei].numTokens;        // number of tokens in line
    lineError = false;

    // expect "while (expression)"
    uint32_t state = 0;                // 0: start, 1: after type, 2: after 'while', 3: after (, 4: after (type, 
                                       // 5: after expression, 6: after ')'
    uint32_t tok;                      // current token index
    SToken token;                      // current token
    SExpression expr;                  // expression in ()
    memset(&code, 0, sizeof(code));    // reset code

    // interpret line by state machine looping through tokens
    for (tok = tokenB; tok < tokenB + tokenN; tok++) {
        if (lineError) return;
        token = tokens[tok];

        switch (state) {
        case 0:  // start. expect type or 'while'
            if (token.type == TOK_TYP) {
                code.dtype = token.id & 0xFF;
                state = 1;
            }
            else if (token.id == HLL_WHILE) state = 2;
            else errors.reportLine(ERR_WHILE_EXPECTED);
            break;
        case 1:  // after type. expect '+' or 'while'
            if (token.type == TOK_OPR && token.id == '+') {
                code.dtype |= TYP_PLUS;
            }
            else if (token.id == HLL_WHILE) state = 2;
            else errors.report(token);
            break;
        case 2: // after 'while'. expect '('
            if (token.type == TOK_OPR && token.id == '(') state = 3;
            else errors.report(token.pos, token.stringLength, ERR_EXPECT_PARENTHESIS);
            break;
        case 3: // after '('. expect type or logical expression
            if (token.type == TOK_TYP && !code.dtype) {
                code.dtype = token.id & 0xFF;
                state = 4;
                break;
            }
        EXPRESSION:
            expr = expression(tok, tokenB + tokenN - tok, (code.dtype & TYP_UNS) != 0);
            if (lineError) return;

            // insert logical expression into block
            insertAll(code, expr);
            tok += expr.tokens - 1;
            state = 5;
            break;
        case 4: // after "while (type". expect '+' or expression
            if (token.type == TOK_OPR && token.id == '+') {
                code.dtype |= TYP_PLUS;
                break;
            }
            // not a '+'. expect expression
            goto EXPRESSION;
        case 5: // after expression. expect ')'
            if (token.type == TOK_OPR && token.id == ')') state = 6;
            else {
                errors.report(token);  return;
            }
            break;
        }
    }
    // should end at state 6
    if (state != 6) errors.report(token);
    if (lineError) return;

    if (code.etype & (XPR_MEM | XPR_OPTION)) {
        errors.reportLine(ERR_MEM_NOT_ALLOWED);
        code.etype = XPR_INT | XPR_JUMPOS;
    }

    // make instruction with condition
    interpretCondition(code);
    code.etype |= XPR_JUMPOS | XPR_SYM1;
    code.section = section;
    code.sym1 = block.jumpLabel;
    // check if it can be merged with previous instruction
    mergeJump(code);
    // finish code and fit it
    checkCode1(code);
    if (lineError) return;    
    fitCode(code);       // find an instruction variant that fits
    if (lineError) return;     
    codeBuffer.push(code);// save code structure

    if (block.breakLabel != 0xFFFFFFFF) {
        // place label here as jump target for break statements
        memset(&code, 0, sizeof(code));
        code.label = block.breakLabel;
        code.section = section;
        codeBuffer.push(code);
    }
}

// Interpret for-loop in assembly code
void CAssembler::codeFor() {
    uint32_t tok;                                 // token number

    // search for 'in' keyword
    for (tok = tokenB; tok < tokenB + tokenN; tok++) {
        if (tokens[tok].type == TOK_HLL && tokens[tok].id == HLL_IN) {
            // this is a for-in vector loop
            codeForIn();
            return;
        }
    }

    // this is a traditional for(;;) loop
    uint32_t state = 0;                // state while interpreting line
                                       // 0: start, 1: after type, 2: after 'for', 3: after (, 4: after (type, 
    SBlock block;                      // block descriptor to save
    memset (&block, 0, sizeof(block)); // reset
    block.blockType = HL_FOR;          // 'for' block
    block.breakLabel = block.jumpLabel = block.continueLabel = 0xFFFFFFFF;
    SToken token;                      // current token
    SToken typeToken;                  // token defining type
    uint32_t type = 0;                 // operand type for all three expressions in for(;;)
    uint32_t symi = 0;                 // symbol index
    char name[48];                     // symbol name
    int conditionFirst = 0;            // evaluation of the condition before first iteration:
                                       // 0: condition must be checked before first iteration
                                       // 2: condition is false before first iteration. zero iterations
                                       // 3: condition is true before first iteration. no initial check needed

    // interpret line by state machine looping through tokens
    for (tok = tokenB; tok < tokenB + tokenN; tok++) {
        if (lineError) break;
        token = tokens[tok];

        if (state == 0) { // start. expect type or 'for'
            if (token.type == TOK_TYP) {
                type = token.id & 0xFF;
                typeToken = token;
                state = 1;
            }
            else if (token.id == HLL_FOR) state = 2;
            else errors.report(token);
        }
        else if (state == 1) { // after type. expect '+' or 'for'
            if (token.type == TOK_OPR && token.id == '+') {
                type |= TYP_PLUS;
            }
            else if (token.id == HLL_FOR) state = 2;
            else errors.report(token);
        }
        else if (state == 2) { // after 'for'. expect '('
            if (token.type == TOK_OPR && token.id == '(') state = 3;
            else errors.report(token.pos, token.stringLength, ERR_EXPECT_PARENTHESIS);
        }
        else if (state == 3) { // after '('. expect type or initialization
            if (token.type == TOK_TYP && !type) {
                type = token.id & 0xFF;
                typeToken = token;
                tok++;
                if (tok < tokenB + tokenN && tokens[tok].type == TOK_OPR && tokens[tok].id == '+') {
                    // '+' after type
                    type |= TYP_PLUS;
                    tok++;
                }
            }
            state = 4;
            break;  // end tok loop here            
        }
    }
    if (state != 4) {
        errors.report(token);
        return;
    }
    if (lineError) return;

    // check type
    if (type == 0) {
        errors.reportLine(ERR_TYPE_MISSING);  return;
    }
    // extend type to int64 if allowed. this allows various optimizations, and int64 is required for array indexes anyway
    if ((type & TYP_PLUS) && !(type & TYP_FLOAT)) type = TYP_INT64 | (type & TYP_UNS);

    // remake token sequence for generating initial instruction
    uint32_t tokensRestorePoint = tokens.numEntries();
    typeToken.id = type;
    tokens.push(typeToken);
    uint32_t tokenFirst = tok;
    if (tokens[tokenFirst].type == TOK_TYP) tokenFirst++; // skip type token. it has already been inserted
    for (; tok < tokenB + tokenN; tok++) {  // insert remaining tokens
        token = tokens[tok];
        if (token.type == TOK_OPR && token.id == ';') break;
        tokens.push(tokens[tok]);
    }
    // make an instruction out of this sequence
    tokenB = tokensRestorePoint;
    tokenN = tokens.numEntries() - tokensRestorePoint;
    uint32_t codePoint = codeBuffer.numEntries();
    SCode initializationCode;
    memset(&initializationCode, 0, sizeof(initializationCode));
    if (tokenN > 1) {   // skip if there is no code
        interpretCodeLine();
        if (codeBuffer.numEntries() == codePoint + 1) {
            // remember initialization code
            initializationCode = codeBuffer[codePoint];
        }
    }
    // remove temporary token sequence
    tokens.setNum(tokensRestorePoint);
    if (lineError) return;

    // get next line containing loop condition
    SCode conditionCode;
    memset(&conditionCode, 0, sizeof(conditionCode));
    conditionCode.section = section;
    if (linei+2 >= lines.numEntries()) {  // no more lines
        errors.reportLine(ERR_UNFINISHED_INSTRUCTION);
        return;
    }
    linei++;
    tokenB = lines[linei].firstToken;       // first token in line        
    tokenN = lines[linei].numTokens;        // number of tokens in line
    if (tokenN == 1 && tokens[tokenB].type == TOK_OPR && tokens[tokenB].id == ';') {
        // no condition specified. infinite loop
        conditionFirst = 3;
        conditionCode.instruction = II_JUMP;
        conditionCode.etype = XPR_JUMPOS;
    }
    else {
        SExpression expr = expression(tokenB, tokenN, (type & TYP_UNS) != 0);
        if (lineError) return;
        insertAll(conditionCode, expr);  // insert logical expression into block
        conditionCode.dtype = type;
        interpretCondition(conditionCode);  // convert expression to conditional jump
        conditionCode.etype |= XPR_JUMPOS | XPR_SYM1;
        conditionCode.section = section;
        tok = tokenB + expr.tokens;       // expect ';' after expression
        if (tokens[tok].type != TOK_OPR || tokens[tok].id != ';') {
            errors.report(tokens[tok]);
        }
        //uint32_t counterRegister = 0;
        uint64_t counterStart = 0;
        // are start and end values known constants?
        if (initializationCode.instruction == II_MOVE && (initializationCode.etype & XPR_INT) && initializationCode.dest
            && !(initializationCode.etype & (XPR_REG1 | XPR_MEM | XPR_OPTION))) {
            //counterRegister = initializationCode.dest;
            counterStart = initializationCode.value.i;
            if ((expr.etype & XPR_INT) && (expr.etype & XPR_REG1) && !(expr.etype & (XPR_REG2 | XPR_MEM | XPR_OPTION))) {
                // start and end values are integer constants
                if ((expr.instruction & 0xFF) == II_COMPARE) {
                    // compare instruction. condition is in option bits
                    switch ((expr.optionbits >> 1) & 3) {
                    case 0:  // ==
                        conditionFirst = 2 + (counterStart == expr.value.u);  break;
                    case 1:  // <
                        if (type & TYP_UNS) conditionFirst = 2 + (counterStart < expr.value.u);
                        else conditionFirst = 2 + ((int64_t)counterStart < expr.value.i);
                        break;
                    case 2:  // >
                        if (type & TYP_UNS) conditionFirst = 2 + (counterStart > expr.value.u);
                        else conditionFirst = 2 + ((int64_t)counterStart > expr.value.i);
                        break;
                    }
                    if (expr.optionbits & 1) conditionFirst ^= 1; // invert if bit 0    
                }
                else if ((expr.instruction & 0xFF) == II_AND) {
                    // bit test. if (r1 & 1 << n)
                    conditionFirst = 2 + (counterStart & ((uint64_t)1 << expr.value.u)) != 0;
                }
            }
        }
    }

    // make block record with label name
    block.blockNumber = ++iLoop;

    if (conditionFirst == 0) {
        // condition must be checked before first iteration
        conditionCode.instruction ^= II_JUMP_INVERT;  // invert condition
        sprintf(name, "@for_%u_b", iLoop);
        symi = makeLabelSymbol(name);
        block.breakLabel = symbols[symi].st_name;
        conditionCode.sym1 = block.breakLabel;
        // check if it can be merged with previous instruction
        mergeJump(conditionCode);
        checkCode1(conditionCode);        // finish code and fit it
        if (lineError) return;    
        fitCode(conditionCode);       // find an instruction variant that fits
        if (lineError) return;     
        codeBuffer.push(conditionCode);// save code structure
        conditionCode.instruction ^= II_JUMP_INVERT;  // invert condition back again
    }
    else if (conditionFirst == 2) {
        // condition is known to be false. loop goes zero times
        SCode jumpAlways;
        memset(&jumpAlways, 0, sizeof(jumpAlways));
        jumpAlways.instruction = II_JUMP;  // jump past loop
        jumpAlways.section = section;
        jumpAlways.etype = XPR_JUMPOS;
        sprintf(name, "@for_%u_goes_zero_times", iLoop);
        symi = makeLabelSymbol(name);
        block.breakLabel = symbols[symi].st_name;
        jumpAlways.sym1 = block.breakLabel;
        // check if it can be merged with previous instruction
        mergeJump(jumpAlways);
        checkCode1(jumpAlways);        // finish code and fit it
        if (lineError) return;    
        fitCode(jumpAlways);       // find an instruction variant that fits
        if (lineError) return;     
        codeBuffer.push(jumpAlways);// save code structure
    }
    // make label for loop back
    if (conditionCode.instruction != II_JUMP) {    
        sprintf(name, "@for_%u_a", iLoop);
    }
    else {
        sprintf(name, "@infinite_loop_%u_a", iLoop);
    }
    symi = makeLabelSymbol(name);
    block.jumpLabel = symbols[symi].st_name;
    conditionCode.sym1 = block.jumpLabel;
    SCode codeLabel;
    memset(&codeLabel, 0, sizeof(codeLabel));
    codeLabel.label = block.jumpLabel;
    codeLabel.section = section;    
    codeBuffer.push(codeLabel);

    // get next line containing increment
    linei++;
    tokenB = lines[linei].firstToken;       // first token in line        
    tokenN = lines[linei].numTokens;        // number of tokens in line
    // line must end with ')'
    if (tokenN < 1) {
        errors.reportLine(ERR_UNFINISHED_INSTRUCTION);
        return;
    }
    if (tokens[tokenB+tokenN-1].type != TOK_OPR || tokens[tokenB+tokenN-1].id != ')') {
        errors.report(tokens[tokenB+tokenN-1]); // expecting ')'
        return;
    }

    // make instruction for loop counter increment
    tokens.push(typeToken);
    for (tok = tokenB; tok < tokenB + tokenN - 1; tok++) {  // insert remaining tokens
        tokens.push(tokens[tok]);
    }
    // make an instruction out of this sequence
    tokenB = tokensRestorePoint;
    tokenN = tokens.numEntries() - tokensRestorePoint;
    SCode incrementCode;
    memset (&incrementCode, 0, sizeof(incrementCode));
    codePoint = codeBuffer.numEntries();
    if (tokenN > 1) {
        interpretCodeLine();
        if (codeBuffer.numEntries() == codePoint + 1) {
            incrementCode = codeBuffer[codePoint];        
            incrementCode.section = section;
        }
    }
    // remove temporary token sequence
    tokens.setNum(tokensRestorePoint);
    // remove temporary incrementation code. it has to be inserted after the loop
    codeBuffer.setNum(codePoint);
    if (lineError) return;

    // save instructions in block
    block.codeBuffer2index = codeBuffer2.push(incrementCode);
    codeBuffer2.push(conditionCode);
    block.codeBuffer2num = 2;

    // get next line containing '{'
    linei++;
    tokenB = lines[linei].firstToken;       // first token in line        
    tokenN = lines[linei].numTokens;        // number of tokens in line
    // line must contain '{'
    if (tokenN != 1 || tokens[tokenB].type != TOK_OPR || tokens[tokenB].id != '{') {
        errors.reportLine(ERR_EXPECT_BRACKET);
        return;
    }
    block.startBracket = tokenB;

    // save block to be recalled at end bracket
    hllBlocks.push(block);

}

// Finish for-loop at end bracket
void CAssembler::codeFor2() {
    SCode incrementCode;                    // code record for incrementing loop counter
    SCode conditionCode;                    // code record for conditional jump back
    SCode labelCode;                        // code record for label
    SBlock block = hllBlocks.pop();         // pop the stack of {} blocks
    if (block.continueLabel != 0xFFFFFFFF) {
        // place label here as jump target for continue statements
        memset(&labelCode, 0, sizeof(labelCode));
        labelCode.label = block.continueLabel;
        labelCode.section = section;
        codeBuffer.push(labelCode);
    }

    uint32_t codebuf2num = codeBuffer2.numEntries();
    if (block.codeBuffer2num == 2 && block.codeBuffer2index < codebuf2num) {
        // retrieve prepared instruction
        incrementCode = codeBuffer2[block.codeBuffer2index];
        conditionCode = codeBuffer2[block.codeBuffer2index+1];

        // finish increment code and store it
        if (incrementCode.instruction) {
            checkCode1(incrementCode);
            if (lineError) return;
            fitCode(incrementCode);       // find an instruction variant that fits
            if (lineError) return;
            codeBuffer.push(incrementCode);  // save code
        }

        // finish condition code and store it
        // check if it can be merged with previous instruction
        mergeJump(conditionCode);
        checkCode1(conditionCode);
        if (lineError) return;
        fitCode(conditionCode);       // find an instruction variant that fits
        if (lineError) return;
        codeBuffer.push(conditionCode);  // save code
                                
        // remove from temporary buffer
        if (block.codeBuffer2index + 2 == codebuf2num) {
            codeBuffer2.pop(); codeBuffer2.pop();
        }
        // place label for breaking out
        if (block.breakLabel != 0xFFFFFFFF) {
            memset(&labelCode, 0, sizeof(labelCode));
            labelCode.label = block.breakLabel;
            labelCode.section = section;
            codeBuffer.push(labelCode);  // save label
        }
    }
}

// Interpret for-in vector loop in assembly code
// for (float v1 in [r1-r2]) {}
void CAssembler::codeForIn() {
    uint32_t state = 0;                // state while interpreting line
                                       // 0: start, 1: after type, 2: after 'for', 3: after (, 4: after (type,
                                       // 5: after vector register, 6: after 'in', 7: after '['
                                       // 8: after base register, 9: after '-', 10: after index register
                                       // 11: after ']', 12: after ')'
    SBlock block;                      // block descriptor to save
    memset (&block, 0, sizeof(block)); // reset
    block.blockType = HL_FOR_IN;       // 'for-in' block
    block.breakLabel = block.jumpLabel = block.continueLabel = 0xFFFFFFFF;
    block.blockNumber = ++iLoop;       // number to use in labels
    //uint32_t baseReg;                  // base register
    uint32_t indexReg = 0;             // index register
    uint32_t tok;                      // token number
    SToken token;                      // current token
    uint32_t type = 0;                 // vector element type
    uint32_t symi;                     // symbol index
    char name[32];                     // symbol name

    // interpret line by state machine looping through tokens
    for (tok = tokenB; tok < tokenB + tokenN; tok++) {
        if (lineError) break;
        token = tokens[tok];

        switch (state) {
        case 0:  // start. expect type or 'for'
            if (token.type == TOK_TYP) {
                type = token.id & 0xFF;
                state = 1;
            }
            else if (token.type == TOK_HLL && token.id == HLL_FOR) {
                state = 2;
            }
            else errors.report(token);
            break;
        case 1:  // after type. expect 'for' ('+' not allowed after type)
            if (token.type == TOK_HLL && token.id == HLL_FOR) {
                state = 2;
            }
            else errors.report(token);
            break;
        case 2:  // after 'for'. expect '('
            if (token.type == TOK_OPR && token.id == '(') {
                state = 3;
            }
            else errors.report(token);
            break;
        case 3:  // after '('. expect type or vector register
            if (token.type == TOK_TYP && !type) {
                type = token.id & 0xFF;
                state = 4;
            }
            else if (token.type == TOK_REG) {
                // must be vector register
                if (!(token.id & REG_V)) errors.report(token.pos, token.stringLength, ERR_WRONG_REG_TYPE);
                state = 5;
            }
            else errors.report(token);
            break;
        case 4:  // after type. expect vector register         
            if (token.type == TOK_REG) {
                // must be vector register
                if (!(token.id & REG_V)) errors.report(token.pos, token.stringLength, ERR_WRONG_REG_TYPE);
                state = 5;
            }
            else errors.report(token);
            break;
        case 5:  // after vector register. expect 'in'
            if (token.type == TOK_HLL && token.id == HLL_IN) {
                state = 6;
            }
            else errors.report(token);
            break;
        case 6:  // after 'in'. expect '['
            if (token.type == TOK_OPR && token.id == '[') {
                state = 7;
            }
            else errors.report(token);
            break;
        case 7:  // after '['. expect base register
            if (token.type == TOK_REG) {
                // must be general purpose register
                if (!(token.id & REG_R)) errors.report(token.pos, token.stringLength, ERR_WRONG_REG_TYPE);
                //baseReg = token.id;
                state = 8;
            }
            else errors.report(token);
            break;
        case 8:  // after base register. expect '-'
            if (token.type == TOK_OPR && token.id == '-') {
                state = 9;
            }
            else errors.report(token);
            break;
        case 9:  // after '-'. expect index register
            if (token.type == TOK_REG) {
                // must be general purpose register, except r31
                if (!(token.id & REG_R) || token.id == (REG_R|31)) errors.report(token.pos, token.stringLength, ERR_WRONG_REG_TYPE);
                indexReg = token.id;
                state = 10;
            }
            else errors.report(token);
            break;
        case 10:  // after index register. expect ']'
            if (token.type == TOK_OPR && token.id == ']') {
                state = 11;
            }
            else errors.report(token);
            break;
        case 11:  // after ']'. expect ')'
            if (token.type == TOK_OPR && token.id == ')') {
                state = 12;
            }
            else errors.report(token);
            break;
        default:
            errors.report(token);
        }
    }
    // get next line and expect '{'
    if (linei == lines.numEntries()-1) {    // no more lines
        errors.reportLine(ERR_UNFINISHED_INSTRUCTION);
        return;
    }
    linei++;
    tokenB = lines[linei].firstToken;       // first token in line        
    tokenN = lines[linei].numTokens;        // number of tokens in line
    lineError = false;

    // expect '{'
    if (tokens[tokenB].id != '{') {
        errors.reportLine(ERR_EXPECT_BRACKET);
        return;
    }
    block.startBracket = tokenB;

    // look at preceding instruction to see if value of index register is known to be positive
    bool startCheckNeeded = true;
    SCode previousInstruction;
    if (codeBuffer.numEntries()) {
        previousInstruction = codeBuffer[codeBuffer.numEntries()-1];  // recall previous instruction
        if (previousInstruction.section == section && previousInstruction.instruction == II_MOVE
            && (previousInstruction.etype & XPR_INT) && previousInstruction.dest == indexReg
            && !(previousInstruction.etype & (XPR_REG1 | XPR_MEM | XPR_OPTION))
            && previousInstruction.value.i > 0) {
            startCheckNeeded = false;
        }
    }
    if (startCheckNeeded) {
        // make label name for break
        sprintf(name, "@for_%u_b", iLoop);
        symi = makeLabelSymbol(name);
        block.breakLabel = symbols[symi].st_name;  // label to jump back to
        // make conditional jump if index not positive
        SCode startCheck;
        memset(&startCheck, 0, sizeof(startCheck));
        startCheck.section = section;
        startCheck.instruction = II_COMPARE | II_JUMP_POSITIVE | II_JUMP_INVERT;
        startCheck.reg1 = indexReg;
        startCheck.sym1 = block.breakLabel;
        startCheck.etype = XPR_INT | XPR_REG | XPR_REG1 | XPR_JUMPOS;
        startCheck.line = linei;
        startCheck.dtype = TYP_INT64;        
        mergeJump(startCheck);   // check if it can be merged with previous instruction
        checkCode1(startCheck);
        fitCode(startCheck);       // find an instruction variant that fits
        if (lineError) return;     
        codeBuffer.push(startCheck);// save instruction
    }
    // make label name for loop
    sprintf(name, "@for_%u_a", iLoop);
    symi = makeLabelSymbol(name);
    block.jumpLabel = symbols[symi].st_name;  // label to jump back to
    SCode labelCode;
    memset(&labelCode, 0, sizeof(labelCode));
    labelCode.section = section;
    labelCode.label = block.jumpLabel;
    codeBuffer.push(labelCode);   // save label

    // save index registr and type in block
    block.codeBuffer2num = indexReg;
    block.codeBuffer2index = type;

    // save block to be recalled at '}'
    hllBlocks.push(block);
}

// Finish for-in vector loop in assembly code
void CAssembler::codeForIn2() {
    SCode code;                            // code record for jump back
    SBlock block = hllBlocks.pop();        // pop the stack of {} blocks
    if (block.continueLabel != 0xFFFFFFFF) {
        // place label here as jump target for continue statements
        memset(&code, 0, sizeof(code));
        code.label = block.continueLabel;
        code.section = section;
        codeBuffer.push(code);
    }
    // make loop instruction
    memset(&code, 0, sizeof(code));
    code.section = section;
    code.line = linei;
    code.instruction = II_SUB_MAXLEN | II_JUMP_POSITIVE;
    code.reg1 = code.dest = block.codeBuffer2num;
    code.value.u = block.codeBuffer2index & 0xF;  // element type in vector
    code.dtype = TYP_INT64;
    code.sym1 = block.jumpLabel;
    code.etype = XPR_INT | XPR_REG | XPR_REG1 | XPR_JUMPOS;
    // fit code and save it
    checkCode1(code);
    fitCode(code); 
    if (lineError) return;     
    codeBuffer.push(code);  // save instruction
    // make break label
    if (block.breakLabel != 0xFFFFFFFF) {
        memset(&code, 0, sizeof(code));
        code.section = section;
        code.label = block.breakLabel;
        codeBuffer.push(code);   // save label
    }
}


// Interpret switch statement in assembly code
void CAssembler::codeSwitch() {
    /*
    CMetaBuffer<CDynamicArray<SCcaseLabel> > caseLabels;
    CDynamicArray<SCode> extraCode;
    if (caseLabels.numEntries() == 0) caseLabels.setNum(switchNumber);
    */


}

// Interpret switch case label in assembly code
void CAssembler::codeCase() {}

// Finish switch statement at end bracket
void CAssembler::codeSwitch2() {
}

// Interpret break or continue statement in assembly code
void CAssembler::codeBreak() {
    uint32_t symi = findBreakTarget(tokens[tokenB].id);
    if (symi == 0) {
        errors.report(tokens[tokenB].pos, tokens[tokenB].stringLength, tokens[tokenB].id == HLL_BREAK ? ERR_MISPLACED_BREAK : ERR_MISPLACED_CONTINUE);
        return;
    }
    // make jump to symi
    SCode code;
    memset(&code, 0, sizeof(code));
    code.section = section;
    code.instruction = II_JUMP;
    code.etype = XPR_JUMPOS | XPR_SYM1;
    code.sym1 = symi;

    // check if it can be merged with previous instruction
    mergeJump(code);

    // finish code and save it
    checkCode1(code);
    if (lineError) return;    
    fitCode(code);       // find an instruction variant that fits
    if (lineError) return;     
    codeBuffer.push(code);// save code structure
}


// Find or make the target symbol of a break or continue statement
uint32_t CAssembler::findBreakTarget(uint32_t k) {
    // k is HLL_BREAK or HLL_CONTINUE
    int32_t blocki;        // block index
    uint32_t symi;         // symbol id
    bool found = false;    // target found
    // search backwords through blocks to find the first block that can have break/continue
    for (blocki = (int32_t)hllBlocks.numEntries() - 1; blocki >= 0; blocki--) {
        switch (hllBlocks[blocki].blockType) {
        case HL_FOR: case HL_FOR_IN: case HL_WHILE: case HL_DO_WHILE:
            // can have break and continue
            found = true;
            break;
        case HL_SWITCH:
            // can have break, but not continue
            if (k == HLL_BREAK) found = true;
            break;
        case HL_FUNC: case HL_SECTION:
            // stop search and fail
            return 0;
        }
        if (found) break;
    }
    if (!found) return 0;  // not found
    char prefix;
    // find symbol in block
    if (k == HLL_BREAK) {
        symi = hllBlocks[blocki].breakLabel;
        prefix = 'b';
    }
    else {
        symi = hllBlocks[blocki].continueLabel;
        prefix = 'c';
    }     
    if (symi != 0xFFFFFFFF) return symi;  // symbol has already been assigned

    // symbol has not been assigned yet. give it a name
    const char * blockName = 0;
    switch (hllBlocks[blocki].blockType) {
    case HL_FOR: case HL_FOR_IN:
        blockName = "for";  break;    
    case HL_WHILE: 
        blockName = "while";  break;    
    case HL_DO_WHILE:
        blockName = "do";  break;
    case HL_SWITCH:
        blockName = "switch";  break;
    default: return 0;
    }
    char name[32];
    sprintf(name, "@%s_%u_%c", blockName, hllBlocks[blocki].blockNumber, prefix);
    symi = makeLabelSymbol(name);
    symi = symbols[symi].st_name;
    // save name in block, in case it is needed again
    if (k == HLL_BREAK) {
        hllBlocks[blocki].breakLabel = symi;
    }
    else {
        hllBlocks[blocki].continueLabel = symi;
    }
    return symi;
}


// Make a symbol for branch label etc., address not known yet. Returns zero if already defined
uint32_t CAssembler::makeLabelSymbol(const char * name) {
    ElfFWC_Sym2 sym;
    memset(&sym, 0, sizeof(sym));
    sym.st_type = STT_FUNC;
    sym.st_other = STV_HIDDEN | STV_IP;
    sym.st_shndx = section;
    sym.st_name = symbolNameBuffer.putStringN(name, (uint32_t)strlen(name));
    uint32_t symi = addSymbol(sym);  // save symbol with name
    if (symi == 0) {
        errors.reportLine(ERR_SYMBOL_DEFINED);
    }
    return symi;
}

// Merge jump instruction with preceding arithmetic instruction.
// If successful, returns true and puts the result in code1
bool CAssembler::mergeJump(SCode & code2) {
    if (cmd.optiLevel == 0) return false;        // merge only if optimization is on
    if (code2.label) return false;               // cannot merge if there is a label between the two instructions
    if (codeBuffer.numEntries() == 0) return false; // no previous instruction to merge with
    SCode code1 = codeBuffer[codeBuffer.numEntries()-1]; // previous code

    if (code1.section != code2.section) return false; // must be in same section
    SCode code3 = code1 | code2;  // combined code
    uint32_t type = code1.dtype;
    // first instruction cannot have memory operand or other special options
    if (code1.etype & (XPR_MEM | XPR_SYM1 | XPR_MASK | XPR_OPTION | XPR_OPTIONS | XPR_JUMPOS | XPR_ERROR)) return false;
    if (!(code2.etype & XPR_JUMPOS)) return false;
    if (code1.dest != code1.reg1 && !(code1.etype & XPR_INT)) return false;  // cannot have 3 registers

    if (code2.instruction == II_JUMP) {
        // combine unconditional jump with add/sub
        if (code2.etype & (XPR_MEM | XPR_OPTION | XPR_REG)) return false;
        if ((code1.instruction & ~1) != II_ADD) return false;  // only add and sub
        if (type & TYP_FLOAT) return false;
        // successful combination of add/sub and jump
        codeBuffer.pop();        // remove previous code from buffer
        code2 = code3;
        return true;
    }

    // second instruction must test the result of the first instruction
    if (code1.dest != code2.reg1) return false;
    // must have same operand type
    if ((code1.dtype & 0xF) > (code2.dtype & 0xF)) {
        if (!(code2.dtype & TYP_PLUS)) return false;
    }
    if ((code1.dtype & 0xF) < (code2.dtype & 0xF)) {
        if (!(code1.dtype & TYP_PLUS)) return false;
        type = code2.dtype;
    }
    type |= code2.dtype & TYP_UNS;  // signed/unsigned distinction only relevant for second instruction
    code3.dtype = type;

    // check if constant bigger than 32 bits
    if ((type & XPR_INT) && !(type & TYP_UNS) && (code1.value.i < (int32_t)0x80000000 || code1.value.i > (int32_t)0x7FFFFFFF)) return false;
    if ((type & XPR_INT) && (type & TYP_UNS) && (code1.value.u > (uint64_t)0xFFFFFFFF)) return false;
    if ((type & XPR_FLT) && (type & 0xFF) > TYP_FLOAT32) return false;

    switch (code1.instruction) {
    case II_ADD: case II_SUB:
        if (type & TYP_FLOAT) return false;
        if (code1.instruction == II_ADD && code1.value.u == 1 && code1.dest == code1.reg1 && !(type & TYP_UNS)) {
            // check if it fits increment_compare/jump above
            code3.value.u = code2.value.u;
            uint32_t nbits = (1 << (type & 7)) * 8;  // number of bits in constant
            if ((code2.instruction & 0xFFFE00) == II_JUMP_NEGATIVE && (code2.etype & XPR_INT) 
            && (code3.value.u & (((uint64_t)1 << nbits) - 1)) != (uint64_t)1 << (nbits - 1)) { // check for overflow
                // convert jump_sbelow n to jump_sbeloweq n-1
                // or jump_saboveeq n to jump sabove n-1
                code3.value.u--;
                code3.instruction ^= II_JUMP_NEGATIVE ^ II_JUMP_POSITIVE ^ II_JUMP_INVERT;
            }            
            if ((code3.instruction & 0xFFFE00) == II_JUMP_POSITIVE) {
                // successful combination of add 1 and jump_sbeloweq
                code3.instruction = (code3.instruction & 0xFFFF00) | II_INCREMENT;
                code3.etype = (code1.etype & ~XPR_IMMEDIATE) | code2.etype;
                codeBuffer.pop();        // remove previous code from buffer
                code2 = code3;
                return true;
            }
        }
        // add/sub + compare
        if (!(code2.etype & XPR_INT) || code2.value.i != 0 || (code2.instruction & 0xFF) != II_COMPARE) return false; // must compare against zero
        if ((type & TYP_UNS) && (code3.instruction & 0xFFFE00) != II_JUMP_ZERO) return false;  // unsigned works only for == and !=
        // successful combination of add/sub and signed compare with zero
        code3.instruction = code1.instruction | (code2.instruction & 0xFFFF00);
        code3.etype = code1.etype | (code2.etype & ~(XPR_IMMEDIATE | XPR_OPTIONS));
        codeBuffer.pop();        // remove previous code from buffer
        code2 = code3;
        return true;

    case II_AND: case II_OR: case II_XOR:
    case II_SHIFT_LEFT:  case II_SHIFT_RIGHT_U:
        // must compare for equal to zero
        if (!(code2.etype & XPR_INT) || code2.value.i != 0) return false;
        //if ((code2.instruction & 0xFFFE00) != II_JUMP_ZERO) return false;
        if ((code2.instruction & ~ II_JUMP_INVERT) != (II_JUMP_ZERO | II_COMPARE)) return false;
        // combine instruction codes
        code3.instruction = code1.instruction | (code2.instruction & 0xFFFF00);
        code3.etype = code1.etype | (code2.etype & ~XPR_IMMEDIATE);
        // successful combination of and/or/xor/shift and compare with zero
        codeBuffer.pop();        // remove previous code from buffer
        code2 = code3;
        return true;
    }
    // everything else fails
    return false;
}

// check if line contains unconditional direct jump and nothing else
uint32_t CAssembler::hasJump(uint32_t line) {
    if (cmd.optiLevel == 0) return 0;             // don't optimize jump if optimization is off
    if (line >= lines.numEntries()) return 0;     // there is no line here
    uint32_t tokB = lines[line].firstToken;
    uint32_t tokN = lines[line].numTokens;        // number of tokens in line
    if (tokN > 0 && tokens[tokB+tokN-1].type == TOK_OPR && tokens[tokB+tokN-1].id == ';') tokN--;  // ignore ';'
    lineError = false;
    if (tokN == 1 && tokens[tokB].type == TOK_HLL) {  // high level keyword
        if (tokens[tokB].id == HLL_BREAK || tokens[tokB].id == HLL_CONTINUE) {
            return findBreakTarget(tokens[tokB].id);  // break or continue statement
        }
    }
    if (tokN == 2 && tokens[tokB].type == TOK_INS && tokens[tokB].id == II_JUMP && tokens[tokB+1].type == TOK_SYM) {
        // jump symbol
       // return tokens[tokB+1].value.w;
        return tokens[tokB+1].id;
    }
    return 0;  // anything else
}

// interpret condition in if(), while(), and for(;;) statements
void CAssembler::interpretCondition(SCode & code) {
    if ((code.instruction & 0xFF) == II_COMPARE) {
        // compare instruction. condition is in option bits
        switch ((code.optionbits >> 1) & 3) {
        case 0:
            code.instruction |= II_JUMP_ZERO;  break;
        case 1:
            code.instruction |= (code.dtype & TYP_UNS) ? II_JUMP_CARRY : II_JUMP_NEGATIVE;  break;
        case 2:
            code.instruction |= (code.dtype & TYP_UNS) ? II_JUMP_UABOVE : II_JUMP_POSITIVE;  break;
        case 3:
            errors.reportLine(ERR_EXPECT_LOGICAL); // should not occur
        }
        if (code.optionbits & 1) code.instruction ^= II_JUMP_INVERT; // invert if bit 0    
    }
    else if ((code.instruction & 0xFF) == II_AND && (code.etype & XPR_INT)) {
        // bit test. if (r1 & 1 << n)
        if (code.value.u == 0 || (code.value.u & (code.value.u-1))) {
            errors.reportLine(ERR_MUST_BE_POW2);
        }
        code.instruction = II_TEST_BIT | II_JUMP_NOTZERO;
        code.value.u = bitScanReverse(code.value.u);
        if (code.optionbits & 4) code.instruction ^= II_JUMP_INVERT;
    }
    else {  // unrecognized expression
        errors.reportLine(ERR_EXPECT_LOGICAL);
        code.instruction = II_JUMP;
    }
    code.optionbits = 0;
}


// push registers on stack
void CAssembler::codePush() {   
    uint32_t state = 0;                     // 0: begin, 1: after 'push', 2: after '(', 3: after register, 4: after ',', 5: after ')'
    uint32_t reg1;                          // register
    uint32_t tok = 0;                       // token index
    SToken token;                           // token
    SCode code;                             // code structure
    memset(&code, 0, sizeof(code));
    code.section = section;
    // loop through tokens on line
    for (tok = tokenB; tok < tokenB + tokenN; tok++) {
        token = tokens[tok];
        switch (state) {
        case 0:               // begin. expect 'push'
            if (token.id == HLL_PUSH) state = 1;
            else if (token.type == TOK_TYP) state = 0;  // ignore type
            else errors.report(token);
            break;
        case 1:  // after 'push'. expect '('
            if (token.type == TOK_OPR && token.id == '(') state = 2;
            else errors.report(token);
            break;
        case 3:  // after register. expect ',' or ')'
            if (token.type == TOK_OPR && token.id == ',') state = 4;
            else if (token.type == TOK_OPR && token.id == ')') state = 5;
            else errors.report(token);
            break;
        case 5:  // after ')'. expect nothing
            errors.report(token);
        case 2: case 4:  // expect register
            if (token.type != TOK_REG) {
                errors.report(token); return;
            }
            state = 3;
            reg1 = token.id;
            if (reg1 & REG_R) {  // general purpose register. 
                // sp -= 8
                code.instruction = II_SUB;
                code.reg1 = code.dest = REG_R + 31; // sp
                code.value.i = 8;
                code.etype = XPR_INT | XPR_REG | XPR_REG1;
                code.dtype = TYP_INT64;
                checkCode1(code);
                fitCode(code); 
                if (lineError) return;     
                codeBuffer.push(code);  // save code
                // [sp] = reg
                code.instruction = II_STORE;
                code.reg1 = reg1;
                code.dest = 2;
                code.value.i = 0;
                code.base = REG_R + 31; // sp
                code.etype = XPR_REG | XPR_REG1 | XPR_MEM | XPR_BASE;
                checkCode1(code);
                fitCode(code); 
                if (lineError) return;     
                codeBuffer.push(code);  // save code 
            }
            else if (reg1 & REG_V) {  // vector register. 
                // sp = sub_cps(sp, reg)
                code.instruction = 0x2001D;
                code.reg1 = code.dest = REG_R + 31; // sp
                code.reg2 = reg1;
                code.etype = XPR_REG | XPR_REG1 | XPR_REG2;
                code.dtype = TYP_INT64;
                checkCode1(code);
                fitCode(code); 
                if (lineError) return;     
                codeBuffer.push(code);  // save code
                // [sp] = save_cp(reg)
                code.instruction = 0x2001F;
                code.reg1 = reg1;
                code.reg2 = 0;
                code.dest = 2;
                code.base =REG_R + 31; // sp 
                code.etype = XPR_REG | XPR_REG1 | XPR_MEM | XPR_BASE;
                checkCode1(code);
                fitCode(code); 
                if (lineError) return;     
                codeBuffer.push(code);  // save code 
            }
            else {  // other register type
                errors.report(token);
            }
        }
    }
    if (state != 5) errors.reportLine(ERR_UNFINISHED_INSTRUCTION);
}


// pop register from stack
void CAssembler::codePop() {
    uint32_t state = 0;                     // 0: begin, 1: after 'pop', 2: after '(', 3: after register, 4: after ',', 5: after ')'
    uint32_t reg1;                          // register
    uint32_t tok = 0;                       // token index
    SToken token;                           // token
    SCode code;                             // code structure
    memset(&code, 0, sizeof(code));
    code.section = section;
    // loop through tokens on line
    for (tok = tokenB; tok < tokenB + tokenN; tok++) {
        token = tokens[tok];
        switch (state) {
        case 0:               // begin. expect 'push'
            if (token.id == HLL_POP) state = 1;
            else if (token.type == TOK_TYP) state = 0;  // ignore type
            else errors.report(token);
            break;
        case 1:  // after 'push'. expect '('
            if (token.type == TOK_OPR && token.id == '(') state = 2;
            else errors.report(token);
            break;
        case 3:  // after register. expect ',' or ')'
            if (token.type == TOK_OPR && token.id == ',') state = 4;
            else if (token.type == TOK_OPR && token.id == ')') state = 5;
            else errors.report(token);
            break;
        case 5:  // after ')'. expect nothing
            errors.report(token);
        case 2: case 4:  // expect register
            if (token.type != TOK_REG) {
                errors.report(token); return;
            }
            state = 3;
            reg1 = token.id;
            if (reg1 & REG_R) {  // general purpose register. 
                // reg = [sp]
                code.instruction = II_MOVE;
                code.reg1 = code.reg2 = 0;
                code.dest = reg1;
                code.base = REG_R + 31; // sp
                code.value.i = 0;
                code.etype = XPR_MEM | XPR_BASE;
                code.dtype = TYP_INT64;
                checkCode1(code);
                fitCode(code);
                if (lineError) return;
                codeBuffer.push(code);  // save code 
                // sp += 8
                code.instruction = II_ADD;
                code.reg1 = code.dest = REG_R + 31; // sp
                code.base = 0;
                code.value.i = 8;
                code.etype = XPR_INT | XPR_REG | XPR_REG1;
                checkCode1(code);
                fitCode(code);
                if (lineError) return;
                codeBuffer.push(code);  // save code
            }
            else if (reg1 & REG_V) {  // vector register. 
                // reg = restore_cp([sp])
                code.instruction = 0x2001E;
                code.reg1 = code.reg2 = 0;
                code.dest = reg1;
                code.base = REG_R + 31; // sp 
                code.value.i = 0;
                code.etype = XPR_MEM | XPR_BASE;
                checkCode1(code);
                fitCode(code);
                if (lineError) return;
                codeBuffer.push(code);  // save code
                // sp = add_cps(sp, reg)
                code.instruction = 0x2001C;
                code.reg1 = code.dest = REG_R + 31; // sp
                code.reg2 = reg1;
                code.etype = XPR_REG | XPR_REG1 | XPR_REG2;
                code.dtype = TYP_INT64;
                checkCode1(code);
                fitCode(code);
                if (lineError) return;
                codeBuffer.push(code);  // save code
            }
        }
    }
    if (state != 5) errors.reportLine(ERR_UNFINISHED_INSTRUCTION);
}

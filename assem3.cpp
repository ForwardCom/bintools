/****************************    assem3.cpp    ********************************
* Author:        Agner Fog
* Date created:  2017-04-17
* Last modified: 2020-05-17
* Version:       1.13
* Project:       Binary tools for ForwardCom instruction set
* Module:        assem.cpp
* Description:
* Module for assembling ForwardCom .as files. 
* This module contains:
* Assemble-time variable assignments and metaprogramming features,
* Copyright 2017-2024 GNU General Public License http://www.gnu.org/licenses
******************************************************************************/
#include "stdafx.h"

// Replace meta variables defined in previous '%' line
void CAssembler::replaceKnownNames() {
    // loop through tokens, replace known symbol names by reference to symbol records
    // and replace assemble-time variables by their current value
    uint32_t tok;                                // token index
    int32_t symi;                                // symbol index
    for (tok = tokenB; tok < tokenB + tokenN; tok++) {
        if (lineError) break; 
        if (tokens[tok].type == TOK_NAM) {
            // name found. search for it in symbol table
            symi = findSymbol((char*)buf() + tokens[tok].pos, tokens[tok].stringLength);
            if (symi > 0) {  // symbol found. replace token by reference to symbol                
                tokens[tok].id = symbols[symi].st_name;  // use name offset as unique identifier because symbol index can change
                if (symbols[symi].st_type == STT_EXPRESSION) {
                    tokens[tok].type = TOK_XPR;
                    // save value of meta variable in token in case it changes later
                    tokens[tok].value.u = symbols[symi].st_value;
                }
                else if (symbols[symi].st_type == STT_TYPENAME) {    // symbol is an alias for a type name
                    if (tokens[tokenB].id != '%' || tok != tokenB + 1) { // replace it unless it comes immediately after %, which means it is redefined
                        tokens[tok].type = TOK_TYP; 
                        tokens[tok].value.u = symbols[symi].st_value;
                        tokens[tok].id = (uint32_t)symbols[symi].st_value;
                    }
                }
                else {
                    tokens[tok].type = TOK_SYM;
                    if ((symbols[symi].st_type & ~1) == STT_CONSTANT) {
                        // save value of meta variable in token in case it changes later
                        tokens[tok].value.u = symbols[symi].st_value;
                        tokens[tok].vartype = 3;
                        if (symbols[symi].st_other & STV_FLOAT) {
                            tokens[tok].vartype = 5;
                        }
                    }
                }
            }
        } /*  not needed because meta code cannot have forward references, except in public directives which are handled elsewhere
        else if (tokens[tok].type == TOK_SYM) {
            symi = findSymbol(tokens[tok].value.w);
            if ((symbols[symi].st_type & ~1) == STT_CONSTANT) {
                // save value of meta variable in token in case it changes later
                tokens[tok].value.u = symbols[symi].st_value;
                tokens[tok].vartype = symbols[symi].st_reguse1;
            }
        } */
    }
}


// Interpret line beginning with '%' containing meta code
void CAssembler::interpretMetaDefinition() {
    uint32_t tok;                                // token index
    int32_t symi = 0;                            // symbol index
    ElfFWC_Sym2 sym;                             // symbol record
    zeroAllMembers(sym);                         // reset symbol
    //uint32_t tokmeta = 0;                        // token containing '%'

    // interpret line defining assemble-time variable
    uint32_t state = 0;   // state during definition of assemble-time variable:
                          // 1: after '%'
                          // 2: after type name
                          // 3: after variable name
                          // 4: after '=' or '+=', etc.
                          // 5: finished assignment
    uint32_t toktyp = 0;  // token containing type definition
    uint32_t tokop = 0;   // token containing ++ or -- operator
    uint32_t type = 3;    // default type is int64
    SExpression exps, expr;
    zeroAllMembers(expr);

    lineError = false;
    for (tok = tokenB; tok < tokenB + tokenN; tok++) {
        if (lineError)  break;

        if (state == 4) {
            // after assignment operator, expecting expression next
            expr = expression(tok, tokenB + tokenN - tok, 0);
            if (tok + expr.tokens < tokenB + tokenN) {
                errors.report(tokens[tok + expr.tokens]);  // extra tokens on line
            }
            exps = symbol2expression(symi); // make expression out of symbol
            if (lineError) break;

            // combine expressions
            switch (tokens[tok - 1].id) {
            case '=':  break;         // assign expr
            case '+' + EQ:            // +=
                expr = op2('+', exps, expr);  break;
            case '-' + EQ:            // -=
                expr = op2('-', exps, expr);  break;
            case '*' + EQ:            // *=
                expr = op2('*', exps, expr);  break;
            case '/' + EQ:            // /=
                expr = op2('/', exps, expr);  break;
            case '&' + EQ:            // &=
                expr = op2('&', exps, expr);  break;
            case '|' + EQ:            // |=
                expr = op2('|', exps, expr);  break;
            case '^' + EQ:            // ^=
                expr = op2('^', exps, expr);  break;
            case '<' + D2 + EQ:            // <<=
                expr = op2('<' + D2, exps, expr);  break;
            case '>' + D2 + EQ:            // >>=
                expr = op2('>' + D2, exps, expr);  break;
            case '>' + D3 + EQ:            // >>>=
                expr = op2('>' + D3, exps, expr);  break;
            default:
                errors.report(tokens[tok - 1].pos, tokens[tok - 1].stringLength, ERR_WRONG_TYPE);
            }
            if (expr.etype == uint32_t(XPR_ERROR)) {
                errors.report(tokens[tok - 1].pos, tokens[tok - 1].stringLength, expr.value.w);
            }
            else assignMetaVariable(symi, expr, toktyp);
            if (lineError) continue;
            state = 5;
            break;
        }
        if (state == 5) {
            errors.report(tokens[tok]);    // extra tokens on line
            return;
        }
        switch (tokens[tok].type) {
        case TOK_OPR:  // operator
            if (tokens[tok].id == '%' && state == 0) {
                state = 1;
            }
            else if (tokens[tok].priority == 15 && state == 3) state = 4; // assignment operator
            else if (tokens[tok].priority == 3) { // ++ or -- operator
                tokop = tok;
                if (state < 3) break;
                if (state == 3) {
                PLUSPLUSOPERATOR:
                    exps = symbol2expression(symi); // make expression out of symbol
                    expr.etype = XPR_INT;
                    expr.value.i = 1;
                    expr.tokens = 0;
                    switch (tokens[tokop].id) {
                    case '+' + D2:            // ++
                        expr = op2('+', exps, expr);  break;
                    case '-' + D2:            // --
                        expr = op2('-', exps, expr);  break;
                    default:
                        errors.report(tokens[tokop]);
                    }
                    if (expr.etype & XPR_ERROR) {
                        errors.report(tokens[tok - 1].pos, tokens[tok - 1].stringLength, expr.value.w);
                    }
                    else assignMetaVariable(symi, expr, toktyp);
                    lines[linei].type = LINE_METADEF;
                    state = 5;
                }
            }
            else tok = tokenB + tokenN;   // anything else. exit loop
            break;
        case TOK_TYP:  // type name
            toktyp = tok;  type = tokens[tok].vartype;
            if (state == 1) state = 2;
            break;
        case TOK_NAM:  // new name. define symbol
            if (state == 0) break;
            if (state >= 3) { errors.report(tokens[tok]);  break; }
            sym.st_name = symbolNameBuffer.putStringN((char*)buf() + tokens[tok].pos, tokens[tok].stringLength);
            symi = symbols.addUnique(sym);
            symbols[symi].st_type = 0;  // remember that symbol has no value yet
            symbols[symi].st_section = SECTION_LOCAL_VAR;  // remember symbol is not external. use arbitrary section
            symbols[symi].st_unitsize = 8;
            symbols[symi].st_unitnum = 1;
            tokens[tok].type = TOK_SYM;  // change token type
            tokens[tok].id = symbols[symi].st_name;
            tokens[tok].vartype = type;
            //if (state == 8) goto PLUSPLUSOPERATOR;
            state = 3;
            break;
        case TOK_SYM: case TOK_XPR: // existing symbol found
            if (state != 1 && state != 2) break;
            symi = findSymbol(tokens[tok].id);
            if (symi < 1) errors.report(tokens[tok]);    // unknown error
            if ((symbols[symi].st_type & ~1) == STT_CONSTANT) {
                symbols[symi].st_type = STT_VARIABLE;  // remember symbol has been modified
                if (tokop && tokop == tok - 1 && state < 3) goto PLUSPLUSOPERATOR;
            }
            state = 3;
            break;
        default:  // anything else. exit loop
            tok = tokenB + tokenN;
        }
    }
    // insert metaprogramming branches, loops and functions here??
}


// define or modify assemble-time constant or variable
void CAssembler::assignMetaVariable(uint32_t symi, SExpression & expr, uint32_t typetoken) {
    // get value and type from expression 

    symbols[symi].st_value = expr.value.u;
    uint32_t type = XPR_INT;
    // set variable type
    switch (expr.etype & 0xF) {
    case XPR_FLT:
        symbols[symi].st_other = STV_FLOAT;
        type = XPR_FLT;
        break;
    case XPR_STRING:
        symbols[symi].st_other = STV_STRING;
        symbols[symi].st_unitsize = 1;
        symbols[symi].st_unitnum = expr.sym2;    // string length
        type = XPR_STRING;
        break;
    default: 
        symbols[symi].st_other = 0;
    }
    if (expr.etype & XPR_TYPENAME) symbols[symi].st_type = STT_TYPENAME;
    else if (symbols[symi].st_type == 0) symbols[symi].st_type = STT_CONSTANT;  // first time: make a constant
    else symbols[symi].st_type = STT_VARIABLE;                             // reassigned later: make a variable
    if (expr.etype & (XPR_REG | XPR_MEM)) {
        symbols[symi].st_type = STT_EXPRESSION;
        symbols[symi].st_value = expressions.push(expr);                         // save expression
    }

    // check expression type
    if (expr.etype & (XPR_OP | XPR_OPTION | XPR_SYMSCALE | XPR_MASK)) {
        errors.reportLine(ERR_WRONG_TYPE_VAR);
        return;
    }
    if ((expr.etype & (XPR_SYM1 | XPR_SYM2)) == XPR_SYM1 && !(expr.etype & XPR_MEM)) {
        // single symbol. must be constant or memory
        int32_t symi1 = findSymbol(expr.sym1);
        if (symi1 <= 0 || (!(symbols[symi1].st_type & STT_CONSTANT))) {
            errors.reportLine(ERR_WRONG_TYPE_VAR);
            return;
        }
    }
    // check if type matches
    if (typetoken == 0 || type == (tokens[typetoken].id & 0xF)) return;       // type matches
    if ((tokens[typetoken].id & 0xF) == XPR_FLT && type == XPR_INT) {
        // convert int to double
        expr.value.d = (double)expr.value.i;
        symbols[symi].st_value = expr.value.u;
        symbols[symi].st_other = STV_FLOAT;
        return;
    }

    errors.reportLine(ERR_WRONG_TYPE_VAR);
}

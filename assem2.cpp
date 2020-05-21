/****************************    assem2.cpp    ********************************
* Author:        Agner Fog
* Date created:  2017-04-17
* Last modified: 2020-05-17
* Version:       1.10
* Project:       Binary tools for ForwardCom instruction set
* Module:        assem.cpp
* Description:
* Module for assembling ForwardCom .as files. 
* This module contains:
* - expression(): Interpretation of expressions containing operators and 
*                 any type of operands.
* Copyright 2017-2020 GNU General Public License http://www.gnu.org/licenses
******************************************************************************/
#include "stdafx.h"


// Interpret and evaluate expre-ssion
SExpression CAssembler::expression(uint32_t tok1, uint32_t maxtok, uint32_t options) {
    // tok1: index to first token, 
    // maxtok: maximum number of tokens to use, 
    // options: 0: normal, 
    // 1: unsigned
    // 2: inside []. interpret as memory operand
    // 4: interpret option = keyword
    // 8: inside {}. has no meaning yet
    // 0x10: check syntax and count tokens, but do not call functions or report numeric 
    //       overflow, wrong operand types, or unknown names

    // This function scans the tokens and finds the operator with lowest priority. 
    // The function is called recursively for each operand to this operator.
    // The level of parantheses is saved in the brackets stack.
    // The scanning terminates at any of these conditions:
    // * a token that cannot be part of the expression is encountered
    // * all tokens are used
    // * a comma is encountered
    // * an unmatched end bracket is encountered

    uint32_t tok;                 // current token
    uint32_t toklow = tok1;       // operator with lowest priority
    uint32_t tokcolon = 0;        // matching triadic operator with lowest priority
    uint32_t ntok = 0;            // number of tokens used
    uint32_t priority = 0;        // priority of this operator
    uint32_t bracketlevel = 0;    // number of brackets in stack
    uint32_t state = 0;           // 0: expecting value, 1: after value, expecting operator or end
    uint32_t i;                   // loop counter
    uint32_t tokid;               // token.id
    int32_t  symi;                // symbol index
    uint8_t endbracket;           // expected end bracket

    SExpression exp1, exp2;       // expressions during evaluation
    zeroAllMembers(exp1);         // reset exp1
    exp1.tokens = 1;

    for (tok = tok1; tok < tok1 + maxtok; tok++) {
        if (lineError) {exp1.etype = 0;  return exp1;}
        if (tokens[tok].type == TOK_OPR) {
            // operator found. search for brackets
            if (tokens[tok].priority == 1 || tokens[tok].priority == 14) {
                // bracket found. ?: operator treated as bracket here                
                switch (tokens[tok].id) {
                case '?':
                    if (tokens[tok].priority > priority && bracketlevel == 0) {  // if multiple ?:, split by the last one
                        priority = tokens[tok].priority; toklow = tok;
                    }
                    // continue in next case
                case '(': case '[': case '{':  // opening bracket. push on bracket stack
                    brackets.push(uint8_t(tokens[tok].id));
                    bracketlevel++;
                    state = 0;
                    break;
                case ')': case ']': case '}': case ':': // closing bracket
                    if (bracketlevel == 0) {                        
                        goto EXIT_LOOP;     // this end bracket is not part of the expression.
                    }
                    // remove matching opening bracket from stack
                    bracketlevel--;
                    endbracket = brackets.pop();
                    switch (endbracket) {
                    case '(':  endbracket = ')';  break;
                    case '[':  endbracket = ']';  break;
                    case '{':  endbracket = '}';  break;
                    case '?':  endbracket = ':';  break;
                    }
                    if (endbracket != tokens[tok].id) {
                        // end bracket does not match begin bracket
                        errors.report(tokens[tok].pos, tokens[tok].stringLength, ERR_BRACKET_END);
                        goto EXIT_LOOP;
                    }
                    if (tokens[tok].id == ':') {
                        if (bracketlevel == 0 && priority == 14 && tokcolon == 0) {
                            tokcolon = tok; // ':' matches current '?' with lowest priority
                        }
                        state = 0;
                        continue;
                    }
                    state = 1;
                    continue;  // finished with this token                
                }
            }
            if (bracketlevel) continue;  // don't search for priority inside brackets

            if (state == 1) {
                // expecting operator
                if (tokens[tok].id == ';') break;  // end at semicolon
                if (tokens[tok].id == ',' && !(options & 2)) break;  // end at comma, except inside []
                if (tokens[tok].id == '=' && !(options & 6)) break;  // end at =, except inside [] or when interpreting option = value
                
                if (tokens[tok].priority >= priority) {  // if multiple operators with same priority, split by the last one to get the first evaluated first
                    // operator with lower priority found
                    priority = tokens[tok].priority;
                    toklow = tok;
                }
                if (tokens[tok].priority == 3) state = 1; else state = 0;  // state 0 except after monadic operator
            }
            else if (state == 0 && (tokens[tok].id == '-' || tokens[tok].id == '+' || tokens[tok].priority == 3) && priority < 3) {
                // monadic operator
                priority = 3;  toklow = tok;
            }
            else {
                break;  // unexpected operator. end here
            }
        }
        else {
            // not an operator
            if (bracketlevel) continue;  // inside brackets: search only for end bracket
            if (state == 0) {
                // expecting value
                switch (tokens[tok].type) {
                case TOK_NAM: case TOK_LAB: case TOK_VAR: case TOK_SEC: 
                case TOK_NUM: case TOK_FLT: case TOK_CHA: case TOK_STR:
                case TOK_REG: case TOK_SYM: case TOK_XPR: case TOK_OPT:
                    state = 1; // allowed value tokens
                    break;
                case TOK_TYP:
                    state = 1; // type expression
                    break;
                default:
                    errors.report(tokens[tok]);  break;
                }
            }
            else {                
                break;    // no operator found after value. end here
            }
        }
    }
    EXIT_LOOP:
    if (lineError) {exp1.etype = 0;  return exp1;}
    // number of tokens used
    ntok = tok - tok1;
    exp1.tokens = ntok;
    if (bracketlevel) {
        endbracket = brackets.pop();
        errors.report(tokens[tok1].pos, tokens[tok].pos - tokens[tok1].pos, endbracket == '?' ? ERR_QUESTION_MARK : ERR_BRACKET_BEGIN);
        if (exp1.etype == 0) exp1.etype = XPR_INT;
        return exp1;
    }
    if (ntok == 0) {  // no expression found
        if (maxtok == 0 && tok > 0) tok--;
        errors.report(tokens[tok].pos, tokens[tok].stringLength, ERR_MISSING_EXPR);
        return exp1;
    }

    switch (priority) {
    case 0:  // no operator found. just an expression
        if (ntok > 2 && tokens[tok1].type == TOK_OPR && tokens[tok1].priority == 1) {
            // this is an expression in brackets
            uint32_t option1 = options;
            if (tokens[tok1].id == '[') {
                if (options & 2) errors.report(tokens[tok1]); // nested [[]] not allowed
                option1 |= 2;
            }
            if (tokens[tok1].id == '{') option1 |= 8;
            // evaluate expression inside bracket
            exp1 = expression(tok1 + 1, ntok - 2, option1);
            exp1.tokens += 2;
            goto RETURNEXP1;
        }
        else if (ntok == 1) {
            // this is a single token. get value
            switch (tokens[tok1].type) {
            case TOK_LAB: case TOK_VAR: case TOK_SEC: case TOK_SYM:
                exp1.etype = XPR_SYM1;            // symbol address
                //exp1.sym1 = tokens[tok1].value.w;
                exp1.sym1 = tokens[tok1].id;
                // get symbol value if possible
                if (options & 2) exp1.etype |= XPR_MEM;
                symi = findSymbol(exp1.sym1);
                if (symi > 0 && symbols[symi].st_bind == STB_LOCAL && (symbols[symi].st_type == STT_CONSTANT || symbols[symi].st_type == STT_VARIABLE)) {
                    exp1.etype = XPR_INT;
                    exp1.value.i = tokens[tok1].value.u;   // don't take value from symbol, it may change
                    exp1.sym1 = 0;  // symbol expression converted to constant expression
                    if (symbols[symi].st_other & STV_FLOAT) exp1.etype = XPR_FLT;
                    if (symbols[symi].st_other & STV_STRING) {
                        exp1.etype = XPR_STRING; exp1.sym2 = (uint32_t)symbols[symi].st_unitnum;
                    }
                    if ((options & 2) && (exp1.etype & (XPR_FLT | XPR_STRING))) { // float or string not allowed in memory operand
                        errors.report(tokens[tok1].pos, tokens[tok1].stringLength, ERR_WRONG_TYPE);
                    }
                    break;
                }
                break;
            case TOK_NUM: 
                exp1.etype = XPR_INT;  // integer value
                exp1.value.i = interpretNumber((char*)buf()+tokens[tok1].pos, tokens[tok1].stringLength, &i);
                if (i) errors.report(tokens[tok1]);
                break;
            case TOK_FLT:
                exp1.etype = XPR_FLT;  // floating point value
                exp1.value.d = interpretFloat((char*)buf()+tokens[tok1].pos, tokens[tok1].stringLength);
                if (options & 2) {   // float not allowed in memory operand
                    errors.report(tokens[tok1].pos, tokens[tok1].stringLength, ERR_WRONG_TYPE);
                }
                break;
            case TOK_CHA:            // character(s). convert to integer
                exp1.etype = XPR_INT; 
                exp1.value.u = 0;
                for (i = 0; i < tokens[tok1].stringLength; i++) {
                    exp1.value.u += (uint64_t)(uint8_t)buf()[tokens[tok1].pos+i] << (i*8);
                }
                break;
            case TOK_STR:  {          // string
                exp1.etype = XPR_STRING;
                exp1.value.u = stringBuffer.dataSize();    // save position of string
                exp1.sym2 = tokens[tok1].stringLength;     // string length
                bool escape = false;                       // check for \ escape characters
                for (i = 0; i < tokens[tok1].stringLength; i++) {
                    char c = get<char>(tokens[tok1].pos + i);
                    if (c == '\\' && !escape) {
                        escape = true; continue;           // escape next character
                    }
                    if (escape) {   // special escape characters
                        switch (c) {
                        case '\\':  escape = false;  break;
                        case 'n':  c = '\n';  break;
                        case 'r':  c = '\r';  break;
                        case 't':  c = '\t';  break;
                        }
                    }
                    if (escape && exp1.sym2) exp1.sym2--;  // reduce length
                    stringBuffer.put(c);
                    escape = false;
                }
                stringBuffer.put(char(0));                 // terminate string
                if (options & 2) {                         // string not allowed in memory operand
                    errors.report(tokens[tok1].pos, tokens[tok1].stringLength, ERR_WRONG_TYPE);
                }
                break;}
            case TOK_REG:
                if (options & 2) { // register inside [] is base register
                    exp1.etype = XPR_BASE | XPR_MEM;
                    exp1.base = tokens[tok1].id;
                }
                else {             // normal register operand
                    exp1.etype = XPR_REG | XPR_REG1;
                    exp1.reg1 = tokens[tok1].id;
                }
                break;
            case TOK_NAM:
                if ((options & 0x10) == 0) errors.report(tokens[tok1]);
                exp1.etype |= XPR_UNRESOLV;        // unresolved name
                break;
            case TOK_OPT:
                exp1.etype = XPR_OPTION;
                if ((tokens[tok1].id) == OPT_SCALAR) {
                    exp1.etype |= XPR_SCALAR;
                }
                else {
                    exp1.value.u = tokens[tok1].id;
                }
                break;
            case TOK_XPR:  // expression
                if (tokens[tok1].value.u < expressions.numEntries()) {
                    exp1 = expressions[tokens[tok1].value.w];
                    exp1.tokens = ntok;
                    if ((exp1.etype & XPR_REG) && !(exp1.etype & XPR_MEM) && (options & 2)) { // register inside [] is base register
                        exp1.etype = XPR_BASE | XPR_MEM;
                        exp1.base = exp1.reg1;
                        exp1.reg1 = 0;
                    }
                }
                else errors.report(tokens[tok1]);
                break;
            case TOK_TYP:
                exp1.etype = XPR_TYPENAME;
                exp1.value.u = tokens[tok1].id;
                break;
            default:
                errors.report(tokens[tok1]);
            }
            if (options & 2) exp1.etype |= XPR_MEM;  // inside [], interpret as memory operand
            goto RETURNEXP1;
        }
        else {
            // unrecognized token
            errors.report(tokens[tok1]);
        }
        break;

    case 3:  // monadic operator
        if (toklow == tok1) {  // operator comes first
            exp1 = expression(toklow + 1, maxtok - 1, options);   // evaluate the rest
            if (exp1.etype & XPR_UNRESOLV) {
                exp1.tokens++;   // unresolved expression. return unresolved result
                goto RETURNEXP1;
            }
            zeroAllMembers(exp2);  // zero exp2
            switch (tokens[toklow].id) {
            case '+':            // value is unchanged
                exp1.tokens++;
                goto RETURNEXP1;;  // value is unchanged
            case '-':
                if (exp1.etype & (XPR_OP | XPR_REG | XPR_MEM)) {
                    exp1 = op1minus(exp1); // convert -(A+B) etc.
                    goto RETURNEXP1;
                }
                exp2 = exp1;      // convert -A to 0-A
                exp1.tokens = 0;
                exp1.etype = XPR_INT;
                exp1.value.i = 0;
                tokid = '-';
                break;  // continue in dyadic operators with 0-exp2
            case '!':
                exp1.tokens++;
                if (exp1.instruction == II_COMPARE
                && (exp1.etype & XPR_REG1) && (exp1.etype & (XPR_REG2 | XPR_INT | XPR_IMMEDIATE))) {
                    // compare instruction. invert condition
                    exp1.optionbits ^= 1;
                    if ((exp1.reg1 & REG_V) && (dataType & TYP_FLOAT)) exp1.optionbits ^= 8; // floating point compare. invert gives unordered
                    goto RETURNEXP1;
                }
                if (exp1.instruction == II_AND
                && (exp1.etype & XPR_REG1) && (exp1.etype & XPR_INT)) {
                    // test_bit/jump instruction. invert condition
                    exp1.optionbits ^= 4;
                    goto RETURNEXP1;
                }
                if (exp1.etype & (XPR_MEM | XPR_REG)) { // '!' ambiguous on register and memory operands
                    errors.report(tokens[toklow].pos, tokens[toklow].stringLength, ERR_NOT_OP_AMBIGUOUS);
                }
                exp2.tokens = 0;
                exp2.etype = XPR_INT;
                exp2.value.i = 0;
                tokid = '='+D2;
                break;  // continue in dyadic operators with exp1 == 0
            case '~':
                exp2.tokens = 0;
                exp2.etype = XPR_INT;
                exp2.value.i = -1;
                tokid = '^';
                break;  // continue in dyadic operators with exp1 ^ -1
            default:
                errors.report(tokens[tok1]);  // ++ and -- not supported in expression
                return exp1;
            }
            goto DYADIC;   // proceed to dyadic operator
        }
        else {  // postfix ++ and --
            errors.report(tokens[tok1+1]);  // ++ and -- not supported in expression
        }
        goto RETURNEXP1;

    case 14: // triadic operator ?:
        // evaluate exp1 ? exp2 : exp3 for all expression types
        return op3(tok1, toklow, tokcolon, maxtok, options);

    default:; // continue below for dyadic operator
    }
    // dyadic operator. evaluate two subexpressions
    exp1 = expression(tok1, toklow - tok1, options);  // evaluate fist expression
    if (exp1.tokens != toklow - tok1) errors.report(tokens[tok1 + exp1.tokens]);
    if (lineError) return exp1;

    exp2 = expression(toklow + 1, tok1 + maxtok - (toklow + 1), options);  // evaluate second expression
    ntok = toklow - tok1 + 1 + exp2.tokens;
    tokid = tokens[toklow].id;  // operator id
    if (lineError) return exp1;

    DYADIC:
    exp1 = op2(tokid, exp1, exp2);
    RETURNEXP1:
    if (lineError) return exp1;
    if (exp1.etype & XPR_ERROR) {
        errors.report(tokens[toklow].pos, tokens[toklow].stringLength, exp1.value.w);
    }
    return exp1;
}

// Interpret dyadic expression with any type of operands
SExpression CAssembler::op2(uint32_t op, SExpression & exp1, SExpression & exp2) {
    if ((exp1.etype | exp2.etype) & XPR_UNRESOLV) {
        exp1.etype = XPR_UNRESOLV;  // unresolved operand. make unresolved result
        exp1.tokens += exp2.tokens + 1;
    }
    else if ((exp1.etype & exp2.etype & XPR_MEM) && 
        ((exp1.etype|exp2.etype) & (XPR_BASE|XPR_INDEX|XPR_OPTION|XPR_SYM1|XPR_SYM2|XPR_LIMIT|XPR_LENGTH|XPR_BROADC))) {
        exp1 = op2Memory(op, exp1, exp2);                // generation of memory operand. both operands inside [] and contain not only constants
    }
    else if (exp1.etype == XPR_OPTION && op == '=') {
        // option = value is handled by op2Memory
        exp1 = op2Memory(op, exp1, exp2);
    }
    // generation of instruction involving registers and/or memory operand:
    // (don't rely on XPR_MEM flag here because we would catch expressions involving constants only inside [] )
    else if ((exp1.etype | exp2.etype) & (XPR_REG | XPR_BASE /*| XPR_SYM1*/)) {
        exp1 = op2Registers(op, exp1, exp2);                
    }
    else if ((exp1.etype | exp2.etype) & XPR_STRING) {
        exp1 = op2String(op, exp1, exp2);             // string operation
    }
    else if ((exp1.etype & 0xF) == XPR_FLT || (exp2.etype & 0xF) == XPR_FLT) {
        // dyadic operators for float                 // floating point operation
        exp1 = op2Float(op, exp1, exp2);
    }
    else if ((exp1.etype | exp2.etype) & XPR_SYM1) {
        // adding or subtracting symbols and integers
        exp1 = op2Memory(op, exp1, exp2);
    }
    else if ((exp1.etype & 0xF) == XPR_INT && (exp2.etype & 0xF) == XPR_INT) {
        // dyadic operators for integers              // integer operation
        exp1 = op2Int(op, exp1, exp2);
    }
    else {
        // other types
        exp1.etype = XPR_ERROR;
        exp1.value.u = ERR_WRONG_TYPE;
    }
    return exp1;
}

// Interpret dyadic expression with integer operands
SExpression CAssembler::op2Int(uint32_t op, SExpression const & exp1, SExpression const & exp2) {
    SExpression expr = exp1;
    expr.tokens = exp1.tokens + exp2.tokens + 1;
    switch (op & ~OP_UNS) {
    case '+':
        expr.value.u = exp1.value.u + exp2.value.u;
        break;
    case '-':
        expr.value.u = exp1.value.u - exp2.value.u;
        break;
    case '*':
        expr.value.i = exp1.value.i * exp2.value.i;
        break;
    case '/':
        if (exp2.value.i == 0) {
            expr.etype |= XPR_ERROR;
            expr.value.u = ERR_OVERFLOW;
            break;
        }
        if (op & OP_UNS) expr.value.u = exp1.value.u / exp2.value.u; // unsigned division
        else  expr.value.i = exp1.value.i / exp2.value.i; // signed division
        break;
    case '%':
        if (exp2.value.i == 0) {
            expr.etype |= XPR_ERROR;
            expr.value.u = ERR_OVERFLOW;
            break;
        }
        if (op & OP_UNS) expr.value.u = exp1.value.u % exp2.value.u; // unsigned modulo
        else  expr.value.i = exp1.value.i % exp2.value.i; // signed modulo
        break;
    case '<' + D2:  // <<
        expr.value.u = exp1.value.u << exp2.value.u;
        break;
    case '>' + D2:  // >>  shift right signed
        if (op & OP_UNS) expr.value.u = exp1.value.u >> exp2.value.u; // unsigned shift right
        else  expr.value.i = exp1.value.i >> exp2.value.i; // signed shift right
        break;
    case '>' + D3:  // >>> unsigned shift right
        expr.value.u = exp1.value.u >> exp2.value.u;
        break;
    case '<':    // < compare
        if (op & OP_UNS) expr.value.i = exp1.value.u < exp2.value.u; // unsigned compare
        else  expr.value.i = exp1.value.i < exp2.value.i; // signed compare
        break;
    case '<' + EQ:  // <=  compare
        if (op & OP_UNS) expr.value.i = exp1.value.u <= exp2.value.u; // unsigned compare
        else  expr.value.i = exp1.value.i <= exp2.value.i; // signed compare
        break;
    case '>':    // > compare
        if (op & OP_UNS) expr.value.i = exp1.value.u > exp2.value.u; // unsigned compare
        else  expr.value.i = exp1.value.i > exp2.value.i; // signed compare
        break;
    case '>' + EQ:   // >= compare
        if (op & OP_UNS) expr.value.i = exp1.value.u >= exp2.value.u; // unsigned compare
        else  expr.value.i = exp1.value.i >= exp2.value.i; // signed compare
        break;
    case '=' + D2:   // ==
        expr.value.u = exp1.value.u == exp2.value.u;
        break;
    case '!' + EQ:   // !=
        expr.value.u = exp1.value.u != exp2.value.u;
        break;
    case '&':  // bitwise and
        expr.value.u = exp1.value.u & exp2.value.u;
        break;
    case '|':  // bitwise or
        expr.value.u = exp1.value.u | exp2.value.u;
        break;
    case '^':  // bitwise xor
        expr.value.u = exp1.value.u ^ exp2.value.u;
        break;
    case '&' + D2:  // logical and
        expr.value.u = exp1.value.u && exp2.value.u;
        break;
    case '|' + D2:  // logical or
        expr.value.u = exp1.value.u || exp2.value.u;
        break;
    default:  // unsupported operator
        expr.etype |= XPR_ERROR;
        expr.value.u = ERR_WRONG_TYPE;
    }
    return expr;
}

// Interpret dyadic expression with floating point operands
SExpression CAssembler::op2Float(uint32_t op, SExpression & exp1, SExpression & exp2) {
    SExpression expr = exp1;
    expr.tokens = exp1.tokens + exp2.tokens + 1;
    if (exp1.etype == XPR_INT) {  // convert exp1 to float
        exp1.value.d = (double)exp1.value.i;
        expr.etype = XPR_FLT;
    }
    if (exp2.etype == XPR_INT) {  // convert exp2 to float
        exp2.value.d = (double)exp2.value.i;
        expr.etype = XPR_FLT;
    }
    // dyadic operator on float
    switch (op) {
    case '+':
        expr.value.d = exp1.value.d + exp2.value.d;
        break;
    case '-':
        expr.value.d = exp1.value.d - exp2.value.d;
        break;
    case '*':
        expr.value.d = exp1.value.d * exp2.value.d;
        break;
    case '/':
        if (exp2.value.d == 0.) {
            expr.etype |= XPR_ERROR;
            expr.value.u = ERR_OVERFLOW;
            break;
        }
        expr.value.d = exp1.value.d / exp2.value.d;
        break;
    case '<':    // signed compare
        expr.value.i = exp1.value.d < exp2.value.d;
        expr.etype = XPR_INT;
        break;
    case '<' + EQ:  // <= signed compare
        expr.value.i = exp1.value.d <= exp2.value.d;
        expr.etype = XPR_INT;
        break;
    case '>':    // signed compare
        expr.value.i = exp1.value.d > exp2.value.d;
        expr.etype = XPR_INT;
        break;
    case '>' + EQ:   // >= signed compare
        expr.value.i = exp1.value.d <= exp2.value.d;
        expr.etype = XPR_INT;
        break;
    case '=' + D2:   // ==
        expr.value.i = exp1.value.d == exp2.value.d;
        expr.etype = XPR_INT;
        break;
    case '!' + EQ:   // !=
        expr.value.i = exp1.value.d != exp2.value.d;
        expr.etype = XPR_INT;
        break;
    case '&' + D2:  // logical and
        expr.value.i = exp1.value.d != 0. && exp2.value.d != 0.;
        expr.etype = XPR_INT;  break;
        break;
    case '|' + D2:  // logical or
        expr.value.i = exp1.value.d != 0. || exp2.value.d != 0.;
        expr.etype = XPR_INT;  break;
        break;
    default:  // unsupported operator
        expr.etype |= XPR_ERROR;
        expr.value.u = ERR_WRONG_TYPE;
    }
    return expr;
}

// Interpret dyadic expression with register or memory operands, generating instruction
SExpression CAssembler::op2Registers(uint32_t op, SExpression const & ex1, SExpression const & ex2) {
    SExpression expr = {0};           // return expression
    uint8_t swapped = false;          // operands are swapped
    uint8_t cannotSwap = false;       // cannot swap operands because both contain vector registers
    uint32_t i;                       // loop counter

                                      // make array of the two expressions
    SExpression exp12[2];             // copy of expressions
    uint32_t numtokens = ex1.tokens + ex2.tokens + 1; // number of tokens
    expr.tokens = numtokens;

    if (ex1.etype & ex2.etype & XPR_SYM1) { // both expression have a symbol
        expr.etype = XPR_ERROR;
        expr.value.u = ERR_CONFLICT_TYPE;
        return expr;
    }

    // resolve nested expressions
    if ((ex1.etype | ex2.etype) & XPR_OP) {
        if (op == '&' && (ex1.etype & XPR_REG) && !(ex1.etype & XPR_OP) && ex2.instruction == II_XOR && ((ex2.etype & 0xF) == XPR_INT) && ex2.value.i == -1) {
            // A & (B ^ -1) = and_not(A,B)
            expr = ex1;  expr.tokens = numtokens;
            expr.etype |= XPR_OP;
            expr.instruction = II_AND_NOT;
            expr.reg2 = ex2.reg1;
            return expr;
        }
        // simplify both expressions if possible
        exp12[0] = ex1;  exp12[1] = ex2; 
        for (i = 0; i < 2; i++) {
            if ((exp12[i].etype & XPR_REG) && (exp12[i].etype & XPR_IMMEDIATE) && exp12[i].value.i == 0) {
                if (exp12[i].instruction == II_SUB_REV) {
                    // expression is -R converted to (0-R). change to register and sign bit
                    exp12[i].etype &= ~(XPR_OPTIONS | XPR_IMMEDIATE | XPR_OP);
                    exp12[i].instruction = 0;
                    exp12[i].optionbits = 1;
                }
                else if (exp12[i].instruction == II_MUL_ADD2) {
                    // expression is -A*B converted to (0-A*B). change to A*B and sign bit
                    exp12[i].instruction = II_MUL;
                    exp12[i].optionbits = (exp12[i].optionbits >> 2) & 1;
                    exp12[i].etype &= ~(XPR_OPTIONS | XPR_IMMEDIATE);
                }
                else if (exp12[i].instruction == II_ADD_ADD && (exp12[i].etype & (XPR_INT | XPR_FLT)) && (exp12[i].optionbits & 3) == 3 && exp12[i].value.i == 0) {
                    // expression is -(A+B) converted to (-A-B+0). change to A+B and sign bit
                    exp12[i].etype &= ~(XPR_INT | XPR_FLT);
                    exp12[i].instruction = II_ADD;
                    exp12[i].optionbits = 1;
                    exp12[i].etype &= ~(XPR_OPTIONS | XPR_IMMEDIATE);
                }
            }
            else if (exp12[i].instruction == II_SUB_REV /* && exp12[i].value.i == 0*/) {
                // change -A+B to -(A-B)
                exp12[i].instruction = II_SUB;
                exp12[i].optionbits ^= 3;
            }
        }
        if ((exp12[0].etype & XPR_IMMEDIATE) && (exp12[1].etype & XPR_IMMEDIATE) && !((exp12[0].etype & exp12[1].etype) & XPR_REG)) {
            // both operands contain an immediate. combine the immediates
            if (exp12[1].etype & XPR_REG) {
                // second operand contains a register. swap operands
                expr = exp12[0];  exp12[0] = exp12[1];  exp12[1] = expr;
                swapped = true;
            }
            bool isfloat[2];  // check if operands are float
            for (i = 0; i < 2; i++) isfloat[i] = (exp12[i].etype & XPR_IMMEDIATE) == XPR_FLT; 

            // convert integer to float if the other operand is float
            for (i = 0; i < 2; i++) {
                if (isfloat[1-i] && !isfloat[i]) {
                    exp12[i].value.d = (double)exp12[i].value.i;
                    isfloat[i] = true;
                }
            }
            expr = exp12[0];  expr.tokens = numtokens;
            if (op == '+' || op == '-') { // add or subtract second operand
                uint8_t s = exp12[0].optionbits;  // sign bits of register, i1, i2
                switch (exp12[0].instruction) {
                case II_ADD:
                    if (op == '-') s ^= swapped ? 3 : 4;
                    break;
                case II_SUB:
                    s ^= 2;
                    if (op == '-') {
                        s ^= 4;
                        if (swapped) s ^= 7;
                    }
                    break;
                case II_SUB_REV:
                    s ^= 1;
                    if (op == '-') {
                        s |= 4;
                        if (swapped) s ^= 7;
                    }
                    break;
                default:   // no other instructions can be combined with + or -
                    expr.etype |= XPR_ERROR; expr.value.u = ERR_WRONG_OPERANDS;
                    return expr;
                }
                // change sign of immediates, and add them
                if (isfloat[0]) {                
                    if (s & 2) exp12[0].value.d = -exp12[0].value.d;
                    if (s & 4) exp12[1].value.d = -exp12[1].value.d;
                    expr.value.d = exp12[0].value.d + exp12[1].value.d;
                }
                else {
                    if (s & 2) exp12[0].value.i = -exp12[0].value.i;
                    if (s & 4) exp12[1].value.i = -exp12[1].value.i;
                    expr.value.u = exp12[0].value.u + exp12[1].value.u;
                }
                expr.optionbits = 0;
                expr.instruction = (s & 1) ? II_SUB_REV : II_ADD; // sign of register operand
                expr.etype = (expr.etype & ~XPR_IMMEDIATE) | (isfloat[0] ? XPR_FLT : XPR_INT); 
            }
            else if (op == '*' && expr.instruction == II_MUL) {
                if (isfloat[0]) {                
                    expr.value.d = exp12[0].value.d * exp12[1].value.d;
                }
                else {
                    expr.value.u = exp12[0].value.u * exp12[1].value.u;
                }
            }
            else if (op == '&' && expr.instruction == II_AND && !isfloat[0]) {
                expr.value.u = exp12[0].value.u & exp12[1].value.u;
            }
            else if (op == '|' && expr.instruction == II_OR && !isfloat[0]) {
                expr.value.u = exp12[0].value.u | exp12[1].value.u;
            }
            else if (op == '^' && expr.instruction == II_XOR && !isfloat[0]) {
                expr.value.u = exp12[0].value.u ^ exp12[1].value.u;
            }
            else {
                expr.etype |= XPR_ERROR; expr.value.u = ERR_WRONG_OPERANDS;
            }
            return expr;
        }

        // error if two memory or integer operands
        if (((exp12[0].etype & (XPR_IMMEDIATE | XPR_MEM)) && (exp12[1].etype & (XPR_IMMEDIATE | XPR_MEM)))
            || (exp12[0].value.i && exp12[1].value.i)) {
            expr.etype |= XPR_ERROR; expr.value.u = ERR_WRONG_OPERANDS;
            return expr;
        }

        if (exp12[0].etype & (XPR_IMMEDIATE | XPR_MEM)) {
            // first operand is integer, float or memory. swap operands if not two vector registers
            if (exp12[0].reg1 & exp12[1].reg1 & REG_V) {                
                // both operands contain a vector register. cannot swap. make error message later if swapping required
                cannotSwap = true;
            }
            else if (exp12[1].etype & (XPR_IMMEDIATE | XPR_MEM)) {
                // second operand also contains memory or immediate constant
                cannotSwap = true;
            }
            else {  // swap operands to get immediate or memory operand last
                expr = exp12[0];  exp12[0] = exp12[1];  exp12[1] = expr;
                swapped = true;
            }
        } 

        if (op == '+' || op == '-') {
            if (exp12[0].etype & (XPR_IMMEDIATE | XPR_MEM) && exp12[1].instruction == II_MUL && !(exp12[1].etype & (XPR_INT | XPR_FLT | XPR_MEM))) {
                // (memory or constant) + reg*reg. swap operands
                expr = exp12[0];  exp12[0] = exp12[1];  exp12[1] = expr;
                if (op == '-') {
                    exp12[0].optionbits ^= 1;  // invert signs in both operands
                    exp12[1].optionbits ^= 1;
                }
            }
            if (!((exp12[0].etype | exp12[1].etype) & XPR_OP)) {
                // +/-R1 +/-R2
                if (op == '-') exp12[1].optionbits ^= 1;   // sign of second operand
                // change sign of constant if this simplifies it
                if ((exp12[1].etype & XPR_INT) && (exp12[1].optionbits & 1)) {
                    exp12[1].value.i = -exp12[1].value.i;
                    exp12[1].optionbits = 0;
                }
                else if ((exp12[1].etype & XPR_FLT) && (exp12[1].optionbits & 1)) {
                    exp12[1].value.d = -exp12[1].value.d;
                    exp12[1].optionbits = 0;
                }
                uint8_t s = exp12[0].optionbits | exp12[1].optionbits << 1;  // combine signs
                expr = exp12[1];  expr.tokens = numtokens;
                expr.reg1 = exp12[0].reg1;
                if (exp12[1].etype & XPR_REG1) {                
                    expr.reg2 = exp12[1].reg1;  expr.etype |= XPR_REG2;
                }
                expr.etype |= XPR_OP | XPR_REG1;
                expr.optionbits = 0;
                switch (s) {
                case 0:  // R1 + R2
                    expr.instruction = II_ADD;  break;
                case 1:  // -R1 + R2
                    expr.instruction = II_SUB_REV;  break;
                case 2:  // R1 - R2
                    expr.instruction = II_SUB;  break;
                case 3:  // -R1 -R2
                    expr.instruction = II_ADD_ADD;
                    expr.value.i = 0;
                    expr.optionbits = s;
                    expr.etype |= XPR_INT | XPR_OPTIONS;
                    break;
                }
                return expr;
            }
            else if (exp12[0].instruction == II_MUL) {
                // A*B+C
                expr = exp12[1];
                expr.tokens = numtokens;
                if (exp12[0].etype & (XPR_IMMEDIATE | XPR_MEM)) {
                    // does not fit
                    expr.etype |= XPR_ERROR;
                    expr.value.w = cannotSwap ? ERR_CANNOT_SWAP_VECT : ERR_TOO_COMPLEX;
                    return expr;
                }
                expr.etype |= XPR_OP;
                expr.instruction = II_MUL_ADD2;
                if (exp12[1].etype & XPR_REG) {  // 3 registers
                    expr.reg3 = exp12[1].reg1;
                    expr.etype |= XPR_REG3;
                }
                expr.reg1 = exp12[0].reg1;  expr.reg2 = exp12[0].reg2;  
                expr.etype |= XPR_REG1 | XPR_REG2;
                expr.optionbits = 0xC * (exp12[0].optionbits & 1) | 3 * ((exp12[1].optionbits & 1) ^ (op == '-'));
                expr.etype |= XPR_OPTIONS;
                return expr;
            }
            else if (exp12[1].instruction == II_MUL) {
                // A+B*C
                expr = exp12[1];  
                expr.tokens = numtokens;
                if (exp12[0].etype & (XPR_IMMEDIATE | XPR_MEM)) {
                    // does not fit
                    expr.etype |= XPR_ERROR;
                    expr.value.w = cannotSwap ? ERR_CANNOT_SWAP_VECT : ERR_TOO_COMPLEX;
                    return expr; 
                }
                expr.etype |= XPR_OP;
                expr.instruction = II_MUL_ADD;
                if (!(exp12[1].etype & (XPR_IMMEDIATE | XPR_MEM))) {
                    // 3 registers
                    expr.reg3 = exp12[1].reg2;
                    expr.etype |= XPR_REG3;
                }
                expr.reg2 = exp12[1].reg1;  expr.etype |= XPR_REG2;
                expr.reg1 = exp12[0].reg1;
                expr.optionbits = 3 * (exp12[0].optionbits & 1) | 0xC * ((exp12[1].optionbits & 1) ^ (op == '-'));
                expr.etype |= XPR_OPTIONS;
                return expr;
            } 
            else if (exp12[0].instruction == II_ADD || exp12[0].instruction == II_SUB) {
            // (A+B)+C
                expr = exp12[0] | exp12[1];  expr.tokens = numtokens;
                expr.reg1 = exp12[0].reg1;
                expr.etype |= XPR_OP;
                expr.instruction = II_ADD_ADD;
                if (exp12[0].etype & (XPR_IMMEDIATE | XPR_MEM)) {  // mem or immediate from exp1 goes to third operand
                    expr.reg2 = exp12[1].reg1;  expr.etype |= XPR_REG2;
                    expr.optionbits = (exp12[0].optionbits & 1) | ((exp12[1].optionbits & 1) ^ (op == '-')) << 1 | ((exp12[0].optionbits >> 1 & 1) ^ (exp12[0].instruction == II_SUB)) << 2;                    
                }
                else {  // exp1 has two registers
                    if (exp12[1].etype & XPR_REG) {
                        expr.reg3 = exp12[1].reg1; // third register
                        expr.etype |= XPR_REG3;
                    }
                    expr.optionbits = 3 * (exp12[0].optionbits & 1) | ((exp12[1].optionbits & 1) ^ (op == '-')) << 2;
                    if (exp12[0].instruction == II_SUB) expr.optionbits ^= 2;                    
                }
                if (swapped && op == '-') expr.optionbits ^= 7;
                expr.etype |= XPR_OPTIONS;
                return expr;
            }
            else if ((exp12[1].instruction == II_ADD || exp12[1].instruction == II_SUB) && !(exp12[0].etype & (XPR_INT | XPR_FLT | XPR_MEM))) {
                // A+(B+C)
                expr = exp12[1];  expr.tokens = numtokens;
                expr.etype |= XPR_OP;
                expr.instruction = II_ADD_ADD;
                if (!(exp12[1].etype & (XPR_IMMEDIATE | XPR_MEM))) {
                    // 3 registers
                    expr.reg3 = exp12[1].reg2;
                    expr.etype |= XPR_REG3;
                }
                expr.reg2 = exp12[1].reg1;  expr.etype |= XPR_REG2;
                expr.reg1 = exp12[0].reg1;
                expr.optionbits = (exp12[0].optionbits & 1) | 6 * ((exp12[1].optionbits & 1) ^ (op == '-'));
                if (exp12[1].instruction == II_SUB) expr.optionbits ^= 4;
                if (swapped && op == '-') expr.optionbits ^= 7;
                expr.etype |= XPR_OPTIONS;
                return expr;
            }
        }
        else if (!((exp12[0].etype | exp12[1].etype) & XPR_OP) 
            && (op == '*' || (op == '/' && !swapped))) { 
            // (+/- a) * (+/- b)
            expr = exp12[0] | exp12[1];
            expr.etype |= XPR_OP;
            expr.tokens = numtokens;
            expr.optionbits = exp12[0].optionbits ^ exp12[1].optionbits;
            if (expr.optionbits & 1) {   // change sign
                if ((exp12[1].etype & 0xF) == XPR_FLT) exp12[1].value.d = -exp12[1].value.d;
                else if ((exp12[1].etype & 0xF) == XPR_INT) exp12[1].value.i = -exp12[1].value.i;
                else if ((exp12[1].etype & XPR_REG) && op == '*' && expr.value.i == 0) {  
                    // change -a*b to 0-a*b
                    expr.instruction = II_MUL_ADD2;
                    expr.optionbits = 0xC;
                    expr.reg1 = exp12[0].reg1;
                    expr.reg2 = exp12[1].reg1;  expr.etype |= XPR_REG2;
                    expr.etype |= XPR_INT | XPR_OPTIONS;
                    return expr;
                }
                else if ((exp12[1].etype & XPR_MEM) && op == '*') {  
                    // note: -mem*reg cannot be represented by a single instruction, even if we may later add a g.p. register so that reg-mem*reg would fit
                    expr.etype |= XPR_ERROR; expr.value.w = ERR_TOO_COMPLEX;
                    return expr;
                }
                else {
                    expr.etype |= XPR_ERROR; expr.value.w = ERR_TOO_COMPLEX;
                    return expr;
                }
            }
            expr.reg1 = exp12[0].reg1;
            expr.reg2 = exp12[1].reg1;  expr.etype |= XPR_REG2;
            expr.instruction = (op == '*') ? II_MUL : II_DIV;
            return expr;
        }

        // complex cases not one of the above
        expr.etype |= XPR_ERROR; expr.value.u = ERR_TOO_COMPLEX;
        expr.tokens = numtokens;
        return expr;
    }

    // not a complex expression
    if ((ex1.etype & (XPR_IMMEDIATE | XPR_MEM)) && !((ex1.reg1 & REG_V) || (ex2.etype & XPR_IMMEDIATE))){
        // first operand is integer, float or memory. swap operands if not two vector registers or memory and immediate
        exp12[0] = ex2;  exp12[1] = ex1;  swapped = true;
    }
    else {
        exp12[0] = ex1;  exp12[1] = ex2;  
    }
    expr.etype |= (exp12[1].etype & XPR_REG1) << 1;  // XPR_REG1 becomes XPR_REG2

    // combine everything from the two operands
    expr = exp12[0] | exp12[1];
    expr.etype |= XPR_OP;
    expr.tokens = numtokens;
    expr.reg1 = exp12[0].reg1;
    expr.reg2 = exp12[1].reg1;
    expr.etype |= (exp12[1].etype & XPR_REG1) << 1;

    // 2-operand instruction
    switch (op) {
    case '+':
        expr.instruction = II_ADD;  break;
    case '-':
        expr.instruction = swapped ? II_SUB_REV : II_SUB;  break;
    case '*':
        expr.instruction = II_MUL;  break;
    case '/':
        expr.instruction = swapped ? II_DIV_REV : II_DIV;  break;
    case '%':
        if (swapped) {expr.etype |= XPR_ERROR; expr.value.u = ERR_WRONG_TYPE;}
        expr.instruction = II_REM;  break;
    case '&':  case '&'+D2:  // boolean AND and bitwise AND have same implementation
        expr.instruction = II_AND;  break;
    case '|':  case '|'+D2:  // boolean OR and bitwise OR have same implementation
        expr.instruction = II_OR;  break;
    case '^':
        expr.instruction = II_XOR;  break;
    case '<':
        expr.instruction = II_COMPARE;
        expr.optionbits = 2 ^ swapped;  
        expr.etype |= XPR_OPTIONS;
        break;
    case '<' + EQ:    // <=
        expr.instruction = II_COMPARE;  
        expr.optionbits = 5 ^ swapped;  
        expr.etype |= XPR_OPTIONS;
        break;
    case '>':
        expr.instruction = II_COMPARE;  
        expr.optionbits = 4 ^ swapped;  
        expr.etype |= XPR_OPTIONS;
        break;
    case '>' + EQ:   // >=
        expr.instruction = II_COMPARE;  
        expr.optionbits = 3 ^ swapped;  
        expr.etype |= XPR_OPTIONS;
        break;
    case '='+D2:  // ==
        expr.instruction = II_COMPARE;  
        expr.optionbits = 0;  
        //expr.etype |= XPR_OPTIONS;
        break;
    case '!'+EQ:  // !=
        expr.instruction = II_COMPARE;  
        expr.optionbits = 1;  
        expr.etype |= XPR_OPTIONS;
        break;
    case '<' + D2:  // <<
        if (swapped) {expr.etype |= XPR_ERROR; expr.value.u = ERR_WRONG_TYPE;}
        expr.instruction = II_SHIFT_LEFT;  break;
    case '>' + D2:   // >>
        if (swapped) {expr.etype |= XPR_ERROR; expr.value.u = ERR_WRONG_TYPE;}
        expr.instruction = II_SHIFT_RIGHT_S;  break;
    case '>' + D3:   // >>>
        if (swapped) {expr.etype |= XPR_ERROR; expr.value.u = ERR_WRONG_TYPE;}
        expr.instruction = II_SHIFT_RIGHT_U;  break;
    default:
        expr.etype |= XPR_ERROR; expr.value.u = ERR_WRONG_TYPE;
    }
    return expr;
}


// Interpret dyadic expression generating memory operand. 
// both expressions are inside [] or at least one contains components other than integer constants
SExpression CAssembler::op2Memory(uint32_t op, SExpression & exp1, SExpression & exp2) {
    SExpression expr;                                // return value
    SExpression expt;    // temporary value
    expr.tokens = exp1.tokens + exp2.tokens + 1;  // total number of tokens
    uint64_t f;        // temporary factor

    if ((exp2.etype & XPR_SYM1) && op == '-') {
        // subtracting two symbol addresses
        exp2.sym2 = exp2.sym1;  exp2.sym1 = 0;
        exp2.etype = (exp2.etype & ~XPR_SYM1) | XPR_SYM2;
        if (exp1.symscale == 0) exp1.symscale = 1;
        if (exp2.symscale == 0) exp2.symscale = 1;
        if (exp1.symscale != exp2.symscale) {
            exp1.value.u = ERR_CONFLICT_TYPE;  // conflicting scale factors
            exp1.etype |= XPR_ERROR;  return exp1;
        }    
    }
    // error checks
    if (exp1.etype & exp2.etype & (XPR_SYM1 | XPR_SYM2 | XPR_SYMSCALE | XPR_INDEX 
        | XPR_LIMIT | XPR_LENGTH | XPR_BROADC)) {        
        exp1.value.u = ERR_MEM_COMPONENT_TWICE;              // some component or option specified twice
        exp1.etype |= XPR_ERROR;  return exp1;
    }
    if (((exp1.etype | exp2.etype) & (XPR_LIMIT | XPR_OFFSET)) == (XPR_LIMIT | XPR_OFFSET)) {
        exp1.value.u = ERR_LIMIT_AND_OFFSET;  // cannot have both offset and limit
        exp1.etype |= XPR_ERROR;  return exp1;
    } 

    if ((exp2.etype & XPR_BASE) && ((exp1.etype & XPR_BASE) || op == '-')) {
        // adding two registers or subtracting a register. make the second an index register
        if (exp2.base == 31 && (exp1.etype & XPR_BASE) && !(exp2.etype & XPR_INDEX)) {  
            // stack pointer cannot be index. make first register an index instead
            exp1.index = exp1.base; exp1.base = 0;
            exp1.etype = (exp1.etype & ~XPR_BASE) | XPR_INDEX;
            exp1.scale = 1;
        }
        else {
            exp2.index = exp2.base; exp2.base = 0;
            exp2.etype = (exp2.etype & ~XPR_BASE) | XPR_INDEX;
            exp2.scale = 1;
        }
    }
    // combine everything from the two operands
    expr = exp1 | exp2;
    expr.tokens = exp1.tokens + exp2.tokens + 1;
    expr.value.u = exp1.value.u + exp2.value.u;  // add values, except for special cases below
    expr.offset = exp1.offset + exp2.offset;     // add offsets, except for special cases below
    expr.etype &= ~XPR_OP;  expr.instruction = 0;  // operator is resolved here

    switch (op) {
    case '+':  // adding components. offsets have been added above
        if ((expr.etype & (XPR_REG | XPR_BASE | XPR_SYM1)) && (expr.etype & XPR_INT) && (expr.etype & XPR_MEM)) {
            // adding offset. convert value to offset
            expr.offset += expr.value.i;
            expr.value.i = 0;
            expr.etype = (expr.etype | XPR_OFFSET) & ~XPR_IMMEDIATE;
        }
        break;
    case ',':  // combining components. components are combined below
        if (exp1.value.u && exp2.value.u) {          
            expr.value.u = ERR_WRONG_TYPE;       // cannot combine integer offsets with comma operator
            expr.etype |= XPR_ERROR;  return expr;
        }
        if ((expr.etype & XPR_INDEX) && (expr.etype & (XPR_LENGTH | XPR_BROADC))) { // both index and broadcast
            if (expr.scale == -1) {
                if (expr.index != expr.length) { // scale = -1. index and length must be the same
                    expr.value.u = ERR_NEG_INDEX_LENGTH;
                    expr.etype |= XPR_ERROR;  return expr;
                }
            }
            else { // cannot have index and length/broadcast
                expr.value.u = ERR_INDEX_AND_LENGTH;
                expr.etype |= XPR_ERROR;  return expr;
            }
        }
        break;
    case '-':         // subtract offsets or registers (symbol addresses subtracted above)
        if ((exp1.etype & (XPR_REG | XPR_BASE | XPR_SYM1)) && (exp2.etype & XPR_INT) && (expr.etype & XPR_MEM)) {
            // subtracting offset. convert value to offset
            expr.offset = exp1.offset - exp2.value.i;
            expr.value.i = 0;
            expr.etype = (expr.etype | XPR_OFFSET) & ~XPR_IMMEDIATE;
        }
        else {
            expr.offset = exp1.offset - exp2.offset;
            expr.value.u = exp1.value.u - exp2.value.u; 
        }
        if (exp2.etype & XPR_INDEX) {  // subtracting a register gives negative index
            expr.scale = - exp2.scale;
        }
        else if ((exp1.etype & XPR_SYM1) && (exp2.etype & XPR_SYM2)) {
            // subtracting two symbols. has been fixed above
            // check if symbols are in the same domain
            int32_t symi1 = findSymbol(exp1.sym1);
            int32_t symi2 = findSymbol(exp2.sym2);
            if (symi1 > 0 && symi2 > 0 
                && (symbols[symi1].st_other & symbols[symi2].st_other & (SHF_IP | SHF_DATAP | SHF_THREADP)) == 0
                && (symbols[symi1].st_type & symbols[symi2].st_type & STT_CONSTANT) == 0) {
                errors.reportLine(ERR_RELOCATION_DOMAIN);
            }
        }        
        //else if ((expr.etype & XPR_IMMEDIATE) == XPR_INT) expr.etype |= XPR_OFFSET;  // value is offset
        if (exp2.etype & (XPR_SYM1|XPR_SYMSCALE)) {
            expr.value.u = ERR_WRONG_TYPE;    // cannot subtract these components
            expr.etype |= XPR_ERROR;  return expr;
        }
        //expr.value.u = exp1.value.u - exp2.value.u; 
        break;
    case '<'+D2:  // index << s = index * (1 << s)
        exp2.value.u = (uint64_t)1 << exp2.value.u;
        goto MULTIPLYINDEX; // continue in case '*'
    case '*':  // indexregister * scale
        if ((exp1.etype & XPR_INT) && (exp2.etype & (XPR_BASE|XPR_INDEX))){
            // first operand is integer, second operand is register. swap operands
            expt = exp2;  exp2 = exp1;  exp1 = expt;
        }
        MULTIPLYINDEX:
        if ((exp1.etype & XPR_BASE) && !(exp1.etype & XPR_INDEX)) {  // convert base to index
            exp1.index = exp1.base;  exp1.base = 0;  exp1.scale = 1;
            exp1.etype = (exp1.etype & ~XPR_BASE) | XPR_INDEX;
        }
        if (!(exp1.etype & XPR_INDEX) || ((exp2.etype & 0xF) != XPR_INT)
            || ((exp1.etype | exp2.etype) & (XPR_OPTION|XPR_SYM1|XPR_SYM2|XPR_LIMIT|XPR_LENGTH|XPR_BROADC))) {
            expr.value.u = ERR_WRONG_TYPE;    // cannot multiply anything else
            expr.etype |= XPR_ERROR;  return expr;
        }
        f = exp2.value.u * exp1.scale;
        if ((f & (f - 1)) || f == 0 || f > 16) {  // check that scale is a power of 2, not bigger than 16
            expr.value.u = ERR_SCALE_FACTOR;    // wrong scale factor
            expr.etype |= XPR_ERROR;  return expr;
        }
        expr.base = exp1.base;  expr.index = exp1.index;
        expr.scale = (int8_t)f;
        expr.etype = exp1.etype | (exp2.etype & ~XPR_INT);
        expr.value.u = 0;
        break;
    case '>'+D2:  // divide (sym1-sym2) >> s = (sym1-sym2) / (1 << s)
        exp2.value.u = (uint64_t)1 << exp2.value.u;
        // continue in case '/'
    case '/':  // divide (sym1-sym2) / scale
        if (!(exp1.etype & XPR_SYM1) || ((exp2.etype & 0xF) != XPR_INT)
            || ((exp1.etype | exp2.etype) & (XPR_REG|XPR_OPTION|XPR_LIMIT|XPR_LENGTH|XPR_BROADC))) {
            expr.value.u = ERR_WRONG_TYPE;    // cannot divide anything else
            expr.etype |= XPR_ERROR;  return expr;
        }
        f = exp2.value.u;
        if (exp1.symscale) f *= exp1.symscale;
        if ((f & (f - 1)) || f == 0 || f > 16) {  // check that scale is a power of 2, not bigger than 16
            expr.value.u = ERR_SCALE_FACTOR;    // wrong scale factor
            expr.etype |= XPR_ERROR;  return expr;
        }
        expr.symscale = (int8_t)f;
        expr.etype |= XPR_SYMSCALE;
        expr.etype = exp1.etype | (exp2.etype & ~XPR_INT);
        expr.value.u = exp1.value.u;
        break;
    case '=':  // option = value
        // check if operands contain anything else
        if (!(exp1.etype & XPR_OPTION) || !(exp2.etype & (XPR_INT | XPR_BASE | XPR_REG))
            || ((exp1.etype | exp2.etype) & (XPR_SYM1|XPR_SYM2|XPR_REG2|XPR_INDEX|XPR_LIMIT|XPR_LENGTH|XPR_BROADC))) {
            expr.value.u = ERR_WRONG_TYPE;    // cannot uses '=' on anyting else inside []
            expr.etype |= XPR_ERROR;  return expr;
        }
        switch (exp1.value.w) {
        case OPT_LENGTH:  // length = register
            if ((exp2.etype & XPR_REG1) && (exp2.reg1 & REG_R)) {
                // length = register, outside []
                expr.etype = XPR_LENGTH | XPR_MEM;
                expr.length = exp2.reg1;
                expr.base = 0;
                expr.value.i = 0;
                break;
            }
            // length = register, inside []
            if (!(exp2.etype & XPR_BASE) || (exp2.base & 0xE0) != REG_R) {
                expr.value.u = ERR_WRONG_TYPE;    // cannot uses '=' on anyting else inside []
                expr.etype |= XPR_ERROR;  return expr;
            }
            expr.etype = XPR_LENGTH | XPR_MEM;
            expr.length = exp2.base;
            expr.base = 0;
            expr.value.i = 0;
            break;
        case OPT_BROADCAST:  // broadcast = register
            if (!(exp2.etype & XPR_BASE) || (exp2.base & 0xE0) != REG_R) {
                expr.value.u = ERR_WRONG_TYPE;    // cannot uses '=' on anyting else inside []
                expr.etype |= XPR_ERROR;  return expr;
            }
            expr.etype = XPR_BROADC | XPR_MEM;
            expr.length = exp2.base;
            expr.base = 0;
            expr.value.i = 0;
            break;
        case OPT_LIMIT:   // limit = integer
            if (!(exp2.etype & XPR_INT)) {
                expr.value.u = ERR_WRONG_TYPE;    // cannot uses '=' on anyting else inside []
                expr.etype |= XPR_ERROR;  return expr;
            }
            if (exp1.etype & XPR_OFFSET) {  // cannot have both limit and offset
                expr.etype = ERR_LIMIT_AND_OFFSET;
                expr.etype |= XPR_ERROR;  return expr;
            }
            expr.etype = XPR_LIMIT | XPR_MEM;
            expr.value.u = exp2.value.u;
            break;
        case OPT_SCALAR:  // scalar
            expr.etype = XPR_SCALAR | XPR_MEM;
            expr.value.i = 0;
            break;
        case OPT_MASK:
            if (!(exp2.etype & (XPR_REG | XPR_REG1))) {
                expr.etype = ERR_MASK_NOT_REGISTER;
                expr.etype |= XPR_ERROR;  return expr;
            }
            expr.etype = XPR_MASK;
            expr.mask = exp2.reg1;
            expr.reg1 = 0;
            break;
        case OPT_FALLBACK:
            if (exp2.etype == (XPR_REG | XPR_REG1) && (exp2.reg1 & 0x1F) != 0x1F) {
                expr.fallback = exp2.reg1;
                expr.etype = XPR_FALLBACK;
                expr.reg1 = 0;
            }
            else if ((exp2.etype & XPR_IMMEDIATE) && exp2.value.i == 0){
                expr.fallback = (expr.mask & 0xF0) | 0x1F;            
                expr.etype = XPR_FALLBACK;
            }
            else {
                expr.value.u = ERR_FALLBACK_WRONG;
                expr.etype |= XPR_ERROR;  return expr;
            }
            break;
        case OPT_OPTIONS:
            if ((exp2.etype & 0xF) == XPR_INT) {
                expr.etype = (expr.etype & ~XPR_IMMEDIATE) | XPR_OPTIONS;
                expr.optionbits = (uint8_t)exp2.value.u;  // move value to optionbits
                expr.value.i = 0;
                return expr;
            }
            else {
                expr.etype = ERR_WRONG_TYPE;
                expr.etype |= XPR_ERROR;  
                return expr;
            }
            break;
        default:  // mask and fallback options not allowed inside []
            expr.value.u = ERR_NOT_INSIDE_MEM;  // change error message
            expr.etype |= XPR_ERROR;  return expr;
        }
        break;

    default:  // wrong operator
        expr.value.u = ERR_WRONG_TYPE;
        expr.etype |= XPR_ERROR;  return expr;
    }
    if ((expr.etype & XPR_INT) && !(expr.etype & (XPR_SYM1 | XPR_INDEX))) {           // value not used otherwise is offset
        expr.etype = (expr.etype & ~(XPR_INT)) | XPR_OFFSET;
    }
    return expr;
}


// Interpreted triadic expression exp1 ? exp2 : exp3 at the indicated positions
SExpression CAssembler::op3(uint32_t tok1, uint32_t toklow, uint32_t tokcolon, uint32_t maxtok, uint32_t options) {
    SExpression exp1, exp2;
    uint32_t cond;   // evaluated condition

    exp1 = expression(tok1, toklow - tok1, options);  // evaluate expression before '?'
    if (exp1.tokens != toklow - tok1) errors.report(tokens[tok1 + exp1.tokens]);

    if ((exp1.etype & XPR_REG) == 0 && (exp1.etype & (XPR_INT | XPR_FLT | XPR_STRING))) {
        // condition is a constant. just choose one of the two operands

        if ((exp1.etype & 0xF) == XPR_FLT) cond = exp1.value.d != 0.; // evaluate condition to true or false
        else if ((exp1.etype & 0xF) == XPR_STRING) {   // string is false if empty or "0"
            cond = (exp1.sym2 != 0 && (exp1.sym2 > 1 || stringBuffer.get<uint16_t>((uint32_t)exp1.value.u) != '0'));
        }
        else cond = exp1.value.i != 0;

        // the expression that is not selected is evaluated with option = 0x10 to suppress errors but still count the tokens
        exp1 = expression(toklow + 1, tokcolon - (toklow + 1), options | (cond ^ 1) << 4);   // evaluate first expression
        if (exp1.tokens != tokcolon - (toklow + 1)) errors.report(tokens[toklow + 1 + exp1.tokens]);
        exp2 = expression(tokcolon + 1, tok1 + maxtok - (tokcolon + 1), options | cond << 4);      // evaluate second expression

        // number of tokens
        exp1.tokens = exp2.tokens = tokcolon - tok1 + 1 + exp2.tokens;

        // return the chosen expression
        if (cond) return exp1; else return exp2;
    }

    // condition is not a constant. It must be a mask register
    if ((exp1.etype & XPR_REG) == 0 || exp1.reg1 == 0 || exp1.etype & (XPR_OP|XPR_OPTION|XPR_MEM|XPR_SYM1|XPR_MASK|XPR_UNRESOLV)) {
        errors.report(tokens[tok1].pos, tokens[tok1].stringLength, ERR_MASK_NOT_REGISTER);
    }
    uint8_t maskreg = exp1.reg1;  // save mask register
    
    // evaluate the middle expression
    exp1 = expression(toklow + 1, tokcolon - (toklow + 1), options);
    if (exp1.tokens != tokcolon - (toklow + 1)) errors.report(tokens[toklow + 1 + exp1.tokens]);

    // third expression must be fallback
    exp2 = expression(tokcolon + 1, tok1 + maxtok - (tokcolon + 1), options);
    uint8_t fallbackreg = 0;  // fallback register
    if (exp2.etype & XPR_REG) {
        fallbackreg = exp2.reg1;
        exp1.etype |= XPR_FALLBACK;
    }
    else if ((exp2.etype & (XPR_INT | XPR_FLT)) && exp2.value.i == 0) {
        fallbackreg = maskreg | 0x1F;  // register 31 with same type as mask register    
        exp1.etype |= XPR_FALLBACK;
    }
    if (exp2.etype & (XPR_STRING | XPR_OP | XPR_OPTION | XPR_MEM | XPR_SYM1 | XPR_MASK) || exp2.value.i) {
        errors.report(tokens[tokcolon+1].pos, tokens[tokcolon+exp2.tokens+1].pos  - tokens[tokcolon+1].pos, ERR_FALLBACK_WRONG);
    }
    // insert mask and fallback in exp1
    exp1.etype |= XPR_MASK;
    exp1.mask = maskreg;
    exp1.fallback = fallbackreg;
    exp1.tokens = tokcolon - tok1 + 1 + exp2.tokens;
    return exp1;
}


// Convert -(expression), e.g. -(A-B)
SExpression CAssembler::op1minus(SExpression & exp1) {
    exp1.tokens++;
    if ((exp1.etype & (XPR_REG | XPR_MEM)) && !(exp1.etype & XPR_OP) && exp1.value.i == 0) {  // -reg or -mem
        exp1.etype |= XPR_OP | XPR_INT;
        exp1.instruction = II_SUB_REV;              // 0 - expression
    }
    else if (exp1.instruction == II_SUB) exp1.instruction = II_SUB_REV;
    else if (exp1.instruction == II_SUB_REV) exp1.instruction = II_SUB;
    else if (exp1.instruction == II_ADD_ADD) exp1.optionbits ^= 3;
    else if (exp1.instruction == II_MUL_ADD || exp1.instruction == II_MUL_ADD2) exp1.optionbits ^= 0xF;
    else if (exp1.instruction == II_ADD && !(exp1.etype & (XPR_IMMEDIATE | XPR_MEM | XPR_SYM1))) {
        // -(R1+R2) = -R1 -R2 + 0
        exp1.instruction = II_ADD_ADD;
        exp1.value.i = 0;
        exp1.optionbits = 3;
    }
    else if (exp1.instruction == II_ADD && (exp1.etype & XPR_IMMEDIATE)) {
        // -(R1+I) = -R1 + (-I)
        exp1.instruction = II_SUB_REV;
        if ((exp1.etype & XPR_IMMEDIATE) == XPR_FLT) exp1.value.d = -exp1.value.d;
        else exp1.value.i = -exp1.value.i;
    }
    else if ((exp1.instruction == 0 || exp1.instruction == II_MUL || exp1.instruction == II_DIV || exp1.instruction == II_DIV_REV)
        && (exp1.etype & XPR_IMMEDIATE)) {
        // -I or -(A*I)
        if (exp1.etype & XPR_FLT) exp1.value.d = -exp1.value.d;
        else exp1.value.i = -exp1.value.i;
    }
    else {
        exp1.etype = XPR_ERROR;
        exp1.value.u = ERR_TOO_COMPLEX; // cannot apply '-' to other expressions
    }
    return exp1;
}

// Interpret dyadic expression with string operands
SExpression CAssembler::op2String(uint32_t op, SExpression const & exp1, SExpression const & exp2) {
    if (op != '+') {
        SExpression exp3;
        exp3.etype = XPR_ERROR;
        exp3.value.u = ERR_WRONG_TYPE;
        return exp3;
    }
    // operation is +. concatenate strings, convert numeric to string

    uint32_t stringpos1 = stringBuffer.dataSize();  // current position in string buffer
    uint32_t stringpos2;                            // position of second part of concatenated string
    const int maxIntLength = 32;                    // maximum length of integer as string
    const int maxFloatLength = 48;
    const char * wrongType = "-wrong type!-";
    uint32_t len = 0;                               // length of string

    // first operand
    if (exp1.etype == XPR_STRING) {
        stringBuffer.push(stringBuffer.buf() + exp1.value.u, exp1.sym2); // copy to string buffer without terminating zero
        stringBuffer.put((char)0);
        //stringpos2 = stringBuffer.dataSize();
    }
    else if (exp1.etype == XPR_INT) {  // convert integer to string
        stringBuffer.push(&exp1, maxIntLength);  // put in anyting here to make space for writing string
        if (sizeof(long int) >= 8) {        
#ifndef _WIN32  // suppress warning
            sprintf((char*)stringBuffer.buf()+stringpos1, "%li", exp1.value.i);
#endif
        }
        else {
            sprintf((char*)stringBuffer.buf()+stringpos1, "%lli", (long long)exp1.value.i);
        }
    }
    else if (exp1.etype == XPR_FLT) {  // convert float to string
        stringBuffer.push(&exp1, maxFloatLength);  // put in anyting here to make space for writing string
        sprintf((char*)stringBuffer.buf()+stringpos1, "%g", exp1.value.d);
    }
    else {
        stringBuffer.put(wrongType);
    }
    len = (uint32_t)strlen((char*)stringBuffer.buf()+stringpos1);
    stringpos2 = stringpos1 + len;
    stringBuffer.setSize(stringpos2);  // remove extra space

    // second operand
    if (exp2.etype == XPR_STRING) {
        stringBuffer.push(stringBuffer.buf() + exp2.value.u, exp2.sym2); // copy to string buffer without terminating zero
        stringBuffer.put((char)0);
    }
    else if (exp2.etype == XPR_INT) {  // convert integer to string
        stringBuffer.push(&exp2, maxIntLength);  // put in anyting here to make space for writing string
        if (sizeof(long int) >= 8) {
#ifndef _WIN32  // suppress warning
            sprintf((char*)stringBuffer.buf()+stringpos2, "%li", exp2.value.i);
#endif
        }
        else {
            sprintf((char*)stringBuffer.buf()+stringpos2, "%lli", (long long)exp2.value.i);
        }
        len = (uint32_t)strlen((char*)stringBuffer.buf()+stringpos2);
        stringBuffer.setSize(stringpos2 + len + 1);
    }
    else if (exp2.etype == XPR_FLT) {  // convert float to string
        stringBuffer.push(&exp2, maxFloatLength);  // put in anyting here to make space for writing string
        sprintf((char*)stringBuffer.buf()+stringpos2, "%g", exp2.value.d);
        len = (uint32_t)strlen((char*)stringBuffer.buf()+stringpos2);
        stringBuffer.setSize(stringpos2 + len + 1);
    }
    else {
        stringBuffer.put(wrongType);
    }
    SExpression exp3;
    exp3.etype = XPR_STRING;
    exp3.value.u = stringpos1;
    exp3.sym2 = (uint32_t)strlen((char*)stringBuffer.buf() + stringpos1);
    exp3.tokens = exp1.tokens + exp2.tokens + 1;
    return exp3;
}


double interpretFloat(const char * s, uint32_t length) {
    // interpret floating point number from string with indicated length
    char buffer[64];
    if (length >= sizeof(buffer)) {
        union {
            uint64_t i;
            double d;
        } nan = {0xFFFFC00000000000};
        return nan.d; // return NAN
    }
    memcpy(buffer, s, length);
    buffer[length] = 0;          // terminate string
    double r;
    sscanf(buffer, "%lf", &r);   // convert string to double
    return r;
} 

// make expression out of symbol
SExpression CAssembler::symbol2expression(uint32_t symi) {
    SExpression expr;
    zeroAllMembers(expr);
    switch (symbols[symi].st_type) {
    case STT_CONSTANT:  case STT_VARIABLE:
        expr.etype = XPR_INT;   // default type
        expr.sym1 = symi;
        if (symbols[symi].st_other & STV_FLOAT) expr.etype = XPR_FLT;
        if (symbols[symi].st_other & STV_STRING) {
            expr.etype = XPR_STRING;
            expr.sym2 = (uint32_t)symbols[symi].st_unitnum;
        }
        expr.value.u = symbols[symi].st_value;
        break;
    case STT_EXPRESSION:
        if (symbols[symi].st_value < expressions.numEntries()) {
            expr = expressions[uint32_t(symbols[symi].st_value)];
        }
        else {
            expr.etype = XPR_ERROR;
            expr.value.u = TOK_XPR;
        }
        break;
    default:
        expr.etype = XPR_ERROR; 
        expr.value.u = ERR_CONFLICT_TYPE;
    }
    expr.tokens = 0;
    return expr;
}

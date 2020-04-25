/****************************    assem1.cpp    ********************************
* Author:        Agner Fog
* Date created:  2017-04-17
* Last modified: 2020-04-15
* Version:       1.09
* Project:       Binary tools for ForwardCom instruction set
* Module:        assem.cpp
* Description:
* Module for assembling ForwardCom .as files. Contains:
* pass1(): Split input file into lines and tokens. Remove comments. Find symbol definitions
* pass2(): Handle meta code. Classify lines. Identify symbol names, sections, functions
*
* Copyright 2017 GNU General Public License http://www.gnu.org/licenses
******************************************************************************/
#include "stdafx.h"

const char * allowedInNames = "_$@";   // characters allowed in symbol names (don't allow characters that are used as operators)
const bool allowUTF8 = true;           // UTF-8 characters allowed in symbol names
const bool allowNestedComments = true; // allow nested comments: /* /* */ */

                                       // Operator for sorting symbols by name. Used by assembler
// List of operators
SOperator operatorsList[] = {
    // name, id, priority
    {"(", '(',      1},
    {")", ')',      1},
    {"[", '[',      1},
    {"]", ']',      1},
    {"{", '{',      1},
    {"}", '}',      1},
    {"'", 39,       1},
    {"\"", '"',     1},           // "
    {"/*", 'c',     1},           // comment begin
    {"*/", 'd',     1},           // comment end
    {".", '.',      2},
    {"!", '!',      3},
    {"~", '~',      3},
    {"++", '+'+D2,  3},
    {"--", '-'+D2,  3},
    {"*", '*',      4},
    {"/", '/',      4},
    {"%", '%',      4},
    {"+", '+',      5},
    {"-", '-',      5},
    {"<<", '<'+D2,  6},
    {">>", '>'+D2,  6},           // signed shift right
    {">>>", '>'+D3, 6},           // unsigned shift right
    {"<", '<',      7},
    {"<=", '<'+EQ,  7},
    {">", '>',      7},
    {">=", '>'+EQ,  7},
    {"==", '='+D2,  8},
    {"!=", '!'+EQ,  8},
    {"&", '&',      9},
    {"^", '^',     10},
    {"|", '|',     11},
    {"&&", '&'+D2, 12},
    {"||", '|'+D2, 13},
    {"?", '?',     14},
    {":", ':',     14},
    {"=", '=',     15},
    {"+=", '+'+EQ, 15},
    {"-=", '-'+EQ, 15},
    {"*=", '*'+EQ, 15},
    {"/=", '/'+EQ, 15},
    {"%=", '%'+EQ, 15},
    {"<<=", '<'+D2+EQ,  15},
    {">>=", '>'+D2+EQ,  15},      // signed shift right
    {">>>=", '>'+D3+EQ, 15},      // unsigned shift right
    {"&=", '&'+EQ, 15},
    {"^=", '^'+EQ, 15},
    {"|=", '|'+EQ, 15},
    {",", ',',     16},
    {"//", '/'+D2, 20},           // comment, end of line
    {";", ';',     20}            // comment, end of line
};


// List of keywords
SKeyword keywordsList[] = {
    // name, id
    {"section",        DIR_SECTION},        // TOK_DIR: section, functions directives
    {"function",       DIR_FUNCTION},
    {"end",            DIR_END},
    {"public",         DIR_PUBLIC},
    {"extern",         DIR_EXTERN},

    // TOK_ATT: attributes of sections, functions and symbols
    {"read",           ATT_READ},           // readable section
    {"write",          ATT_WRITE},          // writeable section
    {"execute",        ATT_EXEC},           // executable section
    {"align",          ATT_ALIGN},          // align section, data, or code
    {"weak",           ATT_WEAK},           // weak linking
    {"reguse",         ATT_REGUSE},         // register use    
    {"constant",       ATT_CONSTANT},       // external constant
    {"uninitialized",  ATT_UNINIT},         // uninitialized section (BSS)
    {"communal",       ATT_COMDAT},         // communal section. duplicates and unreferenced sections are removed
    {"exception_hand", ATT_EXCEPTION},      // exception handler and stack unroll information
    {"event_hand",     ATT_EVENT},          // event handler list, including constructors and destructors
    {"debug_info",     ATT_DEBUG},          // debug information
    {"comment_info",   ATT_COMMENT},        // comments, including copyright and required libraries

    // TOK_TYP: type names
    {"int8",           TYP_INT8},      
    {"uint8",          TYP_INT8+TYP_UNS},
    {"int16",          TYP_INT16},
    {"uint16",         TYP_INT16+TYP_UNS},
    {"int32",          TYP_INT32},
    {"uint32",         TYP_INT32+TYP_UNS},
    {"int64",          TYP_INT64},
    {"uint64",         TYP_INT64+TYP_UNS},
    {"int128",         TYP_INT128},
    {"uint128",        TYP_INT128+TYP_UNS},
    {"int",            TYP_INT32},
    {"float",          TYP_FLOAT32},
    {"double",         TYP_FLOAT64},
    {"float16",        TYP_FLOAT16},
    {"float32",        TYP_FLOAT32},
    {"float64",        TYP_FLOAT64},
    {"float128",       TYP_FLOAT128},
    {"string",         TYP_STRING},  

    // TOK_OPT: options of instructions and operands
    {"mask",           OPT_MASK},      
    {"fallback",       OPT_FALLBACK},
    {"length",         OPT_LENGTH},
    {"broadcast",      OPT_BROADCAST},    
    {"limit",          OPT_LIMIT},
    {"scalar",         OPT_SCALAR},
    {"options",        OPT_OPTIONS}, 

    // TOK_REG: register names
    {"threadp",        REG_THREADP},      
    {"datap",          REG_DATAP},
    {"ip",             REG_IP},
    {"sp",             REG_SP},

    // TOK_HLL: high level language keywords
    {"if",             HLL_IF},         
    {"else",           HLL_ELSE},
    {"switch",         HLL_SWITCH},         // switch (r1, scratch registers) { case 0: break; ...}
    {"case",           HLL_CASE},
    {"for",            HLL_FOR},            // for (r1 = 1; r1 <= r2; r1++) {}
    {"in",             HLL_IN},             // for (float v1 in [r1-r2], nocheck) // (r2 counts down)
    {"while",          HLL_WHILE},          // while (r1 > 0) {}
    {"do",             HLL_DO},             // do {} while ()
    {"break",          HLL_BREAK},          // break out of switch or loop
    {"continue",       HLL_CONTINUE},       // continue loop 

    // temporary additions. will be replaced by macros later:
    {"push",           HLL_PUSH},           // push registers
    {"pop",            HLL_POP},            // pop registers

};

// List of register name prefixes
SKeyword registerNames[] = {
    // name, id
    {"r",            REG_R},
    {"v",            REG_V},
    {"spec",         REG_SPEC},
    {"capab",        REG_CAPAB},
    {"perf",         REG_PERF},
    {"sys",          REG_SYS}
};


CAssembler::CAssembler() {                                 // Constructor
    // Reserve size for buffers
    const int estimatedLineLength = 16;
    const int estimatedTokensPerLine = 10;
    int estimatedNumLines = dataSize() / estimatedLineLength;
    lines.setNum(estimatedNumLines);
    tokens.setNum(estimatedNumLines * estimatedTokensPerLine);
    errors.setOwner(this);
    // Initialize and sort lists
    initializeWordLists();
    ElfFwcShdr nullHeader;         // make first section header empty
    zeroAllMembers(nullHeader);
    sectionHeaders.push(nullHeader);
}

void CAssembler::go() {

    // Write feedback text to console
    feedBackText1();

    // Set default options
    if (cmd.codeSizeOption == 0) cmd.codeSizeOption = 1 << 24;
    if (cmd.dataSizeOption == 0) cmd.dataSizeOption = 1 << 24;

    do {  // This loop is repeated only once. Just convenient to break out of in case of errors
        pass = 1;
        // Split input file into lines and tokens. Find symbol definitions
        pass1();
        if (errors.tooMany()) {err.submit(ERR_TOO_MANY_ERRORS);  break;}

        pass = 2;
        // A. Handle metaprogramming directives
        // B. Classify lines
        // C. Identify symbol names, sections, labels, functions 
        pass2();
        if (errors.tooMany()) {err.submit(ERR_TOO_MANY_ERRORS);  break;}

        //showTokens();  //!! for debugging only
        //showSymbols(); //!! for debugging only        

        pass = 3;
        // Interpret lines. Generate code and data
        pass3();
        if (errors.tooMany()) {err.submit(ERR_TOO_MANY_ERRORS);  break;}
        pass = 4;
        // Resolve internal cross references, optimize forward references
        pass4();
        if (errors.tooMany()) {err.submit(ERR_TOO_MANY_ERRORS);  break;}
        pass = 5;
        // Make binary file
        pass5();
        if (errors.tooMany()) {err.submit(ERR_TOO_MANY_ERRORS);  break;}

    } while (false);

    // output any error messages
    errors.outputErrors();
    if (errors.numErrors()) cmd.mainReturnValue = 1; // make sure makefile process stops on error
    
    // output object file
    outFile.write(cmd.getFilename(cmd.outputFile));
}


// Character can be the start of a symbol name
inline bool nameChar1(char c) {
    return ((c | 0x20) >= 'a' && (c | 0x20) <= 'z') || ((c & 0x80) && allowUTF8) || strchr(allowedInNames, c);
}

// Character can be the part of a symbol name
inline bool nameChar2(char c) {
    return nameChar1(c) || (c >= '0' && c <= '9');
}

// check if string is a number. Can be decimal, binary, octal, hexadecimal, or floating point
// Returns the length of the part of the string that belongs to the number
uint32_t isNumber(const char * s, int maxlen, bool * isFloat) {
    bool is_float = false;
    char c = s[0];
    if ((c < '0' || c > '9') && (c != '.' || s[1] < '0' || s[1] > '9')) return 0;
    int i = 0;
    int state = 0;  
    // 0: begin
    // 1: after 0
    // 2: after digits 0-9
    // 3: after 0x
    // 4: after 0b or 0o
    // 5: after .
    // 6: after E
    // 7: after E09
    // 8: after E+-
    for (i = 0; i < maxlen; i++) {
        c = s[i];
        char cl = c | 0x20;   // upper case letter
        if (c == '0' && state == 0) {state = 1; continue;}
        if (cl == 'x' && state == 1) {state = 3; continue;}
        if ((cl == 'b' || cl == 'o') && state == 1) {state = 4; continue;}
        if (c == '.' && state <= 2)  {state = 5; is_float = true; continue;}
        if (cl == 'e' && (state <= 2 || state == 5)) {state = 6; is_float = true; continue;}
        if ((c == '+' || c == '-') && state == 6) {state = 8; continue;}
        if (c >= '0' && c <= '9') {
            if (state < 2) state = 2;
            if (state == 6) state = 7;
            continue;
        }
        if (cl >= 'a' && cl <= 'f' && state == 3) continue;
        // Anything else: stop here
        break;
    }
    if (isFloat) *isFloat = is_float;       // return isFloat
    return i;                               // return length
}

// Check if string is a register name
uint32_t isRegister(const char * s, uint32_t len) {
    uint32_t i, j, nl, num;
    for (i = 0; i < TableSize(registerNames); i++) {
        if ((s[0] | 0x20) == registerNames[i].name[0]) {   // first character match, lower case
            nl = (uint32_t)strlen(registerNames[i].name);  // length of register name prefix
            if (len < nl + 1 || len > nl + 2) continue;    // continue search if length wrong
            for (j = 0; j < nl; j++) {                     // check if each character matches
                if ((s[j] | 0x20) != registerNames[i].name[j]) { // lower case compare
                    j = 0xFFFFFFFF; break;
                }
            }
            if (j == 0xFFFFFFFF) continue;                 // no match
            if (s[j] < '0' || s[j] > '9') continue;        // not a number
            num = s[j] - '0';                              // get number, first digit
            if (len == nl + 2) {                           // two digit number
                if (s[j+1] < '0' || s[j+1] > '9') continue;// second digit not a number            
                num = num * 10 + (s[j+1] - '0');
            }
            if (num >= 32) continue;                       // number too high
            return num + registerNames[i].id;              // everyting matches
        }
    }
    return 0;       // not found. return 0
} 

// write feedback text on stdout
void CAssembler::feedBackText1() {
    if (cmd.verbose) {
        // Tell what we are doing:
        printf("\nAssembling %s to %s", cmd.getFilename(cmd.inputFile), cmd.getFilename(cmd.outputFile));
    }
}


// Split input file into lines and tokens. Handle preprocessing directives. Find symbol definitions
void CAssembler::pass1() {
    uint32_t n = 0;                // offset into assembly file
    uint32_t m;                    // end of current token
    int32_t  i, f;                 // temporary
    int32_t  comment = 0;          // 0: normal, 1: inside comment to end of line, 2: inside /* */ comment
    uint32_t commentStart;         // start position of multiline comment
    uint32_t commentStartColumn;   // start column of multiline comment
    char c;                        // current character or byte
    SToken token = {0};            // current token
    SKeyword keywSearch;           // record to search for keyword
    SOperator opSearch;            // record to search for operator
    SInstruction instructSearch;   // record to search for instruction
    SLine line = {0,0,0,0,0,0,0};  // line record
    lines.push(line);              // empty records for line 0
    linei = 1;                     // start at line 1
    numSwitch = 0;              // count switch statements
    tokens.push(token);            // unused token 0

    if (dataSize() >= 3 && (get<uint32_t>(0) & 0xFFFFFF) == 0xBFBBEF) {        
        n += 3;                    // skip UTF-8 byte order mark
    }

    line.beginPos = n;             // start of line 1
    line.firstToken = tokens.numEntries();
    line.file = filei;

    // loop through file
    while (n < dataSize()) {
        c = get<char>(n);              // get character

        // is it space or a control character?
        if (uint8_t(c) <= 0x20) {                
            if (c == ' ' || c == '\t') {   // skip space and tab
                n++;
                continue;
            }
            if (c == '\r' || c == '\n') {  // newline
                n++;
                if (c == '\r' && get<char>(n) == '\n') n++;  // "\r\n" windows newline
                if (comment == 1) comment = 0;                  // end comment
                if (n <= dataSize()) {
                    // finish current line
                    line.numTokens = tokens.numEntries() - line.firstToken;
                    line.linenum = linei++;
                    if (line.numTokens) {  // save line if not empty                  
                        lines.push(line);
                    }                    
                    // start next line
                    line.type = 0;
                    line.file = filei;
                    line.beginPos = n;
                    line.firstToken = tokens.numEntries();
                }
                continue;
            } 
            // illegal control character
            token.type = TOK_ERR;
            line.type = LINE_ERROR;
            comment = 1;              // ignore rest of line
            m = tokens.push(token);     // save error token
            errors.report(n, 1, ERR_CONTROL_CHAR);
        }
        // prepare token of any type
        token.pos  = n;
        token.stringLength = 1;
        token.id   = 0;
        //token.column = n - line.beginPos;

        // is it a name?
        if (!comment && nameChar1(c)) {
            // start of a name
            m = n+1;
            while (m < dataSize() && nameChar2(get<char>(m))) m++;
            // name goes from position n to m-1. make token
            token.type = TOK_NAM;
            token.pos = n;
            token.stringLength = m - n;

            // is it a register name
            f = isRegister((char*)buf()+n, token.stringLength);
            if (f) {
                token.type = TOK_REG;
                token.id = f;
            }
            // is it a keyword?
            if (token.type == TOK_NAM && m-n < sizeof(keywSearch.name)) {
                memcpy(keywSearch.name, buf()+n, m-n);
                keywSearch.name[m-n] = 0;
                f = keywords.findFirst(keywSearch);
                if (f >= 0) {  // keyword found
                    token.id = keywords[f].id;
                    token.type = keywords[f].id >> 24;
                    if (token.id == HLL_SWITCH) numSwitch++;
                }
            }
            // is it an instruction?
            if (token.type == TOK_NAM && m-n < sizeof(instructSearch.name)) {
                memcpy(instructSearch.name, buf()+n, m-n);
                instructSearch.name[m-n] = 0;
                f = instructionlist.findFirst(instructSearch);
                if (f >= 0) {  // instruction name found
                    token.type = TOK_INS;
                    token.id = instructionlist[f].id;
                }
            }
            n = m;
            tokens.push(token);     // save token
            continue;
        }

        // Is it a number?
        if (!comment) {
            bool isFloat;
            f = isNumber((char*)buf() + n, dataSize() - n, &isFloat);
            if (f) {
                token.type = TOK_NUM + isFloat;
                token.id = n;               // save number as string. The value is extracted later
                token.stringLength = f;
                n += f;
                tokens.push(token);     // save token
                continue;
            }
        } 

        // is it an operator?
        opSearch.name[0] = c;
        opSearch.name[1] = 0;
        f = operators.findFirst(opSearch);
        if (f >= 0) {
            // found single-character operator
            // make a greedy search for multi-character operators
            i = f;
            for (i = f+1; (uint32_t)i < operators.numEntries(); i++) {
                if (operators[i].name[0] != c) break;
                if (memcmp((char*)buf()+n, operators[i].name, strlen(operators[i].name)) == 0) f = i;
            }
            token.type = TOK_OPR;
            token.id = operators[f].id;
            token.priority = operators[f].priority;
            token.stringLength = (uint32_t)strlen(operators[f].name);

            // search for operators that need consideration here
            switch (token.id) {

            case 39: case '"':  // quoted string in single or double quotes
                // search for end of string
                token.type = token.id == 39 ? TOK_CHA : TOK_STR;
                token.pos = n + 1;
                m = n;
                while (true) {
                    if (get<char>(m+1) == '\r' || get<char>(m+1) == '\n' || m == dataSize()) {
                        // end of line without matching end quote. multi-line quotes not allowed
                        token.type = TOK_ERR;
                        errors.report(token.pos-1, 1, ERR_QUOTE_BEGIN);
                        comment = 1; // skip rest of line
                        break;
                    }
                    if (get<char>(m+1) == c && get<char>(m) != '\\') {  // matching end quote not preceded by escape backslash
                        token.stringLength = m - n;
                        n += 2;
                        break;
                    }
                    m++;
                }
                break;

            case '/'+D2:            // "//". comment to end of line
                if (comment == 0) {                
                    comment = 1;    
                }
                break;
            case 'c':                                 // "/*" start of comment
                if (comment == 1) {
                    n += token.stringLength;         // skip and don't save token
                    continue;
                }
                if (comment == 2) {                     // nested comment
                    if (allowNestedComments) {
                        comment++;
                    }
                    else {
                        token.type = TOK_ERR;
                        errors.report(n, 2, ERR_COMMENT_BEGIN);
                    }
                    break;
                }
                comment = 2;
                commentStart = n;  commentStartColumn = n - line.beginPos;
                break;
            case 'd':                                // "*/" end of comment
                if (comment == 1) {
                    n += token.stringLength;         // skip and don't save token
                    continue;
                }
                if (comment == 2) {
                    comment = 0;
                    n += token.stringLength;         // skip and don't save token
                    continue;
                }
                else if (comment > 2 && allowNestedComments) {
                    comment--;
                    n += token.stringLength;         // skip and don't save token
                    continue;
                }
                else {
                    token.type = TOK_ERR;           // unmatched end comment
                    errors.report(n, 2, ERR_COMMENT_END);
                    comment = 1;
                }
                break;
            case ';':
                // semicolon starts a new pseudo-line
                if (comment) break;
                // finish current line
                tokens.push(token);     // the ';' token is used only in for(;;) loops. should be ignored at the end of the line otherwise
                n += token.stringLength;
                line.numTokens = tokens.numEntries() - line.firstToken;
                line.linenum = linei;
                if (line.numTokens) {  // save line if not empty                  
                    lines.push(line);
                }
                // start next line
                line.beginPos = n;
                line.firstToken = tokens.numEntries();
                continue;  // don't save ';' token twice
            case '{':  case '}':
                if (comment) break;
                // put each bracket in a separate pseudo-line to ease high level language parsing
                // finish current line
                line.numTokens = tokens.numEntries() - line.firstToken;
                line.linenum = linei; 
                if (line.numTokens) {  // save line if not empty                  
                    lines.push(line);
                }                    
                // start line with bracket only
                line.beginPos = n;
                line.firstToken = tokens.numEntries();
                tokens.push(token);      // save token
                n += token.stringLength;
                line.numTokens = 1;
                lines.push(line);
                // start line after bracket
                line.beginPos = n;
                line.firstToken = tokens.numEntries();
                continue;
            }
            if (comment == 0 && token.type != TOK_ERR) {            
                // save token unless we are inside a comment or an error has occurred           
                tokens.push(token);     // save token
            }
            n += token.stringLength;
            continue;
        }

        if (comment) {
            // we are inside a comment. Continue search only for end of line or end of comment
            n++;
            continue;
        }

        // none of the above. Make token for illegal character
        token.type = TOK_ERR;
        line.type = LINE_ERROR;
        errors.report(n, 1, ERR_ILLEGAL_CHAR);
        comment = 1;              // ignore rest of line
        n++;
    }
    // finish last line
   // tokens.push(token);
    line.numTokens = tokens.numEntries() - line.firstToken;
    lines.push(line);
    // start pseudo line
    line.beginPos = n;
    line.firstToken = tokens.numEntries();
    line.type = 0;

    // check for unmatched comment
    if (comment >= 2) {
        token.type = TOK_ERR;
        errors.report(commentStart, commentStartColumn, ERR_COMMENT_BEGIN);
    }
    // make EOF token in the end
    line.type = 0;
    line.beginPos = n; 
    line.firstToken = tokens.numEntries();
    line.numTokens = 1;
    lines.push(line);
    token.pos   = n;
    token.stringLength   = 0;
    token.type = TOK_EOF;    // end of file
    tokens.push(token);     // save eof token
}


void CAssembler::interpretSectionDirective() {
    // Interpret section directive during pass 2 or 3
    // pass 2: identify section name and type, and give it a number
    // pass 3: make section header

    // todo: nested sections

    uint32_t tok;                                          // token number
    ElfFWC_Sym2 sym;                                       // symbol record
    int32_t sectionsym = 0;                                // index to symbol record defining current section name
    uint32_t state = 0;                                    // 1: after align, 2: after '='
    ElfFwcShdr sectionHeader;                             // section header
    zeroAllMembers(sym);                                   // reset symbol
    zeroAllMembers(sectionHeader);                         // reset section header
    sectionHeader.sh_type = SHT_PROGBITS;                  // default section type

    sectionFlags = 0;
    for (tok = tokenB + 2; tok < tokenB + tokenN; tok++) { // get section attributes
        if (tokens[tok].type == TOK_ATT) {
            if (tokens[tok].id == ATT_UNINIT && state != 2) {
                sectionHeader.sh_type = SHT_NOBITS;                // uninitialized section (BSS)
                sectionFlags |= SHF_READ | SHF_WRITE;
            }
            else if (tokens[tok].id == ATT_COMDAT && state != 2) {
                sectionHeader.sh_type = SHT_COMDAT;                // communal section. duplicates and unreferenced sections are removed
            }
            else if (tokens[tok].id != ATT_ALIGN && state == 0) {
                sectionFlags |= tokens[tok].id & 0xFFFFFF;
                if (sectionFlags & SHF_EXEC) sectionFlags |= SHF_IP;  // executable section must be IP based
            }
            else if (tokens[tok].id == ATT_ALIGN && state == 0) {
                state = 1;
            }
            else {
                errors.report(tokens[tok]);  break;
            }
        }
        else if (tokens[tok].type == TOK_REG && tokens[tok].id == REG_IP    && state == 0) sectionFlags |= SHF_IP;
        else if (tokens[tok].type == TOK_REG && tokens[tok].id == REG_DATAP && state == 0) sectionFlags |= SHF_DATAP;
        else if (tokens[tok].type == TOK_REG && tokens[tok].id == REG_THREADP && state == 0) sectionFlags |= SHF_THREADP;
        else if (tokens[tok].type == TOK_OPR && tokens[tok].id == '=' && state == 1) state = 2;
        else if (tokens[tok].type == TOK_OPR && tokens[tok].id == ',' && state != 2) ; // comma, ignore
        else if (tokens[tok].type == TOK_NUM && state == 2) {
            if (pass >= 3) {  // alignment value
                uint32_t alignm = expression(tok, 1, 0).value.w;
                if ((alignm & (alignm - 1)) || alignm > MAX_ALIGN) errors.reportLine(ERR_ALIGNMENT);
                else {
                    sectionHeader.sh_align = bitScanReverse(alignm);
                }
            }
            state = 0;
        }
        else {
            errors.report(tokens[tok]);  break;
        }
    }
    // find or define symbol with section name
    sectionsym = findSymbol((char*)buf() + tokens[tokenB].pos, tokens[tokenB].stringLength);
    if (sectionsym <= 0) {
        // symbol not previously defined. Define it now
        sym.st_type = STT_SECTION;
        sym.st_name = symbolNameBuffer.putStringN((char*)buf() + tokens[tokenB].pos, tokens[tokenB].stringLength);
        sym.st_bind = sectionFlags;
        sectionsym = addSymbol(sym);         // save symbol with section name
    }
    else {
        // symbol already defined. check that it is a section name
        if (symbols[sectionsym].st_type != STT_SECTION) {
            errors.report(tokens[tokenB].pos, tokens[tokenB].stringLength, ERR_SYMBOL_DEFINED);
        }
    }
    sectionFlags |= SHF_ALLOC;
    lines[linei].type = LINE_SECTION;                          // line is section directive
    lines[linei].sectionType = sectionFlags;
    if (symbols[sectionsym].st_section == 0) {
        // new section. make section header
        sectionHeader.sh_name = symbols[sectionsym].st_name;
        if (sectionFlags & SHF_EXEC) {
            sectionHeader.sh_entsize = 4;
            if (sectionHeader.sh_align < 2) sectionHeader.sh_align = 2;
            sectionFlags |= SHF_IP;            
        }
        else { // data section
            if (!(sectionFlags & (SHF_READ | SHF_WRITE))) sectionFlags |= SHF_READ | SHF_WRITE; // read or write attributes not specified, default is both
            if (!(sectionFlags & (SHF_IP | SHF_DATAP | SHF_THREADP))) {  // address reference not specified. assume datap if writeable, ip if readonly
                if (sectionFlags & SHF_WRITE) sectionFlags |= SHF_DATAP;
                else sectionFlags |= SHF_IP;
            }
        }
        sectionHeader.sh_flags = sectionFlags;
        section = sectionHeaders.push(sectionHeader);
        symbols[sectionsym].st_section = section;
    }
    else {  // this section is seen before
        section = symbols[sectionsym].st_section;
        if (sectionHeaders[section].sh_align < sectionHeader.sh_align) sectionHeaders[section].sh_align = sectionHeader.sh_align;
        if (sectionFlags && (sectionFlags & ~sectionHeaders[section].sh_flags)) errors.reportLine(ERR_SECTION_DIFFERENT_TYPE);
        sectionFlags = (uint32_t)sectionHeaders[section].sh_flags;
        if (sectionHeader.sh_align > 2) {
            // insert alignment code
            SCode code;
            zeroAllMembers(code);
            code.instruction = II_ALIGN;
            code.value.u = (int64_t)1 << sectionHeader.sh_align;
            code.sizeUnknown = 0x80;
            code.section = section;
            codeBuffer.push(code);
        }
    }
}

void CAssembler::interpretFunctionDirective() {
    // Interpret function directive during pass 2
    uint32_t tok;                     // token number
    ElfFWC_Sym2 sym;                  // symbol record
    zeroAllMembers(sym);              // reset symbol
    int32_t symi;

    symi = findSymbol((char*)buf() + tokens[tokenB].pos, tokens[tokenB].stringLength);
    if (symi > 0) {
        if (pass == 2) errors.report(tokens[tokenB].pos, tokens[tokenB].stringLength, ERR_SYMBOL_DEFINED);  // symbol already defined
    }
    else {
        // define symbol
        sym.st_type = STT_FUNC;
        sym.st_other = STV_IP;
        sym.st_name = symbolNameBuffer.putStringN((char*)buf() + tokens[tokenB].pos, tokens[tokenB].stringLength);
        sym.st_bind = 0;
        sym.st_section = section;
        for (tok = tokenB + 2; tok < tokenB + tokenN; tok++) { // get function attributes
            if (tokens[tok].type == TOK_OPR && tokens[tok].id == ',') continue;
            if (tokens[tok].id == ATT_WEAK) sym.st_bind |= STB_WEAK;             
            if (tokens[tok].id == ATT_REGUSE) {
                if (tokens[tok+1].id == '=' && tokens[tok+2].type == TOK_NUM) {
                    tok += 2;
                    sym.st_reguse1 = expression(tok, 1, 0).value.w;
                    sym.st_other |= STV_REGUSE;
                    if (tokens[tok+1].id == ',' && tokens[tok+2].type == TOK_NUM) {
                        tok += 2;
                        sym.st_reguse2 = expression(tok, 1, 0).value.w;
                    }
                }             
            }
            else if (tokens[tok].type == TOK_DIR && tokens[tok].id == DIR_PUBLIC) sym.st_bind |= STB_GLOBAL;
            else {
                errors.report(tokens[tok]);  // unexpected token
            }
        }
        symi = addSymbol(sym);          // save symbol with function name
    }
    lines[linei].type = LINE_FUNCTION;           // line is function directive

    if (pass == 3 && symi) {
        // make a label here. The final address will be calculated in pass 4
        SCode code;                              // current instruction code
        zeroAllMembers(code);                    // reset code structure
        code.label = symbols[symi].st_name;
        code.section = section;
        codeBuffer.push(code);
    }
}

void CAssembler::interpretEndDirective() {
    // Interpret section or function end directive during pass 2
    ElfFWC_Sym2 sym;                  // symbol record
    zeroAllMembers(sym);              // reset symbol
    int32_t symi;
    CTextFileBuffer tempBuffer;       // temporary storage of names

    symi = findSymbol((char*)buf() + tokens[tokenB].pos, tokens[tokenB].stringLength);
    if (symi <= 0) {
        errors.reportLine(ERR_UNMATCHED_END);
    }
    else {
        if (symbols[symi].st_type == STT_SECTION) {
            if (symbols[symi].st_section == section) {
                // current section ends here
                section = 0;  sectionFlags = 0;
            }
            else {
                errors.reportLine(ERR_UNMATCHED_END);
            }
        }
        else if (symbols[symi].st_type == STT_FUNC && pass >= 4) {
            symbols[symi].st_unitsize = 4;  
            // todo: insert size
            //symbols[symi].st_unitsize = ?
            // support function(){} syntax. prevent nested functions
        }
    }
    lines[linei].type = LINE_ENDDIR;        // line is end directive
}

// Find symbol by index into symbolNameBuffer. The return value is an index into symbols. 
// Symbol indexes may change when new symbols are added to the symbols list, which is sorted by name
uint32_t CAssembler::findSymbol(uint32_t namei) {
    ElfFWC_Sym2 sym;                                       // temporary symbol record used for searching
    sym.st_name = namei;
    return symbols.findFirst(sym);                         // find symbol by name
} 

// Find symbol by name as string. The return value is an index into symbols. 
// Symbol indexes may change when new symbols are added to the symbols list, which is sorted by name
uint32_t CAssembler::findSymbol(const char * name, uint32_t len) {
    uint32_t saveSize = symbolNameBuffer.dataSize();       // save symbolNameBuffer size for later reset
    uint32_t namei = symbolNameBuffer.putStringN(name, len); // put name temporarily into symbolNameBuffer
    int32_t symi = findSymbol(namei);                      // find symbol by name index
    symbolNameBuffer.setSize(saveSize);                    // remove temporary name from symbolNameBuffer
    return symi;                                           // return symbol index
}

// Add a symbol to symbols list
uint32_t CAssembler::addSymbol(ElfFWC_Sym2 & sym) {
    int32_t f = symbols.findFirst(sym);
    if (f >= 0) {
        // error: symbol already defined
        return 0;
    }
    else {
        return symbols.addUnique(sym);
    }
}

// interpret   name: options {, name: options}
void CAssembler::interpretExternDirective() {
    uint32_t tok;                     // token number
    uint32_t nametok = 0;             // last name token
    ElfFWC_Sym2 sym;                  // symbol record
    zeroAllMembers(sym);              // reset symbol
    sym.st_bind = STB_GLOBAL;

    // Example: extern name1: int32 weak, name2: function, name3, name4: read 
    uint32_t state = 0;  // 0: after extern or comma, 
                         // 1: after name, 
                         // 2: after colon

    // loop through tokens on this line
    for (tok = tokenB + 1; tok < tokenB + tokenN; tok++) {
        switch (state) {
        case 0:  // after extern or comma. expecting name
            if (tokens[tok].type == TOK_NAM) {
                // name encountered
                sym.st_name = symbolNameBuffer.putStringN((char*)buf()+tokens[tok].pos, tokens[tok].stringLength);
                state = 1;  nametok = tok;
            }
            else errors.report(tokens[tok]);
            break;
        case 1: // after name. expecting colon or comma
            if (tokens[tok].type == TOK_OPR) {
                if (tokens[tok].id == ':') {
                    state = 2;
                    continue;
                }
                else if (tokens[tok].id == ',') {
                    goto COMMA;
                }
            }
            errors.report(tokens[tok]);
            break;
        case 2:  // after colon. expecting attribute or comma or end of line
            if (tokens[tok].type == TOK_TYP) {
                // symbol size given by type token
                uint32_t s = tokens[tok].id & 0xF;
                if (s > 4) s -= 3;  // float types
                sym.st_unitsize = uint32_t(1 << s);
                sym.st_unitnum = 1;
            }
            else if (tokens[tok].type == TOK_ATT || tokens[tok].type == TOK_DIR) {
                ATTRIBUTE:
                switch (tokens[tok].id) {
                case DIR_FUNCTION: case ATT_EXEC: // function or execute
                    if (sym.st_type) {
                        errors.report(tokens[tok].pos, tokens[tok].stringLength, ERR_CONFLICT_TYPE);
                    } 
                    sym.st_type = STT_FUNC;
                    sym.st_other = STV_IP | STV_EXEC;
                    break;
                case ATT_READ:  // read
                    if (sym.st_type == 0) sym.st_other |= STV_READ;
                    break;
                case ATT_WRITE:  // write
                    if (sym.st_type == STT_FUNC) {
                        errors.report(tokens[tok].pos, tokens[tok].stringLength, ERR_CONFLICT_TYPE);
                    } 
                    else {
                        sym.st_type = STT_OBJECT;
                    }
                    break;
                case ATT_WEAK:   // weak
                    sym.st_bind = STB_WEAK;
                    break;
                case ATT_CONSTANT:  // constant
                    sym.st_type = STT_CONSTANT;
                    break;
                case ATT_REGUSE:
                    if (tokens[tok+1].id == '=' && (tokens[tok+2].type == TOK_NUM /*|| tokens[tok+2].type == TOK_OPR)*/)) {
                        tok += 2;
                        sym.st_reguse1 = expression(tok, 1, 0).value.w;
                        sym.st_other |= STV_REGUSE;
                        if (tokens[tok+1].id == ',' && tokens[tok+2].type == TOK_NUM) {
                            tok += 2;
                            sym.st_reguse2 = expression(tok, 1, 0).value.w;
                        }
                    }
                    break;
                default:  // error
                    errors.report(tokens[tok]);
                }
            }
            else if (tokens[tok].type == TOK_REG) {
                switch (tokens[tok].id) {
                case REG_IP:  
                    sym.st_other |= STV_IP;  break;
                case REG_DATAP:
                    sym.st_other |= STV_DATAP;  break;
                case REG_THREADP:
                    sym.st_other |= STV_THREADP;  break;
                default: errors.report(tokens[tok]);
                }
            }
            else if (tokens[tok].type == TOK_OPR && tokens[tok].id == ',') {
                // end of definition. save symbol
            COMMA:
                if (tok < tokenB + tokenN 
                    && (tokens[tok + 1].type == TOK_ATT || tokens[tok + 1].type == TOK_DIR)) {
                    tok++; goto ATTRIBUTE;
                }
                uint32_t symi = addSymbol(sym);          // save symbol with function name
                if (symi == 0) {  // symbol already defined
                    errors.report(tokens[nametok].pos, tokens[nametok].stringLength, ERR_SYMBOL_DEFINED);
                }
                sym.st_name = 0;          // clear record for next symbol
                sym.st_type = 0;
                sym.st_other = 0;
                sym.st_unitsize = 0;
                sym.st_unitnum = 0;
                sym.st_bind = STB_GLOBAL;
                state = 0;
            }
            else {
                errors.report(tokens[tok]);
            }
            break;
        }
    }
    if (state) {  // last extern definition does not end with comma. finish it here
        goto COMMA;
    }
    lines[linei].type = LINE_DATADEF;        // line is data definition
}


void CAssembler::interpretLabel(uint32_t tok) {
    // line begins with a name. interpret label
    // todo: add type if data. not string type
    ElfFWC_Sym2 sym;   // symbol record
    zeroAllMembers(sym); // reset symbol

    // save name
    sym.st_name = symbolNameBuffer.putStringN((char*)buf()+tokens[tok].pos, tokens[tok].stringLength);
    sym.st_section = section;
    // determine if code or data from section type
    if (sectionFlags & SHF_EXEC) {
        sym.st_type = STT_FUNC;
        sym.st_other = STV_EXEC | STV_IP;
    }
    else {
        sym.st_type = STT_OBJECT;
        sym.st_other = sectionFlags & STV_SECT_ATTR;
    }

    // look for more exact type information
    if (tokenN > 2) {
        uint32_t t = tok+2;
        if (tokens[t].type == TOK_TYP) {
            uint32_t s = tokens[t].id & 0xF;
            if (s > 4) s -= 3;
            sym.st_unitsize = uint32_t(1 << s);
            sym.st_unitnum = 1;
            if (tokenN > 3) t++;
        }
        if (tokens[t].type == TOK_NUM || tokens[t].type == TOK_FLT) {
            sym.st_type = STT_OBJECT;
            lines[linei].type = LINE_DATADEF;
        }
        else if (tokens[t].type == TOK_REG || tokens[t].type == TOK_INS || tokens[t].id == '[') {
            lines[linei].type = LINE_CODEDEF;
            sym.st_type = STT_FUNC;
        }
    }
    if (section) { // copy type info from section
        sym.st_other = sectionHeaders[section].sh_flags & STV_SECT_ATTR;
    }

    if (lines[linei].type == 0) {
        lines[linei].type = (sectionFlags & SHF_EXEC) ? LINE_CODEDEF : LINE_DATADEF;
    }

    uint32_t symi = addSymbol(sym);     // add symbol to symbols list

    if (section) {
        // symbol address
        symbols[symi].st_value = sectionHeaders[section].sh_size;
    }
    tokens[tok].id = symbols[symi].st_name;         // save symbol name index
    if (symi == 0) errors.report(tokens[tokenB].pos, tokens[tokenB].stringLength, ERR_SYMBOL_DEFINED);
}


// interpret assembly style variable definition:
// label: type value1, value2
void CAssembler::interpretVariableDefinition1() {
    int state = 0;      // 0: start
                        // 1: after label
                        // 2: after :
                        // 3: after type or ,
                        // 4: after value
    uint32_t tok;                      // token index
    uint32_t type = 0;                 // data type
    uint32_t dsize = 0;                // data size
    uint32_t dsize1;                   // log2(dsize)
    uint32_t dnum = 0;                 // number of data items
    uint32_t stringlen = 0;            // length of string
    uint32_t symi = 0;                 // symbol index
    ElfFWC_Sym2 sym;                   // symbol record
    zeroAllMembers(sym);               // reset symbol
    SExpression exp1;                  // expression when interpreting numeric expression

    if (section == 0) {
        errors.reportLine(ERR_DATA_WO_SECTION);
    }

    // loop through tokens on this line
    for (tok = tokenB; tok < tokenB + tokenN; tok++) {
        switch (state) {
        case 0:  // start
            if (tokens[tok].type == TOK_NAM) { // name. make symbol
                sym.st_name = symbolNameBuffer.putStringN((char*)buf()+tokens[tok].pos, tokens[tok].stringLength);
                sym.st_type = STT_OBJECT;
                symi = symbols.addUnique(sym);
                tokens[tok].type = TOK_SYM;      // change token type
                tokens[tok].id = symbols[symi].st_name;  // use name offset as unique identifier because symbol index can change
                state = 1;
            }
            else if (tokens[tok].type == TOK_SYM) { // symbol
                symi = findSymbol(tokens[tok].id);
                if (symi > 0) {
                    if (pass == 2) errors.report(tokens[tok].pos, tokens[tok].stringLength, ERR_SYMBOL_DEFINED);  // symbol already defined
                }
                state = 1;
            }
            else if (tokens[tok].type == TOK_TYP) {
                goto TYPE_TOKEN;
            }
            else errors.report(tokens[tok]);
            if (symi && section) {
                symbols[symi].st_value = sectionHeaders[section].sh_size;
            }            
            break;
        case 1:  // after label. expect colon
            if (tokens[tok].type == TOK_OPR && tokens[tok].id == ':') {
                state = 2;
            }
            else errors.report(tokens[tok].pos, tokens[tok].stringLength, ERR_EXPECT_COLON);
            break;
        case 2:  // expect type
            if (tokens[tok].type == TOK_TYP) {
                TYPE_TOKEN:
                type = tokens[tok].id & 0xFF;
                dsize1 = type & 0xF;
                if (type & 0x40) dsize1 -= 3;
                dsize = 1 << dsize1;
                state = 3; 
                if (section) {  // align data
                    uint32_t addr = (uint32_t)sectionHeaders[section].sh_size;
                    if (sectionHeaders[section].sh_align < dsize1) sectionHeaders[section].sh_align = dsize1;  // update section alignment
                    if (addr & (dsize - 1)) { // needs to insert zeroes
                        uint32_t addr2 = (addr + dsize - 1) & -(int32_t)dsize;
                        sectionHeaders[section].sh_size = addr2;           // update address
                        if (symi) symbols[symi].st_value = addr2;          // update symbol address
                        if (pass >= 3) {                        
                            dataBuffers[section].align((uint32_t)dsize);   // put zeroes in data buffer
                        }                        
                    }
                }
            }
            else errors.report(tokens[tok]);
            break;
        case 3:  // after type. expect value. evaluate expression
            exp1 = expression(tok, tokenB + tokenN - tok, pass < 3 ? 0x10 : 0); // pass 3: may contain symbols not defined yet
            tok += exp1.tokens - 1;
            if (exp1.etype & XPR_STRING) {  // string expression: get size
                if ((type & 0x1F) != (TYP_INT8 & 0x1F)) errors.reportLine(ERR_STRING_TYPE);  // string must use type int8
                stringlen = exp1.sym2; // string length
            }
            else stringlen = 0;
            if (pass < 3) {
                if (section) sectionHeaders[section].sh_size += stringlen ? stringlen : dsize;  // update address
            }
            else {
                if (section) {
                    // save data of desired type
                    if (exp1.etype & XPR_FLT) { 
                        // floating point number specified
                        if ((type & 0xF0) == (TYP_INT8 & 0xF0)) {  // float specified, integer expected
                            exp1.value.i = int64_t(exp1.value.d);
                            errors.reportLine(ERR_CONFLICT_TYPE);
                        }
                    }
                    else if (exp1.etype & XPR_INT) { 
                        if (type & TYP_FLOAT) {  // integer specified, float expected
                            exp1.value.d = double(exp1.value.i);  // convert to float
                        }
                    }
                    int64_t value = exp1.value.i;  //value of expression
                    if (exp1.sym1) {                                                                                      
                        // calculation of symbol value. add relocation if needed
                        uint32_t size = type & 0xF;
                        if (type & 0x40) size -= 3;
                        size = 1 << size;
                        //value = calculateConstantOperand(exp1, dataBuffers[section].dataSize(), size);                            
                        value = calculateConstantOperand(exp1, sectionHeaders[section].sh_size, dsize);                            
                        if (exp1.etype & XPR_ERROR) {
                            errors.reportLine((uint32_t)value); // report error
                            break;
                        }
                    }
                    if (sectionHeaders[section].sh_type == SHT_NOBITS) {
                        // uninitialized (BSS) section. check that value is zero, but don't store
                        if (value != 0) errors.reportLine(ERR_NONZERO_IN_BSS); // not zero
                    }
                    else {
                        // save data
                        switch (type & 0xFF) {
                        case TYP_INT8 & 0xFF:
                            if (stringlen) {
                                dataBuffers[section].push(stringBuffer.buf() + exp1.value.w, stringlen);
                                break;
                            }
                            dataBuffers[section].push(&value, 1);  break;
                        case TYP_INT16 & 0xFF:
                            dataBuffers[section].push(&value, 2);  break;
                        case TYP_INT32 & 0xFF:
                            dataBuffers[section].push(&value, 4);  break;
                        case TYP_INT64 & 0xFF:
                            dataBuffers[section].push(&value, 8);  break;
                        case TYP_INT128 & 0xFF:
                            dataBuffers[section].push(&value, 8);
                            value = value >> 63;     // sign extend
                            dataBuffers[section].push(&value, 8);
                            break;
                        case TYP_FLOAT16 & 0xFF:  // half precision
                            exp1.value.w = double2half(exp1.value.d);
                            dataBuffers[section].push(&exp1.value.w, 2);  break;
                        case TYP_FLOAT32 & 0xFF: { // single precision
                            float val = float(exp1.value.d);
                            dataBuffers[section].push(&val, 4); }
                            break;
                        case TYP_FLOAT64 & 0xFF:  // double precision
                            dataBuffers[section].push(&exp1.value.d, 8);  break;
                        }
                    }
                    sectionHeaders[section].sh_size += stringlen ? stringlen : dsize;  // update address
                }
            }
            if (!(exp1.etype & (XPR_IMMEDIATE | XPR_STRING | XPR_SYM1 | XPR_UNRESOLV)) || (exp1.etype & (XPR_REG|XPR_OPTION|XPR_MEM|XPR_ERROR))) errors.report(tokens[tok]);

            if (stringlen) dnum += stringlen; else dnum += 1;
            state = 4;
            break;
        case 4:  // after value. expect comma or end of line
            if (tokens[tok].type == TOK_OPR && tokens[tok].id == ',') {
                state = 3;
            }
            else errors.report(tokens[tok]);
            break;
        }
        if (lineError) return;
    }
    if (state != 4 && state != 2) errors.report(tokens[tok-1]);
    if (symi) { // save size
        symbols[symi].st_unitsize = dsize;
        symbols[symi].st_unitnum = dnum;
        symbols[symi].st_section = section;
        if ((type & 0xF0) == (TYP_FLOAT32 & 0xF0)) symbols[symi].st_other |= STV_FLOAT;
        if (section) { // copy information from section
            symbols[symi].st_other |= sectionHeaders[section].sh_flags & STV_SECT_ATTR;
        }
    }
}

// interpret C style variable definition:
// type name1 = value1, name2[num] = {value, value, ..}
void CAssembler::interpretVariableDefinition2() {
    int state = 0;      // 0: start
                        // 1: after type or comma
                        // 2: after name
                        // 3: after [
                        // 4: after [number
                        // 5: after =
                        // 6: after = number
                        // 7: after {
                        // 8: after {number

    uint32_t tok;                           // token index
    uint32_t dsize = 0;                     // data element size
    uint32_t dsize1 = 0;                    // data element size = 1 << dsize1
    uint32_t type = 0;                      // data type
    uint32_t arrayNum1 = 1;                 // number of elements indicated in []
    uint32_t arrayNum2 = 0;                 // number of elements in {} list
    uint32_t stringlen = 0;                 // length of string
    uint32_t symi = 0;                      // symbol index
    ElfFWC_Sym2 sym;                        // symbol record
    zeroAllMembers(sym);                    // reset symbol
    SExpression exp1;                       // expression when interpreting numeric expression

    if (section == 0) {
        errors.reportLine(ERR_DATA_WO_SECTION);
    }
    //!!
    pass += 0;

    // loop through tokens on this line
    for (tok = tokenB; tok < tokenB + tokenN; tok++) {
        switch (state) {
        case 0:  // this is a type token
            type  = tokens[tok].id & 0xFF;
            dsize1 = tokens[tok].id & 0xF;  
            if ((type & 0x40) > 3) dsize1 -= 3;
            dsize = 1 << dsize1;
            state = 1;
            if (section) {  // align data
                uint32_t addr = (uint32_t)sectionHeaders[section].sh_size;
                if (addr & (dsize - 1)) { // needs to insert zeroes
                    uint32_t addr2 = (addr + dsize - 1) & -(int32_t)dsize;  // calculate aligned address
                    sectionHeaders[section].sh_size = addr2;           // update address
                    if (pass >= 3) {                    
                        dataBuffers[section].align(dsize);   // put zeroes in data buffer
                    }
                }
                if (sectionHeaders[section].sh_align < dsize1) sectionHeaders[section].sh_align = dsize1;  // update section alignment
            }
            break;
        case 1:  // expecting name token. save name
            if (tokens[tok].type == TOK_NAM) { // name. make symbol
                sym.st_name = symbolNameBuffer.putStringN((char*)buf()+tokens[tok].pos, tokens[tok].stringLength);
                symi = addSymbol(sym);
                if (symi == 0 && pass == 2) {
                    errors.report(tokens[tok].pos, tokens[tok].stringLength, ERR_SYMBOL_DEFINED);  break;
                }
                symbols[symi].st_type = (sectionFlags & SHF_EXEC) ? STT_FUNC : STT_OBJECT;
                tokens[tok].type = TOK_SYM;      // change token type
                tokens[tok].id = symbols[symi].st_name;  // use name offset as unique identifier because symbol index can change
                state = 2;
            }
            else if (tokens[tok].type == TOK_SYM) { // symbol
                symi = findSymbol(tokens[tok].id);
                if (symi > 0 && pass == 2) errors.report(tokens[tok].pos, tokens[tok].stringLength, ERR_SYMBOL_DEFINED);  // symbol already defined
                state = 2;
            }
            else {
                errors.report(tokens[tok]);
            }
            //nametok = tok;
            symbols[symi].st_unitsize = dsize;
            symbols[symi].st_unitnum = 0;

            if ((type & 0xF0) == (TYP_FLOAT32 & 0xF0)) symbols[symi].st_other |= STV_FLOAT;
            if (section) { // copy information from section
                symbols[symi].st_value = sectionHeaders[section].sh_size;
                symbols[symi].st_other |= sectionHeaders[section].sh_flags & STV_SECT_ATTR;
            }
            break;
        case 2:  // after name. expect , = [ eol
            if (tokens[tok].type != TOK_OPR) {
                errors.report(tokens[tok]);  break;
            }
            switch (tokens[tok].id) {
            case ',':  // finish this symbol definition
                COMMA:
                    if (arrayNum2 > arrayNum1) { // check if the two array sizes match
                        if (arrayNum1 > 1) {
                            errors.report(tokens[tok-1].pos, tokens[tok-1].stringLength, ERR_CONFLICT_ARRAYSZ);
                        } 
                        else arrayNum1 = arrayNum2;
                    }
                    symbols[symi].st_unitsize = dsize;
                    symbols[symi].st_unitnum = arrayNum1;
                    symbols[symi].st_reguse1 = linei;
                    symbols[symi].st_section = section;

                    if (arrayNum1 > arrayNum2 && section) {
                        // unspecified elements are zero. calculate extra size
                        uint32_t asize = (arrayNum1 - arrayNum2) * dsize;
                        sectionHeaders[section].sh_size += asize;
                        if (pass >= 3 && sectionHeaders[section].sh_type != SHT_NOBITS) {
                            // store any unspecified elements as zero
                            uint64_t zero = 0;
                            while (asize > 8) {
                                dataBuffers[section].push(&zero, 8);  asize -= 8;
                            }
                            while (asize > 0) {
                                dataBuffers[section].push(&zero, 1);  asize -= 1;
                            }
                        }
                    }

                    // get ready for next symbol
                    zeroAllMembers(sym);
                    arrayNum1 = 1;  arrayNum2 = 0;
                    if (state == 99) return;       // finished line
                    state = 1;
                    break;                
            case '=':
                state = 5;
                break;
            case '[':
                state = 3;
                break;
            default:
                errors.report(tokens[tok]);
            }
            break;
        case 3:  // after [ . expect number or ]
            if (tokens[tok].id == ']') {
                state = 2; break;
            }
            if (arrayNum1 > 1) {
                errors.report(tokens[tok].pos, tokens[tok].stringLength, ERR_MULTIDIMENSIONAL);  break;            // error. multidimensional array not supported
            } 
            // evaluate numeric expression inside []. 
            // it may contain complex expressions that can only be evaluated later, but
            // this will not generate an error message here
            exp1 = expression(tok, tokenB + tokenN - tok, 0x10);
            if (lineError) return;
            tok += exp1.tokens -1;
            if (exp1.etype == 0) errors.report(tokens[tok]);
            if ((exp1.etype & ~XPR_IMMEDIATE) == 0) {
                arrayNum1 = exp1.value.w;
            }
            state = 4;
            break;
        case 4:  // after [number. expect ]
            if (tokens[tok].id != ']') {
                errors.report(tokens[tok]);  break;
            }
            state = 2;
            break;
        case 5:  // after =. expect number or {numbers}
            if (tokens[tok].id == '{') state = 7;
            else {
                state = 6;
                goto SAVE_VALUE;  // interpret value and save it
            }
            break;
        case 6:  // after = number. expect comma or eol
            if (tokens[tok].id != ',') {
                errors.report(tokens[tok]);  break;
            }
            goto COMMA;
        case 7:  // after {. expect number list
            state = 8;
        SAVE_VALUE:
            arrayNum2++;
            if (pass < 3) {
                // may contain symbols not defined yet. just pass expression and count tokens
                exp1 = expression(tok, tokenB + tokenN - tok, 0x10);
                tok += exp1.tokens - 1;
                if (lineError) return;
            }
            else {
                // pass 5. evaluate expression and save value
                exp1 = expression(tok, tokenB + tokenN - tok, 0);
                tok += exp1.tokens - 1;
                if (lineError) return;
                //int64_t value = exp1.value.i;  //value of expression
                if ((exp1.etype & XPR_SYM1) && exp1.sym1 && pass > 3) {                                                                                      
                    // calculation of symbol value. add relocation if needed
                    exp1.value.i = calculateConstantOperand(exp1, sectionHeaders[section].sh_size, dsize);                            
                    if (exp1.etype & XPR_ERROR) {
                        errors.reportLine((uint32_t)(exp1.value.i)); // report error
                        break;
                    }
                }
            }
            if (!(exp1.etype & (XPR_IMMEDIATE | XPR_STRING | XPR_UNRESOLV | XPR_SYM1)) || (exp1.etype & (XPR_REG|XPR_OPTION|XPR_MEM|XPR_ERROR))) {
                errors.report(tokens[tok]);
            }
            if (section && section < dataBuffers.numEntries() && pass >= 3) {
                // save data of desired type
                if ((exp1.etype & XPR_IMMEDIATE) == XPR_FLT) { 
                    // floating point number specified
                    if ((type & 0xF0) == (TYP_INT8 & 0xF0)) {  // float specified, integer expected
                        exp1.value.i = int64_t(exp1.value.d);
                        errors.reportLine(ERR_CONFLICT_TYPE);
                    }
                }
                else if ((exp1.etype & XPR_IMMEDIATE) == XPR_INT) { 
                    if ((type & 0xF0) == (TYP_FLOAT32 & 0xF0)) {  // integer specified, float expected
                        exp1.value.d = double(exp1.value.i);  // convert to float
                    }
                }
                else if (exp1.etype & XPR_STRING) {  // string expression: get size
                    if ((type & 0x1F) != (TYP_INT8 & 0x1F)) errors.reportLine(ERR_STRING_TYPE);  // string must use type int8
                    stringlen = exp1.sym2; // string length
                }
                else stringlen = 0;

                if (sectionHeaders[section].sh_type == SHT_NOBITS) {
                    // uninitialized (BSS) section. check that value is zero, but don't store
                    if (exp1.value.i != 0) errors.reportLine(ERR_NONZERO_IN_BSS); // not zero
                }
                else {
                    // save data
                    switch (type & 0xFF) {
                    case TYP_INT8 & 0xFF:
                        if (stringlen) {
                            dataBuffers[section].push(stringBuffer.buf() + exp1.value.w, stringlen);
                            break;
                        }
                        dataBuffers[section].push(&exp1.value.u, 1);  break;
                    case TYP_INT16 & 0xFF:
                        dataBuffers[section].push(&exp1.value.u, 2);  break;
                    case TYP_INT32 & 0xFF:
                        dataBuffers[section].push(&exp1.value.u, 4);  break;
                    case TYP_INT64 & 0xFF:
                        dataBuffers[section].push(&exp1.value.u, 8);  break;
                    case TYP_INT128 & 0xFF:
                        dataBuffers[section].push(&exp1.value.u, 8);
                        exp1.value.i = exp1.value.i >> 63;     // sign extend
                        dataBuffers[section].push(&exp1.value.u, 8);
                        break;
                    case TYP_FLOAT16 & 0xFF:  // half precision
                        exp1.value.w = double2half(exp1.value.d);
                        dataBuffers[section].push(&exp1.value.w, 2);  break;
                    case TYP_FLOAT32 & 0xFF: { // single precision
                        float val = float(exp1.value.d);
                        dataBuffers[section].push(&val, 4); }
                                             break;
                    case TYP_FLOAT64 & 0xFF:  // double precision
                        dataBuffers[section].push(&exp1.value.d, 8);  break;
                    }
                }
            }
            sectionHeaders[section].sh_size += stringlen ? stringlen : dsize;  // update address
            break;
        case 8:  // after {number. expect comma or }
            if (tokens[tok].id == ',') state = 7;
            else if (tokens[tok].id == '}') state = 6;
            else {
                errors.report(tokens[tok]);  break;
            }
        }
        if (tok + 1 == tokenB + tokenN && (state == 5 || state >= 7) && linei + 1 < lines.numEntries()) {
            // no more tokens. statement with {} can span multiple lines
            if (state == 5) {
                // after '='. expect next line to be '{'
                uint32_t tokNext = lines[linei+1].firstToken;
                if (tokens[tokNext].type != TOK_OPR || tokens[tokNext].id != '{') break; // anything else: break out of loop and get error message
            }
            // append next line
            lines[linei].type = LINE_DATADEF;
            linei++;
            tokenN += lines[linei].numTokens;
        }

    }
    // no more tokens
    if (state == 2 || state == 6) {
        // finish this definition
        lines[linei].type = LINE_DATADEF;
        state = 99; goto COMMA;
    }
    errors.report(tokens[tok-1].pos, tokens[tok-1].stringLength, ERR_UNFINISHED_VAR); 
}

// check if line is code or data
void CAssembler::determineLineType() {
    uint32_t tok;                           // current token
    uint32_t elements = 0;                  // detect type and constant tokens
    
    // loop through tokens on this line
    for (tok = tokenB; tok < tokenB + tokenN; tok++) {
        if (tokens[tok].type == TOK_REG || tokens[tok].type == TOK_INS || tokens[tok].type == TOK_XPR || tokens[tok].type == TOK_HLL) {
            lines[linei].type = LINE_CODEDEF;  return;     // register or instruction found. must be code
        }
        if (tokens[tok].type == TOK_TYP) elements |= 1;
        if (tokens[tok].type == TOK_NUM || tokens[tok].type == TOK_FLT || TOK_CHA || TOK_STR) elements |= 2;
    }
    if (elements == 3)  lines[linei].type = LINE_DATADEF;
    else if (tokens[tokenB].type == TOK_ATT && tokens[tokenB].id == ATT_ALIGN) {  // align directive
        lines[linei].type = (sectionFlags & SHF_EXEC) ? LINE_CODEDEF : LINE_DATADEF;
    }
    else if (tokens[tokenB].type == TOK_EOF) lines[linei].type = 0;   // end of file
    else if (tokenN == 1 && tokens[tokenB].type == TOK_OPR && linei > 1) {
        // {} bracket. same type as previous line
        lines[linei].type = lines[linei-1].type;
    }
    else if (tokens[tokenB].type == TOK_OPR && tokens[tokenB].id == '%') {
        // metaprogramming code
        lines[linei].type = LINE_METADEF;
    }
    else {
        //errors.reportLine
        errors.report(tokens[tokenB]);
        lines[linei].type = LINE_ERROR;
    }
}

// interpret data or code alignment directive
void CAssembler::interpretAlign() {
    if (section) {
        uint32_t addr = (uint32_t)sectionHeaders[section].sh_size;
        SExpression exp1 = expression(tokenB+1, tokenN - 1, pass < 3 ? 0x10 : 0);
        if (exp1.tokens < tokenN - 1) {errors.report(tokens[tokenB+1+exp1.tokens]); return;}
        if ((exp1.etype & XPR_IMMEDIATE) != XPR_INT || (exp1.etype & (XPR_STRING | XPR_REG | XPR_OP | XPR_MEM | XPR_OPTION))) {
            errors.report(tokens[tokenB+1]);  return;
        }
        uint64_t alignm = exp1.value.u;
        if ((alignm & (alignm - 1)) || alignm > MAX_ALIGN) {errors.reportLine(ERR_ALIGNMENT);  return;}
        uint32_t log2ali = bitScanReverse(alignm);
        if (sectionHeaders[section].sh_align < log2ali) {
            sectionHeaders[section].sh_align = log2ali;  // make sure section alignment is not less
        }
        if (addr & ((uint32_t)alignm - 1)) { // needs to insert zeroes
            uint32_t addr2 = (addr + (uint32_t)alignm - 1) & -(int32_t)alignm;
            sectionHeaders[section].sh_size = addr2;           // update address
            if (pass >= 3) {
                dataBuffers[section].align((uint32_t)alignm);  // put zeroes in data buffer
            }
        }
    }
}

// Pass 3 does three things. 
// A. Handle metaprogramming directives
// B. Classify lines
// C. Identify symbol names, sections, labels, functions 
// These must be done in parallel because metaprogramming directives can refer to previously 
// defined symbols, and data/code definitions can involve metaprogramming variables and macros

void CAssembler::pass2() {
    ElfFWC_Sym2 sym;                  // symbol record
    zeroAllMembers(sym);              // reset symbol
    symbols.push(sym);                // symbol record 0 is empty
    symbolNameBuffer.put((char)0);    // put dummy zero to avoid zero offset at next string
    sectionFlags = 0;
    section = 0;

    // lines loop
    for (linei = 1; linei < lines.numEntries(); linei++) {
        lineError = 0;
        tokenB = lines[linei].firstToken;      // first token in line        
        tokenN = lines[linei].numTokens; // number of tokens in line
        if (tokenN == 0) continue;
        replaceKnownNames();                   // replace previously defined names by symbol references
        // check if line begins with '%'
        if (tokens[tokenB].type == TOK_OPR && tokens[tokenB].id == '%') {
            // metaprogramming code
            lines[linei].type = LINE_METADEF;
            interpretMetaDefinition();
            continue;
        }
        // classify other lines
        lines[linei].sectionType = sectionFlags;                    // line is section directive
        if (sectionFlags & ATT_EXEC) lines[linei].type = LINE_CODEDEF;
        else if (sectionFlags & ((ATT_READ | ATT_WRITE))) lines[linei].type = LINE_DATADEF;

        if (tokenN > 1) {
            // search for section, function and symbol definitions
            // lines with a single token cannot legally define a symbol name
            if ((tokens[tokenB].type == TOK_NAM || tokens[tokenB].type == TOK_SYM) && tokens[tokenB+1].type == TOK_DIR) {
                switch (tokens[tokenB + 1].id) {
                case DIR_SECTION:   // section starts here
                    interpretSectionDirective();
                    break;
                case DIR_FUNCTION:   // function starts here
                    interpretFunctionDirective();
                    break;
                case DIR_END:    // section or function end
                    interpretEndDirective(); 
                    break;
                default:
                    errors.report(tokens[tokenB + 1]);
                }
            }
            else if (tokens[tokenB].id == DIR_EXTERN) {
                // extern symbols
                interpretExternDirective();
            }
            else if (tokens[tokenB].id == DIR_PUBLIC) {
                // the interpretation of public symbol declarations is postponed to pass 4 after all 
                // symbols have been defined and got their final value
                lines[linei].type = LINE_PUBLICDEF;
            }
            else if (tokens[tokenB].type == TOK_NAM && tokens[tokenB+1].id == ':') {
                interpretLabel(tokenB);
                if (lines[linei].type == LINE_DATADEF) interpretVariableDefinition1();
            }
            else if (tokens[tokenB].type == TOK_TYP && (tokens[tokenB+1].type == TOK_NAM || tokens[tokenB+1].type == TOK_SYM)) {
                interpretVariableDefinition2();
            }
            else if (tokens[tokenB].type == TOK_ATT && tokens[tokenB].id == ATT_ALIGN) {  
                interpretAlign();
            }
            else if (tokens[tokenB].type == TOK_SYM && tokens[tokenB+1].id == ':' && pass == 2) {
                errors.report(tokens[tokenB].pos, tokens[tokenB].stringLength, ERR_SYMBOL_DEFINED);  // symbol already defined
            }
            else {
                determineLineType();  // check if code or data
                if (lines[linei].type == LINE_DATADEF) interpretVariableDefinition1();
            }
        }
        else {
            determineLineType();  // check if code or data (can only be code)
        }
    }

    // loop through lines again to replace names that are forward references to symbols defined during pass 2
    for (linei = 1; linei < lines.numEntries(); linei++) {
        tokenB = lines[linei].firstToken;      // first token in line        
        tokenN = lines[linei].numTokens;      // number of tokens in line
        replaceKnownNames();                   // replace previously defined names by symbol references
    }
}


// Show all symbols. For debugging only
void CAssembler::showSymbols() {
    uint32_t symi;
    ElfFWC_Sym2 sym;
    printf("\n\nSymbol:    name, section, addr, type, size, binding");
    for (symi = 1; symi < symbols.numEntries(); symi++) {
        sym = symbols[symi];
        printf("\n%3i: %10s, %7i, %4X", symi, symbolNameBuffer.buf() + sym.st_name,
            sym.st_section, (uint32_t)sym.st_value);
        if (sym.st_type == STT_CONSTANT || sym.st_type == STT_VARIABLE) {
            if (sym.st_other & STV_FLOAT) {    // floating point constant
                union { uint64_t i; double d; } val;
                val.i = sym.st_value;
                printf(" = %G", val.d);
            }
            else if (sym.st_other & STV_STRING) {   // string
                printf(" = %s", stringBuffer.getString((uint32_t)sym.st_value));
            }
            else {
                // print 64 bit integer constant
                printf(" = 0x");
                if (uint64_t(sym.st_value) >> 32) {
                    printf("%X%08X", uint32_t(sym.st_value >> 32), uint32_t(sym.st_value));
                }
                else {
                    printf("%X", uint32_t(sym.st_value));
                }
                // this method causes warnings:
                // printf(((sizeof(long int) > 4) ? " = 0x%lx" : " = 0x%llx"), sym.st_value); 
            }
        }
        else {
            printf(" %5X, %X*%X, %7X",  // other type
                sym.st_type, sym.st_unitsize, sym.st_unitnum, sym.st_bind);
        }
    }
}

// Show all tokens. For debugging only
void CAssembler::showTokens() {
    SKeyword const tokenNames[] = {        
        {"name", TOK_NAM},  // unidentified name        
        {"direc", TOK_DIR},   // section or function directive
        {"attrib", TOK_ATT},   // section or function attribute
        {"label", TOK_LAB},   // code label or function name
        {"datalb", TOK_VAR},    // data label
        {"secnm", TOK_SEC},   // section name
        {"type", TOK_TYP},   // type name
        {"reg", TOK_REG},   //  register name
        {"instr", TOK_INS},   //instruction name 
        {"oper", TOK_OPR},   // operator
        {"option", TOK_OPT},   // operator
        {"num", TOK_NUM},   // number
        {"float", TOK_FLT},   // floating point number
        {"char", TOK_CHA},   // character or string in single quotes ' '
        {"string", TOK_STR},   // string in double quotes " "
        {"symbol", TOK_SYM},   // symbol
        {"expression", TOK_XPR},   // expression
        {"eof", TOK_EOF},   // string in double quotes " "
        {"hll", TOK_HLL}   // string in double quotes " "
                           //   {"error", TOK_ERR}   // error. illegal character or unmatched quote
    };

    uint32_t line, tok, i;
    for (line = 1; line < lines.numEntries(); line++) {
        if (line < lines.numEntries() && lines[line].numTokens) {
            printf("\nline %2i type %X", lines[line].linenum, lines[line].type);

            for (tok = lines[line].firstToken; tok < lines[line].firstToken + lines[line].numTokens; tok++) {
                // find name for token type
                const char * nm = 0;
                for (i = 0; i < TableSize(tokenNames); i++) {
                    if (tokenNames[i].id == tokens[tok].type) nm = tokenNames[i].name;
                }
                if (nm) printf("\n%8s: ", nm);                               // Token type
                else printf("type %4X", tokens[tok].type);

                switch (tokens[tok].type) {
                case TOK_DIR: case TOK_ATT: case TOK_TYP: case TOK_OPT: case TOK_HLL:
                    nm = 0;
                    for (i = 0; i < TableSize(keywordsList); i++) {
                        if (keywordsList[i].id == tokens[tok].id) nm = keywordsList[i].name;
                    }
                    if (nm) printf("%s", nm);
                    else printf("%4X %2i", tokens[tok].pos, tokens[tok].stringLength);
                    break;
                case TOK_OPR:
                    nm = 0;
                    for (i = 0; i < TableSize(operatorsList); i++) {
                        if (operatorsList[i].id == tokens[tok].id) nm = operatorsList[i].name;
                    }
                    if (nm) printf("%s", nm);
                    else printf("%4X %2i", tokens[tok].pos, tokens[tok].stringLength);
                    break;
                case TOK_REG: //registerNames
                    nm = 0;
                    for (i = 0; i < TableSize(registerNames); i++) {
                        if (registerNames[i].id == (tokens[tok].id & 0xFFFFFF00)) nm = registerNames[i].name;
                    }
                    if (nm) printf("%s%i", nm, tokens[tok].id & 0xFF);
                    else printf("%4X %2i", tokens[tok].pos, tokens[tok].stringLength);
                    break;
                case TOK_NAM: case TOK_NUM: case TOK_FLT: case TOK_LAB: case TOK_VAR: case TOK_SEC:
                case TOK_CHA: case TOK_STR: case TOK_INS: case TOK_SYM:
                    for (i = 0; i < tokens[tok].stringLength; i++) {
                        printf("%c", buf()[tokens[tok].pos + i]);
                    }
                    printf("  id %X, value %X", tokens[tok].id, tokens[tok].value.w);
                    break;
                case TOK_XPR:
                default:
                    printf("0x%X 0x%X 0x%X %2i", tokens[tok].id, tokens[tok].value.w, tokens[tok].pos, tokens[tok].stringLength);
                    break;
                }
            }
        }
    }
} 

void CAssembler::initializeWordLists() {
    // Operators list
    operators.pushBig(operatorsList, sizeof(operatorsList));
    operators.sort();    
    // Keywords list
    keywords.pushBig(keywordsList,sizeof(keywordsList));
    keywords.sort();
    // Read instruction list from file
    CCSVFile instructionListFile; 
    instructionListFile.read(cmd.getFilename(cmd.instructionListFile), CMDL_FILE_SEARCH_PATH); // Filename of list of instructions
    instructionListFile.parse();                            // Read and interpret instruction list file
    instructionlist << instructionListFile.instructionlist; // Transfer instruction list to my own container
    instructionlistId.copy(instructionlist);                // copy instruction list
    // sort lists by different criteria, defined by the different operators:
    // operator < (SInstruction const & a, SInstruction const & b)
    // operator < (SInstruction3 const & a, SInstruction3 const & b)
    SInstruction3 nullInstruction;                          // empty record
    zeroAllMembers(nullInstruction);
    instructionlistId.push(nullInstruction);                // Empty record will go to position 0 to avoid an instruction with index 0
    instructionlist.sort();                                 // Sort instructionlist by name
    instructionlistId.sort();                               // Sort instructionlistId by id
}

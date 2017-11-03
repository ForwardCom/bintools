/****************************   error.cpp   **********************************
* Author:        Agner Fog
* Date created:  2006-07-15
* Last modified: 2017-11-03
* Version:       1.00
* Project:       Binary tools for ForwardCom instruction set
* Module:        error.cpp
* Description:
* Standard procedure for error reporting to stderr
*
* Copyright 2006-2017 GNU General Public License http://www.gnu.org/licenses
*****************************************************************************/

#include "stdafx.h"

const int MAX_ERROR_TEXT_LENGTH = 1024; // Maximum length of error text including extra info


// Make and initialize error reporter object
CErrorReporter err;

// General error messages

// todo: remove unused error messages!

SErrorText errorTexts[] = {
   // Unknown error
   {0,    2, "Unknown error!"},

   // Warning messages
   {ERR_EMPTY_OPTION, 1, "Empty command line option"},
   {ERR_UNKNOWN_OPTION, 1, "Unknown command line option: %s"},
   {ERR_UNKNOWN_ERROR_NUM, 1, "Unknown warning/error number: %i"},
   {1040, 1, "Name of section %s too long. Truncating to 16 characters"}, //?
   {1052, 1, "Indirect symbol index out of range"}, //?
   {1055, 1, "Communal section currently not supported by forw. Section dropped"}, //?
   {1060, 1, "Different alignments specified for same segment, %s. Using highest alignment"}, //?
   {1062, 1, "Symbol %s has unknown type"}, //?
   {1101, 1, "Output file name should have extension .li"}, //?
   {1102, 1, "Library members have different type"}, //?
   {ERR_OUTFILE_IGNORED, 1, "Output file name ignored"},
   {1104, 1, "Library member %s not found. Extraction failed"}, //?
   {1105, 1, "Library member %s not found. Deletion failed"}, //?
   {1107, 1, "Name of library member %s should have extension .o or .obj"}, //?
   {1108, 1, "Name of library member %s too long. Truncating to 15 characters"}, //?
   {1109, 1, "Library member %s has unknown type. Possibly alias record without code"}, //?

   {1207, 1, "Overlapping data"}, //?
   {1211, 1, "%i comment records ignored"}, //?
   {1212, 1, "Record type (%s) not supported"}, //?
   {1214, 1, "Symbol %s defined in both modules %s"}, //?
   {1303, 1, "Cannot find imported symbol"}, //?
   {1304, 1, "Unknown relocation address"}, //?

   // Error messages
   {ERR_MULTIPLE_IO_FILES, 2, "No more than one input file and one output file can be specified"}, //?
   {ERR_MULTIPLE_COMMANDS, 2, "More than one command specified on command line: %s"},
   {2003, 2, "Only one output format option can be specified. Command line error at %s"}, //?
   {ERR_UNKNOWN_OPTION, 2, "Unknown command line option: %s"},
   {ERR_FILES_SAME_NAME, 2, "Input file and output file cannot have same name: %s"},
   {2006, 2, "Unsupported file type for file %s: %s"}, //?
   {ERR_DUMP_NOT_SUPPORTED, 2, "Sorry. Dump of file type %s is not supported"},
   {ERR_INDEX_OUT_OF_RANGE, 2, "Index out of range"},
   {2017, 2, "File name %s specified more than once"}, //?
   {2018, 2, "Unknown type 0x%X for file: %s"}, //?
   {2020, 2, "Overflow when converting value of symbol %s to 32 bits"}, //?

   {2030, 2, "Unsupported relocation type (%i)"}, //?
   {2031, 2, "Relocated symbol not found"}, //?
   {2032, 2, "Relocation points outside segment"}, //?
   {ERR_ELF_RECORD_SIZE, 2, "Error in ELF file. Record size wrong"},
   {ERR_ELF_SYMTAB_MISSING, 2, "Symbol table not found in ELF file"},
   {ERR_ELF_INDEX_RANGE, 2, "Index out of range in object file"},
   {ERR_ELF_UNKNOWN_SECTION, 2, "Unknown section index in ELF file: %i"},
   {2037, 2, "Symbol storage/binding type %i not supported"}, //?
   {2038, 2, "Symbol type %i not supported"}, //?
   {2040, 2, "Symbol table corrupt in object file"}, //?
   {2041, 2, "File has relocation of uninitialized data"}, //?
   {ERR_CONTAINER_INDEX, 2, "Index out of range in internal container"},
   {ERR_CONTAINER_OVERFLOW, 2, "Overflow of internal container"},

   {ERR_INPUT_FILE, 2, "Cannot read input file %s"},
   {ERR_OUTPUT_FILE, 2, "Cannot write output file %s"},
   {ERR_UNKNOWN_FILE_TYPE, 2, "Unknown file type %i: %s"},
   {ERR_FILE_SIZE, 2, "Wrong size of file %s"},

   {ERR_TOO_MANY_RESP_FILES, 2, "Too many response files"},
   {ERR_ELF_STRING_TABLE, 2, "String table corrupt"},
   {ERR_FILE_NAME_LONG, 2, "File name %s too long"},
   {2210, 2, "File contains overlapping relocation sources"}, //?
   {2302, 2, "Relocation source extends beyond end of section"}, //?
   {2306, 2, "Segment size is 4 Gbytes"}, //?
   {2308, 2, "Unknown alignment %i"}, //?
   {2309, 2, "Data outside bounds of segment %s"}, //?
   {2321, 2, "Wrong record size found"}, //?

   {ERR_INSTRUCTION_LIST_SYNTAX, 2, "Syntax error in instruction list: %s"},
   {ERR_INSTRUCTION_LIST_QUOTE, 2, "Unmatched quote in instruction list, line %i"},  //?

   {2500, 2, "Library/archive file is corrupt"}, //?
   {2501, 2, "Cannot store file of type %s in library"}, //?
   {2502, 2, "Too many members in library"}, //?
   {2503, 2, "Output file name must be specified"}, //?
   {2504, 2, "Object file type (%s) does not match library"}, //?
   {2506, 2, "Overflow of buffer for library member names"}, //?
   {2600, 2, "Library has more than one header"}, //?
   {2601, 2, "Library page size (%i) is not a power of 2"}, //?
   {2606, 2, "Too many library members. Creation of library failed"}, //?
   {2620, 2, "You need to extract library members before disassembling"},
   {2621, 2, "Wrong output file type"}, //?

   // Fatal errors makes the program stop immediately:
   {ERR_INTERNAL, 9, "Objconv program internal inconsistency"}, //?
   {ERR_TOO_MANY_ERRORS, 9, "Too many errors. Aborting"},
   {ERR_BIG_ENDIAN, 9, "This program cannot be compiled on a machine with big-endian memory organization"}, 
   {ERR_MEMORY_ALLOCATION, 9, "Memory allocation failed"}, //?

   // Mark end of list
   {9999, 9999, "End of error text list"}  //?
};


// Error messages for assembly file
SErrorText assemErrorTexts[] = {
    // the status number indicates if an extra string is required
    {0,          0, "Unknown error number!"},
    {TOK_NAM,    1, "unknown name: "}, 
    {TOK_LAB,    1, "misplaced label: "}, 
    {TOK_VAR,    1, "misplaced variable: "}, 
    {TOK_SEC,    1, "misplaced section name: "}, 
    {TOK_INS,    1, "misplaced instruction: "}, 
    {TOK_OPR,    1, "misplaced operator: "}, 
    {TOK_NUM,    1, "misplaced number: "}, 
    {TOK_FLT,    1, "misplaced floating point number: "}, 
    {TOK_CHA,    1, "misplaced character constant: "}, 
    {TOK_STR,    1, "misplaced string: "}, 
    {TOK_DIR,    1, "misplaced directive: "}, 
    {TOK_ATT,    1, "misplaced attribute: "}, 
    {TOK_TYP,    1, "misplaced type name: "}, 
    {TOK_OPT,    1, "misplaced option: "}, 
    {TOK_REG,    1, "misplaced register: "}, 
    {TOK_SYM,    1, "misplaced symbol: "},     
    {TOK_XPR,    1, "misplaced expression: "},     
    {TOK_HLL,    1, "misplaced keyword: "}, 

    {ERR_CONTROL_CHAR,       1, "illegal control character: "}, 
    {ERR_ILLEGAL_CHAR,       1, "illegal character: "}, 
    {ERR_COMMENT_BEGIN,      0, "unmatched comment begin: /*"}, 
    {ERR_COMMENT_END,        0, "unmatched comment end: */"}, 
    {ERR_BRACKET_BEGIN,      1, "unmatched begin bracket: "}, 
    {ERR_BRACKET_END,        1, "unmatched end bracket: "},
    {ERR_QUOTE_BEGIN,        1, "unmatched begin quote: "},
    {ERR_QUESTION_MARK,      0, "unmatched '?'"},
    {ERR_COLON,              0, "unmatched ':'"},
    {ERR_SYMBOL_DEFINED,     1, "symbol already defined, cannot redefine: "},
    {ERR_SYMBOL_UNDEFINED,   1, "symbol not defined: "},    
    {ERR_MULTIDIMENSIONAL,   1, "multidimensional array not allowed: "},
    {ERR_UNFINISHED_VAR,     1, "unfinished variable declaration: "},
    {ERR_MISSING_EXPR,       1, "expecting expression: "},    
    {ERR_CONFLICT_ARRAYSZ,   1, "conflicting array size: "},
    {ERR_CONFLICT_TYPE,      1, "conflicting type of symbol: "},
    {ERR_CONDITION,          1, "expression cannot be used for condition: "},    
    {ERR_OVERFLOW,           1, "expression overflow: "},    
    {ERR_WRONG_TYPE,         1, "wrong operand type for operator: "},    
    {ERR_WRONG_TYPE_VAR,     1, "wrong or mismatched type for variable (must be int64, double, string, register, or memory operand): "},
    {ERR_WRONG_OPERANDS,     1, "wrong operands for this instruction: "},
    {ERR_MISSING_DESTINATION,1, "this instruction needs a destination: "},
    {ERR_NO_DESTINATION,     1, "this instruction should not have a destination: "},
    {ERR_NOT_OP_AMBIGUOUS,   0, "'!' operator is ambiguous. For booleans and masks replace !A by A^1. For numeric operands replace !A by A==0"},
    {ERR_TOO_COMPLEX,        1, "expression does not fit into a single instruction: "},
    {ERR_MASK_NOT_REGISTER,  1, "mask must be a register: "},
    {ERR_FALLBACK_WRONG,     1, "fallback must be a register 0-30 or zero: "},
    {ERR_CONSTANT_TOO_LARGE, 1, "constant too large for specified type: "},
    {ERR_ALIGNMENT,          1, "alignment must be a power of 2, not higher than 4096: "},  // maximum alignment value must equal MAX_ALIGN in assem.h
    {ERR_SECTION_DIFFERENT_TYPE,1, "redefinition of section is different type: "},
    {ERR_EXPECT_COLON,       1, "expecting colon after label: "},
    {ERR_STRING_TYPE,        1, "string must have type int8: "},
    {ERR_NONZERO_IN_BSS,     1, "data in uninitialized section must be zero: "},
    {ERR_SYMBOL_REDEFINED,   1, "symbol has been assigned more than one value: "},
    {ERR_EXPORT_EXPRESSION,  1, "cannot export expression: "},
    {ERR_CANNOT_EXPORT,      1, "cannot export: "},
    {ERR_CODE_WO_SECTION,    1, "code without section: "},
    {ERR_DATA_WO_SECTION,    1, "data without section: "},

    
    {ERR_MEM_COMPONENT_TWICE,1, "component of memory operand specified twice: "},
    {ERR_SCALE_FACTOR,       1, "wrong scale factor for this instruction: "},
    {ERR_MUST_BE_GP,         1, "vector length must be general purpose register: "},
    {ERR_LIMIT_AND_OFFSET,   1, "memory operand cannot have both limit and offset: "},
    {ERR_NOT_INSIDE_MEM,     1, "this option is not allowed inside memory operand: "},
    {ERR_TOO_MANY_OPERANDS,  1, "too many operands: "},
    {ERR_TOO_FEW_OPERANDS,   1, "not enough operands: "},    
    {ERR_OPERANDS_WRONG_ORDER,1, "operands in wrong order. register operands must come first: "},
    {ERR_BOTH_MEM_AND_IMMEDIATE, 1, "this instruction cannot have both a memory operand and immediate constant: "},  // except store in format 2.7B and VARIANT_M1
    {ERR_BOTH_MEM_AND_OPTIONS, 1, "this instruction cannot have both a memory operand and options: "},
    {ERR_UNFINISHED_INSTRUCTION,  1, "unfinished instruction: "},
    {ERR_TYPE_MISSING,       1, "type must be specified: "},
    {ERR_MASK_FALLBACK_TYPE, 0, "mask and fallback must have same register type as destination"},
    {ERR_NEG_INDEX_LENGTH,   0, "length register must be the same as negative index register"},
    {ERR_INDEX_AND_LENGTH,   0, "memory operand cannot have length or broadcast with positive index"},    
    {ERR_MASK_REGISTER,      0, "mask must be register 0-6"},
    {ERR_LIMIT_TOO_HIGH,     1, "limit on memory index cannot exceed 0xFFFF: "},    
    {ERR_NO_INSTRUCTION_FIT, 1, "no version of this instruction fits the specified operands: "},
    {ERR_CANNOT_SWAP_VECT,   0, "cannot change the order of vector registers. if the vectors have the same length then put the register operands before the constant or memory operand"},
    {ERR_EXPECT_JUMP_TARGET, 1, "expecting jump target: "},
    {ERR_JUMP_TARGET_MISALIGN, 1, "jump target offset must be divisible by 4: "},
    {ERR_ABS_RELOCATION,     1, "absolute address not possible here: "},    
    {ERR_RELOCATION_DOMAIN,  1, "cannot calculate difference between two symbols in different domains: "},    

    
    {ERR_WRONG_REG_TYPE,     1, "wrong type for register operand: "},
    {ERR_CONFLICT_OPTIONS,   1, "conflicting options: "},
    {ERR_VECTOR_OPTION,      1, "vector option applied to non-vector operands: "},
    {ERR_LENGTH_OPTION_MISS, 1, "vector memory operand must have scalar, length, or broadcast option: "},
    {ERR_DEST_BROADCAST,     0, "memory destination cannot have broadcast"},
    {ERR_OFFSET_TOO_LARGE,   1, "address offset too large: "},
    {ERR_LIMIT_TOO_LARGE,    1, "limit too large: "},
    {ERR_IMMEDIATE_TOO_LARGE,1, "instruction format does not have space for full-size constant and option/signbits: "},
    {ERR_TOO_LARGE_FOR_JUMP, 1, "conditional jump does not have space for 64-bit constant: "}, 
    {ERR_CANNOT_HAVE_OPTION, 1, "this instruction cannot have options: "}, 
    {ERR_CANNOT_HAVEFALLBACK, 1, "the fallback must be the same as the destination when there is a memory operand with index or vector: "},
    {ERR_3OP_AND_FALLBACK,   1, "the fallback must be the same as the destination on instructions with three operands: "},
    {ERR_3OP_AND_MEM,        1, "the first source register must be the same as the destination when there is a memory operand with index or vector: "}, 
    {ERR_R28_30_BASE,        1, "cannot use r28-r30 as base pointer with more than 8 bits offset: "}, 
    {ERR_NO_BASE,            1, "memory operand has no base pointer: "}, 
    {ERR_MEM_WO_BRACKET,     1, "memory operand requires [] bracket: "}, 

    {ERR_UNMATCHED_END,      0, "unmatched end"}, 
    {ERR_SECTION_MISS_END,   1, "missing end of section: "}, 
    {ERR_FUNCTION_MISS_END,  1, "missing end of function: "}, 
    {ERR_ELSE_WO_IF,         1, "else without if: "},
    {ERR_EXPECT_PARENTHESIS, 1, "expecting parenthesis: "},
    {ERR_EXPECT_BRACKET,     1, "expecting '{' bracket: "},
    {ERR_EXPECT_LOGICAL,     1, "expecting logical expression: "},
    {ERR_MEM_NOT_ALLOWED,    1, "cannot have memory operand: "},
    {ERR_MUST_BE_POW2,       1, "constant must have only one bit set: "},
    {ERR_WHILE_EXPECTED,     1, "'do' statement requires a 'while' here: "},
    {ERR_MISPLACED_BREAK,    1, "nothing: to break out of: "},
    {ERR_MISPLACED_CONTINUE, 1, "no loop to continue: "},

        

    
};

// Members of class CErrorReporter: reporting of general errors

// Constructor for CErrorReporter
CErrorReporter::CErrorReporter() {
   numErrors = numWarnings = worstError = 0;
   maxWarnings = 50;      // Max number of warning messages to pring
   maxErrors   = 50;      // Max number of error messages to print
}

SErrorText * CErrorReporter::FindError(int ErrorNumber) {
   // Search for error in ErrorTexts
   int e;
   const int ErrorTextsLength = sizeof(errorTexts) / sizeof(errorTexts[0]);
   for (e = 0; e < ErrorTextsLength; e++) {
      if (errorTexts[e].errorNumber == ErrorNumber) return errorTexts + e;
   }
   // Error number not found
   static SErrorText UnknownErr = errorTexts[0];
   UnknownErr.errorNumber = ErrorNumber;
   UnknownErr.status      = 0x102;  // Unknown error
   return &UnknownErr;
}


void CErrorReporter::submit(int ErrorNumber) {
   // Print error message with no extra info
   SErrorText * err = FindError(ErrorNumber);
   handleError(err, err->text);
}

void CErrorReporter::submit(int ErrorNumber, int extra) { 
   // Print error message with extra numeric info
   // ErrorTexts[ErrorNumber] must contain %i where extra is to be inserted
   char text[MAX_ERROR_TEXT_LENGTH];
   SErrorText * err = FindError(ErrorNumber);
   sprintf(text, err->text, extra);
   handleError(err, text);
}

void CErrorReporter::submit(int ErrorNumber, int extra1, int extra2) { 
   // Print error message with 2 extra numeric values inserted
   // ErrorTexts[ErrorNumber] must contain two %i fields where extra numbers are to be inserted
   char text[MAX_ERROR_TEXT_LENGTH];
   SErrorText * err = FindError(ErrorNumber);
   sprintf(text, err->text, extra1, extra2);
   handleError(err, text);
}

void CErrorReporter::submit(int ErrorNumber, char const * extra) {
   // Print error message with extra text info
   // ErrorTexts[ErrorNumber] must contain %s where extra is to be inserted
   char text[MAX_ERROR_TEXT_LENGTH];
   if (extra == 0) extra = "???";
   SErrorText * err = FindError(ErrorNumber);
   snprintf(text, MAX_ERROR_TEXT_LENGTH-1, err->text, extra);
   handleError(err, text);
}

void CErrorReporter::submit(int ErrorNumber, char const * extra1, char const * extra2) {
   // Print error message with two extra text info fields
   // ErrorTexts[ErrorNumber] must contain %s where extra texts are to be inserted
   char text[MAX_ERROR_TEXT_LENGTH];
   if (extra1 == 0) extra1 = "???"; if (extra2 == 0) extra2 = "???";
   SErrorText * err = FindError(ErrorNumber);
   sprintf(text, err->text, extra1, extra2);
   handleError(err, text);
}

void CErrorReporter::submit(int ErrorNumber, int extra1, char const * extra2) {
   // Print error message with two extra text fields inserted
   // ErrorTexts[ErrorNumber] must contain %i and %s where extra texts are to be inserted
   char text[MAX_ERROR_TEXT_LENGTH];
   if (extra2 == 0) extra2 = "???";
   SErrorText * err = FindError(ErrorNumber);
   sprintf(text, err->text, extra1, extra2);
   handleError(err, text);
}

// Write an error message.
// To trace an error message: set a breakpoint here
void CErrorReporter::handleError(SErrorText * err, char const * text) {
   // HandleError is used by submit functions
   // check severity
   int severity = err->status & 0x0F;
   if (severity == 0) {
      return;  // Ignore message
   }
   if (severity > 1 && err->errorNumber > worstError) {
      // Store highest error number
      worstError = err->errorNumber;
   }
   if (severity == 1) {
      // Treat message as warning
      if (++numWarnings > maxWarnings) return; // Maximum number of warnings has been printed
      // Treat message as warning
      fprintf(stderr, "\nWarning %i: %s", err->errorNumber, text);
      if (numWarnings == maxWarnings) {
         // Maximum number reached
         fprintf(stderr, "\nSupressing further warning messages");
      }
   }
   else {
      // Treat message as error
      if (++numErrors > maxErrors) return; // Maximum number of warnings has been printed
      fprintf(stderr, "\nError %i: %s", err->errorNumber, text);
      if (numErrors == maxErrors) {
         // Maximum number reached
         fprintf(stderr, "\nSupressing further warning messages");
      }
   }
   if (severity == 9) {
      // Abortion required
      fprintf(stderr, "\nAborting\n");
      exit(err->errorNumber);
   }
}

int CErrorReporter::number() {
   // Get number of fatal errors
   return numErrors;
}

int CErrorReporter::getWorstError() {
   // Get highest warning or error number encountered
   return worstError;
}

void CErrorReporter::clearError(int ErrorNumber) {
   // Ignore further occurrences of this error
   int e;
   const int ErrorTextsLength = sizeof(errorTexts) / sizeof(errorTexts[0]);
   for (e = 0; e < ErrorTextsLength; e++) {
      if (errorTexts[e].errorNumber == ErrorNumber) break;
   }
   if (e < ErrorTextsLength) {
      errorTexts[e].status = 0;
   }
}

// Members of class CAssemErrors: reporting of errors in assembly file
CAssemErrors::CAssemErrors() {                   // Constructor
    maxErrors = cmd.maxErrors;
}

void CAssemErrors::setOwner(CAssembler * a) {
    // Give access to CAssembler
    owner = a;
}

uint32_t CAssemErrors::numErrors() {
    // Return number of errors
    return list.numEntries();
}

bool CAssemErrors::tooMany() {
    // true if too many errors
    return list.numEntries() >= maxErrors;
}

// Report an error in assembly file
// To trace an assembly error: set a breakpoint here
void CAssemErrors::report(uint32_t position, uint32_t stringLength, uint32_t num) {
    // position: position in input file
    // stringLength: length of token
    // num = index into assemErrorTexts or token type
    owner->lineError = true;                         // avoid reporting multiple errors on same line
    uint32_t linei = owner->linei;
    if (linei < owner->lines.numEntries()) {    
        owner->lines[owner->linei].type = LINE_ERROR;    // mark current line as error
    }
    if (tooMany()) return;

    SAssemError e;
    e.pos = position;
    e.stringLength = stringLength;
    e.file = owner->filei;
    e.num = num;
    e.pass = owner->pass;

    // save error record
    list.push(e);
}

// Report a misplaced token
void CAssemErrors::report(SToken const & token) {
    report(token.pos, token.stringLength, token.type);
}

// Report an error in current line
void CAssemErrors::reportLine(uint32_t num) {
    int tokenB = owner->lines[owner->linei].firstToken;
    int tokenN = owner->lines[owner->linei].numTokens;
    report(owner->tokens[tokenB].pos, 
        owner->tokens[tokenB + tokenN - 1].pos + owner->tokens[tokenB + tokenN - 1].stringLength - owner->tokens[tokenB].pos, 
        num);
}

void CAssemErrors::outputErrors() {
    // Output errors to STDERR
    const uint32_t tabstops = 8;                      // default position of tabstops
    
    if (list.numEntries() == 0) return;
    const char * text1;
    char text2[256];
    const char * filename = owner->fileName; // to do: support include filenames
    const uint32_t errorTextsLength = TableSize(assemErrorTexts);
    uint32_t i, j, texti; 

    uint32_t lastPass = 0;
    for (i = 0; i < list.numEntries() && i < maxErrors; i++) {
        // tell which pass if verbose option
        if (list[i].pass != lastPass && cmd.verbose) {
            printf("\n\nDuring pass %i:", list[i].pass);
            lastPass = list[i].pass;
        }

        // find line containing error
        uint32_t line;
        uint32_t numLines = owner->lines.numEntries();
        uint32_t pos = list[i].pos;
        for (line = 0; line < numLines; line++) {
            if (pos < owner->lines[line].beginPos) break;
        }
        line--;
        // if this line has multiple records in lines[] then find the first one
        j = line;
        while (j > 0 && owner->lines[j-1].linenum == owner->lines[line].linenum) j--;
        line = j;

        // find column
        uint32_t pos1 = owner->lines[line].beginPos;
        uint32_t stringLength = list[i].stringLength;
        uint32_t column = pos - pos1;
        // count UTF-8 multibyte characters in line up to error position
        int32_t extraBytes = 0;
        int8_t c;   // current character
        for (uint32_t pp = pos1; pp < pos1 + column; pp++) {
            c = *(owner->buf() + pp);
            if ((c & 0xC0) == 0xC0) extraBytes--;   // count UTF-8 continuation bytes
            if (c == '\t') {
                uint32_t pos2 = (pp + tabstops) % tabstops;  // find next tabstop
                extraBytes += pos2 - pp - 1;
            }  
        }
        // adjust column number to 1-based. count UTF-8 characters as one
        column += extraBytes + 1;
 
        // find text
        texti = list[i].num;
        for (j = 0; j < errorTextsLength; j++) {
            if ((uint32_t)assemErrorTexts[j].errorNumber == texti) break;
        }
        if (j >= errorTextsLength) j = 0;
        text1 = assemErrorTexts[j].text;
        if (assemErrorTexts[j].status && stringLength < sizeof(text2)) {
            // extra text required
            memcpy(text2, owner->buf() + pos, stringLength);
            text2[stringLength] = 0;
        }
        else text2[0] = 0;

        if (filename) {        
            fprintf(stderr, "\n%s:", filename);
        }
        else {
            fprintf(stderr, "\n");
        }
        fprintf(stderr, "%i:%i: %s%s", owner->lines[line].linenum, column, text1, text2);
    }
}

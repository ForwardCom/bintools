/****************************   error.h   ************************************
* Author:        Agner Fog
* Date created:  2017-04-17
* Last modified: 2021-03-30
* Version:       1.13
* Project:       Binary tools for ForwardCom instruction set
* Module:        error.h
* Description:
* Header file for error handler error.cpp
*
* Copyright 2006-2024 GNU General Public License https://www.gnu.org/licenses
*****************************************************************************/
#pragma once

// Error id numbers, general errors
const int ERR_MULTIPLE_COMMANDS        = 100;
const int ERR_OUTFILE_IGNORED          = 101;
const int ERR_EMPTY_OPTION             = 102;
const int ERR_UNKNOWN_OPTION           = 103;
const int ERR_UNKNOWN_ERROR_NUM        = 104;
const int ERR_MULTIPLE_IO_FILES        = 105;
const int ERR_DUMP_NOT_SUPPORTED       = 106;
const int ERR_INPUT_FILE               = 107;
const int ERR_OUTPUT_FILE              = 108;
const int ERR_UNKNOWN_FILE_TYPE        = 109;
const int ERR_FILE_SIZE                = 110;
const int ERR_FILE_NAME_LONG           = 111;
const int ERR_FILES_SAME_NAME          = 112;
const int ERR_TOO_MANY_RESP_FILES      = 112;

const int ERR_MEMORY_ALLOCATION        = 120;
const int ERR_CONTAINER_INDEX          = 121;
const int ERR_CONTAINER_OVERFLOW       = 122;
const int ERR_INDEX_OUT_OF_RANGE       = 123;

const int ERR_ELF_RECORD_SIZE          = 130;
const int ERR_ELF_SYMTAB_MISSING       = 131;
const int ERR_ELF_INDEX_RANGE          = 132;
const int ERR_ELF_UNKNOWN_SECTION      = 133;
const int ERR_ELF_STRING_TABLE         = 134;
const int ERR_ELF_NO_SECTIONS          = 135;

const int ERR_INSTRUCTION_LIST_SYNTAX  = 140;
const int ERR_INSTRUCTION_LIST_QUOTE   = 141;

const int ERR_LIBRARY_FILE_TYPE        = 200;
const int ERR_LIBRARY_FILE_CORRUPT     = 201;
const int ERR_DUPLICATE_NAME_COMMANDL  = 202;
const int ERR_DUPLICATE_NAME_IN_LIB    = 203;
const int ERR_DUPLICATE_SYMBOL_IN_LIB  = 204;
const int ERR_NO_SYMTAB_IN_LIB         = 205;
const int ERR_MEMBER_NOT_FOUND_DEL     = 206;
const int ERR_MEMBER_NOT_FOUND_EXTRACT = 207;
const int ERR_LIBRARY_LIST_ONLY        = 208;
const int ERR_LIBRARY_MEMBER_TYPE      = 209;

const int ERR_LINK_LIST_ONLY           = 300;
const int ERR_LINK_FILE_TYPE           = 301;
const int ERR_LINK_FILE_TYPE_LIB       = 302;
const int ERR_LINK_FILE_TYPE_EXE       = 303;
const int ERR_LINK_COMMUNAL            = 304;
const int ERR_LINK_DUPLICATE_SYMBOL    = 305;
const int ERR_LINK_DIFFERENT_BASE      = 306;
const int ERR_LINK_MISALIGNED_TARGET   = 307;
const int ERR_LINK_OVERFLOW            = 308;
const int ERR_LINK_RELOCATION_OVERFLOW = 309;
const int ERR_LINK_REGUSE              = 310;
const int ERR_LINK_MODULE_NOT_FOUND    = 311;
const int ERR_EVENT_SIZE               = 312;
const int ERR_REL_SYMBOL_NOT_FOUND     = 313;
const int ERR_CANT_RELINK_MODULE       = 314;
const int ERR_CANT_RELINK_LIBRARY      = 315;
const int ERR_RELINK_MODULE_NOT_FOUND  = 316;
const int ERR_RELINK_LIBRARY_NOT_FOUND = 317;
const int ERR_RELINK_BASE_POINTER_MOD  = 318;
const int ERR_INPUT_NOT_RELINKABLE     = 319;
const int ERR_LINK_UNRESOLVED          = 320;
const int ERR_LINK_UNRESOLVED_WARN     = 321;

const int ERR_TOO_MANY_ERRORS          = 500;
const int ERR_BIG_ENDIAN               = 501;
const int ERR_INTERNAL                 = 502;


// Error id numbers during assembly
const int ERR_CONTROL_CHAR             = 0x100;  // illegal control character
const int ERR_ILLEGAL_CHAR             = 0x101;  // illegal character
const int ERR_COMMENT_BEGIN            = 0x102;  // unmatched comment begin
const int ERR_COMMENT_END              = 0x103;  // unmatched comment end
const int ERR_BRACKET_BEGIN            = 0x104;  // unmatched begin bracket
const int ERR_BRACKET_END              = 0x105;  // unmatched end bracket
const int ERR_QUOTE_BEGIN              = 0x106;  // unmatched quote begin
const int ERR_QUESTION_MARK            = 0x108;  // unmatched '?'
const int ERR_COLON                    = 0x109;  // unmatched ':'
const int ERR_SYMBOL_DEFINED           = 0x10A;  // symbol already defined
const int ERR_SYMBOL_UNDEFINED         = 0x10B;  // symbol not defined
const int ERR_UNFINISHED_VAR           = 0x10C;  // unfinished variable declaration
const int ERR_MISSING_EXPR             = 0x10D;  // unfinished variable declaration
const int ERR_MULTIDIMENSIONAL         = 0x110;  // multidimensional array not allowed
const int ERR_CONFLICT_ARRAYSZ         = 0x111;  // conflicting array size
const int ERR_CONFLICT_TYPE            = 0x112;  // conflicting array type
const int ERR_CONDITION                = 0x113;  // incorrect condition for ?: operator
const int ERR_OVERFLOW                 = 0x114;  // overflow in asseble-time calculation
const int ERR_WRONG_TYPE               = 0x115;  // wrong type for operator
const int ERR_WRONG_TYPE_VAR           = 0x116;  // wrong or mismatched type for variable
const int ERR_WRONG_OPERANDS           = 0x117;  // wrong operands for this instruction
const int ERR_MISSING_DESTINATION      = 0x118;  // destination required
const int ERR_NO_DESTINATION           = 0x119;  // should not have destination
const int ERR_NOT_OP_AMBIGUOUS         = 0x11A;  // '!' operator on register or memory is ambiguous
const int ERR_TOO_COMPLEX              = 0x11B;  // expression is too complex for a single instruction
const int ERR_MASK_NOT_REGISTER        = 0x11C;  // mask must be a register
const int ERR_FALLBACK_WRONG           = 0x11D;  // fallback must be a register or zero
const int ERR_CONSTANT_TOO_LARGE       = 0x11E;  // immediate constant too large for specified type
const int ERR_ALIGNMENT                = 0x11F;  // alignment too high or not a power of 2
const int ERR_SECTION_DIFFERENT_TYPE   = 0x120;  // redefinition of section is different type
const int ERR_EXPECT_COLON             = 0x121;  // expect colon after label
const int ERR_STRING_TYPE              = 0x122;  // string must have type int8
const int ERR_NONZERO_IN_BSS           = 0x123;  // nonzero value in uninitialized section
const int ERR_SYMBOL_REDEFINED         = 0x124;  // symbol has been assigned more than one value
const int ERR_EXPORT_EXPRESSION        = 0x125;  // cannot export expression
const int ERR_CANNOT_EXPORT            = 0x126;  // cannot export this type of symbol
const int ERR_CODE_WO_SECTION          = 0x127;  // code without section
const int ERR_DATA_WO_SECTION          = 0x128;  // data without section
const int ERR_MIX_DATA_AND_CODE        = 0x129;  // mixing data and code in same section
const int ERR_MUST_BE_CONSTANT         = 0x12A;  // option value must be constant
const int ERR_MEM_COMPONENT_TWICE      = 0x140;  // component of memory operand specified twice
const int ERR_SCALE_FACTOR             = 0x141;  // wrong scale factor
const int ERR_MUST_BE_GP               = 0x142;  // length or broadcast must be general purpose register
const int ERR_LIMIT_AND_OFFSET         = 0x143;  // cannot have both limit and offset
const int ERR_NOT_INSIDE_MEM           = 0x144;  // mask option not allowed inside memory operand
const int ERR_TOO_MANY_OPERANDS        = 0x145;  // too many operands
const int ERR_TOO_FEW_OPERANDS         = 0x146;  // too many operands
const int ERR_OPERANDS_WRONG_ORDER     = 0x147;  // operands in wrong order
const int ERR_BOTH_MEM_AND_IMMEDIATE   = 0x148;  // cannot have both memory operand and immediate constant
const int ERR_BOTH_MEM_AND_OPTIONS     = 0x149;  // cannot have both memory operand and options
const int ERR_UNFINISHED_INSTRUCTION   = 0x14A;  // unfinished instruction code
const int ERR_TYPE_MISSING             = 0x14B;  // type must be specified
const int ERR_MASK_FALLBACK_TYPE       = 0x14C;  // mask and fallback must have same type as destination
const int ERR_NEG_INDEX_LENGTH         = 0x14D;  // length must be the same as negative index
const int ERR_INDEX_AND_LENGTH         = 0x14E;  // cannot have length/broadcast and index
const int ERR_MASK_REGISTER            = 0x14F;  // mask register number > 6
const int ERR_LIMIT_TOO_HIGH           = 0x150;  // limit on memory operand too high
const int ERR_NO_INSTRUCTION_FIT       = 0x151;  // no version of this instruction fits the specified operands
const int ERR_CANNOT_SWAP_VECT         = 0x152;  // cannot change the order of vector registers
const int ERR_EXPECT_JUMP_TARGET       = 0x158;  // expecting jump target
const int ERR_JUMP_TARGET_MISALIGN     = 0x159;  // jump target offset must be divisible by 4
const int ERR_ABS_RELOCATION           = 0x15a;  // absolute address not allowed here
const int ERR_ABS_RELOCATION_WARN      = 0x15b;  // absolute address allowed, but not recommended. make warning
const int ERR_RELOCATION_DOMAIN        = 0x15c;  // cannot calculate difference between two symbols in different domains
const int ERR_WRONG_REG_TYPE           = 0x160;  // wrong type for register operand
const int ERR_CONFLICT_OPTIONS         = 0x161;  // conflicting options on memory operand
const int ERR_VECTOR_OPTION            = 0x162;  // vector option applied to non-vector operands
const int ERR_LENGTH_OPTION_MISS       = 0x163;  // vector memory operand must have scalar, length, or broadcast option
const int ERR_DEST_BROADCAST           = 0x164;  // memory destination operand cannot have broadcast
const int ERR_OFFSET_TOO_LARGE         = 0x165;  // address offset too large
const int ERR_LIMIT_TOO_LARGE          = 0x166;  // index limit too large
const int ERR_IMMEDIATE_TOO_LARGE      = 0x167;  // instruction format does not have space for full-size constant and option/signbits
const int ERR_TOO_LARGE_FOR_JUMP       = 0x168;  // jump instruction does not have space for full-size constant
const int ERR_CANNOT_HAVE_OPTION       = 0x169;  // this instruction cannot have options
const int ERR_CANNOT_HAVEFALLBACK1     = 0x16A;  // cannot have fallback register
const int ERR_CANNOT_HAVEFALLBACK2     = 0x16B;  // cannot have fallback != destination with this memory operand
const int ERR_3OP_AND_FALLBACK         = 0x16C;  // cannot have fallback != destination with three operands
const int ERR_3OP_AND_MEM              = 0x16D;  // the first source register must be the same as the destination when there is a memory operand with index or vector
const int ERR_R28_30_BASE              = 0x16E;  // cannot use r28-r30 as base pointer with more than 8 bits offset
const int ERR_NO_BASE                  = 0x16F;  // memory operand has no base pointer
const int ERR_MEM_WO_BRACKET           = 0x170;  // memory operand requires [] bracket
const int ERR_UNKNOWN                  = 0x171;  // unknown error during assembly
const int ERR_UNMATCHED_END            = 0x210;  // unmatched end
const int ERR_SECTION_MISS_END         = 0x211;  // missing end of section
const int ERR_FUNCTION_MISS_END        = 0x212;  // missing end of function
const int ERR_ELSE_WO_IF               = 0x222;  // else without if
const int ERR_EXPECT_PARENTHESIS       = 0x223;  // expecting parenthesis after if, for, while, switch
const int ERR_EXPECT_BRACKET           = 0x224;  // expecting bracket after if, for, while, switch()
const int ERR_EXPECT_LOGICAL           = 0x225;  // expecting logical expression in if() or while()
const int ERR_MEM_NOT_ALLOWED          = 0x226;  // cannot have memory operand
//const int ERR_MUST_BE_POW2             = 0x227;  // constant must have only one bit set
const int ERR_WHILE_EXPECTED           = 0x228;  // 'while' expected after 'do'
const int ERR_MISPLACED_BREAK          = 0x229;  // nothing: to break out of
const int ERR_MISPLACED_CONTINUE       = 0x22A;  // no loop to continue



// Structure for defining error message texts
struct SErrorText {
   int  errorNumber;                             // Error number
   int  status;                                  // bit 0-3 = severity: 0 = ignore, 1 = warning, 2 = error, 9 = abort
                                                 // bit 8   = error number not found
   char const * text;                            // Error text
};

// General error routine for reporting warning and error messages to STDERR output
class CErrorReporter {
public:
   CErrorReporter();    // Default constructor
   static SErrorText *FindError(int ErrorNumber);// Search for error in ErrorTexts
   void submit(int ErrorNumber);                 // Print error message
   void submit(int ErrorNumber, int extra);      // Print error message with extra info
   void submit(int ErrorNumber, int, int);       // Print error message with two extra numbers inserted
   void submit(int ErrorNumber, char const * extra); // Print error message with extra info
   void submit(int ErrorNumber, char const *, char const *); // Print error message with two extra text fields inserted
   void submit(int ErrorNumber, char const * extra1, char const * extra2, char const * extra3);// Print error message with three extra text fields
   void submit(int ErrorNumber, int, char const *); // Print error message with two extra text fields inserted
   int number();                                 // Get number of errors
   int getWorstError();                          // Get highest warning or error number encountered
   void clearError(int ErrorNumber);             // Ignore further occurrences of this error
protected:
   int numErrors;                                // Number of errors detected
   int numWarnings;                              // Number of warnings detected
   int worstError;                               // Highest error number encountered
   int maxWarnings;                              // Max number of warning messages to pring
   int maxErrors;                                // Max number of error messages to print
   void handleError(SErrorText * err, char const * text); // Used by submit function
};

extern CErrorReporter err;  // Error handling object is in error.cpp
extern SErrorText errorTexts[]; // List of error texts



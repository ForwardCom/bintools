/****************************  disassem.h   **********************************
* Author:        Agner Fog
* Date created:  2017-04-26
* Last modified: 2021-04-11
* Version:       1.11
* Project:       Binary tools for ForwardCom instruction set
* Module:        disassem.h
* Description:
* Header file for disassembler
*
* Copyright 2006-2021 GNU General Public License http://www.gnu.org/licenses
*****************************************************************************/


/* Additional information stored in symbol records during disassembly:
ElfFwcSym::st_other    bit 31 set if symbol has been written out in section listing
ElfFwcSym::st_reguse1  old symbol index before sorting and adding more symbols, 
                        needs translation to new index in relocation records
ElfFwcSym::st_size     reference symbol if symbol is a relative pointer, stored in bit 32-63 of st_size
ElfFwcSym::st_reguse2  symbol data type: bit 0-2: operand type, bit 3 = 1
                        0x100 code pointer
                        0x200 data pointer
*/


// Universal template for all instruction formats
union STemplate {
    uint64_t     q;          // First 64 bits
    struct {
        uint32_t rt:    5;   // Source register RT in formats A and E
        uint32_t mask:  3;   // Mask register in formats A and E
        uint32_t rs:    5;   // Source register RS in formats A, B, and E
        uint32_t ot:    3;   // Operand type and M in formats A, B, and E
        uint32_t rd:    5;   // Destination register RD in formats A, B, C, and E
        uint32_t op1:   6;   // Destination register RD in formats A, B, C, and E
        uint32_t mode:  3;   // Mode in all formats
        uint32_t il:    2;   // Instruction length in all formats
        uint32_t im2:  16;   // IM2 in format E
        uint32_t im3:   6;   // IM3 in format E
        uint32_t op2:   2;   // OP2 in format E
        uint32_t ru:    5;   // Source register RU in format E
        uint32_t mode2: 3;   // Mode2 in format E
    } a;
    struct {
        int32_t  im2:  24;   // IM2 in format D
        uint32_t op1:   3;   // The remaining part of OP1 in format D
        uint32_t mode:  3;   // Mode in format D
        uint32_t il:    2;   // Instruction length in format D
    } d;
    uint8_t      b[12];      // IM1 in format B
    uint16_t     s[4];       // IM1+2 in format C
    uint32_t     i[3];       // IM2 and IM3 in format A2, A3, B2, B3
    float        f[2];       // IM2 as float
};

// Record in nested lookup lists for formats.
// crit = 0 means that index is an index into the format table
// crit > 0 means that index is an offset into the next table. Add something to this offset according to the criterion
struct SFormatIndex {
    uint8_t  crit;           // Criterion for lookup into next table: 0 = format table. 
                             // 1: mode2, 2: op1 / 8, 3: op1 % 8, 4: IM1 % 64 / 8, 5: IM1 % 8, 
                             // 6: IM12 == 0xFFFF
    uint8_t  index;          // Offset into next table
}; 

// Record in list of formats (formatList) 
struct SFormat { 
    uint16_t format2;        // 0x0XYZ, where X = il, Y = mode, Z = subformat (mode2 or OP1) or variant within format
    uint8_t  category;       // Category: 1 = single format,  3 = multi-format, 4 = jump instruction                         
    uint8_t  tmplate;        // Template: 0xA, 0xB, 0xC, 0xD, 0xE.

    uint8_t  opAvail;        // Operands available: 1 = immediate, 2 = memory,
                             // 0x10 = RT, 0x20 = RS, 0x40 = RU, 0x80 = RD

    uint8_t  ot;             // Operand type. 0: determined by OT field. 0x10 - 0x17: 0-7. 
                             // 0x32: int32 for even OP1, int64 for odd OP1
                             // 0x35: float for even OP1, double for odd OP1

    uint8_t  jumpSize;       // Size of jump offset field (bytes)
    uint8_t  jumpPos;        // Position of jump offset field (bytes)
    uint8_t  addrSize;       // Size of address offset field (bytes)
    uint8_t  addrPos;        // Position of address offset field (bytes)
    uint8_t  immSize;        // Size of first immediate operand (bytes)
    uint8_t  immPos;         // Position of first immediate operand (bytes)

    uint16_t imm2;           // Size and position of extra immediate operands:
                             // 1 = IM2 in template C, 
                             // 2 = IM3 in template E may contain options, 
                             // 4 = IM3 is shift count for IM2 if no options, 
                             // 8 = IM2 is shift count for IM4,
                             // 0x10 = IM3 in template A3 or B3, 
                             // 0x40 = has fixed values
                             // 0x80 = jump OPJ in IM1
                             // 0x90 = jump OPJ is in high part of IM2 in format A2
                             // 0xC0 = jump with no OPJ
                             // 0x100 = OP2 is used for immediate operand as extension of IM3

    uint8_t  vect;           // 1 = vector registers used, 2 = vector length in RT, 4 = broadcast length in RT
                             // 0x10 = vector registers used if M bit

    uint8_t  mem;            // 1 (unused), 2 = base in RS, 4 = index in RT, 
                             // 0x10 = has offset, 0x20 = has limit,

    uint8_t  scale;          // 1 = offset is scaled, 2 = index is scaled by OS, 4 = scale factor is -1
    uint8_t  formatIndex;    // Bit index into format in instruction list
    uint8_t  exeTable;       // table of function pointers used during emulation
};

// tables in emulator2.cpp:
extern const SFormat formatList[];            // == FXEND in disasm1.cpp
extern uint32_t formatListSize;               // size of formatList 
extern const uint32_t dataSizeTable[8];       // = {1, 2, 4, 8, 16, 4, 8, 16};
extern const uint32_t dataSizeTableMax8[8];   // = {1, 2, 4, 8, 8, 4, 8, 8}; 
extern const uint32_t dataSizeTableLog[8];    // = {0, 1, 2, 3, 4, 2, 3, 4};
extern const uint32_t dataSizeTableBits[8];   // = {8, 16, 32, 64, 128, 32, 64, 128};
extern const uint64_t dataSizeMask[8];        // = {FF, FFFF, FFFFFFFF, -1, -1, FFFFFFFF, -1, -1};


// Operator for sorting symbols by address. Used by disassembler
static inline bool operator < (ElfFwcSym const & a, ElfFwcSym const & b) {
    if (a.st_section != b.st_section) return a.st_section < b.st_section;
    return a.st_value < b.st_value;
}

// Operatur == tells if symbols have same address
static inline bool operator == (ElfFwcSym const & a, ElfFwcSym const & b) {
    return a.st_section == b.st_section && a.st_value == b.st_value;
}

// Operator for sorting relocations by address. Used by disassembler
static inline bool operator < (ElfFwcReloc const & a, ElfFwcReloc const & b) {
    if (a.r_section < b.r_section) return true;
    if (a.r_section > b.r_section) return false;
    return a.r_offset < b.r_offset;
}

// Look up format in FormatList (this function is in emulator2.cpp)
uint32_t lookupFormat(uint64_t instruct);

// Check integrity of format lists
void checkFormatListIntegrity();

// Interpret template variants in instruction record
uint64_t interpretTemplateVariants(const char *);
// bits returned by interpretTemplateVariants
const int VARIANT_D0 = (1 << 0);                 // no destination, no operand type
const int VARIANT_D1 = (1 << 1);                 // no destination, but operant type specified
const int VARIANT_D2 = (1 << 2);                 // operant type ignored
const int VARIANT_D3 = (1 << 3);                 // register RD used for other purpose
const int VARIANT_M0 = (1 << 4);                 // memory operand destination
//const int VARIANT_M1 = (1 << 5);                 // IM3 used as extra immediate operand in E formats with a memory operand. obsolote
const int VARIANT_R0 = (1 << 8);                 // destination is general purpose register
const int VARIANT_R1B =       9;                 // bit index to VARIANT_R1
const int VARIANT_R1 = (1 << VARIANT_R1B);       // first source operand is general purpose register
const int VARIANT_R2 = (1 << (VARIANT_R1B+1));   // second source operand is general purpose register
const int VARIANT_R3 = (1 << (VARIANT_R1B+2));   // third source operand is general purpose register
const int VARIANT_R123 = (VARIANT_R1|VARIANT_R2|VARIANT_R3);  // source operand is general purpose register
const int VARIANT_D3R0 = VARIANT_D3 | VARIANT_R0; // RD is general purpose register
const int VARIANT_RL = (1 << 12);                // RS is a general purpose register specifying length
const int VARIANT_F0 = (1 << 14);                // can have mask register, but not fallback register
const int VARIANT_F1 = (1 << 15);                // can have fallback register without mask register
const int VARIANT_I2 = (1 << 16);                // immediate operand is integer
const int VARIANT_U0 = (1 << 18);                // integer operands are unsigned
const int VARIANT_U3 = (1 << 19);                // integer operands are unsigned if bit 3 in IM3 (format 2.4.x, 2.8.x) is set.
//const int VARIANT_Kn = (1 << 20);                // integer operand is implicit
const int VARIANT_On = (7 << 24);                // n IM3 bits used for options
const int VARIANT_H0 = (1 << 28);                // half precision floating point operands
const int VARIANT_SPECB = 32;                    // bit index to special register type
const uint64_t VARIANT_SPEC = (uint64_t)0xF << VARIANT_SPECB; // Special register types for operands
const uint64_t VARIANT_SPECS = 0x1000000000;     // Special register type for source
const uint64_t VARIANT_SPECD = 0x2000000000;     // Special register type for destination


struct SInstruction2;                            // defined below
struct SLineRef;                                 // defined below

uint8_t findFallback(SFormat const * fInstr, STemplate const * pInstr, int nOperands); // find fallback register in instruction code

// class CDisassembler handles disassembly of ForwardCom ELF file
class CDisassembler : public CELF {
public:
    CDisassembler();                             // Constructor
    void getComponents1();                       // Read instruction list, split ELF file into components
    void getComponents2(CELF const & assembler, CMemoryBuffer const & instructList);// Read instruction list, get ELF components for assembler output listing
    void go();                                   // Disassemble
    void getLineList(CDynamicArray<SLineRef> & list); // transfer lineList to debugger
    void getOutFile(CTextFileBuffer & buffer);   // transfer outFile to debugger
    uint32_t outputFile;                         // Output file name, as index into cmd.fileNameBuffer
    uint8_t  debugMode;                          // produce disassembly for emulator/debugger
    uint8_t asmTab0;                             // Column for operand type
    uint8_t asmTab1;                             // Column for opcode
    uint8_t asmTab2;                             // Column for first operand
    uint8_t asmTab3;                             // Column for comment
protected:
    uint32_t pass;                               // Pass number
    uint32_t codeMode;                           // 1 = code, 2 = data in code section, 4 = data section
    uint32_t iInstr;                             // Position of current instruction relative to section start
    uint32_t instrLength;                        // Length of current instruction, in 32-bit words
    uint32_t operandType;                        // Operand type of current instruction
    uint32_t format;                             // Format of current instruction
    uint32_t nextSymbol;                         // Index to next symbol label to write out
    uint32_t section;                            // Current section
    uint32_t sectionEnd;                         // Size of section
    uint64_t sectionAddress;                     // Start address of section
    uint32_t currentFunction;                    // Symbol index of current function
    uint32_t currentFunctionEnd;                 // Address of end of current function
    uint32_t instructionWarning;                 // Warnings and errors for current instruction
    uint32_t relocation;                         // relocation index in current instruction + 1
    int8_t * sectionBuffer;                      // Pointer to start of current section
    uint64_t variant;                            // Template variant and options
    STemplate const * pInstr;                    // Pointer to current instruction code
    SInstruction2 const * iRecord;               // Pointer to instruction table entry
    SFormat const * fInstr;                      // Format details of current instruction code
    CDynamicArray<SInstruction2> instructionlist;// List of instruction set, sorted by category, format, and op1
    CDynamicArray<ElfFwcSym> newSymbols;         // List of new symbols added during pass 1
    CDynamicArray<SLineRef> lineList;            // Cross reference of code addresses to lines in outFile (used by debugger)
    CTextFileBuffer outFile;                     // Output file
    bool isExecutable;                           // Disassembling executable file
    void feedBackText1();                        // Write feedback text on stdout
    void parseInstruction();                     // Parse current instruction
    //void CheckInstructionErrors();             // Check if instruction is valid
    void writeInstruction();                     // Write current instruction to output file
    void writeNormalInstruction();               // Write normal instruction to output file
    void writeJumpInstruction();                 // Write jump instruction to output file
    void writeOperandType(uint32_t ot);          // Write operand type
    void writeMemoryOperand();                   // Write memory operand of current instruction
    void writeImmediateOperand();                // Write immediate operand depending on type in instruction list
    void writeRegister(uint32_t r, uint32_t ot); // Write name of general purpose or vector register
    void writeGPRegister(uint32_t r);            // Write name of general purpose register
    void writeVectorRegister(uint32_t v);        // Write name of vector register
    void writeSpecialRegister(uint32_t r, uint32_t type); // Write name of other type of register
    void pass1();                                // Pass 1 of disassembly. Resolves cross references and adds symbol labels
    void pass2();                                // Pass 2 of disassembly. Writes output file
    void sortSymbolsAndRelocations();            // Sort symbols and relocations by address
    void symbolExeAddress(ElfFwcSym & sym);      // Translate symbol address from section:offset to pointerbase:address
    void updateSymbols();                        // Make missing symbols for jump targets and data references
    void joinSymbolTables();                     // Join the tables: symbols and newSymbols
    void assignSymbolNames();                    // Make names for unnamed symbols
    void initializeInstructionList();            // Read instruction list from file and sort it
    void updateTracer();                         // Trace registers pointing to jump table (to do)
    void followJumpTable(uint32_t symi, uint32_t RelType); // Trace targets of jump table  (to do)
    void markCodeAsDubious();                    // Mark data in code section
    void writeFileBegin();                       // Write beginning of disassembly file
    void writeFileEnd();                         // Write end of disassembly file
    void writeSectionBegin();                    // Write beginning of section
    void writeSectionEnd();                      // Write end of section
    void writeCodeComment();                     // Write comment after instruction
    void writeDataItems();                       // Write data to disassembly file
    void writeLabels();                          // Find and write any labels at current position
    void writeRelocationTarget(uint32_t src, uint32_t size); // Write relocation target for this source position
    void writeJumpTarget(uint32_t src, uint32_t size); // Write jump relocation target for this source position
    void writeWarning(const char * w);           // Write warning message to output file
    void writeError(const char * w);             // Write error message to output file
    void finalErrorCheck();                      // Check for wrong entries in symbol table and relocations table
    //void checkNamesValid();                      // Check for illegal characters in symbols
    void writeSymbolName(uint32_t symi);         // Write name of symbol
    void writeSectionName(int32_t SegIndex);     // Write name of section
    void writePublicsAndExternals();             // Write list of public and external symbols
    void writeAddress();                         // write code address
    void setTabStops();                          // set tab stops for output
};


/*****************************************************************************
Structures and classes for reading instruction list from comma-separated file,
sorting, and searching
*****************************************************************************/

const int maxINameLen = 31;            // Maximum length of instruction name
const int numInstructionColumns = 13;  // Number of columns in csv file to read. Additional columns are ignored

// Record structure for instruction definition
struct SInstruction {
    uint64_t format;                   // Instruction format for single format instructions,
                                       // or one bit for each allowed format for multi format instructions. See table in manual.
    uint64_t variant;                  // Template variant 
    uint32_t id;                       // Instruction id number
    uint8_t  category;                 // 1: single format, 3: multiformat, 4: jump
    uint8_t  templt;                   // Format template. 0xA - 0xE, 0 for multiple templates
    uint8_t  sourceoperands;           // Number of source operands, including register, memory and immediate operands
    uint8_t  op1;                      // Operation code
    uint8_t  op2;                      // Additional operation code
    uint8_t  opimmediate;              // Type of immediate operand for single-format instructions
    uint32_t implicit_imm;             // Value of implicit immediate operand
    uint32_t optypesgp;                // Operand types supported for general purpose registers
    uint32_t optypesscalar;            // Operand types supported for scalars in vector registers
    uint32_t optypesvector;            // Operand types supported for vectors
    char     name[maxINameLen+1];      // Name of instruction. Lower case
};

// Same structure, but sorted by category, format, and operation codes. Used by disassembler
struct SInstruction2 : public SInstruction {
};    

// Same structure, but sorted by id
struct SInstruction3 : public SInstruction {
};    

// Operator for sorting instructions by name. Used by assembler
static inline bool operator < (SInstruction const & a, SInstruction const & b) {
#ifdef _MSC_VER     // case insensitive compare. name depends on compiler
    return _strcmpi(a.name, b.name) < 0;
#else
    return strcasecmp(a.name, b.name) < 0;
#endif
}

// Operator for sorting instructions by category, format, and operation codes. Used by disassembler
static inline bool operator < (SInstruction2 const & a, SInstruction2 const & b) {
    // first sort criterion is category
    if (a.category < b.category) return true;
    if (a.category > b.category) return false;
    // sort by format for single-format instructions
    if (a.category == 1) {
        if (a.format < b.format) return true;
        if (a.format > b.format) return false;
    }
    // sort by op1
    if (a.op1 < b.op1) return true;
    if (a.op1 > b.op1) return false;
    // last sort criterion is op2
    return a.op2 < b.op2;
}

// Operator for sorting instructions by id
static inline bool operator < (SInstruction3 const & a, SInstruction3 const & b) {
    return a.id < b.id;
} 

// class for reading comma-separated file
class CCSVFile : public CFileBuffer {
public:
    CCSVFile() : CFileBuffer() {}                // Constructor
    void parse();                                // Read and parse file
    CDynamicArray<SInstruction> instructionlist; // List of records
    uint64_t interpretNumber(const char * text); // Interpret number in instruction list
};

// Cross reference of code addresses to lines in outFile. Used by debugger
struct SLineRef {
    uint64_t address;                            // code address
    uint32_t domain;                             // 1 = IP, 2 = datap, 4 = threadp
    uint32_t textPos;                            // position of corresponding line in outFile
};

// Operators for sorting SLineRef by address
static inline bool operator < (SLineRef const & a, SLineRef const & b) {
    if (a.domain != b.domain) return a.domain < b.domain;
    return a.address < b.address;
};

static inline bool operator == (SLineRef const & a, SLineRef const & b) {
    return a.domain == b.domain && a.address == b.address;
};

static inline bool operator != (SLineRef const & a, SLineRef const & b) {
    return !(a == b);
};

// Interpret a string with a decimal, binary, octal, or hexadecimal number
int64_t interpretNumber(const char * s, uint32_t maxLength, uint32_t * error);

double  interpretFloat(const char * s, uint32_t length);    // interpret floating point number from string with indicated length

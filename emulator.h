/****************************  emulator.h   **********************************
* Author:        Agner Fog
* date created:  2018-02-18
* Last modified: 2020-05-17
* Version:       1.10
* Project:       Binary tools for ForwardCom instruction set
* Module:        emulator.h
* Description:
* Header file for emulator
*
* Copyright 2018-2020 GNU General Public License http://www.gnu.org/licenses
*****************************************************************************/

// structure for memory map
struct SMemoryMap {
    uint64_t startAddress;                       // virtual address boundary (must be divisible by 8)
    uint64_t access_addend;                      // (access_addend & 7) is access permission: SHF_READ, SHF_WRITE, SHF_EXEC
                                                 // (access_addend & ~7) is added to the virtual address to get physical address
}; 

// union for an operand value of any type
union SNum {
    uint64_t q;                                  // 64 bit unsigned integer
    int64_t  qs;                                 // 64 bit signed integer
    uint32_t i;                                  // 32 bit unsigned integer
    int32_t  is;                                 // 32 bit signed integer
    uint16_t s;                                  // 16 bit unsigned integer
    int16_t  ss;                                 // 16 bit signed integer
    uint8_t  b;                                  // 8 bit  unsigned integer
    int8_t   bs;                                 // 8 bit  signed integer
    double d;                                    // double precision float
    float f;                                     // single precision float
};

class CEmulator;                                 // preliminary declaration

// Class for a thread or CPU core in the emulator
class CThread {
public:
    CThread();                                   // constructor
    ~CThread();                                  // destructor
    void run();                                  // start running
    void setRegisters(CEmulator * emulator);     // initialize registers etc.
    uint64_t ip;                                 // instruction pointer
    uint64_t ip0;                                // address base for code and read-only data
    uint64_t datap;                              // base pointer for writeable data
    uint64_t threadp;                            // base pointer for thread-local data
    uint64_t ninstructions;                      // number of instructions executed
    uint32_t numContr;                           // numeric control register
    uint32_t lastMask;                           // shows last status of subnormal support
    uint32_t options;                            // option bits in instruction
    uint32_t exception;                          // exception or jump caused by current instruction
    STemplate const * pInstr;                    // current instruction code
    SFormat  const * fInstr;                     // format of current instruction
    SNum     parm[6];                            // parm[0] = value of first operand if 3 operands
                                                 // parm[1] = value of first operand if 2 operands or second operand if 3 operands
                                                 // parm[2] = value of last operand
                                                 // parm[3] = value of mask register or NUMCONTR
                                                 // parm[4] = value of immediate operand without shift or conversion
                                                 // parm[5] = high part of double size return value
    uint8_t  operands[6];                        // instruction operands. 0x00-0x1F = register. 0x20 = immediate, 0x40 = memory
                                                 // operands[0] is destination register
                                                 // operands[1] is mask register
                                                 // operands[2] is fallback register
                                                 // two-operand instructions use operands[4-5]
                                                 // three-operand instructions use operands[3-5]
    //uint8_t  rs;                                 // register rs (stored here because the number is lost on tiny instruction memory operands)
    uint8_t  op;                                 // operation code
    uint8_t  operandType;                        // operand type for current instruction
    uint8_t  nOperands;                          // number of source operands for current instruction
    uint8_t  vect;                               // instruction uses vector registers
    uint8_t  running;                            // thread is running. 0 = stop, 1 = save RD, 2 = don't save RD
    bool     readonly;                           // expect memory address to be in read-only section
    bool     ignoreMask;                         // call execution function even if mask is zero
    bool     doubleStep;                         // execution function will process two vector elements at a time
    bool     noVectorLength;                     // RS is not a vector register, or vector length is determined by execution function
    bool     dontRead;                           // don't read source operand before execution
    bool     unchangedRd;                        // store instruction: RD is not destination
    bool     terminate;                          // stop execution
    CMemoryBuffer vectors;                       // vector register i is at offset i*MaxVectorLength
    uint64_t registers[32];                      // value of register r0 - r31
    uint32_t vectorLength[32];                   // length of vector registers v0 - v31
    uint32_t vectorLengthM;                      // vector length of memory operand
    uint32_t vectorLengthR;                      // vector length of result
    uint32_t vectorOffset;                       // offset to current element within vector
    uint32_t MaxVectorLength;                    // maximum vector length
    uint32_t returnType;                         // debug return output. bit 0-3: operand type (8 = half precision). bit 4: register. bit 5: memory. //(bit6: one extra element save_cp)
                                                 // bit 8: vector. bit 12: jump. bit 13: jump taken
    int8_t * memory;                             // program memory
    int8_t * tempBuffer;                         // temporary buffer for vector operand
    uint64_t memAddress;                         // address of memory operand
    int64_t  addrOperand;                        // relative address of memory operand or jump target
    uint64_t readVectorElement(uint32_t v, uint32_t vectorOffset); // read vector element
    void writeVectorElement(uint32_t v, uint64_t value, uint32_t vectorOffset); // write vector element
    uint64_t getMemoryAddress();                 // get address of a memory operand
    uint64_t readMemoryOperand(uint64_t address);// read a memory operand
    void writeMemoryOperand(uint64_t val, uint64_t address);  // write a memory operand
    void interrupt(uint32_t n);                  // interrupt or trap
    uint64_t checkSysMemAccess(uint64_t address, uint64_t size, uint8_t rd, uint8_t rs, uint8_t mode);
    int fprintfEmulated(FILE * stream, const char * format, uint64_t * argumentList); // emulate fprintf with ForwardCom argument list
    // check if system function has access to a particular address
    void systemCall(uint32_t mod, uint32_t funcid, uint8_t rd, uint8_t rs); // entry for system calls
    uint64_t makeNan(uint32_t code, uint32_t operandType);// make a NAN with exception code and address in payload
    CDynamicArray<uint64_t> callStack;           // stack of return addresses
    uint32_t callDept;                           // maximum number of entries observed in callStack
    uint64_t entry_point;                        // program entry point
protected:
    uint32_t mapIndex1;                          // last memory map index for code
    uint32_t mapIndex2;                          // last memory map index for read-only data
    uint32_t mapIndex3;                          // last memory map index for writeable data
    CEmulator * emulator;                        // pointer to owner
    CDynamicArray<SMemoryMap> memoryMap;         // memory map
    CTextFileBuffer listOut;                     // output debug listing
    uint32_t listFileName;                       // file name for listOut (index into cmd.fileNameBuffer)
    uint32_t listLines;                          // line counter
    void fetch();                                // fetch next instruction
    void decode();                               // decode current instruction
    void execute();                              // execute current instruction
    void listStart();                            // start writing debug list
    void listInstruction(uint64_t address);      // write current instruction to debug list
public:
    void listResult(uint64_t result);            // write result of current instruction to debug list
    uint64_t readRegister(uint8_t reg) {         // read register value
        if (vect) {                              // this function is inlined for performance reasons
            uint64_t val = vectors.get<uint64_t>(reg*MaxVectorLength);
            if (vectorLength[reg] < 8) {
                // vector is less than 8 bytes. zero-extend to 8 bytes
                val &= ((uint64_t)1 << vectorLength[reg]) - 1;
            }
            return val;
        }
        else {
            return registers[reg];
        }
    }        
};

// Class for the whole emulator
class CEmulator : public CELF {
public:
    CEmulator();                                 // constructor
    ~CEmulator();                                // destructor
    void go();                                   // start
protected:
    void load();                                 // load executable file into memory
    void relocate();                             // relocate any absolute addresses and system function id's
    void disassemble();                          // make disassembly listing for debug output
    uint32_t MaxVectorLength;                    // maximum vector length
    int8_t * memory;                             // program memory
    uint64_t memsize;                            // total allocated memory size
    uint32_t maxNumThreads;                      // maximum number of threads
    uint64_t ip0;                                // address base for code and read-only data
    uint64_t datap0;                             // address base for writeable data
    uint64_t threadp0;                           // address base for thread data of main thread
    uint64_t stackp;                             // pointer to stack
    uint64_t stackSize;                          // data stack size for main thread
    uint64_t callStackSize;                      // call stack size for main thread
    uint64_t heapSize;                           // heap size for main thread
    uint32_t environmentSize;                    // maximum size of environment and command line data
    CMetaBuffer<CThread> threads;                // one or more threads
    CDynamicArray<SMemoryMap> memoryMap;         // main memory map
    CDynamicArray<SLineRef> lineList;            // Cross reference of code addresses to lines in dissassembler output
    CTextFileBuffer disassemOut;                 // Output file from disassembler
    CDisassembler disassembler;                  // disassembler for producing output list
    friend class CThread;
};

// Functions for floating point exception and rounding control
void setRoundingMode(uint8_t r);
void clearExceptionFlags();
uint32_t getExceptionFlags();
void enableSubnormals(uint32_t e);

// universal function type for execution function
// all operands and option bits are accessed via *thread
typedef uint64_t (*PFunc)(CThread * thread);

// Tables of execution functions
extern PFunc funcTab1[64];                       // multiformat instructions
extern PFunc funcTab2[64];                       // jump instructions
extern PFunc funcTab3[16];                       // jump instructions with 24 bit offset
// single format instructions:
extern PFunc funcTab4[64];                       // format 1.0
extern PFunc funcTab5[64];                       // format 1.1
extern PFunc funcTab6[64];                       // format 1.2
extern PFunc funcTab7[64];                       // format 1.3
extern PFunc funcTab8[64];                       // format 1.4
extern PFunc funcTab9[64];                       // format 1.8
extern PFunc funcTab10[64];                      // format 2.5
extern PFunc funcTab11[64];                      // format 2.6
extern PFunc funcTab12[64];                      // format 2.9
extern PFunc funcTab13[64];                      // format 3.1

// Table of execution function tables, indexed by fInstr->exeTable
extern PFunc * metaFunctionTable[];
// Table of dispatch functions for single format instructions with E template
extern PFunc EDispatchTable[];

// Table of number of operands for each instruction
extern uint8_t numOperands[15][64];
extern uint8_t numOperands2071[64];
extern uint8_t numOperands2261[64];
extern uint8_t numOperands2271[64];

// Execution functions shared between multiple cpp files
uint64_t f_nop(CThread * thread);
uint64_t f_add(CThread * thread);
uint64_t f_sub(CThread * thread);
uint64_t f_mul(CThread * thread);
uint64_t f_div(CThread * thread);
uint64_t f_mul_add(CThread * thread);
uint64_t f_add_h(CThread * thread);
uint64_t f_mul_h(CThread * thread);
uint64_t insert_(CThread * t);
uint64_t extract_(CThread * t);
uint64_t bitscan_(CThread * t);
uint64_t popcount_(CThread * t);


// constants and functions for detecting NAN and infinity
const uint16_t inf_h   = 0x7C00;                 // float16 infinity
const uint16_t inf2h   = inf_h*2;                // for detecting infinity when sign bit has been shifted out
const uint32_t inf_f   = 0x7F800000;             // float infinity
const uint32_t inf2f   = inf_f*2;                // for detecting infinity when sign bit has been shifted out
const uint32_t nan_f   = 0x7FC00000;             // float nan
const uint32_t sign_f  = 0x80000000;             // float  sign bit
const uint32_t nsign_f = 0x7FFFFFFF;             // float not sign bit
const uint64_t inf_d   = 0x7FF0000000000000;     // double infinity
const uint64_t inf2d   = inf_d*2;                // for detecting infinity when sign bit has been shifted out
const uint64_t nan_d   = 0x7FF8000000000000;     // double nan
const uint64_t nsign_d = 0x7FFFFFFFFFFFFFFF;     // double not sign bit
const uint64_t sign_d  = 0x8000000000000000;     // double sign bit

// functions applied to the bit representations of floating point numbers to detect NAN and infinity:
static inline bool isnan_h(uint16_t x) {return uint16_t(x << 1) > inf2h;}
static inline bool isnan_f(uint32_t x) {return (x << 1) > inf2f;}
static inline bool isnan_d(uint64_t x) {return (x << 1) > inf2d;}
static inline bool isinf_h(uint16_t x) {return uint16_t(x << 1) == inf2h;}
static inline bool isinf_f(uint32_t x) {return (x << 1) == inf2f;}
static inline bool isinf_d(uint64_t x) {return (x << 1) == inf2d;}
static inline bool isnan_or_inf_h(uint16_t x) {return uint16_t(x << 1) >= inf2h;}
static inline bool isnan_or_inf_f(uint32_t x) {return (x << 1) >= inf2f;}
static inline bool isnan_or_inf_d(uint64_t x) {return (x << 1) >= inf2d;}
static inline bool is_zero_or_subnormal_h(uint16_t x) {return (x & 0x7C00) == 0;}

/****************************    elf_forwardcom.h    **************************
* Author:              Agner Fog
* Date created:        2016-06-25
* Last modified:       2021-05-28
* ForwardCom version:  1.11
* Program version:     1.11
* Project:             ForwardCom binary tools
* Description:         Definition of ELF file format. See below
*
* To do: define exception handler and stack unwind information
* To do: define stack size and heap size information
* To do: define memory reservation for runtime linking
* To do: define formats for debug information
* To do: define access rights of executable file or device driver
*
* Copyright 2016-2021 GNU General Public License v. 3
* http://www.gnu.org/licenses/gpl.html
*******************************************************************************

This C/C++ header file contains the official definition of the ForwardCom
variant of the ELF file format for object files and executable files.
The latest version is stored at https://github.com/ForwardCom/bintools

An executable file contains the following elements:
1. ELF file header with the structure ElfFwcEhdr 
2. Any number of program headers with the structure ElfFwcPhdr
3. Raw data. Each section aligned by 8
4. Any number of section headers with the structure ElfFwcShdr
   The sections can have different types as defined by sh_type, including
   code, data, symbol tables, string tables, and relocation records.

The program headers and section headers may point to the same raw data. The
program headers are used by the loader and the section headers are used by the
linker. An object file has the same format, but with no program headers.

The program headers in an executable file must come in the following order:
* const (ip)
* code (ip)
* data (datap)
* bss (datap)
* data (threadp)
* bss (threadp)
There may be any number of headers in each category.
The raw data in an executable file must come in the same order as the headers 
that point to them. 
These rules are intended to simplify boot loader code in small devices.


ForwardCom library files have the standard UNIX archive format with a sorted
symbol table. The details are described below. Dynamic link libraries and 
shared objects are not used in the ForwardCom system.

******************************************************************************/

#ifndef ELF_FORW_H
#define ELF_FORW_H  111    // version number


//--------------------------------------------------------------------------
//                         ELF FILE HEADER
//--------------------------------------------------------------------------

struct ElfFwcEhdr {
  uint8_t   e_ident[16];   // Magic number and other info
                           // e_ident[EI_CLASS] = ELFCLASS64: file class
                           // e_ident[EI_DATA] = ELFDATA2LSB: 2's complement, little endian
                           // e_ident[EI_VERSION] = EV_CURRENT: current ELF version
                           // e_ident[EI_OSABI] = ELFOSABI_FORWARDCOM
                           // e_ident[EI_ABIVERSION] = 0 
                           // The rest is unused padding   
  uint16_t  e_type;        // Object file type
  uint16_t  e_machine;     // Architecture
  uint32_t  e_version;     // Object file version
  uint64_t  e_entry;       // Entry point virtual address
  uint64_t  e_phoff;       // Program header table file offset
  uint64_t  e_shoff;       // Section header table file offset
  uint32_t  e_flags;       // Processor-specific flags. We may define any values for these flags
  uint16_t  e_ehsize;      // ELF header size in bytes
  uint16_t  e_phentsize;   // Program header table entry size
  uint16_t  e_phnum;       // Program header table entry count
  uint16_t  e_shentsize;   // Section header table entry size
  uint32_t  e_shnum;       // Section header table entry count (was uint16_t)
  uint32_t  e_shstrndx;    // Section header string table index (was uint16_t)
  // additional fields for ForwardCom
  uint32_t  e_stackvect;   // number of vectors to store on stack. multiply by max vector length and add to stacksize
  uint64_t  e_stacksize;   // size of stack for main thread
  uint64_t  e_ip_base;     // __ip_base relative to first ip based segment
  uint64_t  e_datap_base;  // __datap_base relative to first datap based segment
  uint64_t  e_threadp_base;// __threadp_base relative to first threadp based segment
};


// Fields in the e_ident array.  The EI_* macros are indices into the array.  
// The macros under each EI_* macro are the values the byte may have. 

// Conglomeration of the identification bytes, for easy testing as a word.
//#define ELFMAG        "\177ELF"
#define ELFMAG        0x464C457F  // 0x7F 'E' 'L' 'F'

// File class
#define EI_CLASS      4    // File class byte index
#define ELFCLASSNONE  0    // Invalid class
#define ELFCLASS32    1    // 32-bit objects
#define ELFCLASS64    2    // 64-bit objects *
#define ELFCLASSNUM   3

#define EI_DATA       5    // Data encoding byte index
#define ELFDATANONE   0    // Invalid data encoding
#define ELFDATA2LSB   1    // 2's complement, little endian *
#define ELFDATA2MSB   2    // 2's complement, big endian
#define ELFDATANUM    3

#define EI_VERSION    6    // File version byte index

#define EI_OSABI               7  // OS ABI identification
#define ELFOSABI_SYSV          0  // UNIX System V ABI
#define ELFOSABI_HPUX          1  // HP-UX 
#define ELFOSABI_ARM          97  // ARM 
#define ELFOSABI_STANDALONE  255  // Standalone (embedded) application
#define ELFOSABI_FORWARDCOM  250  // ForwardCom 

#define EI_ABIVERSION    8    // x86 ABI version
#define EI_ABIVERSION_FORWARDCOM    1    // ForwardCom ABI version

#define EI_PAD           9    // Byte index of padding bytes

// Legal values for e_type (object file type). 
#define ET_NONE          0    // No file type
#define ET_REL           1    // Relocatable file
#define ET_EXEC          2    // Executable file
#define ET_DYN           3    // Shared object file (not used by ForwardCom)
#define ET_CORE          4    // Core file 
#define ET_NUM           5    // Number of defined types
#define ET_LOOS     0xfe00    // OS-specific range start
#define ET_HIOS     0xfeff    // OS-specific range end
#define ET_LOPROC   0xff00    // Processor-specific range start
#define ET_HIPROC   0xffff    // Processor-specific range end

// Legal values for e_machine (architecture)
#define EM_NONE          0     // No machine
#define EM_M32           1     // AT&T WE 32100
#define EM_SPARC         2     // SUN SPARC
#define EM_386           3     // Intel 80386
#define EM_68K           4     // Motorola m68k family
#define EM_88K           5     // Motorola m88k family
#define EM_860           7     // Intel 80860
#define EM_MIPS          8     // MIPS R3000 big-endian
#define EM_S370          9     // IBM System/370
#define EM_MIPS_RS3_LE  10     // MIPS R3000 little-endian
#define EM_PARISC       15     // HPPA
#define EM_VPP500       17     // Fujitsu VPP500
#define EM_SPARC32PLUS  18     // Sun's "v8plus"
#define EM_960          19     // Intel 80960
#define EM_PPC          20     // PowerPC
#define EM_PPC64        21     // PowerPC 64-bit
#define EM_S390         22     // IBM S390
#define EM_V800         36     // NEC V800 series
#define EM_FR20         37     // Fujitsu FR20
#define EM_RH32         38     // TRW RH-32
#define EM_RCE          39     // Motorola RCE
#define EM_ARM          40     // ARM
#define EM_FAKE_ALPHA   41     // Digital Alpha
#define EM_SH           42     // Hitachi SH
#define EM_SPARCV9      43     // SPARC v9 64-bit
#define EM_TRICORE      44     // Siemens Tricore
#define EM_ARC          45     // Argonaut RISC Core
#define EM_H8_300       46     // Hitachi H8/300
#define EM_H8_300H      47     // Hitachi H8/300H
#define EM_H8S          48     // Hitachi H8S
#define EM_H8_500       49     // Hitachi H8/500
#define EM_IA_64        50     // Intel Merced
#define EM_MIPS_X       51     // Stanford MIPS-X
#define EM_COLDFIRE     52     // Motorola Coldfire
#define EM_68HC12       53     // Motorola M68HC12
#define EM_MMA          54     // Fujitsu MMA Multimedia Accelerator
#define EM_PCP          55     // Siemens PCP
#define EM_NCPU         56     // Sony nCPU embeeded RISC
#define EM_NDR1         57     // Denso NDR1 microprocessor
#define EM_STARCORE     58     // Motorola Start*Core processor
#define EM_ME16         59     // Toyota ME16 processor
#define EM_ST100        60     // STMicroelectronic ST100 processor
#define EM_TINYJ        61     // Advanced Logic Corp. Tinyj emb.fam
#define EM_X86_64       62     // AMD x86-64 architecture
#define EM_PDSP         63     // Sony DSP Processor
#define EM_FX66         66     // Siemens FX66 microcontroller
#define EM_ST9PLUS      67     // STMicroelectronics ST9+ 8/16 mc
#define EM_ST7          68     // STmicroelectronics ST7 8 bit mc
#define EM_68HC16       69     // Motorola MC68HC16 microcontroller
#define EM_68HC11       70     // Motorola MC68HC11 microcontroller
#define EM_68HC08       71     // Motorola MC68HC08 microcontroller
#define EM_68HC05       72     // Motorola MC68HC05 microcontroller
#define EM_SVX          73     // Silicon Graphics SVx
#define EM_AT19         74     // STMicroelectronics ST19 8 bit mc
#define EM_VAX          75     // Digital VAX
#define EM_CRIS         76     // Axis Communications 32-bit embedded processor
#define EM_JAVELIN      77     // Infineon Technologies 32-bit embedded processor
#define EM_FIREPATH     78     // Element 14 64-bit DSP Processor
#define EM_ZSP          79     // LSI Logic 16-bit DSP Processor
#define EM_MMIX         80     // Donald Knuth's educational 64-bit processor
#define EM_HUANY        81     // Harvard University machine-independent object files
#define EM_PRISM        82     // SiTera Prism
#define EM_AVR          83     // Atmel AVR 8-bit microcontroller
#define EM_FR30         84     // Fujitsu FR30
#define EM_D10V         85     // Mitsubishi D10V
#define EM_D30V         86     // Mitsubishi D30V
#define EM_V850         87     // NEC v850
#define EM_M32R         88     // Mitsubishi M32R
#define EM_MN10300      89     // Matsushita MN10300
#define EM_MN10200      90     // Matsushita MN10200
#define EM_PJ           91     // picoJava
#define EM_OPENRISC     92     // OpenRISC 32-bit embedded processor
#define EM_RISCV        243    // RISC-V
#define EM_OR32         0x8472 // Open RISC
#define EM_ALPHA        0x9026 // Digital Alpha
#define EM_FORWARDCOM   0x6233 // ForwardCom preliminary value (constructed from F=6, W=23, C=3)

// Legal values for e_version (version).
#define EV_NONE          0    // Invalid ELF version
#define EV_CURRENT       1    // Current version
#define EV_NUM           2

// Values for e_flags (file header flags)
#define EF_INCOMPLETE            0x01  // Incomplete executable file contains unresolved references
#define EF_RELINKABLE            0x02  // Relinking of executable file is possible
#define EF_RELOCATE              0x10  // Relocation needed when program is loaded
#define EF_POSITION_DEPENDENT    0x20  // Contains position-dependent relocations. Multiple processes cannot share same read-only data and code


//--------------------------------------------------------------------------
//                         SECTION HEADER
//--------------------------------------------------------------------------

struct ElfFwcShdr {
  uint32_t  sh_name;      // Section name (string table index)
  uint32_t  sh_flags;     // Section flags
  uint64_t  sh_addr;      // Address relative to section group begin
  uint64_t  sh_offset;    // Section file offset
  uint64_t  sh_size;      // Section size in bytes
  uint32_t  sh_link;      // Link to symbol section or string table
  uint32_t  sh_entsize;   // Entry size if section holds table
  uint32_t  sh_module;    // Module name in relinkable executable
  uint32_t  sh_library;   // Library name in relinkable executable
  uint32_t  unused1;      // Alignment filler
  uint8_t   sh_type;      // Section type
  uint8_t   sh_align;     // Section alignment = 1 << sh_align
  uint8_t   sh_relink;    // Commands used during relinking. Unused in file
  uint8_t   unused2;      // Unused filler
};

// Legal values for sh_type (section type)
#define SHT_NULL                    0  // Section header table entry unused
#define SHT_SYMTAB                  2  // Symbol table. There can be only one symbol table
#define SHT_STRTAB                  3  // String table. There are two string tables, one for symbol names and one for section names
#define SHT_RELA                    4  // Relocation entries with addends
#define SHT_NOTE                    7  // Notes
#define SHT_PROGBITS             0x11  // Program data
#define SHT_NOBITS               0x12  // Uninitialized data space (bss)
#define SHT_COMDAT               0x14  // Communal data or code. Duplicate and unreferenced sections are removed
#define SHT_ALLOCATED            0x10  // Allocated at runtime. This bits indicates SHT_PROGBITS, SHT_NOBITS, SHT_COMDAT
#define SHT_LIST                 0x20  // Other list. Not loaded into memory. (unsorted event list, )
#define SHT_STACKSIZE            0x41  // Records for calculation of stack size
#define SHT_ACCESSRIGHTS         0x42  // Records for indicating desired access rights of executable file or device driver
// obsolete types, not belonging to ForwardCom
//#define SHT_REL                     9  // Relocation entries, no addends
//#define SHT_HASH                    5  // Symbol hash table
//#define SHT_DYNAMIC                 6  // Dynamic linking information
//#define SHT_DYNSYM                0xB  // Dynamic linker symbol table
//#define SHT_SHLIB                 0xA  // Reserved
//#define SHT_GROUP                0x11  // Section group

// Legal values for sh_flags (section flags). 
#define SHF_EXEC                  0x1  // Executable
#define SHF_WRITE                 0x2  // Writable
#define SHF_READ                  0x4  // Readable
#define SHF_PERMISSIONS          (SHF_EXEC | SHF_WRITE | SHF_READ) // access permissions mask
#define SHF_MERGE                0x10  // Elements with same value might be merged
#define SHF_STRINGS              0x20  // Contains nul-terminated strings
#define SHF_INFO_LINK            0x40  // sh_info contains section header index
#define SHF_ALLOC               0x100  // Occupies memory during execution
#define SHF_IP                 0x1000  // Addressed relative to IP (executable and read-only sections)
#define SHF_DATAP              0x2000  // Addressed relative to DATAP (writeable data sections)
#define SHF_THREADP            0x4000  // Addressed relative to THREADP (thread-local data sections)
#define SHF_BASEPOINTER      (SHF_IP | SHF_DATAP | SHF_THREADP)  // mask to detect base pointer
#define SHF_EVENT_HND        0x100000  // Event handler list, contains ElfFwcEvent structures
#define SHF_EXCEPTION_HND    0x200000  // Exception handler and stack unroll information
#define SHF_DEBUG_INFO       0x400000  // Debug information
#define SHF_COMMENT          0x800000  // Comments, including copyright and required libraries
#define SHF_RELINK          0x1000000  // Section in executable file can be relinked
#define SHF_FIXED           0x2000000  // Non-relinkable section in relinkable file has fixed address relative to base pointers
#define SHF_AUTOGEN         0x4000000  // Section is generated by the linker. remake when relinking


//--------------------------------------------------------------------------
//                         SYMBOL TABLES
//--------------------------------------------------------------------------

// Symbol table entry, x64
struct Elf64_Sym {
    uint32_t  st_name;       // Symbol name (string tbl index)
    uint8_t   st_type: 4,    // Symbol type
              st_bind: 4;    // Symbol binding
    uint8_t   st_other;      // Symbol visibility
    uint16_t  st_section;    // Section index
    uint64_t  st_value;      // Symbol value
    uint64_t  st_size;       // Symbol size
};

// Symbol table entry, ForwardCom
struct ElfFwcSym {
  uint32_t  st_name;       // Symbol name (string table index)
  uint8_t   st_type;       // Symbol type
  uint8_t   st_bind;       // Symbol binding
  uint8_t   unused1, unused2;// Alignment fillers
  uint32_t  st_other;      // Symbol visibility and additional type information
  uint32_t  st_section;    // Section header index (zero for external symbols)
  uint64_t  st_value;      // Symbol value
  uint32_t  st_unitsize;   // Size of array elements or data unit. Data type is given by st_unitsize and STV_FLOAT
                           // st_unitsize is 4 or more for executable code
  uint32_t  st_unitnum;    // Symbol size = st_unitsize * st_unitnum 
  uint32_t  st_reguse1;    // Register use. bit 0-31 = r0-r31
  uint32_t  st_reguse2;    // Register use. bit 0-31 = v0-v31
};

// Values for st_bind: symbol binding
#define STB_LOCAL           0     // Local symbol
#define STB_GLOBAL          1     // Global symbol
#define STB_WEAK            2     // Weak symbol
#define STB_WEAK2           6     // Weak public symbol with local reference is both import and export
#define STB_UNRESOLVED   0x0A     // Symbol is unresolved. Treat as weak
#define STB_IGNORE       0x10     // This value is used only internally in the linker (ignore weak/strong during search; ignore overridden weak symbol)
#define STB_EXE          0x80     // This value is used only internally in the linker (copy to executable file)

// Values for st_type: symbol type
#define STT_NOTYPE          0     // Symbol type is unspecified
#define STT_OBJECT          1     // Symbol is a data object
#define STT_FUNC            2     // Symbol is a code object
#define STT_SECTION         3     // Symbol is a section begin
#define STT_FILE            4     // Symbol's name is file name
#define STT_COMMON          5     // Symbol is a common data object. Use STV_COMMON instead!
//#define STT_TLS           6     // Thread local data object. Use STV_THREADP instead!
#define STT_CONSTANT     0x10     // Symbol is a constant with no address
#define STT_VARIABLE     0x11     // Symbol is a variable used during assembly. Should not occur in object file
#define STT_EXPRESSION   0x12     // Symbol is an expression used during assembly. Should not occur in object file
#define STT_TYPENAME     0x14     // Symbol is a type name used during assembly. Should not occur in object file

// Symbol visibility specification encoded in the st_other field. 
#define STV_DEFAULT         0     // Default symbol visibility rules
//#define STV_INTERNAL        1     // Processor specific hidden class
#define STV_HIDDEN         0x20     // Symbol unavailable in other modules
//#define STV_PROTECTED       3     // Not preemptible, not exported
// st_other types added for ForwardCom:
#define STV_EXEC         SHF_EXEC    // = 0x1. Executable code
#define STV_WRITE        SHF_WRITE   // = 0x2. Writable data
#define STV_READ         SHF_READ    // = 0x4. Readable data
#define STV_IP           SHF_IP      // = 0x1000. Addressed relative to IP (in executable and read-only sections)
#define STV_DATAP        SHF_DATAP   // = 0x2000. Addressed relative to DATAP (in writeable data sections)
#define STV_THREADP      SHF_THREADP // = 0x4000. Addressed relative to THREADP (in thrad local data sections)
#define STV_REGUSE       0x10000     // st_reguse field contains register use information
#define STV_FLOAT        0x20000     // st_value is a double precision floating point (with STT_CONSTANT)
#define STV_STRING       0x40000     // st_value is an assemble-time string. Should not occur in object file
#define STV_COMMON      0x100000     // Symbol is communal. Multiple identical instances can be joined. Unreferenced instances can be removed
#define STV_UNWIND      0x400000     // Symbol is a table with exception handling and stack unwind information
#define STV_DEBUG       0x800000     // Symbol is a table with debug information
#define STV_RELINK    SHF_RELINK     // Symbol in executable file can be relinked
#define STV_AUTOGEN   SHF_AUTOGEN    // Symbol is generated by the linker. remake when relinking
#define STV_MAIN      0x10000000     // Main entry point in executable file
#define STV_EXPORTED  0x20000000     // Exported from executable file
#define STV_THREAD    0x40000000     // Thread function. Requires own stack
#define STV_SECT_ATTR (SHF_EXEC | SHF_READ | SHF_WRITE | SHF_IP | SHF_DATAP | SHF_THREADP | SHF_RELINK | SHF_AUTOGEN) // section attributes to copy to symbol


/* Definition of absolute symbols:
x86 ELF uses symbols with st_section = SHN_ABS_X86 to indicate a public absolute symbol.
ForwardCom uses st_type = STT_CONSTANT and sets st_section to the index of an arbitrary
section in the same module as the absolute symbol. This is necessary for indicating which
module an absolute symbol belongs to in a relinkable executable file. An object file
defining absolute symbols must have at least one section, even if it is empty.
*/
// Special section indices. not used in ForwardCom
#define SHN_UNDEF                     0    // Undefined section. external symbol
//#define SHN_LORESERVE  ((int16_t)0xff00) // Start of reserved indices
//#define SHN_LOPROC     ((int16_t)0xff00) // Start of processor-specific
//#define SHN_HIPROC     ((int16_t)0xff1f) // End of processor-specific
//#define SHN_LOOS       ((int16_t)0xff20) // Start of OS-specific
//#define SHN_HIOS       ((int16_t)0xff3f) // End of OS-specific
#define SHN_ABS_X86      ((int16_t)0xfff1) // Associated symbol is absolute (x86 ELF)
//#define SHN_COMMON     ((int16_t)0xfff2) // Associated symbol is common (x86 ELF)
//#define SHN_XINDEX     ((int16_t)0xffff) // Index is in extra table
//#define SHN_HIRESERVE  ((int16_t)0xffff) // End of reserved indices


//--------------------------------------------------------------------------
//                         RELOCATION TABLES
//--------------------------------------------------------------------------

// Relocation table entry with addend, x86-64 in section of type SHT_RELA. Not used in ForwardCom
struct Elf64_Rela {
    uint64_t  r_offset;           // Address
    uint32_t  r_type;             // Relocation type
    uint32_t  r_sym;              // Symbol index
    int64_t   r_addend;           // Addend
};

// Relocation table entry for ForwardCom (in section of type SHT_RELA).
struct ElfFwcReloc {
    uint64_t  r_offset;           // Address relative to section
    uint32_t  r_section;          // Section index
    uint32_t  r_type;             // Relocation type
    uint32_t  r_sym;              // Symbol index
    int32_t   r_addend;           // Addend
    uint32_t  r_refsym;           // Reference symbol
};


// AMD x86-64 relocation types
#define R_X86_64_NONE       0     // No reloc
#define R_X86_64_64         1     // Direct 64 bit 
#define R_X86_64_PC32       2     // Self relative 32 bit signed (not RIP relative in the sense used in COFF files)
#define R_X86_64_GOT32      3     // 32 bit GOT entry
#define R_X86_64_PLT32      4     // 32 bit PLT address
#define R_X86_64_COPY       5     // Copy symbol at runtime
#define R_X86_64_GLOB_DAT   6     // Create GOT entry
#define R_X86_64_JUMP_SLOT  7     // Create PLT entry
#define R_X86_64_RELATIVE   8     // Adjust by program base
#define R_X86_64_GOTPCREL   9     // 32 bit signed self relative offset to GOT
#define R_X86_64_32        10     // Direct 32 bit zero extended
#define R_X86_64_32S       11     // Direct 32 bit sign extended
#define R_X86_64_16        12     // Direct 16 bit zero extended
#define R_X86_64_PC16      13     // 16 bit sign extended self relative
#define R_X86_64_8         14     // Direct 8 bit sign extended
#define R_X86_64_PC8       15     // 8 bit sign extended self relative
#define R_X86_64_IRELATIVE 37     // Reference to PLT entry of indirect function (STT_GNU_IFUNC)


// ForwardCom relocation types are composed of these three fields:
// Relocation type in bit 16-31
// Relocation size in bit 8-15
// Scale factor in bit 0-7.
// The r_type field is composed by OR'ing these three.
// The value in the relocation field of the specified size will be multiplied by the scale factor.
// All relative relocations use signed values.
// Instructions with self-relative (IP-relative) addressing are using the END of the instruction 
// as reference point. The r_addend field must compensate for the distance between 
// the end of the instruction and the beginning of the address field. This will be -7 for 
// instructions with format 2.5.3 and -4 for all other jump and call instructions. 
// Any offset of the target may be added to r_addend. The value of r_addend is not scaled.
// Relocations relative to an arbitrary reference point can be used in jump tables.
// The reference point is indicated by a symbol index in r_refsym.
// The system function ID relocations are done by the loader, where r_sym indicates the name
// of the function in the string table, and r_addend indicates the name of the module or
// device driver.
// The value at r_offset is not used in the calculation but overwritten with the calculated 
// target address. 

// ForwardCom relocation types
#define R_FORW_ABS           0x000000       // Absolute address. Scaling is possible, but rarely used
#define R_FORW_SELFREL       0x010000       // Self relative. Scale by 4 for code address
#define R_FORW_IP_BASE       0x040000       // Relative to __ip_base. Any scale
#define R_FORW_DATAP         0x050000       // Relative to __datap_base. Any scale
#define R_FORW_THREADP       0x060000       // Relative to __threadp_base. Any scale
#define R_FORW_REFP          0x080000       // Relative to arbitrary reference point. Reference symbol index in high 32 bits of r_addend. Any scale
#define R_FORW_SYSFUNC       0x100000       // System function ID for system_call, 16 or 32 bit
#define R_FORW_SYSMODUL      0x110000       // System module ID for system_call, 16 or 32 bit
#define R_FORW_SYSCALL       0x120000       // Combined system module and function ID for system_call, 32 or 64 bit
#define R_FORW_DATASTACK     0x200000       // Calculated size of data stack for function, 32 or 64 bit. Scale by 1 or 8
#define R_FORW_CALLSTACK     0x210000       // Calculated size of call stack for function, 32 bit. Scale by 1 or 8
#define R_FORW_REGUSE        0x400000       // Register use of function, 64 bit
#define R_FORW_RELTYPEMASK   0xFF0000       // Mask for isolating relocation type

// Relocation sizes
#define R_FORW_NONE          0x000000       // No relocation
#define R_FORW_8             0x000100       // 8  bit relocation size
#define R_FORW_16            0x000200       // 16 bit relocation size
#define R_FORW_24            0x000300       // 24 bit relocation size
#define R_FORW_32            0x000400       // 32 bit relocation size
#define R_FORW_32LO          0x000500       // Low  16 of 32 bits relocation
#define R_FORW_32HI          0x000600       // High 16 of 32 bits relocation
#define R_FORW_64            0x000800       // 64 bit relocation size
#define R_FORW_64LO          0x000900       // Low  32 of 64 bits relocation
#define R_FORW_64HI          0x000A00       // High 32 of 64 bits relocation
#define R_FORW_RELSIZEMASK   0x00FF00       // Mask for isolating relocation size

// Relocation scale factors
#define R_FORW_SCALE1        0x000000       // Scale factor 1
#define R_FORW_SCALE2        0x000001       // Scale factor 2
#define R_FORW_SCALE4        0x000002       // Scale factor 4
#define R_FORW_SCALE8        0x000003       // Scale factor 8
#define R_FORW_SCALE16       0x000004       // Scale factor 16
#define R_FORW_RELSCALEMASK  0x0000FF       // Mask for isolating relocation scale factor

// Relocation options
#define R_FORW_RELINK      0x01000000       // Refers to relinkable symbol in executable file
#define R_FORW_LOADTIME    0x02000000       // Must be relocated at load time. Records with this bit must come first


//--------------------------------------------------------------------------
//                         PROGRAM HEADER
//--------------------------------------------------------------------------

// Program header
struct ElfFwcPhdr {
  uint32_t  p_type;      // Segment type
  uint32_t  p_flags;     // Segment flags
  uint64_t  p_offset;    // Segment file offset
  uint64_t  p_vaddr;     // Segment virtual address
  uint64_t  p_paddr;     // Segment physical address (not used. indicates first section instead)
  uint64_t  p_filesz;    // Segment size in file
  uint64_t  p_memsz;     // Segment size in memory
  uint8_t   p_align;     // Segment alignment
  uint8_t   unused[7];
};

// Legal values for p_type (segment type). 

#define PT_NULL             0    // Program header table entry unused
#define PT_LOAD             1    // Loadable program segment
#define PT_DYNAMIC          2    // Dynamic linking information
#define PT_INTERP           3    // Program interpreter
#define PT_NOTE             4    // Auxiliary information
#define PT_SHLIB            5    // Reserved
#define PT_PHDR             6    // Entry for header table itself
//#define PT_NUM              7    // Number of defined types
#define PT_LOOS    0x60000000    // Start of OS-specific
#define PT_HIOS    0x6fffffff    // End of OS-specific
#define PT_LOPROC        0x10    // Start of processor-specific
#define PT_HIPROC  0x5fffffff    // End of processor-specific

// Legal values for p_flags (segment flags) are the same as section flags, 
// see sh_flags above

/*
// Legal values for note segment descriptor types for core files.
#define NT_PRSTATUS    1    // Contains copy of prstatus struct
#define NT_FPREGSET    2    // Contains copy of fpregset struct
#define NT_PRPSINFO    3    // Contains copy of prpsinfo struct
#define NT_PRXREG      4    // Contains copy of prxregset struct
#define NT_PLATFORM    5    // String from sysinfo(SI_PLATFORM)
#define NT_AUXV        6    // Contains copy of auxv array
#define NT_GWINDOWS    7    // Contains copy of gwindows struct
#define NT_PSTATUS    10    // Contains copy of pstatus struct
#define NT_PSINFO     13    // Contains copy of psinfo struct
#define NT_PRCRED     14    // Contains copy of prcred struct
#define NT_UTSNAME    15    // Contains copy of utsname struct
#define NT_LWPSTATUS  16    // Contains copy of lwpstatus struct
#define NT_LWPSINFO   17    // Contains copy of lwpinfo struct
#define NT_PRFPXREG   20    // Contains copy of fprxregset struct
*/
// Legal values for the note segment descriptor types for object files.
#define NT_VERSION  1       // Contains a version string.


// Note section contents.  Each entry in the note section begins with a header of a fixed form.

struct Elf64_Nhdr {
  uint32_t n_namesz;      // Length of the note's name
  uint32_t n_descsz;      // Length of the note's descriptor
  uint32_t n_type;        // Type of the note
};

/* Defined note types for GNU systems.  */

/* ABI information.  The descriptor consists of words:
   word 0: OS descriptor
   word 1: major version of the ABI
   word 2: minor version of the ABI
   word 3: subminor version of the ABI
*/
#define ELF_NOTE_ABI    1

/* Known OSes.  These value can appear in word 0 of an ELF_NOTE_ABI
   note section entry.  */
#define ELF_NOTE_OS_LINUX         0
#define ELF_NOTE_OS_GNU           1
#define ELF_NOTE_OS_SOLARIS2      2

#define FILE_DATA_ALIGN           3  // section data must be aligned by (1 << FILE_DATA_ALIGN) in ELF file

// Memory map definitions
#define MEMORY_MAP_ALIGN          3  // align memory map entries by (1 << MEMORY_MAP_ALIGN)
#define DATA_EXTRA_SPACE       0x10  // extra space after const data section and last data section


//--------------------------------------------------------------------------
//                         EVENT HANDLER SYSTEM
//--------------------------------------------------------------------------
/*                      

A program module may contain a table of event handler records in a read-only
section with the attribute SHF_EVENT_HND. The event handler system may be used
for handling events, commands, and messages. It is also used for initialization
and clean-up. This replaces the constructors and destructors sections of other
systems.

The linker will sort the event records of all modules according to event id, key,
and priority. If there is more than one event handler for a particular event,
then all the event handlers will be called in the order of priority.
*/

// event record
struct ElfFwcEvent {
    int32_t  functionPtr;              // scaled relative pointer to event handler function = (function_address - __ip_base) / 4
    uint32_t priority;                 // priority. Highest values are called first. Normal priority = 0x1000
    uint32_t key;                      // keyboard hotkey, menu item, or icon id for user command events
    uint32_t event;                    // event ID
};


//--------------------------------------------------------------------------
//                         STACK SIZE TABLES
//--------------------------------------------------------------------------

// SHT_STACKSIZE stack table entry
struct ElfFwcStacksize {
  uint32_t  ss_syma;                   // Public symbol index
  uint32_t  ss_symb;                   // External symbol index. Zero for frame function or to indicate own stack use
  uint64_t  ss_framesize;              // Size of data stack frame in syma when calling symb
  uint32_t  ss_numvectors;             // Additional data stack frame size for vectors. Multiply by maximum vector length
  uint32_t  ss_calls;                  // Size of call stack when syma calls symb (typically 1). Multiply by stack word size = 8
};


//--------------------------------------------------------------------------
//                         MASK BITS
//--------------------------------------------------------------------------
// Masks are used for conditional execution and for setting options

// Mask bit numbers. These bits are used in instruction masks and NUMCONTR to specify various options

#define MSK_ENABLE                    0     // the instruction is not executed if bit number 0 is 0
#define MSKI_OPTIONS                 18     // bit number 18-23 contain instruction-specific options. currently unused
#define MSKI_ROUNDING                10     // bit number 10-11 indicate rounding mode:
                                            // 00: round to nearest or even
                                            // 01: round down
                                            // 10: round up
                                            // 11: truncate towards zero
#define MSKI_EXCEPTIONS               2     // bit number 2-5 enable exceptions for division by zero, overflow, underflow, inexact
#define MSK_DIVZERO                   2     // enable NAN exception for floating point division by zero
#define MSK_OVERFLOW                  3     // enable NAN exception for floating point overflow
#define MSK_UNDERFLOW                 4     // enable NAN exception for floating point underflow
#define MSK_INEXACT                   5     // enable NAN exception for floating point inexact
#define MSK_SUBNORMAL                13     // enable subnormal numbers for float32 and float64
#define MSK_CONST_TIME               31     // constant execution time, independent of data (for cryptographic security)


//--------------------------------------------------------------------------
//                         EXCEPTION INDICATORS (preliminary list)
//--------------------------------------------------------------------------

// NAN payloads are used for indicating that floating point exceptions have occurred.
// These values are generated in the lower 8 bits of NAN payloads.
// The remaining payload bits may contain information about the code address where the exception occurred.

// The nan exception indicators are generated only when the corresponding exceptions are enabled in mask bits:
const uint32_t nan_inexact           = 0x01;  // inexact result
const uint32_t nan_underflow         = 0x02;  // underflow
const uint32_t nan_div0              = 0x03;  // division by 0
const uint32_t nan_overflow_div      = 0x04;  // division overflow
const uint32_t nan_overflow_mul      = 0x05;  // multiplication overflow
const uint32_t nan_overflow_add      = 0x06;  // addition and subtraction overflow
const uint32_t nan_overflow_conv     = 0x07;  // conversion overflow
const uint32_t nan_overflow_other    = 0x08;  // other overflow

// The nan_invalid indicators are generated in case of invalid operations,
// regardless of whether exceptions are enabled or not:
const uint32_t nan_invalid_sub       = 0x20;  // inf-inf
const uint32_t nan_invalid_0div0     = 0x21;  // 0/0
const uint32_t nan_invalid_divinf    = 0x22;  // inf/inf
const uint32_t nan_invalid_0mulinf   = 0x23;  // 0*inf
const uint32_t nan_invalid_rem       = 0x24;  // inf rem 1, 1 rem 0
const uint32_t nan_invalid_sqrt      = 0x25;  // sqrt(-1)
const uint32_t nan_invalid_pow       = 0x28;  // pow(-1, 2.3)
const uint32_t nan_invalid_log       = 0x29;  // log(-1)


//--------------------------------------------------------------------------
//                         FORMAT FOR LIBRARY FILES
//--------------------------------------------------------------------------
/*
ForwardCom libraries use the standard Unix archive format.
The preferred filename extension is .li

The first archive member is a sorted symbol list, using the same format as used
by Apple/Mac named "/SYMDEF SORTED/". It contains a sorted list of public symbols. 
The sort order is determined by the unsigned bytes of the ASCII/UTF-8 string. 
This format is chosen because it provides the fastest symbol search.

The obsolete archive members with the name "/" containing symbol lists in less
efficient formats are not included.

The second archive member is a longnames record named "//" as used in Linux 
and Windows systems. It contains module names longer than 15 characters. 
Module names are stored without path so that they can be extracted on another 
computer that does not have the same file structure.

The remaining modules contain object files in the format described above.
--------------------------------------------------------------------------------*/

// Signature defining the start of an archive file
#define archiveSignature  "!<arch>\n"

// Each library member starts with a UNIX archive member header:
struct SUNIXLibraryHeader {
    char name[16];                      // member name, terminated by '/'
    char date[12];                      // member date, seconds, decimal ASCII
    char userID[6];                     // member User ID, decimal ASCII
    char groupID[6];                    // member Group ID, decimal ASCII
    char fileMode[8];                   // member file mode, octal ASCII
    char fileSize[10];                  // member file size not including header, decimal ASCII
    char headerEnd[2];                  // "`\n"
};

// Member names no longer than 15 characters are stored in the name field and 
// terminated by '/'. Longer names are stored in the longnames record. The name
// field contains '/' followed by an index into the longnames string table. 
// This index is in decimal ASCII.

// The "/SYMDEF SORTED/" record contains the following:
// 1. The size of the symbol list = 8 * n, where n = number of exported symbols
//    in the library.
// 2. For each symbol: the name as an index into the string table (relative to 
//    the start of the sting table), followed by:
//    an offset to the module containing this symbol relative to file begin.
// 3. The length of the string table.
// 4. The string table as a sequence of zero-terminated strings.
// 5. Zero-padding to a size divisible by 4.

// All numbers in "/SYMDEF SORTED/" are 32-bit unsigned integers (little endian).

// The longnames record has the name "//". It contains member names as zero-terminated strings.

// All archive members are aligned by 8

#endif // ELF_FORW_H

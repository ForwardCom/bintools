/****************************    elf.cpp    *********************************
* Author:        Agner Fog
* Date created:  2006-07-18
* Last modified: 2017-11-03
* Version:       1.00
* Project:       Binary tools for ForwardCom instruction set
* Module:        elf.cpp
* Description:
* Module for manipulating ForwardCom ELF files
*
* Class CELF is used for manipulating ELF files.
* It includes functions for reading, interpreting, dumping files,
* splitting into containers or joining containers into an ELF file
*
* This code is based on the ForwardCom ELF specification in elf_forwardcom.h.
* I have included limited support for x86-64 ELF (e_machine == EM_X86_64) for 
* testing purposes. This may be removed.
*
* Copyright 2006-2017 GNU General Public License http://www.gnu.org/licenses
*****************************************************************************/
#include "stdafx.h"

// File class names
SIntTxt ELFFileClassNames[] = {
    {ELFCLASSNONE,      "None"},
    {ELFCLASS32,        "32-bit object"},
    {ELFCLASS64,        "64-bit object"}
};

// Data encoding names
SIntTxt ELFDataEncodeNames[] = {
    {ELFDATANONE,        "None"},
    {ELFDATA2LSB,        "Little Endian"},
    {ELFDATA2MSB,        "Big Endian"}
};

// ABI names
SIntTxt ELFABINames[] = {
    {ELFOSABI_SYSV,      "System V"},
    {ELFOSABI_HPUX,      "HP-UX"},
    {ELFOSABI_ARM,       "ARM"},
    {ELFOSABI_STANDALONE,"Embedded"},
    {ELFOSABI_FORWARDCOM,"ForwardCom"}
};

// File type names
SIntTxt ELFFileTypeNames[] = {
    {ET_NONE,   "None"},
    {ET_REL,    "Relocatable"},
    {ET_EXEC,   "Executable"},
    {ET_DYN,    "Shared object"},
    {ET_CORE,   "Core file"}
};

// Section type names
SIntTxt ELFSectionTypeNames[] = {
    {SHT_NULL,          "None"},
    {SHT_PROGBITS,      "Program data"},
    {SHT_SYMTAB,        "Symbol table"},
    {SHT_STRTAB,        "String table"},
    {SHT_RELA,          "Relocation w addends"},
    {SHT_HASH,          "Symbol hash table"},
    {SHT_DYNAMIC,       "Dynamic linking info"},
    {SHT_NOTE,          "Notes"},
    {SHT_NOBITS,        "uinitialized"},
    {SHT_REL,           "Relocation entries"},
    {SHT_SHLIB,         "Reserved"},
    {SHT_DYNSYM,        "Dynamic linker symbol table"},
    {SHT_COMDAT,        "Communal section"},    
    {SHT_INIT_ARRAY,    "Array of constructors"},
    {SHT_FINI_ARRAY,    "Array of destructors"},
    {SHT_PREINIT_ARRAY, "Array of pre-constructors"},
    {SHT_GROUP,         "Section group"},
    {SHT_SYMTAB_SHNDX,  "Extended section indices"}
};

// Program header type names
SIntTxt ELFPTypeNames[] = {
     {PT_NULL,        "Unused"},
     {PT_LOAD,        "Loadable program segment"},
     {PT_DYNAMIC,     "Dynamic linking information"},
     {PT_INTERP,      "Program interpreter"},
     {PT_NOTE,        "Auxiliary information"},
     {PT_SHLIB,       "Reserved"},
     {PT_PHDR,        "Entry for header table itself"}
};

// Section flag names
SIntTxt ELFSectionFlagNames[] = {
    {SHF_EXEC,          "Executable"},
    {SHF_READ,          "Readable"},
    {SHF_WRITE,         "Writeable"},
    {SHF_ALLOC,         "Allocate"},
    {SHF_IP,            "IP address"},
    {SHF_DATAP,         "DATAP address"},
    {SHF_THREADP,       "THREADP address"},
    {SHF_MERGE,         "Merge"},
    {SHF_STRINGS,       "Strings"},
    {SHF_INFO_LINK,     "sh_info"}
};

// Symbol binding names
SIntTxt ELFSymbolBindingNames[] = {
    {STB_LOCAL,  "Local"},
    {STB_GLOBAL, "Global"},
    {STB_WEAK,   "Weak"}
};

// Symbol Type names
SIntTxt ELFSymbolTypeNames[] = {
    {STT_NOTYPE,  "None"},
    {STT_OBJECT,  "Object"},
    {STT_FUNC,    "Function"},
    {STT_SECTION, "Section"},
    {STT_FILE,    "File"}
};

// Symbol st_other info names
SIntTxt ELFSymbolInfoNames[] = {
    {STV_EXEC,     "executable"},
    {STV_READ,     "read"},
    {STV_WRITE,    "write"},
    {STV_IP,       "ip"},
    {STV_DATAP,    "datap"},
    {STV_THREADP,  "threadp"},
    {STV_REGUSE,   "reguse"},
    {STV_FLOAT,    "float"},
    {STV_STRING,   "string"}, 
    {STV_CTOR,     "constructor"},
    {STV_DTOR,     "destructor"},
    {STV_UNWIND,   "unwind"},
    {STV_DEBUG,    "debug"},
    {STV_COMMON,   "communal"},
    {STV_RELINK,   "relinkable"},
    {STV_MAIN,     "main"},
    {STV_EXPORTED, "exported"},
    {STV_THREAD,   "thread"}
};

// Relocation type names x86 64 bit
SIntTxt ELF64RelocationNames[] = {
    {R_X86_64_NONE,      "None"},
    {R_X86_64_64,        "Direct 64 bit"},
    {R_X86_64_PC32,      "Self relative 32 bit signed"},
    {R_X86_64_GOT32,     "32 bit GOT entry"},
    {R_X86_64_PLT32,     "32 bit PLT address"},
    {R_X86_64_COPY,      "Copy symbol at runtime"},
    {R_X86_64_GLOB_DAT,  "Create GOT entry"},
    {R_X86_64_JUMP_SLOT, "Create PLT entry"},
    {R_X86_64_RELATIVE,  "Adjust by program base"},
    {R_X86_64_GOTPCREL,  "32 bit signed pc relative offset to GOT"},
    {R_X86_64_32,        "Direct 32 bit zero extended"},
    {R_X86_64_32S,       "Direct 32 bit sign extended"},
    {R_X86_64_16,        "Direct 16 bit zero extended"},
    {R_X86_64_PC16,      "16 bit sign extended pc relative"},
    {R_X86_64_8,         "Direct 8 bit sign extended"},
    {R_X86_64_PC8,       "8 bit sign extended pc relative"},
    {R_X86_64_IRELATIVE, "32 bit ref. to indirect function PLT"}
};

// Relocation type names for ForwardCom
SIntTxt ELFFwcRelocationTypes[] = {
    {R_FORW_ABS,        "Absolute address"},
    {R_FORW_SELFREL,    "Self relative"},
    {R_FORW_CONST,      "Relative to CONST section"},
    {R_FORW_DATAP,      "Relative to data pointer"},
    {R_FORW_THREADP,    "Relative to thread data pointer"},
    {R_FORW_REFP,       "Relative to arbitrary reference point"},
    {R_FORW_SYSFUNC,    "System function ID"},
    {R_FORW_SYSMODUL,   "System module ID"},
    {R_FORW_SYSCALL,    "System module and function ID"},
    {R_FORW_DATASTACK,  "Size of data stack"},
    {R_FORW_CALLSTACK,  "Size of call stack"},
    {R_FORW_REGUSE,     "Register use"}
};

// Relocation sizes for ForwardCom
SIntTxt ELFFwcRelocationSizes[] = {
    {R_FORW_NONE,       "None"},
    {R_FORW_8,          "8 bit"},
    {R_FORW_16,         "16 bit"},
    {R_FORW_24,         "24 bit"},
    {R_FORW_32,         "32 bit"},
    {R_FORW_64,         "64 bit"},
    {R_FORW_32LO,       "Low 16 of 32 bits"},
    {R_FORW_32HI,       "High 16 of 32 bits"},
    {R_FORW_64LO,       "Low 32 of 64 bits"},
    {R_FORW_64HI,       "High 32 of 64 bits"}
};

// Machine names
SIntTxt ELFMachineNames[] = {
    {EM_NONE,         "None"},     // No machine
    {EM_FORWARDCOM,   "ForwardCom"},
    {EM_M32,          "AT&T WE 32100"},
    {EM_SPARC,        "SPARC"},
    {EM_386,          "Intel x86"},
    {EM_68K,          "Motorola m68k"},
    {EM_88K,          "Motorola m88k"},
    {EM_860,          "MIPS R3000 big-endian"},
    {EM_MIPS,         "MIPS R3000 big-endian"},
    {EM_S370,         "IBM System/370"},
    {EM_MIPS_RS3_LE,  "NMIPS R3000 little-endianone"},
    {EM_PARISC,       "HPPA"},
    {EM_VPP500,       "Fujitsu VPP500"},
    {EM_SPARC32PLUS,  "Sun v8plus"},
    {EM_960,          "Intel 80960"},
    {EM_PPC,          "PowerPC"},
    {EM_PPC64,        "PowerPC 64-bit"},
    {EM_S390,         "IBM S390"},
    {EM_V800,         "NEC V800"},
    {EM_FR20,         "Fujitsu FR20"},
    {EM_RH32,         "TRW RH-32"},
    {EM_RCE,          "Motorola RCE"},
    {EM_ARM,          "ARM"},
    {EM_FAKE_ALPHA,   "Digital Alpha"},
    {EM_SH,           "Hitachi SH"},
    {EM_SPARCV9,      "SPARC v9 64-bit"},
    {EM_TRICORE,      "Siemens Tricore"},
    {EM_ARC,          "Argonaut RISC"},
    {EM_H8_300,       "Hitachi H8/300"},
    {EM_H8_300H,      "Hitachi H8/300H"},
    {EM_H8S,          "Hitachi H8S"},
    {EM_H8_500,       "EM_H8_500"},
    {EM_IA_64,        "Intel IA64"},
    {EM_MIPS_X,       "Stanford MIPS-X"},
    {EM_COLDFIRE,     "Motorola Coldfire"},
    {EM_68HC12,       "Motorola M68HC12"},
    {EM_MMA,          "Fujitsu MMA"},
    {EM_PCP,          "Siemens PCP"},
    {EM_NCPU,         "Sony nCPU"},
    {EM_NDR1,         "Denso NDR1"},
    {EM_STARCORE,     "Motorola Start*Core"},
    {EM_ME16,         "Toyota ME16"},
    {EM_ST100,        "ST100"},
    {EM_TINYJ,        "Tinyj"},
    {EM_X86_64,       "x86-64"},
    {EM_PDSP,         "Sony DSP"},
    {EM_FX66,         "Siemens FX66"},
    {EM_ST9PLUS,      "ST9+ 8/16"},
    {EM_ST7,          "ST7 8"},
    {EM_68HC16,       "MC68HC16"},
    {EM_68HC11,       "MC68HC11"},
    {EM_68HC08,       "MC68HC08"},
    {EM_68HC05,       "MC68HC05"},
    {EM_SVX,          "SVx"},
    {EM_AT19,         "ST19"},
    {EM_VAX,          "VAX"},
    {EM_CRIS,         "Axis"},
    {EM_JAVELIN,      "Infineon"},
    {EM_FIREPATH,     "Element 14"},
    {EM_ZSP,          "LSI Logic"},
    {EM_HUANY,        "Harvard"},
    {EM_PRISM,        "SiTera Prism"},
    {EM_AVR,          "Atmel AVR"},
    {EM_FR30,         "FR30"},
    {EM_D10V,         "D10V"},
    {EM_D30V,         "D30V"},
    {EM_V850,         "NEC v850"},
    {EM_M32R,         "M32R"},
    {EM_MN10300,      "MN10300"},
    {EM_MN10200,      "MN10200"},
    {EM_PJ,           "picoJava"},
    {EM_ALPHA,        "Alpha"}
};


// Class CELF members:
// Constructor
CELF::CELF() {
    memset(this, 0, sizeof(*this));
}

// ParseFile
void CELF::parseFile() {
    // Load and parse file buffer
    uint32_t i;
    fileHeader = *(Elf64_Ehdr*)buf();             // Copy file header
    nSections = fileHeader.e_shnum;
    sectionHeaders.setNum(nSections);            // Allocate space for section headers
    uint32_t symtabi = 0;                         // Index to symbol table

    // Find section headers
    sectionHeaderSize = fileHeader.e_shentsize;
    if (sectionHeaderSize <= 0) err.submit(ERR_ELF_RECORD_SIZE);
    uint32_t SectionOffset = uint32_t(fileHeader.e_shoff);
    // check header integrity
    if (fileHeader.e_phoff >= dataSize() || fileHeader.e_phoff + (uint32_t)fileHeader.e_phentsize * fileHeader.e_phnum > dataSize()) err.submit(ERR_ELF_INDEX_RANGE);
    if (fileHeader.e_shoff >= dataSize() || fileHeader.e_shoff + (uint32_t)fileHeader.e_shentsize * fileHeader.e_shnum > dataSize()) err.submit(ERR_ELF_INDEX_RANGE);
    if (fileHeader.e_shstrndx >= dataSize()) err.submit(ERR_ELF_INDEX_RANGE);

    for (i = 0; i < nSections; i++) {
        sectionHeaders[i] = get<Elf64_Shdr>(SectionOffset);
        // check section header integrity
        if (sectionHeaders[i].sh_offset > dataSize() 
            || (sectionHeaders[i].sh_offset + sectionHeaders[i].sh_size > dataSize() && sectionHeaders[i].sh_type != SHT_NOBITS)
            || sectionHeaders[i].sh_offset + sectionHeaders[i].sh_entsize > dataSize()) {
            err.submit(ERR_ELF_INDEX_RANGE);
        }
        SectionOffset += sectionHeaderSize;
        if (sectionHeaders[i].sh_type == SHT_SYMTAB) {
            // Symbol table found
            symtabi = i;
        }
    }
    if (SectionOffset > dataSize()) err.submit(ERR_ELF_INDEX_RANGE);     // Section table points to outside file

    if (buf() && dataSize()) {  // string table
        uint64_t offset = sectionHeaders[fileHeader.e_shstrndx].sh_offset;
        secStringTable = (char*)buf() + uint32_t(offset);
        secStringTableLen = uint32_t(sectionHeaders[fileHeader.e_shstrndx].sh_size);
        if (offset > dataSize() || offset + secStringTableLen > dataSize()) err.submit(ERR_ELF_INDEX_RANGE);
    }
    // check section names
    for (i = 0; i < nSections; i++) {
        if (sectionHeaders[i].sh_name >= secStringTableLen) { err.submit(ERR_ELF_STRING_TABLE); break; }
    }

    if (symtabi) {
        // Save offset to symbol table
        uint64_t offset = sectionHeaders[symtabi].sh_offset;
        symbolTableOffset = (uint32_t)offset;
        symbolTableEntrySize = (uint32_t)(sectionHeaders[symtabi].sh_entsize); // Entry size of symbol table
        if (symbolTableEntrySize == 0) { err.submit(ERR_ELF_SYMTAB_MISSING); return; } // Avoid division by zero
        symbolTableEntries = uint32_t(sectionHeaders[symtabi].sh_size) / symbolTableEntrySize;
        if (offset > dataSize() || offset > 0xFFFFFFFFU || offset + sectionHeaders[symtabi].sh_entsize > dataSize()
            || offset + sectionHeaders[symtabi].sh_size > dataSize()) err.submit(ERR_ELF_INDEX_RANGE);

        // Find associated string table
        uint32_t stringtabi = sectionHeaders[symtabi].sh_link;
        if (stringtabi >= nSections) {
            err.submit(ERR_ELF_INDEX_RANGE);
            return;
        }
        offset = sectionHeaders[stringtabi].sh_offset;
        symbolStringTableOffset = (uint32_t)offset;
        symbolStringTableSize = (uint32_t)(sectionHeaders[stringtabi].sh_size);
        if (offset > dataSize() || offset > 0xFFFFFFFFU || offset + sectionHeaders[stringtabi].sh_size > dataSize()) err.submit(ERR_ELF_INDEX_RANGE);
        // check all symbol names
        int8_t * symtab = buf() + symbolTableOffset;
        uint32_t symname = 0;
        for (uint32_t symi = 0; symi < symbolTableEntries; symi++) {
            // Copy ElfFWC_Sym symbol or convert Elf64_Sym symbol
            if (fileHeader.e_machine == EM_FORWARDCOM) {
                symname = ((ElfFWC_Sym*)symtab)[symi].st_name;
            }
            else {
                // x64 type symbol table entry
                symname = ((Elf64_Sym*)symtab)[symi].st_name;
            }
            if (symname >= symbolStringTableSize) err.submit(ERR_ELF_STRING_TABLE);
        } 
    }
}


// Dump
void CELF::dump(int options) {
    if (options & DUMP_FILEHDR) {
        // File header
        printf("\nDump of ELF file %s", fileName);
        printf("\n-----------------------------------------------");
        printf("\nFile size: %i", dataSize());
        printf("\nFile header:");
        printf("\nFile class: %s, Data encoding: %s, ELF version %i, ABI: %s, ABI version %i",
            Lookup(ELFFileClassNames, fileHeader.e_ident[EI_CLASS]),
            Lookup(ELFDataEncodeNames, fileHeader.e_ident[EI_DATA]),
            fileHeader.e_ident[EI_VERSION],
            Lookup(ELFABINames, fileHeader.e_ident[EI_OSABI]),
            fileHeader.e_ident[EI_ABIVERSION]);

        printf("\nFile type: %s, Machine: %s, version: %i",
            Lookup(ELFFileTypeNames, fileHeader.e_type),
            Lookup(ELFMachineNames, fileHeader.e_machine),
            fileHeader.e_version);
        printf("\nNumber of sections: %2i, Processor flags: 0x%X",
            nSections, fileHeader.e_flags);
    }

    if ((options & DUMP_SECTHDR) && fileHeader.e_phnum) {
        // Dump program headers
        uint32_t nProgramHeaders = fileHeader.e_phnum;
        uint32_t programHeaderSize = fileHeader.e_phentsize;
        if (nProgramHeaders && programHeaderSize <= 0) err.submit(ERR_ELF_RECORD_SIZE);
        uint32_t programHeaderOffset = (uint32_t)fileHeader.e_phoff;
        Elf64_Phdr pHeader;
        for (uint32_t i = 0; i < nProgramHeaders; i++) {
            pHeader = get<Elf64_Phdr>(programHeaderOffset);
            printf("\nProgram header Type: %s, flags 0x%X",
                Lookup(ELFPTypeNames, (uint32_t)pHeader.p_type), (uint32_t)pHeader.p_flags);
            printf("\noffset = 0x%X, vaddr = 0x%X, paddr = 0x%X, filesize = 0x%X, memsize = 0x%X, align = 0x%X",
                (uint32_t)pHeader.p_offset, (uint32_t)pHeader.p_vaddr, (uint32_t)pHeader.p_paddr, (uint32_t)pHeader.p_filesz, (uint32_t)pHeader.p_memsz, (uint32_t)pHeader.p_align);
            programHeaderOffset += programHeaderSize;
            if (pHeader.p_filesz < 0x100 && (uint32_t)pHeader.p_offset < dataSize() && memchr(buf() + pHeader.p_offset, 0, (uint32_t)pHeader.p_filesz)) {
                printf("\nContents: %s", buf() + (int)pHeader.p_offset);
            }
        }
    }

    if (options & DUMP_SECTHDR) {
        // Dump section headers
        printf("\n\nSection headers:");
        for (uint32_t sc = 0; sc < nSections; sc++) {
            // Get copy of 32-bit header or converted 64-bit header
            Elf64_Shdr sheader = sectionHeaders[sc];
            uint32_t entrysize = (uint32_t)(sheader.sh_entsize);
            uint32_t namei = sheader.sh_name;
            if (namei >= secStringTableLen) { err.submit(ERR_ELF_STRING_TABLE); break; }
            printf("\n%2i Name: %-18s Type: %s", sc, secStringTable + namei,
                Lookup(ELFSectionTypeNames, sheader.sh_type));
            if (sheader.sh_flags) {
                printf("\n  Flags: 0x%X:", uint32_t(sheader.sh_flags));
                for (int fi = 1; fi < (1 << 30); fi <<= 1) {
                    if (uint32_t(sheader.sh_flags) & fi) {
                        printf(" %s", Lookup(ELFSectionFlagNames, fi));
                    }
                }
            }
            if (sheader.sh_addr) {
                printf("\n  Address: 0x%X", uint32_t(sheader.sh_addr));
            }
            if (sheader.sh_offset || sheader.sh_size) {
                printf("\n  FileOffset: 0x%X, Size: 0x%X",
                    uint32_t(sheader.sh_offset), uint32_t(sheader.sh_size));
            }
            if (sheader.sh_addralign) {
                printf("\n  Alignment: 0x%X", uint32_t(sheader.sh_addralign));
            }
            if (sheader.sh_entsize) {
                printf("\n  Entry size: 0x%X", uint32_t(sheader.sh_entsize));
                switch (sheader.sh_type) {
                case SHT_DYNAMIC:
                    printf("\n  String table: %i", sheader.sh_link);
                    break;
                case SHT_HASH:
                    printf("\n  Symbol table: %i", sheader.sh_link);
                    break;
                case SHT_REL: case SHT_RELA:
                    printf("\n  Symbol table: %i, Reloc. section: %i",
                        sheader.sh_link, sheader.sh_info);
                    break;
                case SHT_SYMTAB: case SHT_DYNSYM:
                    printf("\n  Symbol string table: %i, First global symbol: %i",
                        sheader.sh_link, sheader.sh_info);
                    break;
                default:
                    if (sheader.sh_link) {
                        printf("\n  Link: %i", sheader.sh_link);
                    }
                    if (sheader.sh_info) {
                        printf("\n  Info: %i", sheader.sh_info);
                    }
                }
            }
            if (sheader.sh_type == SHT_STRTAB && (options & DUMP_STRINGTB)) {
                // Print string table
                printf("\n  String table:");
                char * p = (char*)buf() + uint32_t(sheader.sh_offset) + 1;
                uint32_t nread = 1, len;
                while (nread < uint32_t(sheader.sh_size)) {
                    len = (uint32_t)strlen(p);
                    printf(" >>%s<<", p);
                    nread += len + 1;
                    p += len + 1;
                }
            }
            if ((sheader.sh_type == SHT_SYMTAB || sheader.sh_type == SHT_DYNSYM) && (options & DUMP_SYMTAB)) {
                // Dump symbol table

                // Find associated string table
                if (sheader.sh_link >= (uint32_t)nSections) { err.submit(ERR_ELF_INDEX_RANGE); sheader.sh_link = 0; }
                uint64_t strtabOffset = sectionHeaders[sheader.sh_link].sh_offset;
                if (strtabOffset >= dataSize()) {
                    err.submit(ERR_ELF_INDEX_RANGE); return;
                }
                int8_t * strtab = buf() + strtabOffset;

                // Find symbol table
                uint32_t symtabsize = (uint32_t)(sheader.sh_size);
                int8_t * symtab = buf() + uint32_t(sheader.sh_offset);
                int8_t * symtabend = symtab + symtabsize;
                if (entrysize < sizeof(Elf64_Sym)) { err.submit(ERR_ELF_RECORD_SIZE); entrysize = sizeof(Elf64_Sym); }

                printf("\n  Symbols:");
                // Loop through symbol table
                int symi;  // Symbol number
                for (symi = 0; symtab < symtabend; symtab += entrysize, symi++) {
                    // Copy ElfFWC_Sym symbol or convert Elf64_Sym symbol
                    ElfFWC_Sym sym;
                    if (fileHeader.e_machine == EM_FORWARDCOM) {
                        sym = *(ElfFWC_Sym*)symtab;
                    }
                    else {
                        // Translate x64 type symbol table entry
                        Elf64_Sym sym64 = *(Elf64_Sym*)symtab;
                        sym.st_name = sym64.st_name;
                        sym.st_type = sym64.st_type;
                        sym.st_bind = sym64.st_bind;
                        sym.st_other = sym64.st_other;
                        sym.st_shndx = sym64.st_shndx;
                        sym.st_value = sym64.st_value;
                        sym.st_unitsize = (uint32_t)sym64.st_size;
                        sym.st_unitnum = 1;
                        sym.st_reguse1 = sym.st_reguse2 = 0;
                    }
                    int type = sym.st_type;
                    int binding = sym.st_bind;
                    if (strtabOffset + sym.st_name >= dataSize()) err.submit(ERR_ELF_INDEX_RANGE);
                    else if (*(strtab + sym.st_name)) {
                        printf("\n  %2i Name: %s,", symi, strtab + sym.st_name);
                    }
                    else {
                        printf("\n  %2i Unnamed,", symi);
                    }
                    if (sym.st_value || type == STT_OBJECT || type == STT_FUNC || int16_t(sym.st_shndx) < 0)
                        printf(" Value: 0x%X", uint32_t(sym.st_value));
                    if (sym.st_unitsize)  printf(" size: %X*%X", sym.st_unitsize, sym.st_unitnum);
                    if (sym.st_other) {
                        for (int i = 0; i < 32; i++) {
                            if (sym.st_other & 1 << i) {
                                printf(" %s", Lookup(ELFSymbolInfoNames, 1 << i));
                            }
                        }
                    }
                    if (int16_t(sym.st_shndx) > 0) printf(", section: %i", sym.st_shndx);
                    else { // Special segment values
                        switch (sym.st_shndx) {
                        case 0: 
                        case SHN_ABS_X86:  // not used in ForwardCom
                            printf(", absolute,"); break;
                        case SHN_COMMON:
                            printf(", common,"); break;
                        default:
                            printf(", section: 0x%X", sym.st_shndx);
                        }
                    }
                    if (sym.st_type || sym.st_bind) {
                        printf(" type: %s, binding: %s",
                            Lookup(ELFSymbolTypeNames, type),
                            Lookup(ELFSymbolBindingNames, binding));
                    }
                }
            }
            // Dump relocation table
            if ((sheader.sh_type == SHT_REL || sheader.sh_type == SHT_RELA) && (options & DUMP_RELTAB)) {
                printf("\n  Relocations:");
                int8_t * reltab = buf() + uint32_t(sheader.sh_offset);
                int8_t * reltabend = reltab + uint32_t(sheader.sh_size);
                uint32_t expectedentrysize = sheader.sh_type == SHT_RELA ?
                    sizeof(Elf64_Rela) :                // Elf32_Rela, Elf64_Rela
                    sizeof(Elf64_Rela) - wordSize / 8;  // Elf32_Rel,  Elf64_Rel
                if (entrysize < expectedentrysize) { err.submit(ERR_ELF_RECORD_SIZE); entrysize = expectedentrysize; }

                // Loop through entries
                for (; reltab < reltabend; reltab += entrysize) {
                    // Copy relocation table entry with or without addend
                    ElfFWC_Rela rel;
                    if (sheader.sh_type == SHT_REL) {
                        memcpy(&rel, reltab, 16);     // Obsolete Elf64_Rel
                        rel.r_addend = 0;
                        rel.r_refsym = 0;
                    }
                    else {
                        rel = *(ElfFWC_Rela*)reltab;
                    }
                    const char * relocationName, *relocationSize;
                    char text[128];
                    if (machineType == EM_X86_64) {
                        relocationName = Lookup(ELF64RelocationNames, rel.r_type);
                    }
                    else if (machineType == EM_FORWARDCOM) {
                        relocationName = Lookup(ELFFwcRelocationTypes, rel.r_type & R_FORW_RELTYPEMASK);
                        relocationSize = Lookup(ELFFwcRelocationSizes, rel.r_type & R_FORW_RELSIZEMASK);
                        sprintf(text, "%s, %s, scale by %i", relocationName, relocationSize,
                            1 << (rel.r_type & 0xFF));
                        relocationName = text;
                    }
                    else {
                        relocationName = "unknown";
                    }

                    memcpy(&rel, reltab, entrysize);
                    printf("\n  Offset: 0x%X, Symbol: %i, Name: %s\n   Type: %s",
                        uint32_t(rel.r_offset), rel.r_sym, symbolName(rel.r_sym), relocationName);
                    if (machineType == EM_FORWARDCOM && rel.r_type >> 16 == 8) {
                        printf(", ref. point %i", rel.r_refsym);
                    }
                    if (uint32_t(rel.r_addend)) {
                        printf(", Addend: 0x%X", uint32_t(rel.r_addend));
                    }

                    // Find inline addend
                    Elf64_Shdr relsheader = sectionHeaders[sheader.sh_info];
                    uint32_t relsoffset = uint32_t(relsheader.sh_offset);
                    if (relsoffset + rel.r_offset < dataSize() && relsheader.sh_type != SHT_NOBITS) {
                        int32_t * piaddend = (int32_t*)(buf() + relsoffset + rel.r_offset);
                        if (*piaddend) printf(", Inline value: 0x%X", *piaddend);
                    }
                }
            }
        }
    }
}


// PublicNames
void CELF::publicNames(CMemoryBuffer * strings, CDynamicArray<SStringEntry> * index, int m) {
    // Make list of public names
    // Interpret header:
    parseFile();

    // Loop through section headers
    for (uint32_t sc = 0; sc < nSections; sc++) {
        // Get copy of 32-bit header or converted 64-bit header
        Elf64_Shdr sheader = sectionHeaders[sc];
        uint32_t entrysize = uint32_t(sheader.sh_entsize);

        if (sheader.sh_type == SHT_SYMTAB || sheader.sh_type == SHT_DYNSYM) {
            // Dump symbol table

            // Find associated string table
            if (sheader.sh_link >= (uint32_t)nSections) { err.submit(ERR_ELF_INDEX_RANGE); sheader.sh_link = 0; }
            int8_t * strtab = buf() + uint32_t(sectionHeaders[sheader.sh_link].sh_offset);

            // Find symbol table
            uint32_t symtabsize = uint32_t(sheader.sh_size);
            int8_t * symtab = buf() + uint32_t(sheader.sh_offset);
            int8_t * symtabend = symtab + symtabsize;
            if (entrysize < sizeof(Elf64_Sym)) { err.submit(ERR_ELF_RECORD_SIZE); entrysize = sizeof(Elf64_Sym); }

            // Loop through symbol table
            for (int symi = 0; symtab < symtabend; symtab += entrysize, symi++) {
                // Copy ElfFWC_Sym symbol table entry or convert Elf64_Sym bit entry
                ElfFWC_Sym sym;
                if (fileHeader.e_machine == EM_FORWARDCOM) {
                    sym = *(ElfFWC_Sym*)symtab;
                }
                else {
                    // Translate x64 type symbol table entry
                    Elf64_Sym sym64 = *(Elf64_Sym*)symtab;
                    sym.st_name = sym64.st_name;
                    sym.st_type = sym64.st_type;
                    sym.st_bind = sym64.st_bind;
                    sym.st_other = sym64.st_other;
                    sym.st_shndx = sym64.st_shndx;
                    sym.st_value = sym64.st_value;
                    sym.st_unitsize = (uint32_t)sym64.st_size;
                    sym.st_unitnum = 1;
                    sym.st_reguse1 = sym.st_reguse2 = 0;
                }
                int type = sym.st_type;
                int binding = sym.st_bind;
                if (sym.st_shndx > 0
                    && type != STT_SECTION && type != STT_FILE
                    && (binding == STB_GLOBAL || binding == STB_WEAK)) {
                    // Public symbol found
                    SStringEntry se;
                    se.member = m;
                    // Store name
                    se.string = strings->pushString((char*)strtab + sym.st_name);
                    // Store name index
                    index->push(se);
                }
            }
        }
    }
}

// SymbolName
const char * CELF::symbolName(uint32_t index) {
    // Get name of symbol. (ParseFile() must be called first)
    const char * symname = 0;  // Symbol name
    uint32_t symi;           // Symbol index
    uint32_t stri;           // String index
    if (symbolTableOffset) {
        symi = symbolTableOffset + index * symbolTableEntrySize;
        if (symi < dataSize()) {
            stri = get<Elf64_Sym>(symi).st_name;
            if (stri < symbolStringTableSize) {
                symname = (char*)buf() + symbolStringTableOffset + stri;
            }
        }
    }
    return symname;
}

// split ELF file into container classes
int CELF::split() {
    uint32_t sc;                                 // Section index
    uint32_t type;                               // Section type
    parseFile();                                 // Parse file, get section headers, check integrity
    CDynamicArray<Elf64_Shdr> newSectionHeaders; // Remake list of section headers
    newSectionHeaders.setSize(nSections*(uint32_t)sizeof(Elf64_Shdr)); // Reserve space but leave getNumEntries() zero
    CDynamicArray<uint32_t> sectionIndexTrans;   // Translate old section indices to new indices
    sectionIndexTrans.setNum(nSections + 2);

    // Make program headers list
    uint32_t nProgramHeaders = fileHeader.e_phnum;
    uint32_t programHeaderSize = fileHeader.e_phentsize;
    if (nProgramHeaders && programHeaderSize <= 0) err.submit(ERR_ELF_RECORD_SIZE);
    uint32_t programHeaderOffset = (uint32_t)fileHeader.e_phoff;
    Elf64_Phdr pHeader;
    for (uint32_t i = 0; i < nProgramHeaders; i++) {
        pHeader = get<Elf64_Phdr>(programHeaderOffset);
        if (pHeader.p_filesz > 0 && (uint32_t)pHeader.p_offset < dataSize()) {
            uint32_t phOffset = dataBuffer.push(buf() + (uint32_t)pHeader.p_offset, (uint32_t)pHeader.p_filesz);
            pHeader.p_offset = phOffset;         // New offset refers to dataBuffer
        }
        programHeaders.push(pHeader);            // Save in programHeaders list
    }

    // Make section list
    // Make dummy empty section
    Elf64_Shdr sheader2 = {0,0,0,0,0,0,0,0,0,0};
    newSectionHeaders.push(sheader2);

    for (sc = 0; sc < nSections; sc++) {
        // Copy section header
        sheader2 = sectionHeaders[sc];                     // sectionHeaders[] was filled by ParseFile()
        type = sheader2.sh_type;                           // Section type
        if (type == SHT_NULL /* && sc == 0 */) continue;   // skip first/all empty section headers
        // Skip symbol, relocation, and string tables. They are converted to containers:
        if (type == SHT_SYMTAB || type == SHT_STRTAB || type == SHT_RELA || type == SHT_REL || type == SHT_DYNSYM) continue;

        // Get section name
        uint32_t namei = sheader2.sh_name;
        if (namei >= secStringTableLen) { err.submit(ERR_ELF_STRING_TABLE); return ERR_ELF_STRING_TABLE; }
        const char * Name = secStringTableLen ? secStringTable + namei : "???";
        // Copy name to sectionNameBuffer 
        uint32_t nameo = stringBuffer.pushString(Name);
        sheader2.sh_name = nameo;                           // New name index refers to sectionNameBuffer

        // Get section data
        int8_t  * sectionData = buf() + sheader2.sh_offset;
        uint32_t  InitSize = (sheader2.sh_type == SHT_NOBITS) ? 0 : (uint32_t)sheader2.sh_size;

        if (InitSize) {
            // Copy data to dataBuffer
            uint32_t newOffset = dataBuffer.push(sectionData, InitSize);
            sheader2.sh_offset = newOffset;       // New offset refers to dataBuffer
        }
        else {
            sheader2.sh_offset = sheader2.sh_size = 0;
        }

        // Save modified header and new index
        sectionIndexTrans[sc] = newSectionHeaders.numEntries();
        newSectionHeaders.push(sheader2);
    }

    // Make symbol list

    // Allocate array for translating symbol indices for multiple symbol tables
    // in source file to a single symbol table in the symbols container
    CDynamicArray<uint32_t> SymbolTableOffset;   // Offset of new symbol table indices relative to old indices
    SymbolTableOffset.setNum(nSections + 1);
    uint32_t NumSymbols = 0;

    for (sc = 0; sc < nSections; sc++) {
        Elf64_Shdr & sheader = sectionHeaders[sc];
        uint32_t entrysize = (uint32_t)(sheader.sh_entsize);

        if (sheader.sh_type == SHT_SYMTAB || sheader.sh_type == SHT_DYNSYM) {
            // This is a symbol table

            // Offset for symbols in this symbol table = number of preceding symbols from other symbol tables
            // Symbol number in joined table = symi1 + number of symbols in preceding tables
            SymbolTableOffset[sc] = NumSymbols;

            // Find associated string table
            if (sheader.sh_link >= nSections) { err.submit(ERR_ELF_INDEX_RANGE); sheader.sh_link = 0; }
            uint32_t strtabOffset = (uint32_t)sectionHeaders[sheader.sh_link].sh_offset;
            if (sectionHeaders[sheader.sh_link].sh_offset >= dataSize()) err.submit(ERR_ELF_INDEX_RANGE);

            // Find symbol table
            uint32_t symtabsize = (uint32_t)(sheader.sh_size);
            int8_t * symtab = buf() + uint32_t(sheader.sh_offset);
            int8_t * symtabend = symtab + symtabsize;
            if (entrysize < (uint32_t)sizeof(Elf64_Sym)) { 
                err.submit(ERR_ELF_RECORD_SIZE); entrysize = (uint32_t)sizeof(ElfFWC_Sym); 
            }

            // Loop through symbol table
            uint32_t symi1;                           // Symbol number in this table
            ElfFWC_Sym sym;                           // copy of symbol table entry
            for (symi1 = 0; symtab < symtabend; symtab += entrysize, symi1++) {

                // Copy or convert symbol table entry
                if (fileHeader.e_machine == EM_FORWARDCOM) {
                    sym = *(ElfFWC_Sym*)symtab;
                }
                else {
                    // Translate x64 type symbol table entry
                    Elf64_Sym sym64 = *(Elf64_Sym*)symtab;
                    sym.st_name = sym64.st_name;
                    sym.st_type = sym64.st_type;
                    sym.st_bind = sym64.st_bind;
                    sym.st_other = sym64.st_other;
                    sym.st_shndx = sym64.st_shndx;
                    sym.st_value = sym64.st_value;
                    sym.st_unitsize = (uint32_t)sym64.st_size;
                    sym.st_unitnum = 1;
                    sym.st_reguse1 = sym.st_reguse2 = 0;
                }
                // if (sym.st_type == STT_NOTYPE && symi1 == 0) continue; // Include empty symbols to avoid changing indexes

                // Translate section index in symbol record
                if (sym.st_shndx < sectionIndexTrans.numEntries()) {
                    sym.st_shndx = sectionIndexTrans[sym.st_shndx];
                }

                // Get name
                if (sym.st_name) {
                    if ((uint64_t)strtabOffset + sym.st_name > dataSize()) err.submit(ERR_ELF_INDEX_RANGE);
                    else {
                        const char * symName = (char*)buf() + strtabOffset + sym.st_name;
                        sym.st_name = stringBuffer.pushString(symName);
                    }
                }

                // Put name in symbolNameBuffer and update string index
                symbols.push(sym);

                // Count symbols
                NumSymbols++;
            }
        }
    }

    // Make relocation list
    union {
        Elf64_Rela   a;
        ElfFWC_Rela  i;
        ElfFWC_Rela2 r2;
    } rel;

    // Loop through sections
    for (sc = 0; sc < nSections; sc++) {
        // Get section header
        Elf64_Shdr & sheader = sectionHeaders[sc];

        if (sheader.sh_type == SHT_RELA || sheader.sh_type == SHT_REL) {
            // Relocations section
            int8_t * reltab = buf() + uint32_t(sheader.sh_offset);
            int8_t * reltabend = reltab + uint32_t(sheader.sh_size);
            int entrysize = (uint32_t)(sheader.sh_entsize);
            int expectedentrysize = sheader.sh_type == SHT_RELA ? sizeof(Elf64_Rela) : 16;  // Elf64_Rela : Elf64_Rel
            if (entrysize < expectedentrysize) {
                err.submit(ERR_ELF_RECORD_SIZE); entrysize = expectedentrysize;
            }

            int32_t symbolSection = (int32_t)sheader.sh_link; // Symbol section, old index
            if (symbolSection <= 0 || (uint32_t)symbolSection >= nSections) {
                err.submit(ERR_ELF_SYMTAB_MISSING); return ERR_ELF_SYMTAB_MISSING;
            }
            // Symbol offset is zero if there is only one symbol section, which is the normal case
            uint32_t symbolOffset = SymbolTableOffset[symbolSection];

            uint32_t relSection = sheader.sh_info; // Section to relocate, old index
            if (relSection == 0 || relSection >= nSections) {
                err.submit(ERR_ELF_UNKNOWN_SECTION, relSection); relSection = 0;
            }
            else { // Translate index of relocated section
                relSection = sectionIndexTrans[relSection];
            }

            // Loop through entries
            for (; reltab < reltabend; reltab += entrysize) {
                // Copy or translate relocation table entry 
                rel.a = *(Elf64_Rela*)reltab;
                if (sheader.sh_type == SHT_REL) {
                    rel.a.r_addend = 0;
                }
                if (fileHeader.e_machine == EM_X86_64) {
                    // Translate relocation type
                    switch (rel.a.r_type) {
                    case R_X86_64_64:    // Direct 64 bit 
                        rel.a.r_type = R_FORW_ABS | R_FORW_64;
                        break;
                    case R_X86_64_PC32:  // Self relative 32 bit signed (not RIP relative in the sense used in COFF files)
                        rel.a.r_type = R_FORW_SELFREL | R_FORW_32;
                        break;
                    case R_X86_64_32:    // Direct 32 bit zero extended
                    case R_X86_64_32S:   // Direct 32 bit sign extended
                        rel.a.r_type = R_FORW_ABS | R_FORW_32;
                        break;
                    }
                    rel.i.r_refsym = 0;
                }

                // Target symbol
                rel.a.r_sym += symbolOffset;

                // Insert relocated section
                rel.r2.r_section = relSection;

                // Save relocation
                relocations.push(rel.r2);
            }
        }
    }
    // Replace old section header list by new one
    sectionHeaders << newSectionHeaders;
    nSections = sectionHeaders.numEntries();
    return 0;
}

// Join containers into ELF file
int CELF::join(uint32_t e_type) {
    uint32_t sc;                                 // Section index
    uint32_t os;                                 // Offset of data in file
    uint32_t size;                               // Size of section data
    uint32_t shtype;                             // Section header type
    const char * name;                           // Name of a symbol
    CDynamicArray<Elf64_Shdr> newSectionHeaders; // Modify list of section headers
    CDynamicArray<Elf64_Shdr> newRelocHeaders;   // Make new relocation section headers here
    CDynamicArray<uint32_t> sectionIndexTrans;   // Translate old section indices to new indices
    CMemoryBuffer newStrtab;                     // Temporary symbol string table
    CMemoryBuffer newShStrtab;                   // Temporary section string table
    char text[128];                              // Temporary text
    nSections = sectionHeaders.numEntries();     // Number of sections
    newSectionHeaders.setSize(nSections*sizeof(Elf64_Shdr)); // Allocate space for section headers, but don't set number
    sectionIndexTrans.setNum(nSections + 1);     // Indices are initialized to zero

    // Clear any previous file
    setSize(0);

    // Make file header
    Elf64_Ehdr fileheader;
    memset(&fileheader, 0, sizeof(fileheader));
    uint8_t * eident = fileheader.e_ident;
    *(uint32_t*)eident = ELFMAG; // Put file type magic number in
    fileheader.e_ident[EI_CLASS] = ELFCLASS64; // file class        
    fileheader.e_ident[EI_DATA] = ELFDATA2LSB; //  2's complement, little endian
    fileheader.e_ident[EI_VERSION] = EV_CURRENT; //  current ELF version
    fileheader.e_ident[EI_OSABI] = ELFOSABI_FORWARDCOM; // ForwardCom ABI
    fileheader.e_ident[EI_ABIVERSION] = EI_ABIVERSION_FORWARDCOM; // ForwardCom ABI version
    fileheader.e_type = e_type;        // File type: object or executable
    fileheader.e_machine = EM_FORWARDCOM; // Machine type: ForwardCom 
    fileheader.e_ehsize = sizeof(fileheader);
    push(&fileheader, sizeof(fileheader));       // Insert file header into new file

    // Prepare string tables to be put in last
    Elf64_Shdr strtabHeader = { 0, SHT_STRTAB, 0, 0, 0, 0, 0, 0, 1, 1 };
    Elf64_Shdr shstrtabHeader = { 0, SHT_STRTAB, 0, 0, 0, 0, 0, 0, 1, 1 };
    newStrtab.pushString("");                   // Dummy empty string at start to avoid zero offset
    newShStrtab.pushString("");

    if (e_type == ET_EXEC) {
        // Executable file. Insert program headers
        uint32_t ph;  // Program header index
        fileheader.e_phoff = dataSize();
        fileheader.e_phentsize = (uint16_t)sizeof(Elf64_Phdr);
        for (ph = 0; ph < programHeaders.numEntries(); ph++) {
            push(&programHeaders[ph], sizeof(Elf64_Phdr));
        }
        // Insert program header data
        for (uint32_t ph = 0; ph < programHeaders.numEntries(); ph++) {
            if (programHeaders[ph].p_filesz) {
                os = push(dataBuffer.buf() + programHeaders[ph].p_offset, (uint32_t)programHeaders[ph].p_filesz);
                get<Elf64_Phdr>(uint32_t(fileheader.e_phoff + ph*sizeof(Elf64_Phdr))).p_offset = os;
                fileheader.e_phnum++;
            }
        }
    }

    // Put section data into file
    for (sc = 0; sc < sectionHeaders.numEntries(); sc++) {
        Elf64_Shdr sectionHeader = sectionHeaders[sc];  // Copy section header
        shtype = sectionHeader.sh_type;  // Section type
        // Skip sections with no data        
        // Skip relocation sections, they are reconstructed later.
        // Skip string tables, they are reconstructed later.
        if (shtype == SHT_NULL || shtype == SHT_RELA || shtype == SHT_REL || shtype == SHT_STRTAB) {
            continue;
        }
        else if (shtype != SHT_NOBITS && sectionHeader.sh_size != 0) {
            // Section contains data
            os = (uint32_t)sectionHeader.sh_offset;
            size = (uint32_t)sectionHeader.sh_size;
            if (os + size > dataBuffer.dataSize()) {
                err.submit(ERR_ELF_INDEX_RANGE); return ERR_ELF_INDEX_RANGE;
            }
            // Put raw data into file and save the offset
            os = push(dataBuffer.buf() + os, size);
            sectionHeader.sh_offset = os;
        }
        // Get section name
        if (sectionHeader.sh_name >= stringBuffer.dataSize()) {
            err.submit(ERR_ELF_INDEX_RANGE); sectionHeader.sh_name = 0;
        }
        else {
            const char * name = (char*)stringBuffer.buf() + sectionHeader.sh_name;
            if (*name) {
                sectionHeader.sh_name = newShStrtab.pushString(name);
            }
        }
        // Save modified header and index
        sectionIndexTrans[sc] = newSectionHeaders.numEntries() + 1;
        newSectionHeaders.push(sectionHeader);
    }
    // Number of sections with program code and data:
    uint32_t numDataSections = newSectionHeaders.numEntries();

    // Check which sections need relocations
    for (sc = 0; sc < relocations.numEntries(); sc++) {
        uint32_t rsection = uint32_t(relocations[sc].r_section);
        if (rsection < sectionHeaders.numEntries()) {
            // Mark that this section needs relocation
            // This will not go into the file, and sh_addr is unused in ForwardCom anyway           
            sectionHeaders[rsection].sh_addr = 999999999;
        }
    }
    // Count sections that need relocations
    uint32_t numRelocationSections = 0;
    for (sc = 0; sc < sectionHeaders.numEntries(); sc++) {
        if ((int)sectionHeaders[sc].sh_addr == 999999999) numRelocationSections++;
    }

    // Now we know how many headers the new file will contain. Assign indexes:
    // one empty header at start
    // numDataSections: code and data
    // one section: symbols
    // numRelocationSections sections with relocations
    // one symbol names strtab
    // one section names shstrtab
    uint32_t symbolSection = numDataSections + 1;
    uint32_t firstRelSection = symbolSection + 1;
    uint32_t shstrtabSection = firstRelSection + numRelocationSections;
    uint32_t strtabSection = shstrtabSection + 1;
    uint32_t numSections = strtabSection + 1;

    // Insert symbol table and make temporary string table
    align(8);
    Elf64_Shdr symtabHeader = { 0, SHT_SYMTAB, 0, 0, 0, 0, strtabSection, 0, 8, sizeof(ElfFWC_Sym) };
    symtabHeader.sh_offset = dataSize();

    // Loop through symbol table
    for (uint32_t sym = 0; sym < symbols.numEntries(); sym++) {
        ElfFWC_Sym ss = symbols[sym];
        uint32_t nameOffset = ss.st_name;
        ss.st_name = 0;
        if (nameOffset >= stringBuffer.dataSize()) {
            err.submit(ERR_INDEX_OUT_OF_RANGE);
        }
        else {
            name = (char*)stringBuffer.buf() + nameOffset;
            if (*name) { // Put name string into file and save new offset                
                ss.st_name = newStrtab.pushString(name);
            }
        }
        push(&ss, sizeof(ElfFWC_Sym));
    }
    // Calculate size of symtab
    symtabHeader.sh_size = dataSize() - symtabHeader.sh_offset;
    symtabHeader.sh_name = newShStrtab.pushString("symtab"); // Assign name

   // Insert relocation table   
   // For each section that needs relocations, make relocation section header and save relocation data
    Elf64_Shdr relocationHeader = { 0, SHT_RELA, SHF_INFO_LINK, 0, 0, 0, 0, 0, 0, sizeof(ElfFWC_Rela) };

    for (sc = 0; sc < sectionHeaders.numEntries(); sc++) { // Loop through sections
        if ((int)sectionHeaders[sc].sh_addr == 999999999) { // This section needs relocation.
            relocationHeader.sh_info = sectionIndexTrans[sc]; // Section to relocate, new index
            relocationHeader.sh_link = symbolSection; // Symbol table index
            // Name of relocation section = "rela_" + name of relocated section
            // Limit the section name length that is used. This has no significance because the name of SHT_RELA sections is ignored
            const int sectionNameLimit = 32;
            strcpy(text, "rela_");
            strncpy(text + 5, (char*)stringBuffer.buf() + sectionHeaders[sc].sh_name, sectionNameLimit);
            text[sectionNameLimit] = 0;
            relocationHeader.sh_name = newShStrtab.pushString(text); // Save name
            relocationHeader.sh_offset = dataSize();              // Save offset

            // Search for relocation records for this section and put them into file
            for (uint32_t r = 0; r < relocations.numEntries(); r++) {
                ElfFWC_Rela2 rel = relocations[r];
                if (rel.r_section == sc) {
                    push(&rel, (uint32_t)sizeof(ElfFWC_Rela));  // push relocation record, except r_section which is known from the header
                }
            }
            // Calculate size of this relocation section
            relocationHeader.sh_size = dataSize() - relocationHeader.sh_offset;
            // Save header in temporary list
            newRelocHeaders.push(relocationHeader);
        }
    }

    // Make string tables
    shstrtabHeader.sh_name = newShStrtab.pushString("shstrtab");
    strtabHeader.sh_name = newShStrtab.pushString("strtab");
    shstrtabHeader.sh_offset = dataSize();
    push(newShStrtab.buf(), newShStrtab.dataSize());
    shstrtabHeader.sh_size = dataSize() - shstrtabHeader.sh_offset;
    strtabHeader.sh_offset = dataSize();
    push(newStrtab.buf(), newStrtab.dataSize());
    strtabHeader.sh_size = dataSize() - strtabHeader.sh_offset;

    // Insert section headers
    align(8);
    fileheader.e_shoff = dataSize();
    fileheader.e_shentsize = sizeof(Elf64_Shdr);
    fileheader.e_shstrndx = shstrtabSection;
    Elf64_Shdr nullhdr = { 0,0,0,0,0,0,0,0,0,0 };            // First section header must be empty
    push(&nullhdr, sizeof(Elf64_Shdr));
    push(newSectionHeaders.buf(), newSectionHeaders.numEntries() * sizeof(Elf64_Shdr));
    push(&symtabHeader, sizeof(Elf64_Shdr));
    push(newRelocHeaders.buf(), newRelocHeaders.numEntries() * sizeof(Elf64_Shdr));
    push(&shstrtabHeader, sizeof(Elf64_Shdr));
    push(&strtabHeader, sizeof(Elf64_Shdr));

    // Update file header
    fileheader.e_shnum = numSections;
    get<Elf64_Ehdr>(0) = fileheader;
    return 0;
}


// Add section header and section data
uint32_t  CELF::addSection(Elf64_Shdr & section, CMemoryBuffer const & strings, CMemoryBuffer const & data) {
    Elf64_Shdr section2 = section;             // copy section header
    int nul = 0;
    section2.sh_name = stringBuffer.pushString((const char*)strings.buf() + section.sh_name); // copy string
    if (dataBuffer.dataSize() == 0) dataBuffer.push(&nul, 4);     // add a zero to avoid offset beginning at zero
    section2.sh_offset = dataBuffer.push(data.buf() + section.sh_offset, (uint32_t)section.sh_size); // copy data
    if (sectionHeaders.dataSize() == 0) {    // make empty section 0
        Elf64_Shdr section0 = {0,0,0,0,0,0,0,0,0,0};
        sectionHeaders.push(section0);
    }
    sectionHeaders.push(section2);         // save section header
    return sectionHeaders.numEntries() - 1;  // return section number
}

// Add program header
void CELF::addProgHeader(Elf64_Phdr & header) {
    programHeaders.push(header);
}


// Add a symbol
uint32_t CELF::addSymbol(ElfFWC_Sym & symbol, CMemoryBuffer const & strings) {
    ElfFWC_Sym symbol2 = symbol;   // copy symbol
    if (symbol2.st_unitnum == 0) symbol2.st_unitnum = 1;
    if (stringBuffer.numEntries() == 0) stringBuffer.pushString(""); // put empty string at position zero to avoid zero index
    symbol2.st_name = stringBuffer.pushString((const char*)strings.buf() + symbol.st_name);  // copy name
    symbols.push(symbol2);          // save symbol record
    return symbols.numEntries() - 1;  // return new symbol index
}

// Add a relocation
void CELF::addRelocation(ElfFWC_Rela2 & relocation) {
    relocations.push(relocation);
}


// structure used by removePrivateSymbols()
struct SSymbolCleanup {
    uint32_t preserve;                 // symbol must be preserved in output file
    uint32_t newIndex;                 // new index of symbol after local symbols have been removed
};

// remove local symbols and adjust relocation records with new symbol indexes
void CELF::removePrivateSymbols() {
    CDynamicArray<SSymbolCleanup> symbolTranslate;     // list for translating symbol indexes
    symbolTranslate.setNum(symbols.numEntries());
    uint32_t symi;                                     // symbol index
    uint32_t reli;                                     // relocation index
                                                       // loop through symbols
    for (symi = 1; symi < symbols.numEntries(); symi++) {
        if (symbols[symi].st_bind != STB_LOCAL && !(symbols[symi].st_other & STV_HIDDEN)) {
            symbolTranslate[symi].preserve = 1;               // mark public symbols
        }
    }
    // loop through relocations. preserve any symbols that relocations refer to
    for (reli = 0; reli < relocations.numEntries(); reli++) {
        symi = relocations[reli].r_sym;
        if (symi) {
            symbolTranslate[symi].preserve = 1;
        }
        symi = relocations[reli].r_refsym;
        if (symi) {
            symbolTranslate[symi].preserve = 1;
        }
    }
    // make new symbol list
    CDynamicArray<ElfFWC_Sym> symbols2;
    symbols2.setNum(1);                        // make first symbol empty
    for (symi = 1; symi < symbols.numEntries(); symi++) {
        if (symbolTranslate[symi].preserve) {
            symbolTranslate[symi].newIndex = symbols2.push(symbols[symi]);
        }
    }
    // update relocations with new symbol indexes
    for (reli = 0; reli < relocations.numEntries(); reli++) {
        if (relocations[reli].r_sym) {
            relocations[reli].r_sym = symbolTranslate[relocations[reli].r_sym].newIndex;
        }
        if (relocations[reli].r_refsym) {
            relocations[reli].r_refsym = symbolTranslate[relocations[reli].r_refsym].newIndex;
        }
    }
    // replace symbol table
    symbols << symbols2;
}

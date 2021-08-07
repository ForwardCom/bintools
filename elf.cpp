/****************************    elf.cpp    *********************************
* Author:        Agner Fog
* Date created:  2006-07-18
* Last modified: 2021-05-28
* Version:       1.11
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
* Copyright 2006-2021 GNU General Public License v. 3 http://www.gnu.org/licenses
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
    {ELFDATANONE,       "None"},
    {ELFDATA2LSB,       "Little Endian"},
    {ELFDATA2MSB,       "Big Endian"}
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
    {ET_NONE,           "None"},
    {ET_REL,            "Relocatable"},
    {ET_EXEC,           "Executable"},
    {ET_DYN,            "Shared object"},
    {ET_CORE,           "Core file"}
};

// File header flag names
SIntTxt ELFFileFlagNames[] = {
    {EF_INCOMPLETE,     "Has unresolved references"},
    {EF_RELINKABLE,     "Relinkable"},
    {EF_RELOCATE,       "Relocate when loading"},
    {EF_POSITION_DEPENDENT, "Position dependent"}
};


// Section type names
SIntTxt ELFSectionTypeNames[] = {
    {SHT_NULL,          "None"},
    {SHT_PROGBITS,      "Program data"},
    {SHT_SYMTAB,        "Symbol table"},
    {SHT_STRTAB,        "String table"},
    {SHT_RELA,          "Relocation w addends"},
    {SHT_NOTE,          "Notes"},
    {SHT_NOBITS,        "uinitialized"},
    {SHT_COMDAT,        "Communal section"},    
    {SHT_LIST,          "List"}    
//    {SHT_HASH,          "Symbol hash table"},
//    {SHT_DYNAMIC,       "Dynamic linking info"},
//    {SHT_REL,           "Relocation entries"},
//    {SHT_SHLIB,         "Reserved"},
//    {SHT_DYNSYM,        "Dynamic linker symbol table"},
//    {SHT_GROUP,         "Section group"},
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
    {SHF_EXEC,          "executable"},
    {SHF_WRITE,         "writeable"},
    {SHF_READ,          "readable"},
    {SHF_ALLOC,         "allocate"},
    {SHF_IP,            "IP"},
    {SHF_DATAP,         "DATAP"},
    {SHF_THREADP,       "THREADP"},
    {SHF_MERGE,         "merge"},
    {SHF_STRINGS,       "strings"},
    {SHF_INFO_LINK,     "sh_info"},
    {SHF_EVENT_HND,     "event handler"},
    {SHF_DEBUG_INFO,    "debug info"},
    {SHF_COMMENT,       "comment"},
    {SHF_RELINK,        "relinkable"},
    {SHF_AUTOGEN,       "auto-generated"}
};

// Symbol binding names
SIntTxt ELFSymbolBindingNames[] = {
    {STB_LOCAL,         "local"},
    {STB_GLOBAL,        "global"},
    {STB_WEAK,          "weak"},
    {STB_WEAK2,         "weak2"},
    {STB_UNRESOLVED,    "unresolved"}
};

// Symbol Type names
SIntTxt ELFSymbolTypeNames[] = {
    {STT_NOTYPE,        "None"},
    {STT_OBJECT,        "Object"},
    {STT_FUNC,          "Function"},
    {STT_SECTION,       "Section"},
    {STT_FILE,          "File"},
    {STT_CONSTANT,      "Constant"}
};

// Symbol st_other info names
SIntTxt ELFSymbolInfoNames[] = {
    {STV_EXEC,          "executable"},
    {STV_READ,          "read"},
    {STV_WRITE,         "write"},
    {STV_IP,            "ip"},
    {STV_DATAP,         "datap"},
    {STV_THREADP,       "threadp"},
    {STV_REGUSE,        "reguse"},
    {STV_FLOAT,         "float"},
    {STV_STRING,        "string"}, 
    {STV_UNWIND,        "unwind"},
    {STV_DEBUG,         "debug"},
    {STV_COMMON,        "communal"},
    {STV_RELINK,        "relinkable"},
    {STV_MAIN,          "main"},
    {STV_EXPORTED,      "exported"},
    {STV_THREAD,        "thread"}
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
    {R_FORW_IP_BASE,    "Relative to __ip_base"},
    {R_FORW_DATAP,      "Relative to __datap_base"},
    {R_FORW_THREADP,    "Relative to __threadp_base"},
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
    zeroAllMembers(*this);
}

// ParseFile
void CELF::parseFile() {
    // Load and parse file buffer
    uint32_t i;
    if (dataSize() == 0) {
        nSections = 0;
        return;
    }
    fileHeader = *(ElfFwcEhdr*)buf();             // Copy file header
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
    if (err.number()) 
        return;

    for (i = 0; i < nSections; i++) {
        sectionHeaders[i] = get<ElfFwcShdr>(SectionOffset);
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
            // Copy ElfFwcSym symbol or convert Elf64_Sym symbol
            if (fileHeader.e_machine == EM_FORWARDCOM) {
                symname = ((ElfFwcSym*)symtab)[symi].st_name;
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
    printf("\nDump of ELF file %s", cmd.getFilename(cmd.inputFile));

    if (options == 0) options = DUMP_FILEHDR;

    if (options & DUMP_FILEHDR) {
        // File header
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

        printf("\nNumber of sections: %2i", nSections);

        if (fileHeader.e_machine == EM_FORWARDCOM) {
            printf("\nip_base: 0x%X, datap_base: 0x%X, threadp_base: 0x%X, entry_point: 0x%X", 
                (uint32_t)fileHeader.e_ip_base, (uint32_t)fileHeader.e_datap_base, (uint32_t)fileHeader.e_threadp_base, (uint32_t)fileHeader.e_entry);
        }
    }
    // Always show flags
    if (fileHeader.e_flags) {
        printf("\nFlags:");
        for (int i = 0; i < 32; i++) {
            if (fileHeader.e_flags & (1 << i)) {
                printf(" %s,", Lookup(ELFFileFlagNames, 1 << i));
            }
        }
    }

    if (options & DUMP_LINKMAP) {
        fprintf(stdout, "\nLink map:\n");
        makeLinkMap(stdout);
    } 

    if (options & DUMP_RELINKABLE) {
        // Show names of relinkable modules and libraries
        CDynamicArray<SLCommand> mnames;  // list of relinkable modules
        CDynamicArray<SLCommand> lnames;  // list of relinkable libraries
        SLCommand nameRec;                // record containing name
        uint32_t r;                       // record index
        uint32_t sec;                     // section index
        const char * modName;             // module name
        const char * libName;             // library name
        // loop through sections
        for (sec = 0; sec < sectionHeaders.numEntries(); sec++) {
            ElfFwcShdr & secHdr = sectionHeaders[sec];
            if (secHdr.sh_type == 0) continue;

            if (secHdr.sh_flags & SHF_RELINK) {
                // section is relinkable
                if (secHdr.sh_library && secHdr.sh_library < secStringTableLen) {
                    libName = secStringTable + secHdr.sh_library;             // library name
                    nameRec.value = cmd.fileNameBuffer.pushString(libName);
                    lnames.addUnique(nameRec);                                // add only one instance to lnames
                }
                if (secHdr.sh_module && secHdr.sh_module < secStringTableLen) {
                    modName = secStringTable + secHdr.sh_module;              // module name
                    nameRec.value = cmd.fileNameBuffer.pushString(modName);
                    mnames.addUnique(nameRec);                                // add only one instance to mnames
                }
            }
        }
        if (lnames.numEntries()) {
            printf("\n\nRelinkable libraries:");
            for (r = 0; r < lnames.numEntries(); r++) {
                printf("\n   %s", cmd.getFilename((uint32_t)lnames[r].value));
            }
        }
        if (mnames.numEntries()) {
            printf("\n\nRelinkable modules:");
            for (r = 0; r < mnames.numEntries(); r++) {
                printf("\n   %s", cmd.getFilename((uint32_t)mnames[r].value));
            }
        }
    }

    if ((options & DUMP_SECTHDR) && fileHeader.e_phnum) {
        // Dump program headers
        printf("\n\nProgram headers:");
        uint32_t nProgramHeaders = fileHeader.e_phnum;
        uint32_t programHeaderSize = fileHeader.e_phentsize;
        if (nProgramHeaders && programHeaderSize <= 0) err.submit(ERR_ELF_RECORD_SIZE);
        uint32_t programHeaderOffset = (uint32_t)fileHeader.e_phoff;
        ElfFwcPhdr pHeader;
        for (uint32_t i = 0; i < nProgramHeaders; i++) {
            pHeader = get<ElfFwcPhdr>(programHeaderOffset);
            printf("\nProgram header Type: %s, flags 0x%X",
                Lookup(ELFPTypeNames, (uint32_t)pHeader.p_type), (uint32_t)pHeader.p_flags);
            printf("\noffset = 0x%X, vaddr = 0x%X, paddr = 0x%X, filesize = 0x%X, memsize = 0x%X, align = 0x%X",
                (uint32_t)pHeader.p_offset, (uint32_t)pHeader.p_vaddr, (uint32_t)pHeader.p_paddr, (uint32_t)pHeader.p_filesz, (uint32_t)pHeader.p_memsz, 1 << pHeader.p_align);
            programHeaderOffset += programHeaderSize;
            if (pHeader.p_filesz < 0x100 && (uint32_t)pHeader.p_offset < dataSize() && (pHeader.p_flags & SHF_STRINGS)) {
                printf("\nContents: %s", buf() + (int)pHeader.p_offset);
            }
        }
    }

    if (options & DUMP_SECTHDR) {
        // Dump section headers
        printf("\n\nSection headers:");
        for (uint32_t sc = 0; sc < nSections; sc++) {
            // Get copy of 32-bit header or converted 64-bit header
            ElfFwcShdr sheader = sectionHeaders[sc];
            uint32_t entrysize = (uint32_t)(sheader.sh_entsize);
            uint32_t namei = sheader.sh_name;
            if (namei >= secStringTableLen) { err.submit(ERR_ELF_STRING_TABLE); break; }
            printf("\n%2i Name: %-18s Type: %s", sc, secStringTable + namei,
                Lookup(ELFSectionTypeNames, sheader.sh_type));
            if (sheader.sh_flags) {
                printf("\n  Flags: 0x%X:", uint32_t(sheader.sh_flags));
                for (uint32_t fi = 1; fi != 0; fi <<= 1) {
                    if (uint32_t(sheader.sh_flags) & fi) {
                        printf(" %s", Lookup(ELFSectionFlagNames, fi));
                    }
                }
            }
            //if (sheader.sh_addr) 
                printf("\n  Address: 0x%X", uint32_t(sheader.sh_addr));
            if (sheader.sh_offset || sheader.sh_size) {
                printf("\n  FileOffset: 0x%X, Size: 0x%X",
                    uint32_t(sheader.sh_offset), uint32_t(sheader.sh_size));
            }
            if (sheader.sh_align) {
                printf("\n  Alignment: 0x%X", 1 << sheader.sh_align);
            }
            if (sheader.sh_entsize) {
                printf("\n  Entry size: 0x%X", uint32_t(sheader.sh_entsize));
                switch (sheader.sh_type) {
                /*
                case SHT_DYNAMIC:
                    printf("\n  String table: %i", sheader.sh_link);
                    break;
                case SHT_HASH:
                    printf("\n  Symbol table: %i", sheader.sh_link);
                    break; */
                case SHT_RELA: // case SHT_REL:
                    printf("\n  Symbol table: %i",
                        sheader.sh_link);
                    break;
                case SHT_SYMTAB: //case SHT_DYNSYM:
                    printf("\n  Symbol string table: %i, First global symbol: %i",
                        sheader.sh_link, sheader.sh_module);
                    break;
                default:
                    if (sheader.sh_link) {
                        printf("\n  Link: %i", sheader.sh_link);
                    }
                    if (sheader.sh_module) {
                        printf("\n  Info: %i", sheader.sh_module);
                    }
                }
            }
            if (sheader.sh_module && sheader.sh_module < secStringTableLen) {
                printf("\n  Module: %s", secStringTable + sheader.sh_module);
                if (sheader.sh_library) printf(", Library: %s", secStringTable + sheader.sh_library);
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
            if ((sheader.sh_type == SHT_SYMTAB) && (options & DUMP_SYMTAB)) {
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
                    // Copy ElfFwcSym symbol or convert Elf64_Sym symbol
                    ElfFwcSym sym;
                    if (fileHeader.e_machine == EM_FORWARDCOM) {
                        sym = *(ElfFwcSym*)symtab;
                    }
                    else {
                        // Translate x64 type symbol table entry
                        Elf64_Sym sym64 = *(Elf64_Sym*)symtab;
                        sym.st_name = sym64.st_name;
                        sym.st_type = sym64.st_type;
                        sym.st_bind = sym64.st_bind;
                        sym.st_other = sym64.st_other;
                        sym.st_section = sym64.st_section;
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
                    if (sym.st_value || type == STT_OBJECT || type == STT_FUNC || (int16_t)sym.st_section == SHN_ABS_X86) {
                        printf(" Value: 0x%X", uint32_t(sym.st_value));
                    }
                    if (sym.st_unitsize)  printf(" size: %X*%X", sym.st_unitsize, sym.st_unitnum);
                    if (sym.st_other) {
                        for (int i = 0; i < 32; i++) {
                            if (sym.st_other & 1 << i) {
                                printf(" %s", Lookup(ELFSymbolInfoNames, 1 << i));
                            }
                        }
                    }
                    if (sym.st_reguse1 | sym.st_reguse2) {
                        printf(", register use 0x%X, 0x%X", sym.st_reguse1, sym.st_reguse2);
                    }
                    //if (int32_t(sym.st_section) > 0) printf(", section: %i", sym.st_section);
                    //else { // Special segment values
                    if (sym.st_section == 0) {
                        printf(" extern,");
                    }
                    else if (sym.st_type == STT_CONSTANT) {
                        printf(", absolute,");
                    }
                    else {
                        printf(", section: 0x%X", sym.st_section);
                    }
                    if (sym.st_type || sym.st_bind) {
                        printf(" type: %s, binding: %s",
                            Lookup(ELFSymbolTypeNames, type),
                            Lookup(ELFSymbolBindingNames, binding));
                    }
                }
            }
            // Dump relocation table
            if ((sheader.sh_type == SHT_RELA) && (options & DUMP_RELTAB)) {
                printf("\n  Relocations:");
                int8_t * reltab = buf() + uint32_t(sheader.sh_offset);
                int8_t * reltabend = reltab + uint32_t(sheader.sh_size);
                /*
                uint32_t expectedentrysize = sheader.sh_type == SHT_RELA ?
                    sizeof(Elf64_Rela) :                // Elf32_Rela, Elf64_Rela
                    sizeof(Elf64_Rela) - wordSize / 8;  // Elf32_Rel,  Elf64_Rel
                    */
                uint32_t expectedentrysize = sizeof(Elf64_Rela);
                if (entrysize < expectedentrysize) { err.submit(ERR_ELF_RECORD_SIZE); entrysize = expectedentrysize; }

                // Loop through entries
                for (; reltab < reltabend; reltab += entrysize) {
                    // Copy relocation table entry with or without addend
                    ElfFwcReloc rel = *(ElfFwcReloc*)reltab;
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
                    printf("\n  Section: %i, Offset: 0x%X, Symbol: %i, Name: %s\n   Type: %s", rel.r_section,
                        uint32_t(rel.r_offset), rel.r_sym, symbolName(rel.r_sym), relocationName);
                    if (machineType == EM_FORWARDCOM && rel.r_type >> 16 == 8) {
                        printf(", ref. point %i", rel.r_refsym);
                    }
                    if (uint32_t(rel.r_addend)) {
                        printf(", Addend: 0x%X", uint32_t(rel.r_addend));
                    }

                    /* Inline addend not used by ForwardCom
                    // Find inline addend
                    ElfFwcShdr relsheader = sectionHeaders[rel.r_section];
                    uint32_t relsoffset = uint32_t(relsheader.sh_offset);
                    if (relsoffset + rel.r_offset < dataSize() && relsheader.sh_type != SHT_NOBITS) {
                        int32_t * piaddend = (int32_t*)(buf() + relsoffset + rel.r_offset);
                        if (*piaddend) printf(", Inline value: 0x%X", *piaddend);
                    }*/
                }
            }
        }
    }
}


// PublicNames
void CELF::listSymbols(CMemoryBuffer * strings, CDynamicArray<SSymbolEntry> * index, uint32_t m, uint32_t l, int scope) {
    // Make list of public and external symbols, including weak symbols
    // SStringEntry::member is set to m and library is set to l;
    // scope: 1: exported, 
    //        2: imported (includes STB_WEAK2),
    //        3: both

    // Interpret header:
    parseFile();
    if (err.number()) return;

    // Loop through section headers
    for (uint32_t sc = 0; sc < nSections; sc++) {
        // Get copy of 32-bit header or converted 64-bit header
        ElfFwcShdr sheader = sectionHeaders[sc];
        uint32_t entrysize = uint32_t(sheader.sh_entsize);

        if (sheader.sh_type == SHT_SYMTAB) {
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
                // Copy ElfFwcSym symbol table entry or convert Elf64_Sym bit entry
                ElfFwcSym sym;
                if (fileHeader.e_machine == EM_FORWARDCOM) {
                    sym = *(ElfFwcSym*)symtab;
                }
                else {
                    // Translate x64 type symbol table entry
                    Elf64_Sym sym64 = *(Elf64_Sym*)symtab;
                    sym.st_name = sym64.st_name;
                    sym.st_type = sym64.st_type;
                    sym.st_bind = sym64.st_bind;
                    sym.st_other = sym64.st_other;
                    sym.st_section = sym64.st_section;
                    sym.st_value = sym64.st_value;
                    sym.st_unitsize = (uint32_t)sym64.st_size;
                    sym.st_unitnum = 1;
                    sym.st_reguse1 = sym.st_reguse2 = 0;
                }
                int type = sym.st_type;
                int binding = sym.st_bind;
                if (type != STT_SECTION && type != STT_FILE && (binding & (STB_GLOBAL | STB_WEAK))) {
                    // Public or external symbol found
                    if (((scope & 1) && (sym.st_section != 0))      // scope 1: public
                    || ((scope & 2) && (sym.st_section == 0 || binding == STB_WEAK2))) {  // scope 2: external
                        SSymbolEntry se;
                        se.member = m;
                        se.library = l;
                        se.st_type = type;
                        se.st_bind = binding;
                        se.symindex = symi;
                        se.sectioni = sym.st_section;
                        se.st_other = (uint16_t)sym.st_other;
                        se.status = (scope & 1) << 1;
                        // Store name
                        const char * name = (char*)strtab + sym.st_name;
                        se.name = strings->pushString(name);
                        // Store name index
                        index->push(se);
                    }
                }
            }
        }
    }
}

// get a symbol record
ElfFwcSym * CELF::getSymbol(uint32_t symindex) {
    if (symbolTableOffset) {
        uint32_t symi = symbolTableOffset + symindex * symbolTableEntrySize;
        if (symi < dataSize()) {
            return &get<ElfFwcSym>(symi);
        }
    }
    return 0;
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
    if (dataSize() == 0) return 0;
    parseFile();                                 // Parse file, get section headers, check integrity
    CDynamicArray<ElfFwcShdr> newSectionHeaders; // Remake list of section headers
    newSectionHeaders.setSize(nSections*(uint32_t)sizeof(ElfFwcShdr)); // Reserve space but leave getNumEntries() zero
    CDynamicArray<uint32_t> sectionIndexTrans;   // Translate old section indices to new indices
    sectionIndexTrans.setNum(nSections + 2);

    // Make program headers list
    uint32_t nProgramHeaders = fileHeader.e_phnum;
    uint32_t programHeaderSize = fileHeader.e_phentsize;
    if (nProgramHeaders && programHeaderSize <= 0) err.submit(ERR_ELF_RECORD_SIZE);
    uint32_t programHeaderOffset = (uint32_t)fileHeader.e_phoff;
    ElfFwcPhdr pHeader;
    for (uint32_t i = 0; i < nProgramHeaders; i++) {
        pHeader = get<ElfFwcPhdr>(programHeaderOffset + i * programHeaderSize);
        if (pHeader.p_filesz > 0 && (uint32_t)pHeader.p_offset < dataSize()) {
            uint32_t phOffset = dataBuffer.push(buf() + (uint32_t)pHeader.p_offset, (uint32_t)pHeader.p_filesz);
            pHeader.p_offset = phOffset;         // New offset refers to dataBuffer
        }
        programHeaders.push(pHeader);            // Save in programHeaders list
    }

    // Make section list
    // Make dummy empty section
    ElfFwcShdr sheader2;
    zeroAllMembers(sheader2);
    newSectionHeaders.push(sheader2);

    for (sc = 0; sc < nSections; sc++) {
        // Copy section header
        sheader2 = sectionHeaders[sc];                     // sectionHeaders[] was filled by ParseFile()
        type = sheader2.sh_type;                           // Section type
        if (type == SHT_NULL /* && sc == 0 */) continue;   // skip first/all empty section headers
        // Skip symbol, relocation, and string tables. They are converted to containers:
        if (type == SHT_SYMTAB || type == SHT_STRTAB || type == SHT_RELA) continue;

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
            sheader2.sh_offset = dataBuffer.dataSize();
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
        ElfFwcShdr & sheader = sectionHeaders[sc];
        uint32_t entrysize = (uint32_t)(sheader.sh_entsize);

        if (sheader.sh_type == SHT_SYMTAB) {
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
                err.submit(ERR_ELF_RECORD_SIZE); entrysize = (uint32_t)sizeof(ElfFwcSym); 
            }

            // Loop through symbol table
            uint32_t symi1;                           // Symbol number in this table
            ElfFwcSym sym;                           // copy of symbol table entry
            for (symi1 = 0; symtab < symtabend; symtab += entrysize, symi1++) {

                // Copy or convert symbol table entry
                if (fileHeader.e_machine == EM_FORWARDCOM) {
                    sym = *(ElfFwcSym*)symtab;
                }
                else {
                    // Translate x64 type symbol table entry
                    Elf64_Sym sym64 = *(Elf64_Sym*)symtab;
                    sym.st_name = sym64.st_name;
                    sym.st_type = sym64.st_type;
                    sym.st_bind = sym64.st_bind;
                    sym.st_other = sym64.st_other;
                    sym.st_section = sym64.st_section;
                    sym.st_value = sym64.st_value;
                    sym.st_unitsize = (uint32_t)sym64.st_size;
                    sym.st_unitnum = 1;
                    sym.st_reguse1 = sym.st_reguse2 = 0;
                }
                // if (sym.st_type == STT_NOTYPE && symi1 == 0) continue; // Include empty symbols to avoid changing indexes

                // Translate section index in symbol record
                if (sym.st_section < sectionIndexTrans.numEntries()) {
                    sym.st_section = sectionIndexTrans[sym.st_section];
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
        ElfFwcReloc r2;
    } rel;

    // Loop through sections
    for (sc = 0; sc < nSections; sc++) {
        // Get section header
        ElfFwcShdr & sheader = sectionHeaders[sc];

        if (sheader.sh_type == SHT_RELA) {
            // Relocations section
            int8_t * reltab = buf() + uint32_t(sheader.sh_offset);
            int8_t * reltabend = reltab + uint32_t(sheader.sh_size);
            int entrysize = (uint32_t)(sheader.sh_entsize);
            //int expectedentrysize = sheader.sh_type == SHT_RELA ? sizeof(Elf64_Rela) : 16;  // Elf64_Rela : Elf64_Rel
            int expectedentrysize = sizeof(Elf64_Rela);
            if (entrysize < expectedentrysize) {
                err.submit(ERR_ELF_RECORD_SIZE); entrysize = expectedentrysize;
            }

            int32_t symbolSection = (int32_t)sheader.sh_link; // Symbol section, old index
            if (symbolSection <= 0 || (uint32_t)symbolSection >= nSections) {
                err.submit(ERR_ELF_SYMTAB_MISSING); return ERR_ELF_SYMTAB_MISSING;
            }
            // Symbol offset is zero if there is only one symbol section, which is the normal case
            uint32_t symbolOffset = SymbolTableOffset[symbolSection];

            // Loop through entries
            for (; reltab < reltabend; reltab += entrysize) {
                // Copy or translate relocation table entry 
                if (fileHeader.e_machine == EM_X86_64) {
                    // Translate relocation type
                    rel.a = *(Elf64_Rela*)reltab;
                    //if (sheader.sh_type == SHT_REL) rel.a.r_addend = 0;
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
                    rel.r2.r_refsym = 0;
                    rel.r2.r_section = sheader.sh_module; // sh_module = sh_info in x86
                }
                else {
                    rel.r2 = *(ElfFwcReloc*)reltab;
                }

                // Target symbol
                rel.r2.r_sym += symbolOffset;
                if (rel.r2.r_refsym) rel.r2.r_refsym += symbolOffset;

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
int CELF::join(ElfFwcEhdr * header) {
    uint32_t sc;                                 // Section index
    uint64_t os;                                 // Offset of data in file
    uint32_t size;                               // Size of section data
    uint32_t shtype;                             // Section header type
    uint32_t ph;                                 // Program header index
    const char * name;                           // Name of a symbol
    uint32_t progheadi = 0;                      // program header index
    uint32_t pHfistSection = 0;                  // first section covered by current program header
    uint32_t pHnumSections = 0;                  // number of sections covered by current program header
    bool hasProgHead = false;                    // current section is covered by a program header

    CDynamicArray<ElfFwcShdr> newSectionHeaders;// Modify list of section headers
    CDynamicArray<uint32_t> sectionIndexTrans;   // Translate old section indices to new indices
    CMemoryBuffer newStrtab;                     // Temporary symbol string table
    CMemoryBuffer newShStrtab;                   // Temporary section string table
    nSections = sectionHeaders.numEntries();     // Number of sections
    if (nSections == 0) return 0;
    newSectionHeaders.setSize(nSections*sizeof(ElfFwcShdr)); // Allocate space for section headers, but don't set number
    sectionIndexTrans.setNum(nSections + 1);     // Indices are initialized to zero

    // Clear any previous file
    setSize(0);

    // Make file header
    ElfFwcEhdr fileheader;
    if (header) fileheader = *header;
    else zeroAllMembers(fileheader);
    uint8_t * eident = fileheader.e_ident;
    *(uint32_t*)eident = ELFMAG; // Put file type magic number in
    fileheader.e_ident[EI_CLASS] = ELFCLASS64; // file class        
    fileheader.e_ident[EI_DATA] = ELFDATA2LSB; //  2's complement, little endian
    fileheader.e_ident[EI_VERSION] = EV_CURRENT; //  current ELF version
    fileheader.e_ident[EI_OSABI] = ELFOSABI_FORWARDCOM; // ForwardCom ABI
    fileheader.e_ident[EI_ABIVERSION] = EI_ABIVERSION_FORWARDCOM; // ForwardCom ABI version
    if (fileheader.e_type == 0) fileheader.e_type = ET_REL; // File type: object or executable
    fileheader.e_machine = EM_FORWARDCOM; // Machine type: ForwardCom 
    fileheader.e_ehsize = sizeof(fileheader);
    push(&fileheader, sizeof(fileheader));       // Insert file header into new file

    // Prepare string tables to be put in last
    ElfFwcShdr strtabHeader;
    zeroAllMembers(strtabHeader);
    strtabHeader.sh_type = SHT_STRTAB;
    strtabHeader.sh_align = 0;
    strtabHeader.sh_entsize = 1;
    ElfFwcShdr shstrtabHeader = strtabHeader;
    newStrtab.pushString("");                   // Dummy empty string at start to avoid zero offset
    newShStrtab.pushString("");

    if (fileheader.e_type == ET_EXEC) {
        // Executable file. Insert program headers
        uint32_t ph;  // Program header index
        fileheader.e_phoff = dataSize();
        fileheader.e_phentsize = (uint16_t)sizeof(ElfFwcPhdr);
        fileheader.e_phnum = programHeaders.numEntries();
        for (ph = 0; ph < programHeaders.numEntries(); ph++) {
            push(&programHeaders[ph], sizeof(ElfFwcPhdr));
        }
        // Insert program header data only if they are not the same as section data
        for (ph = 0; ph < programHeaders.numEntries(); ph++) {
            if ((programHeaders[ph].p_type == PT_INTERP || programHeaders[ph].p_type == PT_NOTE) && programHeaders[ph].p_filesz) {
                os = push(dataBuffer.buf() + programHeaders[ph].p_offset, (uint32_t)programHeaders[ph].p_filesz);
                get<ElfFwcPhdr>(uint32_t(fileheader.e_phoff + ph * sizeof(ElfFwcPhdr))).p_offset = os;
            }
        }
        // translate dataBuffer offset to file offset
        os = dataSize();
        os = (os + (1<<FILE_DATA_ALIGN)-1) & -(1<<FILE_DATA_ALIGN);         // align
        for (ph = 0; ph < programHeaders.numEntries(); ph++) {
            if (programHeaders[ph].p_filesz) {
                //programHeaders[ph].p_offset += dataSize();
                get<ElfFwcPhdr>(uint32_t(fileheader.e_phoff + ph * sizeof(ElfFwcPhdr))).p_offset = os;
                os += (uint32_t)programHeaders[ph].p_filesz;
            }
        }
        // sections covered by first program header
        progheadi = 0;
        pHfistSection = pHnumSections = 0;
        if (programHeaders.numEntries()) {
            pHfistSection = (uint32_t)programHeaders[progheadi].p_paddr;
            pHnumSections = (uint32_t)(programHeaders[progheadi].p_paddr >> 32);
        }
    }

    // Put section data into file
    for (sc = 0; sc < sectionHeaders.numEntries(); sc++) {
        ElfFwcShdr sectionHeader = sectionHeaders[sc];  // Copy section header
        shtype = sectionHeader.sh_type;  // Section type
        // Skip sections with no data        
        // Skip relocation sections, they are reconstructed later.
        // Skip string tables, they are reconstructed later.
        if (shtype == SHT_NULL || shtype == SHT_RELA || shtype == SHT_STRTAB) {
            continue;
        }
        else if (shtype != SHT_NOBITS && sectionHeader.sh_size != 0) {
            // Section contains data
            if (fileheader.e_type == ET_EXEC) {
                // find correcponding program header, if any
                while (sc >= pHfistSection + pHnumSections && progheadi+1 < programHeaders.numEntries()) {
                    progheadi++;
                    pHfistSection = (uint32_t)programHeaders[progheadi].p_paddr;
                    pHnumSections = (uint32_t)(programHeaders[progheadi].p_paddr >> 32);
                }
                // is this section covered by a program header?
                hasProgHead = sc >= pHfistSection && sc < pHfistSection + pHnumSections;
            }
            if (hasProgHead) {
                // check if there is filler space between this section and any previous 
                // section under the same program header
                uint64_t lastSecEnd = 0;
                if (sc > pHfistSection && sectionHeaders[sc-1].sh_type != SHT_NOBITS) {
                    // end of previous section in dataBuffer:
                    lastSecEnd = sectionHeaders[sc-1].sh_offset + sectionHeaders[sc-1].sh_size;
                    if (sectionHeader.sh_offset > lastSecEnd) {
                        // number of bytes to insert as filler
                        uint64_t fill = sectionHeader.sh_offset - lastSecEnd;
                        if (fill > MAX_ALIGN) err.submit(ERR_LINK_OVERFLOW, "","",""); // should not occur
                        uint64_t fillValue = 0;
                        if (sectionHeader.sh_flags & SHF_EXEC) {   // use filler instruction
                            fillValue = fillerInstruction | ((uint64_t)fillerInstruction << 32);
                        }
                        while (fill >= 8) {                           // loop to insert fillers
                            push(&fillValue, 8);
                            fill -= 8;
                        }
                        if (fill) push(&fillValue, (uint32_t)fill);
                    }
                }
            }
            // error check
            os = sectionHeader.sh_offset;
            size = (uint32_t)sectionHeader.sh_size;
            if (os + size > dataBuffer.dataSize()) {
                err.submit(ERR_ELF_INDEX_RANGE); return ERR_ELF_INDEX_RANGE;
            }
            // Put raw data into file and save the new offset
            align(1<<FILE_DATA_ALIGN);                               // align file data
            os = push(dataBuffer.buf() + os, size);
            sectionHeader.sh_offset = os;
        }
        else {
            sectionHeader.sh_offset = dataSize();
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
        //sectionIndexTrans[sc] = newSectionHeaders.numEntries() + 1;
        sectionIndexTrans[sc] = newSectionHeaders.numEntries();
        newSectionHeaders.push(sectionHeader);
    }
    // Number of sections with program code and data:
    uint32_t numDataSections = newSectionHeaders.numEntries();

    // put module names and library names into shstrtab for relinkable sections
    updateModuleNames(newSectionHeaders, newShStrtab);

    // Update program segments that cover the same data as sections
    for (ph = 0; ph < programHeaders.numEntries(); ph++) {
        // reference to program header in binary file
        ElfFwcPhdr & pHeader = get<ElfFwcPhdr>(uint32_t(fileheader.e_phoff + ph*sizeof(ElfFwcPhdr)));
        // sections covered by this program header
        uint32_t fistSection = (uint32_t)programHeaders[ph].p_paddr;
        uint32_t numSections = (uint32_t)(programHeaders[ph].p_paddr >> 32);
        uint32_t lastSection = fistSection + numSections - 1;
        // update file offset
        if (fistSection < sectionIndexTrans.numEntries()) {
            uint32_t sc1 = sectionIndexTrans[fistSection];
            if (sc1 < newSectionHeaders.numEntries()) {
                os = newSectionHeaders[sc1].sh_offset;
                pHeader.p_offset = os;
            }
            if (lastSection < sectionIndexTrans.numEntries()) {
                uint32_t sc2 = sectionIndexTrans[lastSection];
                if (sc2 < newSectionHeaders.numEntries()) {
                    // update segment size (it may have been increased by alignment fillers)
                    os = newSectionHeaders[sc2].sh_offset;
                    if (newSectionHeaders[sc2].sh_type != SHT_NOBITS) {
                        os += newSectionHeaders[sc2].sh_size;
                    }
                    pHeader.p_filesz = os - pHeader.p_offset;

                    //!! set p_memsz
                }
            }
        }
    }
    uint32_t numRelocationSections = 1;  // ForwardCom needs only one relocation section

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
    align(1 << FILE_DATA_ALIGN);
    ElfFwcShdr symtabHeader;
    zeroAllMembers(symtabHeader);
    symtabHeader.sh_type = SHT_SYMTAB;
    symtabHeader.sh_link = strtabSection;
    symtabHeader.sh_entsize = sizeof(ElfFwcSym);
    symtabHeader.sh_align = 3;
    symtabHeader.sh_offset = dataSize();

    // Loop through symbol table
    for (uint32_t sym = 0; sym < symbols.numEntries(); sym++) {
        ElfFwcSym ss = symbols[sym];
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
        push(&ss, sizeof(ElfFwcSym));
    }
    // Calculate size of symtab
    symtabHeader.sh_size = dataSize() - symtabHeader.sh_offset;
    symtabHeader.sh_name = newShStrtab.pushString("symtab"); // Assign name

   // Insert relocation table   
    ElfFwcShdr relocationHeader;
    zeroAllMembers(relocationHeader);
    relocationHeader.sh_type = SHT_RELA;
    relocationHeader.sh_flags = SHF_INFO_LINK;
    relocationHeader.sh_entsize = sizeof(ElfFwcReloc);
    relocationHeader.sh_module = 0;
    relocationHeader.sh_link = symbolSection; // Symbol table index
    relocationHeader.sh_name = newShStrtab.pushString("relocations"); // Save name
    relocationHeader.sh_offset = dataSize();              // Save offset
    // insert all relocations
    push(relocations.buf(), relocations.dataSize());
    // Calculate size of this relocation section
    relocationHeader.sh_size = dataSize() - relocationHeader.sh_offset;

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
    align(1 << FILE_DATA_ALIGN);
    fileheader.e_shoff = dataSize();
    fileheader.e_shentsize = sizeof(ElfFwcShdr);
    fileheader.e_shstrndx = shstrtabSection;
    ElfFwcShdr nullhdr;            // First section header must be empty
    zeroAllMembers(nullhdr);
    push(&nullhdr, sizeof(ElfFwcShdr));
    push(newSectionHeaders.buf(), newSectionHeaders.numEntries() * sizeof(ElfFwcShdr));
    push(&symtabHeader, sizeof(ElfFwcShdr));
    push(&relocationHeader, sizeof(ElfFwcShdr));
    push(&shstrtabHeader, sizeof(ElfFwcShdr));
    push(&strtabHeader, sizeof(ElfFwcShdr));

    // Update file header
    fileheader.e_shnum = numSections;
    get<ElfFwcEhdr>(0) = fileheader;
    return 0;
}

// Add section header and section data
uint32_t CELF::addSection(ElfFwcShdr & section, CMemoryBuffer const & strings, CMemoryBuffer const & data) {
    ElfFwcShdr section2 = section;             // copy section header
    int nul = 0;
    section2.sh_name = stringBuffer.pushString((const char*)strings.buf() + section.sh_name); // copy string
    if (dataBuffer.dataSize() == 0) dataBuffer.push(&nul, 4);     // add a zero to avoid offset beginning at zero
    dataBuffer.align(1 << FILE_DATA_ALIGN);                       // align in source file
    if (section.sh_type != SHT_NOBITS) {        
        section2.sh_offset = dataBuffer.push(data.buf() + section.sh_offset, (uint32_t)section.sh_size); // copy data
    }
    else {
        section2.sh_offset = dataBuffer.dataSize() + section.sh_offset; // BSS section
    }
    if (sectionHeaders.dataSize() == 0) {    // make empty section 0
        ElfFwcShdr section0;
        zeroAllMembers(section0);
        sectionHeaders.push(section0);
    }
    sectionHeaders.push(section2);         // save section header
    nSections = sectionHeaders.numEntries();
    return nSections - 1;  // return section number
}

// Extend previously added section
void CELF::extendSection(ElfFwcShdr & section, CMemoryBuffer const & data) {
    if (sectionHeaders.numEntries() == 0) {
        err.submit(ERR_LINK_OVERFLOW, "","","");
        return;
    }
    uint32_t pre = sectionHeaders.numEntries() - 1;  // previously added section
    dataBuffer.align(1 << FILE_DATA_ALIGN);          // align in source file

    // insert new data
    if (section.sh_type != SHT_NOBITS) {            
        dataBuffer.push(data.buf() + section.sh_offset, (uint32_t)section.sh_size); 
        sectionHeaders[pre].sh_size = dataBuffer.dataSize() - sectionHeaders[pre].sh_offset;
    }
    else {
        // adjust header
        sectionHeaders[pre].sh_size += section.sh_size;
    }
}

// Insert alignment fillers between sections
void CELF::insertFiller(uint64_t numBytes) {
    if (sectionHeaders.numEntries() == 0) {
        err.submit(ERR_LINK_OVERFLOW, "","","");
        return;
    }
    uint32_t pre = sectionHeaders.numEntries() - 1;        // previously added section
    uint64_t fillValue = 0;
    if (sectionHeaders[pre].sh_flags & SHF_EXEC) {         // use filler instruction
        fillValue = fillerInstruction | ((uint64_t)fillerInstruction << 32);
    }
    uint64_t f = numBytes;                                 // loop counter
    while (f >= 8) {                                       // loop to insert fillers
        dataBuffer.push(&fillValue, 8);  
        f -= 8;
    }
    if (f) dataBuffer.push(&fillValue, (uint32_t)f);
}


// Add module name and library name to relinkable sections
void CELF::addModuleNames(CDynamicArray<uint32_t> &moduleNames1, CDynamicArray<uint32_t> &libraryNames1) {
    // moduleNames and libraryNames contain indexes into cmd.fileNameBuffer
    moduleNames << moduleNames1;
    libraryNames << libraryNames1;
}

// Put module names and library names into section string table for relinkable sections
void CELF::updateModuleNames(CDynamicArray<ElfFwcShdr> &newSectionHeaders, CMemoryBuffer &newShStrtab) {
    uint32_t sec;                                // section number
    uint32_t mod;                                // module number
    uint32_t lib;                                // library number
    const char * name;                           // module or library name

    CDynamicArray<uint32_t> moduleNames2;        // module names as index into stringBuffer
    CDynamicArray<uint32_t> libraryNames2;       // library names as index into stringBuffer
    moduleNames2.setNum(moduleNames.numEntries());
    libraryNames2.setNum(libraryNames.numEntries());

    // avoid storing same name multiple times by first checking which names are needed
    for (sec = 1; sec < newSectionHeaders.numEntries(); sec++) {
        if ((newSectionHeaders[sec].sh_flags & SHF_RELINK) | cmd.debugOptions) {
            mod = newSectionHeaders[sec].sh_module;
            if (mod < moduleNames.numEntries() && moduleNames[mod]) moduleNames2[mod] = 1;
            lib = newSectionHeaders[sec].sh_library;
            if (lib && lib < libraryNames.numEntries()) libraryNames2[lib] = 1;
        }
    }
    // store needed names in shstrtab
    for (mod = 0; mod < moduleNames.numEntries(); mod++) {
        if (moduleNames2[mod]) {
            name = cmd.getFilename(moduleNames[mod]);
            moduleNames2[mod] = newShStrtab.pushString(name);
        }
    }
    for (lib = 0; lib < libraryNames.numEntries(); lib++) {
        if (libraryNames2[lib]) {
            name = cmd.getFilename(libraryNames[lib]);
            libraryNames2[lib] = newShStrtab.pushString(name);
        }
    }
    // insert updated name indexes
    for (sec = 1; sec < newSectionHeaders.numEntries(); sec++) {
        if ((newSectionHeaders[sec].sh_flags & SHF_RELINK) | cmd.debugOptions) {
            mod = newSectionHeaders[sec].sh_module;
            if (mod < moduleNames2.numEntries() && moduleNames2[mod]) {
                newSectionHeaders[sec].sh_module = moduleNames2[mod];
            }
            lib = newSectionHeaders[sec].sh_library;
            if (lib && lib < libraryNames2.numEntries()) {
                newSectionHeaders[sec].sh_library = libraryNames2[lib];
            }
        }
        else {
            newSectionHeaders[sec].sh_module = 0;
            newSectionHeaders[sec].sh_library = 0;
        }
    }
}


// Add program header
void CELF::addProgHeader(ElfFwcPhdr & header) {
    programHeaders.push(header);
}


// Add a symbol
uint32_t CELF::addSymbol(ElfFwcSym & symbol, CMemoryBuffer const & strings) {
    ElfFwcSym symbol2 = symbol;   // copy symbol
    if (symbol2.st_unitnum == 0) symbol2.st_unitnum = 1;
    if (stringBuffer.numEntries() == 0) stringBuffer.pushString(""); // put empty string at position zero to avoid zero index
    symbol2.st_name = stringBuffer.pushString((const char*)strings.buf() + symbol.st_name);  // copy name
    symbols.push(symbol2);          // save symbol record
    return symbols.numEntries() - 1;  // return new symbol index
}

// Add a relocation
void CELF::addRelocation(ElfFwcReloc & relocation) {
    relocations.push(relocation);
}


// structure used by removePrivateSymbols()
struct SSymbolCleanup {
    uint32_t preserve;                 // symbol must be preserved in output file
    uint32_t newIndex;                 // new index of symbol after local symbols have been removed
};

// remove local symbols and adjust relocation records with new symbol indexes
void CELF::removePrivateSymbols(int debugOptions) {
    CDynamicArray<SSymbolCleanup> symbolTranslate;         // list for translating symbol indexes
    symbolTranslate.setNum(symbols.numEntries());
    uint32_t symi;                                         // symbol index
    uint32_t reli;                                         // relocation index
                                                           // loop through symbols
    for (symi = 1; symi < symbols.numEntries(); symi++) {
        if (symbols[symi].st_bind != STB_LOCAL             // skip local symbols
        && symbols[symi].st_section                        // skip external symbols
        && !(symbols[symi].st_other & STV_HIDDEN)) {       // skip hidden symbols
            symbolTranslate[symi].preserve = 1;            // mark public symbols
        }
        if (debugOptions > 0 && symbols[symi].st_section) {// preserve local symbol names if debugOptions
            // (external unreferenced symbols are still discarded to avoid linking unused library functions)
            symbolTranslate[symi].preserve = 1;            
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
    CDynamicArray<ElfFwcSym> symbols2;
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

// make hexadecimal code file
CFileBuffer & CELF::makeHexBuffer() {
    CFileBuffer hexbuf;                          // temporary output buffer
    CMemoryBuffer databuffer;                    // temporary buffer with binary data
    char string[64];                             // temporary text string
    // Write date and time. 
    time_t time1 = time(0);
    char * timestring = ctime(&time1);
    if (timestring) {
        for (char *c = timestring; *c; c++) {    // Remove terminating '\n' in timestring
            if (*c < ' ') *c = 0;
        }
    } 
    sprintf(string, "// Hexfile %s, date %s\n", cmd.getFilename(cmd.inputFile), timestring);    hexbuf.push(string, (uint32_t)strlen(string));
    uint32_t wordsPerLine = cmd.maxLines;        // number of 32-bit words per line in hex file
    int32_t bytesPerLine = wordsPerLine * 4;     // number of bytes per line in hex file

    // Find program headers
    ElfFwcEhdr fileHeader = get<ElfFwcEhdr>(0);
    uint32_t nProgramHeaders = fileHeader.e_phnum;
    uint32_t programHeaderSize = fileHeader.e_phentsize;
    uint32_t programHeaderOffset = (uint32_t)fileHeader.e_phoff;
    ElfFwcPhdr pHeader;
    for (uint32_t ph = 0; ph < nProgramHeaders; ph++) {         // loop through program headers
        pHeader = get<ElfFwcPhdr>(programHeaderOffset + ph * programHeaderSize); // program header
        uint32_t sectionSize = uint32_t(pHeader.p_filesz);      // size in file
        uint32_t sectionMemSize = uint32_t(pHeader.p_memsz);    // size in memory
        // write comment
        sprintf(string, "// Section %i, size %i\n// %i words per line\n", ph, sectionMemSize, wordsPerLine);
        hexbuf.push(string, (uint32_t)strlen(string));

        // get data from section
        uint32_t os = uint32_t(pHeader.p_offset);                // file offset
        // round up size to multiple of bytesPerLine
        sectionMemSize = (sectionMemSize + bytesPerLine - 1) & -bytesPerLine;
        // copy section data to databuffer
        databuffer.clear();
        databuffer.push(buf() + os, sectionMemSize);
        databuffer.setDataSize(sectionMemSize);
        // line loop
        for (uint32_t L = 0; L < sectionSize; L += bytesPerLine) {
            // word loop backwords for big endian order
            for (int32_t W = wordsPerLine-1; W >= 0; W--) {
                uint32_t data = databuffer.get<uint32_t>(L + W * 4);
                sprintf(string, "%08X", data);
                hexbuf.push(string, (uint32_t)strlen(string));
            }
            hexbuf.push("\n", 1); // end of line
        }
    }
    hexbuf >> *this;  // replace parent buffer (ELF file is lost)
    return *this;
}

// Write a link map
void CELF::makeLinkMap(FILE * stream) {
    // Find program headers
    //fprintf(stream, "\n\nLink map:");
    uint32_t nProgramHeaders = fileHeader.e_phnum;
    uint32_t programHeaderSize = fileHeader.e_phentsize;
    if (nProgramHeaders && programHeaderSize <= 0) err.submit(ERR_ELF_RECORD_SIZE);
    uint32_t programHeaderOffset = (uint32_t)fileHeader.e_phoff;
    ElfFwcPhdr pHeader;           // program header
    ElfFwcShdr sheader;           // section header
    const char * basename = "";   // name of base pointer
    const char * secname = "";    // name of section
    const char * modname = "";    // name of module
    uint64_t codesize = 0;        // size of code and const data
    uint64_t datasize = 0;        // max distance of data from datap

    // print table header
    fprintf(stream, "\n%-9s %-20s %-20s %-10s %-10s %-8s %s", "base", "section", "module", "start", "size", "align", "attributes");

    for (uint32_t i = 0; i < nProgramHeaders; i++) {
        pHeader = get<ElfFwcPhdr>(programHeaderOffset);
        if (pHeader.p_type & PT_LOAD) {
            // get base pointer
            switch (pHeader.p_flags & SHF_BASEPOINTER) {
            case SHF_IP: basename = "ip"; break;
            case SHF_DATAP: basename = "datap"; break;
            case SHF_THREADP: basename = "threadp"; break;
            default: basename = "?"; break;
            }
            // get name of first section
            secname = "";
            uint32_t sc = (uint32_t)pHeader.p_paddr;
            if (sc < nSections) {
                sheader = sectionHeaders[sc];
                if (sheader.sh_flags == pHeader.p_flags && sheader.sh_name < secStringTableLen) {
                    secname = secStringTable + sheader.sh_name;
                }
            }
            // loop through sections under this program header
            for (; sc < nSections; sc++) {
                sheader = sectionHeaders[sc];
                if (sheader.sh_flags != pHeader.p_flags) break; // stop when section does not belong to this program header

                // section name
                if (sheader.sh_name < secStringTableLen) secname = secStringTable + sheader.sh_name;
                else secname = "";

                // module name
                if (sheader.sh_module < secStringTableLen) modname = secStringTable + sheader.sh_module;
                else modname = ""; 
                // write line for section header
                fprintf(stream, "\n%-9s %-20s %-20s 0x%-8X 0x%-8X 0x%-6X ", basename, secname, modname, (uint32_t)sheader.sh_addr, (uint32_t)sheader.sh_size, 1 << sheader.sh_align);
                if (pHeader.p_flags & SHF_READ)  fprintf(stream, "read ");
                if (pHeader.p_flags & SHF_WRITE) fprintf(stream, "write ");
                if (pHeader.p_flags & SHF_EXEC)  fprintf(stream, "execute "); 
                if (sheader.sh_type == SHT_NOBITS) fprintf(stream, "uninititalized");
            }
            // write total
            fprintf(stream, "\n%-9s %-20s %-20s 0x%-8X 0x%-8X 0x%-6X ", basename, "total:", "", 
                (uint32_t)pHeader.p_vaddr, (uint32_t)pHeader.p_memsz, 1 << pHeader.p_align);
            if (pHeader.p_flags & SHF_READ)  fprintf(stream, "read ");
            if (pHeader.p_flags & SHF_WRITE) fprintf(stream, "write ");
            if (pHeader.p_flags & SHF_EXEC)  fprintf(stream, "execute "); 
            fprintf(stream, "\n");

            // find required codesize and datasize
            if (pHeader.p_flags & SHF_IP) {
                uint64_t s = pHeader.p_vaddr + pHeader.p_memsz;
                if (s > codesize) codesize = s;
            }
            else if (pHeader.p_flags & SHF_DATAP) {
                int64_t t = fileHeader.e_datap_base - pHeader.p_vaddr;
                if (t > (int64_t)datasize) datasize = t;
                int64_t u = pHeader.p_vaddr + pHeader.p_memsz - fileHeader.e_datap_base;
                if (u > (int64_t)datasize) datasize = u;
            }
            else if (pHeader.p_flags & SHF_THREADP) {
                int64_t t = fileHeader.e_threadp_base - pHeader.p_vaddr;
                if (t > (int64_t)datasize) datasize = t;
                int64_t u = pHeader.p_vaddr + pHeader.p_memsz - fileHeader.e_threadp_base;
                if (u > (int64_t)datasize) datasize = u;
            }
        }
        programHeaderOffset += programHeaderSize;
    }
    fprintf(stream, "\nip_base:  0x%X, datap_base: 0x%X, threadp_base: 0x%X, entry_point: 0x%X", 
        (uint32_t)fileHeader.e_ip_base, (uint32_t)fileHeader.e_datap_base, (uint32_t)fileHeader.e_threadp_base, (uint32_t)fileHeader.e_entry);
    fprintf(stream, "\ncodesize: 0x%llX, datasize: 0x%llX", codesize, datasize);
}

// Reset everything
void CELF::reset() {
    nSections = 0;
    moduleName = 0;
    library = 0;
    relinkable = false;
    symbols.setSize(0);
    sectionHeaders.setSize(0);
    programHeaders.setSize(0);
    relocations.setSize(0);
    stringBuffer.setSize(0);
    dataBuffer.setSize(0);
    moduleNames.setSize(0);
    libraryNames.setSize(0);
    setSize(0);
}

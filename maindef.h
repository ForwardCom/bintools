/****************************  maindef.h   **********************************
* Author:        Agner Fog
* Date created:  2017-04-17
* Last modified: 2023-01-01
* Version:       1.12
* Project:       Binary tools for ForwardCom instruction set
* Module:        maindef.h
* Original location: https://github.com/forwardcom/binutils
* License: GPL-3.0. https://www.gnu.org/licenses/#GPL
* Copyright 2016-2023 by Agner Fog
*
* Description:
* Header file for type definitions and other main definitions.
*****************************************************************************/
#pragma once 

// Program version
#define FORWARDCOM_VERSION         1
#define FORWARDCOM_SUBVERSION      12


// Get high part of a 64-bit integer
static inline uint32_t highDWord (uint64_t x) {
   return (uint32_t)(x >> 32);
}

// Check if compiling on big-endian machine 
// (__BIG_ENDIAN__ may not be defined even on big endian systems, so this check is not 
// sufficient. A further check is done in CheckEndianness() in main.cpp)
#if defined(__BIG_ENDIAN__) && (__BIG_ENDIAN__ != 4321)
   #error This machine has big-endian memory organization. Program will not work
#endif

// Max file name length
const int MAXFILENAMELENGTH =       256;

// File types 
const int FILETYPE_ELF =              3;         // x86 ELF file
const int FILETYPE_FWC =           0x10;         // ForwardCom ELF file
const int FILETYPE_FWC_EXE =       0x11;         // Executable ForwardCom ELF file
const int FILETYPE_FWC_LIB =       0x20;         // ForwardCom library file
const int FILETYPE_FWC_HEX =       0x40;         // Executable code in hexadecimal format for loader ROM
const int FILETYPE_ASM =          0x100;         // Disassembly output
const int FILETYPE_LIBRARY =     0x1000;         // UNIX-style library/archive


// Define constants for symbol scope
const int S_LOCAL =    0;                        // Local symbol. Accessed only internally
const int S_PUBLIC =   1;                        // Public symbol. Visible from other modules
const int S_EXTERNAL = 2;                        // External symbol. Defined in another module


// Macro to calculate the size of an array
#define TableSize(x) ((int)(sizeof(x)/sizeof(x[0])))

// template to zero all elements of an object
template <typename T>
void zeroAllMembers(T & x) {
    memset(&x, 0, sizeof(T));
}

// Structures and functions used for lookup tables:

// Structure of integers and char *, used for tables of text strings
struct SIntTxt {
   uint32_t a;
   const char * b;
};

// Translate integer value to text string by looking up in table of SIntTxt.
// Parameters: p = table, n = length of table, x = value to find in table
static inline char const * lookupText(SIntTxt const * p, int n, uint32_t x) {
   for (int i=0; i<n; i++, p++) {
      if (p->a == x) return p->b;
   }
   // Not found
   static char utext[32];
   sprintf(utext, "unknown(0x%X)", x);
   return utext;
}

// Macro to get length of text list and call LookupText
#define Lookup(list,x)  lookupText(list, sizeof(list)/sizeof(list[0]), x)

// Bit scan reverse. Returns floor(log2(x)), 0 if x = 0
uint32_t bitScanReverse(uint64_t x);

// // Bit scan forward. Returns index to the lowest set bit, 0 if x = 0
uint32_t bitScanForward(uint64_t x);

// Convert 32 bit time stamp to string
const char * timestring(uint32_t t);

// Convert half precision floating point number to single precision
float half2float(uint32_t half, bool supportSubnormal = true);

// Convert floating point number to half precision
uint16_t float2half(float x, bool supportSubnormal = true);
uint16_t double2half(double x, bool supportSubnormal = true);

/****************************  containers.h   ********************************
* Author:        Agner Fog
* Date created:  2006-07-15
* Last modified: 2018-02-28
* Version:       1.10
* Project:       Binary tools for ForwardCom instruction set
* Module:        containers.h
* Description:
* Header file for container classes and dynamic memory allocation
*
* Copyright 2006-2020 GNU General Public License http://www.gnu.org/licenses
*****************************************************************************/

/*****************************************************************************
This header file declares various container classes for dynamic allocation
of memory for files and other types of data with unpredictable sizes.
These classes have private access to the memory buffer in order to prevent 
memory leaks. It is important to use these classes for all dynamic memory
allocation.

The class CMemoryBuffer and its descendants are used for many purposes of
storage of data with a size that is not known in advance. CMemoryBuffer
allows the size of its data to grow when new data are appended with the
Push() member function.

Several classes are derived from CMemoryBuffer:

The template class CDynamicArray<> is used as a dynamic array where all
elements have the same type. It cannot be used for types that have non-
default constructors or destructors.

The class CFileBuffer is used for reading, writing and storing files.

Other classes can be derived from these to add more properties or functionality.

It is possible to transfer a data buffer from one of these buffers to another,
using the operator

       A >> B

where A and B are both objects of classes that descend from CMemoryBuffer or
CFileBuffer. This operator transfers ownership of the allocated data buffer
from A to B, so that A is empty after the tranfer. This makes sure that a 
memory buffer is always owned by one, and only one, object. Any data owned
by B before the transfer is deallocated. 
The opposite operator B << A does the same thing.

The >> operator can be used when we want to do something to a data buffer
that requires a specialized class. The data buffer can be transferred from 
the object that owns it to an object of the specialized class and 
transferred back again to the original owner when the object of the 
specialized class has done its job. The >> operator transfers the data and 
properties of CMemoryBuffer or CFileBuffer, but not the additional properties
of other classes derived from these.

You may say that these classes have a chameleonic nature:
You can change the nature of a piece of data owned by an object by 
transferring it to an object of a different class. This couldn't be done
by traditional polymorphism because it is not possible to change the class
of an object after it is created.

The container class CMemoryBuffer is useful for storing data of mixed types.
Data of arbitrary type can be accessed by Get<type>(offset) or by
Buf() + offset.

The container class template CDynamicArray is useful for storing data of
the same type.

Warning: 
It is not safe to make pointers or references to data inside one of these 
container classes because the internal buffer may be re-allocated when the 
size grows. Such pointers will work only as long as the size of the container
is unchanged. It is safer to address data inside the buffer by their index
or offset relative to the buffer.

*****************************************************************************/

#pragma once

class CMemoryBuffer;                             // Declared below
class CFileBuffer;                               // Declared below

void operator >> (CMemoryBuffer & a, CMemoryBuffer & b); // Transfer ownership of buffer and other properties
void operator >> (CFileBuffer & a, CFileBuffer & b);     // Transfer ownership of buffer and other properties

// Class CMemoryBuffer makes a container for arbitrary data, which can grow as new data are added.
class CMemoryBuffer {
public:
   CMemoryBuffer();                              // Constructor
   ~CMemoryBuffer();                             // Destructor
   void setSize(uint32_t size);                  // Allocate buffer of specified size
   void setDataSize(uint32_t size);              // Set data size and fill any new data with zeroes
   void clear();                                 // De-allocate buffer
   void zero();                                  // Set all contents to zero without changing data size
   uint32_t dataSize() const {return data_size;};// Get file data size
   uint32_t bufferSize() const {return buffer_size;};// Get buffer size
   uint32_t numEntries() const {return num_entries;};// Get number of entries
   uint32_t push(void const* obj, uint32_t size);// Add object to buffer, return offset
   uint32_t pushString(char const * s);          // Add ASCIIZ string to buffer, return offset
   uint32_t getLastIndex() const;                // Index of last object pushed (zero-based)
   void align(uint32_t a);                       // Align next entry to address divisible by a. must be a power of 2
   int8_t * buf() {return buffer;};              // Access to buffer
   int8_t const * buf() const {return buffer;};  // Access to buffer, const
   template <class TX> TX & get(uint32_t offset) { // Get object of arbitrary type from buffer
      if (offset >= data_size) {
          err.submit(ERR_CONTAINER_INDEX); offset = 0;} // Offset out of range
      return *(TX*)(buffer + offset);}
   char * getString(uint32_t offset) {           // Get string from offset returned from pushString
       return (char *)(buffer + offset);
   }
   void copy(CMemoryBuffer const & b);           // Make a copy of whole buffer
private:
   CMemoryBuffer(CMemoryBuffer&);                // Make private copy constructor to prevent simple copying
   CMemoryBuffer & operator = (CMemoryBuffer const&);// Make assignment operator to prevent simple copying
   int8_t * buffer;                              // Buffer containing binary data. To be modified only by SetSize and operator >>
   uint32_t buffer_size;                         // Size of allocated buffer ( > DataSize)
protected:
   uint32_t num_entries;                         // Number of objects pushed
   uint32_t data_size;                           // Size of data, offset to vacant space
   friend void operator >> (CMemoryBuffer & a, CMemoryBuffer & b); // Transfer ownership of buffer
   friend void operator >> (CFileBuffer & a, CFileBuffer & b);     // Transfer ownership of buffer
};

inline void operator << (CMemoryBuffer & b, CMemoryBuffer & a) {a >> b;} // Same as operator << above
inline void operator << (CFileBuffer & b, CFileBuffer & a) {a >> b;} // Same as operator << above

// Class CFileBuffer is used for storage of input and output files
class CFileBuffer : public CMemoryBuffer {
public:
   CFileBuffer();                                // Default constructor
   //CFileBuffer(uint32_t filename);               // Constructor
   void read(const char * filename, int ignoreError = 0);               // Read file into buffer
   void write(const char * filename);                                 // Write buffer to file
   int  getFileType();                           // Get file format type
   void setFileType(int type);                   // Set file format type
   void reset();                                 // Set all members to zero
   static char const * getFileFormatName(int fileType); // Get name of file format type
   int wordSize;                                 // Segment word size (16, 32, 64)
   int fileType;                                 // Object file type
   int executable;                               // File is executable
   int machineType;                              // Machine type, x86 or ForwarCom
};


// Class CTextFileBuffer is used for building text files
class CTextFileBuffer : public CFileBuffer {
public:
   CTextFileBuffer();                            // Constructor
   uint32_t put(const char * text);              // Write text string to buffer without terminating zero
   void put(const char character);               // Write single character to buffer
   uint32_t putStringN(const char * s, uint32_t len);// Write string to buffer, add terminating zero
   void newLine();                               // Add linefeed
   void tabulate(uint32_t i);                    // Insert spaces until column i
   int  lineType;                                // 0 = DOS/Windows linefeeds, 1 = UNIX linefeeds
   void putDecimal(int32_t x, int IsSigned = 0); // Write decimal number to buffer
   void putHex(uint8_t  x, int ox = 1);          // Write hexadecimal number to buffer
   void putHex(uint16_t x, int ox = 1);          // Write hexadecimal number to buffer
   void putHex(uint32_t x, int ox = 1);          // Write hexadecimal number to buffer
   void putHex(uint64_t x, int ox = 1);          // Write hexadecimal number to buffer
   void putFloat16(uint16_t x);                  // Write half precision floating point number to buffer
   void putFloat(float x);                       // Write floating point number to buffer
   void putFloat(double x);                      // Write floating point number to buffer
   uint32_t getColumn() {return column;}         // Get column number
protected:
   uint32_t column;                              // Current column
};


// Class CDynamicArray<> is used for a variable-size array with elements of the same type
// Note: This will not work correctly if the contained type has non-default constructors or destructors.
// Sorting and searching is supported if operator < is defined for the contained type.
template <class TX>
class CDynamicArray : public CMemoryBuffer {
public:
    // Allocate space for n of entries. Elements will be zero only if the array was empty before
    void setNum(uint32_t n) {
        setSize(n * (uint32_t)sizeof(TX));
        num_entries = n; data_size = n * (uint32_t)sizeof(TX);}

    // Add object to buffer. Return index
    uint32_t push(TX const& obj) {
        CMemoryBuffer::push(&obj, (uint32_t)sizeof(TX));
        return num_entries - 1;
    }

    // Add multiple objects. Return total number
    uint32_t pushBig(TX const * obj, uint32_t sizeInBytes) {
        CMemoryBuffer::push(obj, sizeInBytes);
        num_entries += sizeInBytes / sizeof(TX) - 1;
        return num_entries;
    }

    // Read or write existing elements. Cannot be used for adding new elements
    TX & operator [] (uint32_t i) {
        uint64_t ii = (uint64_t)i * sizeof(TX);
        if (ii >= dataSize()) {
            err.submit(ERR_CONTAINER_INDEX); ii = 0;
        }
        return get<TX>((uint32_t)ii);}

    // Remove latest added object when buffer is used as stack
    TX pop() {
        TX temp;
        if (num_entries == 0) {  // stack is empty. return zero object
            zeroAllMembers(temp);
        }
        else {
            temp = (*this)[num_entries-1];
            data_size -= sizeof(TX);
            num_entries--;
        }
        return temp;
    }

    // Sort list in ascending order. Operator < must be defined for record type TX
    void sort() {
        // Bubble sort:
        TX temp, *p1, *p2;
        int32_t j, n;
        bool swapped;
        n = num_entries - 1;
        do {
            swapped = false;
            for (j = 0; j < n; j++) {
                p1 = (TX*)(buf() + j * sizeof(TX));
                p2 = (TX*)(buf() + j * sizeof(TX) + sizeof(TX));
                if (*p2 < *p1) {                           // Swap adjacent records
                    temp = *p1;  *p1 = *p2;  *p2 = temp;  swapped = true;
                }
            }
            n--;
        } while (swapped);                                 // Early out if already mostly sorted
    }

    int32_t findFirst(TX const & x) {            
        // Finds matching record and returns index to the first matching record
        // Important: The list must be sorted first
        // Returns a negative value if not found
        uint32_t a = 0;                                    // Start of search interval
        uint32_t b = num_entries;                          // End of search interval + 1
        uint32_t c = 0;                                    // Middle of search interval                                                     
        if (num_entries > 0x7FFFFFFF) {err.submit(ERR_CONTAINER_OVERFLOW); return 0x80000000;} // Size overflow
                       
        while (a < b) {                                    // Binary search loop:
            c = (a + b) / 2;
            if ((*this)[c] < x) {
                a = c + 1;}
            else {
                b = c;}
        }
        if (a == num_entries || x < (*this)[a]) a |= 0x80000000; // Not found
        return (int32_t)a;
    }

    int32_t findUnsorted(TX const & x) {            
        // Finds matching record and returns index to the first matching record
        // Use this if the list is not sorted, or sort the list first and use findFirst
        // Returns a negative value if not found
        uint32_t a = 0;
        for (a = 0; a < num_entries; a++) {
            if ((*this)[a] == x) return a;
        }
        return -1;
    } 

    uint32_t findAll(uint32_t * firstIndex, TX const & x) {
        // Returns the number of records that are equal to x.
        // X is regarded as equal to y if !(x < y) && !(y < x)
        // Important: The list must be sorted first.
        // firstIndex (if not null) gets the index to the first matching record
        int32_t index = findFirst(x);                      // finds first matching record
        if (index < 0) return 0;                           // None found
        if (firstIndex) *firstIndex = (uint32_t)index;     // Save index to first matching record
        uint32_t n = 1;                                    // Count matching records
        for (uint32_t i = index+1; i < num_entries; i++) {
            if (x < (*this)[i]) break;
            n++;
        }
        return n;
    }

    uint32_t addUnique(TX const& x) {
        // Add object x to the list only if an object equal to x is not already in the list
        // Important: The list must be sorted first. The list will remain sorted after the addition of x.
        // The return value is the index of the inserted object or a preexisting object equal to x.
        // The indexes of pre-existing objects above the inserted object are incremented.
        int32_t index = findFirst(x);                      // Find where to insert x
        if (index < 0) {
            index &= 0x7FFFFFFF;                           // Remove "not found" bit to recover index
            uint32_t recordsToMove = num_entries - (uint32_t)index; // Number of records to move
            setNum(num_entries + 1);                        // Make space for one more record                                
            if (recordsToMove > 0) {                       // Move subsequent entries up one place
                memmove(buf() + index * sizeof(TX) + sizeof(TX),
                    buf() + index * sizeof(TX),
                    recordsToMove * sizeof(TX));
            }
            // Insert x at index position
            (*this)[index] = x;
        }
        return (uint32_t)index;                            // Return index to symbol
    }
};


// CMetaBuffer is a buffer of buffers. The size can be set only once, it cannot be resized
// The elements of type B may have constructors and destructors
template <class B>
class CMetaBuffer {
public:
    CMetaBuffer<B>() {                 // constructor
        num = 0;  p = 0;
    }
    ~CMetaBuffer<B>() {                // destructor
        if (p) delete[] p;             // call destructors and deallocate
    }
    void setSize(uint32_t n) {         // allocate memory for n elements
        if (num) {
            err.submit(ERR_MEMORY_ALLOCATION);  return;  // re-allocation not allowed
        }
        p = new B[n];                  // allocate, call constructors
        if (p) {
            num = n;
        }
        else {
            err.submit(ERR_MEMORY_ALLOCATION);
        }
    }
    uint32_t numEntries() const {
        return num;
    };
    B & operator [] (uint32_t i) {     // access element number i
        if (i >= num) {        
            err.submit(ERR_CONTAINER_INDEX); i = 0; // index out of range
        }
        return p[i];
    }
protected:
    uint32_t num;                      // number of elements
    B * p;                             // pointer to array of buffers
};
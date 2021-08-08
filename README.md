This repository contains the binary tools for the ForwardCom instruction set:
assembler, disassembler, linker, library manager, emulator

This can be compiled and run on almost any little-endian platform.
A compiled exe file for windows is included. For Linux and other platforms: use make -f forw.make to compile.


Files included: |  Description
--- | ---
forwardcom_sourcecode_documentation.odt | Documentation of the source code
*.cpp    |      C++ source code   
*.h      |      C++ header files   
forw.exe  |     Windows executable, 64-bit  
forw.make  |     Makefile for Gnu C++ compiler  
instruction_list.csv | List of instructions as comma separated file. Made from instruction_list.ods  
forwardcom.pdf | Manual (from ForwardCom/manual repository)  
forw.vcxproj forw.sln forw.vcxproj.filters | Project files for MS Visual Studio  
resp.txt     |  Response file used during debugging in Visual Studio  




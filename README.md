This repository contains the binary tools for the ForwardCom instruction set:
assembler, disassembler, linker, library manager, emulator

This can be compiled and run on almost any little-endian platform.
A compiled exe file for windows is included. For Linux and other platforms: use the makefile to compile everything.


Files included: |  Description
--- | ---
*.cpp    |      C++ source code   
*.h      |      C++ header files   
forw.exe  |     Windows executable, 64-bit  
makefile  |     Makefile for Gnu C++ compiler  
instruction_list.ods | List of instructions  
instruction_list.csv | List of instructions as comma separated file. Made from instruction_list.ods  
forwardcom.pdf | Manual (from ForwardCom/manual repository)  
forw.vcxproj forw.sln forw.vcxproj.filters | Project files for MS Visual Studio  
resp.txt     |  Response file used during debugging in Visual Studio  




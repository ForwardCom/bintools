// assembly file: test.as

data section read write datap
int32 mydata = 4               // a static variable
data end

code section execute

// this function returns mydata + 1
_myfunction function public
int32 r0 = [mydata]
int32 r0++
return
_myfunction end

code end
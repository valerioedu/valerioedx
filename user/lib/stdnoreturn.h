#ifndef STDNORETURN_H
#define STDNORETURN_H

#ifndef __cplusplus
    #define noreturn _Noreturn
#else
    #define noreturn [[noreturn]]
#endif

#endif
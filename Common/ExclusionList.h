
#ifndef REBAR_EXCLUSION_LIST_H
#define REBAR_EXCLUSION_LIST_H

#include <stdint.h>

#if defined(_MSC_VER) // Microsoft Visual C++
#define PACKED_STRUCT \
    __pragma(pack(push, 1)) struct __pragma(pack(pop))
#elif defined(__GNUC__) || defined(__clang__) // GCC or Clang
#define PACKED_STRUCT   struct __attribute__((packed))
#else
#error "Unknown compiler: please define PACKED_STRUCT for this compiler."
#endif

#define EXCLUSION_LIST_VERSION  sizeof(ExclusionList)

typedef PACKED_STRUCT
{
    uint16_t vid;
    uint16_t pid;
} ExclusionListEntry;

typedef PACKED_STRUCT
{
    uint32_t version;
    uint32_t count;
    ExclusionListEntry entries[1];
} ExclusionList;

#endif // REBAR_EXCLUSION_LIST_H

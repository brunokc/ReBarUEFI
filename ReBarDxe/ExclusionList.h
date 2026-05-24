
#ifndef REBAR_DXE_EXCLUSION_LIST_H
#define REBAR_DXE_EXCLUSION_LIST_H

#if defined(_MSC_VER) // Microsoft Visual C++
#define PACKED_STRUCT \
    __pragma(pack(push, 1)) struct __pragma(pack(pop))
#elif defined(__GNUC__) || defined(__clang__) // GCC or Clang
#define PACKED_STRUCT   struct __attribute__((packed))
#else
#error "Unknown compiler: please define PACKED_STRUCT for this compiler."
#endif

typedef PACKED_STRUCT
{
    INT16 vid;
    INT16 pid;
} ExclusionListEntry;

typedef PACKED_STRUCT
{
    UINT32 version;
    UINT32 count;
    ExclusionListEntry entries[];
} ExclusionList;

EFI_STATUS LoadExclusionList();

bool IsDeviceInExclusionList(UINT16 vid, UINT16 did);

#endif // REBAR_DXE_EXCLUSION_LIST_H

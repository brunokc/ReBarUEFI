
#ifndef REBAR_DXE_EXCLUSION_LIST_H
#define REBAR_DXE_EXCLUSION_LIST_H

#include "Common/ExclusionList.h"

EFI_STATUS LoadExclusionList();

bool IsDeviceInExclusionList(UINT16 vid, UINT16 did);

#endif // REBAR_DXE_EXCLUSION_LIST_H

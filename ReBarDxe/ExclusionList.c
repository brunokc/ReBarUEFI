
#include <Uefi.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>

#include "Common/Common.h"
#include "ExclusionList.h"

extern GUID reBarStateGuid;

static ExclusionList* exclusionList = NULL;

EFI_STATUS LoadExclusionList()
{
    EFI_STATUS status;
    UINTN bufferSize = 0;
    ExclusionList* list = NULL;

    // Get the size of the exclusion list variable
    status = gRT->GetVariable(VAR_REBAR_EXCLUSION_LIST_WSTR, &reBarStateGuid, NULL, &bufferSize, NULL);
    if (status != EFI_BUFFER_TOO_SMALL) {
        return status;
    }

    // Allocate memory for the exclusion list
    list = (ExclusionList*)AllocateRuntimeZeroPool(bufferSize);
    if (list == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    // Read the exclusion list variable
    status = gRT->GetVariable(VAR_REBAR_EXCLUSION_LIST_WSTR, &reBarStateGuid, NULL, &bufferSize, list);
    if (EFI_ERROR(status)) {
        FreePool(list);
        return status;
    }

    exclusionList = list;
    return EFI_SUCCESS;
}

bool IsDeviceInExclusionList(UINT16 vid, UINT16 did)
{
    if (exclusionList == NULL) {
        return false;
    }

    for (UINT32 i = 0; i < exclusionList->count; i++) {
        if (exclusionList->entries[i].vid == vid && exclusionList->entries[i].pid == did) {
            return true;
        }
    }

    return false;
}

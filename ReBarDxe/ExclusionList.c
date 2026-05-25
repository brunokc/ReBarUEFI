
#include <Uefi.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>

#include "Common/Common.h"
#include "Common/ExclusionList.h"

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

    if (list->version != EXCLUSION_LIST_VERSION) {
        DEBUG((DEBUG_INFO, "ReBarDXE: incompatible NVRAM exclusion list version %d (expected version %d)\n",
            list->version, EXCLUSION_LIST_VERSION));
        FreePool(list);
        return EFI_UNSUPPORTED;
    }

    uint32_t expectedSize = sizeof(ExclusionList) + (list->count - 1) * sizeof(ExclusionListEntry);
    if (bufferSize != expectedSize) {
        DEBUG((DEBUG_INFO, "ReBarDXE: incompatible NVRAM exclusion list size %d (expected size %d)\n",
            bufferSize, expectedSize));
        FreePool(list);
        return EFI_UNSUPPORTED;
    }

    exclusionList = list;
    return EFI_SUCCESS;
}

bool IsDeviceInExclusionList(UINT16 vid, UINT16 pid)
{
    if (exclusionList == NULL) {
        return false;
    }

    for (UINT32 i = 0; i < exclusionList->count; i++) {
        if (exclusionList->entries[i].vid == vid && exclusionList->entries[i].pid == pid) {
            return true;
        }
    }

    return false;
}

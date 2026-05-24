#ifndef COMMON_H
#define COMMON_H

#define QUOTE(x)    #x
#define STR(x)      QUOTE(x)

#define WIDEN2(x)   L##x
#define WIDEN(x)    WIDEN2(x)
#define WSTR(x)     WIDEN(STR(x))

#define VENDOR_GUID     A3C5B77A-C88F-4A93-BF1C-4A92A32C65CE
#define VENDOR_GUID_STR STR(VENDOR_GUID)

#define VAR_REBAR_STATE         ReBarState
#define VAR_REBAR_STATE_WSTR    WSTR(VAR_REBAR_STATE)
#define VAR_REBAR_EXCLUSION_LIST        ReBarExclusionList
#define VAR_REBAR_EXCLUSION_LIST_WSTR   WSTR(VAR_REBAR_EXCLUSION_LIST)

#endif // COMMON_H

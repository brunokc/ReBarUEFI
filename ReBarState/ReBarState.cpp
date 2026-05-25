/*
Copyright (c) 2022-2023 xCuri0 <zkqri0@gmail.com>
SPDX-License-Identifier: MIT
*/
#include <iostream>
#include <string_view>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <memory>
#include <new>

#ifdef _MSC_VER
#include <Windows.h>
#else
#include <unistd.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <string.h>
#endif

#include "Common/Common.h"
#include "Common/ExclusionList.h"

#define VARIABLE_ATTRIBUTE_NON_VOLATILE 0x00000001
#define VARIABLE_ATTRIBUTE_BOOTSERVICE_ACCESS 0x00000002
#define VARIABLE_ATTRIBUTE_RUNTIME_ACCESS 0x00000004

#define VALUE_NOT_PRESENT	UINT8_MAX

// Exclusion list support

void DeleteExclusionListBuffer(ExclusionList* list)
{
	delete[] reinterpret_cast<uint8_t*>(list);
}

void NullDeleter(ExclusionList* list)
{
}

struct SizeInBytes {};
constexpr auto sizeInBytes = SizeInBytes{};

struct ExclusionListPtr : std::unique_ptr<ExclusionList, decltype(&DeleteExclusionListBuffer)>
{
	using base = std::unique_ptr<ExclusionList, decltype(&DeleteExclusionListBuffer)>;

	ExclusionListPtr() : base(nullptr, DeleteExclusionListBuffer)
	{
	}

	ExclusionListPtr(ExclusionList* list) : base(list, NullDeleter)
	{
		// Constructor to wrap a non-owning buffer, so we can't free the memory during destruction.
	}

	ExclusionListPtr(uint32_t entriesCount) : ExclusionListPtr(GetBufferSize(entriesCount), sizeInBytes)
	{
	}

	ExclusionListPtr(uint32_t bufferSize, const SizeInBytes&) :
		base(
			reinterpret_cast<ExclusionList*>(new (std::nothrow) uint8_t[bufferSize]),
			DeleteExclusionListBuffer)
	{
	}

	static auto Empty()
	{
		static ExclusionList emptyList = {
			.version = EXCLUSION_LIST_VERSION,
			.count = 0,
			.entries = {
				{
					.vid = 0,
					.did = 0,
				},
			},
		};
		return ExclusionListPtr{&emptyList};
	}

	constexpr static uint32_t GetBufferSize(uint32_t entriesCount)
	{
		return sizeof(ExclusionList) + (entriesCount - 1) * sizeof(ExclusionListEntry);
	}

	uint32_t GetBufferSize() const
	{
		return GetBufferSize(get()->count);
	}

	bool CopyEntriesFrom(const ExclusionListPtr& list)
	{
		if (list->count > get()->count) {
			return false;
		}
		std::memcpy(get()->entries, list->entries, list->count * sizeof(ExclusionListEntry));
		get()->count = list->count;
		return true;
	}
};

// Windows
#ifdef _MSC_VER
bool CheckPriviledge()
{
	DWORD len;
	HANDLE hTok;
	TOKEN_PRIVILEGES tokp;

	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
		&hTok)) {
		return FALSE;
	}

	LookupPrivilegeValue(NULL, SE_SYSTEM_ENVIRONMENT_NAME, &tokp.Privileges[0].Luid);
	tokp.PrivilegeCount = 1;
	tokp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	AdjustTokenPrivileges(hTok, FALSE, &tokp, 0, NULL, &len);

	if (GetLastError() != ERROR_SUCCESS) {
		std::cout << "Failed to obtain SE_SYSTEM_ENVIRONMENT_NAME\n";
		return FALSE;
	}
	std::cout << "Obtained SE_SYSTEM_ENVIRONMENT_NAME\n";
	return TRUE;
}

uint8_t GetState() {
	UINT8 rBarState;
	DWORD rSize;

	const TCHAR name[] = TEXT(VAR_REBAR_STATE_STR);
	const TCHAR guid[] = TEXT("{" VENDOR_GUID_STR "}");

	rSize = GetFirmwareEnvironmentVariable(name, guid, &rBarState, 1);

	if (rSize == 1)
		return rBarState;

	return VALUE_NOT_PRESENT;
}

bool WriteState(uint8_t rBarState) {
	const TCHAR name[] = TEXT(VAR_REBAR_STATE_STR);
	const TCHAR guid[] = TEXT("{" VENDOR_GUID_STR "}");

	const DWORD dwAttributes = VARIABLE_ATTRIBUTE_NON_VOLATILE | VARIABLE_ATTRIBUTE_BOOTSERVICE_ACCESS |
		VARIABLE_ATTRIBUTE_RUNTIME_ACCESS;

	return SetFirmwareEnvironmentVariableEx(name, guid, &rBarState, sizeof(rBarState), dwAttributes) != 0;
}

ExclusionListPtr ReadExclusionList() {
	const TCHAR name[] = TEXT(VAR_REBAR_EXCLUSION_LIST_STR);
	const TCHAR guid[] = TEXT("{" VENDOR_GUID_STR "}");

	// Get the size of the exclusion list variable
	DWORD bufferSize = 0;
	DWORD status = GetFirmwareEnvironmentVariable(name, guid, NULL, &bufferSize);
	if (status != 0 || GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
		std::cout << "Failed to get size of exclusion list variable\n";
		return {};
	}

	auto exclusionList = ExclusionListPtr{bufferSize, sizeInBytes};
	if (!exclusionList) {
		std::cout << "Failed to allocate memory for exclusion list\n";
		return {};
	}

	status = GetFirmwareEnvironmentVariable(name, guid, exclusionList.get(), &bufferSize);
	if (status == 0) {
		std::cout << "Failed to read exclusion list variable\n";
		return {};
	}

	if (exclusionList->version != EXCLUSION_LIST_VERSION) {
		std::cout << "Incompatible NVRAM exclusion list version " << exclusionList->version
			<< " (expected version " << EXCLUSION_LIST_VERSION << ")\n";
		return {};
	}

	uint32_t expectedSize = GetExclusionListBufferSize(exclusionList);
	if (bufferSize != expectedSize) {
		std::cout << "Incompatible NVRAM exclusion list size " << bufferSize
			<< " (expected size " << expectedSize << ")\n";
		return {};
	}

	return exclusionList;
}

bool WriteExclusionList(const ExclusionListPtr& exclusionList) {
	const DWORD dwAttributes = VARIABLE_ATTRIBUTE_NON_VOLATILE | VARIABLE_ATTRIBUTE_BOOTSERVICE_ACCESS |
		VARIABLE_ATTRIBUTE_RUNTIME_ACCESS;

	const TCHAR name[] = TEXT(VAR_REBAR_EXCLUSION_LIST_STR);
	const TCHAR guid[] = TEXT("{" VENDOR_GUID_STR "}");

	DWORD bufferSize = exclusionList.GetBufferSize();
	return SetFirmwareEnvironmentVariableEx(name, guid, exclusionList.get(), bufferSize, dwAttributes) != 0;
}

// Linux
#else

#define REBARPATH 	/sys/firmware/efi/efivars/VAR_REBAR_STATE-VENDOR_GUID
#define REBARPS 	STR(REBARPATH)

struct __attribute__((__packed__)) RebarVar {
	uint32_t attr;
	uint8_t value[1];
};

bool CheckPriviledge() {
	return getuid() == 0;
}

bool RemoveOldVariable(const char* path)
{
	int attr;
	FILE* f = fopen(path, "rb");

	if (f) {
		// remove immuteable flag that linux sets on all unknown efi variables
		ioctl(fileno(f), FS_IOC_GETFLAGS, &attr);
		attr &= ~FS_IMMUTABLE_FL;
		ioctl(fileno(f), FS_IOC_SETFLAGS, &attr);

		if (remove(path) != 0) {
			return false;
		}

		fclose(f);
	}

	return true;
}

uint8_t GetState() {
	RebarVar rebarState = {0};

	FILE* f = fopen(REBARPS, "rb");

	if (!(f && (fread(&rebarState, sizeof(RebarVar), 1, f) == 1))) {
		rebarState.value[0] = VALUE_NOT_PRESENT;
	} else
		fclose(f);

	return rebarState.value[0];
}

bool WriteState(uint8_t rBarState) {
	if (!RemoveOldVariable(REBARPS)) {
		std::cout << "Failed to remove old variable\n";
		return false;
	}

	FILE* f = fopen(REBARPS, "wb");

	RebarVar rVar = {
		.attr = VARIABLE_ATTRIBUTE_NON_VOLATILE | VARIABLE_ATTRIBUTE_BOOTSERVICE_ACCESS |
			VARIABLE_ATTRIBUTE_RUNTIME_ACCESS,
		.value = { rBarState },
	};

	bool success = fwrite(&rVar, sizeof(rVar), 1, f) == 1;

	fclose(f);

	return success;
}

ExclusionListPtr ReadExclusionList() {
	struct stat st = {0};

	if (stat(REBARPS, &st) != 0) {
		std::cout << "Failed to retrieve size of exclusion list variable\n";
		return {};
	}

	// subtract attributes
	uint32_t dataSize = st.st_size - sizeof(uint32_t);

	auto exclusionList = ExclusionListPtr{dataSize, sizeInBytes};
	if (!exclusionList) {
		std::cout << "Failed to allocate memory for exclusion list\n";
		return {};
	}

	FILE* f = fopen(REBARPS, "rb");

	if (f)	{
		bool success = (fread(exclusionList.get(), dataSize, 1, f) == 1);
		fclose(f);

		if (!success) {
			std::cout << "Failed to read exclusion list variable\n";
			return {};
		}
	}

	if (exclusionList->version != EXCLUSION_LIST_VERSION) {
		std::cout << "Incompatible NVRAM exclusion list version " << exclusionList->version
			<< " (expected version " << EXCLUSION_LIST_VERSION << ")\n";
		return {};
	}

	uint32_t expectedSize = ExclusionListPtr::GetBufferSize(exclusionList->count);
	if (dataSize != expectedSize) {
		std::cout << "Incompatible NVRAM exclusion list size " << dataSize
			<< " (expected size " << expectedSize << ")\n";
		return {};
	}

	return exclusionList;
}

bool WriteExclusionList(const ExclusionListPtr& exclusionList) {
	if (!RemoveOldVariable(REBARPS)) {
		std::cout << "Failed to remove old variable\n";
		return false;
	}

	FILE* f = fopen(REBARPS, "wb");

	// Attributes + size of exclusion list
	uint32_t dataSize = sizeof(uint32_t) + exclusionList.GetBufferSize();
	auto rVarBuffer = std::make_unique<uint8_t[]>(dataSize);
	if (!rVarBuffer) {
		std::cout << "Failed to allocate memory for writing exclusion list variable\n";
		return false;
	}
	RebarVar* rVar = reinterpret_cast<RebarVar*>(rVarBuffer.get());

	rVar->attr = VARIABLE_ATTRIBUTE_NON_VOLATILE | VARIABLE_ATTRIBUTE_BOOTSERVICE_ACCESS |
		VARIABLE_ATTRIBUTE_RUNTIME_ACCESS;
	std::memcpy(rVar->value, exclusionList.get(), dataSize - sizeof(uint32_t));

	bool success = (fwrite(rVar, dataSize, 1, f) == 1);

	fclose(f);

	return success;
}

#endif

void usage()
{
	std::cout << "Usage: ReBarState [options]\n";
	std::cout << "\nOptions:\n";
	std::cout << "  -h, --help         Show this help message and exit\n";
	std::cout << "  -s [<size>]        Read current size, or write the size if one is provided (see below)\n";
	std::cout << "  -e                 Dump the vid/did exclusion list\n";
	std::cout << "  -e <vid> <did>     Append a device to the exclusion list (hex VID and DID)\n";
	std::cout << "  -e -c              Clear the exclusion list\n";
	std::cout << "\nSize values:\n";
	std::cout << "       0: ReBar disabled\n";
	std::cout << "   1..31: Maximum BAR size set to 2^x MB\n";
	std::cout << "      32: Unlimited BAR size\n";
	std::cout << "\nNote: Must be run with administrative/root privileges to access UEFI variables.\n";
}

bool handleSize(int argc, char* argv[], int idx)
{
	if (argc > idx + 1 && argv[idx + 1][0] != '-') {
		// Write new size to NVRAM variable
		char* endptr;
		++idx;
		long val = strtol(argv[idx], &endptr, 10);
		if (*endptr != '\0' || val < 0 || val > 32) {
			std::cout << "Invalid size value '" << argv[idx] << "'. Must be 0-32.\n";
			return false;
		}
		uint8_t reBarState = (uint8_t)val;
		if (reBarState == 0)
			std::cout << "Writing value of 0 / Disabled to ReBarState\n";
		else if (reBarState == 32)
			std::cout << "Writing Unlimited to ReBarState\n";
		else
			std::cout << "Writing value of " << +reBarState << " / " << std::pow(2, reBarState) << " MB to ReBarState\n";

		if (WriteState(reBarState)) {
			std::cout << "Successfully wrote ReBarState UEFI variable\n";
			std::cout << "Reboot for changes to take effect\n";
		} else {
			std::cout << "Failed to write ReBarState UEFI variable\n";
#ifdef _MSC_VER
			std::cout << "GetLastError: " << GetLastError() << "\n";
#endif
			return false;
		}
	} else {
		// Read existing value from NVRAM variable
		uint8_t reBarState = GetState();
		if (reBarState == VALUE_NOT_PRESENT)
			std::cout << "ReBarState not set / Disabled\n";
		else if (reBarState == 0)
			std::cout << "ReBarState: " << +reBarState << " / Disabled\n";
		else if (reBarState == 32)
			std::cout << "ReBarState: " << +reBarState << " / Unlimited\n";
		else
			std::cout << "ReBarState: " << +reBarState << " / " << std::pow(2, reBarState) << " MB\n";
	}
	return true;
}

bool parseU16Value(const char* text, uint16_t& out)
{
	char* endptr = nullptr;

	// Base 0 supports decimal, 0x-prefixed hex, and octal.
	long val = strtol(text, &endptr, 0);
	if (*endptr == '\0' && val >= 0 && val <= 0xFFFF) {
		out = (uint16_t)val;
		return true;
	}

	// Also accept unprefixed hex for convenience (e.g. 8086, 10DE).
	val = strtol(text, &endptr, 16);
	if (*endptr == '\0' && val >= 0 && val <= 0xFFFF) {
		out = (uint16_t)val;
		return true;
	}
	return false;
}

bool handleExclusionList(int argc, char* argv[], int idx)
{
	if (argc > idx + 1 && std::string_view(argv[idx + 1]) == "-c") {
		// Clear the list by writing an empty exclusion list
		bool ok = WriteExclusionList(ExclusionListPtr::Empty());
		if (ok)
			std::cout << "Exclusion list cleared\n";
		else {
			std::cout << "Failed to clear exclusion list\n";
			return false;
		}
	} else if (argc > idx + 2 && argv[idx + 1][0] != '-') {
		// Add a new vid/did to the exclusion list
		uint16_t vid;
		if (!parseU16Value(argv[idx + 1], vid)) {
			std::cout << "Invalid VID '" << argv[idx + 1] << "'\n";
			return false;
		}
		uint16_t did;
		if (!parseU16Value(argv[idx + 2], did)) {
			std::cout << "Invalid DID '" << argv[idx + 2] << "'\n";
			return false;
		}

		auto list = ReadExclusionList();
		uint32_t oldCount = list ? list->count : 0;
		uint32_t newCount = oldCount + 1;
		auto newList = ExclusionListPtr{newCount};
		if (!newList) {
			std::cout << "Memory allocation failed\n";
			return false;
		}
		newList->version = EXCLUSION_LIST_VERSION;
		if (list && oldCount > 0)
			newList.CopyEntriesFrom(list);

		// Add new entry
		newList->entries[oldCount] = {
			.vid = vid,
			.did = did,
		};

		bool ok = WriteExclusionList(newList);
		if (ok) {
			std::cout << "Added " << std::hex << std::uppercase
					  << std::setw(4) << std::setfill('0') << (uint16_t)vid
					  << ":" << std::setw(4) << (uint16_t)did
					  << std::dec << " to exclusion list\n";
		} else {
			std::cout << "Failed to write exclusion list\n";
			return false;
		}
	} else {
		// Dump existing exclusion list
		auto list = ReadExclusionList();
		if (!list) {
			std::cout << "Exclusion list is empty or not set\n";
		} else {
			std::cout << "Exclusion list (" << list->count << " entries):\n";
			for (uint32_t i = 0; i < list->count; i++) {
				std::cout << "  [" << std::hex << std::uppercase
						  << std::setw(4) << std::setfill('0') << (uint16_t)list->entries[i].vid
						  << ":" << std::setw(4) << (uint16_t)list->entries[i].did << "]\n";
			}
			std::cout << std::dec;
		}
	}
	return true;
}

int main(int argc, char* argv[])
{
	int ret = 0;
	std::string_view arg;

	std::cout << "ReBarState (c) 2023 xCuri0\n\n";

	if (argc < 2 || std::string_view(argv[1]) == "-h" || std::string_view(argv[1]) == "--help") {
		usage();
		if (argc < 2)
			ret = 1;
		goto exit;
	}

	if (!CheckPriviledge()) {
		std::cout << "Failed to obtain EFI variable access, try running as admin/root\n";
		ret = 1;
		goto exit;
	}

	arg = argv[1];

	if (arg == "-s" || arg == "--size") {
		if (argc > 3) {
			std::cout << "Option -s accepts at most one value\n";
			usage();
			ret = 1;
			goto exit;
		}
		int a = 1;
		if (!handleSize(argc, argv, a)) {
			ret = 1;
			goto exit;
		}
	} else if (arg == "-e" || arg == "--exclusion-list") {
		if (argc > 4) {
			std::cout << "Option -e accepts at most two values (or -c)\n";
			usage();
			ret = 1;
			goto exit;
		}
		int idx = 1;
		if (!handleExclusionList(argc, argv, idx)) {
			ret = 1;
			goto exit;
		}

		// Reject trailing unexpected arguments for -e variants.
		if (idx != argc - 1) {
			std::cout << "Unexpected extra arguments for -e\n";
			usage();
			ret = 1;
			goto exit;
		}
	} else {
		std::cout << "Unknown option: " << arg << "\n";
		usage();
		ret = 1;
		goto exit;
	}

	// Linux will probably be run from terminal not requiring this
#ifdef _MSC_VER
	std::cout << "You can close the app now\n";
exit:
	std::cin.get();
#else
exit:
#endif
	return ret;
}

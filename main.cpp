#include "vss_meta.h"
#include <vector>

using namespace std;

BOOL SaveVSSMetaData() {

    HANDLE vHandle = INVALID_HANDLE_VALUE;
    WCHAR diskLabel;
    WCHAR input;
    WCHAR volumePath[MAX_PATH];
    WCHAR directoryPath[MAX_PATH];
    WCHAR                   filePath[MAX_PATH];
    BOOL                    bResult = FALSE;

    VSS_VOLUME_HEADER       vssVolumeHeader;
    LARGE_INTEGER           startingOffset;
    LARGE_INTEGER           catalogOffset;

    wprintf(L"Stage1: Detecting All logical drives\n\n");

    std::bitset<32> drives(GetLogicalDrives());
    if (drives.any()) {
        wprintf(L"\t%I64u Logical drives are founded!\n", drives.count());
        wprintf(L"\nStage2: Checking VSS snapshots each drives \n\n");
        for (int i = 4; i != drives.size(); i++) {
            if (drives[i]) {
                diskLabel = i + 'A';
                wprintf(L"\tDrive %c is detected!\n", diskLabel);
                StringCbPrintfW(volumePath, MAX_PATH, L"\\\\.\\%c:", diskLabel);

                vHandle = CreateFileW(volumePath,          // drive to open
                    GENERIC_ALL,                // no access to the drive
                    FILE_SHARE_READ | // share mode
                    FILE_SHARE_WRITE,
                    NULL,             // default security attributes
                    OPEN_EXISTING,    // disposition
                    0,                // file attributes
                    NULL);            // do not copy file attributes

                startingOffset.QuadPart = 0;

                if (vHandle == INVALID_HANDLE_VALUE) {
                    bResult = GetVolumeDiskExtents(volumePath);
                    if (!bResult) {
                        if (GetLastError() == 32) {
                            wprintf(L"\t\tDrive %c is being used...\n\t\tAre you sure you want to force the disk to read? [Y/N]", diskLabel);
                            bResult = wscanf_s(L"%c", &input);
                            if (input == 'Y' || input == 'y') {
                                vHandle = CreateFileW(L"\\\\.\\PhysicalDrive0",          // drive to open
                                    GENERIC_ALL,                // no access to the drive
                                    FILE_SHARE_READ | // share mode
                                    FILE_SHARE_WRITE,
                                    NULL,             // default security attributes
                                    OPEN_EXISTING,    // disposition
                                    0,                // file attributes
                                    NULL);            // do not copy file attributes
                                GetCDriveStartingOffset(&vHandle, &startingOffset);
                            }
                            else {

                            }

                        }

                    }
                }
                ReadVSSHeader(&vHandle, &vssVolumeHeader, startingOffset);
                if (memcmp(vssVolumeHeader.VSS_ID.Identifier, VSS_IDENFIER, sizeof(FILE_ID_128)) == 0) {
                    StringCbPrintfW(directoryPath, MAX_PATH, L".\\%c", diskLabel);
                    bResult = CreateDirectoryW(directoryPath, NULL);
                    if (!bResult && GetLastError() != 183) {
                        wprintf(L"Failed to make %c directory. error code: %ld\n", diskLabel, GetLastError());
                        CloseHandle(vHandle);
                        continue;
                    }
                    StringCbPrintfW(filePath, MAX_PATH, L".\\%c\\VSSheder.dat", diskLabel);
                    bResult = SaveVSSHeader(&vssVolumeHeader, filePath);
                    if (!bResult) {
                        wprintf(L"Failed to save %c VSS volume header. error code: %ld\n", diskLabel, GetLastError());
                        CloseHandle(vHandle);
                        continue;
                    }
                    if (vssVolumeHeader.CATALOG_OFFSET) {
                        catalogOffset.QuadPart = vssVolumeHeader.CATALOG_OFFSET + startingOffset.QuadPart;
                        bResult = SaveCatalogBlocks(&vHandle, catalogOffset, diskLabel, startingOffset);
                        if (!bResult) {
                            wprintf(L"Failed to save %c CatalogBlocks. error code: %ld\n", diskLabel, GetLastError());
                            CloseHandle(vHandle);
                            continue;
                        }
                    }
                    wprintf(L"\t\tVSS metadata is successfully saved \n");
                }
                else
                    wprintf(L"\t\tVSS Header in %c Drive is not dectected...\n", diskLabel);
                CloseHandle(vHandle);
            }
        }

    }
    return bResult;
}

BOOL LoadVSSMetaData() {

    HANDLE vHandle = INVALID_HANDLE_VALUE;
    HANDLE dHandle = INVALID_HANDLE_VALUE;
    HANDLE fHandle = INVALID_HANDLE_VALUE;
    LPDWORD lpNumberOfBytesWritten;
    LPDWORD junk;
    ZeroMemory(&junk, sizeof(LPDWORD));
    ZeroMemory(&lpNumberOfBytesWritten, sizeof(LPDWORD));
    WIN32_FIND_DATA data;
    ZeroMemory(&data, sizeof(WIN32_FIND_DATA));
    WCHAR diskLabel;
    WCHAR input;
    WCHAR volumePath[MAX_PATH];
    WCHAR directoryPath[MAX_PATH];
    WCHAR                   filePath[MAX_PATH];
    BOOL                    bResult = FALSE;

    VSS_VOLUME_HEADER       vssVolumeHeader;
    CATALOG_BLOCK           catalogBlock;
    STORE_HEADER            storeHeader;
    ZeroMemory(&storeHeader, sizeof(STORE_HEADER));

    LARGE_INTEGER           startingOffset;
    LARGE_INTEGER           ret;
    //startingOffset.QuadPart = 0;
    LARGE_INTEGER           catalogOffset;
    LARGE_INTEGER           storeOffset;


    wprintf(L"Stage1: Detecting Saved VSS metadata\n\n");

    dHandle = FindFirstFile(".\\*", &data);
    if (dHandle != INVALID_HANDLE_VALUE) {
        do {
            if (strlen(data.cFileName) == 1) {
                if (data.cFileName[0] >= 'E' && data.cFileName[0] <= 'Z') {
                    StringCbPrintfW(volumePath, MAX_PATH, L"\\\\.\\%c:", data.cFileName[0]);
                    vHandle = CreateFileW(volumePath,          // drive to open
                        GENERIC_WRITE | GENERIC_READ,                // no access to the drive
                        FILE_SHARE_READ | // share mode
                        FILE_SHARE_WRITE,
                        NULL,             // default security attributes
                        OPEN_EXISTING,    // disposition
                        0,                // file attributes
                        NULL);            // do not copy file attributes


                    bResult = DeviceIoControl(vHandle,                       // device to be queried
                        FSCTL_DISMOUNT_VOLUME, // operation to perform
                        NULL, 0,   //                    // no input buffer
                        NULL, 0,//            // output buffer
                        junk,                         // # bytes returned
                        (LPOVERLAPPED)NULL);          // synchronous I/O


                    StringCbPrintfW(filePath, MAX_PATH, L".\\%c\\VSSheder.dat", data.cFileName[0]);
                    LoadVSSHeader(&vssVolumeHeader, filePath);
                    startingOffset.QuadPart = 0x1e00;
                    SetFilePointerEx(vHandle, startingOffset, &ret, FILE_BEGIN);
                    bResult = WriteFile(vHandle, &vssVolumeHeader, sizeof(VSS_VOLUME_HEADER), lpNumberOfBytesWritten, NULL);
                    wprintf(L"Saved VSS metadata for %c drive is founded\n", data.cFileName[0]);

                    for (int i = 0; i != 128; i++) {
                        StringCbPrintfW(filePath, MAX_PATH, L".\\%c\\CatalogFile%d.dat", data.cFileName[0], i);
                        bResult = LoadCatalogBlock(&catalogBlock, filePath);
                        if (!bResult) {
                            wprintf(L"Failed to restore CatalogBlock%d\n", i);
                            break;
                        }

                        WriteCatalogBlock(&vHandle, &catalogBlock);

                        for (int i = 0; i != 127; i++) {
                            if (catalogBlock.CATALOG_ENTRIES[i].CATALOG_ENTRY_TYPE == 3) {
                                storeOffset.QuadPart = catalogBlock.CATALOG_ENTRIES[i].STORE_HEADER_OFFSET;
                                SetFilePointerEx(vHandle, storeOffset, &ret, FILE_BEGIN);
                                bResult = ReadFile(vHandle, &storeHeader, sizeof(STORE_HEADER), junk, NULL);
                                if (bResult) {
                                    wprintf(L"PREV\n");
                                    ReadFromDrive(&vHandle, ret);
                                    std::memcpy(storeHeader.GUID.Identifier, STORE_GUID, sizeof(STORE_GUID));
                                    bResult = WriteFile(vHandle, &storeHeader, sizeof(STORE_HEADER), lpNumberOfBytesWritten, NULL);
                                    if (bResult) {
                                        wprintf(L"AFTER\n");
                                        ReadFromDrive(&vHandle, ret);
                                    }
                                }
                            }
                        }
                        wprintf(L"Restore CatalogBlock%d\n", i);
                    }

                    CloseHandle(vHandle);
                }
            }
        } while (FindNextFile(dHandle, &data));
        FindClose(dHandle);
    }
    return bResult;
}

int wmain(int argc, wchar_t* argv[])
{
    SaveVSSMetaData();
    //LoadVSSMetaData();
    LARGE_INTEGER offset;
    offset.QuadPart = 0x7A8000;
    HANDLE vHandle = INVALID_HANDLE_VALUE;
    vHandle = CreateFileW(L"\\\\.\\E:",          // drive to open
        GENERIC_READ,                // no access to the drive
        FILE_SHARE_READ,
        NULL,             // default security attributes
        OPEN_EXISTING,    // disposition
        0,                // file attributes
        NULL);            // do not copy file attributes

    ReadFromDrive(&vHandle, offset);

    return EXIT_SUCCESS;
}

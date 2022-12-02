#include "vss_meta.h"

BOOL ReadFromDrive(HANDLE* hDevice, LARGE_INTEGER offset) {
    BOOL bResult = FALSE;                 // results flag
    DWORD junk = 0;                     // discard results
    DWORD dwBytesRead;
    LARGE_INTEGER ret;
    LARGE_INTEGER pos;
    pos.QuadPart = offset.QuadPart;
    DWORD error;
    BYTE buf[512];
    ZeroMemory(buf, 512);

    SetFilePointerEx(*hDevice, pos, &ret, FILE_BEGIN);
    wprintf(L"Current Pointer: %X\n", ret.QuadPart);
    bResult = ReadFile(*hDevice, buf, 512, &dwBytesRead, NULL);
    if (bResult) {
        for (int j = 0; j < 32; j++) {
            wprintf(L"%X: ", pos.QuadPart + j * 16);
            for (int i = 0; i < 16; i++)
                wprintf(L"%X ", buf[i + j * 16]);
            wprintf(L"\n");
        }

    }
    else
        wprintf(L"Failed to ReadFile\n");
    return TRUE;
}

BOOL ReadVSSHeader(HANDLE* hDevice, VSS_VOLUME_HEADER* vssVolumeHeader ,LARGE_INTEGER partitionOffset, DWORD bytesPerSector) {
    BOOL bResult = FALSE;
    DWORD dwBytesRead;
    LARGE_INTEGER ret;
    LARGE_INTEGER pos;

    pos.QuadPart = partitionOffset.QuadPart + VSS_HEADER_OFFSET;
    if (pos.QuadPart % bytesPerSector) {
        wprintf(L"Failed ReadVSSHeader. File Pointer is not Sector size unit\n");
        return bResult;
    }

    SetFilePointerEx(*hDevice, pos, &ret, FILE_BEGIN);
    bResult = ReadFile(*hDevice, vssVolumeHeader, sizeof(VSS_VOLUME_HEADER), &dwBytesRead, NULL);

    if (!bResult)
        wprintf(L"Failed ReadVSSHeader.  Error %ld.\n", GetLastError());

    return bResult;
}

BOOL SaveVSSHeader(VSS_VOLUME_HEADER* vssVolumeHeader, LPCWSTR filePath) {
    BOOL bResult = FALSE;
    HANDLE vssHeaderFile = INVALID_HANDLE_VALUE;
    DWORD junk = 0;                     // discard results

    vssHeaderFile = CreateFileW(filePath,      // Open VSS_HEADER.dat
        GENERIC_WRITE,           // Open for reading
        0,                      // Do not share
        NULL,                   // No security
        OPEN_ALWAYS,          // Existing file only
        FILE_ATTRIBUTE_NORMAL,  // Normal file
        NULL);                  // No template file


    if (vssHeaderFile == INVALID_HANDLE_VALUE)
    {
        // Your error-handling code goes here.
        wprintf(L"Failed to open VSS_HEADER. error code %ld",GetLastError());
        return bResult;
    }

    bResult = WriteFile(vssHeaderFile, vssVolumeHeader, sizeof(VSS_VOLUME_HEADER), &junk, NULL);
    
    CloseHandle(vssHeaderFile);
    return bResult;
}

BOOL LoadVSSHeader(VSS_VOLUME_HEADER* vssVolumeHeader, LPCWSTR filePath) {
    BOOL bResult = FALSE;
    HANDLE vssHeaderFile = INVALID_HANDLE_VALUE;
    DWORD junk = 0;                     // discard results

    vssHeaderFile = CreateFileW(filePath,      // Open VSS_HEADER.dat
        GENERIC_READ,           // Open for reading
        0,                      // Do not share
        NULL,                   // No security
        OPEN_ALWAYS,          // Existing file only
        FILE_ATTRIBUTE_NORMAL,  // Normal file
        NULL);                  // No template file

    if (vssHeaderFile == INVALID_HANDLE_VALUE)
    {
        wprintf(L"Failed to load VSS_HEADER.dat.\n");
        return bResult;
    }

    bResult = ReadFile(
        vssHeaderFile,
        vssVolumeHeader,
        sizeof(VSS_VOLUME_HEADER),
        &junk,
        NULL);
    if(!bResult)
        wprintf(L"Failed to LoadVSSHeader. error: %d\n", GetLastError());

    CloseHandle(vssHeaderFile);
    return bResult;
}

BOOL ReadCatalogBlock(HANDLE* hDrive, CATALOG_BLOCK* catalogBlock,LARGE_INTEGER catalogOffset, DWORD bytesPerSector) {
    BOOL bResult = FALSE;
    DWORD dwBytesRead;
    LARGE_INTEGER ret;

    if (catalogOffset.QuadPart) {
        if (catalogOffset.QuadPart % bytesPerSector) {
            wprintf(L"Failed ReadCatalogBlock. File Pointer is not Sector size unit\n");
            return bResult;
        }
    }
    

    bResult = SetFilePointerEx(*hDrive, catalogOffset, &ret, FILE_BEGIN);
    if (!bResult)
        wprintf(L"Failed SetFilePointerEx in ReadCatalogBlock. Error %ld.\n", GetLastError());

    bResult = ReadFile(*hDrive, catalogBlock, sizeof(*catalogBlock), &dwBytesRead, NULL);
    if (!bResult)
        wprintf(L"Failed ReadFile in ReadCatalogBlock.  Error %ld.\n", GetLastError());
    if (sizeof(CATALOG_BLOCK) != dwBytesRead)
        wprintf(L"Failed ReadFile in ReadCatalogBlock.  Error %ld.\n", GetLastError());
    return bResult;
}

BOOL SaveCatalogBlocks(HANDLE *hDrive, LARGE_INTEGER startingCatalogOffset, WCHAR driveLabel, LARGE_INTEGER startingOffset) {
    BOOL bResult = FALSE;
    WCHAR path[MAX_PATH];
    HANDLE catalogFile = INVALID_HANDLE_VALUE;
    DWORD bytesWritten;
    CATALOG_BLOCK* catalogBlock = (CATALOG_BLOCK*)malloc(sizeof(CATALOG_BLOCK));
    LARGE_INTEGER catalogOffset;
    catalogOffset.QuadPart = startingCatalogOffset.QuadPart;
    size_t idx = 0;

    do {
        bResult = ReadCatalogBlock(hDrive, catalogBlock, catalogOffset);
        if (!bResult) {
            wprintf(L"Failed to ReadCatalogBlock. error %ld", GetLastError());
            break;
        }
        StringCbPrintfW(path, MAX_PATH, L".\\%c\\CatalogFile%lld.dat", driveLabel, idx);
        catalogFile = CreateFileW(path,      // Open VSS_HEADER.dat
            GENERIC_WRITE,           // Open for reading
            0,                      // Do not share
            NULL,                   // No security
            OPEN_ALWAYS,          // Existing file only
            FILE_ATTRIBUTE_NORMAL,  // Normal file
            NULL);                  // No template file
        bResult = WriteFile(catalogFile, catalogBlock, sizeof(CATALOG_BLOCK), &bytesWritten,NULL);
        if (!bResult) {
            wprintf(L"Failed to WriteFile. error %ld", GetLastError());
            break;
        }
        idx++;
        CloseHandle(catalogFile);
        catalogOffset.QuadPart = catalogBlock->CATALOG_HEADER.NEXT_CATALOG_OFFSET + startingOffset.QuadPart;

    } while (catalogBlock != nullptr && catalogBlock->CATALOG_HEADER.NEXT_CATALOG_OFFSET);

    free(catalogBlock);
    return bResult;
}

BOOL LoadCatalogBlock(CATALOG_BLOCK* catalogBlock, LPCWSTR path) {
    BOOL bResult = FALSE;
    HANDLE catalogBlockFile = INVALID_HANDLE_VALUE;
    DWORD junk = 0;                     // discard results

    catalogBlockFile = CreateFileW(path,      // Open VSS_HEADER.dat
        GENERIC_READ,           // Open for reading
        0,                      // Do not share
        NULL,                   // No security
        OPEN_EXISTING,          // Existing file only
        FILE_ATTRIBUTE_NORMAL,  // Normal file
        NULL);                  // No template file

    if (catalogBlockFile == INVALID_HANDLE_VALUE)
    {
        wprintf(L"Failed to load CatalogBlock[digit].dat.\n");
        return bResult;
    }

    bResult = ReadFile(
        catalogBlockFile,
        catalogBlock,
        sizeof(CATALOG_BLOCK),
        &junk,
        NULL);
    if (!bResult)
        wprintf(L"Failed to LoadCatalogBlock. error: %d\n", GetLastError());

    CloseHandle(catalogBlockFile);
    return bResult;
}

BOOL WriteCatalogBlock(HANDLE* hDrive,CATALOG_BLOCK* catalogBlock) {
    BOOL bResult = FALSE;
    HANDLE catalogBlockFile = INVALID_HANDLE_VALUE;
    CATALOG_BUF c_buf;
    DWORD junk = 0;                     // discard results
    LARGE_INTEGER catalogOffset;
    LARGE_INTEGER ret;
    int numofCatalog;

    std::memcpy(&c_buf, catalogBlock, 16384);

    for (int i = 0; i != 32; i++) {
        catalogOffset.QuadPart = catalogBlock->CATALOG_HEADER.CURRENT_CATALOG_OFFSET+i*512;
        wprintf(L"PREV\n");
        ReadFromDrive(hDrive, catalogOffset);
        SetFilePointerEx(*hDrive, catalogOffset, &ret, FILE_BEGIN);
        bResult = WriteFile(*hDrive, &(c_buf.CATALOG_BLOCK[i]), 512, &junk, NULL);
        wprintf(L"AFTER\n");
        ReadFromDrive(hDrive, ret);
    }
    
    return bResult;
}

BOOL FindDriveOffset(HANDLE* hDevice) {
    BOOL bResult = FALSE;                 // results flag
    DWORD junk = 0;                     // discard results
    DWORD error;
    PARTITION_INFORMATION_EX piex;
    ZeroMemory(&piex, sizeof(PARTITION_INFORMATION_EX));

    bResult = DeviceIoControl(*hDevice,
        IOCTL_DISK_GET_PARTITION_INFO_EX,
        NULL, 0,
        &piex, sizeof(PARTITION_INFORMATION_EX),
        &junk,
        (LPOVERLAPPED)NULL);

    if (!bResult)
    {
        error = GetLastError();
        printf("DeviceIoControl error: %d\n", error);
        return -1;
    }
    wprintf(L"%s StartingOffset: %lld\n",piex.Gpt.Name,piex.StartingOffset.QuadPart);

    return bResult;
}

BOOL FindPartitionOffsets(HANDLE* hDevice) {
    BOOL bResult = FALSE;                 // results flag
    DWORD junk = 0;                     // discard results
    DWORD error;
    DRIVE_LAYOUT_INFORMATION_EX* pdg;

    DWORD szNewLayout = sizeof(DRIVE_LAYOUT_INFORMATION_EX) + sizeof(PARTITION_INFORMATION_EX) * 4 * 25;

    pdg = (DRIVE_LAYOUT_INFORMATION_EX*)malloc(szNewLayout);
    if (pdg == NULL)
    {
        error = GetLastError();
        printf("malloc error: %d\n", error);
        return -1;
    }
    ZeroMemory(pdg, szNewLayout);

    bResult = DeviceIoControl(*hDevice,                       // device to be queried
        IOCTL_DISK_GET_DRIVE_LAYOUT_EX, // operation to perform
        NULL, 0,                       // no input buffer
        pdg, szNewLayout,// sizeof(*pdg)*2,            // output buffer
        &junk,                         // # bytes returned
        (LPOVERLAPPED)NULL);          // synchronous I/O

    if (!bResult)
    {
        error = GetLastError();
        printf("DeviceIoControl error: %d\n", error);
        //free(pdg);
        return -1;
    }
    for (int i = 0; i < pdg->PartitionCount; i++) {
        printf("partition %d: %lld\n", i, pdg->PartitionEntry[i].StartingOffset.QuadPart);
    }

    free(pdg);
    return 0;
}

BOOL GetCDriveStartingOffset(HANDLE* hDevice, LARGE_INTEGER* offset) {
    BOOL bResult = FALSE;                 // results flag
    DWORD junk = 0;                     // discard results
    DWORD error;
    DRIVE_LAYOUT_INFORMATION_EX* pdg;

    DWORD szNewLayout = sizeof(DRIVE_LAYOUT_INFORMATION_EX) + sizeof(PARTITION_INFORMATION_EX) * 4 * 25;

    pdg = (DRIVE_LAYOUT_INFORMATION_EX*)malloc(szNewLayout);
    if (pdg == NULL)
    {
        error = GetLastError();
        printf("malloc error: %d\n", error);
        return -1;
    }
    ZeroMemory(pdg, szNewLayout);

    bResult = DeviceIoControl(*hDevice,                       // device to be queried
        IOCTL_DISK_GET_DRIVE_LAYOUT_EX, // operation to perform
        NULL, 0,                       // no input buffer
        pdg, szNewLayout,// sizeof(*pdg)*2,            // output buffer
        &junk,                         // # bytes returned
        (LPOVERLAPPED)NULL);          // synchronous I/O

    if (!bResult)
    {
        error = GetLastError();
        printf("DeviceIoControl error: %d\n", error);
        //free(pdg);
        return -1;
    }
    for (int i = 0; i < pdg->PartitionCount; i++) {
        if (lstrcmpW(L"Basic data partition", pdg->PartitionEntry[i].Gpt.Name)  == 0) {
            offset->QuadPart = pdg->PartitionEntry[i].StartingOffset.QuadPart;
            break;
        }
    }

    free(pdg);
    return 0;
}

BOOL ShowDrvieGeometry(DISK_GEOMETRY* pdg, LPCWSTR wszDrive) {
    BOOL bResult = FALSE;                 // results flag

    if (pdg == nullptr) {
        wprintf(L"ShowDriveGeometry failed. Because argument is null\n");
    }
    else {
        ULONGLONG DiskSize = 0;    // size of the drive, in bytes
        wprintf(L"Drive path      = %ws\n", wszDrive);
        wprintf(L"Cylinders       = %I64d\n", pdg->Cylinders.QuadPart);
        wprintf(L"Tracks/cylinder = %ld\n", (ULONG)pdg->TracksPerCylinder);
        wprintf(L"Sectors/track   = %ld\n", (ULONG)pdg->SectorsPerTrack);
        wprintf(L"Bytes/sector    = %ld\n", (ULONG)pdg->BytesPerSector);

        DiskSize = pdg->Cylinders.QuadPart * (ULONG)pdg->TracksPerCylinder *
            (ULONG)pdg->SectorsPerTrack * (ULONG)pdg->BytesPerSector;
        wprintf(L"Disk size       = %I64d (Bytes)\n"
            L"                = %.2f (Gb)\n",
            DiskSize, (double)DiskSize / (CAPACITY_SIZE_UNIT * CAPACITY_SIZE_UNIT * CAPACITY_SIZE_UNIT));
        bResult = TRUE;
    }

    return bResult;
}

BOOL GetDriveGeometry(HANDLE* hDevice, DISK_GEOMETRY* pdg)
{
    BOOL bResult = FALSE;                 // results flag
    DWORD junk = 0;                     // discard results

    bResult = DeviceIoControl((*hDevice),                       // device to be queried
        IOCTL_DISK_GET_DRIVE_GEOMETRY, // operation to perform
        NULL, 0,                       // no input buffer
        pdg, sizeof(*pdg),            // output buffer
        &junk,                         // # bytes returned
        (LPOVERLAPPED)NULL);          // synchronous I/O

    return (bResult);
}

BOOL GetVolumeDiskExtents(LPCWSTR VolumeName) {
    BOOL bResult = FALSE;
    DWORD junk = 0;                     // discard results
    HANDLE vHandle = INVALID_HANDLE_VALUE;
    VOLUME_DISK_EXTENTS vde;
    DWORD   byteReturned;
    vHandle = CreateFileW(VolumeName,          // drive to open
        GENERIC_ALL,                // no access to the drive
        FILE_SHARE_READ | // share mode
        FILE_SHARE_WRITE,
        NULL,             // default security attributes
        OPEN_EXISTING,    // disposition
        0,                // file attributes
        NULL);            // do not copy file attributes

    if (vHandle == INVALID_HANDLE_VALUE)
        return bResult;

    bResult = DeviceIoControl(
        vHandle,
        IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
        NULL,
        0,
        &vde,
        sizeof(vde),
        &junk,
        (LPOVERLAPPED)NULL);

    if (bResult) {
        wprintf(L"DiskNumber :%ld\n",vde.Extents[0].DiskNumber);
        wprintf(L"ExtentLength :%I64d\n",vde.Extents[0].ExtentLength.QuadPart);
        wprintf(L"StartingOffset :%I64d\n",vde.Extents[0].StartingOffset.QuadPart);
        wprintf(L"NumberOfDiskExtents :%ld\n",vde.NumberOfDiskExtents);
    }

    return bResult;
}
//
// Created by Mr.Yll on 2022/4/7.
//

#include <stdlib.h>
#include <windows.h>

BOOL WINAPI DllMain(
        HINSTANCE hinstDLL,  // handle to DLL module
        DWORD fdwReason,     // reason for calling function
        LPVOID lpReserved)  // reserved
{
    // Perform actions based on the reason for calling.
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            // Initialize once for each new process.
            // Return FALSE to fail DLL load.
            MessageBox(nullptr, TEXT("DLL_PROCESS_ATTACH"),TEXT("TestDll"),0);
            break;

        case DLL_THREAD_ATTACH:
            // Do thread-specific initialization.
            MessageBox(nullptr, TEXT("DLL_THREAD_ATTACH"),TEXT("TestDll"),0);
            break;

        case DLL_THREAD_DETACH:
            // Do thread-specific cleanup.
            MessageBox(nullptr, TEXT("DLL_THREAD_DETACH"),TEXT("TestDll"),0);
            break;

        case DLL_PROCESS_DETACH:
            MessageBox(nullptr, TEXT("DLL_PROCESS_DETACH"),TEXT("TestDll"),0);
            // Perform any necessary cleanup.
            break;
    }
    return TRUE;  // Successful DLL_PROCESS_ATTACH.
}
#include <iostream>
#include <windows.h>
#include "PETools/PETools.h"

/*
 * 通过直接创建远程线程的方式，调用LoadLibrary函数，注入自己的模块。
 */
int JustLoadlibrary(PROCESS_INFORMATION& pi)
{

    TCHAR szDllname[] = TEXT("libtestDll.dll");
    void* remoteAddr = VirtualAllocEx(pi.hProcess, nullptr,sizeof(szDllname),MEM_RESERVE | MEM_COMMIT,
                                      PAGE_EXECUTE_READWRITE|PAGE_NOCACHE);
    if (!remoteAddr)
    {
        printf("VirtualAlloc failed (%d).\n", GetLastError());
    }
    WriteProcessMemory(pi.hProcess,remoteAddr,szDllname,sizeof(szDllname), nullptr);
    HANDLE hRemoteThreadInject = CreateRemoteThread(pi.hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibrary, remoteAddr, 0, NULL);
    WaitForSingleObject(hRemoteThreadInject, INFINITE);
    if (!VirtualFreeEx(pi.hProcess,remoteAddr,sizeof(szDllname),MEM_DECOMMIT))
    {
        printf("VirtualFree failed (%d).\n", GetLastError());
    }
    DWORD InjectDllBaseAddr = 0;
    GetExitCodeThread(hRemoteThreadInject, &InjectDllBaseAddr);
    if (InjectDllBaseAddr)
    {
        printf("Remote Dll Loaded On %08x\n",InjectDllBaseAddr);
    }
    else
    {
        printf("Failed to load remote Dll\n");
        return 0;
    }
    Sleep(5000);
    HANDLE hRemoteThreadFree = CreateRemoteThread(pi.hProcess,NULL,0,(LPTHREAD_START_ROUTINE)FreeLibrary,(LPVOID)InjectDllBaseAddr,0,NULL);
    WaitForSingleObject(hRemoteThreadFree, INFINITE);
    DWORD IsFreeSuccess = 0;
    GetExitCodeThread(hRemoteThreadFree, &IsFreeSuccess);
    if (IsFreeSuccess)
    {
        printf("Remote dll has unloaded!\n");
    }
    return 0;
}

/*
 * 通过直接写入进程内存的方式，注入自己的模块。
 */
int MemWrite(PROCESS_INFORMATION& pi)
{

    return 0;
}

//通过方式2注入进程时，模块开始执行的入口点
extern "C" __declspec(dllexport) void WINAPI Entry()
{
    MessageBox(nullptr,TEXT("Entry Point!"), nullptr,0);
    return;
}

bool createProcess(PROCESS_INFORMATION& pi)
{
    const TCHAR* szApplicationName = TEXT("..\\SrcTest.exe");
    STARTUPINFO si;
    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    ZeroMemory( &pi, sizeof(pi) );
    BOOL res = CreateProcess(szApplicationName,
                             NULL,
                             NULL,
                             NULL,
                             FALSE,          // 新进程是否可以继承父进程的句柄
                             NULL,           // 进程创建标志
                             NULL,            // 环境块
                             NULL,         // 当前目录
                             &si,              // 进程启动信息
                             &pi);       // 输出参数，保存了新进程的相关信息
    if(!res)
    {
        printf( "CreateProcess failed (%d).\n", GetLastError() );
        return false;
    }
    return true;
}

int main() {
    //创建待注入的进程
    PROCESS_INFORMATION pi;
    createProcess(pi);

    //注入方式1
    JustLoadlibrary(pi);

    //注入方式2
    MemWrite(pi);

    return 0;
}

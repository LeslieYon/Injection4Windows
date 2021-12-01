#include <iostream>
#include <windows.h>
#include "PETools/PETools.h"

/*
 * 通过直接创建远程线程的方式，调用LoadLibrary函数，注入自己的模块。
 */
int JustLoadlibrary(PROCESS_INFORMATION& pi)
{
    //将要加载的模块名称字符串写入目标进程
    TCHAR szDllname[] = TEXT("libtestDll.dll");
    void* remoteAddr = VirtualAllocEx(pi.hProcess, nullptr,sizeof(szDllname),MEM_RESERVE | MEM_COMMIT,
                                      PAGE_EXECUTE_READWRITE|PAGE_NOCACHE);
    if (!remoteAddr)
    {
        printf("VirtualAlloc failed (%d).\n", GetLastError());
        return false;
    }
    WriteProcessMemory(pi.hProcess,remoteAddr,szDllname,sizeof(szDllname), nullptr);
    //创建新线程，直接调用LoadLibrary加载需要注入的模块
    HANDLE hRemoteThreadInject = CreateRemoteThread(pi.hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibrary, remoteAddr, 0, NULL);
    //等待远程模块加载过程结束
    WaitForSingleObject(hRemoteThreadInject, INFINITE);
    //释放模块名称字符串在目标进程所占的空间
    if (!VirtualFreeEx(pi.hProcess,remoteAddr,sizeof(szDllname),MEM_DECOMMIT))
    {
        printf("VirtualFree failed (%d).\n", GetLastError());
        return false;
    }
    //获取远程模块加载加载是否成功的结果
    DWORD InjectDllBaseAddr = 0;
    GetExitCodeThread(hRemoteThreadInject, &InjectDllBaseAddr);
    if (InjectDllBaseAddr)
    {
        printf("Remote Dll Loaded On %08x\n",InjectDllBaseAddr);
    }
    else
    {
        printf("Failed to load remote Dll\n");
        return false;
    }
    Sleep(5000);
    //利用FreeLibrary卸载远程加载的模块
    HANDLE hRemoteThreadFree = CreateRemoteThread(pi.hProcess,NULL,0,(LPTHREAD_START_ROUTINE)FreeLibrary,(LPVOID)InjectDllBaseAddr,0,NULL);
    WaitForSingleObject(hRemoteThreadFree, INFINITE);
    DWORD IsFreeSuccess = 0;
    GetExitCodeThread(hRemoteThreadFree, &IsFreeSuccess);
    if (IsFreeSuccess)
    {
        printf("Remote dll has unloaded!\n");
    }
    return true;
}

/*
 * 通过直接写入进程内存的方式，注入自己的模块。
 */
LPCSTR lpModuleName;
int MemWrite(PROCESS_INFORMATION& pi)
{
    //获取当前进程加载基址
    HANDLE handle = GetModuleHandleA(lpModuleName);
    PointersToPEBuffer BufferInfo;
    GetBufferInfo(handle, BufferInfo, BufferType::ImageBuffer);
    void* NewImageBuffer = malloc(BufferInfo.PEBufferSize);
    memcpy(NewImageBuffer,handle,BufferInfo.PEBufferSize);
    //在目标进程中分配内存空间
    void* ImageLoadAddr = VirtualAllocEx(pi.hProcess,nullptr, BufferInfo.PEBufferSize, MEM_RESERVE | MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    //根据分配的内存空间位置，修复重定位
    if (!RebuildRelocationTable(NewImageBuffer, BufferType::ImageBuffer, ImageLoadAddr)) //重定位PE文件
    {
        Error("Can't rebulid Src PE relocation!");
        return false;
    }
    //将PE文件镜像写入目标进程
    if (!WriteProcessMemory(pi.hProcess, ImageLoadAddr, NewImageBuffer, BufferInfo.PEBufferSize, nullptr)) {
        Error("Can't write Src PE file to new process!");
        return false;
    }
    //计算注入的PE文件的入口点的位置
    DWORD EntryAddress = (DWORD)GetProcAddress((HMODULE)handle,"Entry@4")-(DWORD)handle+(DWORD)ImageLoadAddr;
    //远程新建一个线程，使其从入口点的位置开始执行
    //为了方便，此处直接将注入模块的加载位置作为参数传递给入口函数
    HANDLE RemoteThreadHandle = CreateRemoteThread(pi.hProcess,NULL,0,(LPTHREAD_START_ROUTINE)EntryAddress,ImageLoadAddr,0,NULL);
    //等待远程线程执行结束
    WaitForSingleObject(RemoteThreadHandle,INFINITE);
    DWORD szExitCode = 0;
    GetExitCodeThread(RemoteThreadHandle,&szExitCode);
    printf("Remote Thread has exited with code 0x%08x\n",szExitCode);
    //释放目标进程中申请的内存空间
    if (!VirtualFreeEx(pi.hProcess,ImageLoadAddr,BufferInfo.PEBufferSize,MEM_DECOMMIT))
    {
        printf("VirtualFree failed (%d).\n", GetLastError());
    }
    return 0;
}

/*
 * 通过方式2注入进程时，模块开始执行的入口点，
 * 此函数原型应与ThreadProc函数一致。
 * 此函数返回值作为线程退出代码传递给父进程
 */
extern "C" __declspec(dllexport) DWORD WINAPI Entry(DWORD ImageBase)
{
    //注入代码运行第一步：重新修复导入表
    if (!BuildImportTable((void*)ImageBase,BufferType::ImageBuffer))
    {
        MessageBox(nullptr,TEXT("Build ImportTable Failed!"), TEXT("Error"),0);
        return false;
    }
    //注入代码运行第二步：执行真正的代码
    MessageBox(nullptr,TEXT("Entry Point!"), TEXT("Success"),0);
    return true;
}

/*
 * 创建需要注入的目标进程。
 */
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

int main(int argc, char *argv[]) {
    //创建待注入的进程
    PROCESS_INFORMATION pi;
    createProcess(pi);

    //注入方式1
    JustLoadlibrary(pi);

    //注入方式2
    lpModuleName = argv[0]; //获取当前模块名
    MemWrite(pi);

    return 0;
}

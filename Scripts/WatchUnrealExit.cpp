// Diagnostic launcher only: never use debugger-attached timings as a performance pass.
// Uses documented Win32 DEBUG_EVENT/ContinueDebugEvent and DbgHelp dump APIs.
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>

static std::wstring Quote(const std::wstring& Value)
{
    std::wstring Out=L"\""; size_t Slashes=0;
    for(wchar_t C:Value)
    {
        if(C==L'\\') { ++Slashes; continue; }
        Out.append(Slashes*(C==L'"'?2:1),L'\\'); Slashes=0;
        if(C==L'"') Out+=L'\\'; Out+=C;
    }
    Out.append(Slashes*2,L'\\'); return Out+L'"';
}

int wmain(int argc,wchar_t** argv)
{
    if(argc<4) return 2; // output directory, executable, its arguments
    const std::filesystem::path Output(argv[1]);
    std::filesystem::create_directories(Output);
    std::ofstream Log(Output/L"debugger.txt",std::ios::app);
    std::wstring Command;
    for(int I=2;I<argc;++I) { if(I>2) Command+=L' '; Command+=Quote(argv[I]); }
    STARTUPINFOW Startup{}; Startup.cb=sizeof(Startup);
    Startup.dwFlags=STARTF_USESHOWWINDOW; Startup.wShowWindow=SW_HIDE;
    PROCESS_INFORMATION Process{};
    if(!CreateProcessW(argv[2],Command.data(),nullptr,nullptr,FALSE,
        DEBUG_ONLY_THIS_PROCESS|CREATE_UNICODE_ENVIRONMENT,nullptr,nullptr,&Startup,&Process))
    { Log<<"create_error="<<GetLastError()<<std::endl; return 3; }
    Log<<"pid="<<Process.dwProcessId<<" debugger_attached=1 timings_not_accepted=1"<<std::endl;
    bool InitialBreakpoint=true; bool Finished=false; DWORD ExitCode=0; int ExceptionIndex=0;
    const ULONGLONG Begin=GetTickCount64();
    while(!Finished)
    {
        if(GetTickCount64()-Begin>1200000)
        { Log<<"timeout_terminated_owned_process"<<std::endl; TerminateProcess(Process.hProcess,124); }
        DEBUG_EVENT Event{};
        if(!WaitForDebugEvent(&Event,500))
        { if(GetLastError()==ERROR_SEM_TIMEOUT) continue; Log<<"wait_error="<<GetLastError()<<std::endl; break; }
        DWORD Continue=DBG_CONTINUE;
        if(Event.dwDebugEventCode==CREATE_PROCESS_DEBUG_EVENT && Event.u.CreateProcessInfo.hFile)
            CloseHandle(Event.u.CreateProcessInfo.hFile);
        if(Event.dwDebugEventCode==LOAD_DLL_DEBUG_EVENT && Event.u.LoadDll.hFile)
            CloseHandle(Event.u.LoadDll.hFile);
        if(Event.dwDebugEventCode==EXCEPTION_DEBUG_EVENT)
        {
            auto& E=Event.u.Exception;
            if(InitialBreakpoint && E.ExceptionRecord.ExceptionCode==EXCEPTION_BREAKPOINT)
                InitialBreakpoint=false;
            else Continue=DBG_EXCEPTION_NOT_HANDLED;
            if(!E.dwFirstChance || E.ExceptionRecord.ExceptionCode==EXCEPTION_ACCESS_VIOLATION)
            {
                ++ExceptionIndex;
                Log<<"exception first_chance="<<E.dwFirstChance<<" code=0x"<<std::hex<<E.ExceptionRecord.ExceptionCode
                   <<" address="<<E.ExceptionRecord.ExceptionAddress<<std::dec<<" thread="<<Event.dwThreadId<<std::endl;
                for(DWORD I=0;I<E.ExceptionRecord.NumberParameters;++I)
                    Log<<"exception_parameter["<<I<<"]=0x"<<std::hex<<E.ExceptionRecord.ExceptionInformation[I]<<std::dec<<std::endl;
                HANDLE Thread=OpenThread(THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,Event.dwThreadId);
                CONTEXT Context{}; Context.ContextFlags=CONTEXT_ALL;
                if(Thread && GetThreadContext(Thread,&Context))
                {
                    EXCEPTION_POINTERS Pointers{&E.ExceptionRecord,&Context};
                    MINIDUMP_EXCEPTION_INFORMATION Info{Event.dwThreadId,&Pointers,FALSE};
                    const auto DumpPath=Output/(L"exception_"+std::to_wstring(ExceptionIndex)+L"_"+std::to_wstring(Event.dwThreadId)+L".dmp");
                    HANDLE File=CreateFileW(DumpPath.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
                    if(File!=INVALID_HANDLE_VALUE)
                    {
                        const BOOL OK=MiniDumpWriteDump(Process.hProcess,Process.dwProcessId,File,
                            MiniDumpWithIndirectlyReferencedMemory,&Info,nullptr,nullptr);
                        Log<<"minidump_ok="<<OK<<" error="<<(OK?0:GetLastError())<<std::endl; CloseHandle(File);
                    }
                    SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_UNDNAME|SYMOPT_LOAD_LINES);
                    SymInitialize(Process.hProcess,nullptr,TRUE);
                    STACKFRAME64 Frame{};
                    Frame.AddrPC.Offset=Context.Rip; Frame.AddrPC.Mode=AddrModeFlat;
                    Frame.AddrStack.Offset=Context.Rsp; Frame.AddrStack.Mode=AddrModeFlat;
                    Frame.AddrFrame.Offset=Context.Rbp; Frame.AddrFrame.Mode=AddrModeFlat;
                    for(int I=0;I<64 && Frame.AddrPC.Offset;++I)
                    {
                        const DWORD64 Address=Frame.AddrPC.Offset;
                        char SymbolBuffer[sizeof(SYMBOL_INFO)+MAX_SYM_NAME]{};
                        auto* Symbol=reinterpret_cast<SYMBOL_INFO*>(SymbolBuffer);
                        Symbol->SizeOfStruct=sizeof(SYMBOL_INFO); Symbol->MaxNameLen=MAX_SYM_NAME;
                        DWORD64 Offset=0; const BOOL Found=SymFromAddr(Process.hProcess,Address,&Offset,Symbol);
                        IMAGEHLP_MODULE64 Module{}; Module.SizeOfStruct=sizeof(Module);
                        SymGetModuleInfo64(Process.hProcess,Address,&Module);
                        Log<<I<<" pc=0x"<<std::hex<<Address<<" module="<<(Module.ImageName?Module.ImageName:"?")
                           <<" module_offset=0x"<<(Address-Module.BaseOfImage)
                           <<" symbol="<<(Found?Symbol->Name:"?")<<" displacement=0x"<<Offset<<std::dec<<std::endl;
                        if(!StackWalk64(IMAGE_FILE_MACHINE_AMD64,Process.hProcess,Thread,&Frame,&Context,nullptr,
                            SymFunctionTableAccess64,SymGetModuleBase64,nullptr)) break;
                    }
                    SymCleanup(Process.hProcess);
                }
                if(Thread) CloseHandle(Thread);
            }
        }
        if(Event.dwDebugEventCode==EXIT_PROCESS_DEBUG_EVENT)
        { ExitCode=Event.u.ExitProcess.dwExitCode; Finished=true; Log<<"exit_code=0x"<<std::hex<<ExitCode<<std::dec<<std::endl; }
        if(!ContinueDebugEvent(Event.dwProcessId,Event.dwThreadId,Continue))
        { Log<<"continue_error="<<GetLastError()<<std::endl; break; }
    }
    CloseHandle(Process.hThread); CloseHandle(Process.hProcess);
    return Finished?static_cast<int>(ExitCode):4;
}

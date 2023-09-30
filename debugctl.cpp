#include <Windows.h>
#include <cstdio>
#include <iostream>

HANDLE g_synch_ev = nullptr;

long veh_handler( EXCEPTION_POINTERS *exception_pointers )
{
    std::printf( "lbr: 0x%llx dr7: 0x%llx\n",
                 exception_pointers->ExceptionRecord->ExceptionInformation[ 0 ],
                 exception_pointers->ContextRecord->Dr7 );

    switch (exception_pointers->ExceptionRecord->ExceptionCode) {
        case EXCEPTION_SINGLE_STEP: {
            exception_pointers->ContextRecord->EFlags |= 0x100;
            exception_pointers->ContextRecord->Dr7 |= 0x300;
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        default:
            return EXCEPTION_CONTINUE_SEARCH;
    }
}

unsigned long busyloop( void *context )
{
    std::printf( "starting loop...\n" );

    SetEvent( g_synch_ev );

    for ( ;; )
        Sleep( 1000 );
}

int
main( )
{
    g_synch_ev = CreateEvent( nullptr,
                              true,
                              false,
                              nullptr );

    std::printf( "event handle: 0x%08x\n",
                 g_synch_ev );

    auto thread_id = 0lu;

    // do not start suspended. allow some quanta to execute first.
    const auto target_thread_handle = CreateThread(
        nullptr,
        0,
        busyloop,
        nullptr,
        0,
        &thread_id );

    WaitForSingleObject( g_synch_ev,
                         INFINITE );

    SuspendThread( target_thread_handle );

    CONTEXT ctx{ };
    ctx.ContextFlags = CONTEXT_FULL;

    GetThreadContext( target_thread_handle,
                      &ctx );

    std::printf( "pc: 0x%llx dr7: 0x%llx\n",
                 ctx.Rip,
                 ctx.Dr7 );

    ctx.EFlags |= 0x100; // set EFLAGS.TF

    ctx.Dr7 |= 0x300; // set last branch record (IA32_DEBUGCTL.LBR)

    std::printf( "eflags: 0x%lx dr7: 0x%llx\n",
                 ctx.EFlags,
                 ctx.Dr7 );

    AddVectoredExceptionHandler(1, veh_handler);

    SetThreadContext( target_thread_handle,
                      &ctx );

    RaiseException(STATUS_SINGLE_STEP, 0, 0, 0);

    ResumeThread( target_thread_handle );


    std::cin.get ( );
    return 1;
}

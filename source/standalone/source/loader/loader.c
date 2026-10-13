#include "platform.h"
__declspec(dllimport) void FH2DiscordPresenceStart(void);
#ifdef WITH_EXTRA_OPTIONS
__declspec(dllimport) const char *Fh2TweaksStatus(void);
#endif
__declspec(dllexport) DWORD azimuth_tls_index=TLS_OUT_OF_INDEXES;
__declspec(dllexport) DWORD azimuth_tls_attached;
__attribute__((section(".tls"),aligned(16))) static const unsigned char tls_template[0x150]={0};
static const PIMAGE_TLS_CALLBACK callbacks[1]={0};
const IMAGE_TLS_DIRECTORY64 _tls_used={(ULONGLONG)(uintptr_t)tls_template,(ULONGLONG)(uintptr_t)(tls_template+sizeof(tls_template)),(ULONGLONG)(uintptr_t)&azimuth_tls_index,(ULONGLONG)(uintptr_t)callbacks,0,0x00500000};
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID reserved){
    (void)instance;(void)reserved;if(reason==DLL_PROCESS_ATTACH){azimuth_tls_attached=GetCurrentThreadId();
#ifdef WITH_EXTRA_OPTIONS
        /* Standard Windows import loading initialized Extra Options first. */
        (void)Fh2TweaksStatus();
#endif
        FH2DiscordPresenceStart();}
    return TRUE;
}

#include "support.h"
#include "sha256.h"
#include "features/discord_presence.h"

static HINSTANCE module_instance;
static volatile LONG started;
static wchar_t directory[PATH_CAPACITY], launcher[PATH_CAPACITY], path[PATH_CAPACITY], folder[PATH_CAPACITY];
static unsigned char file_buffer[65536];
static const unsigned char expected_hash[32]={0x3e,0xc0,0x4c,0x61,0x63,0xfb,0x56,0x14,0xe3,0xd3,0x51,0xba,0x05,0x70,0xd3,0x9b,0x84,0x60,0x41,0xa2,0x57,0x71,0x5d,0x63,0x40,0xec,0xeb,0x03,0xcc,0xb7,0xf0,0xa3};
static void log_status(const char *message) {
    status_set(message); wchar_t filename[PATH_CAPACITY];
    if(!directory[0]||!path_join(filename,directory,L"FH2_DiscordPresence.log"))return;
    HANDLE f=CreateFileW(filename,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
    if(f==INVALID_HANDLE_VALUE)return;DWORD written;LARGE_INTEGER size;
    if(GetFileSizeEx(f,&size)&&size.QuadPart<1048576){WriteFile(f,message,(DWORD)text_length(message),&written,0);WriteFile(f,"\r\n",2,&written,0);}CloseHandle(f);
}
static int readable(const MEMORY_BASIC_INFORMATION *m) {
    DWORD p=m->Protect&0xff;return m->State==MEM_COMMIT&&!(m->Protect&PAGE_GUARD)&&(p==PAGE_READONLY||p==PAGE_READWRITE||p==PAGE_WRITECOPY||p==PAGE_EXECUTE_READ||p==PAGE_EXECUTE_READWRITE||p==PAGE_EXECUTE_WRITECOPY);
}
static int game_headers(unsigned char *base) {
    IMAGE_DOS_HEADER d;IMAGE_NT_HEADERS64 n;
    return memory_read(base,&d,sizeof(d))&&d.e_magic==IMAGE_DOS_SIGNATURE&&d.e_lfanew>=64&&d.e_lfanew<=4096&&memory_read(base+d.e_lfanew,&n,sizeof(n))&&n.Signature==IMAGE_NT_SIGNATURE&&n.FileHeader.Machine==IMAGE_FILE_MACHINE_AMD64&&!(n.FileHeader.Characteristics&IMAGE_FILE_DLL)&&n.FileHeader.TimeDateStamp==GAME_TIMESTAMP&&n.OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR64_MAGIC&&n.OptionalHeader.SizeOfImage==GAME_IMAGE_SIZE;
}
static int complete(unsigned char *base) {
    uintptr_t p=(uintptr_t)base,end=p+GAME_IMAGE_SIZE;if(end<p)return 0;
    while(p<end){MEMORY_BASIC_INFORMATION m;if(VirtualQuery((void*)p,&m,sizeof(m))!=sizeof(m)||!readable(&m)||m.AllocationBase!=base)return 0;uintptr_t next=(uintptr_t)m.BaseAddress+m.RegionSize;if(next<=p)return 0;p=next;}return 1;
}
static unsigned char *find_game(void) {
    SYSTEM_INFO info;GetSystemInfo(&info);uintptr_t p=(uintptr_t)info.lpMinimumApplicationAddress,max=(uintptr_t)info.lpMaximumApplicationAddress;unsigned char *found=0;
    while(p<=max){MEMORY_BASIC_INFORMATION m;if(VirtualQuery((void*)p,&m,sizeof(m))!=sizeof(m))break;
        if(m.BaseAddress==m.AllocationBase&&(m.Type==MEM_PRIVATE||m.Type==MEM_IMAGE)&&readable(&m)&&m.RegionSize>=sizeof(IMAGE_DOS_HEADER)&&game_headers(m.AllocationBase)){if(!complete(m.AllocationBase)||found)return 0;found=m.AllocationBase;}
        uintptr_t next=(uintptr_t)m.BaseAddress+m.RegionSize;if(next<=p)break;p=next;}
    return found;
}
static int paths(void) {
    DWORD n=GetModuleFileNameW(module_instance,directory,PATH_CAPACITY);if(!n||n>=PATH_CAPACITY)return 0;
    while(n&&directory[n-1]!=L'\\'&&directory[n-1]!=L'/')--n;directory[n]=0;
    n=GetModuleFileNameW(0,launcher,PATH_CAPACITY);if(!n||n>=PATH_CAPACITY)return 0;
    wchar_t *name=launcher;for(wchar_t *p=launcher;*p;++p)if(*p==L'\\'||*p==L'/')name=p+1;
    const wchar_t expected[]=L"azimuth.exe";for(unsigned i=0;;++i){wchar_t c=name[i];if(c>=L'A'&&c<=L'Z')c+=32;if(c!=expected[i])return 0;if(!c)break;}*name=0;
    if(!path_join(path,launcher,L"data\\azimuth.ini"))return 0;
    n=GetPrivateProfileStringW(L"launcher",L"game_folder",L"",folder,PATH_CAPACITY,path);if(!n||n>=PATH_CAPACITY-1)return 0;
    if(!((folder[0]&&folder[1]==L':')||folder[0]==L'\\'||folder[0]==L'/')){if(!path_join(path,launcher,folder))return 0;}else{unsigned i=0;do{path[i]=folder[i];}while(folder[i++]);}
    n=GetFullPathNameW(path,PATH_CAPACITY,folder,0);if(!n||n>=PATH_CAPACITY)return 0;
    return path_join(path,folder,L"Mount\\forza_durango_release_final.exe");
}
static int verify_file(void) {
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);if(f==INVALID_HANDLE_VALUE)return 0;
    LARGE_INTEGER size;if(!GetFileSizeEx(f,&size)||size.QuadPart!=GAME_FILE_SIZE){CloseHandle(f);return 0;}
    Sha256Context c;sha256_initialize(&c);DWORD n;uint64_t total=0;int ok=1;
    for(;;){if(!ReadFile(f,file_buffer,sizeof(file_buffer),&n,0)){ok=0;break;}if(!n)break;sha256_update(&c,file_buffer,n);total+=n;}CloseHandle(f);
    unsigned char digest[32];sha256_finalize(&c,digest);return ok&&total==GAME_FILE_SIZE&&memory_equal(digest,expected_hash,32);
}
static DWORD WINAPI initialize(void *unused) {
    (void)unused;if(!paths()){log_status("PRESENCE 1.0.0: refused unsupported host or unresolved game folder.");return 0;}
    if(!verify_file()){log_status("PRESENCE 1.0.0: refused unsupported or modified game executable.");return 0;}
    unsigned char *image=0;for(unsigned i=0;i<120&&!image;++i){image=find_game();if(!image)Sleep(500);}
    if(!image){log_status("PRESENCE 1.0.0: supported game image not mapped; worker not started.");return 0;}
    log_status("PRESENCE 1.0.0: independent DLL initialized; game memory is read only.");discord_presence_start(image,directory);return 0;
}
__declspec(dllexport) const char *FH2DiscordPresenceVersion(void){return "1.0.0";}
__declspec(dllexport) const char *FH2DiscordPresenceStatus(void){return status_get();}
__declspec(dllexport) void FH2DiscordPresenceStart(void) {
    if(InterlockedCompareExchange(&started,1,0))return;HMODULE pinned;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCWSTR)(uintptr_t)FH2DiscordPresenceStart,&pinned)){status_set("Unable to pin presence DLL");return;}
    HANDLE worker=CreateThread(0,0,initialize,0,0,0);if(worker)CloseHandle(worker);else status_set("Unable to start initialization worker");
}
#ifdef FH2_PRESENCE_TEST
__declspec(dllexport) void *Fh2TestFindAzimuthImage(void){return find_game();}
#endif
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID reserved){(void)reserved;if(reason==DLL_PROCESS_ATTACH)module_instance=instance;return TRUE;}

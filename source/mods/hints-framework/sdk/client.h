#ifndef MOON_HINTS_CLIENT_H
#define MOON_HINTS_CLIENT_H
#include "hints.h"
#include "presentation.h"
#include "../../../runtime/native/services.h"
__declspec(dllimport) DWORD __stdcall GetModuleFileNameA(PTR, char *, DWORD);
static inline const MoonHints *mh_connect(PTR domain, PTR game) {
    /* A recognized loader owns dependency startup and provider identity. A
     * missing/incompatible service fails closed instead of loading a second DLL. */
    if(moon_runtime_loader_present()){
        const MoonRuntimeApiV1 *runtime=moon_runtime_connect();
        const MoonHints *api=runtime?(const MoonHints *)runtime->find("hints-framework","hints",MH_ABI,sizeof(MoonHints)):0;
        return api&&api->abi==MH_ABI&&api->size>=sizeof(MoonHints)?api:0;
    }

    char path[1200];
    DWORD n = GetModuleFileNameA(0, path, 1000);
    if (!n || n >= 1000) return 0;
    while (n && path[n-1] != '\\' && path[n-1] != '/') n--;
    if (!n) return 0;
    cat(path+n, "MoonLab\\mods\\hints-framework\\mod.dll");
    PTR module = LoadLibraryA(path);
    if (!module) return 0;
    int (*start)(PTR, PTR) = (int (*)(PTR, PTR))GetProcAddress(module, "MoonLab_ModStart");
    MoonHintsGet get = (MoonHintsGet)GetProcAddress(module, "MoonLab_Hints_v1");
    if (!start || !get || !start(domain, game)) return 0;
    return get(MH_ABI, sizeof(MoonHints));
}
static inline const MoonHintsPresentation *mhp_connect(PTR domain,PTR game){
    if(moon_runtime_loader_present()){
        const MoonRuntimeApiV1 *runtime=moon_runtime_connect();
        const MoonHintsPresentation *api=runtime?(const MoonHintsPresentation *)runtime->find("hints-framework","presentation",MHP_ABI,sizeof(MoonHintsPresentation)):0;
        return api&&api->abi==MHP_ABI&&api->size>=sizeof(MoonHintsPresentation)?api:0;
    }
    char path[1200];
    DWORD n=GetModuleFileNameA(0,path,1000);
    if(!n||n>=1000)return 0;
    while(n&&path[n-1]!='\\'&&path[n-1]!='/')n--;
    if(!n)return 0;
    cat(path+n,"MoonLab\\mods\\hints-framework\\mod.dll");
    PTR module=LoadLibraryA(path);
    if(!module)return 0;
    int (*start)(PTR,PTR)=(int (*)(PTR,PTR))GetProcAddress(module,"MoonLab_ModStart");
    MoonHintsPresentationGet get=(MoonHintsPresentationGet)GetProcAddress(module,"MoonLab_HintsPresentation_v1");
    if(!start||!get||!start(domain,game))return 0;
    return get(MHP_ABI,sizeof(MoonHintsPresentation));
}
#endif

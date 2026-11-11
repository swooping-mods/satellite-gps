#ifndef MOON_CONTROL_CLIENT_H
#define MOON_CONTROL_CLIENT_H
#include "control.h"
#include "../../../runtime/native/services.h"
__declspec(dllimport) DWORD __stdcall GetModuleFileNameA(PTR,char *,DWORD);
static const MoonControls *mc_connect(PTR domain,PTR game){
    if(moon_runtime_loader_present()){
        const MoonRuntimeApiV1 *runtime=moon_runtime_connect();
        const MoonControls *api=runtime?(const MoonControls *)runtime->find("vehicle-control-framework","controls",MC_ABI,sizeof(MoonControls)):0;
        return api&&api->abi==MC_ABI&&api->size>=sizeof(MoonControls)?api:0;
    }
    char path[1200];DWORD n=GetModuleFileNameA(0,path,1000);if(!n||n>=1000)return 0;
    while(n&&path[n-1]!='\\'&&path[n-1]!='/')n--;if(!n)return 0;
    cat(path+n,"MoonLab\\mods\\vehicle-control-framework\\mod.dll");PTR module=LoadLibraryA(path);if(!module)return 0;
    int (*start)(PTR,PTR)=(int(*)(PTR,PTR))GetProcAddress(module,"MoonLab_ModStart");
    MoonControlsGet get=(MoonControlsGet)GetProcAddress(module,"MoonLab_Controls_v1");
    if(!start||!get||!start(domain,game))return 0;return get(MC_ABI,sizeof(MoonControls));
}
#endif

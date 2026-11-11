#ifndef MOON_SHOP_FRAMEWORK_CLIENT_H
#define MOON_SHOP_FRAMEWORK_CLIENT_H
#include "sdk.h"
#include "../../runtime/native/services.h"
__declspec(dllimport) DWORD __stdcall GetModuleFileNameA(PTR,char *,DWORD);
static const MoonShop *ms_connect(PTR domain,PTR game){
    /* A recognized loader owns dependency startup and provider identity. A
     * missing/incompatible service fails closed instead of loading a second DLL. */
    if(moon_runtime_loader_present()){
        const MoonRuntimeApiV1 *runtime=moon_runtime_connect();
        const MoonShop *api=runtime?(const MoonShop *)runtime->find("shop-framework","shop",MS_ABI,sizeof(MoonShop)):0;
        return api&&api->abi==MS_ABI&&api->size>=sizeof(MoonShop)?api:0;
    }

    char path[1200];DWORD n=GetModuleFileNameA(0,path,1000);
    if(!n||n>=1000)return 0;
    while(n&&path[n-1]!='\\'&&path[n-1]!='/')n--;
    if(!n)return 0;
    cat(path+n,"MoonLab\\mods\\shop-framework\\mod.dll");PTR module=LoadLibraryA(path);
    if(!module)return 0;
    int (*start)(PTR,PTR)=(int(*)(PTR,PTR))GetProcAddress(module,"MoonLab_ModStart");
    MoonShopGet get=(MoonShopGet)GetProcAddress(module,"MoonLab_Shop_v1");
    return start&&get&&start(domain,game)?get(MS_ABI,sizeof(MoonShop)):0;
}
#endif

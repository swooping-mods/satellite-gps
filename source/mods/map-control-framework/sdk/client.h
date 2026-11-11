#ifndef MOON_MAP_CONTROLS_CLIENT_H
#define MOON_MAP_CONTROLS_CLIENT_H
#include "map_controls.h"
#include "../../../runtime/native/services.h"
__declspec(dllimport) DWORD __stdcall GetModuleFileNameA(PTR,char *,DWORD);
static const MoonMapControls *mmc_connect(PTR domain,PTR game){
 if(moon_runtime_loader_present()){
  const MoonRuntimeApiV1 *runtime=moon_runtime_connect();
  const MoonMapControls *api=runtime?(const MoonMapControls *)runtime->find("map-control-framework","map-controls",MMC_ABI,sizeof(MoonMapControls)):0;
  return api&&api->abi==MMC_ABI&&api->size>=sizeof(MoonMapControls)?api:0;
 }
 char path[1200];DWORD n=GetModuleFileNameA(0,path,1000);if(!n||n>=1000)return 0;
 while(n&&path[n-1]!='\\'&&path[n-1]!='/')n--;if(!n)return 0;
 cat(path+n,"MoonLab\\mods\\map-control-framework\\mod.dll");PTR module=LoadLibraryA(path);if(!module)return 0;
 int (*start)(PTR,PTR)=(int(*)(PTR,PTR))GetProcAddress(module,"MoonLab_ModStart");
 MoonMapControlsGet get=(MoonMapControlsGet)GetProcAddress(module,"MoonLab_MapControls_v1");
 return start&&get&&start(domain,game)?get(MMC_ABI,sizeof(MoonMapControls)):0;
}
#endif

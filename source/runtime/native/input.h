#ifndef MOONLAB_NATIVE_INPUT_H
#define MOONLAB_NATIVE_INPUT_H
#include "services.h"
/* Include after the native Windows boundary. Loader builds share one registry;
 * standalone single-mod probes retain local behavior through the same core. */
static int moon_input_local_poll(unsigned vk){
    static short (*key)(int);if(!key){void *user=LoadLibraryA("user32.dll");if(user)key=(short(*)(int))GetProcAddress(user,"GetAsyncKeyState");}
    return key&&vk<256&&(key((int)vk)&0x8000)!=0;
}
#define MOON_INPUT_POLL moon_input_local_poll
#include "../sdk/input_host.h"
#undef MOON_INPUT_POLL
__declspec(dllexport) int MoonLab_InputApiAbi(void){return MOON_INPUT_ABI;}
/* A recognized but incompatible loader must not silently split process-wide
 * input ownership. Deployment receipts reject it; direct loading stays inert. */
static unsigned moon_input_unavailable_register(const char *name,const char *defaults){(void)name;(void)defaults;return 0;}
static MoonInputState moon_input_unavailable_sample(unsigned handle,int allowed){(void)handle;(void)allowed;return (MoonInputState){0};}
static int moon_input_unavailable_describe(unsigned index,MoonInputActionInfo *info){(void)index;(void)info;return 0;}
static int moon_input_unavailable_rebind(unsigned handle,const char *binding){(void)handle;(void)binding;return 0;}
static void moon_input_unavailable_configure(MoonInputResolve resolve){(void)resolve;}
static int moon_input_unavailable_claim(const char *channel,unsigned long long frame,const char *owner){(void)channel;(void)frame;(void)owner;return 0;}
static int moon_input_unavailable_key(unsigned key){(void)key;return 0;}
static inline const MoonInputApiV1 *moon_input_unavailable_api(void){static const MoonInputApiV1 api={sizeof(api),MOON_INPUT_ABI,moon_input_unavailable_register,moon_input_unavailable_sample,moon_input_unavailable_describe,moon_input_unavailable_rebind,moon_input_unavailable_configure,moon_input_unavailable_claim,moon_input_unavailable_key};return &api;}
static inline const MoonInputApiV1 *moon_input_api(void){
    static const MoonInputApiV1 *api;if(!api){void *loader=GetModuleHandleA("version.dll");MoonGetInputApiV1 get=loader?(MoonGetInputApiV1)GetProcAddress(loader,"MoonLab_GetInputApi_v1"):0;const MoonInputApiV1 *shared=get?get():0;api=moon_input_api_valid(shared)?shared:loader&&GetProcAddress(loader,"MoonLab_LoaderAbi")?moon_input_unavailable_api():moon_input_host_api();}return api;
}
static inline MoonInputState moon_input_action(const char *name,const char *defaults,int allowed){const MoonInputApiV1 *api=moon_input_api();return api->sample(api->register_action(name,defaults),allowed);}
static inline void moon_input_vk_binding(unsigned key,char binding[16]){
    if((key>='A'&&key<='Z')||(key>='0'&&key<='9')){binding[0]=(char)key;binding[1]=0;return;}
    static const struct {unsigned key;const char *label;} named[]={{8,"Backspace"},{9,"Tab"},{13,"Enter"},{16,"Shift"},{17,"Ctrl"},{18,"Alt"},{27,"Escape"},{32,"Space"},{33,"PageUp"},{34,"PageDown"},{35,"End"},{36,"Home"},{37,"Left"},{38,"Up"},{39,"Right"},{40,"Down"},{45,"Insert"},{46,"Delete"},{186,"Semicolon"},{187,"Equals"},{188,"Comma"},{189,"Minus"},{190,"Period"},{191,"Slash"}};
    for(unsigned i=0;i<sizeof(named)/sizeof(*named);i++)if(key==named[i].key){mi_copy(binding,16,named[i].label);return;}
    unsigned p=0;if(key>=112&&key<=135){binding[p++]='F';key-=111;}else{binding[p++]='V';binding[p++]='K';}
    char digits[10];unsigned n=0;do{digits[n++]=(char)('0'+key%10);key/=10;}while(key&&n<sizeof(digits));while(n)binding[p++]=digits[--n];binding[p]=0;
}
static inline MoonInputState moon_input_key_action(const char *name,unsigned key,int allowed){char binding[16];moon_input_vk_binding(key,binding);return moon_input_action(name,binding,allowed);}
/* Retained low-level movement/diagnostic clients can share polling without
 * registering hundreds of unowned actions. Named hotkeys overlay these edges. */
static inline void moon_input_snapshot(int down[256],int pressed[256]){for(unsigned key=0;key<256;key++){int held=moon_input_api()->key_held(key);pressed[key]=held&&!down[key];down[key]=held;}}
typedef struct {const char *name,*defaults;unsigned key;} MoonInputAlias;
static inline void moon_input_alias_edges(const MoonInputAlias *aliases,unsigned count,int pressed[256],int allowed){for(unsigned i=0;i<count;i++)if(aliases[i].key<256)pressed[aliases[i].key]=moon_input_action(aliases[i].name,aliases[i].defaults,allowed).pressed;}
static inline int moon_input_claim(const char *channel,unsigned long long frame,const char *owner){return moon_input_api()->claim(channel,frame,owner);}
static inline int moon_input_key(unsigned vk){return moon_input_api()->key_held(vk);}
static inline int moon_input_label(const char *name,const char *defaults,char *out,unsigned cap){
    const MoonInputApiV1 *api=moon_input_api();unsigned handle=api->register_action(name,defaults);MoonInputActionInfo info={.size=sizeof(info)};
    if(!handle||!api->describe(handle-1,&info)||!cap)return 0;
    unsigned at=0,offset=0;while(info.binding[offset]){unsigned start=offset;while(info.binding[offset]&&info.binding[offset]!='+')offset++;if(info.binding[start]!='!'){if(at){if(at+1>=cap)return 0;out[at++]='+';}for(unsigned i=start;i<offset;i++){if(at+1>=cap)return 0;out[at++]=info.binding[i];}}if(info.binding[offset])offset++;}out[at]=0;return 1;
}
static inline int moon_input_key_label(const char *name,unsigned key,char *out,unsigned cap){char binding[16];moon_input_vk_binding(key,binding);return moon_input_label(name,binding,out,cap);}
#endif

#ifndef MOONLAB_INPUT_HOST_H
#define MOONLAB_INPUT_HOST_H
#include "input_registry.h"
/* The including adapter supplies MOON_INPUT_POLL(unsigned vk). */
static MoonInputRegistry moon_input_host_registry;
#ifndef MOON_INPUT_BEFORE_SAMPLE
#define MOON_INPUT_BEFORE_SAMPLE() ((void)0)
#endif
#ifndef MOON_INPUT_ADMIT
#define MOON_INPUT_ADMIT(action,key) 1
#endif
static unsigned moon_input_host_register(const char *name,const char *defaults){return mi_register(&moon_input_host_registry,name,defaults);}
static MoonInputState moon_input_host_sample(unsigned handle,int allowed){
    MOON_INPUT_BEFORE_SAMPLE();
    unsigned key=handle&&handle<=moon_input_host_registry.count?moon_input_host_registry.actions[handle-1].chord.key:0;
    int admit=MOON_INPUT_ADMIT(handle,key);(void)key;
    MoonInputState state=mi_sample(&moon_input_host_registry,handle,allowed&&admit);
    return admit?state:(MoonInputState){0};
}
static int moon_input_host_describe(unsigned index,MoonInputActionInfo *info){return mi_describe(&moon_input_host_registry,index,info);}
static int moon_input_host_rebind(unsigned handle,const char *binding){return mi_rebind(&moon_input_host_registry,handle,binding);}
static void moon_input_host_configure(MoonInputResolve resolve){mi_configure(&moon_input_host_registry,resolve);}
static int moon_input_host_claim(const char *channel,unsigned long long frame,const char *owner){return mi_claim(&moon_input_host_registry,channel,frame,owner);}
static int moon_input_host_poll(unsigned key){return MOON_INPUT_POLL(key);}
static int moon_input_host_key(unsigned key){MOON_INPUT_BEFORE_SAMPLE();return MOON_INPUT_ADMIT(0,key)&&moon_input_host_poll(key);}
static inline const MoonInputApiV1 *moon_input_host_api(void){
    static const MoonInputApiV1 api={sizeof(api),MOON_INPUT_ABI,moon_input_host_register,moon_input_host_sample,moon_input_host_describe,moon_input_host_rebind,moon_input_host_configure,moon_input_host_claim,moon_input_host_key};
    /* Edge tracking always observes physical state, even when an adapter masks
     * delivery. Otherwise releasing a claim can invent a peer's pressed edge. */
    moon_input_host_registry.key=moon_input_host_poll;return &api;
}
#endif

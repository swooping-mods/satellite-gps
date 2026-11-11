#ifndef MOONLAB_INPUT_V1_H
#define MOONLAB_INPUT_V1_H
/* Local input only: each consumer supplies its own world, focus, typing,
 * menu and authority gate. This API does not consume keys; a separate host
 * interaction adapter may mask delivery of a captured physical key. */
#define MOON_INPUT_ABI 1u
#define MOON_INPUT_NAME_CAPACITY 96u
#define MOON_INPUT_LABEL_CAPACITY 64u
typedef struct { int held,pressed,released; } MoonInputState;
typedef struct {
    unsigned size,handle;
    char name[MOON_INPUT_NAME_CAPACITY],binding[MOON_INPUT_LABEL_CAPACITY];
} MoonInputActionInfo;
/* Return nonzero only when an override was read. The registry validates it
 * atomically; malformed overrides retain the previous valid binding. */
typedef int (*MoonInputResolve)(const char *,char *,unsigned);
typedef struct {
    unsigned size,abi;
    unsigned (*register_action)(const char *,const char *);
    MoonInputState (*sample)(unsigned,int);
    int (*describe)(unsigned,MoonInputActionInfo *);
    int (*rebind)(unsigned,const char *);
    void (*configure)(MoonInputResolve);
    int (*claim)(const char *,unsigned long long,const char *);
    int (*key_held)(unsigned);
} MoonInputApiV1;
typedef const MoonInputApiV1 *(*MoonGetInputApiV1)(void);
static inline int moon_input_api_valid(const MoonInputApiV1 *a){
    return a&&a->size==sizeof(*a)&&a->abi==MOON_INPUT_ABI&&a->register_action&&a->sample&&a->describe&&a->rebind&&a->configure&&a->claim&&a->key_held;
}
#endif

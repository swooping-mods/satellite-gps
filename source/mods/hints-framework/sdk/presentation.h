#ifndef MOON_HINTS_PRESENTATION_V1_H
#define MOON_HINTS_PRESENTATION_V1_H
#define MHP_ABI 1u
#define MHP_LIMIT 32u
/* Called on the admitted Unity thread immediately before this native legend
 * invocation selects hints. walker is borrowed only for this call. Callbacks
 * may publish/clear their hint context; they must not throw or mutate this
 * subscription registry. Multiple invocations can occur in one render frame. */
typedef void (*MoonHintsPrepare)(void *walker,void *user);
typedef struct {
    unsigned abi,size;
    /* Late registration is allowed on the observed Unity thread. The stable
     * owner.slot key is copied. Callback/user storage stays resident until a
     * successful unsubscribe, or exit. Zero refuses admission/capacity/key. */
    unsigned (*subscribe)(const char *key,MoonHintsPrepare callback,void *user);
    /* Returns zero during dispatch or on a foreign thread/unknown handle.
     * A failed unsubscribe MUST retain callback code/context roots. */
    int (*unsubscribe)(unsigned handle);
} MoonHintsPresentation;
typedef const MoonHintsPresentation *(*MoonHintsPresentationGet)(unsigned abi,unsigned size);
_Static_assert(sizeof(MoonHintsPresentation)==24,"Hints presentation ABI 1");
#endif

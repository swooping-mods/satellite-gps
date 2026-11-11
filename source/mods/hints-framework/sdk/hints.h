#ifndef MOON_HINTS_V1_H
#define MOON_HINTS_V1_H
#define MH_ABI 1u
#define MH_PROVIDER_LIMIT 32
#define MH_CONTROL_LIMIT 8
/* Main-thread-only borrowed input is copied before returning. Call publish each
 * equipped tick; unpublished contexts expire after one Update frame. Control
 * grammar: "Z: Zoom | Mouse1: Apply". A line without ':' is a status label. */
typedef struct {
    unsigned int abi, size;
    int (*publish)(const char *key, void *walker, const char *controls);
    void (*clear)(const char *key);
} MoonHints;
typedef const MoonHints *(*MoonHintsGet)(unsigned int abi, unsigned int size);
#endif

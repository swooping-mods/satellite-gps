#ifndef MOON_CONTROL_H
#define MOON_CONTROL_H
#include "../../../runtime/native/vehicle_input.h"
#define MC_ABI 1u
#define MC_LIMIT 16
#define MC_CARS 64
#define MC_HARDWARE 1u
#define MC_ASSIST 2u
/* All calls run on the Unity thread. frame.input is the restored vanilla input;
 * manual_active marks steering above .08, throttle above .12 (absolute),
 * brake or boost. Neutral hardware remains a fallback for assisted steering. Providers may return only
 * finite normalized values. Assistance requires an explicit per-car lease. */
typedef struct {
    unsigned int abi,size;
    MoonVehicleInput input;
    unsigned int manual_active;
} MoonControlFrame;
typedef int (*MoonControlCallback)(void *vehicle,const MoonControlFrame *frame,MoonVehicleInput *proposal);
typedef struct {
    unsigned int abi,size;
    const char *id;
    unsigned int kind;
    int priority;
    MoonControlCallback callback;
} MoonControlProvider;
typedef struct {
    unsigned int abi,size;
    int (*register_provider)(const MoonControlProvider *provider);
    int (*acquire)(const char *id,void *vehicle);
    int (*release)(const char *id,void *vehicle);
    int (*owns)(const char *id,void *vehicle);
    int (*set_unattended)(const char *id,void *vehicle,int enabled);
    int (*can_unattended)(void *vehicle);
    int (*set_unattended_work)(const char *id,void *vehicle,int enabled);
    int (*can_unattended_work)(void *vehicle);
} MoonControls;
typedef const MoonControls *(*MoonControlsGet)(unsigned int,unsigned int);
#endif

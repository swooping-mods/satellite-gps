#ifndef MOON_MAP_CONTROLS_V1_H
#define MOON_MAP_CONTROLS_V1_H
#define MMC_ABI 1u
#define MMC_PROVIDER_LIMIT 8
#define MMC_UNIT_LIMIT 64
#define MMC_ACTION_LIMIT 6
#define MMC_TEXT_BYTES 128
/* All callbacks run on the Unity thread, only while the native map is active,
 * focused, outside typing/menu/restoring. Borrowed object pointers expire on
 * return. Providers retain simulation/authority; registration copies definitions.
 * Ground captures are the last native right-click ground menu point during this
 * map session, normalized into world space; they survive clicks on this panel. */
typedef struct {float x,y,z;} MoonMapVector;
typedef struct {
 unsigned abi,size,frame;
 void *world,*walker,*map;
 int has_ground,hovered_sector,hovered_robot,hovered_vehicle;
 MoonMapVector ground_direction,ground_point;
} MoonMapContext;
typedef struct {int id;char label[64];} MoonMapUnit;
typedef struct {unsigned command;char label[32];int enabled;} MoonMapAction;
typedef struct {
 unsigned unit_count,action_count;
 int selected_id;
 MoonMapUnit units[MMC_UNIT_LIMIT];
 MoonMapAction actions[MMC_ACTION_LIMIT];
 char status[MMC_TEXT_BYTES];
} MoonMapRow;
typedef struct {
 unsigned abi,size;
 const char *key,*title;
 /* Fill a zeroed row, keep unit IDs unique/stable in this world. Requested -1
  * means choose an initial unit. Return 0 to hide this provider's row. */
 int (*describe)(const MoonMapContext *,int requested_unit_id,MoonMapRow *);
 int (*invoke)(const MoonMapContext *,int selected_unit_id,unsigned command);
} MoonMapProvider;
typedef struct {
 unsigned abi,size;
 int (*register_provider)(const MoonMapProvider *);
 /* Native panel hit-test for cooperating raw-key/map consumers. True through
  * release of a panel-started drag, preventing selection/key input leakage. */
 int (*pointer_captured)(void *walker);
 /* Native map admission, including pause/dialog/other native screen gates.
  * The map's own uiBlocked flag is expected and does not reject itself. */
 int (*context_active)(void *walker);
} MoonMapControls;
typedef const MoonMapControls *(*MoonMapControlsGet)(unsigned abi,unsigned size);
#endif

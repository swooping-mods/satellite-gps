/* Straight-line GPS and purchased unattended driving share the vehicle broker. */
#include "../../runtime/native/gameplay.h"
#include "../../runtime/native/input.h"
#include "api.h"
#include "store.h"
#include "module_store.h"
#include "guidance_core.h"
#include "../shop-framework/client.h"
#include "../hints-framework/sdk/client.h"
#include "../vehicle-control-framework/sdk/client.h"
#include "../map-control-framework/sdk/client.h"
int _fltused=0;
void *memcpy(void *to,const void *from,U64 n){dcopy(to,from,n);return to;}
#define GPS_KEY "satellite-gps"
#define GPS_PRICE 750
#define GPS_LIMIT 64
enum { GPS_HEADING=1,GPS_A,GPS_B,GPS_FOLLOW,GPS_TARGET,GPS_STOP };
typedef struct {
    DRoot car;char storage[GM_ID],name[64],status[128];int id,seen,active,autonomous,stopping,has_a,has_route;
    SGVec a,progress_position;SGRoute route;SGState state;SGOutput output;U64 last_tick,next_probe,progress_time;
} GPSCar;
static struct {
    PTR world_f,mounted_f,body_f,record_owner_f,spec_name_f;
    PTR owner,driven,world_m,storage,spec,index,cruise,set_cruise,grounded,autopilot,speed,full;
    PTR transform,position,forward,right,up,find,ray,fixed_dt,frame,focus,typing,blocked,menu,restoring,alive;
    PTR joined,host,local,player,wallet,charge,decimal_int,decimal_long,save_root,owned_at,component,map_open,request_save;
    PTR vehicle_type,map_type;const unsigned short *(*chars)(PTR);int (*length)(PTR);
    short (*key)(int);PTR (*foreground)(void);DWORD (*window_pid)(PTR,DWORD *);U64 (*clock)(void);
    const MoonControls *controls;const MoonMapControls *maps;const MoonShop *shop;const MoonHints *hints;
    DRoot world,walker,map;GPSCar cars[GPS_LIMIT];GPSModuleSave modules;
    DWORD thread;int last_frame,invalid,last_car,next_id,was_eligible,authority_set,authority,suspended,joined_initial;
    U64 host_initial,next_scan,next_telemetry;char root[SS_ROOT_MAX+1];PTR log;
} gps;
static int gps_alive(PTR obj){int ok=1;void *a[]={obj};PTR v=obj?call(gps.alive,0,a,&ok):0;return ok&&v&&*(unsigned char *)unbox(v);}
static int gps_string(PTR s,char *out,unsigned cap,int strict){
    if(!s||!cap)return 0;int n=gps.length(s);if(n<0||(unsigned)n>=cap)return 0;const unsigned short *p=gps.chars(s);
    for(int i=0;i<n;i++){if(p[i]<32||p[i]>=127){if(strict)return 0;out[i]='?';}else out[i]=(char)p[i];}out[n]=0;return 1;
}
static void gps_text(char *out,unsigned cap,const char *text){unsigned i=0;if(!cap)return;while(text&&text[i]&&i+1<cap){out[i]=text[i];i++;}out[i]=0;}
static void gps_event(const char *name,int id){if(!gps.log)return;char b[256],*p=cat(b,"{\"event\":\"");p=cat(p,name);p=cat(p,"\",\"car\":");p=num(p,(U64)(id<0?0:id));p=cat(p,"}\n");DWORD n;WriteFile(gps.log,b,(DWORD)(p-b),&n,0);FlushFileBuffers(gps.log);}
static int gps_host(void){
    int ok=1,joined=boolean(gps.joined,0,&ok);U64 mine=0,host=0;
    if(joined){PTR a=call(gps.local,0,0,&ok),b=call(gps.host,0,0,&ok);if(a)dcopy(&mine,unbox(a),8);if(b)dcopy(&host,unbox(b),8);}
    if(!gps.authority_set){gps.authority_set=1;gps.joined_initial=joined;gps.host_initial=host;gps.authority=ok&&(!joined||(mine&&mine==host));}
    if(!ok||joined!=gps.joined_initial||(joined&&host!=gps.host_initial))gps.suspended=1;
    return gps.authority&&!gps.suspended;
}
static int gps_focused(void){int ok=1;DWORD pid=0;gps.window_pid(gps.foreground(),&pid);return pid==GetCurrentProcessId()&&boolean(gps.focus,0,&ok)&&!boolean(gps.typing,0,&ok)&&ok;}
static SGVec gps_vector(PTR method,PTR self,int *ok){DVec v=dvector(method,self,ok);return sg_vec(v.x,v.y,v.z);}
static GPSCar *gps_by_id(int id){for(int i=0;i<GPS_LIMIT;i++)if(gps.cars[i].car&&gps.cars[i].id==id)return &gps.cars[i];return 0;}
static GPSCar *gps_by_car(PTR car){if(!car)return 0;for(int i=0;i<GPS_LIMIT;i++)if(dget(gps.cars[i].car)==car)return &gps.cars[i];return 0;}
static int gps_pose(GPSCar *c,SGInput *input){
    PTR car=dget(c->car);if(!gps_alive(car))return 0;int ok=1;PTR tr=call(gps.transform,car,0,&ok);
    input->position=gps_vector(gps.position,tr,&ok);input->forward=gps_vector(gps.forward,tr,&ok);
    input->speed=dnumber(gps.speed,car,&ok);input->dt=dnumber(gps.fixed_dt,0,&ok);
    return ok&&sg_vec_finite(input->position)&&sg_vec_finite(input->forward);
}
static void gps_release(GPSCar *c,const char *reason){
    PTR car=dget(c->car);if(c->active){
        if(!c->autonomous&&gps_alive(car)){int ok=1;unsigned char off=0;void *a[]={&off};call(gps.set_cruise,car,a,&ok);}
        gps.controls->set_unattended(GPS_KEY,car,0);gps.controls->release(GPS_KEY,car);gps_event("gps-stopped",c->id);
    }
    c->active=c->autonomous=c->stopping=0;c->last_tick=0;sg_reset(&c->state);gps_text(c->status,sizeof(c->status),reason);
}
static void gps_stop(GPSCar *c,const char *reason,int immediate){
    if(!c)return;
    if(!immediate&&c->active&&c->autonomous&&gps_alive(dget(c->car))){c->stopping=1;gps_text(c->status,sizeof(c->status),reason);}
    else gps_release(c,reason);
}
static void gps_drop(void){
    for(int i=0;i<GPS_LIMIT;i++){GPSCar *c=&gps.cars[i];if(c->car)gps_release(c,"World changed");ddrop(&c->car);}
    dzero(gps.cars,sizeof(gps.cars));dzero(&gps.modules,sizeof(gps.modules));ddrop(&gps.world);ddrop(&gps.walker);ddrop(&gps.map);
    gps.hints->clear(GPS_KEY);gps.root[0]=0;gps.invalid=gps.authority_set=gps.authority=gps.suspended=0;gps.last_car=0;gps.next_scan=0;gps.next_id=1;gps.was_eligible=0;
}
static int gps_context(PTR world,PTR walker){
    if(gps.thread&&gps.thread!=GetCurrentThreadId())return 0;gps.thread=GetCurrentThreadId();
    if(gps.world&&(world!=dget(gps.world)||walker!=dget(gps.walker)||!gps_alive(world)))gps_drop();
    if(!gps_alive(world)||!gps_alive(walker))return 0;int ok=1;if(boolean(gps.restoring,world,&ok)||!ok)return 0;
    if(!gps.world){
        gps.world=dhold(world);gps.walker=dhold(walker);if(!gps.world||!gps.walker){gps_drop();return 0;}
        void *a[]={gps.map_type};gps.map=dhold(call(gps.component,walker,a,&ok));
        PTR path=call(gps.save_root,world,0,&ok);unsigned size=0;
        if(!ok||!gps_string(path,gps.root,sizeof(gps.root),1))gps.invalid=1;
        else if(ss_load(gps.root,(unsigned char *)&gps.modules,sizeof(gps.modules),&size))gps.invalid=size!=sizeof(gps.modules)||!gm_save_valid(&gps.modules);
        else if(ss.last_error!=SS_NOT_FOUND)gps.invalid=1;
        (void)gps_host();gps_event("world-ready",0);
    }return 1;
}
static int gps_local_owned(GPSCar *c){
    if(!c||!gps_alive(dget(c->car))||!gm_id_valid(c->storage))return 0;
    int ok=1;void *a[]={da.string(c->storage)};PTR record=call(gps.owned_at,dget(gps.world),a,&ok),owner=0;
    if(!record||!ok)return 0;field_value(record,gps.record_owner_f,&owner);PTR player=call(gps.player,0,0,&ok);char p[128],o[128];
    return ok&&gps_string(player,p,sizeof(p),1)&&gps_string(owner,o,sizeof(o),1)&&gm_equal(p,o);
}
static void gps_discover(void){
    int ok=1,inactive=0,sort=0;void *a[]={gps.vehicle_type,&inactive,&sort};DRoot array=dhold(call(gps.find,0,a,&ok));
    if(!ok||!array){ddrop(&array);return;}U64 count=da.array_length(dget(array));if(count>GPS_LIMIT)count=GPS_LIMIT;
    for(int i=0;i<GPS_LIMIT;i++)gps.cars[i].seen=0;
    for(U64 i=0;i<count;i++){
        PTR vehicle=*(PTR *)darray(dget(array),i);ok=1;if(!gps_alive(vehicle)||call(gps.world_m,vehicle,0,&ok)!=dget(gps.world)||call(gps.owner,vehicle,0,&ok)!=dget(gps.walker)||!ok)continue;
        char id[GM_ID];PTR s=call(gps.storage,vehicle,0,&ok);if(!ok||!gps_string(s,id,sizeof(id),1)||!gm_id_valid(id))continue;
        GPSCar *c=gps_by_car(vehicle);
        if(!c){
            for(int j=0;j<GPS_LIMIT;j++)if(gps.cars[j].car&&gm_equal(gps.cars[j].storage,id)){c=&gps.cars[j];gps_release(c,"Vehicle recalled");ddrop(&c->car);break;}
            if(!c)for(int j=0;j<GPS_LIMIT;j++)if(!gps.cars[j].car){c=&gps.cars[j];break;}
            if(!c)continue;dzero(c,sizeof(*c));c->car=dhold(vehicle);if(!c->car)continue;
            c->id=gps.next_id++;gps_text(c->storage,sizeof(c->storage),id);gps_text(c->name,sizeof(c->name),"Car");
            PTR spec=call(gps.spec,vehicle,0,&ok),name=0;if(spec)field_value(spec,gps.spec_name_f,&name);(void)gps_string(name,c->name,sizeof(c->name),0);
            gps_text(c->status,sizeof(c->status),"Capture heading or A/B line");gps_event("vehicle-discovered",c->id);
        }
        c->seen=1;if(c->active&&c->autonomous&&!gps_local_owned(c))gps_release(c,"Vehicle ownership changed");
        if(boolean(gps.driven,vehicle,&ok)&&ok)gps.last_car=c->id;
    }
    ddrop(&array);
    for(int i=0;i<GPS_LIMIT;i++)if(gps.cars[i].car&&!gps.cars[i].seen){gps_release(&gps.cars[i],"Vehicle unloaded");ddrop(&gps.cars[i].car);}
}
static int gps_capture(GPSCar *c){
    SGInput input={0};if(!gps_pose(c,&input))return 0;SGRoute route;
    /* Native MoonWorld terrain, map Pick and vehicle positions use a moon
     * centered at world zero on this pin; the core supports arbitrary centers. */
    if(!sg_capture_heading(&route,sg_vec(0,0,0),input.position,input.forward))return 0;
    gps_stop(c,"Heading captured",1);c->route=route;c->has_route=1;gps_text(c->status,sizeof(c->status),"Heading captured; K while driving or unattended Follow line");gps_event("heading-captured",c->id);return 1;
}
static int gps_start(GPSCar *c){
    if(!c||c->active||!c->has_route||!gps_focused())return 0;PTR car=dget(c->car);int ok=1,driven=boolean(gps.driven,car,&ok);
    if(!ok||!gps_alive(car)||!boolean(gps.grounded,car,&ok)||boolean(gps.autopilot,car,&ok)||!ok)return 0;
    if(driven){if(call(gps.owner,car,0,&ok)!=dget(gps.walker)||!boolean(gps.cruise,car,&ok)||!ok){gps_text(c->status,sizeof(c->status),"Enable native cruise first");return 0;}}
    else if(!gps_host()||gps.invalid||!gm_owned(&gps.modules,c->storage)||!gps_local_owned(c)||!gps.controls->can_unattended(car)){gps_text(c->status,sizeof(c->status),"Owned autonomous module required ($750)");return 0;}
    SGInput in={0};if(!gps_pose(c,&in))return 0;
    if(c->route.kind==SG_LINE){SGVec up;if(!sg_unit(sg_sub(in.position,c->route.center),&up,0,0))return 0;if(sg_dot(in.forward,sg_cross(c->route.normal,up))<0)sg_reverse(&c->route);}
    SGState state={0};SGOutput out;if(sg_tick(&state,&c->route,&in,&out)!=SG_TRACKING){gps_text(c->status,sizeof(c->status),"Align car with route before starting");return 0;}
    if(!gps.controls->acquire(GPS_KEY,car)){gps_text(c->status,sizeof(c->status),"Another steering assist owns this car");return 0;}
    if(!gps.controls->set_unattended(GPS_KEY,car,!driven)){gps.controls->release(GPS_KEY,car);return 0;}
    if(!driven&&gps.controls->can_unattended_work(car))(void)gps.controls->set_unattended_work(GPS_KEY,car,1);
    c->active=1;c->autonomous=!driven;c->stopping=0;c->last_tick=0;c->next_probe=0;c->progress_position=in.position;c->progress_time=gps.clock();sg_reset(&c->state);
    gps_text(c->status,sizeof(c->status),driven?"GPS steering; manual input takes over":"Autonomous; map Stop brakes this car");gps_event(driven?"gps-started":"autonomous-started",c->id);return 1;
}
static unsigned gps_offer(void *world,void *walker){
    if(!gps_context(world,walker)||!gps_host()||gps.invalid)return MS_BLOCKED;
    GPSCar *c=gps_by_id(gps.last_car);if(!c||!gps_local_owned(c)||!gps.controls->can_unattended(dget(c->car)))return MS_BLOCKED;
    if(gm_owned(&gps.modules,c->storage))return MS_AVAILABLE|MS_OWNED;
    return gps.modules.count<GM_CARS?MS_AVAILABLE:MS_BLOCKED;
}
static long long gps_balance(PTR world,PTR player,int *ok){void *a[]={player};PTR m=call(gps.wallet,world,a,ok);if(!m){*ok=0;return 0;}void *b[]={unbox(m)};PTR v=call(gps.decimal_long,0,b,ok);if(!v){*ok=0;return 0;}return *(long long *)unbox(v);}
static int gps_charge(PTR world,PTR player,int amount){int ok=1;unsigned char decimal[16]={0};void *a[]={&amount};call(gps.decimal_int,decimal,a,&ok);void *b[]={player,decimal};if(ok)call(gps.charge,world,b,&ok);return ok;}
static int gps_buy(void *world,void *walker){
    if(gps_offer(world,walker)!=MS_AVAILABLE||!gps_focused())return 0;GPSCar *c=gps_by_id(gps.last_car);if(!c)return 0;
    int ok=1;PTR player=call(gps.player,0,0,&ok);long long before=gps_balance(world,player,&ok);
    if(!ok||before<GPS_PRICE)return 0;
    int debited=gps_charge(world,player,-GPS_PRICE);long long after=gps_balance(world,player,&ok);
    if(!debited||!ok||after!=before-GPS_PRICE){
        /* A failed invoke can still have changed the wallet before throwing.
         * Only a confirmed unchanged balance permits another purchase attempt. */
        if(ok&&after==before)return 0;gps.invalid=1;return -1;
    }
    /* Purchases are serialized on the Unity thread. Keep the bounded staging
     * ledger off the small freestanding Windows stack (no CRT stack probe). */
    static GPSModuleSave next;next=gps.modules;
    if(!gm_grant(&next,c->storage)||!ss_save(gps.root,(const unsigned char *)&next,sizeof(next))){
        int refunded=gps_charge(world,player,GPS_PRICE);if(!refunded||gps_balance(world,player,&ok)!=before||!ok)gps.invalid=1;
        gps_text(c->status,sizeof(c->status),"Module save failed; purchase not granted");return -1;
    }
    gps.modules=next;gps_text(c->status,sizeof(c->status),"Autonomous module fitted; command on map");call(gps.request_save,world,0,&ok);gps_event("module-purchased",c->id);return 1;
}
static int gps_describe(const MoonMapContext *ctx,int requested,MoonMapRow *row){
    if(!gps_context(ctx->world,ctx->walker))return 0;
    GPSCar *selected=gps_by_id(requested);if(!selected)selected=gps_by_id(gps.last_car);
    for(int i=0;i<GPS_LIMIT;i++)if(gps.cars[i].car&&gps_alive(dget(gps.cars[i].car))){GPSCar *c=&gps.cars[i];MoonMapUnit *u=&row->units[row->unit_count++];u->id=c->id;char text[64],*p=cat(text,"#");p=num(p,(U64)c->id);p=cat(p," ");gps_text(p,(unsigned)(sizeof(text)-(p-text)),c->name);gps_text(u->label,sizeof(u->label),text);if(!selected)selected=c;}
    if(!selected){row->selected_id=-1;gps_text(row->status,sizeof(row->status),"Board a car to select it for free GPS");return 1;}
    row->selected_id=selected->id;int module=!gps.invalid&&gm_owned(&gps.modules,selected->storage),host=gps_host(),ok=1;
    int driven=boolean(gps.driven,dget(selected->car),&ok);if(!ok)driven=1;
    const char *labels[]={"Heading","Set A","Set B","Follow line","Auto to point","Stop"};
    for(int i=0;i<6;i++){MoonMapAction *a=&row->actions[row->action_count++];a->command=(unsigned)i+1;gps_text(a->label,sizeof(a->label),labels[i]);a->enabled=1;}
    row->actions[2].enabled=selected->has_a&&ctx->has_ground;row->actions[3].enabled=host&&module&&selected->has_route&&!selected->active&&!driven;row->actions[4].enabled=host&&module&&ctx->has_ground&&!selected->active&&!driven;row->actions[5].enabled=selected->active;
    if(selected->active&&selected->autonomous)for(int i=0;i<3;i++)row->actions[i].enabled=0;
    char status[128],*p=cat(status,module?"AUTO fitted | ":"GPS free | AUTO $750 | ");gps_text(p,(unsigned)(sizeof(status)-(p-status)),driven?"Close map; native cruise + K follows captured line":selected->status);gps_text(row->status,sizeof(row->status),status);return 1;
}
static int gps_map_command(const MoonMapContext *ctx,int id,unsigned command){
    if(!gps_context(ctx->world,ctx->walker)||!gps_focused())return 0;GPSCar *c=gps_by_id(id);if(!c)return 0;
    if(command==GPS_STOP){gps_stop(c,"Map stop",0);return 1;}
    if(c->active&&c->autonomous)return 0;
    if(command==GPS_HEADING)return gps_capture(c);
    if(command==GPS_A){SGInput in={0};if(!gps_pose(c,&in))return 0;gps_stop(c,"A set",1);c->a=ctx->has_ground?sg_vec(ctx->ground_point.x,ctx->ground_point.y,ctx->ground_point.z):in.position;c->has_a=1;gps_text(c->status,sizeof(c->status),"A set; right-click another point, Set B");return 1;}
    if(command==GPS_B){if(!ctx->has_ground||!c->has_a)return 0;SGRoute route;if(!sg_capture_ab(&route,sg_vec(0,0,0),c->a,sg_vec(ctx->ground_point.x,ctx->ground_point.y,ctx->ground_point.z)))return 0;gps_stop(c,"A/B line captured",1);c->route=route;c->has_route=1;gps_text(c->status,sizeof(c->status),"A/B line set; K while driving or unattended Follow line");gps_event("ab-captured",c->id);return 1;}
    /* Native map UI applies the occupied car's brake. Map driving commands
     * are unattended only; the driver explicitly starts with K after closing. */
    if(command==GPS_FOLLOW||command==GPS_TARGET){int ok=1;if(boolean(gps.driven,dget(c->car),&ok)||!ok){gps_text(c->status,sizeof(c->status),"Close map; native cruise + K follows captured line");return 0;}}
    if(command==GPS_FOLLOW)return gps_start(c);
    if(command==GPS_TARGET){
        if(!ctx->has_ground||!gps_host()||gps.invalid||!gm_owned(&gps.modules,c->storage)||!gps_local_owned(c))return 0;
        SGInput in={0};SGRoute route;if(!gps_pose(c,&in)||!sg_capture_destination(&route,sg_vec(0,0,0),in.position,sg_vec(ctx->ground_point.x,ctx->ground_point.y,ctx->ground_point.z)))return 0;
        gps_stop(c,"Destination set",1);c->route=route;c->has_route=1;gps_event("target-captured",c->id);return gps_start(c);
    }return 0;
}
static int gps_obstacle(GPSCar *c,const SGInput *in){
    SGVec up,right,forward;if(!sg_unit(sg_sub(in->position,c->route.center),&up,0,0)||!sg_unit(in->forward,&forward,0,0)||!sg_unit(sg_cross(up,forward),&right,0,0))return 1;
    float distance=(float)sg_clamp(2+in->speed*.4+in->speed*in->speed/4,3,25);int mask=~(1<<2),ignore=1,ok=1;
    /* Origin is ahead of the ordinary car's nose. These short guard rays
     * stop, rather than attempt navigation around terrain or obstacles. */
    for(int i=-1;i<=1;i++){
        SGVec o=sg_add(sg_add(in->position,sg_mul(forward,2.5)),sg_add(sg_mul(up,.9),sg_mul(right,i*.7)));
        DVec p={(float)o.x,(float)o.y,(float)o.z},d={(float)forward.x,(float)forward.y,(float)forward.z};union {U64 align;unsigned char data[128];} hit={0};
        void *a[]={&p,&d,hit.data,&distance,&mask,&ignore};PTR v=call(gps.ray,0,a,&ok);if(!ok||!v||*(unsigned char *)unbox(v))return 1;
    }return 0;
}
static int gps_control(void *vehicle,const MoonControlFrame *frame,MoonVehicleInput *proposal){
    GPSCar *c=gps_by_car(vehicle);if(!c||!c->active)return 0;int ok=1,driven=boolean(gps.driven,vehicle,&ok);U64 now=gps.clock();
    if(!ok||!gps_alive(dget(gps.world))||!gps_alive(vehicle)||!gps.controls->owns(GPS_KEY,vehicle)){gps_release(c,"Vehicle unavailable");return 0;}
    if(frame->manual_active||(c->autonomous&&driven)||(!c->autonomous&&(sg_abs(frame->input.steer)>.08||frame->input.brake||(gps.key('A')&0x8000)||(gps.key('D')&0x8000)||(gps.key(0x25)&0x8000)||(gps.key(0x27)&0x8000)))){gps_release(c,"Manual control");return 0;}
    if(boolean(gps.autopilot,vehicle,&ok)||!ok||(c->autonomous&&!gps.controls->can_unattended(vehicle))){gps_release(c,"Vehicle control changed");return 0;}
    if(!boolean(gps.grounded,vehicle,&ok)||!ok)gps_stop(c,"Ground contact lost",!c->autonomous);
    int mapopen=gps_alive(dget(gps.map))&&boolean(gps.map_open,dget(gps.map),&ok);
    int ui_blocked=mapopen?(!gps.maps||!gps.maps->context_active(dget(gps.walker))):(boolean(gps.menu,dget(gps.walker),&ok)||boolean(gps.blocked,dget(gps.walker),&ok));
    if(!gps_focused()||ui_blocked||boolean(gps.restoring,dget(gps.world),&ok)||!ok||(c->last_tick&&(now<c->last_tick||now-c->last_tick>250))){gps_stop(c,"Paused; stopped",!c->autonomous);}
    if(!c->active)return 0;c->last_tick=now;
    if(c->autonomous&&(!gps_host()||gps.invalid||!gm_owned(&gps.modules,c->storage))){gps_release(c,"Authority or module unavailable");return 0;}
    if(c->autonomous&&gps.controls->can_unattended_work(vehicle)&&boolean(gps.full,vehicle,&ok))gps_stop(c,"Dust storage full; stopped",0);
    if(!c->autonomous&&(!driven||!boolean(gps.cruise,vehicle,&ok)||!ok)){gps_release(c,"Native cruise stopped");return 0;}
    SGInput input={0};if(!gps_pose(c,&input)){gps_release(c,"Position unavailable");return 0;}
    if(c->autonomous&&!c->stopping&&now>=c->progress_time+4000){
        SGVec delta=sg_sub(input.position,c->progress_position);
        if(sg_dot(delta,delta)<.25)gps_stop(c,"Blocked or no traction; stopped",0);
        c->progress_time=now;c->progress_position=input.position;
    }
    if(!c->stopping){
        int status=sg_tick(&c->state,&c->route,&input,&c->output);
        if(status!=SG_TRACKING)gps_stop(c,status==SG_STOPPED?"Destination reached":"Route lost; stopped",!c->autonomous);
        if(c->active&&c->autonomous&&!c->stopping&&now>=c->next_probe){c->next_probe=now+100;if(gps_obstacle(c,&input))gps_stop(c,"Obstacle ahead; stopped",0);}
    }
    if(!c->active)return 0;
    if(c->stopping){if(sg_abs(input.speed)<.15){gps_release(c,c->status);return 0;}*proposal=(MoonVehicleInput){1,0,0,1,0};return 1;}
    *proposal=frame->input;proposal->valid=1;proposal->steer=(float)c->output.steer;
    if(c->autonomous){proposal->boost=0;proposal->brake=c->output.brake>.05;proposal->throttle=proposal->brake?0:(float)sg_clamp(.25+(c->output.speed_limit-input.speed)*.25,0,.75);}
    return 1;
}
#include "test_tools.h"
static int gps_update(int id,Context *ctx){
    (void)id;int ok=1,frame=integer(gps.frame,0,&ok);if(!ok||frame==gps.last_frame)return 0;gps.last_frame=frame;
    PTR walker=(PTR)ctx->rcx,world=0,mounted=0;field_value(walker,gps.world_f,&world);
    if(!gps_context(world,walker)){(void)moon_input_action("satellite-gps.toggle","!Ctrl+!Shift+!Alt+K",0);if(gps.world)for(int i=0;i<GPS_LIMIT;i++)if(gps.cars[i].active)gps_release(&gps.cars[i],"World paused");gps.was_eligible=0;return 0;}
    U64 now=gps.clock();if(now>=gps.next_scan){gps.next_scan=now+1000;gps_discover();}
    gps_test_tick(walker,now);
    field_value(walker,gps.mounted_f,&mounted);GPSCar *c=gps_by_car(mounted);if(c)gps.last_car=c->id;
    int eligible=c&&gps_focused()&&!boolean(gps.menu,walker,&ok)&&!boolean(gps.blocked,walker,&ok)&&!((gps.key(0x10)|gps.key(0x11)|gps.key(0x12))&0x8000)&&ok;
    MoonInputState toggle=moon_input_action("satellite-gps.toggle","!Ctrl+!Shift+!Alt+K",eligible);
    if(toggle.pressed&&eligible&&gps.was_eligible){if(c->active)gps_release(c,"GPS off");else{if(!c->has_route)(void)gps_capture(c);(void)gps_start(c);}}
    gps.was_eligible=eligible;
    if(eligible){char label[48],hint[192];if(!moon_input_label("satellite-gps.toggle","!Ctrl+!Shift+!Alt+K",label,sizeof(label)))gps_text(label,sizeof(label),"K");char *p=cat(hint,label);p=cat(p,": ");cat(p,c->active?"GPS off | Straight-line guidance":"GPS straight line | Map: A/B and autonomous controls");gps.hints->publish(GPS_KEY,walker,hint);}else gps.hints->clear(GPS_KEY);
    return 0;
}
__declspec(dllexport) int MoonLab_ModStart(PTR domain,PTR game){
    if(!bind_runtime(domain,game)||!dapi(game)||!ss_bind())return 0;
    gps.chars=(const unsigned short *(*)(PTR))GetProcAddress(game,"il2cpp_string_chars");gps.length=(int(*)(PTR))GetProcAddress(game,"il2cpp_string_length");
    PTR user=LoadLibraryA("user32.dll"),kernel=GetModuleHandleA("kernel32.dll");gps.key=(short(*)(int))GetProcAddress(user,"GetAsyncKeyState");gps.foreground=(PTR(*)(void))GetProcAddress(user,"GetForegroundWindow");gps.window_pid=(DWORD(*)(PTR,DWORD *))GetProcAddress(user,"GetWindowThreadProcessId");gps.clock=(U64(*)(void))GetProcAddress(kernel,"GetTickCount64");
    if(!gps.chars||!gps.length||!gps.key||!gps.foreground||!gps.window_pid||!gps.clock)return 0;
#define B(F,N,T,M,S) gps.F=dmethod(N,T,M,S);if(!gps.F)return 0
#define V(F,M) B(F,"MoonCleaners","MoonPrototypeVehicle",M,"")
    V(owner,"get_Owner");V(driven,"get_IsDriven");V(world_m,"get_World");V(storage,"get_StorageId");V(spec,"get_Spec");V(index,"get_Index");V(cruise,"get_CruiseOn");V(grounded,"get_Grounded");V(autopilot,"get_Autopilot");V(speed,"get_ForwardSpeed");V(full,"get_DustFull");B(set_cruise,"MoonCleaners","MoonPrototypeVehicle","SetCruise","System.Boolean");
    B(transform,"UnityEngine","Component","get_transform","");B(position,"UnityEngine","Transform","get_position","");B(forward,"UnityEngine","Transform","get_forward","");B(right,"UnityEngine","Transform","get_right","");B(up,"UnityEngine","Transform","get_up","");
    B(find,"UnityEngine","Object","FindObjectsByType","System.Type;UnityEngine.FindObjectsInactive;UnityEngine.FindObjectsSortMode");B(ray,"UnityEngine","Physics","Raycast","UnityEngine.Vector3;UnityEngine.Vector3;UnityEngine.RaycastHit&;System.Single;System.Int32;UnityEngine.QueryTriggerInteraction");
    B(fixed_dt,"UnityEngine","Time","get_fixedDeltaTime","");B(frame,"UnityEngine","Time","get_frameCount","");B(focus,"UnityEngine","Application","get_isFocused","");B(typing,"MoonCleaners","MoonInput","get_Typing","");B(blocked,"MoonCleaners","MoonWalker","get_UIBlocked","");B(menu,"MoonCleaners","MoonWalker","get_MenuBlocked","");B(restoring,"MoonCleaners","MoonWorld","get_IsRestoringRendering","");B(alive,"UnityEngine","Object","op_Implicit","UnityEngine.Object");
    B(component,"UnityEngine","Component","GetComponent","System.Type");B(map_open,"MoonCleaners","MoonMapScreen","get_IsOpen","");B(joined,"MoonCleaners","MoonCrew","get_Joined","");B(host,"MoonCleaners","MoonCrew","get_HostId","");B(local,"Steamworks","SteamUser","GetSteamID","");B(player,"MoonCleaners","MoonCrew","get_LocalPlayerId","");
    B(wallet,"MoonCleaners","MoonWorld","WalletDollars","System.String");B(charge,"MoonCleaners","MoonWorld","ChargeBuild","System.String;System.Decimal");B(decimal_int,"System","Decimal",".ctor","System.Int32");B(decimal_long,"System","Decimal","ToInt64","System.Decimal");B(save_root,"MoonCleaners","MoonWorld","get_SaveDirectory","");B(owned_at,"MoonCleaners","MoonWorld","OwnedVehicleAt","System.String");B(request_save,"MoonCleaners","MoonWorld","RequestSave","");
#undef V
#undef B
    PTR w=find_class("MoonCleaners","MoonWalker"),record=find_class("MoonCleaners","MoonOwnedVehicle"),catalog=find_class("MoonCleaners","MoonPrototypeCatalog"),nested=0;
    PTR (*children)(PTR,PTR *)=(PTR(*)(PTR,PTR *))GetProcAddress(game,"il2cpp_class_get_nested_types");const char *(*name)(PTR)=(const char *(*)(PTR))GetProcAddress(game,"il2cpp_class_get_name");PTR it=0,k;
    if(!children||!name||!catalog)return 0;while((k=children(catalog,&it)))if(deq(name(k),"Item"))nested=k;
    gps.world_f=get_field(w,"world");gps.mounted_f=get_field(w,"mountedVehicle");gps.record_owner_f=get_field(record,"owner");gps.spec_name_f=nested?get_field(nested,"name"):0;
    gps.vehicle_type=dtype("MoonCleaners","MoonPrototypeVehicle");gps.map_type=dtype("MoonCleaners","MoonMapScreen");
    int (*size)(PTR,DWORD *)=(int(*)(PTR,DWORD *))GetProcAddress(game,"il2cpp_class_value_size");DWORD align=0;PTR hit=find_class("UnityEngine","RaycastHit");if(!size||!hit||size(hit,&align)>128)return 0;
    if(!gps.world_f||!gps.mounted_f||!gps.record_owner_f||!gps.spec_name_f||!gps.vehicle_type||!gps.map_type)return 0;
    gps.controls=mc_connect(domain,game);gps.maps=mmc_connect(domain,game);gps.shop=ms_connect(domain,game);gps.hints=mh_connect(domain,game);if(!gps.controls||!gps.maps||!gps.shop||!gps.hints)return 0;
    MoonControlProvider controller={MC_ABI,sizeof(controller),GPS_KEY,MC_ASSIST,10,gps_control};MoonMapProvider map={MMC_ABI,sizeof(map),GPS_KEY,"Vehicles / GPS",gps_describe,gps_map_command};
    MoonShopDefinition offer={MS_ABI,sizeof(offer),GPS_KEY ".autonomous","Satellite Autonomous Module","$750 for the last car you drove and own. Enables host-controlled unattended driving on a GPS line or to a point in the map's Vehicles / GPS section. Driven straight-line GPS (K while native cruise is on) is free. Hover/custom physics vehicles are excluded.",GPS_PRICE,gps_offer,gps_buy};
    if(!gps.controls->register_provider(&controller)||!gps.maps->register_provider(&map)||!gps.shop->register_item(&offer))return 0;
    char folder[900],path[1000];DWORD n=GetEnvironmentVariableA("MOONLAB_EVIDENCE",folder,sizeof(folder));if(n&&n<sizeof(folder)){cat(cat(path,folder),"\\satellite-gps.jsonl");gps.log=CreateFileA(path,0x40000000,7,0,2,0x80,0);if(gps.log==(PTR)(U64)-1)gps.log=0;}
    if(!gps_test_bind())return 0;gps.last_frame=-1;gps.next_id=1;gps_event("ready",0);if(!prepare_hook(method("MoonCleaners","MoonWalker","Update",0),100671733,0,game))return 0;before_hook=gps_update;(void)integer;(void)num;return install_hooks();
}

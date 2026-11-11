/* Private qualification only. Both opt-in environment flags are required. */
static struct {int enabled;unsigned previous;PTR map_begin,enter,exit,garage_id,owned,recall,fitted,toggle_clean,walker_body,vehicle_holder,mounted,place,body_set,body_position,body_velocity,capsule,hit_point,hit_normal;} gt;
static int gps_test_bind(void){
    char flag[8];gt.enabled=GetEnvironmentVariableA("MOONLAB_TEST_TOOLS",flag,sizeof(flag))==1&&flag[0]=='1'&&GetEnvironmentVariableA("MOONLAB_GPS_TEST_TOOLS",flag,sizeof(flag))==1&&flag[0]=='1';
    if(!gt.enabled)return 1;
#define GT(F,T,N,S) gt.F=dmethod("MoonCleaners",T,N,S);if(!gt.F)return 0
    GT(map_begin,"MoonMapScreen","Open","");GT(enter,"MoonPrototypeVehicle","Enter","");GT(exit,"MoonPrototypeVehicle","Exit","");GT(garage_id,"MoonGarage","get_Id","");GT(owned,"MoonWorld","OwnedVehicle","System.Int32;System.Int32;System.String");GT(recall,"MoonWorld","RecallGarageVehicle","MoonCleaners.MoonOwnedVehicle;MoonCleaners.MoonGarage;System.String&");GT(fitted,"MoonPrototypeVehicle","set_CruiseFitted","System.Boolean");GT(toggle_clean,"MoonPrototypeVehicle","ToggleCleaning","");
    GT(mounted,"MoonWalker","get_Mounted","");GT(place,"MoonWalker","Place","UnityEngine.Vector3;UnityEngine.Vector3;System.Single");
#undef GT
#define GU(F,T,N,S) gt.F=dmethod("UnityEngine",T,N,S);if(!gt.F)return 0
    GU(body_set,"Rigidbody","set_position","UnityEngine.Vector3");GU(body_position,"Rigidbody","get_position","");GU(body_velocity,"Rigidbody","set_linearVelocity","UnityEngine.Vector3");
    GU(capsule,"Physics","CheckCapsule","UnityEngine.Vector3;UnityEngine.Vector3;System.Single;System.Int32;UnityEngine.QueryTriggerInteraction");GU(hit_point,"RaycastHit","get_point","");GU(hit_normal,"RaycastHit","get_normal","");
#undef GU
    gt.walker_body=get_field(find_class("MoonCleaners","MoonWalker"),"body");gt.vehicle_holder=get_field(find_class("MoonCleaners","MoonPrototypeVehicle"),"owner");
    return gt.walker_body&&gt.vehicle_holder;
}
static char *gps_decimal(char *p,double v){if(!sg_finite(v))return cat(p,"0");if(v<0){*p++='-';v=-v;}U64 whole=(U64)v;p=num(p,whole);unsigned frac=(unsigned)((v-whole)*1000);*p++='.';*p++=(char)('0'+frac/100);*p++=(char)('0'+frac/10%10);*p++=(char)('0'+frac%10);*p=0;return p;}
/* Enter enforces genuine player proximity. Stage only the disposable walker;
 * floor and capsule checks admit at most 16 poses beside the untouched car. */
static int gps_test_board_stage(PTR walker,GPSCar *c){
    PTR car=dget(c->car),body=0,holder=0;int ok=1;
    if(!gps_alive(car)||!gps_alive(walker)||boolean(gt.mounted,walker,&ok)||boolean(gps.blocked,walker,&ok)||boolean(gps.menu,walker,&ok)||!ok)return 0;
    field_value(walker,gt.walker_body,&body);field_value(car,gt.vehicle_holder,&holder);if(!gps_alive(body)||!gps_alive(holder))return 0;
    PTR tr=call(gps.transform,car,0,&ok);SGVec at=gps_vector(gps.position,tr,&ok),up,forward=gps_vector(gps.forward,tr,&ok),right;
    if(!ok||!sg_unit(at,&up,0,0)||!sg_unit(sg_sub(forward,sg_mul(up,sg_dot(forward,up))),&forward,0,0)||!sg_unit(sg_cross(up,forward),&right,0,0))return 0;
    static const float directions[8][2]={{1,0},{-1,0},{0,-1},{.7071f,-.7071f},{-.7071f,-.7071f},{0,1},{.7071f,.7071f},{-.7071f,.7071f}};
    const double radii[2]={3,4};
    for(int ring=0;ring<2;ring++){for(int i=0;i<8;i++){
        SGVec offset=sg_mul(sg_add(sg_mul(right,directions[i][0]),sg_mul(forward,directions[i][1])),radii[ring]),start=sg_add(sg_add(at,offset),sg_mul(up,2));
        DVec origin={(float)start.x,(float)start.y,(float)start.z},down={(float)-up.x,(float)-up.y,(float)-up.z};float length=6;int mask=~(1<<2),ignore=1;union {U64 align;unsigned char data[128];} hit={0};
        void *ray[]={&origin,&down,hit.data,&length,&mask,&ignore};PTR result=call(gps.ray,0,ray,&ok);if(!ok||!result||!*(unsigned char *)unbox(result))continue;
        DVec point=dvector(gt.hit_point,hit.data,&ok),normal=dvector(gt.hit_normal,hit.data,&ok);SGVec floor=sg_vec(point.x,point.y,point.z),normal_v=sg_vec(normal.x,normal.y,normal.z);
        /* Keep the candidate on this floor, excluding roof/underground hits. */
        if(!ok||!sg_vec_finite(floor)||!sg_vec_finite(normal_v)||sg_dot(normal_v,up)<.8||sg_abs(sg_dot(sg_sub(floor,at),up))>2)continue;
        SGVec low=sg_add(floor,sg_mul(up,.4)),high=sg_add(floor,sg_mul(up,1.5));DVec a={(float)low.x,(float)low.y,(float)low.z},b={(float)high.x,(float)high.y,(float)high.z};float radius=.3f;
        void *capsule[]={&a,&b,&radius,&mask,&ignore};result=call(gt.capsule,0,capsule,&ok);if(!ok||!result||*(unsigned char *)unbox(result))continue;
        SGVec target=sg_add(floor,sg_mul(up,.1)),facing;if(!sg_unit(sg_sub(at,target),&facing,0,0))continue;
        facing=sg_sub(facing,sg_mul(up,sg_dot(facing,up)));if(!sg_unit(facing,&facing,0,0))continue;
        DVec position={(float)target.x,(float)target.y,(float)target.z},look={(float)facing.x,(float)facing.y,(float)facing.z},zero={0};float pitch=0;
        void *set[]={&position},*velocity[]={&zero},*place[]={&position,&look,&pitch};call(gt.body_set,body,set,&ok);call(gt.body_velocity,body,velocity,&ok);call(gt.place,walker,place,&ok);
        DVec observed=dvector(gt.body_position,body,&ok);PTR owner_transform=call(gps.transform,holder,0,&ok);SGVec owner_position=gps_vector(gps.position,owner_transform,&ok),body_position=sg_vec(observed.x,observed.y,observed.z),error=sg_sub(body_position,target),delta=sg_sub(owner_position,at);
        /* Native Enter compares the equipment holder transform, not the camera.
         * A successful API invocation alone cannot qualify this staging. */
        int placed=ok&&sg_vec_finite(body_position)&&sg_vec_finite(owner_position)&&sg_dot(error,error)<.0625&&sg_dot(delta,delta)<42.25;
        if(gps.log){char line[512],*p=cat(line,"{\"event\":\"test-board-stage-readback\",\"car\":");p=num(p,(U64)c->id);p=cat(p,",\"placed\":");p=num(p,(U64)placed);p=cat(p,",\"body_error_sq\":");p=gps_decimal(p,sg_dot(error,error));p=cat(p,",\"holder_distance_sq\":");p=gps_decimal(p,sg_dot(delta,delta));p=cat(p,",\"body_position\":[");p=gps_decimal(p,body_position.x);p=cat(p,",");p=gps_decimal(p,body_position.y);p=cat(p,",");p=gps_decimal(p,body_position.z);p=cat(p,"],\"holder_position\":[");p=gps_decimal(p,owner_position.x);p=cat(p,",");p=gps_decimal(p,owner_position.y);p=cat(p,",");p=gps_decimal(p,owner_position.z);p=cat(p,"]}\n");DWORD written=0;WriteFile(gps.log,line,(DWORD)(p-line),&written,0);FlushFileBuffers(gps.log);}
        return placed;
    }}
    return 0;
}
static void gps_test_tick(PTR walker,U64 now){
    if(!gt.enabled)return;unsigned keys=0;
    int chord=(gps.key(0x11)&0x8000)&&(gps.key(0x10)&0x8000);if(chord){for(unsigned i=0;i<5;i++)if(gps.key(0x74+(int)i)&0x8000)keys|=1u<<i;if(gps.key('M')&0x8000)keys|=32;}
    unsigned pressed=keys&~gt.previous;gt.previous=keys;int ok=1;GPSCar *c=gps_by_id(gps.last_car);
    if(pressed&&gps_focused()&&gps_host()){
        if(pressed&32){call(gt.map_begin,dget(gps.map),0,&ok);gps_event(ok?"test-map-opened":"test-map-failed",0);}
        if(pressed&1){
            int inactive=0,sort=0,found=0;void *a[]={dtype("MoonCleaners","MoonGarage"),&inactive,&sort};DRoot objects=dhold(call(gps.find,0,a,&ok));U64 n=objects?da.array_length(dget(objects)):0;if(n>128)n=128;
            PTR player=call(gps.player,0,0,&ok);
            for(U64 i=0;i<n&&ok;i++){
                PTR garage=*(PTR *)darray(dget(objects),i);int id=integer(gt.garage_id,garage,&ok),index=0;void *lookup[]={&id,&index,player};PTR owned=call(gt.owned,dget(gps.world),lookup,&ok);if(!owned)continue;
                PTR message=0;void *recall[]={owned,garage,&message};PTR value=call(gt.recall,dget(gps.world),recall,&ok);found=ok&&value&&*(unsigned char *)unbox(value);break;
            }
            ddrop(&objects);gps.next_scan=0;gps_event(found?"test-owned-car-recalled":"test-owned-car-recall-unavailable",0);
        }
        if(pressed&2){
            GPSCar *nearest=0;double best=1e12;PTR tr=call(gps.transform,walker,0,&ok);SGVec at=gps_vector(gps.position,tr,&ok);
            for(int i=0;i<GPS_LIMIT;i++)if(gps.cars[i].car){SGInput input={0};int read=1;if(integer(gps.index,dget(gps.cars[i].car),&read)!=0||!read||!gps_local_owned(&gps.cars[i]))continue;if(gps_pose(&gps.cars[i],&input)){SGVec d=sg_sub(at,input.position);double distance=sg_dot(d,d);if(distance<best){best=distance;nearest=&gps.cars[i];}}}
            int changed=0;
            if(nearest){
                PTR car=dget(nearest->car);int driven=boolean(gps.driven,car,&ok);gps.last_car=nearest->id;
                if(ok&&(driven||gps_test_board_stage(walker,nearest))){
                    PTR value=call(driven?gt.exit:gt.enter,car,0,&ok);int accepted=ok&&value&&*(unsigned char *)unbox(value),after=boolean(gps.driven,car,&ok);
                    changed=ok&&accepted&&after!=driven;
                }else gps_event("test-board-stage-failed",nearest->id);
            }
            gps_event(changed?"test-board-action":"test-board-failed",nearest?nearest->id:0);
        }
        if((pressed&4)&&c){
            unsigned char yes=1;void *a[]={&yes};call(gt.fitted,dget(c->car),a,&ok);call(gps.set_cruise,dget(c->car),a,&ok);call(gt.toggle_clean,dget(c->car),0,&ok);gps_event(ok?"test-cruise-ready":"test-cruise-failed",c->id);
        }
        if((pressed&8)&&c){int bought=gps_buy(dget(gps.world),walker);gps_event(bought==1?"test-module-bought":"test-module-buy-failed",c->id);}
        if((pressed&16)&&c){call(gt.exit,dget(c->car),0,&ok);gps_event(ok?"test-car-exited":"test-exit-failed",c->id);}
    }
    if(now<gps.next_telemetry)return;gps.next_telemetry=now+500;
    for(int i=0;i<GPS_LIMIT;i++)if(gps.cars[i].car){
        c=&gps.cars[i];SGInput input={0};if(!gps_pose(c,&input))continue;ok=1;
        char b[1024],*p=cat(b,"{\"event\":\"car-sample\",\"car\":");p=num(p,(U64)c->id);p=cat(p,",\"time_ms\":");p=num(p,now);p=cat(p,",\"active\":");p=num(p,(U64)c->active);p=cat(p,",\"autonomous\":");p=num(p,(U64)c->autonomous);p=cat(p,",\"stopping\":");p=num(p,(U64)c->stopping);p=cat(p,",\"module\":");p=num(p,gm_owned(&gps.modules,c->storage));p=cat(p,",\"invalid\":");p=num(p,(U64)gps.invalid);p=cat(p,",\"owned\":");p=num(p,(U64)gps_local_owned(c));p=cat(p,",\"vehicle_index\":");p=num(p,(U64)integer(gps.index,dget(c->car),&ok));p=cat(p,",\"driven\":");p=num(p,(U64)boolean(gps.driven,dget(c->car),&ok));p=cat(p,",\"cruise\":");p=num(p,(U64)boolean(gps.cruise,dget(c->car),&ok));p=cat(p,",\"grounded\":");p=num(p,(U64)boolean(gps.grounded,dget(c->car),&ok));p=cat(p,",\"speed\":");p=gps_decimal(p,input.speed);p=cat(p,",\"cross_track\":");p=gps_decimal(p,c->output.cross_track_m);p=cat(p,",\"position\":[");p=gps_decimal(p,input.position.x);p=cat(p,",");p=gps_decimal(p,input.position.y);p=cat(p,",");p=gps_decimal(p,input.position.z);p=cat(p,"]}\n");DWORD n=0;if(gps.log){WriteFile(gps.log,b,(DWORD)(p-b),&n,0);FlushFileBuffers(gps.log);}
    }
}

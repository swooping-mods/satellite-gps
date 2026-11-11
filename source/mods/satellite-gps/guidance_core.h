/* Engine-independent, allocation-free spherical GPS guidance. No CRT imports.
 * The owning adapter arbitrates input, authority, obstacles and vehicle liveness.
 * All positions and the moon center must share one world coordinate frame. */
#ifndef MOON_SATELLITE_GUIDANCE_CORE_H
#define MOON_SATELLITE_GUIDANCE_CORE_H

typedef struct { double x,y,z; } SGVec;
typedef struct {
    SGVec center,normal,destination;
    double radius,speed_limit;
    int kind; /* SG_LINE or SG_DESTINATION; normal gives A -> B direction. */
} SGRoute;
typedef struct { double steer; } SGState;
typedef struct { SGVec position,forward; double dt,speed; } SGInput;
typedef struct {
    double steer,speed_limit,cross_track_m,heading_error_rad,remaining_m,brake;
    int stop;
    unsigned math_iterations; /* Actual fixed sqrt/atan loop iterations. */
} SGOutput;
enum { SG_LINE=1, SG_DESTINATION=2 };
enum { SG_INVALID=-1, SG_UNSUPPORTED=-2, SG_TRACKING=1, SG_STOPPED=2 };

static inline double sg_abs(double x){return x<0?-x:x;}
static inline double sg_clamp(double x,double lo,double hi){return x<lo?lo:x>hi?hi:x;}
static inline int sg_finite(double x){return x==x&&x>=-1e12&&x<=1e12;}
static inline int sg_vec_finite(SGVec v){return sg_finite(v.x)&&sg_finite(v.y)&&sg_finite(v.z);}
static inline SGVec sg_vec(double x,double y,double z){return (SGVec){x,y,z};}
static inline SGVec sg_add(SGVec a,SGVec b){return sg_vec(a.x+b.x,a.y+b.y,a.z+b.z);}
static inline SGVec sg_sub(SGVec a,SGVec b){return sg_vec(a.x-b.x,a.y-b.y,a.z-b.z);}
static inline SGVec sg_mul(SGVec a,double x){return sg_vec(a.x*x,a.y*x,a.z*x);}
static inline double sg_dot(SGVec a,SGVec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static inline SGVec sg_cross(SGVec a,SGVec b){return sg_vec(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);}
/* IEEE754 binary64 on the supported native x64 target. The exponent seed
 * bounds relative error independently of moon radius; six Newton steps
 * converge to binary64 precision without compiler sqrt/CRT calls. */
static inline double sg_sqrt(double x,unsigned *work){
    if(x<=0)return 0;
    union {double d;unsigned long long u;} v={.d=x};
    v.u=(v.u>>1)+0x1ff8000000000000ULL;
    double r=v.d;
    for(int i=0;i<6;i++){r=.5*(r+x/r);if(work)(*work)++;}
    return r;
}
/* atan range reduction keeps |z| <= tan(pi/8). Twelve fixed terms give
 * < 1e-11 rad absolute error, including very small cross-track angles. */
static inline double sg_atan2(double y,double x,unsigned *work){
    const double pi=3.14159265358979323846,quarter=.78539816339744830962;
    double a=sg_abs(y),b=sg_abs(x),z=0,base=0;
    int swap=a>b;
    if(a==0&&b==0)return 0;
    z=swap?b/a:a/b;
    if(z>.41421356237309504880){z=(z-1)/(z+1);base=quarter;}
    double square=z*z,term=z,sum=0;
    for(int i=0;i<12;i++){sum+=term/(2*i+1);term*=-square;if(work)(*work)++;}
    double result=base+sum;
    if(swap)result=pi*.5-result;
    if(x<0)result=pi-result;
    return y<0?-result:result;
}
static inline int sg_unit(SGVec v,SGVec *out,double *length,unsigned *work){
    if(!sg_vec_finite(v))return 0;
    double size=sg_sqrt(sg_dot(v,v),work);
    if(!sg_finite(size)||size<1e-9)return 0;
    *out=sg_mul(v,1/size);if(length)*length=size;return 1;
}
static inline int sg_surface(SGVec center,SGVec position,SGVec *up,double *radius,unsigned *work){
    if(!sg_vec_finite(center)||!sg_vec_finite(position))return 0;
    return sg_unit(sg_sub(position,center),up,radius,work);
}
static inline void sg_reset(SGState *s){if(s)*s=(SGState){0};}
static inline int sg_capture_heading(SGRoute *route,SGVec center,SGVec position,SGVec forward){
    SGRoute r={0};SGVec up,tangent;double radius;
    if(!route||!sg_vec_finite(forward)||!sg_surface(center,position,&up,&radius,0)||radius<1||radius>1e8)return 0;
    if(!sg_unit(sg_sub(forward,sg_mul(up,sg_dot(forward,up))),&tangent,0,0))return 0;
    r.center=center;r.normal=sg_cross(up,tangent);r.radius=radius;r.kind=SG_LINE;r.speed_limit=8;
    *route=r;return 1;
}
static inline int sg_capture_ab(SGRoute *route,SGVec center,SGVec a,SGVec b){
    SGRoute r={0};SGVec ua,ub,n;double ra,rb;
    if(!route||!sg_surface(center,a,&ua,&ra,0)||!sg_surface(center,b,&ub,&rb,0)||ra<1||ra>1e8||sg_abs(ra-rb)>20+ra*.05)return 0;
    double similarity=sg_dot(ua,ub),separation=sg_sqrt(sg_dot(sg_cross(ua,ub),sg_cross(ua,ub)),0);
    /* Almost identical and almost antipodal points do not define a reliable
     * directed great circle; reject instead of magnifying coordinate noise. */
    if(separation<1e-6||(similarity<0&&separation<1e-4)||!sg_unit(sg_cross(ua,ub),&n,0,0))return 0;
    r.center=center;r.normal=n;r.destination=ub;r.radius=ra;r.kind=SG_LINE;r.speed_limit=8;
    *route=r;return 1;
}
static inline int sg_capture_destination(SGRoute *route,SGVec center,SGVec from,SGVec target){
    SGRoute r;
    if(!route||!sg_capture_ab(&r,center,from,target))return 0;
    r.kind=SG_DESTINATION;*route=r;return 1;
}
/* Continuous lines may be traversed in either direction; the adapter chooses
 * the direction on activation. A destination always retains its endpoint. */
static inline int sg_reverse(SGRoute *route){
    if(!route||route->kind!=SG_LINE)return 0;
    route->normal=sg_mul(route->normal,-1);return 1;
}
static inline int sg_fail(SGState *s,SGOutput *out,int status){
    sg_reset(s);if(out){*out=(SGOutput){0};out->brake=1;out->stop=1;}return status;
}
static inline int sg_fail_work(SGState *s,SGOutput *out,int status,unsigned work){
    int result=sg_fail(s,out,status);if(out)out->math_iterations=work;return result;
}
/* No map scans, callbacks, allocation, catch-up loops or pathfinding. Input
 * speed is signed ground speed, not rigidbody magnitude. Steering is right
 * positive; the adapter must preserve the game's actual sign convention.
 * Unsupported heading errors refuse tight turns/U-turns. */
static inline int sg_tick(SGState *state,const SGRoute *route,const SGInput *in,SGOutput *out){
    if(!state||!route||!in||!out)return sg_fail(state,out,SG_INVALID);
    *out=(SGOutput){0};unsigned work=0;SGVec up,forward,normal,right,tangent;double radius;
    if((route->kind!=SG_LINE&&route->kind!=SG_DESTINATION)||!sg_finite(route->radius)||route->radius<1||route->radius>1e8||
       !sg_finite(route->speed_limit)||route->speed_limit<.5||route->speed_limit>15||!sg_finite(state->steer)||sg_abs(state->steer)>.380001||
       !sg_finite(in->dt)||in->dt<=0||in->dt>.06||!sg_finite(in->speed)||!sg_vec_finite(in->forward)||
       !sg_surface(route->center,in->position,&up,&radius,&work)||sg_abs(radius-route->radius)>20+route->radius*.05||
       !sg_unit(route->normal,&normal,0,&work))return sg_fail_work(state,out,SG_INVALID,work);
    /* Parked native bodies drift by millimetres per second on slopes.
     * Treat at most 5 cm/s backward drift as stationary, never as reverse guidance. */
    if(in->speed<-.05||in->speed>15)return sg_fail_work(state,out,SG_UNSUPPORTED,work);
    double speed=in->speed>0?in->speed:0;
    if(!sg_unit(sg_sub(in->forward,sg_mul(up,sg_dot(in->forward,up))),&forward,0,&work)||
       !sg_unit(sg_cross(normal,up),&tangent,0,&work))return sg_fail_work(state,out,SG_INVALID,work);
    right=sg_cross(up,forward);
    double sine=sg_clamp(sg_dot(up,normal),-1,1);
    /* A line is only a local tracking reference. The opposite side of the
     * moon is ambiguous and is never an invitation to turn around. */
    if(sg_abs(sine)>.5)return sg_fail_work(state,out,SG_UNSUPPORTED,work);
    out->cross_track_m=sg_atan2(sine,sg_sqrt(1-sine*sine,&work),&work)*route->radius;
    out->heading_error_rad=sg_atan2(sg_dot(right,tangent),sg_dot(forward,tangent),&work);
    if(sg_abs(out->heading_error_rad)>1.4)return sg_fail_work(state,out,SG_UNSUPPORTED,work);
    out->speed_limit=route->speed_limit;
    if(route->kind==SG_DESTINATION){
        SGVec target;
        if(!sg_unit(route->destination,&target,0,&work)||sg_abs(sg_dot(target,normal))>1e-5)return sg_fail_work(state,out,SG_INVALID,work);
        double cosine=sg_clamp(sg_dot(up,target),-1,1);
        SGVec cross=sg_cross(up,target);
        double distance=sg_atan2(sg_sqrt(sg_dot(cross,cross),&work),cosine,&work)*route->radius;
        out->remaining_m=sg_atan2(sg_dot(cross,normal),cosine,&work)*route->radius;
        if(distance<=1){state->steer=0;out->steer=0;out->speed_limit=0;out->brake=1;out->stop=1;out->math_iterations=work;return SG_STOPPED;}
        /* Passing an endpoint outside its arrival zone is a failed approach,
         * not success and not a command to make a U-turn. */
        if(out->remaining_m<=0)return sg_fail_work(state,out,SG_UNSUPPORTED,work);
        double space=sg_clamp(out->remaining_m-.8,0,1e9);
        /* 2 m/s^2 braking with 0.35s command/actuator allowance. */
        double safe=sg_sqrt(.49+4*space,&work)-.7;
        out->speed_limit=sg_clamp(safe,0,route->speed_limit);
    }
    if(speed>out->speed_limit+.1)out->brake=sg_clamp((speed-out->speed_limit)/2,0,1);
    double lookahead=sg_clamp(3+speed*.9,4,14);
    double correction=sg_clamp(sg_atan2(out->cross_track_m,lookahead,&work),-.65,.65);
    double angle=sg_clamp(out->heading_error_rad-correction,-.7,.7);
    double goal=sg_clamp(angle*.9,-.38,.38);
    state->steer+=sg_clamp(goal-state->steer,-in->dt*.9,in->dt*.9);
    out->steer=state->steer;out->math_iterations=work;return SG_TRACKING;
}
#endif

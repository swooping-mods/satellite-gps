#ifndef MOON_GPS_MODULE_STORE_H
#define MOON_GPS_MODULE_STORE_H
#define GM_CARS 64
#define GM_ID 80
typedef struct {unsigned magic,version,count,check;char ids[GM_CARS][GM_ID];} GPSModuleSave;
static inline unsigned gm_hash(const GPSModuleSave *s){
    const unsigned char *p=(const unsigned char *)s;unsigned h=2166136261u;
    for(unsigned i=0;i<sizeof(*s);i++)if(i<12||i>=16){h^=p[i];h*=16777619u;}return h;
}
static inline int gm_id_valid(const char *id){
    if(!id||!id[0])return 0;
    for(unsigned i=0;i<GM_ID;i++){if(!id[i])return 1;if((unsigned char)id[i]<32||(unsigned char)id[i]>=127)return 0;}return 0;
}
static inline int gm_equal(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static inline int gm_owned(const GPSModuleSave *s,const char *id){if(!gm_id_valid(id))return 0;for(unsigned i=0;i<s->count&&i<GM_CARS;i++)if(gm_equal(s->ids[i],id))return 1;return 0;}
static inline int gm_save_valid(const GPSModuleSave *s){
    if(s->magic!=0x47505341||s->version!=1||s->count>GM_CARS||s->check!=gm_hash(s))return 0;
    for(unsigned i=0;i<s->count;i++){
        if(!gm_id_valid(s->ids[i]))return 0;
        for(unsigned j=0;j<i;j++)if(gm_equal(s->ids[i],s->ids[j]))return 0;
    }return 1;
}
static inline int gm_grant(GPSModuleSave *s,const char *id){
    if(s->count>=GM_CARS||!gm_id_valid(id)||gm_owned(s,id))return 0;
    unsigned i=0;do{s->ids[s->count][i]=id[i];}while(id[i++]);s->count++;
    s->magic=0x47505341;s->version=1;s->check=gm_hash(s);return 1;
}
#endif

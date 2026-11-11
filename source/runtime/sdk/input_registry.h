#ifndef MOONLAB_INPUT_REGISTRY_H
#define MOONLAB_INPUT_REGISTRY_H
#include "input.h"
#define MOON_INPUT_ACTION_CAPACITY 512u
#define MOON_INPUT_CLAIM_CAPACITY 32u
/* Deliberately freestanding, bounded and allocation-free. All registry access
 * belongs to the game update thread, including binding configuration. */
typedef struct { unsigned key,required,forbidden; } MoonInputChord;
typedef struct {
    char name[MOON_INPUT_NAME_CAPACITY],defaults[MOON_INPUT_LABEL_CAPACITY],label[MOON_INPUT_LABEL_CAPACITY];
    MoonInputChord chord;
    int previous,held,inhibit;
} MoonInputAction;
typedef struct { char channel[96],owner[96];unsigned long long frame;int used; } MoonInputClaim;
typedef struct {
    MoonInputAction actions[MOON_INPUT_ACTION_CAPACITY];unsigned count;
    MoonInputClaim claims[MOON_INPUT_CLAIM_CAPACITY];
    int (*key)(unsigned);MoonInputResolve resolve;
    /* Optional adapter-owned coherent frame. This implementation structure is
     * private to each host; it does not change the public InputApiV1 table. */
    const unsigned char *snapshot;
} MoonInputRegistry;
static inline int mi_equal(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static inline int mi_copy(char *out,unsigned cap,const char *in){unsigned n=0;if(!in||!cap)return 0;while(in[n]){if(n+1>=cap)return 0;out[n]=in[n];n++;}out[n]=0;return 1;}
static inline int mi_name(const char *s){unsigned n=0;int dot=0;if(!s)return 0;while(*s){char c=*s++;if(c=='.')dot=1;if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.')||++n>=MOON_INPUT_NAME_CAPACITY)return 0;}return n&&dot;}
static inline unsigned mi_key(const char *s){
    if(s[0]&&!s[1]){unsigned c=(unsigned char)s[0];if(c>='a'&&c<='z')c-=32;if((c>='A'&&c<='Z')||(c>='0'&&c<='9'))return c;}
    static const struct { const char *name;unsigned key; } keys[]={
        {"LeftMouse",1},{"RightMouse",2},{"MiddleMouse",4},{"Mouse4",5},{"Mouse5",6},
        {"Backspace",8},{"Tab",9},{"Enter",13},{"Shift",16},{"Ctrl",17},{"Control",17},{"Alt",18},{"Pause",19},{"CapsLock",20},
        {"Escape",27},{"Esc",27},{"Space",32},{"PageUp",33},{"PageDown",34},{"End",35},{"Home",36},
        {"Left",37},{"Up",38},{"Right",39},{"Down",40},{"Insert",45},{"Delete",46},
        {"Semicolon",186},{"Equals",187},{"Comma",188},{"Minus",189},{"Period",190},{"Slash",191},{"Backquote",192},
        {"LeftBracket",219},{"Backslash",220},{"RightBracket",221},{"Quote",222},
        {"LeftShift",160},{"RightShift",161},{"LeftCtrl",162},{"RightCtrl",163},{"LeftAlt",164},{"RightAlt",165}
    };
    for(unsigned i=0;i<sizeof(keys)/sizeof(*keys);i++)if(mi_equal(s,keys[i].name))return keys[i].key;
    if(s[0]=='F'&&s[1]>='1'&&s[1]<='9'){unsigned n=(unsigned)(s[1]-'0');if(s[2]){if(s[2]<'0'||s[2]>'9'||s[3])return 0;n=n*10+(unsigned)(s[2]-'0');}if(n>=1&&n<=24)return 111+n;}
    /* VK<number> preserves configurable device-specific virtual keys. */
    if(s[0]=='V'&&s[1]=='K'&&s[2]){unsigned n=0;for(unsigned i=2;s[i];i++){if(s[i]<'0'||s[i]>'9'||i>4)return 0;n=n*10+(unsigned)(s[i]-'0');}if(n&&n<256)return n;}
    return 0;
}
static inline unsigned mi_modifier(const char *s){return mi_equal(s,"Ctrl")||mi_equal(s,"Control")?1u:mi_equal(s,"Shift")?2u:mi_equal(s,"Alt")?4u:0;}
static inline int mi_parse(const char *text,MoonInputChord *out){
    MoonInputChord chord={0};if(!text)return 0;if(mi_equal(text,"None")){*out=chord;return 1;}
    unsigned offset=0;for(unsigned part=0;part<5;part++){
        char token[32];unsigned n=0;while(text[offset]&&text[offset]!='+'){if(n+1>=sizeof(token))return 0;token[n++]=text[offset++];}token[n]=0;if(!n)return 0;
        int deny=token[0]=='!';const char *name=token+deny;unsigned modifier=mi_modifier(name);
        if(text[offset]=='+'){if(!modifier)return 0;if(deny)chord.forbidden|=modifier;else chord.required|=modifier;offset++;continue;}
        if(deny||!mi_key(name))return 0;
        chord.key=mi_key(name);if(chord.required&chord.forbidden)return 0;*out=chord;return 1;
    }return 0;
}
static inline int mi_down(MoonInputRegistry *r,unsigned key){return key&&key<256&&(r->snapshot?r->snapshot[key]!=0:r->key&&r->key(key));}
static inline int mi_chord_base(MoonInputRegistry *r,const MoonInputChord *c,unsigned modifiers){return mi_down(r,c->key)&&(modifiers&c->required)==c->required;}
static inline unsigned mi_modifiers(MoonInputRegistry *r){return (mi_down(r,17)?1u:0)|(mi_down(r,16)?2u:0)|(mi_down(r,18)?4u:0);}
static inline int mi_rebind(MoonInputRegistry *r,unsigned handle,const char *text){
    if(!handle||handle>r->count)return 0;
    MoonInputChord chord;char label[MOON_INPUT_LABEL_CAPACITY];
    if(!mi_parse(text,&chord)||!mi_copy(label,sizeof(label),text))return 0;
    MoonInputAction *a=r->actions+handle-1;a->chord=chord;mi_copy(a->label,sizeof(a->label),label);
    a->inhibit=mi_chord_base(r,&chord,mi_modifiers(r));a->previous=a->inhibit;a->held=0;return 1;
}
static inline void mi_resolve(MoonInputRegistry *r,unsigned handle){
    MoonInputAction *a=r->actions+handle-1;char text[MOON_INPUT_LABEL_CAPACITY]={0};
    if(r->resolve&&r->resolve(a->name,text,sizeof(text))){text[sizeof(text)-1]=0;(void)mi_rebind(r,handle,text);}
    else (void)mi_rebind(r,handle,a->defaults);
}
static inline unsigned mi_register(MoonInputRegistry *r,const char *name,const char *defaults){
    if(!mi_name(name))return 0;
    for(unsigned i=0;i<r->count;i++)if(mi_equal(name,r->actions[i].name))return i+1;
    MoonInputChord chord;if(r->count==MOON_INPUT_ACTION_CAPACITY||!mi_parse(defaults,&chord))return 0;
    MoonInputAction *a=r->actions+r->count;if(!mi_copy(a->defaults,sizeof(a->defaults),defaults))return 0;
    mi_copy(a->name,sizeof(a->name),name);mi_copy(a->label,sizeof(a->label),defaults);a->chord=chord;unsigned handle=++r->count;
    /* Default registration preserves a consumer's first observed edge. Only a
     * configured override is a binding change that inhibits a held new chord. */
    if(r->resolve){char text[MOON_INPUT_LABEL_CAPACITY]={0};if(r->resolve(a->name,text,sizeof(text))){text[sizeof(text)-1]=0;(void)mi_rebind(r,handle,text);}}
    return handle;
}
static inline MoonInputState mi_sample(MoonInputRegistry *r,unsigned handle,int allowed){
    MoonInputState state={0};if(!handle||handle>r->count)return state;MoonInputAction *a=r->actions+handle-1;unsigned modifiers=mi_modifiers(r);
    int base=mi_chord_base(r,&a->chord,modifiers),valid=base&&!(modifiers&a->chord.forbidden);
    if(!base)a->inhibit=0;
    state.held=allowed&&valid&&!a->inhibit;state.pressed=state.held&&!a->previous;state.released=a->held&&!state.held;
    /* Consume the base chord while gated or forbidden. Releasing a forbidden
     * modifier cannot invent an edge, nor can focus/menu/host return. */
    a->previous=base;a->held=state.held;return state;
}
static inline int mi_describe(MoonInputRegistry *r,unsigned index,MoonInputActionInfo *out){
    if(!out||out->size!=sizeof(*out)||index>=r->count)return 0;
    MoonInputAction *a=r->actions+index;out->handle=index+1;mi_copy(out->name,sizeof(out->name),a->name);mi_copy(out->binding,sizeof(out->binding),a->label);return 1;
}
static inline void mi_configure(MoonInputRegistry *r,MoonInputResolve resolve){r->resolve=resolve;for(unsigned i=0;i<r->count;i++)mi_resolve(r,i+1);}
static inline int mi_claim(MoonInputRegistry *r,const char *channel,unsigned long long frame,const char *owner){
    char checked_channel[96],checked_owner[96];if(!channel||!owner||!channel[0]||!owner[0]||!mi_copy(checked_channel,sizeof(checked_channel),channel)||!mi_copy(checked_owner,sizeof(checked_owner),owner))return 0;
    MoonInputClaim *empty=0;for(unsigned i=0;i<MOON_INPUT_CLAIM_CAPACITY;i++){MoonInputClaim *c=r->claims+i;if(!c->used){if(!empty)empty=c;continue;}if(mi_equal(c->channel,channel)){if(c->frame==frame)return mi_equal(c->owner,owner);c->frame=frame;mi_copy(c->owner,sizeof(c->owner),owner);return 1;}}
    if(!empty)return 0;
    mi_copy(empty->channel,sizeof(empty->channel),channel);mi_copy(empty->owner,sizeof(empty->owner),owner);empty->frame=frame;empty->used=1;return 1;
}
#endif

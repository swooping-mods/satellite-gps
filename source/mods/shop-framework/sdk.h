#ifndef MOON_SHOP_FRAMEWORK_V1_H
#define MOON_SHOP_FRAMEWORK_V1_H
#define MS_ABI 1u
#define MS_LIMIT 64u
#define MS_AVAILABLE 1u
#define MS_OWNED 2u
#define MS_BLOCKED 4u
/* Register at startup. Keys, strings and callbacks remain valid for the process.
 * Callbacks execute on the Unity Update thread. State is read-only; purchase
 * must recheck authority, balance and ownership, debit and persist the grant.
 * The framework never debits independently of that transaction. */
typedef struct {
    unsigned abi,size;
    const char *key,*name,*description;
    int price;
    unsigned (*state)(void *world,void *walker);
    int (*purchase)(void *world,void *walker);
} MoonShopDefinition;
typedef struct {
    unsigned abi,size;
    int (*register_item)(const MoonShopDefinition *definition);
    unsigned (*count)(void);
    const MoonShopDefinition *(*get)(unsigned index);
    unsigned (*state)(const char *key,void *world,void *walker);
    int (*purchase)(const char *key,void *world,void *walker);
    /* Separate modded context in the native menu; vanilla opens remain vanilla. */
    int (*open_modded)(void *walker);
} MoonShop;
typedef const MoonShop *(*MoonShopGet)(unsigned abi,unsigned size);
#endif

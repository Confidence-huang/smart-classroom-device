#include "net_recovery.h"
#include <assert.h>
#include <stdio.h>

typedef struct { unsigned probes,escapes,sets; uint32_t baud,answer_at; bool transient,escape_ok; } Model;
static int probe(void *context,uint32_t baud) {
 Model *m=context;m->probes++;m->baud=baud;
 return !m->transient && baud==m->answer_at?0:-2;
}
static bool escape(void *context) {Model *m=context;m->escapes++;return m->escape_ok;}
static void configure(void *context,uint32_t baud) {Model *m=context;m->sets++;m->baud=baud;}
static NetRecoveryIo io(Model *m) {NetRecoveryIo v={m,probe,escape,configure};return v;}
int main(void) {
 NetAtLink link={false,115200};Model m={0};m.answer_at=115200;NetRecoveryIo v=io(&m);
 assert(Net_FindAt(&link,&v) && link.known && link.baud==115200 && m.probes==1 && !m.escapes);
 m.transient=true;unsigned before=m.probes;
 assert(!Net_FindAt(&link,&v) && link.known && m.baud==115200 && m.probes==before+1 && !m.escapes);
 m.transient=false;assert(Net_FindAt(&link,&v) && m.probes==before+2 && !m.escapes);
 link=(NetAtLink){false,115200};m=(Model){0};v=io(&m);
 assert(!Net_FindAt(&link,&v) && !link.known && m.baud==115200 && m.probes==6 && m.escapes==1 && m.sets==1);
 link=(NetAtLink){false,115200};m=(Model){0};m.answer_at=57600;v=io(&m);
 assert(Net_FindAt(&link,&v) && link.known && link.baud==57600);
 m.transient=true;before=m.probes;assert(!Net_FindAt(&link,&v) && m.probes==before+1 && m.baud==57600 && m.escapes==1);
 link=(NetAtLink){false,115200};m=(Model){0};m.escape_ok=true;v=io(&m);
 assert(Net_FindAt(&link,&v) && link.known && link.baud==115200 && m.escapes==1);
 puts("PASS: confirmed-baud reconnect, no runtime PPP escape, startup fallback/restore, transient recovery and legacy compatibility");
 return 0;
}

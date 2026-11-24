#include "rx_cursor.h"
int RxCursor_Advance(RxCursor *c,uint16_t pos,uint16_t cap,RxSpan s[2]) {
 if(!c||!s||!cap||pos>=cap||c->position>=cap)return -1;
 uint16_t old=c->position;c->position=pos;
 if(old==pos)return 0;
 if(pos>old){s[0]=(RxSpan){old,(uint16_t)(pos-old)};return 1;}
 s[0]=(RxSpan){old,(uint16_t)(cap-old)};
 if(!pos)return 1;
 s[1]=(RxSpan){0,pos};return 2;
}

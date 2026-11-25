#include "rx_cursor.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
 RxCursor c={0};RxSpan s[2];
 assert(RxCursor_Advance(&c,256,512,s)==1 && s[0].offset==0 && s[0].length==256);
 assert(RxCursor_Advance(&c,256,512,s)==0);
 assert(RxCursor_Advance(&c,500,512,s)==1 && s[0].length==244);
 assert(RxCursor_Advance(&c,20,512,s)==2 && s[0].length==12 && s[1].length==20);
 assert(RxCursor_Advance(&c,0,512,s)==1 && s[0].length==492);
 assert(RxCursor_Advance(&c,513,512,s)==-1 && c.position==0);
 assert(RxCursor_Advance(&c,0,0,s)==-1);
 puts("PASS: DMA cursor partial, duplicate, wrap, full-boundary, invalid bounds");return 0;
}

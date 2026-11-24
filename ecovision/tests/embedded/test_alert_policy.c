#include "alert_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    AlertPolicy s={0};
    assert(AlertPolicy_Update(&s,0,false,true,false,0,0)==0);
    assert(AlertPolicy_Update(&s,4999,false,true,false,0,0)==0);
    assert(AlertPolicy_Update(&s,5000,false,true,false,0,0)==1);
    assert(AlertPolicy_Update(&s,6000,false,true,false,0,0)==0);
    assert(AlertPolicy_Update(&s,6100,true,true,false,0,0)==0);
    assert(AlertPolicy_Update(&s,6200,false,true,false,0,0)==1);
    assert(AlertPolicy_Update(&s,7000,true,true,false,4,2)==4);
    assert(AlertPolicy_Update(&s,7100,true,true,false,0,0)==0);
    assert(AlertPolicy_Update(&s,17000,true,true,false,4,2)==4);
    assert(AlertPolicy_Update(&s,17100,true,false,true,4,2)==2);
    assert(AlertPolicy_Update(&s,17200,true,false,true,4,2)==0);
    assert(AlertPolicy_Update(&s,17300,true,false,true,0,2)==2);
    /* Local console override and LOCAL_OFF never pretend to be cloud actions. */
    assert(AlertPolicy_Update(&s,17400,true,false,false,0,0)==0);
    assert(!AlertHealth_Stale(6000,1001));
    assert(AlertHealth_Stale(6000,1000));
    assert(!AlertHealth_Stale(20,0xfffffff0u));
    assert(AlertHealth_Stale(5000,0xfffffff0u));
    AlertPolicy wrap={0};
    assert(AlertPolicy_Update(&wrap,0xfffff000u,false,true,false,0,0)==0);
    assert(AlertPolicy_Update(&wrap,1000,false,true,false,0,0)==1);
    puts("PASS: alert transitions, deduplication, cooldown, source and health wraparound");
    return 0;
}

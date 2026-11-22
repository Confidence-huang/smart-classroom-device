#include "net_publish.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int begin_result;
    bool input_mode,write_ok,ack_ok;
    unsigned begins,writes,acks;
    size_t declared;
    uint16_t id;
    const char *expected_payload;
} Modem;
static int begin(void *context,const char *topic,uint16_t id,bool retain,size_t length) {
    Modem *m=context;m->begins++;m->declared=length;m->id=id;
    assert(!strcmp(topic,"ecovision/ev001/telemetry") && !retain);
    m->input_mode=m->begin_result==0 || m->begin_result==-2;
    return m->begin_result;
}
static bool write_payload(void *context,const char *data,size_t length) {
    Modem *m=context;m->writes++;
    /* Model the documented fixed-length input: AT recovery text here corrupts
     * the published message. Only the original payload may be consumed. */
    assert(m->input_mode && length==m->declared);
    assert(length==strlen(m->expected_payload) && !memcmp(data,m->expected_payload,length));
    if(m->write_ok)m->input_mode=false;
    return m->write_ok;
}
static bool wait_ack(void *context,uint16_t id) {
    Modem *m=context;m->acks++;assert(!m->input_mode && id==m->id);return m->ack_ok;
}
static Modem modem(int prompt) {
    Modem m={0};m.begin_result=prompt;m.write_ok=true;m.ack_ok=true;
    m.expected_payload="{\"count\":1,\"vision_valid\":true}";return m;
}
static bool publish(Modem *m,bool *recovery) {
    NetPublishIo io={m,begin,write_payload,wait_ack};
    return Net_PublishFixed(&io,"ecovision/ev001/telemetry",m->expected_payload,41,false,recovery);
}
int main(void) {
    bool recovery=false;Modem m=modem(-2);
    assert(publish(&m,&recovery) && recovery && m.writes==1 && m.acks==1);
    m=modem(0);assert(publish(&m,&recovery) && !recovery && m.writes==1 && m.acks==1);
    m=modem(-1);assert(!publish(&m,&recovery) && m.writes==0 && m.acks==0);
    m=modem(-4);assert(!publish(&m,&recovery) && m.writes==0 && m.acks==0);
    m=modem(-2);m.ack_ok=false;assert(!publish(&m,&recovery) && m.writes==1 && m.acks==1);
    m=modem(0);m.write_ok=false;assert(!publish(&m,&recovery) && m.writes==1 && m.acks==0);
    puts("PASS: missing-prompt fixed-length completion, one payload only, strict ACK, rejection and failed transport boundaries");
    return 0;
}

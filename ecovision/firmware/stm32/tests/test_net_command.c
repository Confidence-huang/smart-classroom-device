#include "net_command.h"
#include "control_policy.h" /* The original ON command keeps the default level. */
#include "net_codec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char command_json[]=
 "{\"v\":\"1.0.0\",\"device_id\":\"ev001\",\"boot_id\":\"0123456789abcdef\","
 "\"control_epoch\":4,\"cmd_id\":\"fan-1\",\"issued_at\":1795272586,\"ttl_s\":15,"
 "\"mode\":\"REMOTE\",\"fan_out\":1,\"led_out\":0,\"hold_s\":120}";

int main(void) {
 NetCommand command,other;
 assert(Net_ParseCommand(command_json,&command)==NET_COMMAND_OK);
 assert(command.fan_level==CONTROL_FAN_DEFAULT && command.fan==1 && command.led==0 && command.epoch==4 &&
        command.issued_at==1795272586ULL && command.ttl_s==15 && command.hold_s==120);

 /* A different wire key order and a JSON escape still represent the same command. */
 const char *reordered=
  "{\"cmd_id\":\"fan\\u002d1\",\"hold_s\":120,\"v\":\"1.0.0\",\"device_id\":\"ev001\","
  "\"boot_id\":\"0123456789abcdef\",\"issued_at\":1795272586,\"control_epoch\":4,"
  "\"ttl_s\":15,\"mode\":\"REMOTE\",\"led_out\":0,\"fan_out\":1}";
 assert(Net_ParseCommand(reordered,&other)==NET_COMMAND_OK && Net_SameCommand(&command,&other));
 other.hold_s=30;
 assert(!Net_SameCommand(&command,&other));

 char changed[sizeof command_json+40];
 /* Compare absolute settings when the same command ID is delivered again. */
 for(unsigned level=1;level<=CONTROL_FAN_MAX;level++) {
  snprintf(changed,sizeof changed,"{\"fan_level\":%u,%s",level,command_json+1);
  assert(Net_ParseCommand(changed,&other)==NET_COMMAND_INVALID);
  memcpy(strstr(changed,"1.0.0"),"1.1.0",5);
  assert(Net_ParseCommand(changed,&other)==NET_COMMAND_OK && other.fan_level==(int)level);
  assert(other.fan_levels_version && !Net_SameCommand(&command,&other));
 }
 snprintf(changed,sizeof changed,"{\"fan_level\":0,%s",command_json+1);
 memcpy(strstr(changed,"1.0.0"),"1.1.0",5);
 assert(Net_ParseCommand(changed,&other)==NET_COMMAND_INVALID);
 snprintf(changed,sizeof changed,"{\"fan_level\":6,%s",command_json+1);
 memcpy(strstr(changed,"1.0.0"),"1.1.0",5);
 assert(Net_ParseCommand(changed,&other)==NET_COMMAND_INVALID);
 snprintf(changed,sizeof changed,"{\"fan_level\":5,\"fan_level\":4,%s",command_json+1);
 memcpy(strstr(changed,"1.0.0"),"1.1.0",5);
 assert(Net_ParseCommand(changed,&other)==NET_COMMAND_INVALID);
 strcpy(changed,command_json);
 memcpy(strstr(changed,"1.0.0"),"2.0.0",5);
 assert(Net_ParseCommand(changed,&other)==NET_COMMAND_UNSUPPORTED);
 strcpy(changed,command_json);
 strstr(changed,"120")[0]='0';
 assert(Net_ParseCommand(changed,&other)==NET_COMMAND_INVALID);
 snprintf(changed,sizeof changed,"{\"ttl_s\":2,%s",command_json+1);
 assert(Net_ParseCommand(changed,&other)==NET_COMMAND_INVALID);
 snprintf(changed,sizeof changed,"{\"unknown\":2,%s",command_json+1);
 assert(Net_ParseCommand(changed,&other)==NET_COMMAND_INVALID);
 snprintf(changed,sizeof changed,"%s{}",command_json);
 assert(Net_ParseCommand(changed,&other)==NET_COMMAND_INVALID);
 const char *automatic=
  "{\"v\":\"1.0.0\",\"device_id\":\"ev001\",\"boot_id\":\"0123456789abcdef\","
  "\"control_epoch\":4,\"cmd_id\":\"auto-1\",\"issued_at\":1795272586,\"ttl_s\":15,"
  "\"mode\":\"AUTO\",\"fan_out\":null,\"led_out\":null,\"hold_s\":0}";
 assert(Net_ParseCommand(automatic,&other)==NET_COMMAND_OK && other.fan==-1 && other.hold_s==0);
 strcpy(changed,automatic);
 memcpy(strstr(changed,"1.0.0"),"1.1.0",5);
 assert(Net_ParseCommand(changed,&other)==NET_COMMAND_OK && other.fan_levels_version);
 strcpy(changed,command_json);
 memcpy(strstr(changed,"1.0.0"),"1.1.0",5);
 assert(Net_ParseCommand(changed,&other)==NET_COMMAND_INVALID);

 char urc[300];
 const char *topic="ecovision/ev001/cmd";
 snprintf(urc,sizeof urc,"+QMTRECV: 0,12,\"%s\",%u,\"%s\"",
          topic,(unsigned)strlen(command_json),command_json);
 assert(Net_CommandUrc(urc,"ecovision/ev002/cmd")==NULL);
 char *json=Net_CommandUrc(urc,topic);
 assert(json && Net_ParseCommand(json,&other)==NET_COMMAND_OK && Net_SameCommand(&command,&other));
 snprintf(urc,sizeof urc,"+QMTRECV: 0,12,\"%s\",%u,%s",
          topic,(unsigned)strlen(command_json),command_json);
 assert(Net_CommandUrc(urc,topic));
 snprintf(urc,sizeof urc,"+QMTRECV: 0,12,\"%s\",10,%s",topic,command_json);
 assert(Net_CommandUrc(urc,topic)==NULL);
 assert(Net_CommandUrc(urc,"")==NULL);
 puts("PASS: command framing, exact topic/length, semantic validation, canonical dedup identity");
 return 0;
}

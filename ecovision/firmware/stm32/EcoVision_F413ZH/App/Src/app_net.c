/* EC200U network waits run only in this task; no credential or AT echo logging. */
#include "app_net.h"
#include "net_codec.h"
#include "net_command.h"
#include "net_publish.h"
#include "net_recovery.h"
#include "app_log.h"
#include "app_environment.h"
#include "app_vision.h"
#include "app_fan.h"
#include "app_led.h"
#include "app_control.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern UART_HandleTypeDef huart6;
enum {RX_CAP=4096, COMMAND_QUEUE=4, COMMAND_CACHE=32};
static volatile uint16_t head,tail;
static volatile bool rx_fault;
static volatile uint32_t received_bytes;
static volatile uint32_t receive_arm_failures,receive_errors,last_receive_error;
static uint8_t ring[RX_CAP],rx_byte;
static NetAtParser parser;
static NetConfig config;
static char command_buffer[640],payload[1501],topic[96],boot[33];
static char pdp_profile[384];
static const char * volatile state="INIT";
static uint16_t message_id;
static uint32_t sequence,status_sequence,utc_base,utc_tick;
static bool clock_valid;
static char matched_line[512];
static char command_topic[55],ack_topic[55];
static bool receive_commands,command_subscribed;
static uint32_t subscription_utc;

/* Queue modem deliveries independently of AT replies; the network task owns both. */
typedef struct {
 NetCommand command;
 NetCommandParse parsed;
 uint32_t utc,tick;
} ReceivedCommand;
static ReceivedCommand inbox[COMMAND_QUEUE],working;
static unsigned inbox_start,inbox_size;
static bool awaiting_output;

/* Cache only successful applications so duplicate delivery never renews a lease. */
typedef struct {
 ReceivedCommand received;
 ControlOutput output;
} AppliedCommand;
static AppliedCommand applied_cache[COMMAND_CACHE];
static unsigned reserved_cache;

static uint32_t utc_now(void);

static void receive_command(char *line) {
 if(!command_subscribed)return;
 char *json=Net_CommandUrc(line,command_topic);
 if(!json)return;
 if(inbox_size==COMMAND_QUEUE){AppLog_Printf("[NET] command inbox full\r\n");return;}

 ReceivedCommand *item=&inbox[(inbox_start+inbox_size)%COMMAND_QUEUE];
 item->parsed=Net_ParseCommand(json,&item->command);
 /* Reply only to this device and a command id that can safely be correlated. */
 if(strcmp(item->command.device,config.device)!=0 || !Net_CommandId(item->command.id))return;
 item->utc=clock_valid?utc_now():0;
 item->tick=HAL_GetTick();
 inbox_size++;
}

/* Preserve complete and partial downlink frames between ordinary AT commands. */
static bool drain_online_uart(void) {
 if(rx_fault)return false;
 while(tail!=head) {
  uint8_t byte=ring[tail];tail=(uint16_t)((tail+1U)%RX_CAP);
  NetAtEvent event=NetAt_Feed(&parser,byte);
  if(event==NET_AT_DROPPED)return false;
  if(event!=NET_AT_LINE)continue;
  if(!strncmp(parser.line,"+QMTSTAT:",9))return false;
  receive_command(parser.line);
 }
 return true;
}

const char *AppNet_State(void){return state;}
static void stage(const char *value){state=value;AppLog_Printf("[NET] state=%s\r\n",value);}
static void arm(void){if(HAL_UART_Receive_IT(&huart6,&rx_byte,1)!=HAL_OK){receive_arm_failures++;rx_fault=true;}}
void AppNet_RxComplete(UART_HandleTypeDef *uart) {
 if(uart!=&huart6)return;
 uint16_t next=(uint16_t)((head+1U)%RX_CAP);
 received_bytes++;
 if(next==tail)rx_fault=true;else{ring[head]=rx_byte;head=next;}arm();
}
void AppNet_RxError(UART_HandleTypeDef *uart){if(uart==&huart6){last_receive_error=uart->ErrorCode;receive_errors++;rx_fault=true;}}
static void discard(void) {
 HAL_NVIC_DisableIRQ(USART6_IRQn);tail=head;rx_fault=false;
 (void)HAL_UART_AbortReceive(&huart6);arm();HAL_NVIC_EnableIRQ(USART6_IRQn);
 memset(&parser,0,sizeof parser);
}
static bool transmit(const void *p,size_t n) {
 return n<=65535U && HAL_UART_Transmit(&huart6,(uint8_t *)p,(uint16_t)n,600U)==HAL_OK;
}
/* A text result is accepted only together with final OK; asynchronous URCs
 * may precede or follow OK. A bare OK cannot confirm MQTT CONNECT/PUBACK. */
static int transaction(const char *command,const char *expected,uint32_t timeout) {
 uint32_t received_before=received_bytes;
 bool ok=false,match=expected==NULL;
 bool prompt=expected && !strcmp(expected,">");
 if(command) {
  if(command_subscribed) {
   if(!drain_online_uart())return -5;
  } else discard();
  if(!transmit(command,strlen(command))||!transmit("\r",1))return -4;
 }
 matched_line[0]=0;uint32_t start=HAL_GetTick();
 while((uint32_t)(HAL_GetTick()-start)<timeout) {
  if(rx_fault){discard();return -3;}
  while(tail!=head) {
   uint8_t byte=ring[tail];tail=(uint16_t)((tail+1U)%RX_CAP);
   NetAtEvent event=NetAt_Feed(&parser,byte);
   if(event==NET_AT_DROPPED)return -3;
   if(event==NET_AT_PROMPT && prompt)return 0;
   if(event!=NET_AT_LINE)continue;
   if(!strncmp(parser.line,"+QMTRECV:",9)) {
    receive_command(parser.line);
    continue;
   }
   const char *line=parser.line;
   if(!strcmp(line,"ERROR")||!strncmp(line,"+CME ERROR",10)||!strncmp(line,"+CMS ERROR",10)){AppLog_Printf("[NET] modem result=%s\r\n",line);return -1;}
   if(!strncmp(line,"+QMTSTAT:",9)){AppLog_Printf("[NET] modem result=%s\r\n",line);return -5;}
   if(!strcmp(line,"OK"))ok=true;
   if(expected && strncmp(line,expected,strlen(expected))==0) {
    match=true;(void)snprintf(matched_line,sizeof matched_line,"%s",line);
    if(!strcmp(expected,"CONNECT"))return 0;
   }
   if(ok && match && !prompt)return 0;
  }
  osDelay(2U);
 }
 AppLog_Printf("[NET] timeout expect=%s ok=%u match=%u rx=%lu SR=%lx CR1=%lx err=%lx used=%u drop=%u\r\n",expected?expected:"OK",ok?1U:0U,match?1U:0U,(unsigned long)(received_bytes-received_before),(unsigned long)huart6.Instance->SR,(unsigned long)huart6.Instance->CR1,(unsigned long)huart6.ErrorCode,(unsigned)parser.used,parser.dropping?1U:0U);
 AppLog_Printf("[NET] UART baud=%lu brr=%lx CR3=%lx RxState=%lx RxCount=%u irq=%u arm_fail=%lu errors=%lu last_err=%lx\r\n",(unsigned long)huart6.Init.BaudRate,(unsigned long)huart6.Instance->BRR,(unsigned long)huart6.Instance->CR3,(unsigned long)huart6.RxState,(unsigned)huart6.RxXferCount,(unsigned)NVIC_GetEnableIRQ(USART6_IRQn),(unsigned long)receive_arm_failures,(unsigned long)receive_errors,(unsigned long)last_receive_error);
 return -2;
}
static bool simple(const char *cmd){return transaction(cmd,NULL,5000U)==0;}
static bool configure_at_rate(uint32_t baud) {
 (void)HAL_UART_AbortReceive(&huart6);huart6.Init.BaudRate=baud;
 if(HAL_UART_Init(&huart6)!=HAL_OK) {
  AppLog_Printf("[NET] UART reconfigure failed baud=%lu\r\n",(unsigned long)baud);return false;
 }
 return true;
}
static void configure_at(void *context,uint32_t baud) {
 (void)context;(void)configure_at_rate(baud);
}
static int probe_at(void *context,uint32_t baud) {
 (void)context;if(!configure_at_rate(baud))return -4;
 uint32_t before=received_bytes;
 int result=transaction("AT",NULL,1800U);
 AppLog_Printf("[NET] AT probe baud=%lu result=%d rx_bytes=%lu rx_pin=%u\r\n",(unsigned long)baud,result,(unsigned long)(received_bytes-before),(unsigned)HAL_GPIO_ReadPin(GPIOG,GPIO_PIN_9));
 return result;
}
static bool escape_initial_ppp(void *context) {
 (void)context;
 /* Compatibility with the original MicroPython PPP entry is startup-only. */
 osDelay(1200U);(void)transmit("+++",3);osDelay(1500U);
 int result=transaction("AT",NULL,1800U);
 AppLog_Printf("[NET] initial guarded data-mode escape result=%d\r\n",result);
 if(!result){(void)transaction("ATH",NULL,3000U);return true;}
 return false;
}
static bool find_at(void) {
 static NetAtLink link={false,115200U};
 const NetRecoveryIo io={NULL,probe_at,escape_initial_ppp,configure_at};
 return Net_FindAt(&link,&io);
}
static bool wait_result(const char *cmd,const char *prefix,size_t count,uint32_t timeout,int *v) {
 return transaction(cmd,prefix,timeout)==0 && Net_Result(matched_line,prefix,v,count);
}
static bool random_id(void) {
 __HAL_RCC_RNG_CLK_ENABLE();RNG->CR=RNG_CR_RNGEN;
 for(unsigned i=0;i<4;i++) {
  uint32_t start=HAL_GetTick();while(!(RNG->SR&RNG_SR_DRDY)) {
   if(HAL_GetTick()-start>=100U || (RNG->SR&(RNG_SR_CECS|RNG_SR_SECS)))return false;
   osDelay(1U);
  }
  uint32_t v=RNG->DR;(void)snprintf(boot+i*8,9,"%08lx",(unsigned long)v);
 }
 return true;
}
static bool sync_clock(void) {
 uint32_t epoch=0;
 bool trusted=transaction("AT+QLTS=2","+QLTS:",5000U)==0 && Net_ClockUTC(matched_line,&epoch);
 if(!trusted) {
  if(transaction("AT+QNTP=1,\"pool.ntp.org\"","+QNTP:",65000U)||strncmp(matched_line,"+QNTP: 0,",9))return false;
  if(transaction("AT+CCLK?","+CCLK:",5000U)||!Net_ClockUTC(matched_line,&epoch))return false;
 }
 utc_base=epoch;utc_tick=HAL_GetTick();clock_valid=true;
 AppLog_Printf("[NET] trusted_utc=%lu source=%s\r\n",(unsigned long)epoch,trusted?"NETWORK_NITZ":"NTP");return true;
}
static uint32_t utc_now(void){return utc_base+(uint32_t)(HAL_GetTick()-utc_tick)/1000U;}
static int begin_publish(void *context,const char *name,uint16_t id,bool retain,size_t length) {
 (void)context;
 int n=snprintf(command_buffer,sizeof command_buffer,"AT+QMTPUBEX=0,%u,1,%u,\"%s\",%u",id,retain?1U:0U,name,(unsigned)length);
 if(n<0 || (size_t)n>=sizeof command_buffer)return -4;
 return transaction(command_buffer,">",10000U);
}
static bool write_publish_payload(void *context,const char *data,size_t length) {
 (void)context;return transmit(data,length);
}
static bool finish_publish(void *context,uint16_t id) {
 (void)context;int v[3]={-1,-1,-1};bool ack=wait_result(NULL,"+QMTPUBEX:",3,20000U,v);
 if(!ack||v[0]!=0||v[1]!=id||v[2]!=0){AppLog_Printf("[NET] PUBACK failed matched=%u result=%d,%d,%d expected_id=%u\r\n",ack?1U:0U,v[0],v[1],v[2],id);return false;}
 return true;
}
static bool publish(const char *name,const char *data,bool retain) {
 if(++message_id==0)message_id=1;
 const NetPublishIo io={NULL,begin_publish,write_publish_payload,finish_publish};
 bool missing_prompt=false;
 bool result=Net_PublishFixed(&io,name,data,message_id,retain,&missing_prompt);
 if(missing_prompt)AppLog_Printf("[NET] missing prompt; completed declared payload once; matching PUBACK=%u id=%u\r\n",result?1U:0U,message_id);
 return result;
}
static bool publish_ack(const ReceivedCommand *item, const ControlOutput *output,
                        const char *reason) {
 char received_at[16]="null";
 if(item->utc)(void)snprintf(received_at,sizeof received_at,"%lu",(unsigned long)item->utc);
 int size=snprintf(payload,sizeof payload,
  "{\"v\":\"1.0.0\",\"device_id\":\"%s\",\"boot_id\":\"%s\",\"cmd_id\":\"%s\","
  "\"status\":\"%s\",\"reason\":\"%s\",\"received_at\":%s,\"uptime_ms\":%lu,"
  "\"control_epoch\":%lu,\"mode\":\"%s\",\"fan_out\":%u,\"fan_level\":%u,\"led_out\":%u,\"hold_remaining_s\":%u}",
  config.device,boot,item->command.id,strcmp(reason,"OK")==0?"APPLIED":"REJECTED",
  reason,received_at,(unsigned long)item->tick,(unsigned long)output->control_epoch,
  Control_ModeName(output->mode),output->fan_level?1U:0U,(unsigned)output->fan_level,output->led?1U:0U,(unsigned)output->hold_remaining_s);
 if(size<0 || (size_t)size>=sizeof payload)return false;
 bool sent=publish(ack_topic,payload,false);
 AppLog_Printf("[NET] command ACK reason=%s puback=%u\r\n",reason,sent?1U:0U);
 return sent;
}

static bool finish_command(bool *refresh) {
 ControlReply reply;
 if(!AppControl_TakeReply(&reply))return true;
 awaiting_output=false;
 const char *reason="LOCAL_LOCKED";
 if(reply.result==CONTROL_APPLIED) {
  applied_cache[reserved_cache]=(AppliedCommand){working,reply.output};
  *refresh=true;
  reason="OK";
 } else if(reply.result==CONTROL_EXPIRED)reason="EXPIRED";
 else if(reply.result==CONTROL_EPOCH_MISMATCH)reason="EPOCH_MISMATCH";
 return publish_ack(&working,&reply.output,reason);
}

static const char *command_rejection(const ReceivedCommand *item,const ControlOutput *output,
                                     uint32_t now,uint32_t expire) {
 const NetCommand *cmd=&item->command;
 if(item->parsed==NET_COMMAND_INVALID)return "INVALID_MESSAGE";
 if(item->parsed==NET_COMMAND_UNSUPPORTED)return "UNSUPPORTED_VERSION";
 if(strcmp(cmd->boot,boot)!=0)return "BOOT_MISMATCH";
 if(!clock_valid || !item->utc)return "TIME_UNTRUSTED";
 if(cmd->issued_at>item->utc)return "FUTURE_COMMAND";
 if(cmd->issued_at<subscription_utc || (int32_t)(now-expire)>=0)return "EXPIRED";
 if(cmd->epoch!=output->control_epoch)return "EPOCH_MISMATCH";
 if(output->mode==CONTROL_LOCAL_OFF)return "LOCAL_LOCKED";
 return NULL;
}

static bool service_commands(bool *refresh) {
 if(!command_subscribed)return true;
 if(awaiting_output)return finish_command(refresh);
 if(!inbox_size)return true;

 working=inbox[inbox_start];
 inbox_start=(inbox_start+1U)%COMMAND_QUEUE;inbox_size--;
 const NetCommand *cmd=&working.command;
 ControlOutput current;AppControl_GetSnapshot(&current);
 uint32_t now=HAL_GetTick(),utc=utc_base+(now-utc_tick)/1000U;
 unsigned free_slot=COMMAND_CACHE;
 if(working.parsed==NET_COMMAND_OK && strcmp(cmd->boot,boot)==0) {
  for(unsigned i=0;i<COMMAND_CACHE;i++) {
   AppliedCommand *entry=&applied_cache[i];
   const NetCommand *old=&entry->received.command;
   if(old->id[0] && strcmp(old->id,cmd->id)==0) {
    if(Net_SameCommand(old,cmd))return publish_ack(&entry->received,&entry->output,"OK");
    return publish_ack(&working,&current,"CMD_ID_CONFLICT");
   }
   if(!old->id[0] || (uint64_t)utc>=old->issued_at+old->ttl_s)free_slot=i;
  }
 }

 /* The output task uses this same monotonic deadline if scheduling delays the write. */
 uint64_t expires_utc=cmd->issued_at+cmd->ttl_s;
 uint32_t expires_tick=now;
 if(expires_utc>utc && expires_utc-utc<=30U)
  expires_tick=now+(uint32_t)(expires_utc-utc)*1000U-(now-utc_tick)%1000U;
 const char *rejection=command_rejection(&working,&current,now,expires_tick);
 if(rejection)return publish_ack(&working,&current,rejection);
 if(free_slot==COMMAND_CACHE)return publish_ack(&working,&current,"BUSY");

 ControlCommand request={.epoch=cmd->epoch,.expires_at=expires_tick,.hold_s=cmd->hold_s,
                         .fan_level=cmd->hold_s?(uint8_t)cmd->fan_level:CONTROL_FAN_OFF,.led=cmd->led==1};
 memcpy(request.id,cmd->id,sizeof request.id);
 if(!AppControl_Queue(&request))return publish_ack(&working,&current,"BUSY");
 reserved_cache=free_slot;
 awaiting_output=true;
 return true;
}

static bool configure_tls(void) {
 /* The modem remains powered across MCU reset: close the previous socket
  * before replacing its CA file or changing the active TLS context. */
 (void)transaction("AT+QMTCLOSE=0","+QMTCLOSE:",10000U);
 osDelay(100U);
 (void)transaction("AT+QFDEL=\"UFS:ev-ca.pem\"",NULL,5000U);
 (void)snprintf(command_buffer,sizeof command_buffer,"AT+QFUPL=\"UFS:ev-ca.pem\",%u,10",config.ca_length);
 if(transaction(command_buffer,"CONNECT",5000U)||!transmit(config.ca,config.ca_length)||transaction(NULL,NULL,15000U))return false;
 /* Explicitly retain all certificate checks, including the expected CN. */
 const char *commands[]={"AT+QSSLCFG=\"sslversion\",0,3","AT+QSSLCFG=\"seclevel\",0,1",
  "AT+QSSLCFG=\"cacert\",0,\"UFS:ev-ca.pem\"","AT+QSSLCFG=\"sni\",0,1",
  "AT+QSSLCFG=\"ignorelocaltime\",0,0","AT+QSSLCFG=\"ignoreinvalidcertsign\",0,0",
  "AT+QSSLCFG=\"ignorecertitem\",0,0","AT+QMTCFG=\"version\",0,4",
  "AT+QMTCFG=\"pdpcid\",0,1","AT+QMTCFG=\"session\",0,1","AT+QMTCFG=\"keepalive\",0,30",
  "AT+QMTCFG=\"ssl\",0,1,0"};
 for(unsigned i=0;i<sizeof commands/sizeof commands[0];i++)if(!simple(commands[i]))return false;
 /* Length-bearing direct notifications avoid treating AT replies as a payload. */
 receive_commands=config.remote_enabled && simple("AT+QMTCFG=\"recv/mode\",0,0,1");
 if(config.remote_enabled && !receive_commands)AppLog_Printf("[NET] MQTT downlink framing unavailable\r\n");
 return true;
}
static bool connect_cloud(void) {
 (void)transaction("AT+QMTCLOSE=0",NULL,5000U);
 if(status_sequence>0xfffffffdU)return false;
 uint32_t online_seq=status_sequence;status_sequence+=2U;
 (void)snprintf(payload,sizeof payload,"{\"v\":\"1.0.0\",\"device_id\":\"%s\",\"boot_id\":\"%s\",\"seq\":%lu,\"state\":\"offline\",\"reason\":\"lwt\",\"ts\":null}",config.device,boot,(unsigned long)(online_seq+1U));
 (void)snprintf(topic,sizeof topic,"ecovision/%s/status",config.device);
 (void)snprintf(command_buffer,sizeof command_buffer,"AT+QMTCFG=\"willex\",0,1,1,1,\"%s\",%u",topic,(unsigned)strlen(payload));
 if(transaction(command_buffer,">",5000U)||!transmit(payload,strlen(payload))||transaction(NULL,NULL,5000U)){AppLog_Printf("[NET] will configuration failed\r\n");return false;}
 (void)snprintf(command_buffer,sizeof command_buffer,"AT+QMTOPEN=0,\"%s\",%u",config.host,config.port);
 int v[3]={-1,-1,-1};if(!wait_result(command_buffer,"+QMTOPEN:",2,90000U,v)||v[0]!=0||v[1]!=0){AppLog_Printf("[NET] MQTT OPEN failed result=%d,%d\r\n",v[0],v[1]);return false;}
 /* Credentials live only in RAM/private device configuration; never print this command. */
 (void)snprintf(command_buffer,sizeof command_buffer,"AT+QMTCONN=0,\"%s\",\"%s\",\"%s\"",config.device,config.user,config.password);
 bool connected=wait_result(command_buffer,"+QMTCONN:",3,45000U,v)&&v[0]==0&&v[1]==0&&v[2]==0;
 memset(command_buffer,0,sizeof command_buffer);if(!connected){AppLog_Printf("[NET] MQTT CONN failed result=%d,%d,%d\r\n",v[0],v[1],v[2]);return false;}
 (void)snprintf(payload,sizeof payload,"{\"v\":\"1.0.0\",\"device_id\":\"%s\",\"boot_id\":\"%s\",\"seq\":%lu,\"state\":\"online\",\"reason\":\"connected\",\"ts\":%lu}",config.device,boot,(unsigned long)online_seq,(unsigned long)utc_now());
 if(receive_commands) {
  uint16_t id=++message_id;if(id==0)id=++message_id;
  int sub[4]={-1,-1,-1,-1};
  (void)snprintf(command_buffer,sizeof command_buffer,"AT+QMTSUB=0,%u,\"%s\",1",id,command_topic);
  subscription_utc=utc_now();
  command_subscribed=wait_result(command_buffer,"+QMTSUB:",4,20000U,sub) &&
                     sub[0]==0 && sub[1]==id && sub[2]==0 && sub[3]==1;
  AppLog_Printf("[NET] MQTT command subscription=%s\r\n",command_subscribed?"ready":"unavailable");
 }
 return publish(topic,payload,true);
}
void AppNet_Run(void *argument) {
 (void)argument;osDelay(1600U);arm();
 bool configured=Net_ConfigDecode((const uint8_t *)0x08160000U,8192U,&config);
 AppLog_Printf("[NET] private_config=%s (values not logged)\r\n",configured?"valid":"invalid");
 (void)snprintf(command_topic,sizeof command_topic,"ecovision/%s/cmd",config.device);
 (void)snprintf(ack_topic,sizeof ack_topic,"ecovision/%s/ack",config.device);
 if(!random_id()){stage("RNG_ERROR");for(;;)osDelay(1000U);}
 uint32_t retry=0;uint32_t previous_temp_at=0,previous_vision_at=0;bool have_temp=false,have_vision=false;
 static const uint32_t delays[]={2000,5000,10000,30000,60000};
 for(;;) {
  clock_valid=false;command_subscribed=false;inbox_start=0;inbox_size=0;
  stage("AT_WAIT");
  if(!find_at()){AppLog_Printf("[NET] AT no response UART6 PG14/PG9\r\n");goto backoff;}
  if(!simple("ATE0")||!simple("AT+CMEE=2")||!simple("AT+QURCCFG=\"urcport\",\"uart1\""))goto backoff;
  stage("SIM_CHECK");
  if(transaction("AT+CPIN?","+CPIN: READY",5000U))goto backoff;
  stage("REGISTER");
  if(transaction("AT+CEREG?","+CEREG:",5000U))goto backoff;
  int n,registered;if(sscanf(matched_line,"+CEREG: %d,%d",&n,&registered)!=2 || (registered!=1&&registered!=5))goto backoff;
  if(!configured){stage("CONFIG_REQUIRED");osDelay(60000U);continue;}
  stage("PDP");
  if(transaction("AT+QICSGP=1","+QICSGP:",5000U))goto backoff;
  int pdp_type=0;if(sscanf(matched_line,"+QICSGP: %d",&pdp_type)!=1)goto backoff;
  if(pdp_type!=1) {
   if(Net_IPv4Profile(pdp_profile,sizeof pdp_profile,matched_line)<0)goto backoff;
   (void)transaction("AT+QIDEACT=1",NULL,45000U);
   bool configured_ipv4=simple(pdp_profile);memset(pdp_profile,0,sizeof pdp_profile);
   if(!configured_ipv4)goto backoff;
   AppLog_Printf("[NET] IPv4 PDP configured; existing APN/auth preserved\r\n");
  }
  (void)transaction("AT+QIACT=1",NULL,150000U);
  if(transaction("AT+QIACT?","+QIACT: 1,1,",10000U))goto backoff;
  stage("TIME_SYNC");if(!sync_clock())goto backoff;
  stage("TLS_CONFIG");if(!configure_tls())goto backoff;
  stage("MQTT_CONNECT");if(!connect_cloud())goto backoff;
  stage("ONLINE");retry=0;
  for(;;) {
   EnvironmentSnapshot env;AppEnvironment_GetSnapshot(&env);
   EvVision vis;AppVision_GetSnapshot(&vis);
   uint32_t now=HAL_GetTick();
   if(env.th_valid){have_temp=true;previous_temp_at=env.sampled_at;}
   if(vis.active&&vis.valid){have_vision=true;previous_vision_at=vis.fresh_at;}
   NetSample sample={0};sample.uptime=now;sample.seq=sequence++;sample.utc=utc_now();sample.time_valid=clock_valid;
   sample.vision_valid=vis.active&&vis.valid;sample.count=vis.count;sample.have_vision=have_vision;sample.vision_age=now-previous_vision_at;
   sample.temp_valid=env.th_valid;sample.temp_x100=env.temp_x100;sample.rh_x100=env.humi_x100;sample.have_temp=have_temp;sample.temp_age=now-previous_temp_at;
   ControlOutput control;AppControl_GetRemoteSnapshot(&control,sample.last_command);
   sample.remote=control.mode==CONTROL_REMOTE;
   sample.hold_remaining_s=control.hold_remaining_s;
   /* One applied snapshot avoids mixing pre/post LOCAL_OFF driver values. */
   sample.fan_level=control.fan_level;sample.led=control.led;
   sample.occupancy=(uint8_t)control.occupancy;
   sample.local_off=control.mode==CONTROL_LOCAL_OFF;sample.control_epoch=control.control_epoch;
   sample.light_valid=env.light_valid;sample.light_raw16=env.light_raw16;
   if(Net_Telemetry(payload,sizeof payload,config.device,boot,&sample)<0)break;
   (void)snprintf(topic,sizeof topic,"ecovision/%s/telemetry",config.device);
   if(!publish(topic,payload,false))break;
   AppLog_Printf("[NET] PUBACK seq=%lu bytes=%u temp_valid=%u vision_valid=%u\r\n",(unsigned long)sample.seq,(unsigned)strlen(payload),sample.temp_valid?1U:0U,sample.vision_valid?1U:0U);
   /* Explicit private opt-in to the existing dashboard's legacy input.
    * Official 1.0.0 telemetry above is unchanged. Invalid sensor data is
    * never fabricated to satisfy the old consumer's non-null fields. */
   if(config.legacy_topic[0] && Net_LegacyTelemetry(payload,sizeof payload,config.device,config.room,&sample)>0) {
    if(publish(config.legacy_topic,payload,true))AppLog_Printf("[NET] LEGACY PUBACK seq=%lu source=C_HAL_REAL_4G\r\n",(unsigned long)(sample.utc-946684800U));
    else AppLog_Printf("[NET] legacy consumer publish failed; official path remains primary\r\n");
   }
   uint32_t until=now+5000U;
   while((int32_t)(HAL_GetTick()-until)<0) {
    bool refresh=false;
    if(command_subscribed && (!drain_online_uart() || !service_commands(&refresh)))goto backoff;
    ControlOutput latest;AppControl_GetSnapshot(&latest);
    if(refresh || latest.mode!=control.mode || latest.control_epoch!=control.control_epoch ||
       latest.fan_level!=control.fan_level || latest.led!=control.led)break;
    osDelay(20U);
   }
  }
backoff:
  AppLog_Printf("[NET] stage=%s failed; retry=%lu (local tasks continue)\r\n",state,(unsigned long)retry);
  stage("OFFLINE");uint32_t delay=delays[retry<5U?retry:4U];if(retry<5U)retry++;
  uint32_t jitter=(Net_Crc32((const uint8_t *)boot,32)^HAL_GetTick())%201U;
  osDelay(delay+(delay*jitter)/1000U);
 }
}

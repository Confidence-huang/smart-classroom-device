/* EC200U network waits run only in this task; no credential or AT echo logging. */
#include "app_net.h"
#include "net_codec.h"
#include "net_publish.h"
#include "net_recovery.h"
#include "app_log.h"
#include "app_environment.h"
#include "app_vision.h"
#include "app_fan.h"
#include "app_led.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern UART_HandleTypeDef huart6;
enum {RX_CAP=2048};
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
 if(command){discard();if(!transmit(command,strlen(command))||!transmit("\r",1))return -4;}
 matched_line[0]=0;uint32_t start=HAL_GetTick();
 while((uint32_t)(HAL_GetTick()-start)<timeout) {
  if(rx_fault){discard();return -3;}
  while(tail!=head) {
   uint8_t byte=ring[tail];tail=(uint16_t)((tail+1U)%RX_CAP);
   NetAtEvent event=NetAt_Feed(&parser,byte);
   if(event==NET_AT_DROPPED)return -3;
   if(event==NET_AT_PROMPT && prompt)return 0;
   if(event!=NET_AT_LINE)continue;
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
 return publish(topic,payload,true);
}
void AppNet_Run(void *argument) {
 (void)argument;osDelay(1600U);arm();
 bool configured=Net_ConfigDecode((const uint8_t *)0x08160000U,8192U,&config);
 AppLog_Printf("[NET] private_config=%s (values not logged)\r\n",configured?"valid":"invalid");
 if(!random_id()){stage("RNG_ERROR");for(;;)osDelay(1000U);}
 uint32_t retry=0;uint32_t previous_temp_at=0,previous_vision_at=0;bool have_temp=false,have_vision=false;
 static const uint32_t delays[]={2000,5000,10000,30000,60000};
 for(;;) {
  clock_valid=false;stage("AT_WAIT");
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
   sample.fan=AppFan_GetLevel()!=0;sample.led=AppLed_GetLevel()!=0;
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
   uint32_t until=now+5000U;while((int32_t)(HAL_GetTick()-until)<0)osDelay(20U);
  }
backoff:
  AppLog_Printf("[NET] stage=%s failed; retry=%lu (local tasks continue)\r\n",state,(unsigned long)retry);
  stage("OFFLINE");uint32_t delay=delays[retry<5U?retry:4U];if(retry<5U)retry++;
  uint32_t jitter=(Net_Crc32((const uint8_t *)boot,32)^HAL_GetTick())%201U;
  osDelay(delay+(delay*jitter)/1000U);
 }
}

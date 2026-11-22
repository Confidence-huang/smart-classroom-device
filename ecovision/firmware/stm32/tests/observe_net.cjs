/* Read-only MQTT observer. No credentials are logged or published. */
const fs=require('node:fs');
const path=require('node:path');
const mqtt=require(path.resolve('cloud/node_modules/mqtt'));
const privateDir=path.resolve('firmware/stm32/.private');
const config=JSON.parse(fs.readFileSync(path.join(privateDir,'recovered-settings.json'),'utf8'));
const ca=fs.readFileSync(path.join(privateDir,'recovered-ca-0.pem'));
const output=path.resolve('firmware/stm32/net-evidence/2026-11-21/subscriber.json');
if(fs.existsSync(output))fs.copyFileSync(output,output.replace('.json','-archive-'+Date.now()+'.json'));
const report={started_at:new Date().toISOString(),observer_only:true,connected:false,messages:[],errors:[]};
const safeKeys=new Set(['v','device_id','boot_id','seq','uptime_ms','ts','time_valid','control_epoch','count','vision_valid','vision_age_ms','temp_c','temp_valid','temp_age_ms','humidity_pct','humidity_valid','occupancy','mode','fan_out','led_out','hold_remaining_s','net_state','last_cmd_id','faults','state','reason','schema_version','type','simulated','people_count','temperature_c','captured_at','light_lux','light_raw_u16','source']);
const client=mqtt.connect({host:config.MQTT_HOST,port:config.MQTT_PORT,protocol:'mqtts',ca,rejectUnauthorized:true,
 username:config.MQTT_USER,password:config.MQTT_PASSWORD,protocolVersion:4,clean:true,
 clientId:'ecovision-observer-'+process.pid,reconnectPeriod:0,connectTimeout:12000});
function save(){fs.mkdirSync(path.dirname(output),{recursive:true});fs.writeFileSync(output,JSON.stringify(report,null,2));}
client.on('connect',()=>{report.connected=true;console.log('TLS/MQTT observer connected; no publications sent');
 client.subscribe(['ecovision/'+config.DEVICE_ID+'/telemetry','ecovision/'+config.DEVICE_ID+'/status',config.MQTT_TOPIC],{qos:1},(err,grants)=>{
  if(err)report.errors.push('SUBSCRIBE_ERROR');else report.grants=grants.map(x=>({topic:x.topic,qos:x.qos}));save();
 });});
client.on('message',(topic,payload,packet)=>{try{
 const value=JSON.parse(payload.toString());const filtered=Object.fromEntries(Object.entries(value).filter(([key])=>safeKeys.has(key)));
 report.messages.push({received_at:new Date().toISOString(),topic,retain:packet.retain,payload:filtered});
 if(report.messages.length>300)report.messages.shift();save();
 console.log(JSON.stringify({received:true,formal:value.v==='1.0.0',seq:value.seq,retain:packet.retain}));
}catch{report.errors.push('NON_JSON_PAYLOAD');}});
client.on('error',error=>{const text=String(error.message).split(String(config.MQTT_PASSWORD)).join('[redacted]');report.errors.push(text);console.log('Observer error: '+text);save();});
const duration=Math.max(10,Math.min(300,Number(process.argv[2]||50)))*1000;
setTimeout(()=>{report.ended_at=new Date().toISOString();save();client.end(true,()=>process.exit(report.connected?0:1));},duration);

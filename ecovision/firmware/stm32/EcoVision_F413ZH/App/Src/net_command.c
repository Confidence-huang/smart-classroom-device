/* Decode the flat v1.0.0 MQTT command before any access to the output task. */
#include "net_command.h"
#include <string.h>

static void whitespace(const char **p) {
    while(**p==' ' || **p=='\t' || **p=='\r' || **p=='\n')(*p)++;
}

static bool take(const char **p,char delimiter) {
    whitespace(p);
    if(**p!=delimiter)return false;
    (*p)++;
    return true;
}

static bool digit(char c) {return c>='0' && c<='9';}

static bool unsigned_value(const char **p,uint64_t limit,uint64_t *value) {
    whitespace(p);
    if(!digit(**p) || (**p=='0' && digit((*p)[1])))return false;
    uint64_t n=0;
    do {
        unsigned d=(unsigned)(**p-'0');
        if(n>limit/10U || (n==limit/10U && d>limit%10U))return false;
        n=n*10U+d;
        (*p)++;
    } while(digit(**p));
    *value=n;
    return true;
}

/* Decode only representable ASCII. Field-specific validators check the decoded identifiers. */
static int hex_digit(char c) {
    if(digit(c))return c-'0';
    if(c>='a' && c<='f')return c-'a'+10;
    if(c>='A' && c<='F')return c-'A'+10;
    return -1;
}

static bool string_value(const char **p,char *out,size_t size) {
    if(!take(p,'"'))return false;
    size_t used=0;
    while(**p && **p!='"') {
        unsigned char c=(unsigned char)*(*p)++;
        if(c=='\\') {
            if(**p!='u')return false; /* Accepted fields never contain quoting or control characters. */
            (*p)++;
            unsigned code=0;
            for(unsigned i=0;i<4;i++) {
                int h=hex_digit(**p);
                if(h<0)return false;
                code=code*16U+(unsigned)h;
                (*p)++;
            }
            if(code>126U)return false;
            c=(unsigned char)code;
        }
        if(c<32U || c>126U || used+1>=size)return false;
        out[used++]=(char)c;
    }
    if(**p!='"')return false;
    (*p)++;
    out[used]=0;
    return true;
}

static bool lower(char c) {return c>='a' && c<='z';}
static bool device_id(const char *id) {
    if(!lower(*id) && !digit(*id))return false;
    for(;*id;id++)if(!lower(*id) && !digit(*id) && *id!='_' && *id!='-')return false;
    return true;
}

bool Net_CommandId(const char *id) {
    size_t size=strlen(id);
    if(size==0 || size>=NET_COMMAND_ID_SIZE)return false;
    for(;*id;id++)
        if(!lower(*id) && !digit(*id) && !(*id>='A' && *id<='Z') && *id!='_' && *id!='-')
            return false;
    return true;
}

static bool boot_id(const char *id) {
    if(strlen(id)<8U)return false;
    for(;*id;id++)if(!digit(*id) && !(*id>='a' && *id<='f'))return false;
    return true;
}

char *Net_CommandUrc(char *line,const char *topic) {
    const char prefix[]="+QMTRECV:";
    if(strncmp(line,prefix,sizeof prefix-1))return NULL;
    const char *at=line+sizeof prefix-1;
    uint64_t client,id,length;
    char name[64];
    if(!unsigned_value(&at,0,&client) || !take(&at,',') ||
       !unsigned_value(&at,65535,&id) || id==0 || !take(&at,',') ||
       !string_value(&at,name,sizeof name) || strcmp(name,topic)!=0 ||
       !take(&at,',') || !unsigned_value(&at,NET_COMMAND_LIMIT,&length) ||
       !take(&at,','))return NULL;

    /* Firmware families report either raw payload bytes or an outer quoted field. */
    size_t visible=strlen(at);
    if(visible==length)return (char *)at;
    if(visible!=length+2U || *at!='"' || at[visible-1]!='"')return NULL;
    ((char *)at)[visible-1]=0;
    return (char *)at+1;
}

static bool output_value(const char **p,int8_t *output) {
    whitespace(p);
    if(strncmp(*p,"null",4)==0) {
        *p+=4;
        *output=-1;
        return true;
    }
    uint64_t value;
    if(!unsigned_value(p,1,&value))return false;
    *output=(int8_t)value;
    return true;
}

enum CommandField {
    VERSION, DEVICE, BOOT, EPOCH, ID, ISSUED, TTL, MODE, FAN, LED, HOLD, FIELD_COUNT
};
static const char *const fields[FIELD_COUNT]={
    "v","device_id","boot_id","control_epoch","cmd_id","issued_at","ttl_s","mode",
    "fan_out","led_out","hold_s"
};

static bool field_value(enum CommandField key,const char **p,NetCommand *cmd,
                        bool *supported,bool *remote) {
    uint64_t value;
    char word[24];
    switch(key) {
    case VERSION:
        if(!string_value(p,word,sizeof word))return false;
        *supported=strcmp(word,"1.0.0")==0;
        return true;
    case DEVICE: return string_value(p,cmd->device,sizeof cmd->device) && device_id(cmd->device);
    case BOOT: return string_value(p,cmd->boot,sizeof cmd->boot) && boot_id(cmd->boot);
    case ID: return string_value(p,cmd->id,sizeof cmd->id) && Net_CommandId(cmd->id);
    case ISSUED: return unsigned_value(p,UINT64_C(9007199254740991),&cmd->issued_at);
    case EPOCH:
        if(!unsigned_value(p,UINT32_MAX,&value))return false;
        cmd->epoch=(uint32_t)value;
        return true;
    case TTL:
        if(!unsigned_value(p,30,&value) || value==0)return false;
        cmd->ttl_s=(uint8_t)value;
        return true;
    case MODE:
        if(!string_value(p,word,sizeof word))return false;
        *remote=strcmp(word,"REMOTE")==0;
        return *remote || strcmp(word,"AUTO")==0;
    case FAN: return output_value(p,&cmd->fan);
    case LED: return output_value(p,&cmd->led);
    case HOLD:
        if(!unsigned_value(p,300,&value))return false;
        cmd->hold_s=(uint16_t)value;
        return true;
    default: return false;
    }
}

NetCommandParse Net_ParseCommand(const char *json,NetCommand *cmd) {
    memset(cmd,0,sizeof *cmd);
    if(strlen(json)>NET_COMMAND_LIMIT || !take(&json,'{'))return NET_COMMAND_INVALID;
    unsigned seen=0;
    bool supported=false,remote=false;
    do {
        char name[20];
        if(!string_value(&json,name,sizeof name) || !take(&json,':'))return NET_COMMAND_INVALID;
        unsigned key=0;
        while(key<FIELD_COUNT && strcmp(name,fields[key])!=0)key++;
        if(key==FIELD_COUNT || (seen&(1U<<key)))return NET_COMMAND_INVALID;
        if(!field_value((enum CommandField)key,&json,cmd,&supported,&remote))return NET_COMMAND_INVALID;
        seen|=1U<<key;
    } while(take(&json,','));

    if(seen!=(1U<<FIELD_COUNT)-1U || !take(&json,'}'))return NET_COMMAND_INVALID;
    whitespace(&json);
    if(*json)return NET_COMMAND_INVALID;
    if(!supported)return NET_COMMAND_UNSUPPORTED;
    if(remote && (cmd->fan<0 || cmd->led<0 || cmd->hold_s<30))return NET_COMMAND_INVALID;
    if(!remote && (cmd->fan!=-1 || cmd->led!=-1 || cmd->hold_s!=0))return NET_COMMAND_INVALID;
    return NET_COMMAND_OK;
}

bool Net_SameCommand(const NetCommand *a,const NetCommand *b) {
    return strcmp(a->device,b->device)==0 && strcmp(a->boot,b->boot)==0 &&
           strcmp(a->id,b->id)==0 && a->issued_at==b->issued_at &&
           a->epoch==b->epoch && a->hold_s==b->hold_s && a->ttl_s==b->ttl_s &&
           a->fan==b->fan && a->led==b->led;
}

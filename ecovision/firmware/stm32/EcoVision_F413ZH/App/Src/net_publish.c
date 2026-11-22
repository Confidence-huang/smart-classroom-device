#include "net_publish.h"
#include <string.h>

bool Net_PublishFixed(const NetPublishIo *io,const char *topic,const char *payload,
                      uint16_t id,bool retain,bool *recovery) {
    if(recovery)*recovery=false;
    if(!io || !io->begin || !io->write_payload || !io->wait_ack || !topic ||
       !payload || !*topic || !id)return false;
    size_t size=strlen(payload);
    if(!size || size>1500U)return false;
    int prompt=io->begin(io->context,topic,id,retain,size);
    if(prompt!=0 && prompt!=-2)return false;
    if(recovery)*recovery=prompt==-2;
    /* A lost prompt can leave the module expecting exactly this many bytes.
     * Finishing the original data once avoids feeding AT probes as payload.
     * Never duplicate the data, and never call success without its PUBACK. */
    if(!io->write_payload(io->context,payload,size))return false;
    return io->wait_ack(io->context,id);
}

#include "net_recovery.h"
#include <stddef.h>
bool Net_FindAt(NetAtLink *link,const NetRecoveryIo *io) {
 static const uint32_t rates[]={115200U,921600U,460800U,230400U,57600U,9600U};
 if(!link || !io || !io->probe || !io->escape_legacy_ppp || !io->configure)return false;
 /* A temporary response gap does not change a previously confirmed rate or
  * establish PPP mode. Do not turn that gap into a scan/escape outage. */
 if(link->known)return link->baud!=0U && io->probe(io->context,link->baud)==0;
 for(size_t i=0;i<sizeof rates/sizeof rates[0];i++) {
  int result=io->probe(io->context,rates[i]);
  if(result==0 || (i==0U && result==-2 && io->escape_legacy_ppp(io->context))) {
   link->known=true;link->baud=rates[i];return true;
  }
 }
 /* Never spend a retry backoff accidentally parked at the last scan rate. */
 link->baud=rates[0];io->configure(io->context,link->baud);
 return false;
}

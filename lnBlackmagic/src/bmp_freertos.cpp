/*
https://ftp.gnu.org/old-gnu/Manuals/gdb/html_chapter/gdb_15.html
http://web.mit.edu/rhel-doc/3/rhel-gdb-en-3/general-query-packets.html

qTStatus
qAttached
qP0000001f0000000000007fff
qL120000000000

OFFSET_LIST_ITEM_NEXT = 4,
OFFSET_LIST_ITEM_OWNER = 12,
OFFSET_LIST_NUMBER_OF_ITEM = 0,
OFFSET_LIST_INDEX = 4,
NB_OF_PRIORITIES = 16,
MPU_ENABLED = 0,
MAX_TASK_NAME_LEN = 16,
OFFSET_TASK_NAME = 52,
OFFSET_TASK_NUM = 68}


 */

 /**
   The following is assumed
   - Everything starts with a qfthread info or qC
   - The number of threads is not too high
   - We cache the TCB and thread # between 2 calls


 */


 #include "lnArduino.h"
 #include "bmp_string.h"
 extern "C"
 {
 #include "hex_utils.h"
 #include "target.h"
 #include "target_internal.h"
 #include "gdb_packet.h"
 #include "lnFreeRTOSDebug.h"
}
#include "bmp_util.h"




AllSymbols allSymbols;
lnThreadInfoCache *threadCache=NULL;

/**

*/
void initFreeRTOS()
{
  if(!threadCache)
  {
    threadCache=new lnThreadInfoCache;
    allSymbols.clear();
  }
}


class listThread : public ThreadParserBase
{
public:
    listThread()
    {
    }
    bool execList(FreeRTOSSymbols state,uint32_t tcbAdr)
    {
        uint32_t id=readMem32(tcbAdr,O(OFFSET_TASK_NUM));
        threadCache->add(id,tcbAdr); //,state);
        return true;
    }
};


#include "bmp_cortex_registers.h"

#include "bmp_gdb_cmd.h"

#define PRE_CHECK_DEBUG_TARGET(sym)  { if(!allSymbols.readDebugBlock ()) \
                                      {    gdb_putpacketz("");  \
                                          return;  } }

#define STUBFUNCTION_END(x)  extern "C" void x(const char *packet, int len) \
{ \
  Logger("::: %s:%s\n",#x,packet); \
  gdb_putpacket("l", 1); \
}
#define STUBFUNCTION_EMPTY(x)  extern "C" void x(const char *packet, int len) \
{ \
  Logger("::: %s:%s\n",#x,packet); \
  gdb_putpacketz(""); \
}



void updateCache()
{
  threadCache->clear();
  listThread list; // list all the threads
  list.run();
}

/**

*/
extern "C" void exect_qC(const char *packet, int len)
{
  Logger("::: exect_qC:%s\n",packet);
  initFreeRTOS(); // host mode
  updateCache();
  PRE_CHECK_DEBUG_TARGET();
  Gdb::Qc();
}

/*
    Grab FreeRTOS symbols
*/
extern "C" void execqSymbol(const char *packet, int len)
{
  Logger(":::<execqSymbol>:<%s>\n",packet);
  if(len==1 && packet[0]==':') // :: : ready to serve https://sourceware.org/gdb/onlinedocs/gdb/General-Query-Packets.html
  {
    Gdb::startGatheringSymbol();
    return;
  }
  if(len>1)
  {
      Gdb::decodeSymbol(len,packet);
      return;
  }
  gdb_putpacket("OK", 2);
}
/**

*/
extern "C" bool lnProcessCommand(int size, const char *data)
{
    Logger("Received packet %s\n",data);
    return false;
}
//
extern "C" void execqOffsets(const char *packet, int len)
{
  Logger("::: execqOffsets:%s\n",packet);
  // it's xip...
  gdb_putpacket("Text=0;Data=0;Bss=0", 19); // 7 7 5=>19
}


STUBFUNCTION_END(execqsThreadInfo)

STUBFUNCTION_EMPTY(execqThreadInfo)


/**

*/
extern "C" void execqfThreadInfo(const char *packet, int len)
{
  Logger("::: qfThreadinfo:%s\n",packet);
  PRE_CHECK_DEBUG_TARGET();

  updateCache();
  stringWrapper wrapper;
  threadCache->collectIdAsWrapperString(wrapper);
  char *out=wrapper.string();
  if(strlen(out))
  {
      gdb_putpacket2("m",1,out,strlen(out));
  }else
  {
      gdb_putpacket("m0", 2);
  }
  free(out);
}

/**

*/
extern "C" void exect_qThreadExtraInfo(const char *packet, int len)
{
  Logger("::: exect_qThreadExtraInfo:%s\n",packet);
  PRE_CHECK_DEBUG_TARGET();
  uint32_t tid;
  if(1!=sscanf(packet,",%x",&tid))
  {
     Logger("Invalid thread info\n");
	   gdb_putpacketz("E01");
     return;
  }
  Gdb::threadInfo(tid);
}

extern "C" void exec_H_cmd(const char *packet, int len)
{
    Logger("::: exec_H_cmd:<%s>\n",packet);
    PRE_CHECK_DEBUG_TARGET();
    int tid;
    if(1!=sscanf(packet,"%d",&tid))
    {
       Logger("Invalid thread id\n");
       gdb_putpacketz("E01");
       return;
    }
    if(0==tid)
    {
      Logger("Invalid thread id\n");
      gdb_putpacketz("E01");
      return;
    }
    if(!Gdb::switchThread(tid))
    {
      gdb_putpacketz("E01");
      return;
    }
}

extern "C" void exec_H_cmd2(const char *packet, int len)
{
    Logger("::: exec_H_cmd2:<%s>\n",packet);
    PRE_CHECK_DEBUG_TARGET();
    gdb_putpacketz("OK");

}
extern "C" void exec_T_cmd(const char *packet, int len)
{
    Logger("::: exec_T_cmd:<%s>\n",packet);
    PRE_CHECK_DEBUG_TARGET();
    int tid;
    if(1!=sscanf(packet,"%d",&tid))
    {
       Logger("Invalid thread id\n");
       gdb_putpacketz("E01");
       return;
    }
    if(0==tid)
    {
      Logger("Invalid thread id\n");
      gdb_putpacketz("E01");
      return;
    }
    fdebug2("Thread : %d\n",tid);
    if(!Gdb::isThreadAlive(tid))
    {
      gdb_putpacketz("E01");
      return;
    }else
    {
      gdb_putpacketz("OK");
      return;
    }
}

//

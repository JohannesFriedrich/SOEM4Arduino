#include <stdarg.h>
#include <stdio.h>
#include <Arduino.h>
#include "osal/osal_defs.h" // EC_DEBUG

// just for debug (not thread safe)
void debug_print(const char* format, ...)
{
    static char buff[1024]; // TODO non-reentrant
    
    va_list args;
    va_start( args, format );
    vsnprintf( buff, sizeof(buff), format, args );
    va_end(args);

    Serial.print(buff);
}

#if defined(GRSAKURA) || defined(GRROSE)
/**************************************************
  for GR-SAKURA / GR-ROSE (T4 library)
**************************************************/

#include "rx63n/iodefine.h"
//#include <Ethernet.h>
#include "utility/T4_src/r_t4_itcpip.h"
#include "utility/driver/r_ether.h"
#include "utility/driver/r_ether_local.h"
#include "soem/ethercattype.h"

#define ETH_HEADERSIZE      sizeof(ec_etherheadert)

// TODO This is for tcpudp_open()
#define TCPUDP_WORK                     1780/sizeof(UW)
static UW tcpudp_work[TCPUDP_WORK];

// polling Ethernet instead of interrupt. (TODO This is a bad trick.)
static void ethernet_poll(void)
{
    uint32_t status_ecsr = ETHERC.ECSR.LONG;
    uint32_t status_eesr = EDMAC.EESR.LONG;
                                      
    /* When the ETHERC status interrupt is generated */
    if (status_eesr & EMAC_ECI_INT)
    {
        /* When the Magic Packet detection interrupt is generated */
        if (status_ecsr & EMAC_MPD_INT)
        {
            //g_ether_MpdFlag = ETHER_FLAG_ON;
        }
        /*
         * Because each bit of the ECSR register is cleared when one is written, 
         * the value read from the register is written and the bit is cleared. 
         */
        /* Clear all ETHERC status BFR, PSRTO, LCHNG, MPD, ICD */
        ETHERC.ECSR.LONG = status_ecsr;
    }
    EDMAC.EESR.LONG  = status_eesr; /* Clear EDMAC status bits */
}

#ifdef __cplusplus
extern "C"
{
#endif

int  hal_ethernet_open(void)
{
    int result = tcpudp_open(tcpudp_work);
    
    // disable Ethernet interrupt. (TODO This is a bad trick.) 
    IEN(ETHER, EINT) = 0;
    
    return result;	
}

void hal_ethernet_close(void)
{
    tcpudp_close();
}

int  hal_ethernet_send(unsigned char *data, int len)
{
    int result = lan_write(
        0,  // *stack->sock  (meaningless parameter)
        (B*)(&data[0]),               ETH_HEADERSIZE,
        (B*)(&data[ETH_HEADERSIZE]) , len - ETH_HEADERSIZE);
    
    return result;
}

int  hal_ethernet_recv(unsigned char **data)
{
    // polling Ethernet instead of interrupt. (TODO This is a bad trick.)
    ethernet_poll();
    
    int result = lan_read(
        0, // *stack->sock  (meaningless parameter)
        (B**)data);
    
    return result;
}

#ifdef __cplusplus
}
#endif

#else
/**************************************************
  for Ethernet Shield (W5500)
**************************************************/

#include <SPI.h>
#include <Ethernet2.h>
#include <utility/w5500.h>

// W5500 RAW socket
static SOCKET sock;
// W5500 RAW socket buffer
static unsigned char socketBuffer[1536];

#ifdef __cplusplus
extern "C"
{
#endif

int hal_ethernet_open(void)
{
    w5500.init();
    w5500.writeSnMR(sock, SnMR::MACRAW); 
    w5500.execCmdSn(sock, Sock_OPEN);
    return 0;
}

void hal_ethernet_close(void)
{
    // w5500 doesn't have close() or terminate() method
    w5500.swReset();
}

int hal_ethernet_send(unsigned char *data, int len)
{
    w5500.send_data_processing(sock, (byte *)data, len);
    w5500.execCmdSn(sock, Sock_SEND_MAC);
    return len;
}

int hal_ethernet_recv(unsigned char **data)
{
    unsigned short packetSize;
    int res = w5500.getRXReceivedSize(sock);
    if(res > 0){
        // first 2byte shows packet size
        w5500.recv_data_processing(sock, (byte*)socketBuffer, 2);
        w5500.execCmdSn(sock, Sock_RECV);
        // packet size
        packetSize  = socketBuffer[0];
        packetSize  <<= 8;
        packetSize  &= 0xFF00;
        packetSize  |= (unsigned short)( (unsigned char)socketBuffer[1] & 0x00FF);
        packetSize  -= 2;
        // get received data
        memset(socketBuffer, 0x00, 1536);
        w5500.recv_data_processing(sock, (byte *)socketBuffer, packetSize);
        w5500.execCmdSn(sock, Sock_RECV);
        *data = socketBuffer;
    }
    return packetSize;
}

#ifdef __cplusplus
}
#endif

#endif // for Ethernet Shield (W5500)

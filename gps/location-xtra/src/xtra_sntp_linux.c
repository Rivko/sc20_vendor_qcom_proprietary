/******************************************************************************
  @file:  xtra_sntp_linux.c
  @brief: loc_api_test XTRA time services

  DESCRIPTION

  INITIALIZATION AND SEQUENCING REQUIREMENTS

  -----------------------------------------------------------------------------
  Copyright (c) 2013 Qualcomm Technology Incoporated.
  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
  -----------------------------------------------------------------------------
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>         /* struct sockaddr_in */
#include <netdb.h>              /* struct hostent     */
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>

#include "xtra.h"
#include "log_util.h"
#include "platform_lib_abstractions/platform_lib_includes.h"

/*======

FUNCTION loc_xtra_get_sys_time

DESCRIPTION
   Fills system time in loc_api assistance time struct

DEPENDENCIES
   n/a

RETURN VALUE
   none

SIDE EFFECTS
   n/a

======*/
void loc_xtra_get_sys_time( xtra_assist_data_time_s_type *assist_time )
{
   struct timeval  time_val;

   gettimeofday(&time_val, (void *)0);

   assist_time->time_utc = (unsigned int) time_val.tv_sec;
   /* secs since 00:00 UTC 1/1/1970 */
   // (unsigned int)(70 * 365 + 17) * (24 * 60 * 60));
   assist_time->time_utc *= 1000;

   assist_time->uncertainty = 20 /* sec */ * 1000;
}

/*======

FUNCTION gps_xtra_get_sys_time

DESCRIPTION
   Fills system time in gps_xtra_time_s_type struct

DEPENDENCIES
   n/a

RETURN VALUE
   none

SIDE EFFECTS
   n/a

======*/
static void gps_xtra_get_sys_time( gps_xtra_time_s_type *time_ptr, struct timeval *system_time_ptr )
{

   /* Converting to NTP time by adding seconds between 1900 and 1970 to the
      seconds part of the system time. NTP time is reported as
      number of seconds since Jan 1 1900 */
   time_ptr->sec = htonl((unsigned int)system_time_ptr->tv_sec
                  + (unsigned int)(70 * 365 + 17) * (24 * 60 * 60));

   /* fractional second is calculated by first converting micro seconds
      to seconds and multiplying it by 2^32 (maximum value of the fraction
      until it rolls over to 1. tv_usec is guaranteed to be between 0 and
      999,999 */
   time_ptr->fsec = htonl((unsigned int)(4294967296.
                  * (double)system_time_ptr->tv_usec/1000000. +.5));
}

/*======

FUNCTION loc_xtra_create_sntp_request_pkt

DESCRIPTION

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS

======*/
static int loc_xtra_create_sntp_request_pkt(gps_xtra_sntp_pkt_s_type *pkt_ptr, struct timeval *system_time_ptr)
{
   if(pkt_ptr == NULL)
   {
      return -1;
   }
   pkt_ptr->li_vn_mode = (SNTP_VN_NUM <<3) + SNTP_CLIENT_MODE;
   gps_xtra_get_sys_time(&pkt_ptr->tx_time,system_time_ptr);
   return 0;
}

/*======

FUNCTION loc_xtra_wait_resp

DESCRIPTION

DEPENDENCIES
   n/a

RETURN VALUE
   0   if successful
  -1   error

SIDE EFFECTS
   n/a

======*/
static int loc_xtra_wait_resp
(
      int                       sock_fd,
      gps_xtra_sntp_pkt_s_type *resp_pkt
)
{
   struct sockaddr_in dst;
   socklen_t dst_len;
   int len, ret;
   char recv_buf[SNTP_RECV_BUF_SIZE];
   fd_set rfds;
   struct timeval tv;

   dst_len = sizeof(dst);
   bzero( &dst, dst_len );
   FD_ZERO(&rfds);
   FD_SET( sock_fd, &rfds);
   tv.tv_sec  = SNTP_TIMEOUT_SEC;
   tv.tv_usec = SNTP_TIMEOUT_USEC;
   ret = select( sock_fd + 1, &rfds, NULL, NULL, &tv );

   if( ret )
   {
      len = recvfrom(sock_fd,
            recv_buf,
            SNTP_RECV_BUF_SIZE,
            0,
            (struct sockaddr *)&dst,
            &dst_len);

      if(ntohs(dst.sin_port) != SNTP_PORT_NUM)
      {
         return -1;
      }
   }
   else
   {

     // XTRA_TRACE_ERROR("SNTP receive time out\n" );
      return -1;
   }

   if(len < (signed) sizeof(*resp_pkt))
   {
     // XTRA_TRACE_ERROR("Packet size error.\n");
   }

   memcpy( resp_pkt, recv_buf, sizeof(*resp_pkt));

   if( SNTP_VN(resp_pkt->li_vn_mode) != SNTP_VN_NUM
         ||SNTP_MODE(resp_pkt->li_vn_mode) != SNTP_SERVER_MODE)
   {
    //  XTRA_TRACE_ERROR("Illegal packet.\n");
   }

   return 0;
}

/*======

FUNCTION timeval_diff

DESCRIPTION
   Calculates the difference between time intervals

DEPENDENCIES
   n/a

RETURN VALUE
   result - time difference in seconds and microseconds
   return: if y is newer

SIDE EFFECTS
   n/a

======*/
static int timeval_diff
(
      struct timeval *result,
      struct timeval *x,
      struct timeval *y
)
{
   int y_is_newer;
   int diff_sec;
   int diff_usec;

   diff_sec  = y->tv_sec  - x->tv_sec;
   diff_usec = y->tv_usec - x->tv_usec;

   /* in case usec is greater than 1 second */
   diff_sec += diff_usec / 1000000;
   diff_usec = diff_usec % 1000000;

   /* offset the negative value for usec */
   if (diff_usec > 0) {
      diff_sec  += 1;
      diff_usec -= 1000000;
   }

   /* x is newer or y is newer? */
   if ( (diff_sec >  0) || (diff_sec == 0 && diff_usec > 0)   )
   {
      /* y is newer */
      y_is_newer = 1;
   }
   else if ( diff_sec == 0 && diff_usec == 0 )
   {
     /* same time */
     y_is_newer = 0;
   }
   else
   {
     /* y is older */
     y_is_newer = 0;
   }

   result->tv_sec  = -diff_sec;
   result->tv_usec = -diff_usec;

   return y_is_newer;
}


/*======

FUNCTION _resolve_in_addr

DESCRIPTION
   Translates a hostname to in_addr struct

DEPENDENCIES
   n/a

RETURN VALUE
   TRUE if successful

SIDE EFFECTS
   n/a

======*/
static int _resolve_in_addr(char *host_addr, struct in_addr *in_addr_ptr)
{
   struct hostent             *hp;
   hp = gethostbyname(host_addr);


   if (hp != NULL) /* DNS OK */
   {
       /* Prevent stack based overflow attack */
       size_t length = ( hp->h_length > sizeof(struct in_addr) ) ?
                       ( sizeof(struct in_addr) ) :
                       ( hp->h_length );
       memcpy(in_addr_ptr, hp->h_addr_list[0], length);
   }
   else
   {
      /* Try IP representation */
      if (inet_aton(host_addr, in_addr_ptr) == 0)
      {
         /* IP not valid */
         XTRA_TRACE_ERROR("SNTP Client: DNS query on '%s' failed\n", host_addr);
         return FAIL;
      }
   }

   return OK;
}

/*
    Reads the NTP time stamp and returns it as a system time
    (milliseconds since January 1, 1970).
*/
static int64_t convertSntpPacketTime(gps_xtra_time_s_type * pTime) {
   int64_t seconds = ntohl(pTime->sec);
   int64_t fraction = ntohl(pTime->fsec);
   return ((seconds - SECSINSEVENTYYEARS) * 1000) + ((fraction * 1000L) / 0x100000000L);
}

/*======

FUNCTION Xtra_DownloadSntpTime

DESCRIPTION
   Download time from SNTP server

DEPENDENCIES
   n/a

RETURN VALUE
   0            if successful.
  -1            error

SIDE EFFECTS
   n/a

======*/
int Xtra_DownloadSntpTime(xtra_assist_data_time_s_type *assist_time, xtra_url_t xtra_server_addr)
{
   struct sockaddr_in         peer_addr;
   gps_xtra_sntp_pkt_s_type   sntp_req_pkt_info;
   gps_xtra_sntp_pkt_s_type   sntp_resp_pkt_info;
   struct timeval             sntp_req_send_time;
   struct timeval             sntp_resp_recv_time;
   struct timeval             time_diff;
   int64_t                    time_seconds = 0;
   int64_t                    serverReceiveTime = 0;
   int64_t                    serverTransmitTime = 0;
   int64_t                    originateTime= 0;
   int64_t                    roundTripTime = 0;
   int64_t                    clockOffset = 0;
   int64_t                    responseTime = 0;
   int64_t                    clientSendTime = 0;
   int64_t                    clientRecvTime = 0;
   int64_t                    timeDiffInMillis = 0;
   int                        sntp_socket_fd;
   int                        rc;

   /* DNS query */
   XTRA_TRACE("Resolving hostname for SNTP server...\n");

   memset(&peer_addr,0, sizeof(peer_addr));
   peer_addr.sin_family = AF_INET;
   peer_addr.sin_port = htons(SNTP_PORT_NUM);
   if (!_resolve_in_addr(xtra_server_addr, &peer_addr.sin_addr))
   {
      XTRA_TRACE_ERROR("Cannot resolve hostname name: %s\n", xtra_server_addr);
      return -1;
   }

   /* open the socket */
   XTRA_TRACE("Connecting to SNTP server: %s\n", inet_ntoa(peer_addr.sin_addr));
   sntp_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
   if(sntp_socket_fd < 0)
   {
      XTRA_TRACE_ERROR("Socket failed.\n");
      return -1;
   }

   bzero(&sntp_req_pkt_info, sizeof(sntp_req_pkt_info));

   gettimeofday(&sntp_req_send_time, (void *)0);

   /* Set timestamp among other things so server knows */
   if(loc_xtra_create_sntp_request_pkt(&sntp_req_pkt_info,&sntp_req_send_time) == -1)
   {
      XTRA_TRACE_ERROR("SNTP pkt request creation failed.\n");
      if(sntp_socket_fd) { close(sntp_socket_fd); }
      return -1;
   }


   /* Send SNTP Request */
   sendto(sntp_socket_fd,
         (void *)&sntp_req_pkt_info,
         sizeof(sntp_req_pkt_info),
         0,
         (struct sockaddr *)&peer_addr,
         sizeof(peer_addr));

   /* Wait SNTP Resp */
   rc = loc_xtra_wait_resp(sntp_socket_fd, &sntp_resp_pkt_info);
   gettimeofday(&sntp_resp_recv_time, (void *)0);
   if (rc != 0)
   {
      if (sntp_socket_fd) { close(sntp_socket_fd); }
      XTRA_TRACE_ERROR("Timed out Waiting for SNTP response \n");
      return -1; /* timed out, exit */
   }
   XTRA_TRACE("Received SNTP Resp\n");
   close(sntp_socket_fd);

   /* the following call will get the difference between Client receive time and
      client send time */
   timeval_diff(&time_diff, &sntp_resp_recv_time, &sntp_req_send_time);

   clientSendTime = (int64_t) ( (int64_t)1000*sntp_req_send_time.tv_sec +
                    (int64_t)sntp_req_send_time.tv_usec/1000 );

   clientRecvTime = (int64_t)( (int64_t)1000*sntp_resp_recv_time.tv_sec +
                    (int64_t)sntp_resp_recv_time.tv_usec/1000 );

   timeDiffInMillis = (int64_t)( (int64_t)time_diff.tv_sec*1000 + (int64_t)time_diff.tv_usec/1000 );

   responseTime = (int64_t)(clientSendTime + timeDiffInMillis);

   originateTime = convertSntpPacketTime(&sntp_resp_pkt_info.orig_time);
   serverReceiveTime = convertSntpPacketTime(&sntp_resp_pkt_info.recv_time);
   serverTransmitTime = convertSntpPacketTime(&sntp_resp_pkt_info.tx_time);

   /* round trip time in milliseconds */
   roundTripTime = (int64_t)(timeDiffInMillis - (serverTransmitTime - serverReceiveTime));
   clockOffset = (int64_t)( ( (serverReceiveTime - originateTime) + (serverTransmitTime - responseTime) )/2 );
   assist_time->uncertainty = (unsigned long)roundTripTime/2;
   assist_time->time_utc = (int64_t)(responseTime + clockOffset);

   return 0; /* successful */
}




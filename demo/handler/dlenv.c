/**************************************************************************
*
* Copyright (C) 2009 Steve Karg <skarg@users.sourceforge.net>
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "net.h"
#include "apdu.h"
#include "bip.h"
#include "datalink.h"
#include "dlenv.h"
#include "tsm.h"

static uint16_t BBMD_Timer_Seconds;
static long bbmd_timetolive_seconds = 60000;
static long bbmd_port = 0xBAC0;
static long bbmd_address = 0;
static int bbmd_result = 0;
static uint16_t bacnet_port = 0xBAC0;

/** Register as a Foreign Device with the designated BBMD.
 *
 * The BBMD's address, port, and lease time must be provided by internal variables or Environment variables.
 * If no address for the BBMD is provided, no BBMD registration will occur.
 *
 * The Environment Variables depend on define of BACDL_BIP:
 *     - BACNET_BBMD_PORT - 0..65534, defaults to 47808
 *     - BACNET_BBMD_TIMETOLIVE - 0..65535 seconds, defaults to 60000
 *     - BACNET_BBMD_ADDRESS - dotted IPv4 address
 *
 * @return Positive number (of bytes sent) on success, 0 if no registration request is sent, or -1 if registration fails.
 */
int dlenv_register_as_foreign_device(void)
{
    int retval = 0;
#if defined(BACDL_BIP)
    char *pEnv = NULL;

    pEnv = getenv("BACNET_BBMD_PORT");
    if (pEnv) {
        bbmd_port = strtol(pEnv, NULL, 0);
        if (bbmd_port > 0xFFFF) {
            bbmd_port = 0xBAC0;
        }
    }
    pEnv = getenv("BACNET_BBMD_TIMETOLIVE");
    if (pEnv) {
        bbmd_timetolive_seconds = strtol(pEnv, NULL, 0);
        if (bbmd_timetolive_seconds > 0xFFFF) {
            bbmd_timetolive_seconds = 0xFFFF;
        }
    }
    pEnv = getenv("BACNET_BBMD_ADDRESS");
    if (pEnv) {
        bbmd_address = bip_getaddrbyname(pEnv);
    }
    if (bbmd_address) {
        struct in_addr addr;
        addr.s_addr = bbmd_address;
        fprintf(stderr, "Registering with BBMD at %s:%ld for %ld seconds\n", inet_ntoa(addr), bbmd_port, bbmd_timetolive_seconds);
        retval = bvlc_register_with_bbmd(bbmd_address, htons((uint16_t) bbmd_port), (uint16_t)bbmd_timetolive_seconds);
        if (retval < 0)
            fprintf(stderr, "FAILED to Register with BBMD at %s \n", inet_ntoa(addr));
        BBMD_Timer_Seconds = (uint16_t) bbmd_timetolive_seconds;
    }

    bbmd_result = retval;
#endif
    return retval;
}

/**
 * Datalink maintenance timer
 *
 * Call this function to renew our Foreign Device Registration
 *
 * @param elapsed_seconds Number of seconds that have elapsed since last called.
 */
void dlenv_maintenance_timer(uint16_t elapsed_seconds)
{
#if defined(BACDL_BIP) && BBMD_ENABLED
    bvlc_maintenance_timer(elapsed_seconds);
#endif
#if defined(BACDL_BIP6) && BBMD6_ENABLED
    bvlc6_maintenance_timer(elapsed_seconds);
#endif
#if defined(BACDL_BIP)
    if (BBMD_Timer_Seconds) {
        if (BBMD_Timer_Seconds <= elapsed_seconds) {
            BBMD_Timer_Seconds = 0;
        } else {
            BBMD_Timer_Seconds -= elapsed_seconds;
        }
        if (BBMD_Timer_Seconds == 0) {
            (void) dlenv_register_as_foreign_device();
            // If that failed (negative), maybe just a network issue. If nothing happened (0), may be un/misconfigured. Set up to try again later in all cases.
            BBMD_Timer_Seconds = (uint16_t) bbmd_timetolive_seconds;
        }
    }
#endif
}

/** Initialize the DataLink configuration from Environment variables, or else to defaults.
 *
 * The items configured depend on which BACDL_ the code is built for, eg, BACDL_BIP.
 *
 * For most items, checks first for an environment variable, and, if found, uses that to set the item's value. Otherwise, will set to a default value.
 *
 * The Environment Variables, by BACDL_ type, are:
 * - BACDL_ALL: (the general-purpose solution)
 *   - BACNET_DATALINK to set which BACDL_ type we are using.
 * - (Any):
 *   - BACNET_APDU_TIMEOUT - set this value in milliseconds to change the APDU timeout. APDU Timeout is how much time a client waits for a response from a BACnet device.
 *   - BACNET_APDU_RETRIES - indicate the maximum number of times that an APDU shall be retransmitted.
 *   - BACNET_IFACE - set this value to dotted IP address (Windows) of the interface (see ipconfig command on Windows) for which you want to bind. On Linux, set this to the /dev interface (i.e. eth0, arc0). Default is eth0 on Linux, and the default interface on Windows. Hence, if there is only a single network interface on Windows, the applications will choose it, and this setting will not be needed.
 * - BACDL_BIP: (BACnet/IP)
 *   - BACNET_IP_PORT - UDP/IP port number (0..65534) used for BACnet/IP communications. Default is 47808 (0xBAC0).
 *   - BACNET_BBMD_PORT - UDP/IP port number (0..65534) used for Foreign Device Registration. Defaults to 47808 (0xBAC0).
 *   - BACNET_BBMD_TIMETOLIVE - number of seconds used in Foreign Device Registration (0..65535). Defaults to 60000 seconds.
 *   - BACNET_BBMD_ADDRESS - dotted IPv4 address of the BBMD or Foreign Device Registrar.
 * - BACDL_MSTP: (BACnet MS/TP)
 *   - BACNET_MAX_INFO_FRAMES
 *   - BACNET_MAX_MASTER
 *   - BACNET_MSTP_BAUD
 *   - BACNET_MSTP_MAC
 * - BACDL_BIP6: (BACnet/IPv6)
 *   - BACNET_BIP6_PORT - UDP/IP port number (0..65534) used for BACnet/IPv6 communications. Default is 47808 (0xBAC0).
 *   - BACNET_BIP6_BROADCAST - FF05::BAC0 or FF02::BAC0 or ...
 */
void dlenv_init(void)
{
    char *pEnv = NULL;

#if defined(BACDL_ALL)
    pEnv = getenv("BACNET_DATALINK");
    if (pEnv) {
        datalink_set(pEnv);
    } else {
        datalink_set(NULL);
    }
#endif
#if defined(BACDL_BIP6)
    BACNET_IP6_ADDRESS addr;
    pEnv = getenv("BACNET_BIP6_BROADCAST");
    if (pEnv) {
        bvlc6_address_set(&addr, (uint16_t) strtol(pEnv, NULL, 0), 0, 0, 0, 0, 0, 0, BIP6_MULTICAST_GROUP_ID);
        bip6_set_broadcast_addr(&addr);
    } else {
        bvlc6_address_set(&addr, BIP6_MULTICAST_SITE_LOCAL, 0, 0, 0, 0, 0, 0, BIP6_MULTICAST_GROUP_ID);
        bip6_set_broadcast_addr(&addr);
    }
    pEnv = getenv("BACNET_BIP6_PORT");
    if (pEnv) {
        bip6_set_port((uint16_t) strtol(pEnv, NULL, 0));
    } else {
        bip6_set_port(0xBAC0);
    }
#endif
#if defined(BACDL_BIP)
#if defined(BIP_DEBUG)
    BIP_Debug = true;
#endif
    pEnv = getenv("BACNET_IP_PORT");
    if (pEnv) {
        bip_set_port(htons((uint16_t) strtol(pEnv, NULL, 0)));
    } else {
        // BIP_Port is statically initialized to 0xBAC0, so if it is different, then it was programmatically altered, and we shouldn't just stomp on it here.
        // Unless it is set below 1024, since: "The range for well-known ports managed by the IANA is 0-1023."
        if (ntohs(bip_get_port()) < 1024)
            bip_set_port(htons(0xBAC0));
    }
#elif defined(BACDL_MSTP)
    pEnv = getenv("BACNET_MAX_INFO_FRAMES");
    if (pEnv) {
        dlmstp_set_max_info_frames(strtol(pEnv, NULL, 0));
    } else {
        dlmstp_set_max_info_frames(1);
    }
    pEnv = getenv("BACNET_MAX_MASTER");
    if (pEnv) {
        dlmstp_set_max_master(strtol(pEnv, NULL, 0));
    } else {
        dlmstp_set_max_master(127);
    }
    pEnv = getenv("BACNET_MSTP_BAUD");
    if (pEnv) {
        dlmstp_set_baud_rate(strtol(pEnv, NULL, 0));
    } else {
        dlmstp_set_baud_rate(38400);
    }
    pEnv = getenv("BACNET_MSTP_MAC");
    if (pEnv) {
        dlmstp_set_mac_address(strtol(pEnv, NULL, 0));
    } else {
        dlmstp_set_mac_address(127);
    }
#endif
    pEnv = getenv("BACNET_APDU_TIMEOUT");
    if (pEnv) {
        apdu_timeout_set((uint16_t) strtol(pEnv, NULL, 0));
    } else {
#if defined(BACDL_MSTP)
        apdu_timeout_set(60000);
#endif
    }
    pEnv = getenv("BACNET_APDU_RETRIES");
    if (pEnv) {
        apdu_retries_set((uint8_t) strtol(pEnv, NULL, 0));
    }
    // Initialize the Datalink Here
    if (!datalink_init(getenv("BACNET_IFACE"))) {
        exit(1);
    }
#if (MAX_TSM_TRANSACTIONS)
    pEnv = getenv("BACNET_INVOKE_ID");
    if (pEnv) {
        tsm_invokeID_set((uint8_t) strtol(pEnv, NULL, 0));
    }
#endif
    dlenv_register_as_foreign_device();
}

//*****************************************************************************
// network_task.c
//
// Network Interface
//
// Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
//
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions
//  are met:
//
//    Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the
//    distribution.
//
//    Neither the name of Texas Instruments Incorporated nor the names of
//    its contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
//  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//*****************************************************************************

#include <stdio.h>

// Simplelink includes
#include "simplelink.h"

// driverlib includes
#include "rom_map.h"
#include "uart_if.h"
#include "utils.h"
#include "gpio_if.h"

// common interface includes
#include "common.h"

//*****************************************************************************
//                          LOCAL DEFINES
//*****************************************************************************

#define AUTO_CONNECTION_TIMEOUT_COUNT (50)  // 5 Sec

//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************
extern OsiTaskHandle g_NetworkTask;
volatile unsigned long g_ulStatus = 0;               // SimpleLink Status
extern unsigned long g_uiIpAddress = 0;              // Device IP address
unsigned char g_ucConnectionSSID[SSID_LEN_MAX + 1];  // Connection SSID
unsigned char g_ucConnectionBSSID[BSSID_LEN_MAX];    // Connection BSSID

//*****************************************************************************
//                 GLOBAL VARIABLES -- End
//*****************************************************************************

//*****************************************************************************
// SimpleLink Asynchronous Event Handlers -- Start
//*****************************************************************************

//*****************************************************************************
//
//! \brief The Function Handles WLAN Events
//!
//! \param[in]  pWlanEvent - Pointer to WLAN Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkWlanEventHandler(SlWlanEvent_t *pWlanEvent) {
    if (pWlanEvent == NULL) {
        UART_PRINT("Null pointer\n\r");
        LOOP_FOREVER();
    }
    switch (pWlanEvent->Event) {
    case SL_WLAN_CONNECT_EVENT: {
        SET_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);

        //
        // Information about the connected AP (like name, MAC etc) will be
        // available in 'slWlanConnectAsyncResponse_t'
        // Applications can use it if required
        //
        //  slWlanConnectAsyncResponse_t *pEventData = NULL;
        // pEventData = &pWlanEvent->EventData.STAandP2PModeWlanConnected;
        //

        // Copy new connection SSID and BSSID to global parameters
        memcpy(g_ucConnectionSSID,
               pWlanEvent->EventData.STAandP2PModeWlanConnected.ssid_name,
               pWlanEvent->EventData.STAandP2PModeWlanConnected.ssid_len);
        memcpy(g_ucConnectionBSSID,
               pWlanEvent->EventData.STAandP2PModeWlanConnected.bssid,
               SL_BSSID_LENGTH);

        UART_PRINT(
            "[WLAN EVENT] STA Connected to the AP: %s , "
            "BSSID: %x:%x:%x:%x:%x:%x\n\r",
            g_ucConnectionSSID, g_ucConnectionBSSID[0], g_ucConnectionBSSID[1],
            g_ucConnectionBSSID[2], g_ucConnectionBSSID[3],
            g_ucConnectionBSSID[4], g_ucConnectionBSSID[5]);
    } break;

    case SL_WLAN_DISCONNECT_EVENT: {
        slWlanConnectAsyncResponse_t *pEventData = NULL;

        CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
        CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);

        pEventData = &pWlanEvent->EventData.STAandP2PModeDisconnected;

        // If the user has initiated 'Disconnect' request,
        //'reason_code' is SL_WLAN_DISCONNECT_USER_INITIATED_DISCONNECTION
        if (SL_WLAN_DISCONNECT_USER_INITIATED_DISCONNECTION ==
            pEventData->reason_code) {
            UART_PRINT(
                "[WLAN EVENT]Device disconnected from the AP: %s, "
                "BSSID: %x:%x:%x:%x:%x:%x on application's "
                "request \n\r",
                g_ucConnectionSSID, g_ucConnectionBSSID[0],
                g_ucConnectionBSSID[1], g_ucConnectionBSSID[2],
                g_ucConnectionBSSID[3], g_ucConnectionBSSID[4],
                g_ucConnectionBSSID[5]);
        } else {
            UART_PRINT(
                "[WLAN ERROR]Device disconnected from the AP AP: %s, "
                "BSSID: %x:%x:%x:%x:%x:%x on an ERROR..!! \n\r",
                g_ucConnectionSSID, g_ucConnectionBSSID[0],
                g_ucConnectionBSSID[1], g_ucConnectionBSSID[2],
                g_ucConnectionBSSID[3], g_ucConnectionBSSID[4],
                g_ucConnectionBSSID[5]);
        }
        memset(g_ucConnectionSSID, 0, sizeof(g_ucConnectionSSID));
        memset(g_ucConnectionBSSID, 0, sizeof(g_ucConnectionBSSID));
    } break;

    default: {
        UART_PRINT("[WLAN EVENT] Unexpected event [0x%x]\n\r",
                   pWlanEvent->Event);
    } break;
    }
}

//*****************************************************************************
//
//! \brief This function handles network events such as IP acquisition, IP
//!           leased, IP released etc.
//!
//! \param[in]  pNetAppEvent - Pointer to NetApp Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent) {
    if (pNetAppEvent == NULL) {
        UART_PRINT("Null pointer\n\r");
        LOOP_FOREVER();
    }

    switch (pNetAppEvent->Event) {
    case SL_NETAPP_IPV4_IPACQUIRED_EVENT: {
        SlIpV4AcquiredAsync_t *pEventData = NULL;

        SET_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);

        // Ip Acquired Event Data
        pEventData = &pNetAppEvent->EventData.ipAcquiredV4;

        // Gateway IP address
        g_uiIpAddress = pEventData->ip;

        UART_PRINT(
            "[NETAPP EVENT] IP Acquired: IP=%d.%d.%d.%d , "
            "Gateway=%d.%d.%d.%d\n\r",
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip, 3),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip, 2),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip, 1),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip, 0),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway, 3),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway, 2),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway, 1),
            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway, 0));
    } break;

    default: {
        UART_PRINT("[NETAPP EVENT] Unexpected event [0x%x] \n\r",
                   pNetAppEvent->Event);
    } break;
    }
}

//*****************************************************************************
//
//! \brief This function handles HTTP server events
//!
//! \param[in]  pServerEvent - Contains the relevant event information
//! \param[in]    pServerResponse - Should be filled by the user with the
//!                                      relevant response information
//!
//! \return None
//!
//****************************************************************************
void SimpleLinkHttpServerCallback(SlHttpServerEvent_t *pHttpEvent,
                                  SlHttpServerResponse_t *pHttpResponse) {
    // Unused in this application
}

//*****************************************************************************
//
//! \brief This function handles General Events
//!
//! \param[in]     pDevEvent - Pointer to General Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent) {
    if (pDevEvent == NULL) {
        UART_PRINT("Null pointer\n\r");
        LOOP_FOREVER();
    }

    //
    // Most of the general errors are not FATAL are are to be handled
    // appropriately by the application
    //
    UART_PRINT("[GENERAL EVENT] - ID=[%d] Sender=[%d]\n\n",
               pDevEvent->EventData.deviceEvent.status,
               pDevEvent->EventData.deviceEvent.sender);
}

//*****************************************************************************
//
//! This function handles socket events indication
//!
//! \param[in]      pSock - Pointer to Socket Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkSockEventHandler(SlSockEvent_t *pSock) {
    if (pSock == NULL) {
        return;
    }

    //
    // This application doesn't work w/ socket - Events are not expected
    //
    switch (pSock->Event) {
    case SL_SOCKET_TX_FAILED_EVENT:
        switch (pSock->socketAsyncEvent.SockTxFailData.status) {
        case SL_ECLOSE:
            // UART_PRINT("[SOCK ERROR] - close socket (%d) operation failed to
            // transmit all queued packets\n\n", pSock->EventData.sd);
            break;
        default:
            // UART_PRINT("[SOCK ERROR] - TX FAILED  :  socket %d , reason (%d)
            // \n\n",  pSock->EventData.sd, pSock->EventData.status); */
            break;
        }
        break;

    default:
        // UART_PRINT("[SOCK EVENT] - Unexpected Event
        // [%x0x]\n\n",pSock->Event);
        break;
    }
}

//*****************************************************************************
// SimpleLink Asynchronous Event Handlers -- End
//*****************************************************************************

//*****************************************************************************
//
//! \brief This function initializes the application variables
//!
//! \param    None
//!
//! \return None
//!
//*****************************************************************************
static void InitializeAppVariables() {
    g_ulStatus = 0;
    g_uiIpAddress = 0;
    memset(g_ucConnectionSSID, 0, sizeof(g_ucConnectionSSID));
    memset(g_ucConnectionBSSID, 0, sizeof(g_ucConnectionBSSID));
}

//*****************************************************************************
//! \brief This function puts the device in its default state. It:
//!           - Set the mode to STATION
//!           - Configures connection policy to Auto and AutoSmartConfig
//!           - Deletes all the stored profiles
//!           - Enables DHCP
//!           - Disables Scan policy
//!           - Sets Tx power to maximum
//!           - Sets power policy to normal
//!           - Unregister mDNS services
//!           - Remove all filters
//!
//! \param   none
//! \return  On success, zero is returned. On error, negative is returned
//*****************************************************************************
long ConfigureSimpleLinkToDefaultState() {
    SlVersionFull ver = {0};
    _WlanRxFilterOperationCommandBuff_t RxFilterIdMask = {0};

    unsigned char ucVal = 1;
    unsigned char ucConfigOpt = 0;
    unsigned char ucConfigLen = 0;
    unsigned char ucPower = 0;

    long lRetVal = -1;
    long lMode = -1;

    lMode = sl_Start(0, 0, 0);
    ASSERT_ON_ERROR(lMode);

    // If the device is not in station-mode, try configuring it in station-mode
    if (ROLE_STA != lMode) {
        if (ROLE_AP == lMode) {
            // If the device is in AP mode, we need to wait for this event
            // before doing anything
            while (!IS_IP_ACQUIRED(g_ulStatus)) {
#ifndef SL_PLATFORM_MULTI_THREADED
                _SlNonOsMainLoopTask();
#endif
            }
        }

        // Switch to STA role and restart
        lRetVal = sl_WlanSetMode(ROLE_STA);
        ASSERT_ON_ERROR(lRetVal);

        lRetVal = sl_Stop(0xFF);
        ASSERT_ON_ERROR(lRetVal);

        lRetVal = sl_Start(0, 0, 0);
        ASSERT_ON_ERROR(lRetVal);

        // Check if the device is in station again
        if (ROLE_STA != lRetVal) {
            // We don't want to proceed if the device is not coming up in
            // STA-mode
            return -1;
        }
    }

    // Get the device's version-information
    ucConfigOpt = SL_DEVICE_GENERAL_VERSION;
    ucConfigLen = sizeof(ver);
    lRetVal = sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION, &ucConfigOpt,
                        &ucConfigLen, (unsigned char *)(&ver));
    ASSERT_ON_ERROR(lRetVal);

    UART_PRINT("Host Driver Version: %s\n\r", SL_DRIVER_VERSION);
    UART_PRINT("Build Version %d.%d.%d.%d.31.%d.%d.%d.%d.%d.%d.%d.%d\n\r",
               ver.NwpVersion[0], ver.NwpVersion[1], ver.NwpVersion[2],
               ver.NwpVersion[3], ver.ChipFwAndPhyVersion.FwVersion[0],
               ver.ChipFwAndPhyVersion.FwVersion[1],
               ver.ChipFwAndPhyVersion.FwVersion[2],
               ver.ChipFwAndPhyVersion.FwVersion[3],
               ver.ChipFwAndPhyVersion.PhyVersion[0],
               ver.ChipFwAndPhyVersion.PhyVersion[1],
               ver.ChipFwAndPhyVersion.PhyVersion[2],
               ver.ChipFwAndPhyVersion.PhyVersion[3]);

    // Set connection policy to Auto + SmartConfig
    //      (Device's default connection policy)
    lRetVal = sl_WlanPolicySet(SL_POLICY_CONNECTION,
                               SL_CONNECTION_POLICY(1, 0, 0, 0, 1), NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Remove all profiles
    lRetVal = sl_WlanProfileDel(0xFF);
    ASSERT_ON_ERROR(lRetVal);

    //
    // Device in station-mode. Disconnect previous connection if any
    // The function returns 0 if 'Disconnected done', negative number if already
    // disconnected Wait for 'disconnection' event if 0 is returned, Ignore
    // other return-codes
    //
    lRetVal = sl_WlanDisconnect();
    if (0 == lRetVal) {
        // Wait
        while (IS_CONNECTED(g_ulStatus)) {
#ifndef SL_PLATFORM_MULTI_THREADED
            _SlNonOsMainLoopTask();
#endif
        }
    }

    // Enable DHCP client
    lRetVal = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE, 1, 1, &ucVal);
    ASSERT_ON_ERROR(lRetVal);

    // Disable scan
    ucConfigOpt = SL_SCAN_POLICY(0);
    lRetVal = sl_WlanPolicySet(SL_POLICY_SCAN, ucConfigOpt, NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Set Tx power level for station mode
    // Number between 0-15, as dB offset from max power - 0 will set max power
    ucPower = 0;
    lRetVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID,
                         WLAN_GENERAL_PARAM_OPT_STA_TX_POWER, 1,
                         (unsigned char *)&ucPower);
    ASSERT_ON_ERROR(lRetVal);

    // Set PM policy to normal
    lRetVal = sl_WlanPolicySet(SL_POLICY_PM, SL_NORMAL_POLICY, NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Unregister mDNS services
    lRetVal = sl_NetAppMDNSUnRegisterService(0, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Remove  all 64 filters (8*8)
    memset(RxFilterIdMask.FilterIdMask, 0xFF, 8);
    lRetVal = sl_WlanRxFilterSet(SL_REMOVE_RX_FILTER, (_u8 *)&RxFilterIdMask,
                                 sizeof(_WlanRxFilterOperationCommandBuff_t));
    ASSERT_ON_ERROR(lRetVal);

    lRetVal = sl_Stop(SL_STOP_TIMEOUT);
    ASSERT_ON_ERROR(lRetVal);

    InitializeAppVariables();

    return lRetVal;  // Success
}

//****************************************************************************
//
//!  \brief Connecting to a WLAN Accesspoint
//!
//!   This function connects to the required AP (SSID_NAME) with Security
//!   parameters specified in te form of macros at the top of this file
//!
//!   \param[in]              None
//!
//!   \return       status value
//!
//!   \warning    If the WLAN connection fails or we don't aquire an IP
//!            address, It will be stuck in this function forever.
//
//****************************************************************************
long WlanConnect() {
    SlSecParams_t secParams = {0};
    long lRetVal = 0;

    secParams.Key = (signed char *)SECURITY_KEY;
    secParams.KeyLen = strlen(SECURITY_KEY);
    secParams.Type = SECURITY_TYPE;

    lRetVal = sl_WlanConnect((signed char *)SSID_NAME, strlen(SSID_NAME), 0,
                             &secParams, 0);
    ASSERT_ON_ERROR(lRetVal);

    while ((!IS_CONNECTED(g_ulStatus)) || (!IS_IP_ACQUIRED(g_ulStatus))) {
// Wait for WLAN Event
#ifndef SL_PLATFORM_MULTI_THREADED
        _SlNonOsMainLoopTask();
#endif
    }

    return SUCCESS;
}

//*****************************************************************************
//
//! Network Task
//!
//! \param  pvParameters - Parameters to the task's entry function
//!
//! \return None
//!
//*****************************************************************************
void Network(void *pvParameters) {
    long lRetVal = -1;

    // Initialize Global Variable
    InitializeAppVariables();

    //
    // Following function configure the device to default state by cleaning
    // the persistent settings stored in NVMEM (viz. connection profiles &
    // policies, power policy etc)
    //
    // Applications may choose to skip this step if the developer is sure
    // that the device is in its desired state at start of applicaton
    //
    // Note that all profiles and persistent settings that were done on the
    // device will be lost
    //
    lRetVal = ConfigureSimpleLinkToDefaultState();

    if (lRetVal < 0) {
        UART_PRINT("Failed to configure the device in its default state \n\r");
        LOOP_FOREVER();
    }

    UART_PRINT("Device is configured in default state \n\r");

    //
    // Asumption is that the device is configured in station mode already
    // and it is in its default state
    //
    lRetVal = sl_Start(0, 0, 0);
    if (lRetVal < 0 || lRetVal != ROLE_STA) {
        UART_PRINT("Failed to start the device \n\r");
        LOOP_FOREVER();
    }

    UART_PRINT("Device started as STATION \n\r");

    UART_PRINT("Connecting to AP: %s ...\r\n", SSID_NAME);

    //
    // Connecting to WLAN AP
    //
    lRetVal = WlanConnect();
    if (lRetVal < 0) {
        UART_PRINT("Failed to establish connection w/ an AP \n\r");
        LOOP_FOREVER();
    }

    UART_PRINT("Connected to AP: %s \n\r", SSID_NAME);

    UART_PRINT("Device IP: %d.%d.%d.%d\n\r\n\r", SL_IPV4_BYTE(g_uiIpAddress, 3),
               SL_IPV4_BYTE(g_uiIpAddress, 2), SL_IPV4_BYTE(g_uiIpAddress, 1),
               SL_IPV4_BYTE(g_uiIpAddress, 0));

    osi_TaskDelete(&g_NetworkTask);
}

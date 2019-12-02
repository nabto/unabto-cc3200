# uNabto CC3200 Streaming Demo

This project turns the CC3200 into a Nabto streaming echo server. It uses FreeRTOS and leverages the CC3200's hardware accelerated cryptography capabilities.

# How to set it up

## Step 1: Clone the repository

*Hint: Preferably, do this in your Code Composer Studio workspace.*

```
git clone --recursive https://github.com/nabto/unabto-cc3200.git
```

## Step 2: Import Project into Code Composer Studio

1. *File* > *Import*
2. Select *Code Compose Studio* > *CCS Projects*
3. Press *Next*
4. *Browse...* to the `unabto-cc3200` folder inside you CCS workspace
5. Untick *Copy projects into workspace*
6. Press *Finish*

If your CC3200 SDK is not located in `C:/TI/CC3200SDK_1.2.0/cc3200-sdk` goto

1. *Project* > *Properties*
2. *Select Resource* > *Linked Resources*

and fix the *Path Variables* in order to point to the right directories.

## Step 3: Set Wi-Fi SSID and Password

If not done for previous SDK examples, open `cc3200-sdk\example\common\common.h` and configure the access-point defines `SSID_NAME`, `SECURITY_TYPE` and `SECURITY_KEY`.
    
## Step 4: Set Nabto Device ID and Key

Enter your *Device ID* and *Key* from the [Nabto Cloud console](https://cloud.console.nabto.com) in `unabto_task.c` lines 66 and 67:

```
    char* nabtoId = "<DEVICE ID>";
    const char* presharedKey = "<KEY>";
```

## Step 5: Build the project

*Project* > *Build All*.

## Step 6: Flash the Image

Please refer to the [CC31xx & CC32xx UniFlash Quick Start Guide](http://processors.wiki.ti.com/index.php/CC31xx_%26_CC32xx_UniFlash_Quick_Start_Guide#CC32xx_MCU_image_flashing) for flashing the image located in `<YOUR-CCS-WORSPACE>\unabto-cc3200\Release\unabto-cc3200.bin`.

# How to test the application

Using a serial terminal you should see a printout similar to the following every time the CC3200 starts up:

```
*************************************************
                   CC3200 + uNabto
*************************************************

Host Driver Version: 1.0.1.6
Build Version 2.6.0.5.31.1.4.0.1.1.0.3.34
Device is configured in default state
Device started as STATION
Connecting to AP: ASUS ...
[WLAN EVENT] STA Connected to the AP: ASUS , BSSID: c8:60:0:93:88:f8
[NETAPP EVENT] IP Acquired: IP=192.168.1.18 , Gateway=192.168.1.1
Connected to AP: ASUS
Device IP: 192.168.1.18

Device id: 'deviceid.demo.nab.to'
Program Release 123.456
Application event framework using SYNC model
sizeof(stream__)=u
SECURE ATTACH: 1, DATA: 1
NONCE_SIZE: 32, CLEAR_TEXT: 0
Nabto was successfully initialized
SECURE ATTACH: 1, DATA: 1
NONCE_SIZE: 32, CLEAR_TEXT: 0
State change from IDLE to WAIT_DNS
Resolving dns: deviceid.demo.nab.to
State change from WAIT_DNS to WAIT_BS
State change from WAIT_BS to WAIT_GSP
########    U_INVITE with LARGE nonce sent, version: - URL: -
State change from WAIT_GSP to ATTACHED
```

Using the Nabto [Echo Stream Tester](https://github.com/nabto/echo-stream-tester) you can now test the echo server by sending and receiving e.g. 1 MB of data:

```
./echo_stream_tester <DEVICE ID> 1000000
```


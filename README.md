# uNabto CC3200 Audio Demo

This project turns the CC3200 into a Nabto audio streaming device. It uses FreeRTOS and leverages the CC3200's hardware accelerated cryptography capabilities.

# How to set it up

## Step 1: Clone the repository

*Hint: Preferably, do this in your Code Composer Studio workspace.*

```
git clone --recursive https://github.com/nabto/unabto-cc3200.git
git branch audio
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

Enter your *Device ID* and *Key* from [portal.appmyproduct.com](portal.appmyproduct.com) in `unabto_task.c` lines 66 and 67:

```
    char* nabtoId = "<DEVICE ID>";
    const char* presharedKey = "<KEY>";
```

## Step 5: Build the project

*Project* > *Build All*.

## Step 6: Flash the Image

Please refer to the [CC31xx & CC32xx UniFlash Quick Start Guide](http://processors.wiki.ti.com/index.php/CC31xx_%26_CC32xx_UniFlash_Quick_Start_Guide#CC32xx_MCU_image_flashing) for flashing the image located in `<YOUR-CCS-WORSPACE>\unabto-cc3200\Release\unabto-cc3200.bin`.

## Step 7: Stream Audio

Get the Android Audio Stream Demo)[https://github.com/nabto/android-audio-stream-demo] App and connect to your CC3200 using the unqiue ID. You should now be able to hear the microphone (and line-in) input of the opposite device.

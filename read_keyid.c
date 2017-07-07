//*****************************************************************************
// read_id-key.c
//
// uNabto Task
//
//*****************************************************************************

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Nabto Includes
#include <unabto/unabto_app.h>
#include <unabto/unabto_common_main.h>
#include <unabto_version.h>

#include "read_keyid.h"

#include "fs.h"

// Global statics
char nabtoId[129];
char presharedKey[33];


int8_t readIdAndKey(char *id, char *key) {


    strcpy(nabtoId, "phavkzs3.smtth.appmyproduct.com");
    strcpy(presharedKey, "bd96af80e59d0d72c290807973fdb7f1");

    return 0;

    char preallok_filebuffer[256];


    char DeviceFileName[] = "ampidkey.txt";

    int32_t DeviceFileHandle = -1;
    SlFsFileInfo_t FsFileInfo;

    //NABTO_LOG_INFO(("Trying to read info about id+key file file"));
    int32_t retval = 0;
    retval = sl_FsGetInfo(DeviceFileName, 0, &FsFileInfo);
    if( retval !=0 ) {
       //NABTO_LOG_INFO(("Could not getinfo for id+key file: amp-id-key.txt, errorno:%d", retval));
       return -1;
    }

    //NABTO_LOG_INFO(("Trying to open id+key file"));

    retval = sl_FsOpen(DeviceFileName, FS_MODE_OPEN_READ, NULL, &DeviceFileHandle);
    if( retval != 0) {
       //NABTO_LOG_INFO(("Could not open id+key file: amp-id-key.txt , error:%d", retval));
       return -1;
    }

    
    int32_t len = FsFileInfo.FileLen < sizeof(preallok_filebuffer) ? FsFileInfo.FileLen : sizeof(preallok_filebuffer);
    //NABTO_LOG_INFO(("Length to read:%d", len));


    //NABTO_LOG_INFO(("Reading %d from key+id file", len));

    retval = sl_FsRead(DeviceFileHandle, 0, preallok_filebuffer, len);
    if( retval != 0) {
       //NABTO_LOG_INFO(("Could not read from id+key file, errorno:%d", retval));
       return -1;
    }


}


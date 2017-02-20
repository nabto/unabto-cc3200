// Hardware & driverlib library includes
#include "rom.h"
#include "rom_map.h"
#include "utils.h"
#include "hw_ints.h"

// common interface includes
#include "common.h"
#include "uart_if.h"
// Demo app includes
#include "circ_buff.h"
#include "audiocodec.h"

#include "audio_config.h"

//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************

extern tCircularBuffer *pPlayBuffer;
extern tCircularBuffer *pRecordBuffer;

//*****************************************************************************
//                 GLOBAL VARIABLES -- End
//*****************************************************************************

//*****************************************************************************
//
//! Audio Test Routine
//!
//! \param pvParameters     Parameters to the task's entry function
//!
//! \return None
//
//*****************************************************************************
void AudioTest( void *pvParameters ) {
	UART_PRINT("+++ Audio Test +++\n\r");
	long lRetVal = -1;
	int iBufferFilled = 0;

    while(1) {
    	iBufferFilled = GetBufferSize(pRecordBuffer);
    	if(iBufferFilled >= (2*PACKET_SIZE)) {
    		// loopback microphone to speakers:
    		lRetVal = FillBuffer(pPlayBuffer, (unsigned char*)(pRecordBuffer->pucReadPtr), PACKET_SIZE);
            if(lRetVal < 0) {
            	UART_PRINT("Unable to fill play buffer\n\r");
            	LOOP_FOREVER();
            }
            //UART_PRINT("%i\r\n", iBufferFilled);
            UpdateReadPtr(pRecordBuffer, PACKET_SIZE);
            //MAP_UtilsDelay(1000);
        }
    	//MAP_UtilsDelay(1000);
    }
}

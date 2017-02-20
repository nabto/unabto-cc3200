//*****************************************************************************
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

// Standard includes
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// SimpleLink includes
#include "simplelink.h"

// free-rtos/ ti_rtos includes
#include "osi.h"

// Hardware & DriverLib library includes.
#include "hw_common_reg.h"
#include "hw_ints.h"
#include "hw_memmap.h"
#include "hw_types.h"
#include "interrupt.h"
#include "i2s.h"
#include "udma.h"
#include "prcm.h"
#include "rom.h"
#include "rom_map.h"
#include "uart.h"
#include "utils.h"
#include "gpio.h"
#include "gpio_if.h"
#include "shamd5.h"
#include "aes.h"

// Common interface includes
#include "common.h"
#include "udma_if.h"
#include "uart_if.h"
#include "i2c_if.h"

// App include
#include "pinmux.h"
#include "circ_buff.h"
#include "audiocodec.h"
#include "i2s_if.h"
#include "pcm_handler.h"
#include "audio_config.h"

#define OSI_STACK_SIZE 2048

//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************
tCircularBuffer *pRecordBuffer;
tCircularBuffer *pPlayBuffer;

OsiTaskHandle g_NetworkTask = NULL;
OsiTaskHandle g_UNabtoTask = NULL;
OsiTaskHandle g_AudioTestTask = NULL;

#if defined(ccs)
extern void (*const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif
//*****************************************************************************
//                 GLOBAL VARIABLES -- End
//*****************************************************************************

//******************************************************************************
//                    FUNCTION DECLARATIONS
//******************************************************************************
extern void Network(void *pvParameters);
extern void UNabto(void *pvParameters);
extern void AudioTest(void *pvParameters);

extern void SHAMD5IntHandler(void);
extern void AESIntHandler(void);

//*****************************************************************************
//
//! Application defined hook (or callback) function - the tick hook.
//! The tick interrupt can optionally call this
//!
//! \param  none
//!
//! \return none
//!
//*****************************************************************************
void vApplicationTickHook(void) {
}

//*****************************************************************************
//
//! Application defined hook (or callback) function - assert
//!
//! \param  none
//!
//! \return none
//!
//*****************************************************************************
void vAssertCalled(const char *pcFile, unsigned long ulLine) {
    LOOP_FOREVER();
}

//*****************************************************************************
//
//! Application defined idle task hook
//!
//! \param  none
//!
//! \return none
//!
//*****************************************************************************
void vApplicationIdleHook(void) {
}

//*****************************************************************************
//
//! Application provided stack overflow hook function.
//!
//! \param  handle of the offending task
//! \param  name  of the offending task
//!
//! \return none
//!
//*****************************************************************************
void vApplicationStackOverflowHook(OsiTaskHandle *pxTask,
                                   signed char *pcTaskName) {
    (void)pxTask;
    (void)pcTaskName;

    LOOP_FOREVER();
}

void vApplicationMallocFailedHook() {
    LOOP_FOREVER();
}

//*****************************************************************************
//
//! Application startup display on UART
//!
//! \param  none
//!
//! \return none
//!
//*****************************************************************************
static void DisplayBanner(void) {
    UART_PRINT("*************************************************\n\r");
    UART_PRINT("           CC3200 + uNabto + Audio               \n\r");
    UART_PRINT("*************************************************\n\r");
    UART_PRINT("\n\r");
}

//*****************************************************************************
//
//! Board Initialization & Configuration
//!
//! \param  none
//!
//! \return none
//
//*****************************************************************************
void BoardInit(void) {
/* In case of TI-RTOS vector table is initialize by OS itself */
#ifndef USE_TIRTOS
//
// Set vector table base
//
#if defined(ccs)
    MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
#endif
#if defined(ewarm)
    MAP_IntVTableBaseSet((unsigned long)&__vector_table);
#endif
#endif
    //
    // Enable Processor
    //
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);

    PRCMCC3200MCUInit();
}
//******************************************************************************
//                            MAIN FUNCTION
//******************************************************************************
int main() {
    long lRetVal = -1;

    BoardInit();

    PinMuxConfig();

    InitTerm();

    DisplayBanner();

    //
	// Initialising the I2C Interface
	//
	lRetVal = I2C_IF_Open(I2C_MASTER_MODE_FST);
	if (lRetVal < 0) {
		ERR_PRINT(lRetVal);
		LOOP_FOREVER();
	}

    //
    // Enable the crypto module
    //
    MAP_PRCMPeripheralClkEnable(PRCM_DTHE, PRCM_RUN_MODE_CLK);

    //
    // Enable crypto interrupts
    //
    MAP_SHAMD5IntRegister(SHAMD5_BASE, SHAMD5IntHandler);
    MAP_AESIntRegister(AES_BASE, AESIntHandler);

    //
    // Create Record Buffer
    //
    pRecordBuffer = CreateCircularBuffer(RECORD_BUFFER_SIZE);
    if (pRecordBuffer == NULL) {
        UART_PRINT("Unable to Allocate Memory for Record Buffer\n\r");
        LOOP_FOREVER();
    }

    //
    // Create Play Buffer
    //
    pPlayBuffer = CreateCircularBuffer(PLAY_BUFFER_SIZE);
    if (pPlayBuffer == NULL) {
        UART_PRINT("Unable to Allocate Memory for Play Buffer\n\r");
        LOOP_FOREVER();
    }

    //
    // Configure Audio Codec
    //
    AudioCodecReset(AUDIO_CODEC_TI_3254, NULL);

    AudioCodecConfig(AUDIO_CODEC_TI_3254, AUDIO_CODEC_16_BIT, 16000,
                     AUDIO_CODEC_STEREO, AUDIO_CODEC_SPEAKER_ALL,
                     AUDIO_CODEC_MIC_ALL);

    AudioCodecSpeakerVolCtrl(AUDIO_CODEC_TI_3254, AUDIO_CODEC_SPEAKER_ALL, 50);

    AudioCodecMicVolCtrl(AUDIO_CODEC_TI_3254, AUDIO_CODEC_MIC_ALL, 50);

    //
    // Initialize the Audio (I2S) Module
    //
    AudioInit();

    //
    // Initialize the DMA Module
    //
    UDMAInit();

    // Tx (Play)
    UDMAChannelSelect(UDMA_CH5_I2S_TX, NULL);
    SetupPingPongDMATransferRx(pPlayBuffer);

    // Rx (Record)
    UDMAChannelSelect(UDMA_CH4_I2S_RX, NULL);
    SetupPingPongDMATransferTx(pRecordBuffer);

    //
    // Setup the Audio In/Out
    //
    lRetVal = AudioSetupDMAMode(DMAPingPongCompleteAppCB_opt,
                                CB_EVENT_CONFIG_SZ, I2S_MODE_RX_TX);
    if (lRetVal < 0) {
        ERR_PRINT(lRetVal);
        LOOP_FOREVER();
    }
    AudioCaptureRendererConfigure(AUDIO_CODEC_16_BIT, 16000, AUDIO_CODEC_STEREO,
                                  I2S_MODE_RX_TX, 1);
    //
    // Start Audio Tx/Rx
    //
    Audio_Start(I2S_MODE_RX_TX);

    //
    // Start the simplelink thread
    //
    lRetVal = VStartSimpleLinkSpawnTask(9);
    if (lRetVal < 0) {
        ERR_PRINT(lRetVal);
        LOOP_FOREVER();
    }

    //
    // Start the Network Task
    //
    lRetVal = osi_TaskCreate(Network, (signed char *)"NetworkTask",
                             OSI_STACK_SIZE, NULL, 1, &g_NetworkTask);
    if (lRetVal < 0) {
        ERR_PRINT(lRetVal);
        LOOP_FOREVER();
    }

    //
    // Start the uNabto Task
    //
    lRetVal = osi_TaskCreate(UNabto, (signed char *)"uNabtoTask", OSI_STACK_SIZE,
                             NULL, 1, &g_UNabtoTask);
    if (lRetVal < 0) {
        ERR_PRINT(lRetVal);
        LOOP_FOREVER();
    }

    //
	// Start the Audio Test Task
	//
    lRetVal = osi_TaskCreate(AudioTest, (signed char *)"AudioTest", OSI_STACK_SIZE,
                             NULL, 1, &g_AudioTestTask);
	if (lRetVal < 0) {
		ERR_PRINT(lRetVal);
		LOOP_FOREVER();
	}

    //
    // Start the task scheduler
    //
    osi_start();
}

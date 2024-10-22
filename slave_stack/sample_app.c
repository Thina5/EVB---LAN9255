/*******************************************************************************
  Sample application source file 

  Company:
    Microchip Technology Inc.

  File Name:
    sample_app.c

  Summary:
    This source file is modified for PDO input and output processing

  Description:
    This source file is modified for PDO input and output processing. 
	This file can be replaced with the generated sample file by the SSC tool.
*******************************************************************************/

/*****************************************************************************
 Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.

Microchip Technology Inc. and its subsidiaries.

Subject to your compliance with these terms, you may use Microchip software 
and any derivatives exclusively with Microchip products. It is your 
responsibility to comply with third party license terms applicable to your 
use of third party software (including open source software) that may 
accompany Microchip software.

THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER 
EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED 
WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A PARTICULAR 
PURPOSE.

IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, 
INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND 
WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS 
BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE 
FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN 
ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY, 
THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
*****************************************************************************/
/*-----------------------------------------------------------------------------------------
------
------    Includes
------
-----------------------------------------------------------------------------------------*/
#include "ecat_def.h"

#include "applInterface.h"

#define _SAMPLE_APP_ 1
#include "sample_app.h"
#undef _SAMPLE_APP_

#include "bootmode.h"
#include "ethercat_configuration.h"
#include "app_lan9255.h"


UINT32  gTriggerCounterValMeasure=0;
// configure this variable to generate the counter value w.r.t to that value.
// if the gFOETestFrameSize == 1, then the counter value will increase by 1000
// if the gFOETestFrameSize == 1000, the the counter value will increase by 1
UINT32  gFOETestFrameSize=1000;

#if defined(ETHERCAT_USE_FOE)
#define FOE_FILEDOWNLOAD_PASSWORD ETHERCAT_FOE_FILE_DOWNLOAD_PASSWORD
/*--------------------------------------------------------------------------------------
------
------    local types and defines
------
--------------------------------------------------------------------------------------*/
#define    MAX_FILE_NAME_SIZE    ETHERCAT_FOE_FILE_NAME_LENGTH

/** \brief  MAX_FILE_SIZE: Maximum file size */
#define MAX_FILE_SIZE	APP_MAX_NVM_BANK_SIZE   //SAME53 Bank A/B Size
#define MAX_BLOCK_SIZE  APP_ERASE_BLOCK_SIZE    //SAME53 Block Size
/*-----------------------------------------------------------------------------------------
------
------    local variables and constants
------
-----------------------------------------------------------------------------------------*/
UINT32			nFileWriteOffset=0;
UINT32          nFileTotalLengthOffset = 0;   
UINT32			nFileStartWriteAddress=0;
CHAR			aFileName[MAX_FILE_NAME_SIZE];
UINT8			aFileData[MAX_BLOCK_SIZE];

UINT32			u32FileSize=0;
volatile BOOL	gFirmwareDownload_Started = FALSE;
volatile BOOL	gFirmwareDownload_Finished = FALSE;





















/*-----------------------------------------------------------------------------------------
------
------    application specific functions
------
-----------------------------------------------------------------------------------------*/

void APP_FW_startDownload(void)
{
	/************************************************************************/
	/* Check if the download is already started, if yes return error.                                                                      */
	/************************************************************************/
	if(gFirmwareDownload_Started)
	{
		return ;		
	}
	/** 
	 * Remember a firmware download has started, when BOOT state is requested
	 * to flash new binary to the other BANK B.
	 */
	gFirmwareDownload_Started = 1;
	nFileWriteOffset = 0;
	nFileStartWriteAddress = 0;
	nFileTotalLengthOffset = 0;
}


void APP_FW_StateBOOTtoINIT(void)
{
	// Download is finished which was already initiated
	// to update firmware and clean up  flash
	if(gFirmwareDownload_Started == 1)
	{
		gFirmwareDownload_Finished = 1;		
	}
}


BOOL APP_FW_GetDownloadStateFinished(void)
{
	return gFirmwareDownload_Finished;
}
/*-----------------------------------------------------------------------------------------
------
------    generic functions
------
-----------------------------------------------------------------------------------------*/
UINT16 FoE_Read(UINT16 MBXMEM * pName, UINT16 nameSize, UINT32 password, UINT16 maxBlockSize, UINT16 *pData)
{
    UINT16 i = 0;
    UINT16 sizeError = 0;

    CHAR aReadFileName[MAX_FILE_NAME_SIZE];


    if ((nameSize + 1) > MAX_FILE_NAME_SIZE)
    {

        return ECAT_FOE_ERRCODE_DISKFULL;
    }

    /*Read requested file name to endianess conversion if required*/
    MBXSTRCPY(aReadFileName, pName, nameSize);
    aReadFileName[nameSize] = '\0';

    if (u32FileSize == 0)
    {
        /* No file stored*/
        return ECAT_FOE_ERRCODE_NOTFOUND;
    }

    /* for test only the written file name can be read */
    for (i = 0; i < nameSize; i++)
    {
        if (aReadFileName[i] != aFileName[i])
        {
            /* file name not found */
            return ECAT_FOE_ERRCODE_NOTFOUND;
        }
    }

    sizeError = maxBlockSize;

    if (u32FileSize < sizeError)
    {
        sizeError = (UINT16)u32FileSize;
    }

    /*copy the first foe data block*/
    MEMCPY(pData, aFileData, sizeError);

    return sizeError;
}

UINT16 FoE_ReadData(UINT32 offset, UINT16 maxBlockSize, UINT16 *pData)
{
    UINT16 sizeError = 0;

    if (u32FileSize < offset)
    {
        return 0;
    }

    /*get file length to send*/
    sizeError = (UINT16)(u32FileSize - offset);


    if (sizeError > maxBlockSize)
    {
        /*transmit max block size if the file data to be send is greater than the max data block*/
        sizeError = maxBlockSize;
    }

    /*copy the foe data block 2 .. n*/
    MEMCPY(pData, &(((UINT8 *)aFileData)[offset]), sizeError);

    return sizeError;
}


UINT16 FoE_WriteData(UINT16 MBXMEM * pData, UINT16 Size, BOOL bDataFollowing)
{
	if ((Size > 0) && (nFileStartWriteAddress < MAX_FILE_SIZE))
    {
    	//If file size is more than 512 KB, error will be thrown as disk full
    	if((nFileTotalLengthOffset+Size) > MAX_FILE_SIZE)
        {
            nFileTotalLengthOffset = 0;
            return ECAT_FOE_ERRCODE_DISKFULL;
        }
        
		if ((nFileWriteOffset + Size) > MAX_BLOCK_SIZE)
		{
			u32FileSize = MAX_BLOCK_SIZE - nFileWriteOffset;
			MBXMEMCPY(&aFileData[nFileWriteOffset], (UINT8*)pData, u32FileSize);
			nFileWriteOffset += u32FileSize;
			
			APP_FlashWrite(nFileStartWriteAddress, aFileData);
			
			nFileStartWriteAddress += nFileWriteOffset;
			nFileTotalLengthOffset += nFileWriteOffset;
			
			pData = (UINT16 MBXMEM *)((UINT8*)pData + u32FileSize);
			Size -= u32FileSize;
			nFileWriteOffset = 0;
			
			MBXMEMCPY(&aFileData[nFileWriteOffset], (UINT8*)pData, Size);
			nFileWriteOffset += Size;
		}
		else
		{
			MBXMEMCPY(&aFileData[nFileWriteOffset], (UINT8*)pData, Size);
			nFileWriteOffset += Size;
		}
		
		if ((nFileWriteOffset  == MAX_BLOCK_SIZE) || (bDataFollowing == FALSE))
		{
			if (nFileWriteOffset  < MAX_BLOCK_SIZE)
			{
				u32FileSize = MAX_BLOCK_SIZE - nFileWriteOffset;
				MBXMEMSET(&aFileData[nFileWriteOffset], 0xFF, u32FileSize);
			}
			APP_FlashWrite(nFileStartWriteAddress, aFileData);
			nFileStartWriteAddress += nFileWriteOffset;
			nFileTotalLengthOffset += nFileWriteOffset;
			nFileWriteOffset = 0;
		}
    }
    else    
    {
		//If file size is less than 0 KB, error will be thrown as invalid firmware
        return ECAT_FOE_ERRCODE_INVALID_FIRMWARE;
    }

    return 0;
}

UINT16 FoE_Write(UINT16 MBXMEM * pName, UINT16 nameSize, UINT32 password)
{
	BOOL writeCheck1 = FALSE;
	BOOL writeCheck2 = FALSE;
    if (nameSize < MAX_FILE_NAME_SIZE)
    {
        /* for test every file name can be written */
        MBXSTRCPY(aFileName, pName, nameSize);
        MBXSTRCPY(aFileName + nameSize, "\0", 1); //string termination

        nFileWriteOffset = 0;
        nFileTotalLengthOffset = 0;
        u32FileSize = 0;
		writeCheck1 = TRUE;

    }
    else
    {
        return ECAT_FOE_ERRCODE_DISKFULL;
    }
	
	/* Password check is provided */
	if(password == FOE_FILEDOWNLOAD_PASSWORD)
	{
		writeCheck2 = TRUE;		
	}
	else
	{
		return ECAT_FOE_ERRCODE_DISKFULL;		
	}

	if(writeCheck1 && writeCheck2)
	{
		APP_FW_startDownload();
	}
	
    return 0;
}


/////////////////////////////////////////////////////////////////////////////////////////
/**
\brief    This function will called from the main and after the MainInit()
*////////////////////////////////////////////////////////////////////////////////////////
void BL_FOE_Application(void)
{
	/* Reset FOE application function pointer*/
	gFirmwareDownload_Started = FALSE;
    gFirmwareDownload_Finished = FALSE;
	pAPPL_FoeRead = FoE_Read;
	pAPPL_FoeWriteData = FoE_WriteData;
	pAPPL_FoeReadData = FoE_ReadData;
	pAPPL_FoeWrite = FoE_Write;
	u32FileSize = 0;
}

#endif

#if (ETHERCAT_EEPROM_EMULATION_SUPPORT == true)

/* Beckhoff Hw abstraction layer interface API to write new configuration parameters to slave EEPROM area */
UINT16 HW_EepromWrite(UINT32 wordaddr)
{
  
  UINT16 EEPROMReg = 0U; /* Regvalue 0x502 - 0x5003 */
  uint8_t   checksum = 0;
  UINT8    readdata[ESC_EEPROM_SIZE];
   
  if ((wordaddr <= (uint32_t)ESC_EEPROM_SIZE) && (pEEPROM != NULL))
  {
      UINT8 *pData = (UINT8 *)pEEPROM;
     
	  MEMCPY(&readdata[0],pData, ESC_EEPROM_SIZE);
      //update to buffer - ESC eeprom configuration value from the master
      HW_EscRead((MEM_ADDR *)&readdata[8], (uint16_t)ESC_EEPROM_DATA_OFFSET, EEPROM_WRITE_SIZE);
      pData = readdata;
	  
     if((wordaddr == (uint32_t)4U))
     {
        /*lets program the complete EEPROM new prepare Data for EEPROM */
        HW_EscReadWord(EEPROMReg,ESC_EEPROM_CONTROL_OFFSET);
        EEPROMReg = EEPROMReg & (~ESC_EEPROM_BUSY_MASK);
        HW_EscWriteWord(EEPROMReg,ESC_EEPROM_CONTROL_OFFSET);

        DISABLE_ESC_INT();

        //calculate checksum (Byte 7) needed by SII EEPROM Contents 
        checksum = CalculateCRC8((UINT8 *)pData, ESC_EEPROM_CONFIG_BYTES);
        
        /* Program the RAM contents to Emulated EEPROM*/
        APP_FlashEEPROMUpdate((UINT8 *)pData, checksum);
        ENABLE_ESC_INT();
        
     }
     
  }
  return 0;
}

/* API to reload slave EEPROM area */
UINT16 HW_EepromReload(void)
{
    UINT16 EEPROMReg = 0; /* Register0x502 - 0x503 */
    UINT32 cmd = (uint32_t)EEPROMReg & (uint32_t)ESC_EEPROM_CMD_MASK;
    UINT32 addr;
  
    HW_EscReadDWord(addr,(uint32_t)ESC_EEPROM_ADDRESS_OFFSET);
    addr = SWAPDWORD(addr);

    /* EEPROM emulation ( 8Bytes EEPROM data) */
    if ((addr <= (uint32_t)ESC_EEPROM_SIZE) && (pEEPROM != NULL))
    {
        UINT16 *pData = (UINT16 *)(void*)pEEPROM;
        do
        {
            HW_EscWrite((MEM_ADDR *)(void*)&pData[(addr)], (uint16_t)ESC_EEPROM_DATA_OFFSET, EEPROM_READ_SIZE);

            /*Clear error bits */
            EEPROMReg &= (uint16_t)(~(uint16_t)(ESC_EEPROM_ERROR_MASK));

            /*Ack current reload segment */
            HW_EscWriteWord(EEPROMReg,(uint32_t)ESC_EEPROM_CONTROL_OFFSET);

            /* Read EEPROM control (To check if the reload is still pending) */
            HW_EscReadWord(EEPROMReg,ESC_EEPROM_CONTROL_OFFSET);
            cmd = (uint32_t)EEPROMReg  & (uint32_t)ESC_EEPROM_CMD_MASK;

            HW_EscReadDWord(addr,(uint32_t)ESC_EEPROM_ADDRESS_OFFSET);

        } while(cmd == (uint32_t)ESC_EEPROM_CMD_RELOAD_MASK);
  }
  else
  {
        /* Set Error  */
        EEPROMReg |= (uint16_t)ESC_EEPROM_ERROR_CMD_ACK;
  }
  return 0;
}

void Emulation_Init()
{
   pAPPL_EEPROM_Reload = HW_EepromReload;
   pAPPL_EEPROM_Write  = HW_EepromWrite;
}
#else
UINT16 HW_EepromReload(void)
{
    return 0;
}
#endif


/////////////////////////////////////////////////////////////////////////////////////////
/**
 \brief    The function is called when an error state was acknowledged by the master

*////////////////////////////////////////////////////////////////////////////////////////

void    APPL_AckErrorInd(UINT16 stateTrans)
{

}

/////////////////////////////////////////////////////////////////////////////////////////
/**
 \return    AL Status Code (see ecatslv.h ALSTATUSCODE_....)

 \brief    The function is called in the state transition from INIT to PREOP when
             all general settings were checked to start the mailbox handler. This function
             informs the application about the state transition, the application can refuse
             the state transition when returning an AL Status error code.
            The return code NOERROR_INWORK can be used, if the application cannot confirm
            the state transition immediately, in that case this function will be called cyclically
            until a value unequal NOERROR_INWORK is returned

*////////////////////////////////////////////////////////////////////////////////////////

UINT16 APPL_StartMailboxHandler(void)
{
    return ALSTATUSCODE_NOERROR;
}

/////////////////////////////////////////////////////////////////////////////////////////
/**
 \return     0, NOERROR_INWORK

 \brief    The function is called in the state transition from PREEOP to INIT
             to stop the mailbox handler. This functions informs the application
             about the state transition, the application cannot refuse
             the state transition.

*////////////////////////////////////////////////////////////////////////////////////////

UINT16 APPL_StopMailboxHandler(void)
{
#if defined(ETHERCAT_USE_FOE)
	APP_FW_StateBOOTtoINIT();
#endif    
    return ALSTATUSCODE_NOERROR;
}

/////////////////////////////////////////////////////////////////////////////////////////
/**
 \param    pIntMask    pointer to the AL Event Mask which will be written to the AL event Mask
                       register (0x204) when this function is succeeded. The event mask can be adapted
                       in this function
 \return    AL Status Code (see ecatslv.h ALSTATUSCODE_....)

 \brief    The function is called in the state transition from PREOP to SAFEOP when
           all general settings were checked to start the input handler. This function
           informs the application about the state transition, the application can refuse
           the state transition when returning an AL Status error code.
           The return code NOERROR_INWORK can be used, if the application cannot confirm
           the state transition immediately, in that case the application need to be complete 
           the transition by calling ECAT_StateChange.
*////////////////////////////////////////////////////////////////////////////////////////

UINT16 APPL_StartInputHandler(UINT16 *pIntMask)
{
    return ALSTATUSCODE_NOERROR;
}

/////////////////////////////////////////////////////////////////////////////////////////
/**
 \return     0, NOERROR_INWORK

 \brief    The function is called in the state transition from SAFEOP to PREEOP
             to stop the input handler. This functions informs the application
             about the state transition, the application cannot refuse
             the state transition.

*////////////////////////////////////////////////////////////////////////////////////////

UINT16 APPL_StopInputHandler(void)
{
    return ALSTATUSCODE_NOERROR;
}

/////////////////////////////////////////////////////////////////////////////////////////
/**
 \return    AL Status Code (see ecatslv.h ALSTATUSCODE_....)

 \brief    The function is called in the state transition from SAFEOP to OP when
             all general settings were checked to start the output handler. This function
             informs the application about the state transition, the application can refuse
             the state transition when returning an AL Status error code.
           The return code NOERROR_INWORK can be used, if the application cannot confirm
           the state transition immediately, in that case the application need to be complete 
           the transition by calling ECAT_StateChange.
*////////////////////////////////////////////////////////////////////////////////////////

UINT16 APPL_StartOutputHandler(void)
{
    return ALSTATUSCODE_NOERROR;
}

/////////////////////////////////////////////////////////////////////////////////////////
/**
 \return     0, NOERROR_INWORK

 \brief    The function is called in the state transition from OP to SAFEOP
             to stop the output handler. This functions informs the application
             about the state transition, the application cannot refuse
             the state transition.

*////////////////////////////////////////////////////////////////////////////////////////

UINT16 APPL_StopOutputHandler(void)
{
    return ALSTATUSCODE_NOERROR;
}

/////////////////////////////////////////////////////////////////////////////////////////
/**
\return     0(ALSTATUSCODE_NOERROR), NOERROR_INWORK
\param      pInputSize  pointer to save the input process data length
\param      pOutputSize  pointer to save the output process data length

\brief    This function calculates the process data sizes from the actual SM-PDO-Assign
            and PDO mapping
*////////////////////////////////////////////////////////////////////////////////////////
UINT16 APPL_GenerateMapping(UINT16 *pInputSize,UINT16 *pOutputSize)
{
    UINT16 result = ALSTATUSCODE_NOERROR;
    UINT16 InputSize = 0;
    UINT16 OutputSize = 0;

#if COE_SUPPORTED
    UINT16 PDOAssignEntryCnt = 0;
    OBJCONST TOBJECT OBJMEM * pPDO = NULL;
    UINT16 PDOSubindex0 = 0;
    UINT32 *pPDOEntry = NULL;
    UINT16 PDOEntryCnt = 0;
   
#if MAX_PD_OUTPUT_SIZE > 0
    /*Scan object 0x1C12 RXPDO assign*/
    for(PDOAssignEntryCnt = 0; PDOAssignEntryCnt < sRxPDOassign.u16SubIndex0; PDOAssignEntryCnt++)
    {
        pPDO = OBJ_GetObjectHandle(sRxPDOassign.aEntries[PDOAssignEntryCnt]);
        if(pPDO != NULL)
        {
            PDOSubindex0 = *((UINT16 *)pPDO->pVarPtr);
            for(PDOEntryCnt = 0; PDOEntryCnt < PDOSubindex0; PDOEntryCnt++)
            {
                pPDOEntry = (UINT32 *)(((UINT16 *)pPDO->pVarPtr) + (OBJ_GetEntryOffset((PDOEntryCnt+1),pPDO)>>4));    //goto PDO entry
                // we increment the expected output size depending on the mapped Entry
                OutputSize += (UINT16) ((*pPDOEntry) & 0xFF);
            }
        }
        else
        {
            /*assigned PDO was not found in object dictionary. return invalid mapping*/
            OutputSize = 0;
            result = ALSTATUSCODE_INVALIDOUTPUTMAPPING;
            break;
        }
    }

    OutputSize = (OutputSize + 7) >> 3;
#endif

#if MAX_PD_INPUT_SIZE > 0
    if(result == 0)
    {
        /*Scan Object 0x1C13 TXPDO assign*/
        for(PDOAssignEntryCnt = 0; PDOAssignEntryCnt < sTxPDOassign.u16SubIndex0; PDOAssignEntryCnt++)
        {
            pPDO = OBJ_GetObjectHandle(sTxPDOassign.aEntries[PDOAssignEntryCnt]);
            if(pPDO != NULL)
            {
                PDOSubindex0 = *((UINT16 *)pPDO->pVarPtr);
                for(PDOEntryCnt = 0; PDOEntryCnt < PDOSubindex0; PDOEntryCnt++)
                {
                    pPDOEntry = (UINT32 *)(((UINT16 *)pPDO->pVarPtr) + (OBJ_GetEntryOffset((PDOEntryCnt+1),pPDO)>>4));    //goto PDO entry
                    // we increment the expected output size depending on the mapped Entry
                    InputSize += (UINT16) ((*pPDOEntry) & 0xFF);
                }
            }
            else
            {
                /*assigned PDO was not found in object dictionary. return invalid mapping*/
                InputSize = 0;
                result = ALSTATUSCODE_INVALIDINPUTMAPPING;
                break;
            }
        }
    }
    InputSize = (InputSize + 7) >> 3;
#endif

#else
#if _WIN32
   #pragma message ("Warning: Define 'InputSize' and 'OutputSize'.")
#else
    #warning "Define 'InputSize' and 'OutputSize'."
#endif
#endif

    *pInputSize = InputSize;
    *pOutputSize = OutputSize;
    return result;
}

/////////////////////////////////////////////////////////////////////////////////////////
/**
\param      pData  pointer to input process data

\brief      This function will copies the inputs from the local memory to the ESC memory
            to the hardware
*////////////////////////////////////////////////////////////////////////////////////////
void APPL_InputMapping(UINT16* pData)
{
	UINT32 *pTemp = (UINT32*)pData;
	*pTemp = Inputs0x6000.Counter;
}

/////////////////////////////////////////////////////////////////////////////////////////
/**
\param      pData  pointer to output process data

\brief    This function will copies the outputs from the ESC memory to the local memory
            to the hardware
*////////////////////////////////////////////////////////////////////////////////////////
void APPL_OutputMapping(UINT16* pData)
{
	UINT32 *pTemp = (UINT32*)pData;
	Outputs0x7010.Trigger = *pTemp;
}

/////////////////////////////////////////////////////////////////////////////////////////
/**
\brief    This function will called from the synchronisation ISR 
            or from the mainloop if no synchronisation is supported
*////////////////////////////////////////////////////////////////////////////////////////
void APPL_Application(void)
{
    
    if(Outputs0x7010.Trigger==1)
    {
     PORT_PinWrite(PIN_PB04,0);
      PORT_PinWrite(PIN_PB05,0);
       PORT_PinWrite(PIN_PB06,0);
        PORT_PinWrite(PIN_PB07,0);
        
            SERCOM0_USART_Write("triggered",9);
           Inputs0x6000.Counter = 50;
     }
    else
    {
           SERCOM0_USART_Write("not triggered",13);
     PORT_PinWrite(PIN_PB04,1);
      PORT_PinWrite(PIN_PB05,1);
       PORT_PinWrite(PIN_PB06,1);
        PORT_PinWrite(PIN_PB07,1);
         Inputs0x6000.Counter = 100;
    }
	if(Outputs0x7010.Trigger)
	{
        gTriggerCounterValMeasure += Outputs0x7010.Trigger;
        if(gTriggerCounterValMeasure == gFOETestFrameSize)
        {
         //   Inputs0x6000.Counter += Outputs0x7010.Trigger;
            gTriggerCounterValMeasure = 0;
        }
        
        
	} else
	{
		Inputs0x6000.Counter = 0;
        gTriggerCounterValMeasure = 0;
        PORT_PinWrite(PIN_PB04,1);
	}
}

#if EXPLICIT_DEVICE_ID
/////////////////////////////////////////////////////////////////////////////////////////
/**
 \return    The Explicit Device ID of the EtherCAT slave

 \brief     Read the Explicit Device ID (from an external ID switch)
*////////////////////////////////////////////////////////////////////////////////////////
UINT16 APPL_GetDeviceID()
{
#if _WIN32
   #pragma message ("Warning: Implement explicit Device ID latching")
#else
    #warning "Implement explicit Device ID latching"
#endif
    /* Explicit Device 5 is expected by Explicit Device ID conformance tests*/
    return 0x5;
}
#endif



#if USE_DEFAULT_MAIN
/////////////////////////////////////////////////////////////////////////////////////////
/**

 \brief    This is the main function

*////////////////////////////////////////////////////////////////////////////////////////
#if _PIC24 && EL9800_HW
int main(void)
#elif _WIN32
int main(int argc, char* argv[])
#else
void main(void)
#endif
{
    /* initialize the Hardware and the EtherCAT Slave Controller */
#if FC1100_HW
#if _WIN32
    u16FcInstance = 0;
    if (argc > 1)
    {
        u16FcInstance = atoi(argv[1]);
    }
#endif
    if(HW_Init())
    {
        HW_Release();
        return;
    }
#else
    HW_Init();
#endif
    MainInit();

    bRunApplication = TRUE;
    do
    {
        MainLoop();
        
    } while (bRunApplication == TRUE);

    HW_Release();
#if _PIC24
    return 0;
#endif
}
#endif //#if USE_DEFAULT_MAIN
/** @} */



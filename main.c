/*******************************************************************************
  Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This file contains the "main" function for a project.

  Description:
    This file contains the "main" function for a project.  The
    "main" function calls the "SYS_Initialize" function to initialize the state
    machines of all modules in the system
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include "definitions.h"                // SYS function prototypes
#include  "app_lan9255.h"

extern UINT32  gTriggerCounterValMeasure;
// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************

int main ( void )
{
    /* Initialize all modules */
    SYS_Initialize ( NULL );
    PORT_PinWrite(PIN_PB04,0);
    SYSTICK_TimerStart();
    APP_Initialize();
   
    
    while ( true )
    {
        /* Maintain state machines of all polled MPLAB Harmony modules. */
        
     //   PORT_PinToggle(PIN_PB04);
        
        PORT_PinWrite(PIN_PB04,0);
//        / SYSTICK_DelayMs(500); 
//         PORT_PinWrite(PIN_PB04,1);
//          SYSTICK_DelayMs(500);
                
                
        SERCOM0_USART_Write("Welcome",7);
        SYSTICK_DelayMs(100);
        APP_Tasks ( );
       
    }

    
    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}


/*******************************************************************************
 End of File
*/


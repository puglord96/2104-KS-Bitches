
//#include "driverlib.h"
#include <msp432p401r.h>

/* Standard Includes */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

volatile unsigned int adcValue;
volatile float calValue;


void delay(uint32_t time){
    uint32_t count = 0;                      // Simple counter variable

    for(count = 0; count < time; count++)   // Busy Loop for Delay
          {

          }
}

void main(void){
    volatile unsigned int i;
    WDTCTL = WDTPW+WDTHOLD; //Stop Watchdog Timer


    //------  Setup MSP432 Clock ----------
    CS->KEY = CS_KEY_VAL;                   // Unlock CS module for register access
    CS->CTL0 = 0;                           // Reset tuning parameters
    CS->CTL0 = CS_CTL0_DCORSEL_3;           // Set DCO to 12MHz (nominal, center of 8-16MHz range)
    CS->CTL1 = CS_CTL1_SELA_2 |             // Select ACLK = REFO
               CS_CTL1_SELS_3 |             // SMCLK = DCO
               CS_CTL1_SELM_3;              // MCLK = DCO
    CS->KEY = 0;                            // Lock CS module from unintended accesses

    //------  Setup MSP432 UART ----------
    // Configure UART pins
    P1->SEL0 |= BIT2 | BIT3;                // set 2-UART pin as secondary function

    // Configure UART
    EUSCI_A0->CTLW0 |= EUSCI_A_CTLW0_SWRST;      // Put eUSCI in reset
    EUSCI_A0->CTLW0 = EUSCI_A_CTLW0_SWRST |      // Remain eUSCI in reset
                      EUSCI_B_CTLW0_SSEL__SMCLK; // Configure eUSCI clock source for SMCLK

    // 9600 Baud Rate calculation -> 12000000/(16*9600) = 78.125
    // Fractional portion = 0.125 -> User's Guide Table 21-4: UCBRSx = 0x10 ;UCBRFx = (78.125-78)*16 = 2
    EUSCI_A0->BRW = 78;
    EUSCI_A0->MCTLW = (2 << EUSCI_A_MCTLW_BRF_OFS) |
            EUSCI_A_MCTLW_OS16;

    EUSCI_A0->CTLW0 &= ~EUSCI_A_CTLW0_SWRST; // Initialize eUSCI


//ADC for Soil Moisture Module----------------------------------------------------------------------
    ADC14->CTL0 = ADC14_CTL0_ON | ADC14_CTL0_SHT0_2 | ADC14_CTL0_SHP;
    ADC14->CTL1 = ADC14_CTL1_RES_1;         // Use sampling timer, 10-bit conversion results

    ADC14->MCTL[0] = ADC14_MCTLN_VRSEL_0 |  ADC14_MCTLN_INCH_0; //Using Channel A0 (P5.5) for ADC input select; Vref = AVCC
    ADC14->IER0 |= ADC14_IER0_IE0; // Enable ADC plus interrupt

    P5->SEL1 |= BIT5; //Configure P5.5 for ADC
    P5->SEL0 |= BIT5; //Configure P5.5 for ADC


    __enable_irq(); //Enable global Interrupts

    // Enable ADC interrupt in NVIC module
    NVIC->ISER[0] = 1 << ((ADC14_IRQn) & 31);


    SCB->SCR &= ~SCB_SCR_SLEEPONEXIT_Msk;   // Wake up on exit from ISR

    // Ensures SLEEPONEXIT takes effect immediately
    __DSB();

    while(1){
        for (i = 20000; i > 0; i--);        // Delay

        //Start Sampling/conversion
        ADC14->CTL0 |= ADC14_CTL0_ENC | ADC14_CTL0_SC;

        __sleep();

        __no_operation();                   // For debugger
    }

//--------------------------------------------------------------------------------------

}

// ADC14 interrupt service routine
void UARTSend(char * TxArray)
{
    delay(100000); //Delay display of results

    unsigned short i = 0;
    size_t len = strlen(TxArray); //Getting length of string to be sent via UART

    //Sending String to serial monitor via UART char by char
    while(len--)
    { // Loop until StringLength == 0 and post decrement
        while(!(EUSCI_A0->IFG & EUSCI_A_IFG_TXIFG));// Wait for TX buffer to be ready for new data
        EUSCI_A0->TXBUF = *(TxArray+i);             //Write the character at the location specified py the pointer
        i++;                                        //Increment the TxString pointer to point to the next character
    }
}

void ADC14_IRQHandler(void) {
    if (ADC14->MEM[0]){
        adcValue = ADC14->MEM[0]; //Value from ADC register
        calValue = 100 - ((adcValue/1023.00)*100); //Calculated value by taking ADC register value divide by 1023( 2^10 - 1 cos 10 bits ADC)

        char text[50]; //Buffer to store result as string to be sent over to serial monitor via UART
        char strVal[10];

        sprintf(strVal,"%0.2f",calValue); //Convert Result into string and store into text

        strcpy(text,"Soil Moisture: ");
        strcat(text,strVal);
        strcat(text,"%\n\r");

        //Sending value over via UART
        UARTSend(text);
        UARTSend("----------------\n\r");

    }

}



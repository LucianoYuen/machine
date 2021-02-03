#include "mcc_generated_files/mcc.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <xc.h>

uint8_t EEPROM_Buffer[5] =
{
     0x00,0x01,0x02,0x03,0x04
};

uint8_t dataAddressByte = 0;

volatile uint8_t eepromAddress = 0;

uint8_t Address[2]={5,5};

void flashled(void);
void I2C2_ISR(void);
void main(void)
{
    // Initialize the device
    SYSTEM_Initialize();
    //__delay_ms(2000);
    // Enable the Global Interrupts
    INTERRUPT_GlobalInterruptEnable();

    // Disable the Global Interrupts
    //INTERRUPT_GlobalInterruptDisable();
    
    while (1)
    {
    flashled();
    }
    //__delay_ms(4000);
}
/**
 End of File
*/  

void flashled(void)
{
    LED1_SetHigh();
    DELAY_milliseconds(100);
    LED1_SetLow();
    LED2_SetHigh();
    DELAY_milliseconds(100);
    LED2_SetLow();
    LED3_SetHigh();
    DELAY_milliseconds(100);
    LED3_SetLow();
    LED4_SetHigh();
    DELAY_milliseconds(100);
    LED4_SetLow();
}



void I2C1_Initialize(void)
{
    I2C1ADR0 = 0x40;
    I2C1ADR1 = 0x40;
    I2C1ADR2 = 0x40;
    I2C1ADR3 = 0x40;

    I2C1CON0 = 0x00;
    I2C1CON1 = 0x80;

    I2C1CON2 = 0x00;

    I2C1CNTL = 0x00;
    I2C1CNTH = 0x00;

    PIR7bits.I2C1RXIF = 0;
    PIR7bits.I2C1TXIF = 0;
    PIR7bits.I2C1EIF = 0;
    I2C1ERRbits.NACKIF = 0;
    PIR7bits.I2C1IF = 0;
    I2C1PIRbits.PCIF = 0;
    I2C1PIRbits.ADRIF = 0;


    PIE7bits.I2C1RXIE = 1;
    PIE7bits.I2C1TXIE = 1;
    PIE7bits.I2C1EIE = 1;
    I2C1ERRbits.NACKIE = 1;
    PIE7bits.I2C1IE = 1;
    I2C1PIEbits.PCIE = 1;
    I2C1PIEbits.ADRIE = 1;

    I2C1PIR = 0x00;
    I2C1ERR = 0x00;

    I2C1CON0bits.EN = 1; 
    
}

void I2C2_ISR(void)
{
    if((PIR7bits.I2C1IF == 1) || (PIR7bits.I2C1RXIF == 1) || (PIR7bits.I2C1TXIF == 1))
    {
        if(I2C1STAT0bits.SMA == 1)
        {    
            if(I2C1STAT0bits.R == 0)
            {
                if((I2C1PIRbits.ADRIF == 1) && (I2C1STAT0bits.D == 0)) //ADDR //Slave Read ADDR //Master Write ADDR
                {
                I2C1PIRbits.ADRIF = 0;
                I2C1PIRbits.SCIF = 0;
                I2C1PIRbits.WRIF = 0;
                I2C1STAT1bits.CLRBF = 1;
                I2C1CON0bits.CSTR = 0;
                //I2C1PIRbits.PCIF = 0; //TEST
                } 
                if((PIR7bits.I2C1RXIF == 1) && (I2C1STAT0bits.D == 1))
                {
                    if(dataAddressByte == 0)
                    {
                    eepromAddress = I2C1RXB;
                    I2C1PIRbits.WRIF = 0;
                    dataAddressByte = 1;
                    }
                    else
                    {
                    while(I2C1STAT1bits.RXBF == 0);
                    EEPROM_Buffer[eepromAddress] = I2C1RXB;
                    I2C1PIRbits.WRIF = 0;
                    if(EEPROM_Buffer[1] == 0x10)
                    LED4_SetHigh();
                    __delay_ms(500);
                    LED4_SetLow();
                    }
                    I2C1CON1bits.ACKCNT = 0;
                }
            I2C1CON0bits.CSTR = 0;    
            }            
        }        
        else
        {
        if(I2C1PIRbits.PCIF)
                {
                I2C1PIRbits.PCIF = 0;
                I2C1PIRbits.SCIF = 0;
                I2C1PIRbits.CNTIF = 0;
                I2C1PIRbits.WRIF = 0;
                I2C1STAT1bits.CLRBF = 1;
                I2C1CNTL = 0;
                dataAddressByte = 0;
                eepromAddress = 0;  
                } 
        }
     if(PIR7bits.I2C1EIF == 1)
        {
            if(I2C1ERRbits.NACKIF)
            {
            I2C1ERRbits.NACKIF=0;
            I2C1STAT1bits.CLRBF=1;
            dataAddressByte = 0;
            eepromAddress = 0;       
            }     
        }    
    }   
}



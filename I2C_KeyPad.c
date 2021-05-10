#include "mcc_generated_files/mcc.h"
//#include <string.h>
//#include <avr/io.h>
#include <util/delay.h>

#define COLUMN_gm (PIN0_bm | PIN1_bm | PIN2_bm | PIN3_bm)

volatile uint8_t key_pressed;
volatile uint8_t key;

uint8_t buf[12];
int16_t val;
  
void row_output_column_input();
void row_input_column_output();
void scan_keys();

void requestEvent();

void system_led(void);

#define LOCATION_I2C_ADDRESS 0x01 //Location in EEPROM where the I2C address is stored
#define I2C_ADDRESS_DEFAULT 75
#define I2C_ADDRESS_JUMPER 74

#define BUTTON_STACK_SIZE 15

struct {
  uint8_t button; //Which button was pressed?
  //unsigned long buttonTime; //When?
} buttonEvents[BUTTON_STACK_SIZE];

volatile uint8_t newestPress = 0;
volatile uint8_t oldestPress = 0;

void loadFifoRegister();
void requestEvent();

struct memoryMap {
  uint8_t id;                    // Reg: 0x00 - Default I2C Address
  uint8_t fifo_button;           // Reg: 0x01 - oldest button (aka the "first" button pressed)
  uint8_t updateFIFO;            // Reg: 0x02 - "command" from master, set bit0 to command fifo increment
  uint8_t i2cAddress;            // Reg: 0x03 - Set I2C New Address (re-writable).
};

//These are the default values for all settings
volatile struct memoryMap registerMap = {
  .id = I2C_ADDRESS_DEFAULT, //Default I2C Address (0x20)
  .fifo_button = 0,
  .updateFIFO = 0,
  .i2cAddress = I2C_ADDRESS_DEFAULT,
};

//This defines which of the registers are read-only (0) vs read-write (1)
struct memoryMap protectionMap = {
  .id = 0x00,
  .fifo_button = 0x00,
  .updateFIFO = 0x01, // allow read-write on bit0 to "command" fifo increment update
  .i2cAddress = 0xFF,
};

static volatile uint8_t* rxRegisters;
static volatile uint8_t* txRegisters;

static uint8_t	rxRegSize;
static uint8_t	txRegSize;

static volatile uint8_t	rxRegIndex;
static volatile uint8_t	txRegIndex;

volatile bool setRegIndex = false;

//Cast 32bit address of the object registerMap with uint8_t so we can increment the pointer
uint8_t *registerPointer = (uint8_t *)&registerMap;
uint8_t *protectionPointer = (uint8_t *)&protectionMap;

volatile uint8_t registerNumber; //Gets set when user writes an address. We then serve the spot the user requested.

int main(void)
{
    /* Initializes MCU, drivers and middleware */
    SYSTEM_Initialize();
    
    /* initialize pins connected to the rows and columns of keypad */
	PORTA.OUTCLR = PIN2_bm | PIN1_bm;
	PORTB.OUTCLR = PIN2_bm | PIN3_bm;
	PORTC.OUTCLR = PIN0_bm | PIN1_bm | PIN2_bm | PIN3_bm;

   row_output_column_input();

	while (1) {
		/* Poll for low input values for the columns */
		if ((PORTC.IN & COLUMN_gm) != COLUMN_gm) {
			/* Debounce */
			_delay_ms(10);
			/* If the button is still pressed after 10 ms, the press is registered as valid */
			if ((PORTC.IN & COLUMN_gm) != COLUMN_gm) {
				scan_keys();
                if(key_pressed){
                    PORTB_set_pin_level(5, true);    
                    DELAY_milliseconds(100);    
                    PORTB_set_pin_level(5, false);}                   
				/* Wait for all buttons to be released */
				while ((PORTC.IN & COLUMN_gm) != COLUMN_gm);
			}
            
        if (key_pressed)
        {
        if (oldestPress == (newestPress + 1)) oldestPress++;
        if ( (newestPress == (BUTTON_STACK_SIZE - 1)) && (oldestPress == 0) ) oldestPress++;
        if (oldestPress == BUTTON_STACK_SIZE) oldestPress = 0;
        newestPress++;
        if (newestPress == BUTTON_STACK_SIZE) newestPress = 0;
        buttonEvents[newestPress].button = key_pressed;
        }
        //loadFifoRegister(); 
        }
	} 
}

void row_output_column_input()
{
	/* Set rows to output */
	PORTA.DIRSET = PIN2_bm | PIN1_bm;
	PORTB.DIRSET = PIN2_bm | PIN3_bm;

	/* Set columns to input */
	PORTC.DIRCLR = PIN0_bm | PIN1_bm | PIN2_bm | PIN3_bm;
}

void row_input_column_output()
{
	/* Set rows to input */
	PORTA.DIRCLR = PIN2_bm | PIN1_bm;
	PORTB.DIRCLR = PIN2_bm | PIN3_bm;

	/* Set columns to output */
	PORTC.DIRSET = PIN0_bm | PIN1_bm | PIN2_bm | PIN3_bm;
}

void scan_keys()
{
	key_pressed = 0;

	/* If PC3 (COLUMN 0) is pulled low */
	if (!(PORTC.IN & PIN0_bm)) {
		key_pressed = 1;
	}
	/* If PC0 (COLUMN 1) is pulled low */
	else if (!(PORTC.IN & PIN1_bm)) {
		key_pressed = 2;
	}
	/* If PC1 (COLUMN 2) is pulled low */
	else if (!(PORTC.IN & PIN2_bm)) {
		key_pressed = 3;
	}
	/* If PC2 (COLUMN 3) is pulled low */
	else if (!(PORTC.IN & PIN3_bm)) {
		key_pressed = 4;
	}

	row_input_column_output();

	/* If PB0 (ROW 0) is pulled low */
	if (!(PORTB.IN & PIN2_bm)) {
		key_pressed += 0;
	}
	/* If PB1 (ROW 1) is pulled low */
	else if (!(PORTB.IN & PIN3_bm)) {
		key_pressed += 4;
	}
	/* If PA2 (ROW 2) is pulled low */
	else if (!(PORTA.IN & PIN2_bm)) {
		key_pressed += 8;
	}
	/* If PA1 (ROW 3) is pulled low */
	else if (!(PORTA.IN & PIN1_bm)) {
		key_pressed += 12;
	}

	row_output_column_input();
}

void requestEvent()
{
  if (registerMap.updateFIFO & (1 << 0))
  {
    // clear command bit
    registerMap.updateFIFO &= ~(1 << 0);

    // update fifo, that is... copy oldest button (and buttonTime) into fifo register (ready for reading)
    loadFifoRegister();
  }
    //Wire.write((registerPointer + registerNumber), sizeof(memoryMap) - registerNumber);
}

//Take the FIFO button press off the stack and load it into the fifo register (ready for reading)
void loadFifoRegister()
{
  if (oldestPress != newestPress)
  {
    oldestPress++;
    if (oldestPress == BUTTON_STACK_SIZE) oldestPress = 0;

    registerMap.fifo_button = buttonEvents[oldestPress].button;
  }
  else
  {
    //No new button presses. load blank records
    registerMap.fifo_button = 0;
  }
}

void system_led(void)
{
    PORTB_set_pin_level(4, true);    
    DELAY_milliseconds(100);    
    PORTB_set_pin_level(4, false);
    DELAY_milliseconds(100);
}

ISR(TWI0_TWIS_vect)
{
    if( TWI0_SSTATUS & (TWI_COLL_bm | TWI_BUSERR_bm) ) {
		TWI0_SCTRLB = TWI_SCMD_COMPTRANS_gc;
	}
    
    if ((TWI0.SSTATUS & TWI_APIF_bm) && (TWI0.SSTATUS & TWI_AP_bm)) { //I2C0_SlaveIsAddressInterrupt()
        if (TWI0.SSTATUS & TWI_DIR_bm) {
        TWI0_SCTRLB = TWI_SCMD_RESPONSE_gc;
        }
        else {
        setRegIndex = true;    
        TWI0_SCTRLB = TWI_SCMD_RESPONSE_gc;
        }
    }
    
    if (TWI0.SSTATUS & TWI_DIF_bm) { //I2C0_SlaveIsDataInterrupt()
        if (TWI0.SSTATUS & TWI_DIR_bm) {
            if( TWI0_SSTATUS & TWI_RXACK_bm ) {//I2C0_SlaveDIR()   ---(R)---
                //if( txRegIndex < txRegSize ) {
				//TWI0.SDATA = txRegisters[txRegIndex++];		// load data and advance index.
                loadFifoRegister();
                TWI0.SDATA = registerMap.fifo_button;
                //}
                TWI0_SCTRLB = TWI_SCMD_RESPONSE_gc;
            }
            else {     
            	//if( txRegIndex < txRegSize ) {
				//TWI0.SDATA = txRegisters[txRegIndex++];		// load data and advance index.
                loadFifoRegister();
                TWI0.SDATA = registerMap.fifo_button;
                //}
                TWI0.SSTATUS |= (TWI_DIF_bm | TWI_APIF_bm); //I2C0_GotoUnaddressed()
                //TWI0.SCTRLB = TWI_SCMD_COMPTRANS_gc;
                TWI0_SCTRLB = TWI_SCMD_RESPONSE_gc;
                }   
        }

        else {
            if(setRegIndex) {
            txRegIndex = TWI0.SDATA;
            rxRegIndex = txRegIndex;
            setRegIndex = false;
            }
            //else {
            //    if( rxRegIndex < rxRegSize ) { 
            //    rxRegisters[rxRegIndex++] = TWI0.SDATA;
            //    }
            //     } 
            TWI0_SCTRLB = TWI_SCMD_RESPONSE_gc;
             }     
    }
    else {
        TWI0.SCTRLB = TWI_SCMD_COMPTRANS_gc; // To check the status of the transaction
        return;
    }
}

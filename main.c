#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#define KEYBOARD 0xFF200100
#define TIMER 0xFFFEC600
#define LEDR 0xFF200000
#define KEYS 0xFF200050
#define SWITCHES 0xFF200040
#define HEX 0xFF200020

void disable_A9_interrupts (void);
void set_A9_IRQ_stack (void);
void config_GIC (void);
void config_Keyboard (void);
void config_Timer (void);
void enable_A9_interrupts (void);

void initPlayers(void);
void mainMenu(void);
void newRound(void);
void scoreboard(void);

/************************************************************************
 *               1v1 Reaction Speed Game
 *  This game challenges your reaction speed against an opponent. Press
 *  the corresponding WASD or arrow keys on the monitor before the timer
 *  runs out. Get bonus points if you can get it faster than your opponent.
 *
 * This program makes use of interrupts and I/O devices such as vga monitor,
 * ps/2 keyboard, switches and keys on the DE1-SOC board
 *
************************************************************************/
unsigned char byte1 = 0, byte2 = 0, byte3 = 0; //Stores the three most recent bytes sent by the ps/2 keyboard
int difficulties[10] = {3000000, 5000000, 10000000, 15000000, 20000000, 40000000, 50000000, 60000000, 70000000, 100000000};
int difficulty, rounds;
volatile int ticks;

struct player {
    bool fail;
    bool success;
    int points;
    char keys[4];
    char targetKey, pressedKey;
} p1, p2;

int main(void)
{

    disable_A9_interrupts (); // disable interrupts in the A9 processor
    set_A9_IRQ_stack (); // initialize the stack pointer for IRQ mode
    config_GIC (); // configure the general interrupt controller
    config_Keyboard (); // configure Ps/2 Keyboard to generate interrupts
    //config_Timer(); //configure Private Timer to generate interrupts and loop every second
    enable_A9_interrupts (); // enable interrupts in the A9 processor

    time_t t;
    srand((unsigned) time(&t));

    initPlayers();
    mainMenu();
    config_Timer();
    while (1){
        printf("\nDifficulty: %d \n Rounds: %d", difficulty, rounds);
        //mainMenu();
        while(rounds >= 0) {
            newRound();
            scoreboard();
        }
    } // wait for an interrupt

}

void selectingDifficulty();
void selectingRounds();

//Displays and calls functions to select the number of rounds and starting difficulty
void mainMenu(){

    selectingDifficulty();
    selectingRounds();


}
//Selects the starting difficulty of the game using the switches and key 0
void selectingDifficulty(){
    int * keys_ptr = (int *) KEYS;
    volatile int switches, keys, leds;
    int i;

    while(1){
        keys = *(keys_ptr + 3);
        if(*(keys_ptr + 3) & 0x1){
            for(i = 9; i >= 0; i--){
                leds = *((int *) LEDR);
                if(leds >> i & 0b1){
                    difficulty = difficulties[9-i];
                    *(keys_ptr + 3) = keys;
                    return;
                }
            }
            *(keys_ptr + 3) = keys;
        }

        for(i = 9; i >= 0; i--){
            switches = *((int *) SWITCHES) & 0x3ff;
            if(((unsigned int) switches>> i) & 0b1){
                *((int *) LEDR) = 0b1111111111 >> 9-i;
                break;
            }
            else if(i == 0){
                *((int *) LEDR) = 0;
            }
        }
    }
}
//selects the number of rounds for the games using the switches and key0, key1 will bring you back to difficulty selection
void selectingRounds(){
    printf("");
    int * keys_ptr = (int *) KEYS;
    volatile int keys, switches, leds;
    int i;

    while(1){
        keys = *(keys_ptr + 3);
        if(keys & 0x1){
            for(i = 9; i >= 0; i--){
                leds = *((int *) LEDR);
                if(leds >> i & 0b1){
                    rounds = i;
                    *(keys_ptr + 3) = keys;
                    return;
                }
            }
            *(keys_ptr + 3) = keys;
        }

        if(keys & 0x2){
            selectingDifficulty();
        }

        for(i = 9; i >= 0; i--){
            switches = *((int *) SWITCHES) & 0x3ff;
            if(((unsigned int) switches>> i) & 0b1){
                *((int *) LEDR) = 0b1111111111 >> 9-i;
                break;
            }
            else if(i == 0){
                *((int *) LEDR) = 0;
            }
        }
    }
}

//New round is configured and played here
void newRound(){

    p1.targetKey = p1.keys[rand()%4];
    p2.targetKey = p2.keys[rand()%4];

    volatile int *HEX3_HEX0_ptr = (int *) HEX;
    int HEX_bits;

    *HEX3_HEX0_ptr = 0;

    if (p1.targetKey == 0x1D) { // W key
        HEX_bits = 0b110111;
        *HEX3_HEX0_ptr = HEX_bits;
    }

    else if (p1.targetKey == 0x1C) { //A key
        HEX_bits = 0b111001;
        *HEX3_HEX0_ptr = HEX_bits;
    }
    else if (p1.targetKey == 0x1B) { //S key
        HEX_bits = 0b111110;
        *HEX3_HEX0_ptr = HEX_bits;
    }
    else if (p1.targetKey == 0x23){ //D key
        HEX_bits = 0b1111;
        *HEX3_HEX0_ptr = HEX_bits;
    }

    ticks = 10;
    while(ticks > 0){
        if(!p1.fail && !p1.success && (p1.pressedKey == 0x1D || p1.pressedKey == 0x1C || p1.pressedKey == 0x1B || p1.pressedKey == 0x23)){
            if(p1.pressedKey == p1.targetKey){
                p1.success = true;
                *(HEX3_HEX0_ptr) |= 0b1111111 << 8;
            }
            else{
                p1.fail = true;
                *(HEX3_HEX0_ptr) = 0b0;
            }
        }

        if(!p2.fail && !p2.success && (p2.pressedKey == 0x75 || p2.pressedKey == 0x68 || p2.pressedKey == 0x72 || p2.pressedKey == 0x74)){
            if(p2.pressedKey == p2.targetKey){
                p2.success = true;
                //*(HEX3_HEX0_ptr + 1) = 0b1111111;
            }
            else{
                p2.fail = true;
            }
        }
    }

    rounds--;
    return;
}

void scoreboard(){
    clock_t startTime = clock();

    while(clock() < startTime +300){

    }
    p1.pressedKey = 0;
    p1.fail = false;
    p1.success = false;

    p2.pressedKey = 0;
    p2.fail = false;
    p2.success = false;

}
//Initializes player variables
void initPlayers(){
    p1.keys[0] = 0x1D;
    p1.keys[1] = 0x1C;
    p1.keys[2] = 0x1B;
    p1.keys[3] = 0x23;

    p1.fail = false;
    p1.success = false;
    p1.pressedKey = 0;

    p2.keys[0] = 0x75;
    p2.keys[1] = 0x68;
    p2.keys[2] = 0x72;
    p2.keys[3] = 0x74;

    p2.fail = false;
    p2.success = false;
    p2.pressedKey = 0;
}

/* setup the ps/2 keyboard interrupts in the FPGA */
void config_Keyboard()
{
    volatile int * keyboard_ptr = (int *) KEYBOARD; // keyboard controller 1 base address
    *(keyboard_ptr + 1) = 1; // enable interrupts for controller 1
}

// Setup the A9 Private timer interrupts in the FPGA and initialize
void config_Timer(){
    volatile int * timer_ptr = (int *) TIMER;
    *(timer_ptr) = difficulty;
    *(timer_ptr + 2) = 0b111; //CHANGE THIS LINE
    //*((int*) LEDR) = 0b11111111;
}

void keyboard_ISR (void);
void timer_ISR(void);

void config_interrupt (int, int);

// Define the IRQ exception handler
void __attribute__ ((interrupt)) __cs3_isr_irq (void)
{
    // Read the ICCIAR from the CPU Interface in the GIC
    int interrupt_ID = *((int *) 0xFFFEC10C);
    if (interrupt_ID == 79) // check if interrupt is from the keyboard
        keyboard_ISR ();
    else if(interrupt_ID == 29) //check if interrupt is from the timer
        timer_ISR();
    else
        while (1); // if unexpected, then stay here
    // Write to the End of Interrupt Register (ICCEOIR)
    *((int *) 0xFFFEC110) = interrupt_ID;
}

// Define the remaining exception handlers
void __attribute__ ((interrupt)) __cs3_reset (void)
{
    while(1);
}
void __attribute__ ((interrupt)) __cs3_isr_undef (void)
{
    while(1);
}
void __attribute__ ((interrupt)) __cs3_isr_swi (void)
{
    while(1);
}
void __attribute__ ((interrupt)) __cs3_isr_pabort (void)
{
    while(1);
}
void __attribute__ ((interrupt)) __cs3_isr_dabort (void)
{
    while(1);
}
void __attribute__ ((interrupt)) __cs3_isr_fiq (void)
{
    while(1);
}

/*
* Turn off interrupts in the ARM processor
*/
void disable_A9_interrupts(void)
{
    int status = 0b11010011;
    asm("msr cpsr, %[ps]" : : [ps]"r"(status));
}

/*
* Initialize the banked stack pointer register for IRQ mode
*/
void set_A9_IRQ_stack(void)
{
    int stack, mode;
    stack = 0xFFFFFFFF - 7; // top of A9 onchip memory, aligned to 8 bytes
    /* change processor to IRQ mode with interrupts disabled */
    mode = 0b11010010;
    asm("msr cpsr, %[ps]" : : [ps] "r" (mode));
    /* set banked stack pointer */
    asm("mov sp, %[ps]" : : [ps] "r" (stack));
    /* go back to SVC mode before executing subroutine return! */
    mode = 0b11010011;
    asm("msr cpsr, %[ps]" : : [ps] "r" (mode));
}
/*
* Turn on interrupts in the ARM processor
*/
void enable_A9_interrupts(void)
{
    int status = 0b01010011;
    asm("msr cpsr, %[ps]" : : [ps]"r"(status));
}
/*
* Configure the Generic Interrupt Controller (GIC)
*/
void config_GIC(void)
{
    config_interrupt (79, 1); // configure the PS/2 interrupt (ID: 73)
    config_interrupt (29, 1); // configure the A9 Private Timer interrupt (ID: 29)
    // Set Interrupt Priority Mask Register (ICCPMR). Enable all priorities
    *((int *) 0xFFFEC104) = 0xFFFF;
    // Set the enable in the CPU Interface Control Register (ICCICR)
    *((int *) 0xFFFEC100) = 1;
    // Set the enable in the Distributor Control Register (ICDDCR)
    *((int *) 0xFFFED000) = 1;
}
/*
* Configure registers in the GIC for an individual Interrupt ID. We
* configure only the Interrupt Set Enable Registers (ICDISERn) and
* Interrupt Processor Target Registers (ICDIPTRn). The default (reset)
* values are used for other registers in the GIC
*/
void config_interrupt (int N, int CPU_target)
{
    int reg_offset, index, value, address;
    /* Configure the Interrupt Set-Enable Registers (ICDISERn).
    * reg_offset = (integer_div(N / 32) * 4; value = 1 << (N mod 32) */
    reg_offset = (N >> 3) & 0xFFFFFFFC;
    index = N & 0x1F;
    value = 0x1 << index;
    address = 0xFFFED100 + reg_offset;
    /* Using the address and value, set the appropriate bit */
    *(int *)address |= value;
    /* Configure the Interrupt Processor Targets Register (ICDIPTRn)
    * reg_offset = integer_div(N / 4) * 4; index = N mod 4 */
    reg_offset = (N & 0xFFFFFFFC);
    index = N & 0x3;
    address = 0xFFFED800 + reg_offset + index;
    /* Using the address and value, write to (only) the appropriate byte */
    *(char *)address = (char) CPU_target;
}

/********************************************************************
* Keyboard - Interrupt Service Routine
*
* This routine checks which key has been pressed and sets the flag
*******************************************************************/
void keyboard_ISR( void )
{
    printf("\nkeyboard interrupt entered");
    /* KEY base address */
    volatile int *keyboard_ptr = (int *) KEYBOARD;
    /* HEX display base address */
    volatile int *HEX3_HEX0_ptr = (int *) HEX;
    int data, RVALID, HEX_bits;

    data = *(keyboard_ptr);
    RVALID = (data & 0x8000);

    if (RVALID != 0)
    {
        /* always save the last three bytes received */
        byte1 = byte2;
        byte2 = byte3;
        byte3 = data & 0xFF;
    }

    printf(" %hhx ", byte3);
    if (byte3 == 0x1D && byte2 != 0xF0) { // W key
        p1.pressedKey = 0x1D;
        printf(" W key entered");
    }

    if (byte3 == 0x1C && byte2 != 0xF0) { //A key
        p1.pressedKey = 0x1C;
        printf(" A key entered");
    }
    else if (byte3 == 0x1B && byte2 != 0xF0) { //S key
        p1.pressedKey = 0x1B;
        printf(" S key entered");
    }
    else if (byte3 == 0x23 && byte2 != 0xF0){ //D key
        p1.pressedKey = 0x23;
        printf(" D key entered");
    }

    if (byte3 == 0x75 && byte2 == 0xE0 && byte1 != 0xF0) { // Up key
        HEX_bits = 0b110111;
        *HEX3_HEX0_ptr = HEX_bits;
        p2.pressedKey = 0x75;
        printf(" Up key entered");
    }
    else if (byte3 == 0x6B && byte2 == 0xE0 && byte1 != 0xF0) { // Left key
        HEX_bits = 0b111001;
        *HEX3_HEX0_ptr = HEX_bits;
        p2.pressedKey = 0x6B;
        printf(" Left key entered");
    }

    else if (byte3 == 0x72 && byte2 == 0xE0 && byte1 != 0xF0) { // Down key
        HEX_bits = 0b111110;
        *HEX3_HEX0_ptr = HEX_bits;
        p2.pressedKey = 0x72;
        printf(" Down key entered");
    }

    else if (byte3 == 0x74 && byte2 == 0xE0 && byte1 != 0xF0) { // Right key
        HEX_bits = 0b1111;
        *HEX3_HEX0_ptr = HEX_bits;
        p2.pressedKey = 0x74;
        printf(" Up key entered");
    }

    return;
}

void timer_ISR(){
    int * ledr_ptr = (int *) LEDR;
    int * timer_ptr = (int *) TIMER;

    if(*ledr_ptr == 1){
        *ledr_ptr = 0b1111111111;
    }
    else{
        *ledr_ptr = *ledr_ptr >> 1;
    }

    ticks--;
    *(timer_ptr + 3) = 1;

}
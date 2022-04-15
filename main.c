    #include <stdio.h>
    #include <stdlib.h>
    #include "Sprites.h"
    #include <stdbool.h>
    #include <string.h>
    #include <time.h>
    #include <stdint.h>

    #define KEYBOARD 0xFF200100
    #define TIMER 0xFFFEC600
    #define LEDR 0xFF200000
    #define KEYS 0xFF200050
    #define SWITCHES 0xFF200040
    #define HEX 0xFF200020
    #define PIXELBUFFERCTRLREGS 0xFF203020

    #define p1DisplayX 50
    #define p1DisplayY 125
    #define p2DisplayX 175
    #define p2DisplayY 125
    #define scoreBoardTop 120
    #define scoreBoardBottom 160
    #define scoreBoardRight 75

    void disable_A9_interrupts (void);
    void set_A9_IRQ_stack (void);
    void config_GIC (void);
    void config_Keyboard (void);
    void config_Timer (bool enabled);
    void enable_A9_interrupts (void);

    void initPlayers(void);
    void mainMenu(void);
    void newRound(void);
    void scoreboard(void);

    void plot_pixel(int x, int y, short int line_color);
    void initDisplay(void);
    void clearScreen(void);
    void swapBuffers(void);
    void drawKey(int x, int y, const uint16_t image[][25]);
    void drawNumber(int x, int y, int number);



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
    volatile int ticks, pixel_buffer_start; // global variable;
    volatile int * pixel_ctrl_ptr = (int *)PIXELBUFFERCTRLREGS;

    struct player {
        bool fail;
        bool success;
        int points;
        char keys[4];
        char targetKey, pressedKey;
    } p1, p2;

    int main(void)
    {
        /* Interrupt methods */
        disable_A9_interrupts (); // disable interrupts in the A9 processor
        set_A9_IRQ_stack (); // initialize the stack pointer for IRQ mode
        config_GIC (); // configure the general interrupt controller
        config_Keyboard (); // configure Ps/2 Keyboard to generate interrupts
        config_Timer(false); //configure Private Timer to generate interrupts and loop every second
        enable_A9_interrupts (); // enable interrupts in the A9 processor


        time_t t;
        srand((unsigned) time(&t));

        initDisplay();
        initPlayers();

        while (1){
            p1.points = 0;
            p2.points = 0;

            mainMenu();
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
        config_Timer(false);
        clearScreen();

        int i, j;
        for(i = 0; i < 319; i++){
            for(j = 0; j < 75; j++){
                plot_pixel(i, j, Title[j][i]);
            }
        }

        for(i = 0; i < 319; i++){
            for(j = 0; j < 75; j++){
                plot_pixel(i, j+170, Hint[j][i]);
            }
        }

        swapBuffers();
        clearScreen();

        for(i = 0; i < 319; i++){
            for(j = 0; j < 75; j++){
                plot_pixel(i, j, Title[j][i]);
            }
        }

        for(i = 0; i < 319; i++){
            for(j = 0; j < 75; j++){
                plot_pixel(i, j+170, Hint[j][i]);
            }
        }
        selectingDifficulty();
        selectingRounds();

    }
    //Selects the starting difficulty of the game using the switches and key 0
    void selectingDifficulty(){
        int * keys_ptr = (int *) KEYS;
        volatile int switches, keys, leds;
        int i, j;

        for(i = 0; i < 319; i++){
            for(j = 100; j < 150; j++){
                if(i >= 100 && i < 200 && j >= 130) plot_pixel(i, j, Difficulty[j-130][i-100]);
                else plot_pixel(i, j, 0);
            }
        }

        swapBuffers();



        while(1){
            keys = *(keys_ptr + 3);
            if(*(keys_ptr + 3) & 0x1){
                for(i = 9; i >= 0; i--){
                    leds = *((int *) LEDR);
                    if(leds >> i & 0b1){
                        difficulty = difficulties[9-i];
                        *(keys_ptr + 3) = keys;

                        for(i = 0; i < 319; i++){
                            for(j = 100; j < 150; j++){
                                if(i >= 120 && i < 220 && j >= 130) plot_pixel(i, j, Round[j-130][i-120]);
                                else plot_pixel(i, j, 0);
                            }
                        }

                        swapBuffers();

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

    void displayNewRound(void);
    void drawTicker(void);
    void drawButton(bool, bool, bool, bool);

    //New round is configured and played here
    void newRound(){
        p1.targetKey = p1.keys[rand()%4];
        p2.targetKey = p2.keys[rand()%4];
        clearScreen();
        displayNewRound();
        swapBuffers();

        clearScreen();
        displayNewRound();


        ticks = 10;
        config_Timer(true);

        while(ticks > 0){
            drawTicker();
            drawButton(p1.success,p1.fail, p2.success, p2.fail);
            swapBuffers();

            if(!p1.success && !p1.fail && (p1.pressedKey == 0x1D || p1.pressedKey == 0x1C || p1.pressedKey == 0x1B || p1.pressedKey == 0x23)){
                if(p1.pressedKey == p1.targetKey){
                    p1.success = true;
                    if(!p2.success) p1.points += 100*ticks;
                    else p1.points += 100;
                }
                else{
                    p1.fail = true;
                }
            }

            if(!p2.fail && !p2.success && (p2.pressedKey == 0x75 || p2.pressedKey == 0x6B || p2.pressedKey == 0x72 || p2.pressedKey == 0x74)){
                if(p2.pressedKey == p2.targetKey){
                    p2.success = true;
                    if(!p1.success) p2.points += 100*ticks;
                    else p2.points += 100;
                }
                else{
                    p2.fail = true;
                }
            }
        }

        rounds--;
        return;
    }

    void displayNewRound(){
        int i, j;

        for(i = 0; i< 71; i++){
            for(j = 0; j < 72; j++){
                plot_pixel(i+p1DisplayX, j+p1DisplayY, unpushedButton[j][i]);
            }
        }

        for(i = 0; i< 71; i++){
            for(j = 0; j < 72; j++){
                if(unpushedButton[j][i] == 1407)
                    plot_pixel(i+p2DisplayX, j+p2DisplayY, 0xFCC0);
                else if(unpushedButton[j][i] == 17951)
                    plot_pixel(i+p2DisplayX, j+p2DisplayY, 0xFD24);
                else if(unpushedButton[j][i] == 1113)
                    plot_pixel(i+p2DisplayX, j+p2DisplayY, 0xFC60);
                else if(unpushedButton[j][i] == 951)
                    plot_pixel(i+p2DisplayX, j+p2DisplayY, 0xF3E0);
                else
                    plot_pixel(i+p2DisplayX, j+p2DisplayY, unpushedButton[j][i]);
            }
        }


        if (p1.targetKey == 0x1D) { // W key
            drawKey(p1DisplayX + 20, p1DisplayY +15, w);
        }
        else if (p1.targetKey == 0x1C) { //A key
            drawKey(p1DisplayX + 20, p1DisplayY +15, a);
        }
        else if (p1.targetKey == 0x1B) { //S key
            drawKey(p1DisplayX + 20, p1DisplayY +15, s);
        }
        else if (p1.targetKey == 0x23){ //D key
            drawKey(p1DisplayX + 20, p1DisplayY +15, d);
        }

        if (p2.targetKey == 0x75) { // Up key
            drawKey(p2DisplayX + 20, p2DisplayY +15, up);
        }
        else if (p2.targetKey == 0x6B) { //A key
            drawKey(p2DisplayX + 20, p2DisplayY +15, left);
        }
        else if (p2.targetKey == 0x72) { //S key
            drawKey(p2DisplayX + 20, p2DisplayY +15, down);
        }
        else if (p2.targetKey == 0x74){ //D key
            drawKey(p2DisplayX + 20, p2DisplayY +15, right);
        }

    }

    void drawTicker(){
        int i, j, k;
        int colours[] = {0xF800, 0xFAA2, 0xFD41, 0xFFE0, 0xE7C4, 0xBFC6, 0x07E0, 0x4688, 0x4DC8, 0x1463};

        for(i = 0; i < 10; i ++) {
            if(i < ticks){
                for (j = 0; j < 10; j++) {
                    for (k = 0; k < 15; k++) {
                        plot_pixel(105 + j + i*10, 50 + k, colours[i]);
                    }
                }
            }
            else if (i == ticks){
                for (j = 0; j < 10; j++) {
                    for (k = 0; k < 15; k++) {
                        plot_pixel(105 + j + i*10, 50 + k, 0);
                    }
                }
                break;
            }
        }
    }

    void scoreboard(){
        ticks = 0;
        int i, j, temp1 = p1.points, temp2 = p2.points, divisor = 10000;

        clearScreen();
        swapBuffers();
        clearScreen();


        for(i = 0; i < 320; i++){
            for(j = 0; j < 75; j++){
                plot_pixel(i, j, Scoreboard[j][i]);
            }
        }

        for(i = 50; i < 270; i++){
            for(j = 80; j < 85; j++){
                plot_pixel(i, j, 0xFFFF);
            }
        }

        if(p1.points >= p2.points){
            for(i=0; i < 30; i++){
                for(j=0; j < 20; j++){
                    plot_pixel(i+scoreBoardRight, j+scoreBoardTop, P1[j][i]);
                }
            }
            for(i = 0; i < 5; i++){
                drawNumber(scoreBoardRight + 75 +i*17, scoreBoardTop, temp1/divisor);
                temp1 = temp1%divisor;
                divisor /= 10;
            }
            divisor = 10000;
            for(i=0; i < 30; i++){
                for(j=0; j < 20; j++){
                    plot_pixel(i+scoreBoardRight, j+scoreBoardBottom, P2[j][i]);
                }
            }
            for(i = 0; i < 5; i++){
                drawNumber(scoreBoardRight + 75 +i*17, scoreBoardBottom, temp2/divisor);
                temp2 = temp2%divisor;
                divisor /= 10;
            }
        }
        else{
            for(i=0; i < 30; i++){
                for(j=0; j < 20; j++){
                    plot_pixel(i+scoreBoardRight, j+scoreBoardTop, P2[j][i]);
                }
            }
            for(i = 0; i < 5; i++){
                drawNumber(scoreBoardRight + 75 +i*17, scoreBoardTop, temp2/divisor);
                temp2 = temp2%divisor;
                divisor /= 10;
            }
            divisor = 10000;
            for(i=0; i < 30; i++){
                for(j=0; j < 20; j++){
                    plot_pixel(i+scoreBoardRight, j+scoreBoardBottom, P1[j][i]);
                }
            }
            for(i = 0; i < 5; i++){
                drawNumber(scoreBoardRight + 75 +i*17, scoreBoardBottom, temp1/divisor);
                temp1 = temp1%divisor;
                divisor /= 10;
            }
        }

        swapBuffers();
        while(ticks*-difficulty < 200000000*3);

        p1.pressedKey = 0;
        p1.fail = false;
        p1.success = false;

        p2.pressedKey = 0;
        p2.fail = false;
        p2.success = false;


    }


    void plot_pixel(int x, int y, short int line_color)
    {
        *(short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = line_color;
    }
    void drawNumber(int x, int y, int number){
        int i, j;

        switch(number){
            case 1:
                for(i = 0; i< 12; i++){
                    for(j = 0; j < 15; j++){
                        plot_pixel(i+x, j+y, one[j][i]) ;
                    }
                }
                break;
            case 2:
                for(i = 0; i< 12; i++){
                    for(j = 0; j < 15; j++){
                        plot_pixel(i+x, j+y, two[j][i]);
                    }
                }
                break;
            case 3:
                for(i = 0; i< 12; i++){
                    for(j = 0; j < 15; j++){
                        plot_pixel(i+x, j+y, three[j][i]);
                    }
                }
                break;
            case 4:
                for(i = 0; i< 12; i++){
                    for(j = 0; j < 15; j++){
                        plot_pixel(i+x, j+y, four[j][i]);
                    }
                }
                break;
            case 5:
                for(i = 0; i< 12; i++){
                    for(j = 0; j < 15; j++){
                        plot_pixel(i+x, j+y, five[j][i]);
                    }
                }
                break;
            case 6:
                for(i = 0; i< 12; i++){
                    for(j = 0; j < 15; j++){
                        plot_pixel(i+x, j+y, six[j][i]);
                    }
                }
                break;
            case 7:
                for(i = 0; i< 12; i++){
                    for(j = 0; j < 15; j++){
                        plot_pixel(i+x, j+y, seven[j][i]);
                    }
                }
                break;
            case 8:
                for(i = 0; i< 12; i++){
                    for(j = 0; j < 15; j++){
                        plot_pixel(i+x, j+y, eight[j][i]);
                    }
                }
                break;
            case 9:
                for(i = 0; i< 12; i++){
                    for(j = 0; j < 15; j++){
                        plot_pixel(i+x, j+y, nine[j][i]);
                    }
                }
                break;
            default:
                for(i = 0; i< 12; i++){
                    for(j = 0; j < 15; j++){
                        plot_pixel(i+x, j+y, zero[j][i]);
                    }
                }
                break;
        }


    }

    void drawButton(bool p1success, bool p1fail, bool p2success, bool p2fail){
        int i, j;

        if(p1success){
            for(i = 0; i< 71; i++){
                for(j = 0; j < 72; j++){
                    plot_pixel(i+p1DisplayX, j+p1DisplayY, pushedButton[j][i]);
                }
            }
        }
        else if(p1fail){
            for(i = 0; i< 71; i++){
                for(j = 0; j < 72; j++){
                    if(pushedButton[j][i] == 19818)
                        plot_pixel(i+p1DisplayX, j+p1DisplayY, 57798);
                    else if(pushedButton[j][i] == 30703)
                        plot_pixel(i+p1DisplayX, j+p1DisplayY, 60074);
                    else if(pushedButton[j][i] == 15399)
                        plot_pixel(i+p1DisplayX, j+p1DisplayY, 53638);
                    else if(pushedButton[j][i] == 17576)
                        plot_pixel(i+p1DisplayX, j+p1DisplayY, 49477);
                    else
                        plot_pixel(i+p1DisplayX, j+p1DisplayY, pushedButton[j][i]);
                }
            }
        }

        if(p2success){
            for(i = 0; i< 71; i++){
                for(j = 0; j < 72; j++){
                    plot_pixel(i+p2DisplayX, j+p2DisplayY, pushedButton[j][i]);
                }
            }
        }
        else if(p2fail){
            for(i = 0; i< 71; i++){
                for(j = 0; j < 72; j++){
                    if(pushedButton[j][i] == 19818)
                        plot_pixel(i+p2DisplayX, j+p2DisplayY, 57798);
                    else if(pushedButton[j][i] == 30703)
                        plot_pixel(i+p2DisplayX, j+p2DisplayY, 60074);
                    else if(pushedButton[j][i] == 15399)
                        plot_pixel(i+p2DisplayX, j+p2DisplayY, 53638);
                    else if(pushedButton[j][i] == 17576)
                        plot_pixel(i+p2DisplayX, j+p2DisplayY, 49477);
                    else
                        plot_pixel(i+p2DisplayX, j+p2DisplayY, pushedButton[j][i]);
                }
            }
        }

        if (p1.pressedKey == 0x1D) { // W key
            drawKey(p1DisplayX + 20, p1DisplayY +25, w);
        }
        else if (p1.pressedKey == 0x1C) { //A key
            drawKey(p1DisplayX + 20, p1DisplayY +25, a);
        }
        else if (p1.pressedKey == 0x1B) { //S key
            drawKey(p1DisplayX + 20, p1DisplayY +25, s);
        }
        else if (p1.pressedKey == 0x23){ //D key
            drawKey(p1DisplayX + 20, p1DisplayY +25, d);
        }

        if (p2.pressedKey == 0x75) { // Up key
            drawKey(p2DisplayX + 20, p2DisplayY +25, up);
        }
        else if (p2.pressedKey == 0x6B) { //A key
            drawKey(p2DisplayX + 20, p2DisplayY +25, left);
        }
        else if (p2.pressedKey == 0x72) { //S key
            drawKey(p2DisplayX + 20, p2DisplayY +25, down);
        }
        else if (p2.pressedKey == 0x74){ //D key
            drawKey(p2DisplayX + 20, p2DisplayY +25, right);
        }
    }

    void drawKey(int x, int y, const uint16_t image[][25]){
        int i, j;

        for(i = 0; i< 25; i++){
            for(j = 0; j < 25; j++){
                if(image[j][i])
                    plot_pixel(i+x, j+y, image[j][i]);
            }
        }
    }

    void swapBuffers(){
        *pixel_ctrl_ptr = 1;
        while(*(pixel_ctrl_ptr + 3) & 1);
        pixel_buffer_start = *(pixel_ctrl_ptr+1);
    }

    void clearScreen(){
        int i, j;
        for(i = 0; i < 320; i++){
            for(j =0; j < 240; j++){
                plot_pixel(i,j,0);
            }
        }
    }

    void initDisplay(){
        *(pixel_ctrl_ptr + 1) = 0xC8000000; // first store the address in the back buffer
        pixel_buffer_start = *(pixel_ctrl_ptr+1);
        clearScreen(); // pixel_buffer_start points to the pixel buffer
        swapBuffers();
        /* set back pixel buffer to start of SDRAM memory */
        *(pixel_ctrl_ptr + 1) = 0xC0000000;
        clearScreen(); // pixel_buffer_start points to the pixel buffer
        swapBuffers();

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
        p2.keys[1] = 0x6B;
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
    void config_Timer(bool enabled){
        volatile int * timer_ptr = (int *) TIMER;
        *(timer_ptr) = difficulty;
        if(enabled) *(timer_ptr + 2) = 0b111; //CHANGE THIS LINE
        else *(timer_ptr + 2) = 0b110;
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
        __asm__("msr cpsr, %[ps]" : : [ps]"r"(status));
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
        __asm__("msr cpsr, %[ps]" : : [ps] "r" (mode));
        /* set banked stack pointer */
        __asm__("mov sp, %[ps]" : : [ps] "r" (stack));
        /* go back to SVC mode before executing subroutine return! */
        mode = 0b11010011;
        __asm__("msr cpsr, %[ps]" : : [ps] "r" (mode));
    }
    /*
    * Turn on interrupts in the ARM processor
    */
    void enable_A9_interrupts(void)
    {
        int status = 0b01010011;
        __asm__("msr cpsr, %[ps]" : : [ps]"r"(status));
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

        if (byte3 == 0x1D && byte2 != 0xF0) { // W key
            p1.pressedKey = 0x1D;
        }

        if (byte3 == 0x1C && byte2 != 0xF0) { //A key
            p1.pressedKey = 0x1C;
        }
        else if (byte3 == 0x1B && byte2 != 0xF0) { //S key
            p1.pressedKey = 0x1B;
        }
        else if (byte3 == 0x23 && byte2 != 0xF0){ //D key
            p1.pressedKey = 0x23;
        }

        if (byte3 == 0x75 && byte2 == 0xE0 && byte1 != 0xF0) { // Up key
            HEX_bits = 0b110111;
            *HEX3_HEX0_ptr = HEX_bits;
            p2.pressedKey = 0x75;
        }
        else if (byte3 == 0x6B && byte2 == 0xE0 && byte1 != 0xF0) { // Left key
            HEX_bits = 0b111001;
            *HEX3_HEX0_ptr = HEX_bits;
            p2.pressedKey = 0x6B;
        }

        else if (byte3 == 0x72 && byte2 == 0xE0 && byte1 != 0xF0) { // Down key
            HEX_bits = 0b111110;
            *HEX3_HEX0_ptr = HEX_bits;
            p2.pressedKey = 0x72;
        }

        else if (byte3 == 0x74 && byte2 == 0xE0 && byte1 != 0xF0) { // Right key
            HEX_bits = 0b1111;
            *HEX3_HEX0_ptr = HEX_bits;
            p2.pressedKey = 0x74;
        }

        return;
    }

    void timer_ISR(){
        int * timer_ptr = (int *) TIMER;
        ticks--;
        *(timer_ptr + 3) = 1;

    }


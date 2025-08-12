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

uint8_t Control_pin = 13;
uint8_t attackPin = 23;
int temp = 0;

const uint8_t DATA_PINS[] = {15, 2, 4, 16, 17, 5, 18, 19}; // D0 to D7
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

void IRAM_ATTR vSync_Raising_ISR()
{
    vcount++; // Increment vertical count on rising edge of vSync
}

void IRAM_ATTR hSync_ISR()
{
    if (digitalRead(pin_hSync) == LOW)
    {
        vcount = 0; // Reset vertical count on falling edge of hSync
        line80_detected = false;
        line160_detected = false;
        line320_detected = false;
    }
}

// Initialize Pins
void setupDataPins_OUTPUT()
{
    for (uint8_t i = 0; i < NUM_DATA_PINS; i++)
    {
        pinMode(DATA_PINS[i], OUTPUT);
    }
}

// Change function signature to use uint8_t for 8-bit binary input
void setAllDataPins(uint8_t pattern)
{
    for (uint8_t i = 0; i < NUM_DATA_PINS; i++)
    {
        // Extract each bit from pattern using right shift and mask
        bool pinState = (pattern >> i) & 0x01;
        digitalWrite(DATA_PINS[i], pinState);
    }
}

void Control_pin_Zmode()
{
    digitalWrite(Control_pin, HIGH);
}

void Control_pin_ControlMode()
{
    digitalWrite(Control_pin, LOW);
}

void setup()
{
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

    attachInterrupt(digitalPinToInterrupt(pin_vSync), vSync_Raising_ISR, RISING);
    attachInterrupt(digitalPinToInterrupt(pin_hSync), hSync_ISR, FALLING);
}

pixel target_pixel = {120, 120}; // Target pixel location
int stripe_dist = 240;           // unit pixel

void loop()
{
    while (digitalRead(attackPin) == LOW)
    {

        // Vertical Line
        if (vcount > 10 && vcount < 300)
        { // Line start and end number

            if (temp != vcount)
            {
                for (int j = 0; j < 10; j++) {

                    // Move Right - offset 50px
                    for (int i = 0; i < 49 * 20; i++)
                    {                                // 49*pixel number
                        __asm__ __volatile__("nop"); // No-operation instruction
                    }

                    // attack - Pulling down | 00000000 are more stable than 111111111
                    setAllDataPins(0b00000000); // 1st byte
                    Control_pin_ControlMode();
                    for (int i = 0; i < 49 * 1; i++) __asm__ __volatile__("nop"); // No-operation instruction for 1 px duration
                    Control_pin_Zmode();

                
                }                

                temp = vcount;
            }
        }


    }
}
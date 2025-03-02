#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

// Define GPIO pins for HC-SR04 (sonar sensor)
#define TRIG_PIN 13     
#define ECHO_PIN 12     
#define SONAR_INTERVAL 200  // 200ms between pings
#define TIMEOUT_US 38000    // 40ms timeout in microseconds reflect max sensed distance
#define MAX_SONAR_DISTANCE  255 // Maximum distance in cm
// FSM States for the sonar sensor
typedef enum {
    PING_SENT,        // Trigger sent, waiting for rising edge
    ECHO_IN_PROGRESS, // Rising edge received, waiting for falling edge
    PING_SUCCESS,     // Falling edge received, measurement successful
    NO_ECHO           // Timeout occurred (no echo received)
} sonar_state_t;

const float US_TO_CM = 0.01715;  // Conversion: cm per microsecond

// Array to store the last 5 readings
volatile int8_t distances[5] = {0};  
volatile int8_t reading_index = 0;  // Index for storing readings

// Global variables for sonar sensor measurements and timing
volatile sonar_state_t sonar_state;
volatile uint64_t start_time = 0;
volatile uint64_t end_time = 0;
volatile uint64_t sonar_time=0;
volatile uint8_t distance = 0;
volatile bool distance_updated=false;

volatile uint64_t display_time = 0; // for only testing 

// IRQ callback function for the echo pin (lightweight)
void echo_callback(uint gpio, uint32_t events) {
    if ((events & GPIO_IRQ_EDGE_RISE) && (sonar_state == PING_SENT)) {
        start_time = time_us_64();        
        sonar_state = ECHO_IN_PROGRESS;
    } 
    else if ((events & GPIO_IRQ_EDGE_FALL) && (sonar_state == ECHO_IN_PROGRESS)) {
        end_time = time_us_64(); 
        sonar_state = PING_SUCCESS;        
    }
}
// Function to send a sonar ping (executed periodically)
void send_sonar_ping() { 
    //     
    gpio_put(TRIG_PIN, 1);
    sleep_us(10);
    gpio_put(TRIG_PIN, 0);
    sonar_state = PING_SENT;
}
int main() {
    //setup()
    stdio_init_all();
    // Initialize trigger pin as output and set low
    gpio_init(TRIG_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);
    gpio_put(TRIG_PIN, 0);  
    // Initialize echo pin as input
    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    // Enable IRQ for echo pin (always active)
    gpio_set_irq_enabled_with_callback(ECHO_PIN,GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &echo_callback); 
    //loop()   
    while (true) {
        // Periodic ping every SONAR_INTERVAL
        if ((time_us_64() - sonar_time) / 1000 >= SONAR_INTERVAL) {
            distance_updated = false; // start new measurement 
            send_sonar_ping();
            sonar_time = time_us_64(); 
        }        
        // Process echo results outside the interrupt handler
        if (!distance_updated && sonar_state == PING_SUCCESS) {
            uint64_t echo_time = end_time - start_time;            
            distance = (uint8_t) ((end_time - start_time) * US_TO_CM);            
            distance_updated = true;
            // Store the echo time in the array
            distances[reading_index] = distance;
            reading_index = (reading_index + 1) % 5; // Circular index
            
        }
        // Handle timeout (if no echo was received in time)
        if (!distance_updated && sonar_state == ECHO_IN_PROGRESS && ((time_us_64() - start_time) > TIMEOUT_US)) {             
            sonar_state = NO_ECHO;
            distance = MAX_SONAR_DISTANCE;
            distance_updated = true;
        }

        // Display distance every 1000ms
        if ((time_us_64()- display_time) / 1000 >= 1000) {
            //printf("Distance: %.2f cm\n", distance);
            printf("Last 5 Echo Times (us): [ ");
            for (int i = 0; i < 5; i++) {
                printf("%d  ", distances[i]);
            }
            printf("]\n");
            display_time = time_us_64(); // may be current_time is better solution
        }
    }
}


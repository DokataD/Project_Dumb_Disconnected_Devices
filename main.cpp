#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <cmath>

#include "hardware/adc.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"

extern "C"
{
#include "motor/MotorDriver.h"
#include "sensors/MPU6050.h"
#include "sensors/Ultrasonic.h"
#include "algorithm/remember.h"
#include "servoControl/servo.h"
}

//Ultrasonic Pins
const uint trigPinR = 5;
const uint echoPinR = 4;

const uint trigPinF = 8;
const uint echoPinF = 9;

const uint trigPinL = 7;
const uint echoPinL = 6;

// IR Input
const uint aout3 = 26;
const uint aout2 = 27;
const uint aout1 = 28;

// Led Output
const uint ledpin = 25;

// initalise ultrasonic sensors and their respective namespace
Ultrasonic ultrasonicF(trigPinF, echoPinF);
Ultrasonic ultrasonicL(trigPinL, echoPinL);
Ultrasonic ultrasonicR(trigPinR, echoPinR);

// initalise gyro and namespcae
gyro mpu;

// Gyro drift correction variable
float angle_offset = 0.0f;


// initialise built in led and ir sensors
void initLED()
{
    gpio_init(ledpin);
    gpio_set_dir(ledpin, GPIO_OUT);
    gpio_put(ledpin, 0);
    adc_init();
    adc_gpio_init(aout1);
    adc_gpio_init(aout2);
    adc_gpio_init(aout3);
    adc_select_input(2);
}

// this function looks at all the ir sensors and if any one of them detects a change it returns a bool
bool betOnBlack()
{
    uint16_t ir = 0;
    
    adc_select_input(0);
    ir += adc_read();
    
    adc_select_input(1);
    ir += adc_read();
    
    adc_select_input(2);
    ir += adc_read();
    
    printf("\n %d sensor output", ir);
    if (ir < 9000){
        return true;
    } else {
        return false;
    }
}

char uart_received() {
    char c = 0; 
    if (uart_is_readable(uart0)) {
        c = uart_getc(uart0);
        uart_puts(uart0, "Ack\n");
    }
    return c;
}

int main()
{   
    //start the stdio for serial communication, uart is on 0TX and 1RX 
    stdio_init_all();
    uart_init(uart0, 115200);
    gpio_set_function(0, GPIO_FUNC_UART); // TX
    gpio_set_function(1, GPIO_FUNC_UART); // RX
    
    // sets up leds
    initLED();
    
    // sets the motor dirver i2c and pwm 
    Motor_Init(); 
    
    //servo setup
    initServo(13);
    setServoAngle(0);
    sleep_ms(50);
    
    ////////////////////////////////////////////////////
    // below is test code (it's now working code :D ) //
    ////////////////////////////////////////////////////
    
    int WALL = 40;    // the distance to the wall
    int drifted = 20;
    int tolerance = 8;    // the amount it should not exced if it is drifted
    int speed = 60;    //the speed of the motors
    int completion = 0;    // to check if the end of the maze hase been reached
    bool turning_right = false;
    bool ignore_right_sensor = false;   // Flag to ignore right ultrasonic during turns
    
    while (true) 
    {
        gpio_put(ledpin, 1);

        // Check for sparkly coins
        if (betOnBlack()) {
            if (completion == 0) {
            printf("TREASURE");
            Motor_Stop_All();
            completion++;
            sleep_ms(2000); // Stop to signal treasure found
            rotate(197, CC);  // Turn around
            sleep_ms(200);
            continue; 
            } else {
                printf("Maze completed. Yo ho laddies. Yo ho.\n");
                Motor_Stop_All(); 
                Motor_All(BACKWARD, speed); // Stop to celebrate
                sleep_ms(50);
                Motor_All(FORWARD, speed);
                Motor_Stop_All();
                sleep_ms(50);
                exit(0); //End
            }
        }

        // Getting the distances from the ultrasonic sensors
        int distanceF = ultrasonicF.getDistanceCm();
        int distanceL = ultrasonicL.getDistanceCm();
        
        // Handle right sensor with ignore logic
        int distanceR = 0;
        if (!ignore_right_sensor)
        {
            distanceR = ultrasonicR.getDistanceCm();
            if (distanceR == -1)
            {
                distanceR = 200; // Treat timeout as far away
            }
        }

        else
        {
            // Check if we should stop ignoring (wall detected > 30cm away)
            int temp_distanceR = ultrasonicR.getDistanceCm();
            if (temp_distanceR < 30 && temp_distanceR != -1)
            {
                ignore_right_sensor = false;
                distanceR = temp_distanceR;
                printf("Right sensor re-enabled, detected wall at %dcm\n", distanceR);
            }
            
            else
            {
                distanceR = 200; // Keep treating as far away
            }
        }

        // Handle front sensor timeout (wall is far away)
        if (distanceF == -1) 
        {
            distanceF = 200; // Treat as "no obstacle ahead" - safe to continue forward
        }

        printf("F:%d L:%d R:%d (ignore_right: %s)\n", distanceF, distanceL, distanceR, ignore_right_sensor ? "true" : "false");
        
        int error = distanceL - distanceR; // To get the drifted distance 
        
        // rotating to the right
        if (distanceR > WALL && distanceR != 0 && !turning_right)
        {
            Motor_All(FORWARD, speed);
            sleep_ms(500);
            printf("TURNING RIGHT\n");
            ignore_right_sensor = true; // Start ignoring right sensor during turn
            rotate(93, C);
            turning_right = true; // Set flag to enter forward movement phase
            Motor_All(FORWARD, speed);
            // sleep_ms(300);

        } 
        
        else if (turning_right)
        {
            // During right turn, use the current distanceR (which respects ignore logic)
            if (distanceR > 20)
            {
                Motor_All(FORWARD, speed);
                sleep_ms(180);
                continue; // Skip rest of main loop logic
            }
            
            else
            {
                turning_right = false; // Exit this phase, return to normal logic
            }
        }

        // rotating to the left
        else if (distanceF <= 17 && distanceL > WALL && distanceR <= WALL) 
        {
            Motor_All(FORWARD, speed);
            sleep_ms(300);
            printf("TURNING LEFT\n");
            rotate(93, CC);
        }

        // driving forward
        else if (distanceF > 17) 
        {
            bool WallsOnBothSides = (distanceR < drifted && distanceL < drifted);

            // Centering the car in the middle
            if (WallsOnBothSides && abs(error) > tolerance && (distanceL != -1 && distanceR != -1))
            {
                if (error < -tolerance)
                { //drifting to the right if it is close to the left
                    printf("CORRECTING RIGHT\n");
                    Motor_All(RIGHT, speed);
                    sleep_ms(100);
                    continue;
                }

                //drifting to the left if it is close to the right
                else
                { 
                    printf("CORRECTING LEFT\n");
                    Motor_All(LEFT, speed);
                    sleep_ms(100);
                    continue;
                }
            }

            // Adjusting checks if the car is too close to the wall
            if (distanceL == -1)
            { 
                printf("Adjust right\n");
                Motor_All(RIGHT, speed);
                sleep_ms(100);
                continue;
            }

            if (distanceR == -1)
            {
                printf("Adjust left\n");
                Motor_All(LEFT, speed);
                sleep_ms(100);
                continue;
            }

            // moving forward
            else
            {
                printf("GOING FORWARD\n");
                Motor_All(FORWARD, speed);
            }
        }

        // rotating to 180
        else if (distanceF <= 17 && distanceL <= WALL && distanceR <= WALL) 
        {
            printf("DEAD END - ROTATING\n");
            rotate(191, CC);
        }

        gpio_put(ledpin, 0);
        sleep_ms(100);
    }
    return 0;
}
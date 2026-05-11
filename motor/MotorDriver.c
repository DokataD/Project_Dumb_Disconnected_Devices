/*****************************************************************************
* | File        :   MotorDriver.c
* | Author      :   Waveshare team
* | Function    :   Drive TB6612FNG
* | Info        :
*                TB6612FNG is a driver IC for DC motor with output transistor in
*                LD MOS structure with low ON-resistor. Two input signals, IN1
*                and IN2, can choose one of four modes such as CW, CCW, short
*                brake, and stop mode.
*----------------
* |	This version:   V1.0
* | Date        :   2018-09-04
* | Info        :   Basic version
*
******************************************************************************/
#include "motor/MotorDriver.h"
#include "motor/Debug.h"

int MotorPin[4][3]={ {0,1,2},{3,4,5},{6,7,8},{9,10,11} } ;//MA,MB,MC,MD
int MotorDir[2][2]={ {0,1},{1,0} } ;//forward, backward

/**
 * Motor rotation.
 *
 * Example:
 * Motor_Init();
 */
void Motor_Init(void)
{
    PCA9685_Init(0x40);
    PCA9685_SetPWMFreq(100);
}

/**
 * Motor rotation.
 *
 * @param motor: Motor A and Motor B.
 * @param dir: forward and backward.
 * @param speed: Rotation speed.  //(0~100)
 *
 * Example:
 * @code
 * Motor_Run(MOTORA, FORWARD, 50);
 * Motor_Run(MOTORB, BACKWARD, 100);
 */
void Motor_Run(UBYTE motor, DIR dir, UWORD speed)
{
    if(speed > 100)
        speed = 100;

    PCA9685_SetPwmDutyCycle(MotorPin[motor][0], speed);
    PCA9685_SetLevel(MotorPin[motor][1], MotorDir[dir][0]);
    PCA9685_SetLevel(MotorPin[motor][2], MotorDir[dir][1]);
}

/**
 * Motor stop rotation.
 *
 * @param motor: Motor A and Motor B.
 *
 * Example:
 * @code
 * Motor_Stop(MOTORA);
 */
void Motor_Stop()
{
    PCA9685_SetPwmDutyCycle(MotorPin[MOTORA][0], 0);
    PCA9685_SetPwmDutyCycle(MotorPin[MOTORB][0], 0);
    PCA9685_SetPwmDutyCycle(MotorPin[MOTORC][0], 0);
    PCA9685_SetPwmDutyCycle(MotorPin[MOTORD][0], 0);
}



/**
 * Motor stop rotation.
 *
 * @param motor: Motor A and Motor B.
 *
 * Example:
 * @code
 * Motor_Stop(MOTORA);
 */
void Motor_test(void)
{
    //1.System Initialization
    if(DEV_Module_Init()!=0){
        return;
    }
    DEV_Delay_ms(3000);
    printf("MOTOR TEST");
    // int x = 0;
    // while(1){
        // x++;
        // printf("MOTOR TEST\r\n");
        // DEV_Delay_ms(1000);
        
         // 2.Motor Initialization
        // Motor_Init();
        // printf("Motor_Init\r\n");
        
        // printf("Motor_Run time = %d\r\n", x );
        // Motor_Run(MOTORA, FORWARD, 100);
        
    // }
    
    //2.Motor Initialization
    Motor_Init();
    // printf("Motor_Init\r\n");
    
    // printf("Motor_Run\r\n");
    Motor_Run(MOTORA, FORWARD, 100);
    Motor_Run(MOTORB, FORWARD, 100);
    Motor_Run(MOTORC, FORWARD, 100);
    Motor_Run(MOTORD, FORWARD, 100);
    Motor_Run(MOTORB, BACKWARD, 100);
}


//New function
/**
 * Motor rotation.
 *
 * @param dir: forward backward left and right.
 * @param speed: Rotation speed.  //(0~100)
 *
 * Example:
 * @code
 * Motor_All(FORWARD, 50);
 */ 
void Motor_All(DIR dir, UWORD speed)
{
    int adjust = 20;
    if(speed > 100) {
        speed = 100;
    }

    if(dir == FORWARD) {

        Motor_Run(MOTORA, FORWARD, speed + 2);
        Motor_Run(MOTORB, FORWARD, speed + 2);
        Motor_Run(MOTORC, FORWARD, speed); 
        Motor_Run(MOTORD, FORWARD, speed + 3);
        return;
    } 

    if(dir == LEFT) {
        Motor_Run(MOTORA, BACKWARD, speed + 3);
        Motor_Run(MOTORB, FORWARD, speed + 25);
        Motor_Run(MOTORC, FORWARD, speed);
        Motor_Run(MOTORD, BACKWARD, speed + 3);
        return;
    }
    
    if(dir == RIGHT) {

        Motor_Run(MOTORA, FORWARD, speed + 20);
        Motor_Run(MOTORB, BACKWARD, speed + 3);
        Motor_Run(MOTORC, BACKWARD, speed);
        Motor_Run(MOTORD, FORWARD, speed + 3);
        return;

    }

    if(dir == BACKWARD) {
        Motor_Run(MOTORA, BACKWARD, speed + 2);
        Motor_Run(MOTORB, BACKWARD, speed + 2);
        Motor_Run(MOTORC, BACKWARD, speed);
        Motor_Run(MOTORD, BACKWARD, speed + 3);
        return;
    }

    if(dir == CORNER_R) {
        Motor_Run(MOTORA, FORWARD, speed);
        Motor_Run(MOTORB, FORWARD, speed - adjust);
        Motor_Run(MOTORC, FORWARD, speed);
        Motor_Run(MOTORD, FORWARD, speed - adjust);
        return;
    }
    
    if(dir == CORNER_L) {
        Motor_Run(MOTORA, FORWARD, speed - adjust);
        Motor_Run(MOTORB, FORWARD, speed);
        Motor_Run(MOTORC, FORWARD, speed - adjust);
        Motor_Run(MOTORD, FORWARD, speed);
        return;
    }
}


/**
 * Motor rotation.
 *
 * @param dir: forward backward left and right.
 *
 * Example:
 * @code
 * Rotate(90);
 */
void rotate (int delay, ROT rotate)
{
    int speed = 75;

    if (rotate == 0){
        Motor_Run(MOTORA, FORWARD, speed);
        Motor_Run(MOTORB, BACKWARD, speed + 3);
        Motor_Run(MOTORC, FORWARD, speed);
        Motor_Run(MOTORD, BACKWARD, speed + 3);
        //sleep_ms(delay * 6.2);

        sleep_ms(delay * 7.577);
        Motor_Stop_All();

    } else {
        Motor_Run(MOTORA, BACKWARD, speed);
        Motor_Run(MOTORB, FORWARD, speed + 3);
        Motor_Run(MOTORC, BACKWARD, speed);
        Motor_Run(MOTORD, FORWARD, speed + 3);
        
        //sleep_ms(delay * 6.2);
        sleep_ms(delay * 7.577);
        Motor_Stop_All();

    }

}

/**
 * Motor stop rotation.
 *
 * @param motor: Motor A and Motor B.
 *
 * Example:
 * @code
 * Motor_Stop(MOTORA);
 */
void Motor_Stop_All()
{
    PCA9685_SetPwmDutyCycle(MotorPin[0][0], 0);
    PCA9685_SetPwmDutyCycle(MotorPin[1][0], 0);
    PCA9685_SetPwmDutyCycle(MotorPin[2][0], 0);
    PCA9685_SetPwmDutyCycle(MotorPin[3][0], 0);
}
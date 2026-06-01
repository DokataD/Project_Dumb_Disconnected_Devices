#include <cstdio>

#include "pico/stdlib.h"

extern "C"
{
#include "include_motor/motor/DEV_Config.h"
#include "include_motor/motor/MotorDriver.h"
}

// Built-in LED on the Raspberry Pi Pico / Pico W
const uint LED_PIN = 25;

// Motor speed: 0 to 100
const int SPEED = 60;

// Each command movement duration
const int COMMAND_TIME_MS = 1000;

void init_led()
{
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);
}

void print_menu()
{
    printf("\n=== Terminal car control ===\n");
    printf("Type one command and press Enter:\n");
    printf("1 = forward\n");
    printf("2 = backward\n");
    printf("3 = left\n");
    printf("4 = right\n");
    printf("s = stop\n");
    printf("> ");
    fflush(stdout);
}

void move_for_one_second(DIR direction, const char *message)
{
    printf("%s for 1 second\n", message);

    gpio_put(LED_PIN, 1);
    Motor_All(direction, SPEED);
    sleep_ms(COMMAND_TIME_MS);
    Motor_Stop_All();
    gpio_put(LED_PIN, 0);

    printf("Stopped\n");
}

int main()
{
    // USB serial terminal setup
    stdio_init_all();
    sleep_ms(2000); // Gives the serial monitor time to connect after boot

    init_led();

    // Initialize I2C/PCA9685 motor driver board
    DEV_Module_Init();
    Motor_Init();
    Motor_Stop_All();

    print_menu();

    while (true)
    {
        int input = getchar_timeout_us(0);

        if (input == PICO_ERROR_TIMEOUT)
        {
            sleep_ms(10);
            continue;
        }

        char command = static_cast<char>(input);

        // Ignore Enter/newline characters from the terminal
        if (command == '\r' || command == '\n')
        {
            continue;
        }

        switch (command)
        {
            case '1':
                move_for_one_second(FORWARD, "Moving forward");
                break;

            case '2':
                move_for_one_second(BACKWARD, "Moving backward");
                break;

            case '3':
                move_for_one_second(LEFT, "Moving left");
                break;

            case '4':
                move_for_one_second(RIGHT, "Moving right");
                break;

            case 's':
            case 'S':
                printf("Emergency stop\n");
                Motor_Stop_All();
                gpio_put(LED_PIN, 0);
                break;

            default:
                printf("Unknown command: %c\n", command);
                break;
        }

        print_menu();
    }

    return 0;
}

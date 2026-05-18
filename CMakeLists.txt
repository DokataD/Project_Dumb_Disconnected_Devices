# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)


set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)


set(PICO_SDK_FETCH_FROM_GIT 1)


set(PICO_BOARD pico_w CACHE STRING "Board type")


include(pico_sdk_import.cmake)


if (PICO_SDK_VERSION_STRING VERSION_LESS "1.4.0")
  message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.4.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()


project(SmartRobotCar C CXX ASM)


pico_sdk_init()


file(GLOB_RECURSE
        SOURCE_FILES
        source/*.c
        source/*.cpp
        source/**/*.c
        source/**/*.cpp
        )


<<<<<<< HEAD
add_executable(SmartRobotCar ${SOURCE_FILES})

=======
add_executable(SmartRobotCar
    main.cpp
    motor/DEV_Config.c
    motor/MotorDriver.c
    motor/PCA9685.c
)
>>>>>>> c686f4f (Implemented BLE connection Arduino-Pico and car moves with serial monitor command imput)

pico_set_program_name(SmartRobotCar "SmartRobotCar")
pico_set_program_version(SmartRobotCar "0.1")


pico_enable_stdio_uart(SmartRobotCar 1)
pico_enable_stdio_usb(SmartRobotCar 1)


target_link_libraries(SmartRobotCar
        pico_multicore
        pico_stdlib)


target_include_directories(SmartRobotCar PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/include
  ${CMAKE_CURRENT_LIST_DIR}/include_motor/motor
)


target_link_libraries(SmartRobotCar
        hardware_pwm
        hardware_i2c
        hardware_spi
        hardware_adc
        )


pico_add_extra_outputs(SmartRobotCar)


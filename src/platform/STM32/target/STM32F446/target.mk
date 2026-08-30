TARGET_MCU        := STM32F446xx
TARGET_MCU_FAMILY := STM32F4

TARGET_SRC = \
            drivers/accgyro/accgyro_mpu6500.c \
            drivers/accgyro/accgyro_spi_mpu6500.c \
            drivers/accgyro/accgyro_spi_mpu9250.c \
            drivers/accgyro/accgyro_virtual.c \
            drivers/barometer/barometer_virtual.c \
            drivers/compass/compass_virtual.c

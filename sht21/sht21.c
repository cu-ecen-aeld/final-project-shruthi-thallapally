#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

// I2C device path
#define I2C_DEVICE_PATH "/dev/i2c-1"

// SHT21 I2C address
#define SHT21_ADDRESS 0x40

// SHT21 Commands
#define SHT21_TRIGGER_TEMP_MEASURE_HOLD 0xE3
#define SHT21_TRIGGER_HUMIDITY_MEASURE_HOLD 0xE5
#define SHT21_SOFT_RESET 0xFE

// Function to initialize the SHT21
int sht21_init(int fd) {
    // Send the soft reset command
    if (write(fd, (uint8_t[]){SHT21_SOFT_RESET}, 1) != 1) {
        perror("Error sending soft reset command");
        return -1;
    }
    // Wait for the reset to complete
    usleep(15000);
    return 0;
}

// Function to read raw data from the SHT21
int sht21_read_raw(int fd, uint8_t command, uint16_t *raw_data) {
    uint8_t buffer[3] = {0};

    // Send the measurement command
    if (write(fd, &command, 1) != 1) {
        perror("Error sending measurement command");
        return -1;
    }

    // Wait for measurement to complete
    usleep(85000);

    // Read 3 bytes: 2 data bytes + 1 checksum
    if (read(fd, buffer, 3) != 3) {
        perror("Error reading measurement data");
        return -1;
    }

    // Combine the first two bytes to get the raw data
    *raw_data = (buffer[0] << 8) | buffer[1];

    return 0;
}

// Function to calculate temperature in Celsius
float calculate_temperature(uint16_t raw_temp) {
    return -46.85 + 175.72 * (raw_temp / 65536.0);
}

// Function to calculate relative humidity
float calculate_humidity(uint16_t raw_humidity) {
    return -6.0 + 125.0 * (raw_humidity / 65536.0);
}

int main() {
    int fd;
    uint16_t raw_temp, raw_humidity;
    float temperature, humidity;

    // Open the I2C device
    if ((fd = open(I2C_DEVICE_PATH, O_RDWR)) < 0) {
        perror("Unable to open I2C device");
        return 1;
    }

    // Set the I2C slave address
    if (ioctl(fd, I2C_SLAVE, SHT21_ADDRESS) < 0) {
        perror("Unable to set I2C slave address");
        close(fd);
        return 1;
    }

    // Initialize the SHT21
    if (sht21_init(fd) < 0) {
        close(fd);
        return 1;
    }

    while (1) {
        // Read raw temperature
        if (sht21_read_raw(fd, SHT21_TRIGGER_TEMP_MEASURE_HOLD, &raw_temp) < 0) {
            close(fd);
            return 1;
        }
        // Read raw humidity
        if (sht21_read_raw(fd, SHT21_TRIGGER_HUMIDITY_MEASURE_HOLD, &raw_humidity) < 0) {
            close(fd);
            return 1;
        }

        // Calculate temperature and humidity
        temperature = calculate_temperature(raw_temp);
        humidity = calculate_humidity(raw_humidity);

        // Print results
        printf("Temperature: %.2f °C, Humidity: %.2f %%\n", temperature, humidity);

        // Sleep for 1 second before the next reading
        sleep(1);
    }

    close(fd);
    return 0;
}

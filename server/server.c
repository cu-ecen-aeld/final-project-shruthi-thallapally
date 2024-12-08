/*****************************************************************************
 * Copyright (C) 2024 by Shruthi Thallapally
 *
 * Redistribution, modification or use of this software in source or binary
 * forms is permitted as long as the files maintain this copyright. Users are
 * permitted to modify this and use it to learn about the field of embedded
 * software. Shruthi Thallapally and the University of Colorado are not liable for
 * any misuse of this material.
 *
 *****************************************************************************/
/**
 * @file server.c
 * @brief This file contains functionality of temperature sensor application and sending it to client through socket.
 *
 * To compile: make
 *
 * @author Shruthi Thallapally
 * @date Dec 5 2024
 * @version 1.0
 * @resources sht21 data sheet, aesd socket from assignments
 	      google pages and chatGPT for trouble shooting
              
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <math.h>

#define I2C_DEVICE_PATH "/dev/i2c-1"
#define SHT21_ADDRESS 0x40
#define SHT21_TRIGGER_TEMP_MEASURE_HOLD 0xE3
#define SHT21_TRIGGER_HUMIDITY_MEASURE_HOLD 0xE5
#define SHT21_SOFT_RESET 0xFE

#define PORT 9000
#define BACKLOG 5

int sock_fd = -1;
bool signal_caught = false;

void signal_handler(int signal) {
    if ((signal == SIGINT) || (signal == SIGTERM)) {
        signal_caught = true;
        printf("Signal Caught: %d, exiting...\n", signal);
        close(sock_fd);
        exit(0);
    }
}

int sht21_init(int fd) {
    if (write(fd, (uint8_t[]){SHT21_SOFT_RESET}, 1) != 1) {
        perror("Error sending soft reset command");
        return -1;
    }
    usleep(15000); // Wait for the reset to complete
    return 0;
}

int sht21_read_raw(int fd, uint8_t command, uint16_t *raw_data) {
    uint8_t buffer[3] = {0};

    if (write(fd, &command, 1) != 1) {
        perror("Error sending measurement command");
        return -1;
    }

    usleep(85000); // Wait for measurement to complete

    if (read(fd, buffer, 3) != 3) {
        perror("Error reading measurement data");
        return -1;
    }

    *raw_data = (buffer[0] << 8) | buffer[1];
    return 0;
}

float calculate_temperature(uint16_t raw_temp) {
    return -46.85 + 175.72 * (raw_temp / 65536.0);
}

float calculate_humidity(uint16_t raw_humidity) {
    return -6.0 + 125.0 * (raw_humidity / 65536.0);
}

int main() {
    openlog("server", LOG_PID | LOG_CONS, LOG_USER);
    printf("server main function\n");
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    struct sockaddr_in server, client;
    socklen_t cli_len = sizeof(client);

    int i2c_fd, cli_fd;
    uint16_t raw_temp, raw_humidity;
    float temperature, humidity;

    if ((i2c_fd = open(I2C_DEVICE_PATH, O_RDWR)) < 0) {
        perror("Unable to open I2C device");
        return 1;
    }

    if (ioctl(i2c_fd, I2C_SLAVE, SHT21_ADDRESS) < 0) {
        perror("Unable to set I2C slave address");
        close(i2c_fd);
        return 1;
    }

    if (sht21_init(i2c_fd) < 0) {
        close(i2c_fd);
        return 1;
    }

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("Error creating socket");
        return 1;
    }

    int opt = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    if (bind(sock_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Binding failed");
        close(sock_fd);
        return 1;
    }

    if (listen(sock_fd, BACKLOG) < 0) {
        perror("Listening failed");
        close(sock_fd);
        return 1;
    }

    printf("Waiting for client connections...\n");

    while (!signal_caught) {
        cli_fd = accept(sock_fd, (struct sockaddr *)&client, &cli_len);
        if (cli_fd < 0) {
            perror("Accept failed");
            continue;
        }

        char address[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client.sin_addr, address, sizeof(address));
        printf("Accepted connection from %s:%d\n", address, ntohs(client.sin_port));

        while (!signal_caught) {
            if (sht21_read_raw(i2c_fd, SHT21_TRIGGER_TEMP_MEASURE_HOLD, &raw_temp) < 0 ||
                sht21_read_raw(i2c_fd, SHT21_TRIGGER_HUMIDITY_MEASURE_HOLD, &raw_humidity) < 0) {
                printf("Error reading sensor data\n");
                break;
            }

            temperature = calculate_temperature(raw_temp);
            humidity = calculate_humidity(raw_humidity);

            char message[100];
            snprintf(message, sizeof(message), "Temperature: %.2f°C, Humidity: %.2f%%\n", temperature, humidity);
            send(cli_fd, message, strlen(message), 0);

            sleep(1); // Wait for 1 second before the next reading
        }

        close(cli_fd);
    }

    close(sock_fd);
    close(i2c_fd);
    return 0;
}


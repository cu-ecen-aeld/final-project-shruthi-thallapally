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
#include <arpa/inet.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

#define SHT21_I2C_ADDR 0x40
#define SERVER_PORT 9000
#define MAX_QUEUE 5

int server_socket = -1; // Server socket descriptor
int i2c_file = -1;      // I2C file descriptor
volatile sig_atomic_t termination_flag = false; // Handle exit signals

// Function prototypes
void cleanup_and_exit();
void handle_signal(int sig);
float fetch_temperature();
float fetch_humidity();

void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        termination_flag = true;
        fprintf(stderr, "Signal received: %d, exiting...\n", sig);
        cleanup_and_exit();
        exit(EXIT_SUCCESS);
    }
}

void cleanup_and_exit() {
    if (server_socket > -1) {
        shutdown(server_socket, SHUT_RDWR);
        close(server_socket);
    }
    if (i2c_file > -1) {
        close(i2c_file);
    }
    closelog();
}

float fetch_temperature() {
    uint8_t temp_cmd = 0xE3; // SHT21 command to read temperature
    uint8_t data_buf[2];

    if (write(i2c_file, &temp_cmd, 1) != 1) {
        perror("Error sending temperature command");
        return -999.0;
    }

    if (read(i2c_file, data_buf, 2) != 2) {
        perror("Error reading temperature data");
        return -999.0;
    }

    uint16_t raw_temp = (data_buf[0] << 8) | data_buf[1];
    raw_temp &= ~0x03; // Mask out status bits

    return -46.85 + 175.72 * ((float)raw_temp / 65536.0);
}

float fetch_humidity() {
    uint8_t humid_cmd = 0xE5; // SHT21 command to read humidity
    uint8_t data_buf[2];

    if (write(i2c_file, &humid_cmd, 1) != 1) {
        perror("Error sending humidity command");
        return -999.0;
    }

    if (read(i2c_file, data_buf, 2) != 2) {
        perror("Error reading humidity data");
        return -999.0;
    }

    uint16_t raw_humidity = (data_buf[0] << 8) | data_buf[1];
    raw_humidity &= ~0x03; // Mask out status bits

    return -6.0 + 125.0 * ((float)raw_humidity / 65536.0);
}

int main(int argc, char *argv[]) {
    openlog("sht21_monitor", LOG_PID, LOG_USER);

    // Setup signal handling for graceful termination
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Open the I2C device
    const char *i2c_dev_path = "/dev/i2c-1";
    i2c_file = open(i2c_dev_path, O_RDWR);
    if (i2c_file < 0) {
        perror("Failed to open I2C device");
        return EXIT_FAILURE;
    }

    // Set the I2C slave address for the SHT21 sensor
    if (ioctl(i2c_file, I2C_SLAVE, SHT21_I2C_ADDR) < 0) {
        perror("Failed to set I2C slave address");
        cleanup_and_exit();
        return EXIT_FAILURE;
    }

    // Setup the server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        cleanup_and_exit();
        return EXIT_FAILURE;
    }

    int opt_val = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(SERVER_PORT),
    };

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Socket bind failed");
        cleanup_and_exit();
        return EXIT_FAILURE;
    }

    if (listen(server_socket, MAX_QUEUE) < 0) {
        perror("Error in listen");
        cleanup_and_exit();
        return EXIT_FAILURE;
    }

    fprintf(stdout, "Server is ready and waiting for client connections...\n");

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);

    if (client_socket < 0) {
        perror("Error accepting client connection");
        cleanup_and_exit();
        return EXIT_FAILURE;
    }

    fprintf(stdout, "Client connected. Sending sensor data...\n");

    // Main loop to send sensor data
    while (!termination_flag) {
        float temperature = fetch_temperature();
        float humidity = fetch_humidity();

        if (temperature == -999.0 || humidity == -999.0) {
            fprintf(stderr, "Error reading sensor values\n");
            continue;
        }

        char output[128];
        snprintf(output, sizeof(output), "Temperature: %.2f°C, Humidity: %.2f%%\n", temperature, humidity);
        fprintf(stdout, "%s", output);
        send(client_socket, output, strlen(output), 0);

        sleep(1); // Pause before sending next reading
    }

    cleanup_and_exit();
    return EXIT_SUCCESS;
}


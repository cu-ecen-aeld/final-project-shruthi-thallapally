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
 * @file client.c
 * @brief This file contains functionality of receiving the temperature and humidity sensor values from server.
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <syslog.h>

#define SERVER_IP "192.168.57.181" // the server's IP address
#define SERVER_PORT 9000
#define BUFFER_SIZE 1024

int main() {
    int sock_fd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    int bytes_received;

    openlog("client", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "Client starting...");

    // Create a socket
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("Socket creation failed");
        syslog(LOG_ERR, "Socket creation failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    syslog(LOG_INFO, "Socket created successfully");

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    // Convert and set the server IP address
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid server IP address");
        syslog(LOG_ERR, "Invalid server IP address");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection to server failed");
        syslog(LOG_ERR, "Connection to server failed: %s", strerror(errno));
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
    syslog(LOG_INFO, "Connected to server at %s:%d", SERVER_IP, SERVER_PORT);

    // Communication loop
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);

        // Receive data from the server
        bytes_received = recv(sock_fd, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received < 0) {
            perror("Failed to receive data from server");
            syslog(LOG_ERR, "Failed to receive data: %s", strerror(errno));
            break;
        } else if (bytes_received == 0) {
            printf("Server disconnected\n");
            syslog(LOG_INFO, "Server disconnected");
            break;
        }

        // Null-terminate the received data and print
        buffer[bytes_received] = '\0';
        printf("Received from server: %s\n", buffer);
        syslog(LOG_INFO, "Received data: %s", buffer);

        sleep(1); // Polling interval, adjust as needed
    }

    // Cleanup
    close(sock_fd);
    syslog(LOG_INFO, "Client shutting down");
    closelog();

    return 0;
}


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include "sht21.h" // Assuming you have a header file for SHT21 functions

#define PORT 9000

// Global variable to catch signals
volatile sig_atomic_t signal_caught = 0;

// Signal handler for cleanup
void signal_handler(int signal) {
    signal_caught = 1;
}

int main() {
    int server_fd, cli_fd, i2c_fd;
    struct sockaddr_in server, client;
    socklen_t addr_size;
    char message[128];

    // Open syslog
    openlog("SHT21_Server", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "Server starting...");

    // Set up signal handling
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        syslog(LOG_ERR, "Socket creation failed");
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Bind socket
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr*)&server, sizeof(server)) == -1) {
        syslog(LOG_ERR, "Socket binding failed");
        perror("Socket binding failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_fd, 5) == -1) {
        syslog(LOG_ERR, "Listening failed");
        perror("Listening failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    syslog(LOG_INFO, "Server listening on port %d", PORT);

    // Open I2C device for SHT21
    if ((i2c_fd = open("/dev/i2c-1", O_RDWR)) < 0) {
        syslog(LOG_ERR, "Failed to open I2C device");
        perror("Failed to open I2C device");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    while (!signal_caught) {
        addr_size = sizeof(client);

        // Accept client connection
        if ((cli_fd = accept(server_fd, (struct sockaddr*)&client, &addr_size)) == -1) {
            if (signal_caught) break; // Exit if interrupted by a signal
            syslog(LOG_ERR, "Failed to accept connection");
            perror("Failed to accept connection");
            continue;
        }
        syslog(LOG_INFO, "Accepted connection from %s:%d",
               inet_ntoa(client.sin_addr), ntohs(client.sin_port));

        // Handle client connection
        while (!signal_caught) {
            uint16_t raw_temp, raw_humidity;

            // Read temperature
            if (sht21_read_raw(i2c_fd, SHT21_TRIGGER_TEMP_MEASURE_HOLD, &raw_temp) < 0) {
                syslog(LOG_ERR, "Failed to read temperature");
                sleep(1); // Retry after delay
                continue;
            }

            // Read humidity
            if (sht21_read_raw(i2c_fd, SHT21_TRIGGER_HUMIDITY_MEASURE_HOLD, &raw_humidity) < 0) {
                syslog(LOG_ERR, "Failed to read humidity");
                sleep(1); // Retry after delay
                continue;
            }

            // Format data into a message
            snprintf(message, sizeof(message), "Temperature: %.2f°C, Humidity: %.2f%%\n",
                     sht21_convert_temp(raw_temp), sht21_convert_humidity(raw_humidity));

            // Send data to client
            if (send(cli_fd, message, strlen(message), 0) == -1) {
                syslog(LOG_ERR, "Failed to send data to client");
                perror("Send failed");
                break;
            }

            sleep(2); // Throttle data sending to the client
        }

        // Close client connection
        close(cli_fd);
        syslog(LOG_INFO, "Closed connection to client");
    }

    // Cleanup and exit
    syslog(LOG_INFO, "Shutting down server...");
    close(i2c_fd);
    close(server_fd);
    closelog();

    return 0;
}


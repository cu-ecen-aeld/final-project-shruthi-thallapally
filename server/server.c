#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <signal.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

// Constants for SHT21 sensor commands
#define SHT21_TRIGGER_TEMP_MEASURE_HOLD  0xE3
#define SHT21_TRIGGER_HUMIDITY_MEASURE_HOLD  0xE5

// Function prototypes
int read_raw_data(int i2c_fd, uint8_t command, uint16_t *data);
float convert_temp(uint16_t raw_temp);
float convert_humidity(uint16_t raw_humidity);
void handle_sigint(int sig);

volatile int keep_running = 1;

// Signal handler for graceful shutdown
void handle_sigint(int sig) {
    printf("Server shutting down...\n");
    syslog(LOG_INFO, "Server shutting down");
    keep_running = 0;
}

int main(int argc, char *argv[]) {
    openlog("server", LOG_PID | LOG_CONS, LOG_USER);
    printf("Starting server...\n");
    syslog(LOG_INFO, "Server starting...");

    // Handle SIGINT for graceful shutdown
    signal(SIGINT, handle_sigint);

    // Initialize the I2C interface
    const char *i2c_device = "/dev/i2c-1"; // Adjust if necessary
    int i2c_fd = open(i2c_device, O_RDWR);
    if (i2c_fd < 0) {
        perror("Unable to open I2C device");
        syslog(LOG_ERR, "Unable to open I2C device");
        return EXIT_FAILURE;
    }

    // Set the I2C address of the SHT21 sensor
    int sht21_address = 0x40; // SHT21 I2C address
    if (ioctl(i2c_fd, I2C_SLAVE, sht21_address) < 0) {
        perror("Failed to set I2C address");
        syslog(LOG_ERR, "Failed to set I2C address");
        close(i2c_fd);
        return EXIT_FAILURE;
    }

    // Initialize the server socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        syslog(LOG_ERR, "Socket creation failed");
        close(i2c_fd);
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9000);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Socket bind failed");
        syslog(LOG_ERR, "Socket bind failed");
        close(server_fd);
        close(i2c_fd);
        return EXIT_FAILURE;
    }

    if (listen(server_fd, 5) < 0) {
        perror("Socket listen failed");
        syslog(LOG_ERR, "Socket listen failed");
        close(server_fd);
        close(i2c_fd);
        return EXIT_FAILURE;
    }

    printf("Server is listening on port 9000...\n");
    syslog(LOG_INFO, "Server is listening on port 9000");

    while (keep_running) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            if (keep_running) {
                perror("Socket accept failed");
                syslog(LOG_ERR, "Socket accept failed");
            }
            break;
        }

        printf("Client connected\n");
        syslog(LOG_INFO, "Client connected");

        // Read temperature and humidity
        uint16_t raw_temp, raw_humidity;
        if (read_raw_data(i2c_fd, SHT21_TRIGGER_TEMP_MEASURE_HOLD, &raw_temp) < 0 ||
            read_raw_data(i2c_fd, SHT21_TRIGGER_HUMIDITY_MEASURE_HOLD, &raw_humidity) < 0) {
            printf("Failed to read sensor data\n");
            syslog(LOG_ERR, "Failed to read sensor data");
            close(client_fd);
            continue;
        }

        float temp = convert_temp(raw_temp);
        float humidity = convert_humidity(raw_humidity);

        char response[128];
        snprintf(response, sizeof(response), "Temperature: %.2f C, Humidity: %.2f%%\n", temp, humidity);

        // Send data to client
        send(client_fd, response, strlen(response), 0);
        printf("Sent data to client: %s", response);
        syslog(LOG_INFO, "Sent data to client: %s", response);

        close(client_fd);
    }

    close(server_fd);
    close(i2c_fd);
    printf("Server terminated\n");
    syslog(LOG_INFO, "Server terminated");
    closelog();

    return EXIT_SUCCESS;
}

// Read raw data from the SHT21 sensor
int read_raw_data(int i2c_fd, uint8_t command, uint16_t *data) {
    if (write(i2c_fd, &command, 1) != 1) {
        perror("Failed to write to I2C device");
        syslog(LOG_ERR, "Failed to write to I2C device");
        return -1;
    }

    usleep(50000); // Delay for sensor measurement (50 ms)

    uint8_t buffer[2];
    if (read(i2c_fd, buffer, 2) != 2) {
        perror("Failed to read from I2C device");
        syslog(LOG_ERR, "Failed to read from I2C device");
        return -1;
    }

    *data = (buffer[0] << 8) | buffer[1];
    *data &= ~0x0003; // Mask off the status bits
    return 0;
}

// Convert raw temperature data to Celsius
float convert_temp(uint16_t raw_temp) {
    return -46.85 + 175.72 * raw_temp / 65536.0;
}

// Convert raw humidity data to percentage
float convert_humidity(uint16_t raw_humidity) {
    return -6.0 + 125.0 * raw_humidity / 65536.0;
}


import smbus_cffi as smbus  
import time  

# Get I2C bus
bus = smbus.SMBus(1)

# Address of SHT21 sensor
device = 0x40  

while True:
    try:
        # Temperature measurement
        time.sleep(0.1)  # Wait for the sensor to stabilize
        data = bus.read_i2c_block_data(device, 0xE3)  

        big_int = (data[0] << 8) | data[1]  # Combine bytes
        temperature = ((big_int / 65536) * 175.72) - 46.85  

        # Humidity measurement
        time.sleep(0.1)  # Wait for the sensor to stabilize
        data = bus.read_i2c_block_data(device, 0xE5)  

        big_int = (data[0] << 8) | data[1]  # Combine bytes
        humidity = ((big_int / 65536) * 125) - 6  

        # Compensated humidity
        compensated_humidity = (25 - temperature) * 0.15 + humidity  

        print(f"Temperature: {temperature:.2f}°C")
        print(f"Humidity: {humidity:.2f}%")
        print(f"Compensated Humidity: {compensated_humidity:.2f}%")

    except OSError as e:
        print(f"I2C Error: {e}")
        time.sleep(1)


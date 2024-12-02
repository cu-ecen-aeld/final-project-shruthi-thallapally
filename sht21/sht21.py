import smbus
import time

# Get I2C bus
bus = smbus.SMBus(1)

# Address: HTU21D/SHT21
device = 0x40

while True:
    try:
        # Temperature
        data = bus.read_i2c_block_data(device, 0xE3)
        big_int = (data[0] * 256) + data[1]  # Combine both bytes into one big integer
        temperature = ((big_int / 65536) * 175.72) - 46.85  # Formula from datasheet

        # Humidity
        data = bus.read_i2c_block_data(device, 0xE5)
        big_int = (data[0] * 256) + data[1]
        humidity = ((big_int / 65536) * 125) - 6

        # Print results
        print('Temperature: {:.2f}°C'.format(temperature))
        print('Humidity: {:.2f}%'.format((25 - temperature) * 0.15 + humidity))
    
    except OSError as e:
        print(f"I2C Error: {e}")
    
    except Exception as e:
        print(f"Unexpected Error: {e}")
    
    time.sleep(1)  # Wait before the next read


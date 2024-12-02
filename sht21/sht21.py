from smbus2 import SMBus

bus = SMBus(1)  # Raspberry Pi I2C bus
address = 0x40  # SHT21 I2C address

try:
    # Send a temperature measurement command (0xF3 for SHT21)
    bus.write_byte(address, 0xF3)
    print("Command sent successfully!")
except Exception as e:
    print(f"I²C Error: {e}")
finally:
    bus.close()


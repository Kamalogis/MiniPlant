import serial
from pymodbus.client import ModbusTcpClient
from time import sleep

# Konstanta
START_BYTE = 0xAA
PACKET_LENGTH = 10
SERIAL_PORT = '/dev/ttyUSB0'
BAUDRATE = 9600
IP_PLC = "" 

def parse_flags(flag_byte):
    # byte do data flag
    return [(flag_byte >> i) & 1 for i in range(8)]

def calculate_checksum(packet):
    checksum = 0
    for byte in packet[:-1]:
        checksum ^= byte
    return checksum

def process_packet(packet, debug=False):
    if len(packet) != PACKET_LENGTH or packet[0] != START_BYTE:
        print("Paket salah: Panjang atau start byte tidak valid")
        return None

    if calculate_checksum(packet) != packet[-1]:
        print("Checksum tidak cocok!")
        return None

    #Parsing 
    level_1 = packet[1]
    level_2 = packet[2]
    tds_1 = packet[3]
    flow_1 = packet[4]
    pressure_1 = packet[5]
    input_flags = parse_flags(packet[6])
    output_flags = parse_flags(packet[7])
    output_flags2 = parse_flags(packet[8])

    if debug:
        print(f"Level 1: {level_1}, Level 2: {level_2}, TDS: {tds_1}, Flow: {flow_1}, Pressure: {pressure_1}")
        print(f"Input Flags: {input_flags}")
        print(f"Output Flags: {output_flags}, Output Flags2: {output_flags2}")

    return {
        'level_1': level_1,
        'level_2': level_2,
        'tds_1': tds_1,
        'flow_1': flow_1,
        'pressure_1': pressure_1,
        'input_flags': input_flags,
        'output_flags': output_flags,
        'output_flags2': output_flags2
    }

def upload_to_database(data):
    #upload data ke database
    return None

def upload_to_plc(data):
    #upload data ke memory PLC
    return None

def override_command(client):
    #read memory PLC untuk override 
    return None

def data_plc():
    #read data aktuator dari PLC
    return None

def main():
    try:
        ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=1)
        print("Berhasil terhubung ke port serial")
    except:
        print(f"Gagal terhubung ke serial")
        return

    client = ModbusTcpClient(IP_PLC)
    if client.connect():
        print("Berhasil terhubung ke PLC")
    else:
        print("Gagal terhubung ke PLC")

    try:
        while True:
            try:
                if ser.in_waiting >= PACKET_LENGTH:
                    packet = ser.read(PACKET_LENGTH)
                    data = process_packet(packet, debug=True)

                    if data:
                        upload_to_database(data)
                        upload_to_plc(data)

                        if override_command(client) and data['input_flags'][2]:
                            print("=== Mode Override Aktif ===")
                            plc_data = data_plc()
                            ser.write(bytes([0xBB]) + plc_data)
                    else:
                        print("Paket rusak")

                sleep(0.5) 
            except Exception as e:
                print(f"Error dalam loop: {e}")
                sleep(1)

    except KeyboardInterrupt:
        print("Program dihentikan")
    finally:
        ser.close()
        client.close()
        print("Koneksi serial dan PLC ditutup")

if __name__ == "__main__":
    main()
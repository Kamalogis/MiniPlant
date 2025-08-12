import serial
from pymodbus.client import ModbusTcpClient
from time import sleep
import os
import platform

#Konstan
START_BYTE = 0xAA
PACKET_LENGTH = 10
SERIAL_PORT = 'COM3'
BAUDRATE = 9600
IP_PLC = "10.10.17.210" 
LOG = False

FLAGS_INPUT = [
    "level_switch", "pb_start", "mode_standby", "mode_filtering", "mode_backwash",
    "mode_drain", "mode_override", "emergency_stop" 
]
FLAGS_OUTPUT = [
    "solenoid_1", "solenoid_2", "solenoid_3", "solenoid_4", "solenoid_5", "solenoid_6", "pompa_1", "pompa_2",
]
FLAGS_OUTPUT_2= [
    "pompa_3", "standby_lamp", "filtering_lamp", "backwash_lamp", "drain_lamp", "stepper"
]

"""
========INPUT========== ()
int level_1;            (1)
int level_2;            (2)
int tds_1;              (3)
int flow_1;             (4)
int pressure_1;         (5)
bool level_switch;      (6:0)
bool pb_start;          (6:1)
bool mode_standby;      (6:2)
bool mode_filtering;    (6:3)
bool mode_backwash;     (6:4)
bool mode_drain;        (6:5)
bool mode_override;     (6:6)
bool emergency_stop;    (6:7)

========OUTPUT========== (data 7-8)
bool solenoid_1;        (7:0)
bool solenoid_2;        (7:1)
bool solenoid_3;        (7:2)
bool solenoid_4;        (7:3)
bool solenoid_5;        (7:4)
bool solenoid_6;        (7:5)
bool pompa_1;           (7:6)
bool pompa_2;           (7:7)
bool pompa_3;           (8:0)
bool standby_lamp;      (8:1)
bool filtering_lamp;    (8:2)
bool backwash_lamp;     (8:3)
bool drain_lamp;        (8:4)
bool stepper_pulse;     (8:5)
bool stepper_en;        (8:6)
bool stepper_dir;       (8:7)
"""

#================== CONNECTION ==================
def connect_serial(port=SERIAL_PORT, baud=BAUDRATE):
    try:
        ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=1)
        print("Berhasil terhubung ke port serial")
        return ser
    except:
        print(f"Gagal terhubung ke serial")
        return False

def connect_PLC(ip=IP_PLC):
    client = ModbusTcpClient(IP_PLC)
    if client.connect():
        print("Berhasil terhubung ke PLC")
        return client
    else:
        print("Gagal terhubung ke PLC")
        return False


#========================== PARSING AND PACKING ==========================
def parse_flags(flag_byte):
    # byte do data flag
    return [(flag_byte >> i) & 1 for i in range(8)]

def flags_to_bytes(flags):
    if len(flags) == 16:
        byte1 = sum((bit << i) for i, bit in enumerate(flags[:8]))
        byte2 = sum((bit << i) for i, bit in enumerate(flags[8:]))
        return byte1, byte2
    else:
        return False

def calculate_checksum(packet):
    checksum = 0
    for byte in packet[:-1]:
        checksum ^= byte
    return checksum

def process_packet(packet, override, debug=False):
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
    output_flags2 = (parse_flags(packet[8]))[:6]
    if not LOG:
        if platform.system() == "Windows":
            os.system('cls')
        else:
            os.system('clear')

    if debug == "Simple":
        print("="*40)
        print(f"Level 1: {level_1}, Level 2: {level_2}, TDS: {tds_1}, Flow: {flow_1}, Pressure: {pressure_1}")
        print(f"Input Flags: {input_flags}")
        print(f"Output Flags: {output_flags}\nOutput Flags2: {output_flags2}")
        print("="*40)
    elif debug == "All":
        print("="*40)
        print(f"Level 1: {level_1}, Level 2: {level_2}, TDS: {tds_1}, Flow: {flow_1}, Pressure: {pressure_1}")
        for i, flag in enumerate(FLAGS_INPUT):
            print(f"{flag.ljust(15)} : {'Nyala' if input_flags[i] == 1 else 'Mati'}")
        for i, flag in enumerate(FLAGS_OUTPUT):
            print(f"{flag.ljust(15)} : {'Nyala' if output_flags[i] == 1 else 'Mati'}")
        for i, flag in enumerate(FLAGS_OUTPUT_2):
            print(f"{flag.ljust(15)} : {'Nyala' if output_flags2[i] == 1 else 'Mati'}")
        print("="*40)
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

#========================== UPLOAD DATA ==========================
def upload_to_database(data):
    #upload data ke database
    return None

def upload_to_plc(data, client, override):
    #upload data ke memory PLC
    client.write_registers(0, list(data.values())[0:5], slave=1)
    if override:
        print()
    else:
        client.write_coils(0, data['input_flags'], slave=2)
        client.write_coils(8, data['output_flags'], slave=2)
        client.write_coils(16, data['output_flags2'], slave=2)
    return None


#========================== OVERRIDE ==========================
def override_command(client):
    #read memory PLC untuk override
    return (client.read_coils(6, slave=2).bits)[0]

def data_plc(client):
    #read data aktuator dari PLC untuk override
    data = (client.read_coils(8,count=14, slave=2).bits)
    flag_one, flag_two = flags_to_bytes(data)
    packet = bytearray()
    packet.append(0xBB)
    packet.append(flag_one)
    packet.append(flag_two)
    # print(list(packet))
    return packet


#========================== MAIN ==========================
def main(debug):
    ser = connect_serial()
    if not ser: 
        print("Tidak dapat melanjutkan tanpa koneksi serial.")
        return 

    plc_client = connect_PLC()
    if not plc_client:
        print("Tidak dapat melanjutkan tanpa koneksi PLC.")
        ser.close() 
        return 
    
    try:
        while True:
            try:
                if ser.in_waiting >= PACKET_LENGTH:
                    packet = ser.read(PACKET_LENGTH)
                    ser.flush()
                    data = process_packet(packet, override_command(plc_client), debug)

                    if data:
                        upload_to_plc(data, plc_client, override_command(plc_client))
                        upload_to_database(data)

                        if override_command(plc_client):
                            print("================== MODE OVERRIDE AKTIF ==================\n")
                            ser.write(data_plc(plc_client))
                            ser.flush()
                        else:
                            ser.write(0xFF)
                            ser.flush()
                    else:
                        print("Paket rusak")

                # sleep(0.2)
            except Exception as e:
                print(f"Error dalam loop: {e}")
                sleep(1)

    except KeyboardInterrupt:
        print("Program dihentikan")
    finally:
        ser.close()
        plc_client.close()
        print("Koneksi serial dan PLC ditutup")

if __name__ == "__main__":
    debug_input = input("Debug?  ")
    if debug_input ==" ":
        debug_input = "Simple"
    elif debug_input == "":
        debug_input = "All"
    else:
        debug_input = False
    main(debug_input)
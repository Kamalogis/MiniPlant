import serial
from pymodbus.client import ModbusTcpClient
from time import sleep

#Konstan
START_BYTE = 0xAA
PACKET_LENGTH = 10
SERIAL_PORT = 'COM6'
BAUDRATE = 9600
IP_PLC = "127.0.0.1" 

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
        print("="*40)
        print(f"Level 1: {level_1}, Level 2: {level_2}, TDS: {tds_1}, Flow: {flow_1}, Pressure: {pressure_1}")
        print(f"Input Flags: {input_flags}")
        print(f"Output Flags: {output_flags}\nOutput Flags2: {output_flags2}\n")

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

def upload_to_plc(data, client, override):
    #upload data ke memory PLC
    # client.write_register(0, data['level_1'])
    # client.write_register(1, data['level_2'])
    # client.write_register(2, data['tds_1'])
    # client.write_register(3, data['flow_1'])
    # client.write_register(4, data['pressure_1'])
    client.write_registers(0, list(data.values())[0:4])
    if not override:
        client.write_coils(0, data['input_flags'], slave=3)
        client.write_coils(0, data['output_flags'], slave=2)
        client.write_coils(8, data['output_flags2'], slave=2)
    else:
        print("Override Command Aktif")
    return None

def override_command(client):
    #read memory PLC untuk override
    return (client.read_coils(16, slave=2).bits)[0]

def data_plc(client):
    #read data aktuator dari PLC
    data = (client.read_coils(0,count=16, slave=2).bits)
    flag_one, flag_two = flags_to_bytes(data)

    packet = bytearray()
    packet.append(0xBB)
    packet.append(flag_one)
    packet.append(flag_two)
    return packet

def main(debug):
    try:
        ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=1)
        print("Berhasil terhubung ke port serial")
    except:
        print(f"Gagal terhubung ke serial")
        return

    plc_client = ModbusTcpClient(IP_PLC)
    if plc_client.connect():
        print("Berhasil terhubung ke PLC")
    else:
        print("Gagal terhubung ke PLC")

    try:
        while True:
            try:
                if ser.in_waiting >= PACKET_LENGTH:
                    packet = ser.read(PACKET_LENGTH)
                    data = process_packet(packet, debug)

                    if data:
                        upload_to_plc(data, plc_client, override_command(plc_client))
                        upload_to_database(data)

                        if override_command(plc_client) and data['input_flags'][2]:
                            print("=== Mode Override Aktif ===")
                            ser.write(data_plc(plc_client))
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
        plc_client.close()
        print("Koneksi serial dan PLC ditutup")

if __name__ == "__main__":
    debug_input = input("Debug [Y/n] ? ").strip().lower()
    if debug_input in ["y",""]:
        debug = True
    elif debug_input in ["n"]:
        debug = False
    else:
        debug = True
    main(debug)
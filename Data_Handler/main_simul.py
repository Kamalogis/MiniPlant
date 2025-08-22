import serial
from pymodbus.client import ModbusTcpClient
from time import sleep
import os
import platform

#Konstan
START_BYTE = 0xAA
PACKET_LENGTH = 10
# SERIAL_PORT = '/dev/ttyUSB0'
SERIAL_PORT = '/dev/serial0'
BAUDRATE = 115200
# IP_PLC = "10.10.17.210" 
IP_PLC = "192.168.0.101"
LOG = False
DEBUG = True

"""
======================================================================================================
                                        DOKUMENTASI KODE
======================================================================================================
1. TUJUAN PROGRAM
    Program ini digunakan untuk menghubungkan mikrokontroler (via serial) dengan PLC 
    namun hanya untuk simulasi dengan modbusPoll dengan konfigurasi id 1 untuk memory
    word, dan id 2 untuk memory bit. program ini bertujuan untuk keperluan monitoring 
    dan kontrol pada sistem Water Treatment Plant (WTP) by KAMALOGIS, dengan Fungsi:
        1) Membaca data sensor & status input dari mikrokontroler (serial).
        2) Menyimpan data ke memory PLC dan ke database SQLite.
        3) Mendukung mode override, di mana kontrol dilakukan dari PLC dan 
           bukan dari mikrokontroler.

2. STRKTUR DATA DAN KOMUNIKASI 
    Struktur Data dari mikrokontroler ke program:
        Byte ke | Data
        0       | START_BYTE (0xAA)
        1       | level_1 (int)
        2       | level_2 (int)
        3       | tds_1 (int)
        4       | flow_1 (int)
        5       | pressure_1 (int)
        6       | Input Flags (8 bit)
        7       | Output Flags (8 bit)
        8       | Output Flags 2 (8 bit)
        9       | Checksum (XOR dari byte 0-8)

        Input Flags (byte 6) berisi:
            level_switch, pb_start, mode_standby, mode_filtering, mode_backwash, 
            mode_drain, mode_override, emergency_stop.

        Output Flags (byte 7) berisi:
            solenoid_1 - solenoid_6, pompa_1, pompa_2.

        Output Flags 2 (byte 8) berisi:
            pompa_3, standby_lamp, filtering_lamp, backwash_lamp, drain_lamp, stepper.
    
    Struktur Data dari program ke mikrokontroler:
        1) Mode Normal (bukan override)
            Hanya mengirim 1 byte 0xFF dengan fungsi sebagai acknowledge bahwa paket 
            sudah diterima, serta memberi tahu mikrokontroler untuk lanjut mengirim 
            data berikutnya.

        2) Mode Override (kontrol dari PLC)
            Program mengirim 3 byte paket:

            Byte | Isi	    | Keterangan
            0	 | 0xBB	    | Start byte untuk override
            1	 | flag_one	| 8 bit pertama hasil pembacaan coil dari PLC
            2	 | flag_two	| 8 bit kedua hasil pembacaan coil dari PLC

        3) Alur Komunikasi
        [ Mikrokontroler ]  --(10 byte paket data)-->  [ Program Python ]  
            ^                                               |
            |                                               |
          +--(0xFF jika normal / 0xBB+2 byte jika override)---+

3. FUNGSI-FUNGSI DALAM PROGRAM
    A) Koneksi
        1) connect_serial(port, baud)
            Membuka koneksi serial ke mikrokontroler.
        2) connect_PLC(ip)
            Membuka koneksi Modbus TCP ke PLC.

    B) Parsing & Packing
        1) parse_flags(flag_byte)
            Memecah satu byte menjadi list bit [b0, b1, ..., b7].
        2) flags_to_bytes(flags)
            Menggabungkan list 16 flag menjadi 2 byte.
        3) calculate_checksum(packet)
            Hitung checksum dengan XOR.
        4) process_packet(packet, override, debug)
            Validasi paket, parsing data sensor & flags, menampilkan ke layar 
            (mode simple/all), lalu mengembalikan dictionary data.

    C) Upload Data
        1) upload_to_database(data, client)
            Menyimpan data ke database SQLite data_wtp.db.
        2) upload_to_plc(data, client, override)
            Menulis data sensor dan flags ke PLC (kecuali override aktif).

    D) Override Mode
        1) override_command(client)
            Membaca status mode override dari PLC.
        2) data_plc(client)
            Membaca status output dari PLC dan membentuk paket untuk dikirim kembali ke mikrokontroler.

    E) Main Loop
        1. Membaca paket dari serial.
        2. Parsing dan validasi.
        3. Upload data ke PLC / database.
        4. Jika override aktif → kirim data dari PLC ke mikrokontroler.
        5. Jika override non-aktif → kirim byte 0xFF sebagai ACK.

4. ALUR PROGRAM
    Buka koneksi serial dan PLC.
    Loop utama:
        Cek apakah ada data masuk dari serial (>= 10 byte).
        Parsing paket → process_packet().
        Simpan/Upload data:
            Jika override aktif → mikrokontroler mengikuti PLC.
            Jika override non-aktif → PLC mengikuti mikrokontroler.
            Simpan data ke SQLite.
        Jika ada error, tunggu sebentar lalu lanjutkan loop.
    Tutup koneksi ketika program dihentikan (Ctrl+C).

======================================================================================================

======================================================================================================
"""

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
def upload_to_database(data, client):
    #upload data ke database
    input_plc = (client.read_coils(0,count=5, slave=2).bits)
    try:
        for i, flag in enumerate(FLAGS_INPUT):
            data[flag] = input_plc[i]
        for i, flag in enumerate(FLAGS_OUTPUT):
            data[flag] = data['output_flags'][i]
        for i, flag in enumerate(FLAGS_OUTPUT_2):
            data[flag] = data['output_flags2'][i]
        timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        conn = sqlite3.connect('data_wtp.db')
        c = conn.cursor()
        c.execute("""
        CREATE TABLE IF NOT EXISTS monitor_wtp (
            timestamp TEXT,
            level_1 INTEGER,
            level_2 INTEGER,
            tds_1 INTEGER,
            flow_1 INTEGER,
            pressure_1 INTEGER,
            level_switch INTEGER,
            mode_standby INTEGER,
            mode_filtering INTEGER,
            mode_backwash INTEGER,
            mode_drain INTEGER,
            mode_override INTEGER,
            emergency_stop INTEGER,
            solenoid_1 INTEGER,
            solenoid_2 INTEGER,
            solenoid_3 INTEGER,
            solenoid_4 INTEGER,
            solenoid_5 INTEGER,
            solenoid_6 INTEGER,
            pompa_1 INTEGER,
            pompa_2 INTEGER,
            pompa_3 INTEGER,
            stepper INTEGER
        )
        """)

        c.execute("""INSERT INTO monitor_wtp (
        timestamp,
        level_1,
        level_2,
        tds_1,
        flow_1,
        pressure_1,
        level_switch,
        mode_standby,
        mode_filtering,
        mode_backwash,
        mode_drain,
        mode_override,
        emergency_stop,
        solenoid_1,
        solenoid_2,
        solenoid_3,
        solenoid_4,
        solenoid_5,
        solenoid_6,
        pompa_1,
        pompa_2,
        pompa_3,
        stepper
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?,?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""", 
        (
            timestamp, 
            data["level_1"], 
            data["level_2"], 
            data["tds_1"], 
            data["flow_1"], 
            data["pressure_1"], 
            data["level_switch"], 
            data["mode_standby"], 
            data["mode_filtering"], 
            data["mode_backwash"], 
            data["mode_drain"], 
            data["mode_override"], 
            data["emergency_stop"], 
            data["solenoid_1"], 
            data["solenoid_2"], 
            data["solenoid_3"], 
            data["solenoid_4"], 
            data["solenoid_5"], 
            data["solenoid_6"], 
            data["pompa_1"], 
            data["pompa_2"], 
            data["pompa_3"],
            data["stepper"]))
        conn.commit()
        conn.close()
        return print("Data Tersimpan di Database  ", timestamp)
    except Exception as e:
        print(f"Terjadi Kesalahan dalam Menyimpan Data: {e}")
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
                        # upload_to_database(data)

                        if override_command(plc_client):
                            print("================== MODE OVERRIDE AKTIF ==================\n")
                            ser.write(data_plc(plc_client))
                            # ser.flush()
                        else:
                            ser.write(bytes([0xFF]))
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
    main(DEBUG)

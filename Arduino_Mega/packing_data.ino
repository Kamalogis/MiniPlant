struct InputState {
  int level_1;
  int level_2;
  int tds_1;
  int flow_1;
  int pressure_1;
  bool level_switch;
  bool pb_start;
  bool mode_standby;
  bool mode_filtering;
  bool mode_backwash;
  bool mode_drain;
  bool mode_override;
  bool emergency_stop;
};

struct OutputState {
  bool solenoid_1;
  bool solenoid_2;
  bool solenoid_3;
  bool solenoid_4;
  bool solenoid_5;
  bool solenoid_6;
  bool pompa_1;
  bool pompa_2;
  bool pompa_3;
  bool standby_lamp;
  bool filtering_lamp;
  bool backwash_lamp;
  bool drain_lamp;
  bool stepper;
};

InputState input;
OutputState output;

uint8_t data[10];

const byte PACKET_LENGTH = 3; 
const byte START_BYTE = 0xBB;

byte packet[PACKET_LENGTH];
byte index = 0;
bool receiving = false;

void checkSerial() {
  while (Serial.available()) {
    byte b = Serial.read();
    Serial.flush();

    if (!receiving) {
      if (b == START_BYTE) {
        receiving = true;
        index = 0;
        packet[index++] = b;
      } 
    } else {
      packet[index++] = b;
      if (index == PACKET_LENGTH) {
        receiving = false;
        processPacket(packet); 
      }
    }
  } 
}

bool validatePacket(byte *pkt) {
  byte checksum = pkt[0] ^ pkt[1] ^ pkt[2];
  return (checksum == pkt[3]);
}

void processPacket(byte *pkt) {
  byte flags1 = pkt[1];
  byte flags2 = pkt[2];

  // Contoh: pembacaan bit
  output.solenoid_1 = bitRead(flags1, 0);
  output.solenoid_2 = bitRead(flags1, 1);
  output.solenoid_3 = bitRead(flags1, 2);
  output.solenoid_4 = bitRead(flags1, 3);
  output.solenoid_5 = bitRead(flags1, 4);
  output.solenoid_6 = bitRead(flags1, 5);
  output.pompa_1 = bitRead(flags1, 6);
  output.pompa_2 = bitRead(flags1, 7);
  output.pompa_3 = bitRead(flags2, 0);
  output.standby_lamp = bitRead(flags2, 1);
  output.filtering_lamp = bitRead(flags2, 2);
  output.backwash_lamp = bitRead(flags2, 3);
  output.drain_lamp = bitRead(flags2, 4);
  output.stepper = bitRead(flags2, 5);
}

void tambah() {
  input.level_1++;
  if (input.level_1 > 100) {
    input.level_1 = 0;
  }
}

void data_awal() {
  input.level_1 = 120;
  input.level_2 = 85;
  input.tds_1 = 33;
  input.flow_1 = 200;
  input.pressure_1 = 60;
  input.level_switch = true;
  input.pb_start = false;
  input.mode_standby = true;
  input.mode_filtering = false;
  input.mode_backwash = false;
  input.mode_drain = false;
  input.mode_override = false;
  input.emergency_stop = false;

  //Output
  output.solenoid_1 = true;
  output.solenoid_2 = true;
  output.solenoid_3 = false;
  output.solenoid_4 = false;
  output.solenoid_5 = false;
  output.solenoid_6 = true;
  output.pompa_1 = true;
  output.pompa_2 = false;
  output.pompa_3 = true;
  output.standby_lamp = false;
  output.filtering_lamp = false;
  output.backwash_lamp = false;
  output.drain_lamp = true;
  output.stepper = true;
}

void packing(){
  //======================PACKING==============================
  data[0] = 0xAA;  // start byte

  //=====================INPUT INT=========================
  data[1] = input.level_1;
  data[2] = input.level_2;
  data[3] = input.tds_1;
  data[4] = input.flow_1;
  data[5] = input.pressure_1;

  //=======================INPUT BOOL=========================
  uint8_t input_flags = 0;
  input_flags |= input.level_switch << 0;
  input_flags |= input.pb_start << 1;
  input_flags |= input.mode_standby << 2;
  input_flags |= input.mode_filtering << 3;
  input_flags |= input.mode_backwash << 4;
  input_flags |= input.mode_drain << 5;
  input_flags |= input.mode_override << 6;
  input_flags |= input.emergency_stop << 7;
  data[6] = input_flags;

  //======================OUTPUT=========================
  uint8_t output_flags = 0;
  output_flags |= output.solenoid_1 << 0;
  output_flags |= output.solenoid_2 << 1;
  output_flags |= output.solenoid_3 << 2;
  output_flags |= output.solenoid_4 << 3;
  output_flags |= output.solenoid_5 << 4;
  output_flags |= output.solenoid_6 << 5;
  output_flags |= output.pompa_1 << 6;
  output_flags |= output.pompa_2 << 7;
  data[7] = output_flags;

  uint8_t output_flags2 = 0;
  output_flags2 |= output.pompa_3 << 0;
  output_flags2 |= output.standby_lamp << 1;
  output_flags2 |= output.filtering_lamp << 2;
  output_flags2 |= output.backwash_lamp << 3;
  output_flags2 |= output.drain_lamp << 4;
  output_flags2 |= output.stepper << 5;
  data[8] = output_flags2;

  //======================VALIDASI=========================
  uint8_t validasi = 0;
  for (int i = 0; i < 9; i++) {
    validasi ^= data[i];
  }
  data[9] = validasi;
}

void setup() {
  Serial.begin(9600);
  data_awal(); //Ceritanya data masuk
}

void loop() {
  tambah();
  packing();
  Serial.write(data, 10);
  Serial.flush();
  checkSerial();
  delay(200);
}
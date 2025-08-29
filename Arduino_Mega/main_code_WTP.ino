// struktur kode:
// 1. define input output variable
// 2. define i/o pins
// 3. define FSM
// 4. setup
// 5. main loop
// 6. functions

// for stepper
// #include <AccelStepper.h>

// for I2C
#include <Wire.h>

// pressure transmitter 0x10
// flow transmitter 0x11
// level transmitter 1 0x12
// level transmitter 2 0x13
// quality transmitter 0x14
uint8_t pressure_transmitter_address = 0x10; // address for pressure transmitter
uint8_t flow_transmitter_address = 0x11; // address for flow transmitter
uint8_t level1_transmitter_address = 0x13; // address for level transmitter
uint8_t level2_transmitter_address = 0x13; // address for level transmitter
uint8_t quality_transmitter_address = 0x14; // address for quality transmitter

// setpoints:
int SETPOINT_ATAS_TANGKI_AGITATOR = 70; // 80 percent
int SETPOINT_BAWAH_TANGKI_AGITATOR = 20; // 20 percent
int LEVEL_TANGKI_STORAGE_KOSONG = 5; // 5 percent height for empty storage tank

unsigned long WAKTU_ADUK = 5000; // 2 minutes
unsigned long WAKTU_ENDAPAN = 5000; // 3 minutes

unsigned long ENDAPAN_LED_ON_TIME = 500; // led on for 1 s when in ENDAPAN process
unsigned long ENDAPAN_LED_OFF_TIME = 500; // led off for 0.5 s when in ENDAPAN process

unsigned long EMERGENCY_LED_ON_TIME = 200; // led on for 1 s when in emergency mode
unsigned long EMERGENCY_LED_OFF_TIME = 200; // led off for 0.75 s when in emergency mode

unsigned long DRAIN_LED_ON_TIME = 1000; // led on for 1 s when in drain mode
unsigned long DRAIN_LED_OFF_TIME = 600; // led off for 0.6 s when in drain mode

unsigned long DELAY_POMPA = 1000; // delay when pump is about to be turned on (for safety)
unsigned long WAKTU_DOSING = 5000; // dosing time

unsigned long JEDA_FILTERING_BERULANG = 10000; // repeat filtering if in 10 s no other command

// Flag to ensure the dosing logic (triggered by millis timing) runs only once per activation,
// preventing repeated execution during continuous loop cycles.
bool DOSING_FLAG = true;
bool ADUK_FLAG = true;
bool ENDAPAN_FLAG = true;
bool FILTERING_FLAG_CHANGE = true;
bool FILTERING_BERULANG_FLAG = true;
bool SOLENOID_BACKWASH_FLAG = true;
bool EMERGENGY_LAMP_FLAG = true;

// global variables
unsigned long waktu_dosing_mulai = 0;
unsigned long waktu_aduk_mulai = 0;
unsigned long waktu_endapan_mulai = 0;
unsigned long waktu_led_endapan_blink = 0;
unsigned long waktu_led_emergency_blink = 0;
unsigned long waktu_jeda_pompa_mulai = 0;
unsigned long waktu_led_drain_blink = 0;
unsigned long waktu_jeda_filtering_berulang = 0;
const byte PACKET_LENGTH = 10; 
const byte PACKET_LENGTH_OVERRIDE = 3; 
const byte START_BYTE = 0xBB;
uint8_t data_ke_raspi[PACKET_LENGTH];
byte data_dari_raspi[PACKET_LENGTH_OVERRIDE];
const byte ACK_BYTE = 0xFF;

bool kondisi_led_endapan = true;
bool kondisi_led_emergency = true;
bool kondisi_led_drain = true;

struct InputState {
  float level_1 = 0;
  float level_2 = 0;
  float tds_1 = 0;
  float flow_1 = 0;
  float pressure_1 = 0;
  bool level_switch = false;
  bool pb_start = false;
  bool mode_standby = false;
  bool mode_filtering = false;
  bool mode_backwash = false;
  bool mode_drain = false;
  bool mode_override = false;
  bool emergency_stop = false;
};

struct OutputState {
  bool solenoid_1 = false;
  bool solenoid_2 = false;
  bool solenoid_3 = false;
  bool solenoid_4 = false;
  bool solenoid_5 = false;
  bool solenoid_6 = false;
  bool pompa_1 = false;
  bool pompa_2 = false;
  bool pompa_3 = false;
  bool standby_lamp = false;
  bool filtering_lamp = false;
  bool backwash_lamp = false;
  bool drain_lamp = false;
  bool stepper = false;
};

InputState input;
OutputState output;

// define params for data comm. with raspi
byte index = 0;
bool receiving = false;

// define I/O pins
#define LEVEL_SWITCH_PIN 53

#define PB_START_PIN 44
#define MODE_FILTERING_PIN 52
#define MODE_BACKWASH_PIN 50
#define MODE_DRAIN_PIN 48
#define EMERGENCY_STOP_PIN 6
#define SOLENOID_1_PIN 9
#define SOLENOID_2_PIN 7
#define SOLENOID_3_PIN 5
#define SOLENOID_4_PIN 3 
#define SOLENOID_5_PIN 2
#define SOLENOID_6_PIN 24
#define POMPA_1_PIN 28
#define POMPA_2_PIN 32
#define POMPA_3_PIN 36
#define STEPPER_OUT 46

#define STANDBY_LAMP_PIN -1
#define FILTERING_LAMP_PIN 13
#define BACKWASH_LAMP_PIN 12
#define DRAIN_LAMP_PIN 11

enum ModeSistem {
  IDLE,
  FILTERING,
  BACKWASH,
  OVERRIDE,
  EMERGENCY,
  DRAIN
};

enum FilteringStep {
  PROSES_IDLE,
  PROSES_FILTER_START,
  PROSES_ISI,
  PROSES_DOSING,
  PROSES_ADUK,
  PROSES_ENDAPAN,
  PROSES_FILTERING
};

enum BackwashStep{
  PROSES_IDLE_BACKWASH,
  PROSES_BACKWASH_START,
  PROSES_ISI_BACKWASH,
  PROSES_BUKA_SOLENOID,
  PROSES_BACKWASH,
  PROSES_TUTUP_SOLENOID
};

// for additional safety and reliable use of pump 2
enum FilteringSubstep {
  FILTER_START,
  FILTER_JALAN,
  FILTER_TUNGGU_MATI_POMPA,
  FILTER_SELESAI
};

enum DrainStep{
  PROSES_IDLE_DRAIN,
  PROSES_DRAIN,
  PROSES_DRAIN_SELESAI
};

ModeSistem ModeSistemAktif = IDLE;
ModeSistem LastMode = IDLE; // untuk menyimpan mode terakhir, dipake buat fungsi exit
ModeSistem CurrentMode = IDLE; // untuk menyimpan mode, dipake buat fungsi exit

FilteringStep StepFilteringAktif = PROSES_IDLE;
FilteringSubstep SubFilteringAktif = FILTER_START;

BackwashStep StepBackwashAktif = PROSES_IDLE_BACKWASH;

DrainStep StepDrainAktif = PROSES_IDLE_DRAIN;

void setup() {
  // for debug
  Serial.begin(115200);

  Wire.begin();

  // input
  pinMode(LEVEL_SWITCH_PIN, INPUT_PULLUP);
  pinMode(PB_START_PIN, INPUT_PULLUP);
  pinMode(MODE_FILTERING_PIN, INPUT_PULLUP);
  pinMode(MODE_BACKWASH_PIN, INPUT_PULLUP);
  pinMode(MODE_DRAIN_PIN, INPUT_PULLUP);
  pinMode(EMERGENCY_STOP_PIN, INPUT_PULLUP);

  // output
  pinMode(SOLENOID_1_PIN, OUTPUT);
  digitalWrite(SOLENOID_1_PIN, HIGH);
  pinMode(SOLENOID_2_PIN, OUTPUT);
  digitalWrite(SOLENOID_2_PIN, HIGH);
  pinMode(SOLENOID_3_PIN, OUTPUT);
  digitalWrite(SOLENOID_3_PIN, HIGH);
  pinMode(SOLENOID_4_PIN, OUTPUT);
  digitalWrite(SOLENOID_4_PIN, HIGH);
  pinMode(SOLENOID_5_PIN, OUTPUT);
  digitalWrite(SOLENOID_5_PIN, HIGH);
  pinMode(SOLENOID_6_PIN, OUTPUT);
  digitalWrite(SOLENOID_6_PIN, HIGH);
  pinMode(POMPA_1_PIN, OUTPUT);
  digitalWrite(POMPA_1_PIN, HIGH);
  pinMode(POMPA_2_PIN, OUTPUT);
  digitalWrite(POMPA_2_PIN, HIGH);
  pinMode(POMPA_3_PIN, OUTPUT);
  digitalWrite(POMPA_3_PIN, HIGH);
  pinMode(STEPPER_OUT, OUTPUT);
  digitalWrite(STEPPER_OUT, HIGH);
  pinMode(STANDBY_LAMP_PIN, OUTPUT);
  digitalWrite(STANDBY_LAMP_PIN, HIGH);
  pinMode(FILTERING_LAMP_PIN, OUTPUT);
  digitalWrite(FILTERING_LAMP_PIN, HIGH);
  pinMode(BACKWASH_LAMP_PIN, OUTPUT);
  digitalWrite(BACKWASH_LAMP_PIN, HIGH);
  pinMode(DRAIN_LAMP_PIN, OUTPUT);
  digitalWrite(DRAIN_LAMP_PIN, HIGH);

  // for raspi communication
  Serial1.begin(115200);

  // netralize all outputs
  delay(100);
  reset_output();
  // debug_relay();
}

void baca_data_raspi(unsigned long timeout = 250) {
  unsigned long start = millis();
  while (millis() - start < timeout) {
    if (Serial1.available() > 0) {
      byte b = Serial1.read();
      
      if (b == ACK_BYTE) {
        // return ACK_BYTE;
        input.mode_override = false;
      } 
      else if (b == START_BYTE) {
        input.mode_override = true;
        // Tunggu hingga seluruh paket diterima
        while (Serial1.available() < (PACKET_LENGTH_OVERRIDE - 1) && (millis() - start < timeout)) {
          // Tunggu
        }
        
        // Pastikan seluruh paket diterima sebelum membaca
        if (Serial1.available() >= (PACKET_LENGTH_OVERRIDE - 1)) {
          data_dari_raspi[0] = b;
          for (int i = 1; i < PACKET_LENGTH_OVERRIDE; i++) {
            data_dari_raspi[i] = Serial1.read();
          }

          // Langsung proses paket karena tidak ada checksum
          // return START_BYTE;
        }
      }
    }
  }
  // return 0;
}

void loop() {
  scan_input();

  handle_mode_selection();
  handleTransition();

  switch (ModeSistemAktif){
    case IDLE:
      mode_idle();
      break;
    case FILTERING:
      mode_filtering();
      break;
    case BACKWASH:
      mode_backwash();
      break;
    case OVERRIDE:
      mode_override();
      break;
    case EMERGENCY:
      mode_emergency();
      break;
    case DRAIN:
      mode_drain();
    default:
      mode_idle();
      break;
  }
  apply_output();
  kirim_data_raspi();
  baca_data_raspi();
  debug_input();
  debug_output();
  // debugKomunikasiRaspi();
  // delay(10);
}

void scan_input(){
  input.level_1    = readTransmitter(level1_transmitter_address);
  // konversi sementara
  // input.level_1 = 100.0 - (constrain(readTransmitter(level1_transmitter_address), 0, 35) / 35.0 * 100.0);
  input.level_2    = readTransmitter(level2_transmitter_address);
  // input.tds_1      = readTransmitter(quality_transmitter_address);
  // input.flow_1     = readTransmitter(flow_transmitter_address);
  // input.pressure_1 = readTransmitter(pressure_transmitter_address);
  input.tds_1      = 0;
  input.flow_1     = 0;
  input.pressure_1 = 0;
  input.level_switch   = digitalRead(LEVEL_SWITCH_PIN);
  input.pb_start       = !digitalRead(PB_START_PIN);
  input.emergency_stop = digitalRead(EMERGENCY_STOP_PIN);

  delay(10);
  
  if(!input.emergency_stop){
    input.mode_filtering = !digitalRead(MODE_FILTERING_PIN);
    input.mode_backwash  = !digitalRead(MODE_BACKWASH_PIN);
    input.mode_drain     = !digitalRead(MODE_DRAIN_PIN);
  }
  else if(input.emergency_stop){
    input.mode_filtering = false;
    input.mode_backwash  = false;
    input.mode_drain     = false;
  }
}

float readTransmitter(uint8_t addr) {
  uint8_t n = Wire.requestFrom(addr, (uint8_t)8);

  if (n == 0) {
    String msg = "0x";
    if (addr < 16) msg += "0";
    msg += String(addr, HEX);
    msg += " : (no response)";
    // Serial.print(msg);
    return 0.0;
  }

  char buf[33];
  uint8_t k = 0;
  while (Wire.available() && k < 32) {
    buf[k++] = (char)Wire.read();
  }
  buf[k] = '\0';

  return atof(buf); // ubah string ASCII jadi float
}

void handle_mode_selection(){
  if(input.mode_filtering){
    ModeSistemAktif = FILTERING;
    CurrentMode = FILTERING;
  }
  else if(input.mode_backwash){
    ModeSistemAktif = BACKWASH;
    CurrentMode = BACKWASH;
  }
  else if(input.mode_override){
    ModeSistemAktif = OVERRIDE;
    CurrentMode = OVERRIDE;
  }
  else if(input.emergency_stop){
    ModeSistemAktif = EMERGENCY;
    CurrentMode = EMERGENCY;
  }
  else if(input.mode_drain){
    ModeSistemAktif = DRAIN;
    CurrentMode = DRAIN;
  }
  else{
    ModeSistemAktif = IDLE;
    CurrentMode = IDLE;
  }
}

void handleTransition(){
  // jika mode berubah, reset semua flag
  if(ModeSistemAktif != LastMode){
    exitMode();
    LastMode = CurrentMode;
  }
}

void exitMode(){
  switch(LastMode){
    case IDLE:
      input.mode_standby = false;
      output.standby_lamp = false;
      input.mode_override = false;
      break;

    case FILTERING:
      input.mode_override = false;
      output.standby_lamp = true;
      output.filtering_lamp = false;

      // reset flags
      DOSING_FLAG = true;
      ADUK_FLAG = true;
      ENDAPAN_FLAG = true;
      FILTERING_FLAG_CHANGE = true;
      FILTERING_BERULANG_FLAG = true;

      StepFilteringAktif = PROSES_IDLE; // reset filtering step
      SubFilteringAktif = FILTER_START; // reset sub filtering step
      reset_output(); // reset all outputs
      break;
      
    case BACKWASH:
      input.mode_override = false;
      output.standby_lamp = true;
      output.backwash_lamp = false;

      SOLENOID_BACKWASH_FLAG = true; // reset flag for solenoid backwash
      
      StepBackwashAktif = PROSES_IDLE_BACKWASH;
      reset_output(); // reset all outputs
      break;
      
    case DRAIN:
      input.mode_override = false;
      output.standby_lamp = false;
      output.drain_lamp = false;
      kondisi_led_drain = true; // reset led drain state
      StepDrainAktif = PROSES_IDLE_DRAIN; // reset drain step
      reset_output(); // reset all outputs
      break;
      
    case OVERRIDE:
      input.mode_override = false;
      reset_output(); // reset all outputs
      break;
      
    case EMERGENCY:
      EMERGENGY_LAMP_FLAG = true; // reset flag for emergency lamp
      input.mode_override = false;
      reset_output(); // reset all outputs
      break;
  }
  LastMode = ModeSistemAktif; // update last mode
}

void mode_idle(){
  input.mode_standby = true;
  output.standby_lamp = true;
  FILTERING_BERULANG_FLAG = true; // reset flag for repeat filtering
}

void mode_filtering(){
  switch(StepFilteringAktif){
    case PROSES_IDLE:
      output.filtering_lamp = true; 
      if(input.pb_start){
        StepFilteringAktif = PROSES_FILTER_START;
        SubFilteringAktif = FILTER_START;
      }
      break;

    case PROSES_FILTER_START:
      if(input.level_1 <= SETPOINT_ATAS_TANGKI_AGITATOR){
        if(input.level_switch){
          output.pompa_1 = true;
          StepFilteringAktif = PROSES_ISI;
        }
      }
      else if(input.level_1 >= SETPOINT_ATAS_TANGKI_AGITATOR){
        StepFilteringAktif = PROSES_DOSING;
      }
      break;

    case PROSES_ISI:
      if(input.level_1 >= SETPOINT_ATAS_TANGKI_AGITATOR){
        output.pompa_1 = false;
        StepFilteringAktif = PROSES_DOSING;
      }
      break;

    case PROSES_DOSING:
      if(run_once(DOSING_FLAG)){
        waktu_dosing_mulai = millis();
        // WAKTU_DOSING = map(input.tds_1, 0, 100, 0, 2000); // seting waktu dosing bedasarkan input tds, dimapping ke 0 sampai 2 detik
        output.pompa_3 =  true;
      }
      else if(millis() - waktu_dosing_mulai >= WAKTU_DOSING){ // non blocking delay
          output.pompa_3 = false;
          DOSING_FLAG = true;
          StepFilteringAktif = PROSES_ADUK;
        }
      break;

    case PROSES_ADUK:
      if(run_once(ADUK_FLAG)){
        waktu_aduk_mulai = millis();
        // stepper run with acceleration
        output.stepper = 1;
      }

      else if(millis() - waktu_aduk_mulai >= WAKTU_ADUK){
        // stepper stop with deceleration
        StepFilteringAktif = PROSES_ENDAPAN;
        ADUK_FLAG = true;
        output.stepper = 0;
      }
      break;

    case PROSES_ENDAPAN:
      if(run_once(ENDAPAN_FLAG)){
        waktu_endapan_mulai = millis();
        waktu_led_endapan_blink = millis();
        kondisi_led_endapan = true;
      }

      else if(millis() - waktu_endapan_mulai >= WAKTU_ENDAPAN){
        StepFilteringAktif = PROSES_FILTERING;
        ENDAPAN_FLAG = true;
        output.filtering_lamp = true;
      }
      output.filtering_lamp = blink_led(waktu_led_endapan_blink, ENDAPAN_LED_ON_TIME, ENDAPAN_LED_OFF_TIME, kondisi_led_endapan);
      break;

    case PROSES_FILTERING:
      switch(SubFilteringAktif){
        case FILTER_START:
          if(run_once(FILTERING_FLAG_CHANGE)){
            waktu_jeda_pompa_mulai = millis();
            output.solenoid_1 = true;
            output.solenoid_2 = true;
            delay(1);
            output.solenoid_3 = true;
          }
          else if(input.level_1 >= SETPOINT_BAWAH_TANGKI_AGITATOR && millis() - waktu_jeda_pompa_mulai >= DELAY_POMPA){
            output.pompa_2 = true;
            SubFilteringAktif = FILTER_JALAN;
          }
          break;

        case FILTER_JALAN:
          if(input.level_1 <= SETPOINT_BAWAH_TANGKI_AGITATOR){
              waktu_jeda_pompa_mulai = millis();
              output.pompa_2 = false;
              SubFilteringAktif = FILTER_TUNGGU_MATI_POMPA;
            }

          break;

        case FILTER_TUNGGU_MATI_POMPA:
          if(millis() - waktu_jeda_pompa_mulai >= DELAY_POMPA){
            output.solenoid_1 = false;
            output.solenoid_2 = false;
            Serial.println("AAAAAAAAAAAAAAAAA");
            output.solenoid_3 = false;
            Serial.println("BBBBBBBBBBBBBBBBB");
            SubFilteringAktif = FILTER_SELESAI;
          }
          break;

        case FILTER_SELESAI:
          if(run_once(FILTERING_BERULANG_FLAG)){
            waktu_jeda_filtering_berulang = millis();
            // SubFilteringAktif = FILTER_START;
            // output.filtering_lamp = false;
            FILTERING_FLAG_CHANGE = true;
          }
          if(input.pb_start){
            StepFilteringAktif = PROSES_IDLE;
            SubFilteringAktif = FILTER_START;
          }
          // if(millis() - waktu_jeda_filtering_berulang >= JEDA_FILTERING_BERULANG){
          //   StepFilteringAktif = PROSES_FILTER_START;
          // }
          break;
      }
      break;
  }
}

void mode_backwash(){
  switch(StepBackwashAktif){
    case PROSES_IDLE_BACKWASH:
      output.backwash_lamp = true;
      if(input.pb_start){
        StepBackwashAktif = PROSES_BACKWASH_START;
      }
      break;
    case PROSES_BACKWASH_START:
      if(input.level_1 <= SETPOINT_ATAS_TANGKI_AGITATOR){
        if(input.level_switch){
          output.pompa_1 = true;
          StepBackwashAktif = PROSES_ISI_BACKWASH;
        }
      }
      else if(input.level_1 >= SETPOINT_ATAS_TANGKI_AGITATOR){
        StepBackwashAktif = PROSES_BUKA_SOLENOID;
      }
      break;

    case PROSES_ISI_BACKWASH:
      if(input.level_1 >= SETPOINT_ATAS_TANGKI_AGITATOR){
        output.pompa_1 = false;
        StepBackwashAktif = PROSES_BUKA_SOLENOID;
      }
      break;

    case PROSES_BUKA_SOLENOID:
      if(run_once(SOLENOID_BACKWASH_FLAG)){
        waktu_jeda_pompa_mulai = millis();
        output.solenoid_1 = true;
        output.solenoid_5 = true;
        output.solenoid_6 = true;
      }
      else if(millis() - waktu_jeda_pompa_mulai >= DELAY_POMPA){
        output.pompa_2 = true;
        StepBackwashAktif = PROSES_BACKWASH;
      }

      break;

    case PROSES_BACKWASH:
      if(input.level_1 <= SETPOINT_BAWAH_TANGKI_AGITATOR){
        waktu_jeda_pompa_mulai = millis();
        output.pompa_2 = false;
        StepBackwashAktif = PROSES_TUTUP_SOLENOID;
      }
      break;

    case PROSES_TUTUP_SOLENOID:    
      if(millis() - waktu_jeda_pompa_mulai >= DELAY_POMPA){
        output.solenoid_1 = false;
        output.solenoid_5 = false;
        output.solenoid_6 = false;
        StepBackwashAktif = PROSES_IDLE_BACKWASH;
        ModeSistemAktif = IDLE;
        SOLENOID_BACKWASH_FLAG = true;
        output.backwash_lamp = false;
      }
      break;
  }
}

void mode_override(){
  byte flags1 = data_dari_raspi[1];
  byte flags2 = data_dari_raspi[2];

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

void mode_emergency(){
  // reset all mode when emergency
  // StepFilteringAktif = PROSES_IDLE;
  // StepBackwashAktif = PROSES_IDLE_BACKWASH;
  // StepDrainAktif = PROSES_IDLE_DRAIN;

  input.mode_standby = false;
  output.solenoid_1 = false;
  output.solenoid_2 = false;
  output.solenoid_3 = false;
  output.solenoid_4 = false;
  output.solenoid_5 = false;
  output.solenoid_6 = false;
  output.pompa_1 = false;
  output.pompa_2 = false;
  output.pompa_3 = false;
  
  if(run_once(EMERGENGY_LAMP_FLAG)){
    waktu_led_emergency_blink = millis();
    kondisi_led_emergency = true;
  }

  // blinking all LED
  kondisi_led_emergency = blink_led(waktu_led_emergency_blink, EMERGENCY_LED_ON_TIME, EMERGENCY_LED_OFF_TIME, kondisi_led_emergency);
  output.standby_lamp = kondisi_led_emergency;
  output.filtering_lamp = kondisi_led_emergency;
  // output.solenoid_1 = kondisi_led_emergency;
  output.backwash_lamp = kondisi_led_emergency;
  output.drain_lamp = kondisi_led_emergency;
}

void mode_drain(){
  switch(StepDrainAktif){ 
    case PROSES_IDLE_DRAIN:
      // output.standby_lamp = false;
      output.drain_lamp = kondisi_led_drain;
      output.solenoid_1 = true;
      output.solenoid_2 = true;
      output.solenoid_3 = true;
      output.solenoid_4 = true;
      output.solenoid_5 = true;
      output.solenoid_6 = true;

      if(input.pb_start){
        StepDrainAktif = PROSES_DRAIN;
        waktu_led_drain_blink = millis();
      }
      break;  

    case PROSES_DRAIN:
      output.drain_lamp = blink_led(waktu_led_drain_blink, 1000, 500, kondisi_led_drain);
      
      if(input.level_2 <= LEVEL_TANGKI_STORAGE_KOSONG){
        StepDrainAktif = PROSES_DRAIN_SELESAI;
      }
      break;  

    case PROSES_DRAIN_SELESAI:
      StepDrainAktif = PROSES_IDLE_DRAIN;
      break;  
  }
}

void apply_output(){
  digitalWrite(SOLENOID_1_PIN, !output.solenoid_1);
  digitalWrite(SOLENOID_2_PIN, !output.solenoid_2);
  digitalWrite(SOLENOID_3_PIN, !output.solenoid_3);
  digitalWrite(SOLENOID_4_PIN, !output.solenoid_4);
  digitalWrite(SOLENOID_5_PIN, !output.solenoid_5);
  digitalWrite(SOLENOID_6_PIN, !output.solenoid_6);
  
  digitalWrite(POMPA_1_PIN, !output.pompa_1);
  digitalWrite(POMPA_2_PIN, !output.pompa_2);
  digitalWrite(POMPA_3_PIN, !output.pompa_3);
  digitalWrite(STEPPER_OUT, !output.pompa_3);

  // digitalWrite(STANDBY_LAMP_PIN, !output.standby_lamp);
  digitalWrite(FILTERING_LAMP_PIN, !output.filtering_lamp);
  digitalWrite(BACKWASH_LAMP_PIN, !output.backwash_lamp);
  digitalWrite(DRAIN_LAMP_PIN, !output.drain_lamp);
  digitalWrite(STEPPER_OUT, !output.stepper);

  delay(50);
}

uint8_t boolsToByte(bool arr[8]) {
  uint8_t b = 0;
  for (int i = 0; i < 8; i++) {
    if (arr[i]) b |= (1 << i);
  }
  return b;
}

uint8_t calculateChecksum(uint8_t *packet, int length) {
  uint8_t checksum = 0;
  for (int i = 0; i < length - 1; i++) {
    checksum ^= packet[i];
  }
  return checksum;
}

void kirim_data_raspi(){
  //======================PACKING==============================
  data_ke_raspi[0] = 0xAA;  // start byte

  //=====================INPUT INT=========================
  data_ke_raspi[1] = input.level_1;
  data_ke_raspi[2] = input.level_2;
  data_ke_raspi[3] = input.tds_1;
  data_ke_raspi[4] = input.flow_1;
  data_ke_raspi[5] = input.pressure_1;

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
  data_ke_raspi[6] = input_flags;

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
  data_ke_raspi[7] = output_flags;

  uint8_t output_flags2 = 0;
  output_flags2 |= output.pompa_3 << 0;
  output_flags2 |= output.standby_lamp << 1;
  output_flags2 |= output.filtering_lamp << 2;
  output_flags2 |= output.backwash_lamp << 3;
  output_flags2 |= output.drain_lamp << 4;
  output_flags2 |= output.stepper << 5;
  data_ke_raspi[8] = output_flags2;

  //======================VALIDASI=========================
  uint8_t validasi = 0;
  for (int i = 0; i < 9; i++) {
    validasi ^= data_ke_raspi[i];
  }
  data_ke_raspi[9] = validasi;

  Serial1.write(data_ke_raspi, PACKET_LENGTH);
  // delay(500);
}

// void baca_data_raspi(){
//   while (Serial1.available()) {
//     byte b = Serial1.read();

//     if (!receiving) {
//       if (b == START_BYTE) {
//         receiving = true;
//         index = 0;
//         data_dari_raspi[index++] = b;
//         input.mode_override = true;
//       } 
//     } else {
//       data_dari_raspi[index++] = b;
//       if (index == PACKET_LENGTH) {
//         receiving = false;
//       }
//     }
//   } 
// }



void reset_output() {
  output = OutputState(); // otomatis semua LOW (karena bool default = false)
}

bool run_once(bool &flag) {
  if (flag) {
    flag = false;   // langsung ubah variabel aslinya
    return true;
  }
  return false;
}

bool blink_led(unsigned long &waktu_last_toggle, unsigned long on_time, unsigned long off_time, bool &state){
  unsigned long interval = state? on_time : off_time;
  if (millis() - waktu_last_toggle >= interval){
    state = !state;
    waktu_last_toggle = millis();
  }
  return state;
}

void debug_relay(){
  digitalWrite(SOLENOID_1_PIN, LOW); delay(250); digitalWrite(SOLENOID_1_PIN, HIGH); delay(250);
  digitalWrite(SOLENOID_2_PIN, LOW); delay(250); digitalWrite(SOLENOID_2_PIN, HIGH); delay(250);
  digitalWrite(SOLENOID_3_PIN, LOW); delay(250); digitalWrite(SOLENOID_3_PIN, HIGH); delay(250);
  digitalWrite(SOLENOID_4_PIN, LOW); delay(250); digitalWrite(SOLENOID_4_PIN, HIGH); delay(250);
  digitalWrite(SOLENOID_5_PIN, LOW); delay(250); digitalWrite(SOLENOID_5_PIN, HIGH); delay(250);
  digitalWrite(SOLENOID_6_PIN, LOW); delay(250); digitalWrite(SOLENOID_6_PIN, HIGH); delay(250);

  digitalWrite(POMPA_1_PIN, LOW); delay(250); digitalWrite(POMPA_1_PIN, HIGH); delay(250);
  digitalWrite(POMPA_2_PIN, LOW); delay(250); digitalWrite(POMPA_2_PIN, HIGH); delay(250);
  digitalWrite(POMPA_3_PIN, LOW); delay(250); digitalWrite(POMPA_3_PIN, HIGH); delay(250);

  digitalWrite(FILTERING_LAMP_PIN, LOW); delay(250); digitalWrite(FILTERING_LAMP_PIN, HIGH); delay(250);
  digitalWrite(BACKWASH_LAMP_PIN, LOW); delay(250); digitalWrite(BACKWASH_LAMP_PIN, HIGH); delay(250);
  digitalWrite(DRAIN_LAMP_PIN, LOW); delay(250); digitalWrite(DRAIN_LAMP_PIN, HIGH); delay(250);
}

void debug_input() {
  Serial.println(F("===== DEBUG INPUT ====="));
  Serial.print(F("Level Switch   : ")); Serial.println(input.level_switch ? "ON" : "OFF");
  Serial.print(F("PB Start       : ")); Serial.println(input.pb_start ? "PRESSED" : "RELEASED");
  Serial.print(F("Mode Filtering : ")); Serial.println(input.mode_filtering ? "ON" : "OFF");
  Serial.print(F("Mode Backwash  : ")); Serial.println(input.mode_backwash ? "ON" : "OFF");
  Serial.print(F("Mode Drain     : ")); Serial.println(input.mode_drain ? "ON" : "OFF");
  Serial.print(F("Emergency Stop : ")); Serial.println(input.emergency_stop ? "ACTIVE" : "NORMAL");
  Serial.print(F("Level 1        : ")); Serial.println(input.level_1);
  Serial.print(F("Level 2        : ")); Serial.println(input.level_2);
  Serial.print(F("Pressure       : ")); Serial.println(input.pressure_1);
  Serial.print(F("Quality (TDS)  : ")); Serial.println(input.tds_1);
  Serial.print(F("Flow           : ")); Serial.println(input.flow_1);
  // Serial.print(F("Stepper           : ")); Serial.println(output.stepper);
  Serial.println(F("===== DEBUG STATE ====="));
  
  // Mode Sistem
  Serial.print(F("Mode Sistem        : "));
  switch(ModeSistemAktif) {
    case IDLE:       Serial.println("IDLE"); break;
    case FILTERING:  Serial.println("FILTERING"); break;
    case BACKWASH:   Serial.println("BACKWASH"); break;
    case OVERRIDE:   Serial.println("OVERRIDE"); break;
    case EMERGENCY:  Serial.println("EMERGENCY"); break;
    case DRAIN:      Serial.println("DRAIN"); break;
    default:         Serial.println("UNKNOWN"); break;
  }

  // Add debug for LastMode
  Serial.print(F("Last Mode          : "));
  switch(LastMode) {
    case IDLE:       Serial.println("IDLE"); break;
    case FILTERING:  Serial.println("FILTERING"); break;
    case BACKWASH:   Serial.println("BACKWASH"); break;
    case OVERRIDE:   Serial.println("OVERRIDE"); break;
    case EMERGENCY:  Serial.println("EMERGENCY"); break;
    case DRAIN:      Serial.println("DRAIN"); break;
    default:         Serial.println("UNKNOWN"); break;
  }

  // Add debug for CurrentMode
  Serial.print(F("Current Mode       : "));
  switch(CurrentMode) {
    case IDLE:       Serial.println("IDLE"); break;
    case FILTERING:  Serial.println("FILTERING"); break;
    case BACKWASH:   Serial.println("BACKWASH"); break;
    case OVERRIDE:   Serial.println("OVERRIDE"); break;
    case EMERGENCY:  Serial.println("EMERGENCY"); break;
    case DRAIN:      Serial.println("DRAIN"); break;
    default:         Serial.println("UNKNOWN"); break;
  }

  // Step Filtering
  Serial.print(F("Step Filtering     : "));
  switch(StepFilteringAktif) {
    case PROSES_IDLE:         Serial.println("IDLE"); break;
    case PROSES_FILTER_START: Serial.println("FILTER_START"); break;
    case PROSES_ISI:          Serial.println("ISI"); break;
    case PROSES_DOSING:       Serial.println("DOSING"); break;
    case PROSES_ADUK:         Serial.println("ADUK"); break;
    case PROSES_ENDAPAN:      Serial.println("ENDAPAN"); break;
    case PROSES_FILTERING:    Serial.println("FILTERING"); break;
    default:                  Serial.println("UNKNOWN"); break;
  }

  // Step Backwash
  Serial.print(F("Step Backwash      : "));
  switch(StepBackwashAktif) {
    case PROSES_IDLE_BACKWASH:   Serial.println("IDLE"); break;
    case PROSES_ISI_BACKWASH:    Serial.println("ISI_BACKWASH"); break;
    case PROSES_BUKA_SOLENOID:   Serial.println("BUKA_SOLENOID"); break;
    case PROSES_BACKWASH:        Serial.println("BACKWASH"); break;
    case PROSES_TUTUP_SOLENOID:  Serial.println("TUTUP_SOLENOID"); break;
    default:                     Serial.println("UNKNOWN"); break;
  }

  // Substep Filtering
  Serial.print(F("Substep Filtering  : "));
  switch(SubFilteringAktif) {
    case FILTER_START:            Serial.println("START"); break;
    case FILTER_JALAN:            Serial.println("JALAN"); break;
    case FILTER_TUNGGU_MATI_POMPA:Serial.println("TUNGGU_MATI_POMPA"); break;
    case FILTER_SELESAI:          Serial.println("SELESAI"); break;
    default:                      Serial.println("UNKNOWN"); break;
  }

  // Step Drain
  Serial.print(F("Step Drain         : "));
  switch(StepDrainAktif) {
    case PROSES_IDLE_DRAIN:   Serial.println("IDLE"); break;
    case PROSES_DRAIN:        Serial.println("DRAIN"); break;
    case PROSES_DRAIN_SELESAI:Serial.println("DRAIN_SELESAI"); break;
    default:                  Serial.println("UNKNOWN"); break;
  }

  Serial.println(F("=========================\n"));
  delay(200);
}

void debug_output() {
  Serial.println(F("===== OUTPUT STATE ====="));
  Serial.print(F("Solenoid 1: ")); Serial.println(output.solenoid_1);
  Serial.print(F("Solenoid 2: ")); Serial.println(output.solenoid_2);
  Serial.print(F("Solenoid 3: ")); Serial.println(output.solenoid_3);
  Serial.print(F("Solenoid 4: ")); Serial.println(output.solenoid_4);
  Serial.print(F("Solenoid 5: ")); Serial.println(output.solenoid_5);
  Serial.print(F("Solenoid 6: ")); Serial.println(output.solenoid_6);

  Serial.print(F("Pompa 1: ")); Serial.println(output.pompa_1);
  Serial.print(F("Pompa 2: ")); Serial.println(output.pompa_2);
  Serial.print(F("Pompa 3: ")); Serial.println(output.pompa_3);

  Serial.print(F("Standby Lamp: ")); Serial.println(output.standby_lamp);
  Serial.print(F("Filtering Lamp: ")); Serial.println(output.filtering_lamp);
  Serial.print(F("Backwash Lamp: ")); Serial.println(output.backwash_lamp);
  Serial.print(F("Drain Lamp: ")); Serial.println(output.drain_lamp);

  Serial.print(F("Stepper: ")); Serial.println(output.stepper);
  Serial.println(F("========================"));
}

void debugKomunikasiRaspi() {
  Serial.println(F("======= DEBUG KOMUNIKASI RASPI ======="));

  // Cetak data ke raspi
  Serial.print(F("Kirim -> "));
  for (int i = 0; i < PACKET_LENGTH; i++) {
    if (data_ke_raspi[i] < 0x10) Serial.print("0");
    Serial.print(data_ke_raspi[i], HEX);
    Serial.print(" ");
  }
  Serial.print(F(" | Checksum: 0x"));
  Serial.println(data_ke_raspi[PACKET_LENGTH - 1], HEX);

  // Cetak data dari raspi
  Serial.print(F("Terima <- "));
  for (int i = 0; i < PACKET_LENGTH_OVERRIDE; i++) {
    if (data_dari_raspi[i] < 0x10) Serial.print("0");
    Serial.print(data_dari_raspi[i], HEX);
    Serial.print(" ");
  }
  Serial.print(F(" | Checksum: 0x"));
  Serial.println(data_dari_raspi[PACKET_LENGTH_OVERRIDE - 1], HEX);

  Serial.println(F("======================================"));
}

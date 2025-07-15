// struktur kode:
// 1. define input output variable
// 2. define i/o pins
// 3. define FSM
// 4. setup
// 5. main loop
// 6. functions

// setpoints:
int SETPOINT_ATAS_TANGKI_AGITATOR = 80; // 80 percent
int SETPOINT_BAWAH_TANGKI_AGITATOR = 20; // 20 percent
unsigned long WAKTU_ADUK = 120000; // 2 minutes
unsigned long WAKTU_ENDAPAN = 180000; // 3 minutes
unsigned long ENDAPAN_LED_ON_TIME = 1000; // led on for 1 s when in ENDAPAN process
unsigned long ENDAPAN_LED_OFF_TIME = 500; // led off for 0.5 s when in ENDAPAN process
unsigned long EMERGENCY_LED_ON_TIME = 1000; // led on for 1 s when in emergency mode
unsigned long EMERGENCY_LED_OFF_TIME = 750; // led off for 0.75 s when in emergency mode
unsigned long DELAY_POMPA = 2000; // delay when pump is about to be turned on (for safety)
unsigned long WAKTU_DOSING = 2000; // dosing time

// Flag to ensure the dosing logic (triggered by millis timing) runs only once per activation,
// preventing repeated execution during continuous loop cycles.
bool DOSING_FLAG = true;
bool ADUK_FLAG = true;
bool ENDAPAN_FLAG = true;
bool FILTERING_FLAG = true;
bool SOLENOID_BACKWASH_FLAG = true;
bool EMERGENGY_LAMP_FLAG = true;

// global variables
unsigned long waktu_dosing_mulai = 0;
unsigned long waktu_aduk_mulai = 0;
unsigned long waktu_endapan_mulai = 0;
unsigned long waktu_led_endapan_blink = 0;
unsigned long waktu_led_emergency_blink = 0;
unsigned long waktu_jeda_pompa_mulai = 0;
int data_dari_esp = 0;
int data_ke_esp = 0;

bool kondisi_led_endapan = true;
bool kondisi_led_emergency = true;

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
  bool stepper_pulse;
  bool stepper_en;
  bool stepper_dir;
};

InputState input;
OutputState output;

// define I/O pins
#define LEVEL_1_PIN 10
#define LEVEL_2_PIN 9
#define TDS_1_PIN 12
#define FLOW_1_PIN 11
#define PRESSURE_1_PIN 13
#define LEVEL_SWITCH_PIN -1

#define PB_START_PIN -1
#define MODE_STANDBY_PIN -1
#define MODE_FILTERING_PIN -1
#define MODE_BACKWASH_PIN -1
#define MODE_DRAIN_PIN -1
#define EMERGENCY_STOP_PIN -1

#define SOLENOID_1_PIN 4
#define SOLENOID_2_PIN 3
#define SOLENOID_3_PIN 2
#define SOLENOID_4_PIN 1
#define SOLENOID_5_PIN 0
#define SOLENOID_6_PIN 21
#define POMPA_1_PIN 7
#define POMPA_2_PIN 6
#define POMPA_3_PIN 5

#define STANDBY_LAMP_PIN -1
#define FILTERING_LAMP_PIN -1
#define BACKWASH_LAMP_PIN -1
#define DRAIN_LAMP_PIN -1

#define STEPPER_PULSE_PIN -1
#define STEPPER_EN_PIN -1
#define STEPPER_DIR_PIN -1

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
  PROSES_ISI,
  PROSES_DOSING,
  PROSES_ADUK,
  PROSES_ENDAPAN,
  PROSES_FILTERING
};

enum BackwashStep{
  PROSES_IDLE_BACKWASH,
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

ModeSistem ModeSistemAktif = IDLE;
FilteringStep StepFilteringAktif = PROSES_IDLE;
BackwashStep StepBackwashAktif = PROSES_IDLE_BACKWASH;
FilteringSubstep SubFilteringAktif = FILTER_START;

void setup() {
  // input
  pinMode(LEVEL_1_PIN, INPUT);
  pinMode(LEVEL_2_PIN, INPUT);
  pinMode(TDS_1_PIN, INPUT);
  pinMode(FLOW_1_PIN, INPUT);
  pinMode(PRESSURE_1_PIN, INPUT);
  pinMode(LEVEL_SWITCH_PIN, INPUT);
  pinMode(PB_START_PIN, INPUT);
  pinMode(MODE_STANDBY_PIN, INPUT);
  pinMode(MODE_FILTERING_PIN, INPUT);
  pinMode(MODE_BACKWASH_PIN, INPUT);
  pinMode(MODE_DRAIN_PIN, INPUT);
  pinMode(EMERGENCY_STOP_PIN, INPUT);

  // output
  pinMode(SOLENOID_1_PIN, OUTPUT);
  pinMode(SOLENOID_2_PIN, OUTPUT);
  pinMode(SOLENOID_3_PIN, OUTPUT);
  pinMode(SOLENOID_4_PIN, OUTPUT);
  pinMode(SOLENOID_5_PIN, OUTPUT);
  pinMode(SOLENOID_6_PIN, OUTPUT);
  pinMode(POMPA_1_PIN, OUTPUT);
  pinMode(POMPA_2_PIN, OUTPUT);
  pinMode(POMPA_3_PIN, OUTPUT);
  pinMode(STANDBY_LAMP_PIN, OUTPUT);
  pinMode(FILTERING_LAMP_PIN, OUTPUT);
  pinMode(BACKWASH_LAMP_PIN, OUTPUT);
  pinMode(DRAIN_LAMP_PIN, OUTPUT);
  pinMode(STEPPER_PULSE_PIN, OUTPUT);
  pinMode(STEPPER_EN_PIN, OUTPUT);
  pinMode(STEPPER_DIR_PIN, OUTPUT);

  // for esp32 communication
  Serial1.begin(115200);

  // for debug
  Serial.begin(57600);

  // netralize all outputs
  resetOutput();
}

void loop() {
  scan_input();
  baca_data_esp();
  handle_mode_selection();

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
  kirim_data_esp();
}

void scan_input(){
  input.level_1        = pwmRead(LEVEL_1_PIN);
  input.level_2        = pwmRead(LEVEL_2_PIN);
  input.tds_1          = pwmRead(TDS_1_PIN);
  input.flow_1         = pwmRead(FLOW_1_PIN);
  input.pressure_1     = pwmRead(PRESSURE_1_PIN);
  input.level_switch   = digitalRead(LEVEL_SWITCH_PIN);
  input.pb_start       = debounce_read(PB_START_PIN);
  input.mode_standby   = debounce_read(MODE_STANDBY_PIN);
  input.mode_filtering = debounce_read(MODE_FILTERING_PIN);
  input.mode_backwash  = debounce_read(MODE_BACKWASH_PIN);
  input.mode_drain     = debounce_read(MODE_DRAIN_PIN);
  input.emergency_stop = debounce_read(EMERGENCY_STOP_PIN);
}

int pwmRead(int pin){

}

bool debounce_read(int pin) {
  static unsigned long lastDebounce = 0;
  bool lastState = digitalRead(pin);
  delay(5); // delay pendek
  bool currentState = digitalRead(pin);
  if (lastState == currentState) {
    return currentState;
  }
  return lastState;
}

void handle_mode_selection(){
  if(input.mode_standby){
    ModeSistemAktif = IDLE;
  }
  else if(input.mode_filtering && input.pb_start){
    ModeSistemAktif = FILTERING;
  }
  else if(input.mode_backwash && input.pb_start){
    ModeSistemAktif = BACKWASH;
  }
  else if(input.mode_override){
    ModeSistemAktif = OVERRIDE;
  }
  else if(input.emergency_stop){
    ModeSistemAktif = EMERGENCY;
  }
  else if(input.mode_drain){
    ModeSistemAktif = DRAIN;
  }
}

void mode_idle(){
  resetOutput();
  output.standby_lamp = true;
}

void mode_filtering(){
  switch(StepFilteringAktif){
    case PROSES_IDLE:
      output.filtering_lamp = true;
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
        WAKTU_DOSING = map(input.tds_1, 0, 100, 0, 2000); // seting waktu dosing bedasarkan input tds, dimapping ke 0 sampai 2 detik
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
      }
      else if(millis() - waktu_aduk_mulai >= WAKTU_ADUK){
        // stepper stop with deceleration
        StepFilteringAktif = PROSES_ENDAPAN;
        ADUK_FLAG = true;
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
      }
      output.filtering_lamp = blink_led(waktu_led_endapan_blink, ENDAPAN_LED_ON_TIME, ENDAPAN_LED_OFF_TIME, kondisi_led_endapan);
      break;

    case PROSES_FILTERING:
      switch(SubFilteringAktif){
        case FILTER_START:
          if(run_once(FILTERING_FLAG)){
            waktu_jeda_pompa_mulai = millis();
            output.solenoid_1 = true;
            output.solenoid_2 = true;
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
            output.solenoid_3 = false;
            SubFilteringAktif = FILTER_SELESAI;
          }
          break;
        case FILTER_SELESAI:
          StepFilteringAktif = PROSES_IDLE;
          ModeSistemAktif = IDLE;
          FILTERING_FLAG = true;
          output.filtering_lamp = false;
          break;
      }
      break;
  }
}

void mode_backwash(){
  switch(StepBackwashAktif){
    case PROSES_IDLE_BACKWASH:
      output.backwash_lamp = true;
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

}

void mode_emergency(){
  solenoid_1 = false;
  solenoid_2 = false;
  solenoid_3 = false;
  solenoid_4 = false;
  solenoid_5 = false;
  solenoid_6 = false;
  pompa_1 = false;
  pompa_2 = false;
  pompa_3 = false;
  
  if(run_once(EMERGENGY_LAMP_FLAG)){
    waktu_led_emergency_blink = millis();
    kondisi_led_emergency = true;
  }

  // blinking all LED
  kondisi_led_emergency = blink_led(waktu_led_emergency_blink, EMERGENCY_LED_ON_TIME, EMERGENCY_LED_OFF_TIME, kondisi_led_emergency);
  output.standby_lamp = kondisi_led_emergency;
  output.filtering_lamp = kondisi_led_emergency;
  output.backwash_lamp = kondisi_led_emergency;
  output.drain_lamp = kondisi_led_emergency;
}

void mode_drain(){
  
}

void apply_output(){
  digitalWrite(SOLENOID_1_PIN, output.solenoid_1);
  digitalWrite(SOLENOID_2_PIN, output.solenoid_2);
  digitalWrite(SOLENOID_3_PIN, output.solenoid_3);
  digitalWrite(SOLENOID_4_PIN, output.solenoid_4);
  digitalWrite(SOLENOID_5_PIN, output.solenoid_5);
  digitalWrite(SOLENOID_6_PIN, output.solenoid_6);

  digitalWrite(POMPA_1_PIN, output.pompa_1);
  digitalWrite(POMPA_2_PIN, output.pompa_2);
  digitalWrite(POMPA_3_PIN, output.pompa_3);

  digitalWrite(STANDBY_LAMP_PIN, output.standby_lamp);
  digitalWrite(FILTERING_LAMP_PIN, output.filtering_lamp);
  digitalWrite(BACKWASH_LAMP_PIN, output.backwash_lamp);
  digitalWrite(DRAIN_LAMP_PIN, output.drain_lamp);
}

void kirim_data_esp(){

}
void baca_data_esp(){
  data_dari_esp = Serial1.read();
  if(data_dari_esp == 1){
    input.mode_override = true;
  }
}

void resetOutput() {
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

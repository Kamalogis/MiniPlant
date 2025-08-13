/*
  Kontrol stepper (driver STEP/DIR/EN) tanpa delay() dengan FSM:
    IDLE -> SOFT_START -> RUN -> SOFT_STOP -> STOP + EMERGENCY

  Uji cepat via Serial (115200):
    '1' : start  60 rpm (CW)
    '2' : start 120 rpm (CW)
    't' : toggle arah (berlaku untuk start berikutnya)
    'x' : soft stop (decelerasi halus)
    'e' : emergency stop (hard disable)
    'r' : reset emergency -> IDLE

  Catatan:
  - Ganti pin & STEPS_PER_REV sesuai hardware (motor+microstepping).
  - EN aktif-LOW umum untuk A4988/DRV8825/TMC. Ubah jika berbeda.
*/

#include <AccelStepper.h>

// ======================== 1) VARIABEL UTAMA ========================
static float DESIRED_SPEED_RPM = 60.0f;  // target rpm default
static float ACCELERATION_RPS2 = 0.5f;   // percepatan (rev/s^2)
static bool  ARAH_CW           = true;   // true=CW, false=CCW

static const float STEPS_PER_REV = 3200.0f; // contoh: 200step * 16 micro
inline float rpmToSps(float rpm) { return (rpm * STEPS_PER_REV) / 60.0f; } // steps/s
inline float rps2ToSps2(float a) { return a * STEPS_PER_REV; }             // steps/s^2
inline float f_abs(float x) { return x < 0 ? -x : x; }

// ======================== 2) PIN ASSIGNMENT =========================
#define STEPPER_STEP_PIN 38
#define STEPPER_DIR_PIN  36
#define STEPPER_EN_PIN   40
#define STEPPER_EN_ACTIVE_LOW 1   // 1=EN aktif LOW; 0=EN aktif HIGH

// ======================== 3) FSM DEFINISI ===========================
enum StepperState {
  STEPPER_IDLE,
  STEPPER_SOFT_START,
  STEPPER_RUN,
  STEPPER_SOFT_STOP,
  STEPPER_STOP,
  STEPPER_EMERGENCY
};

static StepperState stepperState   = STEPPER_IDLE;
static bool         stepperEnabled = false;
static bool         stopRequested  = false;
static bool         emergencyLatched = false;

// Driver STEP/DIR
AccelStepper stepper(AccelStepper::DRIVER, STEPPER_STEP_PIN, STEPPER_DIR_PIN);

// ======================== 4) SETUP =================================
void setup() {
  Serial.begin(115200);
  pinMode(STEPPER_EN_PIN, OUTPUT);
  stepperEnable(false);                 // pastikan driver off saat boot

  stepper.setCurrentPosition(0);
  stepper.setAcceleration(rps2ToSps2(ACCELERATION_RPS2));
  stepper.setMaxSpeed(rpmToSps(DESIRED_SPEED_RPM));

  infoBanner();
}

// ======================== 5) LOOP ==================================
void loop() {
  handleSerialTestCommands(); // opsional untuk uji cepat
  stepperService();           // jalankan FSM stepper (non-blocking)
}

// ======================== 6) FUNGSI ================================

// ---- EN control ----
void stepperEnable(bool en) {
  stepperEnabled = en;
  // Jika EN aktif-LOW: en=true -> tulis LOW; en=false -> tulis HIGH
  digitalWrite(STEPPER_EN_PIN, STEPPER_EN_ACTIVE_LOW ? !en : en);
}

// ---- API untuk dipanggil dari sistem lain (nanti saat integrasi) ----
void stepperStart(float rpm, bool arahCW) {
  if (emergencyLatched) return;  // tolak saat emergency
  DESIRED_SPEED_RPM = constrain(rpm, 1.0f, 2000.0f);
  ARAH_CW = arahCW;

  stepper.setAcceleration(rps2ToSps2(ACCELERATION_RPS2));
  stepper.setMaxSpeed(rpmToSps(DESIRED_SPEED_RPM));

  stepperEnable(true);

  long farTarget = ARAH_CW
    ? stepper.currentPosition() + 1000000000L
    : stepper.currentPosition() - 1000000000L;

  stepper.moveTo(farTarget);
  stopRequested = false;
  stepperState  = STEPPER_SOFT_START;
}

void stepperSoftStop() {
  if (stepperState == STEPPER_RUN || stepperState == STEPPER_SOFT_START) {
    stopRequested = true;
    stepper.stop();              // minta decel ke 0 (soft stop)
    stepperState = STEPPER_SOFT_STOP;
  }
}

void stepperEmergencyStop() {
  emergencyLatched = true;
  stopRequested    = false;
  stepper.stop();                // opsional (hormati decel)
  stepperEnable(false);          // hard disable driver
  stepperState = STEPPER_EMERGENCY;
}

void stepperClearEmergency() {
  emergencyLatched = false;
  stepperState     = STEPPER_IDLE;
}

bool stepperIsRunning() {
  return (stepperState == STEPPER_SOFT_START ||
          stepperState == STEPPER_RUN ||
          stepperState == STEPPER_SOFT_STOP);
}

void stepperSetAcceleration(float accel_rps2) {
  ACCELERATION_RPS2 = max(0.1f, accel_rps2);
  stepper.setAcceleration(rps2ToSps2(ACCELERATION_RPS2));
}

void stepperSetRPM(float rpm) {
  DESIRED_SPEED_RPM = constrain(rpm, 1.0f, 2000.0f);
  stepper.setMaxSpeed(rpmToSps(DESIRED_SPEED_RPM));
}

// ---- Mesin status utama (panggil tiap loop) ----
void stepperService() {
  switch (stepperState) {
    case STEPPER_IDLE:
      if (stepperEnabled) stepperEnable(false);
      break;

    case STEPPER_SOFT_START:
      stepper.run(); // non-blocking
      if (f_abs(stepper.speed()) >= 0.25f * rpmToSps(DESIRED_SPEED_RPM)) {
        stepperState = STEPPER_RUN;
      }
      if (stopRequested) {
        stepper.stop();
        stepperState = STEPPER_SOFT_STOP;
      }
      break;

    case STEPPER_RUN:
      stepper.run();
      if (stopRequested) {
        stepper.stop();
        stepperState = STEPPER_SOFT_STOP;
      }
      break;

    case STEPPER_SOFT_STOP:
      stepper.run();
      if (stepper.distanceToGo() == 0 && f_abs(stepper.speed()) < 0.01f) {
        stepperEnable(false);
        stepperState  = STEPPER_STOP;
        stopRequested = false;
      }
      break;

    case STEPPER_STOP:
      stepperState = STEPPER_IDLE; // kembali idle setelah benar-benar stop
      break;

    case STEPPER_EMERGENCY:
      // tetap menunggu clearEmergency()
      break;
  }
}

// ======================== UTIL: SERIAL TEST ========================
void handleSerialTestCommands() {
  if (!Serial.available()) return;
  char c = (char)Serial.read();
  switch (c) {
    case '1': stepperStart(60.0f,  true);  Serial.println(F("[CMD] Start 60 rpm CW"));   break;
    case '2': stepperStart(120.0f, true);  Serial.println(F("[CMD] Start 120 rpm CW"));  break;
    case 't': ARAH_CW = !ARAH_CW;          Serial.print(F("[CMD] Toggle arah -> "));
              Serial.println(ARAH_CW ? F("CW") : F("CCW"));                              break;
    case 'x': stepperSoftStop();            Serial.println(F("[CMD] Soft Stop"));        break;
    case 'e': stepperEmergencyStop();       Serial.println(F("[CMD] EMERGENCY STOP"));   break;
    case 'r': stepperClearEmergency();      Serial.println(F("[CMD] Emergency cleared"));break;
    default: break;
  }
}

// ======================== INFO ====================================
void infoBanner() {
  Serial.println();
  Serial.println(F("=== Stepper SoftStart/SoftStop FSM (No LED) ==="));
  Serial.println(F("Cmd: '1' 60rpm, '2' 120rpm, 't' toggle arah, 'x' soft stop, 'e' emergency, 'r' reset"));
  Serial.print (F("EN active: ")); Serial.println(STEPPER_EN_ACTIVE_LOW ? F("LOW") : F("HIGH"));
  Serial.print (F("STEPS_PER_REV = ")); Serial.println(STEPS_PER_REV);
  Serial.println();
}

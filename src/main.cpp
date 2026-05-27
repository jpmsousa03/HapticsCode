/*
 * ============================================================
 *  HAPTIC EMOTION PRESETS  v6
 *  MicroSpora + DRV8316 + MT6701 + 5010 BLDC
 *  Pure spring-torque, no PID needed
 *
 *  SERIAL COMMANDS (250000 baud, LF line ending)
 *  P <0-12> select preset       e.g.  P 0
 *  G <val>  spring gain         e.g.  G 6.0
 *  V <val>  voltage limit       e.g.  V 1.5
 *  T        trigger preset now
 *  I        print status JSON
 *  H        help
 *
 *  Presets: 0=Pattern 1, 1=Pattern 2, 2=Pattern 3, 3=Pattern 4, 4=Pattern 5...
 * ============================================================
 */

#include "Arduino.h"
#include <SimpleFOC.h>
#include <SimpleFOCDrivers.h>
#include "drivers/drv8316/drv8316.h"
#include "encoders/mt6701/MagneticSensorMT6701SSI.h"

// ============================================================
//  SECTION 1 — HARDWARE
// ============================================================
BLDCMotor motor = BLDCMotor(7);

DRV8316Driver6PWM driver =
    DRV8316Driver6PWM(PHA_H, PHA_L, PHB_H, PHB_L, PHC_H, PHC_L, DRV_CS, false);

MagneticSensorMT6701SSI sensor = MagneticSensorMT6701SSI(ENC_CS);

SPIClass SPI_1(ENC_NC, ENC_SDO, ENC_CLK);
SPIClass SPI_3(PB5_ALT1, PB4_ALT1, PB3_ALT1);

Commander command = Commander(Serial);

// ============================================================
//  SECTION 2 — MOTOR TUNING
//  If motor hums    -> lower GAIN or VOLTAGE_LIMIT
//  If motor is weak -> raise VOLTAGE_LIMIT
// ============================================================
float GAIN = 6.0f;
float VOLTAGE_LIMIT = 1.5f;

// ============================================================
//  SECTION 2.5 — BASE ANGLE
// ============================================================
float initial_angle = 2.320f;

// ============================================================
//  SECTION 3 — EMOTION CONFIGS
//  This is where you tweak the feel of each emotion.
//
//  amp         how far the motor tilts [radians]
//              0.17 = 10 deg, 0.35 = 20 deg, 0.52 = 30 deg,
//              0.70 = 40 deg, 0.87 = 50 deg
//
//  tilt_ms     how long the tilt-in movement takes [ms]
//              50  = very snappy
//              300 = moderate
//              800 = slow and heavy
//
//  hold_ms     how long it stays at the tilted position [ms]
//              0    = no pause
//              500  = short pause
//              1500 = long emotional hold
//
//  return_ms   how long the return-to-center takes [ms]
//
//  overshoot   how far past center it bounces on return [rad]
//              0    = no bounce
//              0.05 = small bounce
//              0.15 = large bounce
//
//  jitter_ms   duration of each tiny jitter shake [ms]
//              0  = no jitter
//              30 = fast jitter
//              80 = slow jitter
//
//  jitter_amp  how big each jitter shake is [rad]
//              0    = no jitter
//              0.04 = subtle
//              0.10 = noticeable
// ============================================================

struct EmotionConfig
{
  float amp;
  int tilt_ms;
  int hold_ms;
  int return_ms;
  float overshoot;
  int jitter_ms;
  float jitter_amp;
};

EmotionConfig emo[13];

void setupEmotions() {
  // All overshoot and jitter values — set to 0 unless you want them
  // The amp/tilt_ms/hold_ms/return_ms values here are unused by patterns 1-4
  // because those patterns have their moves hardcoded in the build_ functions

  for (int i = 0; i < 13; i++) {
    emo[i].amp        = 0;
    emo[i].tilt_ms    = 0;
    emo[i].hold_ms    = 0;
    emo[i].return_ms  = 0;
    emo[i].overshoot  = 0;
    emo[i].jitter_ms  = 0;
    emo[i].jitter_amp = 0;
  }

  // Add overshoot or jitter to any pattern here when you want it
  // e.g. emo[0].jitter_ms = 40; emo[0].jitter_amp = 0.06f;
  emo[9].overshoot = 0.1f;  // Pattern 10 (DELIGHTED) - bouncy settle
}

// ============================================================
//  SECTION 4 — PRESET STATE MACHINE
// ============================================================
struct Step
{
  float target;
  int ms;
};

#define MAX_STEPS 120
#define REPEAT_COUNT 1

Step seq[MAX_STEPS];
int seq_len = 0;
int seq_idx = 0;
unsigned long step_start = 0;
bool preset_running = false;
bool manual_trigger = false;
int active_preset = 0;

float touch_baseline = 0.0f;
unsigned long last_touch_ms = 0;
#define TOUCH_THRESHOLD_RAD 0.01f
#define TOUCH_DEBOUNCE_MS 500

unsigned long last_broadcast_ms = 0;
#define BROADCAST_INTERVAL_MS 50

// ============================================================
//  SPRING CONTROL
// ============================================================
void spring_toward(float target)
{
  float error = target - motor.shaft_angle;
  float voltage = GAIN * error;
  if (voltage > VOLTAGE_LIMIT)
    voltage = VOLTAGE_LIMIT;
  if (voltage < -VOLTAGE_LIMIT)
    voltage = -VOLTAGE_LIMIT;
  motor.move(voltage);
}

// ============================================================
//  SECTION 5 — PRESET BUILDERS
//  You do not need to edit these — edit Section 3 instead.
//
//  Each builder follows the same pattern:
//    1. main motion (tilt/wiggle)
//    2. overshoot bounce (if overshoot > 0)
//    3. jitter shakes   (if jitter_ms > 0)
//    4. return home
// ============================================================

void repeat_into_seq(Step *tmp, int tmp_len)
{
  seq_len = 0;
  for (int r = 0; r < REPEAT_COUNT; r++)
    for (int i = 0; i < tmp_len; i++)
      seq[seq_len++] = tmp[i];
}

// Shared helper: appends overshoot + jitter + return to tmp[]
int append_ending(Step *tmp, int tmp_len, int idx)
{
  if (emo[idx].overshoot > 0)
  {
    tmp[tmp_len++] = {emo[idx].overshoot, 80};
    tmp[tmp_len++] = {-emo[idx].overshoot, 80};
    tmp[tmp_len++] = {0.0f, 100};
  }
  if (emo[idx].jitter_ms > 0)
  {
    for (int i = 0; i < 6; i++)
    {
      tmp[tmp_len++] = {emo[idx].jitter_amp, emo[idx].jitter_ms};
      tmp[tmp_len++] = {-emo[idx].jitter_amp, emo[idx].jitter_ms};
    }
  }
  tmp[tmp_len++] = {0.0f, 1000};
  return tmp_len;
}

void build_pattern0() //  delighted (before 1-angry)
{
 Step tmp[MAX_STEPS / REPEAT_COUNT];
  int tmp_len = 0;

  for (int i = 0; i < 3; i++)
  {
    // Fast swing to positive corner (from pattern1's energy)
    tmp[tmp_len++] = {0.55f, 100};

    // Basketball bounce settle at positive corner (from pattern3)
    tmp[tmp_len++] = {0.25f, 180}; // Big rebound
    tmp[tmp_len++] = {0.55f, 180};
    tmp[tmp_len++] = {0.38f, 120}; // Medium rebound
    tmp[tmp_len++] = {0.55f, 120};
    tmp[tmp_len++] = {0.48f, 70};  // Small rebound
    tmp[tmp_len++] = {0.55f, 70};
    tmp[tmp_len++] = {0.53f, 30};  // Micro-bounce to settle
    tmp[tmp_len++] = {0.55f, 30};

    // Fast swing to negative corner
    tmp[tmp_len++] = {-0.55f, 100};

    // Basketball bounce settle at negative corner
    tmp[tmp_len++] = {-0.25f, 180};
    tmp[tmp_len++] = {-0.55f, 180};
    tmp[tmp_len++] = {-0.38f, 120};
    tmp[tmp_len++] = {-0.55f, 120};
    tmp[tmp_len++] = {-0.48f, 70};
    tmp[tmp_len++] = {-0.55f, 70};
    tmp[tmp_len++] = {-0.53f, 30};
    tmp[tmp_len++] = {-0.55f, 30};
  }

  tmp[tmp_len++] = {0.0f, 400}; // Settle to center
  tmp_len = append_ending(tmp, tmp_len, 0);
  repeat_into_seq(tmp, tmp_len);
}

void build_pattern1() //unhappy (before 2-unhappy)
{
  Step tmp[MAX_STEPS / REPEAT_COUNT];
  int tmp_len = 0;

  // Look a bit away (1st time)
  tmp[tmp_len++] = {0.8f, 1000};

  tmp[tmp_len++] = {0.82f, 600};
  // Look a bit away and stop (2nd time)
  tmp[tmp_len++] = {1.6f, 1000};
  tmp[tmp_len++] = {1.5f, 1200};
  // Look a bit away and stop (2nd time)
  tmp[tmp_len++] = {2.5f, 1000};
  tmp[tmp_len++] = {2.5f, 2000};
  tmp[tmp_len++] = {0.0f, 1000};
  // Away again (other direction)
  tmp_len = append_ending(tmp, tmp_len, 1);
  repeat_into_seq(tmp, tmp_len);
}


void build_pattern5() // relaxed (before 6-relaxed)
{
  Step tmp[MAX_STEPS / REPEAT_COUNT]; 
  int tmp_len = 0;

  tmp[tmp_len++] = {6.28f, 6000};  // much slower gentle movement
  tmp[tmp_len++] = {6.28f, 1200};  // soft resting pause
  tmp[tmp_len++] = {0.0f, 7000};   // long smooth return

  tmp_len = append_ending(tmp, tmp_len, 1);
  repeat_into_seq(tmp, tmp_len);
}


void build_pattern8() // angry (before 9- angry)
{
  Step tmp[MAX_STEPS / REPEAT_COUNT]; 
  int tmp_len = 0;

  // Burst 1 — escalating erratic shakes, no holds
  tmp[tmp_len++] = {-0.75f, 80};
  tmp[tmp_len++] = { 0.95f, 65};
  tmp[tmp_len++] = {-1.00f, 55};
  tmp[tmp_len++] = { 0.80f, 70};
  tmp[tmp_len++] = {-0.90f, 50};
  tmp[tmp_len++] = { 1.00f, 60};

  // Burst 2 — direction changes become even less predictable
  tmp[tmp_len++] = {-0.85f, 45};
  tmp[tmp_len++] = { 0.70f, 75};
  tmp[tmp_len++] = {-1.00f, 40};
  tmp[tmp_len++] = { 0.60f, 55};
  tmp[tmp_len++] = {-0.95f, 50};
  tmp[tmp_len++] = { 1.00f, 45};

  // Burst 3 — peak frenzy, shortest intervals
  tmp[tmp_len++] = {-0.80f, 35};
  tmp[tmp_len++] = { 1.00f, 40};
  tmp[tmp_len++] = {-0.90f, 35};
  tmp[tmp_len++] = { 0.85f, 35};
  tmp[tmp_len++] = {-1.00f, 30};
  tmp[tmp_len++] = { 0.75f, 35};

  // Abrupt stop — anger doesn't wind down gently
  tmp[tmp_len++] = {0.0f, 200};

  tmp_len = append_ending(tmp, tmp_len, 8);
  repeat_into_seq(tmp, tmp_len);
}


void start_preset(int id)
{
  switch (id)
  {
  case 0:
    build_pattern0(); 
    break;
  case 1:
    build_pattern1();
    break;
  case 5:
    build_pattern5();
    break;
  case 8:
    build_pattern8();
    break;
  default:
    return;
  }
  seq_idx = 0;
  step_start = millis();
  preset_running = true;
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.print("{\"event\":\"preset_start\",\"id\":");
  Serial.print(id);
  Serial.println("}");
}

// ============================================================
//  SECTION 6 — PRESET TICK
// ============================================================
float preset_tick()
{
  if (!preset_running)
    return 0.0f;

  unsigned long elapsed = millis() - step_start;

  if (elapsed >= (unsigned long)seq[seq_idx].ms)
  {
    seq_idx++;
    step_start = millis();
    elapsed = 0;

    if (seq_idx >= seq_len)
    {
      preset_running = false;
      manual_trigger = false;
      touch_baseline = motor.shaft_angle;
      last_touch_ms = millis();
      digitalWrite(LED_BUILTIN, LOW);
      Serial.println("{\"event\":\"preset_done\"}");
      return 0.0f;
    }
  }

  // Smoothly interpolate the target over the duration of the step
  float start_target = (seq_idx == 0) ? 0.0f : seq[seq_idx - 1].target;
  float end_target = seq[seq_idx].target;
  int duration = seq[seq_idx].ms;

  if (duration <= 0)
    return end_target;

  float progress = (float)elapsed / (float)duration;
  return start_target + (end_target - start_target) * progress;
}

// ============================================================
//  SECTION 7 — TOUCH DETECTION
// ============================================================
bool detect_touch()
{
  if (preset_running) return false;

  float current = motor.shaft_angle;
  
  // Prevent immediate trigger on startup by fast-tracking the baseline while the spring settles
  static unsigned long loop_start_ms = 0;
  if (loop_start_ms == 0) loop_start_ms = millis();
  
  unsigned long now = millis();

  // Also delay touch sensing by 1 second after a preset finishes to prevent recoil triggers
  if (now - loop_start_ms < 2000 || now - last_touch_ms < 1000) {
    touch_baseline = current;
    return false;
  }

  float delta = fabsf(current - touch_baseline);

  // MUCH slower baseline tracking (0.0001 instead of 0.001)
  // This ensures the baseline stays put while you are actively pushing.
  touch_baseline += (current - touch_baseline) * 0.0001f;

  // Check if the movement is significant enough
  if (delta > TOUCH_THRESHOLD_RAD) 
  {
    if ((now - last_touch_ms) > TOUCH_DEBOUNCE_MS)
    {
      last_touch_ms = now;
      return true;
    }
  }
  return false;
}

// ============================================================
//  SECTION 8 — SERIAL COMMANDS
// ============================================================
void onPreset(char *cmd)
{
  int id = atoi(cmd);
  if (id >= 0 && id <= 12)
  {
    active_preset = id;
    Serial.print("{\"preset\":");
    Serial.print(id);
    Serial.println("}");
  }
}
void onGain(char *cmd)
{
  GAIN = constrain(atof(cmd), 0.5f, 20.0f);
  Serial.print("{\"gain\":");
  Serial.print(GAIN, 2);
  Serial.println("}");
}
void onVoltage(char *cmd)
{
  VOLTAGE_LIMIT = constrain(atof(cmd), 0.1f, 3.0f);
  motor.voltage_limit = VOLTAGE_LIMIT;
  Serial.print("{\"voltage_limit\":");
  Serial.print(VOLTAGE_LIMIT, 2);
  Serial.println("}");
}
void onInitialAngle(char *cmd)
{
  initial_angle = atof(cmd);
  Serial.print("{\"initial_angle\":");
  Serial.print(initial_angle, 4);
  Serial.println("}");
}
void onTrigger(char *cmd)
{
  (void)cmd;
  if (!preset_running)
    manual_trigger = true;
}
void onInfo(char *cmd)
{
  (void)cmd;
  Serial.print("{\"preset\":");
  Serial.print(active_preset);
  Serial.print(",\"gain\":");
  Serial.print(GAIN, 2);
  Serial.print(",\"voltage_limit\":");
  Serial.print(VOLTAGE_LIMIT, 2);
  Serial.print(",\"angle\":");
  Serial.print(motor.shaft_angle, 4);
  Serial.print(",\"running\":");
  Serial.print(preset_running ? "true" : "false");
  Serial.println("}");
}
void onHelp(char *cmd)
{
  (void)cmd;
  Serial.println("Commands (LF ending):");
  Serial.println("  P <0-5>  preset   e.g. P 0");
  Serial.println("  G <val>  gain     e.g. G 6.0");
  Serial.println("  V <val>  voltage  e.g. V 1.5");
  Serial.println("  A <val>  angle    e.g. A 1.0 (initial angle)");
  Serial.println("  T        trigger");
  Serial.println("  I        status");
  Serial.println("  H        help");
  Serial.println("Presets: 0=ANGRY 1=UNHAPPY 2=DELIGHTED 3=RELAXED 4=CURIOUS 5=MY_PRESET");
}

// ============================================================
//  SECTION 9 — SETUP
// ============================================================
void setup()
{
  setupEmotions();

  delay(1500);
  Serial.begin(250000);
  delay(200);
  SimpleFOCDebug::enable(&Serial);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  pinMode(ENC_CS, OUTPUT);
  digitalWrite(ENC_CS, HIGH);
  sensor.init(&SPI_1);
  motor.linkSensor(&sensor);

  driver.voltage_power_supply = 12.0f;
  driver.pwm_frequency = 25000;
  driver.dead_zone = 0.001f;
  driver.init(&SPI_3);
  driver.setSlew(Slew_200Vus);
  driver.setCurrentSenseGain(Gain_0V375);
  driver.setOCPMode(NoAction);
  driver.setPWMMode(PWM6_Mode);
  driver.setBuckVoltage(VB_5V);
  motor.linkDriver(&driver);

  motor.controller = MotionControlType::torque;
  motor.torque_controller = TorqueControlType::voltage;
  motor.voltage_limit = VOLTAGE_LIMIT;
  motor.voltage_sensor_align = 1.5f;
  motor.velocity_limit = 100.0f;
  motor.LPF_velocity.Tf = 0.01f;

  motor.useMonitoring(Serial);
  motor.monitor_downsample = 0;

  if (!motor.init())
  {
    Serial.println("{\"error\":\"motor init failed\"}");
    while (1)
      delay(100);
  }
  motor.initFOC();

  command.add('P', onPreset, "preset");
  command.add('G', onGain, "gain");
  command.add('V', onVoltage, "voltage limit");
  command.add('A', onInitialAngle, "initial angle");
  command.add('T', onTrigger, "trigger");
  command.add('I', onInfo, "info");
  command.add('H', onHelp, "help");

  touch_baseline = motor.shaft_angle;

  Serial.println("{\"status\":\"ready\"}");
  onHelp(nullptr);
  digitalWrite(LED_BUILTIN, LOW);
}

// ============================================================
//  SECTION 10 — MAIN LOOP
// ============================================================
void loop()
{
  motor.loopFOC();

  static float current_target = 0.0f;
  static bool first_loop = true;
  if (first_loop) {
    // On the very first loop after setup, set our target to wherever the motor landed
    current_target = motor.shaft_angle;
    first_loop = false;
  }

  float desired_target = preset_running ? (preset_tick() + initial_angle) : initial_angle;

  if (!preset_running) {
    // Slowly glide from the current position back to initial_angle center
    // 0.001f determines the speed. Lower = slower. 
    if (current_target > desired_target) {
      current_target -= 0.001f;
      if (current_target < desired_target) current_target = desired_target;
    } else if (current_target < desired_target) {
      current_target += 0.001f;
      if (current_target > desired_target) current_target = desired_target;
    }
  } else {
    // Follow the preset instantly when active
    current_target = desired_target;
  }

  spring_toward(current_target);

  bool touched = detect_touch();

  if (!preset_running && (touched || manual_trigger))
  {
    start_preset(active_preset);
  }

  unsigned long now = millis();
  if (now - last_broadcast_ms >= BROADCAST_INTERVAL_MS)
  {
    last_broadcast_ms = now;
    Serial.print("{\"angle\":");
    Serial.print(motor.shaft_angle, 3);
    Serial.print(",\"vel\":");
    Serial.print(motor.shaft_velocity, 2);
    Serial.println("}");
  }

  command.run();
}
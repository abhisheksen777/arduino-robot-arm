/******************************************************************************
  Author: Smartbuilds.io
  YouTube: https://www.youtube.com/channel/UCGxwyXJWEarxh2XWqvygiIg
  Fork your own version: https://github.com/EbenKouao/arduino-robot-arm
  Check out the full article: https://smartbuilds.io/diy-arduino-robot-arm-controlled-hand-gestures/
  Date: 06/01/2021
  Robot Arm
  Version 1.0
  Creator: smartbuilds.io
  Description: Robotic Arm Mark II - Servo Motor
  Modification: Base rotation is an MG995 positional servo on PCA9685 CH0
                (replaces the original NEMA 17 stepper + driver). See
                HARDWARE_WIRING.md for wiring, power and calibration.

  To use the module connect it to your Arduino as follows:

  PCA9685...........Uno/Nano
  GND...............GND (also tie to external servo supply GND)
  OE................N/A
  SCL...............A5
  SDA...............A4
  VCC...............5V  (logic only)
  V+................External regulated 5-6V servo supply (NOT the Arduino 5V)

  PCA9685 channel map (this build):
  CH0 Base (MG995)  CH1 Shoulder  CH2 Elbow  CH3 Wrist 1  CH4 Wrist 2  CH5 Gripper
  Commands: T/t (also S/O) Base  P/p Shoulder  C/c Elbow  U/D Wrist 1  L/R Wrist 2  F/f Gripper

******************************************************************************/

/* Include the HCPCA9685 library */
#include "HCPCA9685.h"

/* I2C slave address for the device/module. For the HCMODU0097 the default I2C address
   is 0x40 */
#define  I2CAdd 0x40

/* Create an instance of the library */
HCPCA9685 HCPCA9685(I2CAdd);

//PCA9685 channel assignment - change here if you rewire a servo
const int CH_BASE = 0;        // MG995 base rotation
const int CH_SHOULDER = 1;
const int CH_ELBOW = 2;
const int CH_WRIST_1 = 3;     // wrist up / down (U / D commands)
const int CH_WRIST_2 = 4;     // wrist rotation (R / L commands)
const int CH_GRIPPER = 5;

//initial parking position of the motor
const int servo_joint_elbow_parking_pos = 60;
const int servo_joint_1_parking_pos = 70;
const int servo_joint_2_parking_pos = 47;
const int servo_joint_3_parking_pos = 63;
const int servo_joint_4_parking_pos = 63;

//Degree of robot servo sensitivity - Intervals
int servo_joint_elbow_pos_increment = 20;
int servo_joint_1_pos_increment = 20;
int servo_joint_2_pos_increment = 50;
int servo_joint_3_pos_increment = 60;
int servo_joint_4_pos_increment = 40;

//Keep track of the current value of the motor positions
int servo_joint_elbow_parking_pos_i = servo_joint_elbow_parking_pos;
int servo_joint_1_parking_pos_i = servo_joint_1_parking_pos;
int servo_joint_2_parking_pos_i = servo_joint_2_parking_pos;
int servo_joint_3_parking_pos_i = servo_joint_3_parking_pos;
int servo_joint_4_parking_pos_i = servo_joint_4_parking_pos;


//Minimum and maximum angle of servo motor
int servo_joint_elbow_min_pos = 10;
int servo_joint_elbow_max_pos = 180;


int servo_joint_1_min_pos = 10;
int servo_joint_1_max_pos = 400;

int servo_joint_2_min_pos = 10;
int servo_joint_2_max_pos = 380;

int servo_joint_3_min_pos = 10;
int servo_joint_3_max_pos = 380;

int servo_joint_4_min_pos = 10;
int servo_joint_4_max_pos = 120;

int servo_joint_1_pos = 0;
int servo_joint_2_pos = 0;
int servo_joint_3_pos = 0;
int servo_joint_4_pos = 0;

char state = 0; // Changes value from ASCII to char
int response_time = 5;
int response_time_4 = 2;
int loop_check = 0;
int response_time_fast = 20;
int action_delay = 600;

//Posiion of motor for example demos
unsigned int Pos;

/* MG995 base rotation (PCA9685 CH0)
   All values are HCPCA9685 position units (0-480), NOT degrees.
   The values below are CONSERVATIVE, UNCALIBRATED defaults: a narrow window
   around mid-range. Widen them step by step using
   code/test-code/mg995_base_calibration, then copy the results here. */
const int servo_base_parking_pos = 240;  // startup position (must be within min/max)
int servo_base_min_pos = 140;            // software limit, "left" or "right" end
int servo_base_max_pos = 340;            // software limit, opposite end
int servo_base_pos_increment = 10;       // units moved per T / t / S / O command
int servo_base_step_delay = 5;           // ms per unit while moving (bigger = slower)
// +1: baseRotateLeft() increases the position value, -1: it decreases it.
// Flip this if the base turns the wrong way for 'T' / 'S' (no rewiring needed).
const int servo_base_left_direction = 1;
int servo_base_pos_i = servo_base_parking_pos; // current base position

//Clamp to [mn, mx] using long math, so a stepped value can never overshoot a limit
//or go negative (a negative int would wrap to a huge unsigned servo position).
int clampJoint(long v, int mn, int mx) {
  return (int)constrain(v, (long)mn, (long)mx);
}

//The ONLY way joints 1-4 / elbow reach the PCA9685: every write is clamped to the joint's limits.
void writeJoint(int ch, long pos, int mn, int mx) {
  HCPCA9685.Servo(ch, (unsigned int)clampJoint(pos, mn, mx));
}

//Every limit pair must satisfy 0 <= min < max, or the sketch refuses to move anything.
bool limitsValid() {
  return servo_joint_elbow_min_pos >= 0 && servo_joint_elbow_min_pos < servo_joint_elbow_max_pos
      && servo_joint_1_min_pos >= 0 && servo_joint_1_min_pos < servo_joint_1_max_pos
      && servo_joint_2_min_pos >= 0 && servo_joint_2_min_pos < servo_joint_2_max_pos
      && servo_joint_3_min_pos >= 0 && servo_joint_3_min_pos < servo_joint_3_max_pos
      && servo_joint_4_min_pos >= 0 && servo_joint_4_min_pos < servo_joint_4_max_pos
      && servo_base_min_pos >= 0 && servo_base_min_pos < servo_base_max_pos;
}

void setup()
{
  /* Invalid limits: send NO servo pulse at all, report, and halt. */
  if (!limitsValid()) {
    Serial.begin(4800);
    Serial.println(F("ERROR: invalid servo limits (need 0 <= min < max). Halted."));
    while (true) {}
  }

  /* Initialise the library and set it to 'servo mode' */
  HCPCA9685.Init(SERVO_MODE);

  /* Wake the device up */
  HCPCA9685.Sleep(false);

  /* Move the base to its parking position (clamped to the base limits).
     Until now the PCA9685 outputs no pulse, so the MG995 is idle.
     NOTE: this first move is OPEN-LOOP. There is no position feedback, so the
     firmware cannot know where the horn really is; the servo jumps from its
     physical position to the parking value at full speed (the ramp in
     baseMoveTo() only applies to later moves). Keep the horn off / the arm
     clear on first power-up, and set servo_base_parking_pos from calibration. */
  servo_base_pos_i = constrain(servo_base_parking_pos, servo_base_min_pos, servo_base_max_pos);
  HCPCA9685.Servo(CH_BASE, servo_base_pos_i);

  Serial.begin(4800); // Initialise default communication rate of the Bluetooth module


  delay(3000);
  //wakeUp(); -- Uncomment for Example Demo 1
  //flexMotors(); -- Uncomment for Example Demo 1

}


void loop() {


  if (Serial.available() > 0) { // Checks whether data is coming from the serial port

    state = Serial.read(); // Reads the data from the serial port
    Serial.print(state); // Prints out the value sent


    //For the naming of the motors, refer to the article / tutorial
    //Move (Base Rotation) MG995 Servo Left  ('T' = glove thumb flex above threshold, 'S' = original phone command)
    if (state == 'T' || state == 'S') {
      baseRotateLeft();
      delay(response_time);
    }

    //Move (Base Rotation) MG995 Servo Right ('t' = glove thumb flex below threshold)
    if (state == 't' || state == 'O') { // 'O' = original phone command
      baseRotateRight();
      delay(response_time);
    }


    //Move Shoulder Down
    if (state == 'p') {

      shoulderServoForward();
      delay(response_time);

    }

    //Move Shoulder Up
    if (state == 'P') {

      shoulderServoBackward();
      delay(response_time);

    }

    //Move Elbow Down
    if (state == 'c') {

      elbowServoForward();
      delay(response_time);

    }

    //Move Elbow Up
    if (state == 'C') {

      elbowServoBackward();
      delay(response_time);

    }


    //Move Wrist 1 UP
    if (state == 'U') {

      wristServo1Backward();
      delay(response_time);
    }

    //Move Move Wrist 1 Down
    if (state == 'D' || state == 'G') { // 'G' = legacy glove code, same as 'D'

      wristServo1Forward();
      delay(response_time);

    }


    //Move Wrist 2 Clockwise
    if (state == 'R') {

      wristServoCW();
      delay(response_time);

    }

    //Move Wrist 2 Counter-CW
    if (state == 'L') {

      wristServoCCW();
      delay(response_time);

    }


    //Open Claw Grip
    if (state == 'F') {
      gripperServoBackward();
      delay(response_time);

    }

    //Close Claw Grip
    if (state == 'f') {
      gripperServoForward();
      delay(response_time);
    }


  }
}

//Boiler plate function - These functions move the servo motors in a specific direction for a duration.

void gripperServoForward() {

  if (servo_joint_4_parking_pos_i > servo_joint_4_min_pos) {
    writeJoint(CH_GRIPPER, servo_joint_4_parking_pos_i, servo_joint_4_min_pos, servo_joint_4_max_pos);
    delay(response_time); //Delay the time takee to turn the servo by the given increment
    Serial.println(servo_joint_4_parking_pos_i);
    servo_joint_4_parking_pos_i = clampJoint((long)servo_joint_4_parking_pos_i - servo_joint_4_pos_increment, servo_joint_4_min_pos, servo_joint_4_max_pos);

  }
}

void gripperServoBackward() {

  if (servo_joint_4_parking_pos_i < servo_joint_4_max_pos) {
    writeJoint(CH_GRIPPER, servo_joint_4_parking_pos_i, servo_joint_4_min_pos, servo_joint_4_max_pos);
    delay(response_time);
    Serial.println(servo_joint_4_parking_pos_i);
    servo_joint_4_parking_pos_i = clampJoint((long)servo_joint_4_parking_pos_i + servo_joint_4_pos_increment, servo_joint_4_min_pos, servo_joint_4_max_pos);

  }

}

void wristServoCW() {

  if (servo_joint_3_parking_pos_i > servo_joint_3_min_pos) {
    writeJoint(CH_WRIST_2, servo_joint_3_parking_pos_i, servo_joint_3_min_pos, servo_joint_3_max_pos);
    delay(response_time_4);
    Serial.println(servo_joint_3_parking_pos_i);
    servo_joint_3_parking_pos_i = clampJoint((long)servo_joint_3_parking_pos_i - servo_joint_3_pos_increment, servo_joint_3_min_pos, servo_joint_3_max_pos);

  }

}

void wristServoCCW() {

  if (servo_joint_3_parking_pos_i < servo_joint_3_max_pos) {
    writeJoint(CH_WRIST_2, servo_joint_3_parking_pos_i, servo_joint_3_min_pos, servo_joint_3_max_pos);
    delay(response_time_4);
    Serial.println(servo_joint_3_parking_pos_i);
    servo_joint_3_parking_pos_i = clampJoint((long)servo_joint_3_parking_pos_i + servo_joint_3_pos_increment, servo_joint_3_min_pos, servo_joint_3_max_pos);

  }

}

void wristServo1Forward() {

  if (servo_joint_2_parking_pos_i < servo_joint_2_max_pos) {
    writeJoint(CH_WRIST_1, servo_joint_2_parking_pos_i, servo_joint_2_min_pos, servo_joint_2_max_pos);
    delay(response_time);
    Serial.println(servo_joint_2_parking_pos_i);

    servo_joint_2_parking_pos_i = clampJoint((long)servo_joint_2_parking_pos_i + servo_joint_2_pos_increment, servo_joint_2_min_pos, servo_joint_2_max_pos);

  }


}

void wristServo1Backward() {

  if (servo_joint_2_parking_pos_i > servo_joint_2_min_pos) {
    writeJoint(CH_WRIST_1, servo_joint_2_parking_pos_i, servo_joint_2_min_pos, servo_joint_2_max_pos);
    delay(response_time);
    Serial.println(servo_joint_2_parking_pos_i);

    servo_joint_2_parking_pos_i = clampJoint((long)servo_joint_2_parking_pos_i - servo_joint_2_pos_increment, servo_joint_2_min_pos, servo_joint_2_max_pos);

  }

}


void elbowServoForward() {

  if (servo_joint_elbow_parking_pos_i < servo_joint_elbow_max_pos) {
    writeJoint(CH_ELBOW, servo_joint_elbow_parking_pos_i, servo_joint_elbow_min_pos, servo_joint_elbow_max_pos);

    delay(response_time);
    Serial.println(servo_joint_elbow_parking_pos_i);

    servo_joint_elbow_parking_pos_i = clampJoint((long)servo_joint_elbow_parking_pos_i + servo_joint_elbow_pos_increment, servo_joint_elbow_min_pos, servo_joint_elbow_max_pos);

  }
}

void elbowServoBackward() {
  if (servo_joint_elbow_parking_pos_i > servo_joint_elbow_min_pos) {
    writeJoint(CH_ELBOW, servo_joint_elbow_parking_pos_i, servo_joint_elbow_min_pos, servo_joint_elbow_max_pos);

    delay(response_time);
    Serial.println(servo_joint_elbow_parking_pos_i);


    servo_joint_elbow_parking_pos_i = clampJoint((long)servo_joint_elbow_parking_pos_i - servo_joint_elbow_pos_increment, servo_joint_elbow_min_pos, servo_joint_elbow_max_pos);

  }

}

void shoulderServoForward() {

  if (servo_joint_1_parking_pos_i < servo_joint_1_max_pos) {
    writeJoint(CH_SHOULDER, servo_joint_1_parking_pos_i, servo_joint_1_min_pos, servo_joint_1_max_pos);
    delay(response_time);
    Serial.println(servo_joint_1_parking_pos_i);

    servo_joint_1_parking_pos_i = clampJoint((long)servo_joint_1_parking_pos_i + servo_joint_1_pos_increment, servo_joint_1_min_pos, servo_joint_1_max_pos);

  }

}

void shoulderServoBackward() {


  if (servo_joint_1_parking_pos_i > servo_joint_1_min_pos) {
    writeJoint(CH_SHOULDER, servo_joint_1_parking_pos_i, servo_joint_1_min_pos, servo_joint_1_max_pos);
    delay(response_time);
    Serial.println(servo_joint_1_parking_pos_i);

    servo_joint_1_parking_pos_i = clampJoint((long)servo_joint_1_parking_pos_i - servo_joint_1_pos_increment, servo_joint_1_min_pos, servo_joint_1_max_pos);

  }
}

//Move the MG995 to a target position one unit at a time (smooth, speed set by
//servo_base_step_delay). The target is always clamped to the min/max limits.
void baseMoveTo(long requested) {
  int target = clampJoint(requested, servo_base_min_pos, servo_base_max_pos);
  while (servo_base_pos_i != target) {
    servo_base_pos_i += (target > servo_base_pos_i) ? 1 : -1;
    HCPCA9685.Servo(CH_BASE, servo_base_pos_i);
    delay(servo_base_step_delay);
  }
}

void baseRotateLeft() {
  baseMoveTo((long)servo_base_pos_i + (long)servo_base_left_direction * servo_base_pos_increment);
  Serial.println(servo_base_pos_i);
}


void baseRotateRight() {
  baseMoveTo((long)servo_base_pos_i - (long)servo_base_left_direction * servo_base_pos_increment);
  Serial.println(servo_base_pos_i);
}

void wakeUp() {

  //Pre-Program Function - Wake Up Robot on Start

  if (loop_check == 0) {

    //    //Shoulder Raise
    for (Pos = 0; Pos < 10; Pos++)
    {

      writeJoint(CH_ELBOW, Pos, servo_joint_elbow_min_pos, servo_joint_elbow_max_pos);
      delay(response_time_fast);
    }

    //  //Move Elbow Backwards
    for (Pos = 400; Pos > 390; Pos--)
    {

      writeJoint(CH_SHOULDER, Pos, servo_joint_1_min_pos, servo_joint_1_max_pos);

      delay(response_time_fast);
    }

    //Move Wrist 1 Forward
    for (Pos = 10; Pos < 20; Pos++)
    {
      writeJoint(CH_WRIST_1, Pos, servo_joint_2_min_pos, servo_joint_2_max_pos);
      delay(response_time);
    }

    //Move Wrist 2 Backwards
    for (Pos = 380; Pos > 50; Pos--)
    {
      writeJoint(CH_WRIST_2, Pos, servo_joint_3_min_pos, servo_joint_3_max_pos);
      delay(response_time);
    }

    //Move Wrist 2 Backwards
    for (Pos = 50; Pos < 150; Pos++)
    {
      writeJoint(CH_WRIST_2, Pos, servo_joint_3_min_pos, servo_joint_3_max_pos);
      delay(response_time);
    }

    //Move Wrist 1 Forward
    for (Pos = 19; Pos < 100; Pos++)
    {
      writeJoint(CH_WRIST_1, Pos, servo_joint_2_min_pos, servo_joint_2_max_pos);
      delay(response_time);
    }
    loop_check = 0;

  }
}

void flexMotors() {

  //Example Demo Pre-program Function to Make Robot Wake Up (Motor by Motor)

  if (loop_check == 0) {

    delay(action_delay);

    //Move Wrist 1 Forward
    for (Pos = 100; Pos > 10; Pos--)
    {
      writeJoint(CH_WRIST_1, Pos, servo_joint_2_min_pos, servo_joint_2_max_pos);
      delay(10);
    }

    delay(action_delay);

    //Move Wrist 1 Forward
    for (Pos = 10; Pos < 70; Pos++)
    {
      writeJoint(CH_WRIST_1, Pos, servo_joint_2_min_pos, servo_joint_2_max_pos);
      delay(10);
    }

    delay(action_delay);

    baseRotateLeft();
    delay(action_delay);


    //Move Wrist 2 Backwards
    for (Pos = 200; Pos < 380; Pos++)
    {
      writeJoint(CH_WRIST_2, Pos, servo_joint_3_min_pos, servo_joint_3_max_pos);
      delay(10);
    }

    delay(action_delay);

    loop_check = 1;


  }
}

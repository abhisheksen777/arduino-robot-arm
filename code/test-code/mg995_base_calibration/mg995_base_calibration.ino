/******************************************************************************
  Description: MG995 base servo calibration (PCA9685 CH0, HCPCA9685 library)

  Finds the safe min / max / centre HCPCA9685 position values for the base.
  Values are HCPCA9685 position units (0-480), NOT degrees.

  SAFETY: do this with the servo horn REMOVED or the base free to move and
  no arm linkage that can jam. Power the MG995 from the external 5-6V supply.
  Disconnect the HC-05 from D0/D1 (it shares the USB serial port).

  Open Serial Monitor at 9600 baud, line ending "No line ending".
    d / a : +10 / -10      w / s : +1 / -1
    c     : go to centre (240)
    p     : print current value
  Start at 240 and creep outward. Stop when the servo hums/buzzes against its
  end stop or the mechanics bind, then back off ~10 units. Write down:
    min, max, the value where the base points straight ahead (parking).
  Copy them into servo_base_min_pos / servo_base_max_pos /
  servo_base_parking_pos in arduino_robot_arm_code.ino.
******************************************************************************/

#include "HCPCA9685.h"

#define I2CAdd 0x40
#define CH_BASE 0

HCPCA9685 HCPCA9685(I2CAdd);

const int POS_START = 240;
int pos = POS_START;

void setup() {
  Serial.begin(9600);
  HCPCA9685.Init(SERVO_MODE);
  HCPCA9685.Sleep(false);
  HCPCA9685.Servo(CH_BASE, pos);
  Serial.println(F("MG995 calibration. d/a=+-10  w/s=+-1  c=centre  p=print"));
  Serial.println(pos);
}

void loop() {
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'd') pos += 10;
    else if (c == 'a') pos -= 10;
    else if (c == 'w') pos += 1;
    else if (c == 's') pos -= 1;
    else if (c == 'c') pos = POS_START;
    else if (c != 'p') return;

    pos = constrain(pos, 0, 480);
    HCPCA9685.Servo(CH_BASE, pos);
    Serial.println(pos);
  }
}

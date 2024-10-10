#include "stmpu6050.h"
#include <SoftwareSerial.h>

SMPU6050 mpu6050;

// Khai báo đối tượng SoftwareSerial
SoftwareSerial Bluetooth(2, 3); // RX, TX

#define Step_2       5   // D5
#define Step_1       7   // D7
#define Dir_2        4   // D4
#define Dir_1        6   // D6

void pin_INI() {
  pinMode(Step_1, OUTPUT);
  pinMode(Step_2, OUTPUT);
  pinMode(Dir_1, OUTPUT);
  pinMode(Dir_2, OUTPUT);
}

void timer_INI() {
  TCCR2A = 0;
  TCCR2B = 0;
  TCCR2B |= (1 << CS21);
  OCR2A = 39;
  TCCR2A |= (1 << WGM21);
  TIMSK2 |= (1 << OCIE2A);
}

int8_t Dir_M1, Dir_M2;
volatile int Count_timer1, Count_timer2;
volatile int32_t Step1, Step2;
volatile int Count_TIMER1_TOP, Count_TIMER1_BOT;
volatile int Count_TIMER2_TOP, Count_TIMER2_BOT;
float inputL, inputR, OutputL, OutputR, I_L, I_R, input_lastL, input_lastR;
float Offset = 4;
float Kp = 105;
float Ki = 20;
float Kd = 3;
float Vgo_L = 0; // Declare and initialize
float Vgo_R = 0;  // Declare and initialize
char BluetoothChar;
char lastCommand = ' '; // To keep track of the last command

// New variables for controlling turning behavior
unsigned long lastTurnTime = 0; // To track the time of the last turn
const unsigned long turnDuration = 500; // Duration of turn in milliseconds
bool isTurning = false;

ISR(TIMER2_COMPA_vect) {
  // Xử lý động cơ 1
  if (Dir_M1 != 0) { 
    Count_timer1++;
    if (Count_timer1 <= Count_TIMER1_TOP) 
      PORTD |= (1 << Step_1);
    else 
      PORTD &= ~(1 << Step_1);
    if (Count_timer1 > Count_TIMER1_BOT) {
      Count_timer1 = 0;
      if (Dir_M1 > 0) 
        Step1++;
      else if (Dir_M1 < 0) 
        Step1--;
    }
  }

  // Xử lý động cơ 2
  if (Dir_M2 != 0) {
    Count_timer2++;
    if (Count_timer2 <= Count_TIMER2_TOP) 
      PORTD |= (1 << Step_2);
    else 
      PORTD &= ~(1 << Step_2);
    if (Count_timer2 > Count_TIMER2_BOT) {
      Count_timer2 = 0;
      if (Dir_M2 > 0) 
        Step2++;
      else if (Dir_M2 < 0) 
        Step2--;
    }
  }
}

void Speed_M1(int16_t x) {
  if (x < 0) {
    Dir_M1 = -1;
    PORTD &= ~(1 << Dir_1);
  }
  else if (x > 0) {
    Dir_M1 = 1;
    PORTD |= (1 << Dir_1);
  }
  else 
    Dir_M1 = 0;

  Count_TIMER1_BOT = abs(x);
  Count_TIMER1_TOP = Count_TIMER1_BOT / 2;
}

void Speed_L(int16_t x) {
  if (x < 0) {
    Dir_M2 = -1;
    PORTD &= ~(1 << Dir_2);
  }
  else if (x > 0) {
    Dir_M2 = 1;
    PORTD |= (1 << Dir_2);
  }
  else 
    Dir_M2 = 0;

  Count_TIMER2_BOT = abs(x);
  Count_TIMER2_TOP = Count_TIMER2_BOT / 2;
}

void setup() {
  mpu6050.init(0x68);
  Bluetooth.begin(9600); // Khởi tạo SoftwareSerial với tốc độ 9600 bps
  Serial.begin(9600);    // Khởi tạo Serial Monitor
  pin_INI();
  timer_INI();
  // Đặt giá trị khởi tạo cho các biến tốc độ và điều khiển
  Vgo_L = 0;
  Vgo_R = 0;
  OutputL = 0;
  OutputR = 0;
  
  // Cân chỉnh MPU6050 và cho phép hệ thống ổn định trước khi bắt đầu
  delay(1000);
  Serial.println("System ready");
}

// New variables for controlling forward and backward motion timing
unsigned long lastMoveTime = 0; // To track the start of the move
const unsigned long moveDuration = 2000; // Duration of move in milliseconds
const unsigned long stopDuration = 150; // Duration to stop in milliseconds
bool isMoving = false; // Track if the robot is currently moving
bool isStopping = false; // Track if the robot is currently stopping

void loop() {
  float AngleX = mpu6050.getXAngle();

  if (Bluetooth.available() > 0) {
    BluetoothChar = Bluetooth.read();
  }

  // Update the last command before processing the new one
  if (BluetoothChar != ' ') {
    lastCommand = BluetoothChar;
  }

  unsigned long currentTime = millis();

  // Handle forward movement ('G')
  if (lastCommand == 'G') {
    if (!isMoving) {
      isMoving = true;
      lastMoveTime = currentTime; // Mark the start of movement
    }

    // Increase speed gradually for the first 1.5 seconds
    if (currentTime - lastMoveTime <= moveDuration) {
      if (Vgo_L < 1.4 && Vgo_R < 1.4) {
        Vgo_L += 0.1;
        Vgo_R += 0.1;
      }
    } else if (currentTime - lastMoveTime <= moveDuration + stopDuration) {
      // After 1.5 seconds, stop for 200ms
      Vgo_L = 0;
      Vgo_R = 0;
      isStopping = true;
    } else {
      // After stopping, reset everything
      isMoving = false;
      isStopping = false;
    }

    Ki = 8;
    isTurning = false; // Ensure turning is off when moving forward
  }

  // Handle backward movement ('B')
  else if (lastCommand == 'B') {
    if (!isMoving) {
      isMoving = true;
      lastMoveTime = currentTime; // Mark the start of movement
    }

    // Increase reverse speed gradually for the first 1.5 seconds
    if (currentTime - lastMoveTime <= moveDuration) {
      if (Vgo_L > -1.8 && Vgo_R > -1.8) {
        Vgo_L -= 0.21;
        Vgo_R -= 0.21;
      }
    } else if (currentTime - lastMoveTime <= moveDuration + stopDuration) {
      // After 1.5 seconds, stop for 200ms
      Vgo_L = 0;
      Vgo_R = 0;
      isStopping = true;
    } else {
      // After stopping, reset everything
      isMoving = false;
      isStopping = false;
    }

    Ki = 8;
    isTurning = false; // Ensure turning is off when moving backward
  }

  // Handle left turn ('L')
  else if (lastCommand == 'L') {
    if (Vgo_L > -0.24) Vgo_L -= 0.02; // Giảm tốc độ bên trái
    if (Vgo_R < 0.24) Vgo_R += 0.02;  // Tăng tốc độ bên phải
    Ki = 0;
    isTurning = true; // Đánh dấu đang trong trạng thái quay
  }

  // Handle right turn ('R')
  else if (lastCommand == 'R') {
    if (Vgo_L < 0.24) Vgo_L += 0.02;  // Tăng tốc độ bên trái
    if (Vgo_R > -0.24) Vgo_R -= 0.02;  // Giảm tốc độ bên phải
    Ki = 0;
    isTurning = true; // Đánh dấu đang trong trạng thái quay
  }

  // Handle stop ('S')
  else if (lastCommand == 'S') {
    Vgo_L = 0;
    Vgo_R = 0;
    if (lastCommand == 'G' || lastCommand == 'B') {
      Ki = 20; // Cho phép tích hợp Ki khi dừng lại
    } else if (lastCommand == 'L' || lastCommand == 'R') {
      Ki = 5; // Ki = 0 khi dừng quay
    }
    isTurning = false; // Dừng quay khi dừng lại
  }

  else {
    // Nếu không có lệnh điều khiển, giữ tốc độ ở mức 0
    Vgo_L = 0;
    Vgo_R = 0;
    isTurning = false; // Đảm bảo dừng quay khi không có lệnh
  }

  inputL = AngleX + Offset - Vgo_L;
  I_L += inputL;
  OutputL = Kp * inputL + Ki * I_L + Kd * (inputL - input_lastL);
  input_lastL = inputL;
  if (OutputL > -5 && OutputL < 5) OutputL = 0;
  OutputL = constrain(OutputL, -400, 400);

  inputR = AngleX + Offset - Vgo_R;
  I_R += inputR;
  OutputR = Kp * inputR + Ki * I_R + Kd * (inputR - input_lastR);
  input_lastR = inputR;
  if (OutputR > -5 && OutputR < 5) OutputR = 0;
  OutputR = constrain(OutputR, -400, 400);

  float M_L, M_R;
  if (OutputL > 0) M_L = 405 - (1 / (OutputL + 9)) * 5500;
  else if (OutputL < 0) M_L = -405 - (1 / (OutputL - 9)) * 5500;
  else M_L = 0;

  if (OutputR > 0) M_R = 405 - (1 / (OutputR + 9)) * 5500;
  else if (OutputR < 0) M_R = -405 - (1 / (OutputR - 9)) * 5500;
  else M_R = 0;

  float MotorL, MotorR;
  if (M_L > 0) MotorL = 400 - M_L;
  else if (M_L < 0) MotorL = -400 - M_L;
  else MotorL = 0;

  if (M_R > 0) MotorR = 400 - M_R;
  else if (M_R < 0) MotorR = -400 - M_R;
  else MotorR = 0;

  Serial.println(OutputL);
  Serial.println(OutputR);

  Speed_L(MotorL);
  Speed_M1(MotorR); // Đổi từ Speed_R thành Speed_M1
}

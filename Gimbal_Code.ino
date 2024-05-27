#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"
#include "Wire.h"

#include <Servo.h>

bool dmpReady = false;
uint8_t mpuIntStatus;
uint8_t devStatus;
uint16_t packetSize;
uint16_t fifoCount;
uint8_t fifoBuffer[64];

Quaternion q;
VectorFloat gravity;
float ypr[3];

volatile bool mpuInterrupt = false;
void dmpDataReady() {
  mpuInterrupt = true;
}

#define INTERRUPT_PIN 2

MPU6050 mpu;

Servo servo_yaw;
Servo servo_pitch;
Servo servo_roll;

void setup() {
  Wire.begin();
  Wire.setClock(400000);

  mpu.initialize();
  pinMode(INTERRUPT_PIN, INPUT);
  devStatus = mpu.dmpInitialize();

  mpu.setXGyroOffset(17);
  mpu.setYGyroOffset(-69);
  mpu.setZGyroOffset(80);
  mpu.setZAccelOffset(1450);

  if (devStatus == 0) {
    Serial.println(F("Enabling DMP..."));
    mpu.setDMPEnabled(true);

    attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), dmpDataReady, RISING);
    mpuIntStatus = mpu.getIntStatus();

    Serial.println(F("DMP ready! Waiting for first interrupt..."));
    dmpReady = true;

    packetSize = mpu.dmpGetFIFOPacketSize();
  }

  servo_yaw.attach(6);
  servo_pitch.attach(3);
  servo_roll.attach(5);
}

void loop() {
  if (!dmpReady) return;
  
  readMPU6050();

  int yawValue = map(ypr[0], -90, 90, 0, 180);
  int pitchValue = map(ypr[1], -90, 90, 180, 0);
  int rollValue = map(ypr[2], -90, 90, 0, 180);

  servo_yaw.write(min(180, max(0, yawValue)));
  servo_pitch.write(min(180, max(0, pitchValue)) );
  servo_roll.write(min(180, max(0, rollValue)));

}

float correct;
int j = 0;

void readMPU6050() {
  while (!mpuInterrupt && fifoCount < packetSize) {
    if (mpuInterrupt && fifoCount < packetSize) {
      fifoCount = mpu.getFIFOCount();
    }
  }

  mpuInterrupt = false;
  mpuIntStatus = mpu.getIntStatus();

  fifoCount = mpu.getFIFOCount();
  if ((mpuIntStatus & _BV(MPU6050_INTERRUPT_FIFO_OFLOW_BIT)) || fifoCount >= 1024) {
    mpu.resetFIFO();
    fifoCount = mpu.getFIFOCount();
    Serial.println(F("FIFO overflow!"));
  } else if (mpuIntStatus & _BV(MPU6050_INTERRUPT_DMP_INT_BIT)) {
    while (fifoCount < packetSize) fifoCount = mpu.getFIFOCount();

    mpu.getFIFOBytes(fifoBuffer, packetSize);
    fifoCount -= packetSize;

    mpu.dmpGetQuaternion(&q, fifoBuffer);
    mpu.dmpGetGravity(&gravity, &q);
    mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);

    ypr[0] = ypr[0] * 180 / M_PI;
    ypr[1] = ypr[1] * 180 / M_PI;
    ypr[2] = ypr[2] * 180 / M_PI;

    if (j <= 300) {
      j++;
      correct = ypr[0];
      readMPU6050();
      return;
    }
    ypr[0] = ypr[0] - correct;
  }
}

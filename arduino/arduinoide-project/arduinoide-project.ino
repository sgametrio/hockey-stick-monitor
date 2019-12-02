
// =================
// UNCOMMENT THE FOLLOWING LINE TO GET DEBUG OUTPUT ON THE USB SERIAL INTERFACE
#define DEBUG
// =================


#include <ArduinoBLE.h>
#include <Arduino_LSM9DS1.h>
#include <MPU6050.h>

#define BLE_DATA_PACKET_LEN 14
#define MEASURING_INTERVAL 30

rtos::Thread eventQueueThread;
events::EventQueue eventQueue;

// BLE Battery Service
BLEService hockeyService("6a0a5c16-0dcf-11ea-8d71-362b9e155667");

// BLE Battery Level Characteristic
BLEBoolCharacteristic managementCharacteristic("5143d572-0dcf-11ea-8d71-362b9e155667", BLERead | BLEWrite | BLENotify);
BLECharacteristic dataCharacteristic("2541489a-0dd1-11ea-8d71-362b9e155667", BLERead | BLENotify, BLE_DATA_PACKET_LEN, true);

MPU6050 mpu6050_low, mpu6050_high(MPU6050_ADDRESS_AD0_HIGH);

int packet_counter;
int periodic_read_sensors_id;

void initSerial() {
  Serial.begin(9600);    // initialize serial communication
  while (!Serial);
}

void initBLE() {
  int result = BLE.begin();
  #ifdef DEBUG 
    if (!result)
      Serial.println("starting BLE failed!");
  #endif
  BLE.setLocalName("HockeyBLEServer");
  BLE.setDeviceName("HockeyBLEServer");
  BLE.setAdvertisedService(hockeyService);
  hockeyService.addCharacteristic(managementCharacteristic);
  hockeyService.addCharacteristic(dataCharacteristic);
  BLE.addService(hockeyService);

  BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
  BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);

  managementCharacteristic.setEventHandler(BLEWritten, managementCharacteristicWritten);

  BLE.advertise();
  #ifdef DEBUG 
    Serial.println("BLE advertising started, waiting for connections...");
  #endif
}

void initInternalIMU() {
  int result = IMU.begin();
  #ifdef DEBUG 
    if (!result)
      Serial.println("starting internal IMU failed!");
  #endif
}

void initExternalIMUs() {
  Wire.begin();
  mpu6050_low.initialize();
  mpu6050_high.initialize();
  #ifdef DEBUG
    Serial.println(mpu6050_low.testConnection() ? "MPU6050 low connection successful" : "MPU6050 low connection failed");
    Serial.println(mpu6050_high.testConnection() ? "MPU6050 high connection successful" : "MPU6050 high connection failed");
  #endif
}

void setup() {
  #ifdef DEBUG
    initSerial();
  #endif

  initBLE();

  initInternalIMU();

  initExternalIMUs();

  eventQueueThread.start(mbed::callback(&eventQueue, &events::EventQueue::dispatch_forever));
}

void loop() {
  // Just poll for BLE events
  BLE.poll();
}

void blePeripheralConnectHandler(BLEDevice central) {
  #ifdef DEBUG
    Serial.print("Connected event, central: ");
    Serial.println(central.address());
  #endif
  packet_counter = 0;
  periodic_read_sensors_id = 0;
}

void blePeripheralDisconnectHandler(BLEDevice central) {
  #ifdef DEBUG
    Serial.print("Disconnected event, central: ");
    Serial.println(central.address());
  #endif
  if (periodic_read_sensors_id > 0) {
    eventQueue.cancel(periodic_read_sensors_id);
    periodic_read_sensors_id = 0;
  }
}

void managementCharacteristicWritten(BLEDevice central, BLECharacteristic characteristic) {
  bool value = managementCharacteristic.value();
  #ifdef DEBUG
    Serial.print("New value received on management characteristic: ");
    Serial.println(value);
  #endif
  if (value == 1 && periodic_read_sensors_id == 0) {
    #ifdef DEBUG
      Serial.println("Starting periodic call");
    #endif
    periodic_read_sensors_id = eventQueue.call_every(MEASURING_INTERVAL, &readSensors);
  }
  else if (value == 0 && periodic_read_sensors_id > 0) {
    eventQueue.cancel(periodic_read_sensors_id);
    periodic_read_sensors_id = 0;
  }
}

void mpu6050ConvertData(int16_t x, int16_t y, int16_t z, float &fx, float &fy, float &fz, float fullscale) {
  fx = x * fullscale / 32768.0;
  fy = y * fullscale / 32768.0;
  fz = z * fullscale / 32768.0;
}

void readSensors() {
  float ax, ay, az, gx, gy, gz;
  int16_t ax2, ay2, az2, gx2, gy2, gz2;
  float ax2_float, ay2_float, az2_float, gx2_float, gy2_float, gz2_float;
  int16_t ax3, ay3, az3, gx3, gy3, gz3;
  float ax3_float, ay3_float, az3_float, gx3_float, gy3_float, gz3_float;

  IMU.readAcceleration(ax, ay, az);
  IMU.readGyroscope(gx, gy, gz);
  mpu6050_low.getMotion6( &ax2, &ay2, &az2, &gx2, &gy2, &gz2);
  mpu6050_high.getMotion6(&ax3, &ay3, &az3, &gx3, &gy3, &gz3);

  mpu6050ConvertData(ax2, ay2, az2, ax2_float, ay2_float, az2_float, 2.0);
  mpu6050ConvertData(gx2, gy2, gz2, gx2_float, gy2_float, gz2_float, 250.0);
  mpu6050ConvertData(ax3, ay3, az3, ax3_float, ay3_float, az3_float, 2.0);
  mpu6050ConvertData(gx3, gy3, gz3, gx3_float, gy3_float, gz3_float, 250.0);

  #ifdef DEBUG
    Serial.print("Values from internal accelerometer:");
    Serial.print("ax: ");
    Serial.print(ax);
    Serial.print("ay: ");
    Serial.print(ay);
    Serial.print("az: ");
    Serial.println(az);
    Serial.print("Values from low external accelerometer:");
    Serial.print("ax: ");
    Serial.print(ax2_float);
    Serial.print("ay: ");
    Serial.print(ay2_float);
    Serial.print("az: ");
    Serial.print(az2_float);
    Serial.println();
    Serial.print("Values from high external accelerometer:");
    Serial.print("ax: ");
    Serial.print(ax3_float);
    Serial.print("ay: ");
    Serial.print(ay3_float);
    Serial.print("az: ");
    Serial.print(az3_float);
    Serial.println();

  #endif

  sendDataBLE(0x01, ax, ay, az);
  sendDataBLE(0x02, gx, gy, gz);
  sendDataBLE(0x03, ax2_float, ay2_float, az2_float);
  sendDataBLE(0x04, gx2_float, gy2_float, gz2_float);
  sendDataBLE(0x05, ax3_float, ay3_float, az3_float);
  sendDataBLE(0x06, gx3_float, gy3_float, gz3_float);

}

void sendDataBLE(uint8_t sensor_id, float x, float y, float z) {
  uint8_t buffer[BLE_DATA_PACKET_LEN];
  buffer[0] = sensor_id;
  memcpy(buffer+1,   (uint8_t*)(&x), 4);
  memcpy(buffer+1+4, (uint8_t*)(&y), 4);
  memcpy(buffer+1+8, (uint8_t*)(&z), 4);
  buffer[13] = packet_counter++;
  dataCharacteristic.writeValue(buffer, BLE_DATA_PACKET_LEN);
  #ifdef DEBUG
    Serial.print("Sending packet counter: ");
    Serial.println(packet_counter-1);
  #endif
}

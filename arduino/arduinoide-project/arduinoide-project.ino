
// =================
// UNCOMMENT THE FOLLOWING LINE TO GET DEBUG OUTPUT ON THE USB SERIAL INTERFACE
// #define DEBUG
// =================


#include <ArduinoBLE.h>
#include <Arduino_LSM9DS1.h>
#include <MPU6050.h>
#include <assert.h>

#define MEASURING_INTERVAL 10
#define NUM_MEASURES_IN_PACKET 6
#define SINGLE_XYZ_SIZE 3*2
#define SINGLE_IMU_SIZE 2*SINGLE_XYZ_SIZE
#define SINGLE_MEASURE_SIZE 3*SINGLE_IMU_SIZE
#define BLE_DATA_PACKET_LEN NUM_MEASURES_IN_PACKET*SINGLE_MEASURE_SIZE + 1

struct data_packet_t {
  uint8_t buffer[BLE_DATA_PACKET_LEN];
  int buffer_len;
};

rtos::Thread eventQueueThread, sendingQueueThread(osPriorityBelowNormal7);
events::EventQueue eventQueue, sendingQueue(64 * (4 + sizeof(data_packet_t)));

// BLE Battery Service
BLEService hockeyService("6a0a5c16-0dcf-11ea-8d71-362b9e155667");

// BLE Battery Level Characteristic
BLEBoolCharacteristic managementCharacteristic("5143d572-0dcf-11ea-8d71-362b9e155667", BLERead | BLEWrite | BLENotify);
BLECharacteristic dataCharacteristic("2541489a-0dd1-11ea-8d71-362b9e155667", BLERead | BLENotify, BLE_DATA_PACKET_LEN, true);

MPU6050 mpu6050_low, mpu6050_high(MPU6050_ADDRESS_AD0_HIGH);

int packet_counter;
int periodic_read_sensors_id;
int measurements_in_databuffer;

data_packet_t data_packet;

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
  data_packet.buffer_len = BLE_DATA_PACKET_LEN;

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
  mpu6050_low.setFullScaleAccelRange(MPU6050_ACCEL_FS_4);
  mpu6050_low.setFullScaleGyroRange(MPU6050_GYRO_FS_2000);
  mpu6050_high.setFullScaleAccelRange(MPU6050_ACCEL_FS_4);
  mpu6050_high.setFullScaleGyroRange(MPU6050_GYRO_FS_2000);
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
  sendingQueueThread.start(mbed::callback(&sendingQueue, &events::EventQueue::dispatch_forever));
}

void loop() {
  // Repeatedly poll for BLE events, using a long timeout like 10s (otherwise this loop behaves almost like a busy-wait)
  BLE.poll(10000);
}

void blePeripheralConnectHandler(BLEDevice central) {
  #ifdef DEBUG
    Serial.print("Connected event, central: ");
    Serial.println(central.address());
  #endif
  packet_counter = 0;
  periodic_read_sensors_id = 0;
  measurements_in_databuffer = 0;
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
    // Enqueue a "FIN" packet (just one byte: 0xFF)
    data_packet_t fin_packet;
    fin_packet.buffer[0] = 0xFF;
    fin_packet.buffer_len = 1;
    tryToEnqueueSendingBLEPacket(fin_packet);
  }
}

void readSensors() {
  int16_t ax, ay, az, gx, gy, gz;
  int16_t ax2, ay2, az2, gx2, gy2, gz2;
  int16_t ax3, ay3, az3, gx3, gy3, gz3;

  IMU.readRawAcceleration(ax, ay, az);
  IMU.readRawGyroscope(gx, gy, gz);
  mpu6050_low.getMotion6( &ax2, &ay2, &az2, &gx2, &gy2, &gz2);
  mpu6050_high.getMotion6(&ax3, &ay3, &az3, &gx3, &gy3, &gz3);

  packDataInBuffer(data_packet.buffer, measurements_in_databuffer * SINGLE_MEASURE_SIZE, ax, ay, az);
  packDataInBuffer(data_packet.buffer, measurements_in_databuffer * SINGLE_MEASURE_SIZE + SINGLE_XYZ_SIZE, gx, gy, gz);
  packDataInBuffer(data_packet.buffer, measurements_in_databuffer * SINGLE_MEASURE_SIZE + SINGLE_IMU_SIZE, ax2, ay2, az2);
  packDataInBuffer(data_packet.buffer, measurements_in_databuffer * SINGLE_MEASURE_SIZE + SINGLE_IMU_SIZE + SINGLE_XYZ_SIZE, gx2, gy2, gz2);
  packDataInBuffer(data_packet.buffer, measurements_in_databuffer * SINGLE_MEASURE_SIZE + 2*SINGLE_IMU_SIZE, ax3, ay3, az3);
  packDataInBuffer(data_packet.buffer, measurements_in_databuffer * SINGLE_MEASURE_SIZE + 2*SINGLE_IMU_SIZE + SINGLE_XYZ_SIZE, gx3, gy3, gz3);

  measurements_in_databuffer++;

  if (measurements_in_databuffer == NUM_MEASURES_IN_PACKET) {
    // This assignment makes a "deep copy" of the buffer array within the struct
    data_packet_t final_data_packet = data_packet;
    final_data_packet.buffer[BLE_DATA_PACKET_LEN - 1] = packet_counter;
    packet_counter++;
    measurements_in_databuffer = 0;
    // Attempt to schedule the sending of the packet over BLE
    tryToEnqueueSendingBLEPacket(final_data_packet);
  }

}

void tryToEnqueueSendingBLEPacket(data_packet_t data_packet) {
  int res = sendingQueue.call(sendBufferBLE, data_packet);
  #ifdef DEBUG
    if (res == 0)
      Serial.println("Failed to enqueue a sending packet!");
  #endif
  assert(res != 0);
}

void packDataInBuffer(uint8_t* buffer, int offset, float x, float y, float z) {
  memcpy(buffer+offset,   (uint8_t*)(&x), 4);
  memcpy(buffer+offset+4, (uint8_t*)(&y), 4);
  memcpy(buffer+offset+8, (uint8_t*)(&z), 4);
}

void sendBufferBLE(data_packet_t data_packet) {
  #ifdef DEBUG
    Serial.print("Sending packet counter: ");
    Serial.println(data_packet.buffer[BLE_DATA_PACKET_LEN - 1]);
  #endif
  dataCharacteristic.writeValue(data_packet.buffer, data_packet.buffer_len);
}

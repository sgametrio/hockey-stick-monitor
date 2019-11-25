/* mbed Microcontroller Library
 * Copyright (c) 2017 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


// =================
// UNCOMMENT THE FOLLOWING LINE TO GET DEBUG OUTPUT ON THE USB SERIAL INTERFACE
#define DEBUG
// =================

#include <stdio.h>

#include "platform/Callback.h"
#include "events/EventQueue.h"
#include "platform/NonCopyable.h"

#include "ble/BLE.h"
#include "ble/Gap.h"
#include "ble/GattClient.h"
#include "ble/GapAdvertisingParams.h"
#include "ble/GapAdvertisingData.h"
#include "ble/GattServer.h"
#include "BLEProcess.h"

#include "USBSerial.h"

using mbed::callback;

#ifdef DEBUG
    USBSerial pc;
#endif DEBUG

#include "LSM9DS1.h"
#include "MPU6050.h"


DigitalOut i2c_pullup(P1_0);
DigitalOut i2c_vdd_enable(P0_22);

class HockeyService {
    typedef HockeyService Self;

public:
    HockeyService(LSM9DS1 arduino_imu, MPU6050 external_imu) :
        _management_characterisic("5143d572-0dcf-11ea-8d71-362b9e155667", 0),
        _data_characterisic("2541489a-0dd1-11ea-8d71-362b9e155667"),
        _hockey_service(
            /* uuid */ "6a0a5c16-0dcf-11ea-8d71-362b9e155667",
            /* characteristics */ _characteristics,
            /* numCharacteristics */ sizeof(_characteristics) /
                                     sizeof(_characteristics[0])
        ),
        _server(NULL),
        _event_queue(NULL),
        _arduino_imu(arduino_imu),
        _external_imu(external_imu)
    {
        // update internal pointers (value, descriptors and characteristics array)
        _characteristics[0] = &_management_characterisic;
        _characteristics[1] = &_data_characterisic;

        // setup authorization handlers for write-enable characteristics
        _management_characterisic.setWriteAuthorizationCallback(this, &Self::authorize_client_write);
    }



    void start(BLE &ble_interface, events::EventQueue &event_queue)
    {
         if (_event_queue) {
            return;
        }

        _server = &ble_interface.gattServer();
        _event_queue = &event_queue;

        // register the service
        #ifdef DEBUG
            pc.printf("Adding hockey service\r\n");
        #endif
        ble_error_t err = _server->addService(_hockey_service);

        if (err) {
            #ifdef DEBUG
                pc.printf("Error %u during demo service registration.\r\n", err);
            #endif
            return;
        }

        // read write handler
        _server->onDataSent(as_cb(&Self::when_data_sent));
        _server->onDataWritten(as_cb(&Self::when_data_written));
        _server->onDataRead(as_cb(&Self::when_data_read));

        // updates subscribtion handlers
        _server->onUpdatesEnabled(as_cb(&Self::when_update_enabled));
        _server->onUpdatesDisabled(as_cb(&Self::when_update_disabled));
        _server->onConfirmationReceived(as_cb(&Self::when_confirmation_received));

        #ifdef DEBUG
            // print the handles
            pc.printf("clock service registered\r\n");
            pc.printf("service handle: %u\r\n", _hockey_service.getHandle());
            pc.printf("\tmanagement characteristic value handle %u\r\n", _management_characterisic.getValueHandle());
            pc.printf("\tdata characteristic value handle %u\r\n", _data_characterisic.getValueHandle());
        #endif
    }

private:

    /**
     * Handler called when a notification or an indication has been sent.
     */
    void when_data_sent(unsigned count)
    {
        #ifdef DEBUG
            pc.printf("sent %u updates\r\n", count);
        #endif
    }

    /**
     * Handler called after an attribute has been written.
     */
    void when_data_written(const GattWriteCallbackParams *e)
    {
        #ifdef DEBUG
            pc.printf("data written:\r\n");
            pc.printf("\tconnection handle: %u\r\n", e->connHandle);
            pc.printf("\tattribute handle: %u", e->handle);
        #endif

        if (e->handle == _management_characterisic.getValueHandle()) {
            if (e->data[0] == 1) {
                // Start recording from sensors
                _periodic_read_sensors_id = _event_queue->call_every(500 /* ms */, callback(this, &Self::read_sensors));
            } else if (e->data[0] == 0) {
                // Stop recording from sensors
                _event_queue->cancel(_periodic_read_sensors_id);
            }
        }

        #ifdef DEBUG
            pc.printf("\twrite operation: %u\r\n", e->writeOp);
            pc.printf("\toffset: %u\r\n", e->offset);
            pc.printf("\tlength: %u\r\n", e->len);
            pc.printf("\t data: ");

            for (size_t i = 0; i < e->len; ++i) {
                pc.printf("%02X", e->data[i]);
            }

            pc.printf("\r\n");
        #endif
    }

    /**
     * Handler called after an attribute has been read.
     */
    void when_data_read(const GattReadCallbackParams *e)
    {
        #ifdef DEBUG
            pc.printf("data read:\r\n");
            pc.printf("\tconnection handle: %u\r\n", e->connHandle);
            pc.printf("\tattribute handle: %u", e->handle);
            if (e->handle == _management_characterisic.getValueHandle()) {
                pc.printf(" (management characteristic)\r\n");
            } else if (e->handle == _data_characterisic.getValueHandle()) {
                pc.printf(" (data characteristic)\r\n");
            } else {
                pc.printf("\r\n");
            }
        #endif
    }

    /**
     * Handler called after a client has subscribed to notification or indication.
     *
     * @param handle Handle of the characteristic value affected by the change.
     */
    void when_update_enabled(GattAttribute::Handle_t handle)
    {
        #ifdef DEBUG
            pc.printf("update enabled on handle %d\r\n", handle);
        #endif
    }

    /**
     * Handler called after a client has cancelled his subscription from
     * notification or indication.
     *
     * @param handle Handle of the characteristic value affected by the change.
     */
    void when_update_disabled(GattAttribute::Handle_t handle)
    {
        #ifdef DEBUG
            pc.printf("update disabled on handle %d\r\n", handle);
        #endif
    }

    /**
     * Handler called when an indication confirmation has been received.
     *
     * @param handle Handle of the characteristic value that has emitted the
     * indication.
     */
    void when_confirmation_received(GattAttribute::Handle_t handle)
    {
        #ifdef DEBUG
            pc.printf("confirmation received on handle %d\r\n", handle);
        #endif
    }

    /**
     * Handler called when a write request is received.
     *
     * This handler verify that the value submitted by the client is valid before
     * authorizing the operation.
     */
    void authorize_client_write(GattWriteAuthCallbackParams *e)
    {
        #ifdef DEBUG
            pc.printf("characteristic %u write authorization\r\n", e->handle);
        #endif

        if (e->offset != 0) {      
            #ifdef DEBUG
                pc.printf("Error invalid offset\r\n");
            #endif
            e->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_INVALID_OFFSET;
            return;
        }

        if (e->len != 1) {
            #ifdef DEBUG
                pc.printf("Error invalid len\r\n");
            #endif
            e->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_INVALID_ATT_VAL_LENGTH;
            return;
        }

        e->authorizationReply = AUTH_CALLBACK_REPLY_SUCCESS;
    }

    void read_sensors() {
        uint8_t buffer[2 * 6 * 4];

        // Internal IMU
        _arduino_imu.readAccel();
        _arduino_imu.readGyro();
        memcpy(buffer,    (uint8_t*)(&_arduino_imu.ax), 4); // original datatype is float
        memcpy(buffer+4,  (uint8_t*)(&_arduino_imu.ay), 4);
        memcpy(buffer+8,  (uint8_t*)(&_arduino_imu.az), 4); 
        memcpy(buffer+12, (uint8_t*)(&_arduino_imu.gx), 4);
        memcpy(buffer+16, (uint8_t*)(&_arduino_imu.gy), 4);
        memcpy(buffer+20, (uint8_t*)(&_arduino_imu.gz), 4);

        // External IMU
        _external_imu.readAccelData(accelCount);
        float ax = (float)accelCount[0]*aRes - accelBias[0];  // get actual g value, this depends on scale being set
        float ay = (float)accelCount[1]*aRes - accelBias[1];   
        float az = (float)accelCount[2]*aRes - accelBias[2];
        _external_imu.readGyroData(gyroCount);  // Read the x/y/z adc values
        _external_imu.getGres();
        float gx = (float)gyroCount[0]*gRes; // - gyroBias[0];  // get actual gyro value, this depends on scale being set
        float gy = (float)gyroCount[1]*gRes; // - gyroBias[1];  
        float gz = (float)gyroCount[2]*gRes; // - gyroBias[2];   
        memcpy(buffer+24, (uint8_t*)(&ax), 4); // original datatype is float
        memcpy(buffer+28, (uint8_t*)(&ay), 4);
        memcpy(buffer+32, (uint8_t*)(&az), 4); 
        memcpy(buffer+36, (uint8_t*)(&gx), 4); 
        memcpy(buffer+40, (uint8_t*)(&gy), 4);
        memcpy(buffer+44, (uint8_t*)(&gz), 4);
        
        #ifdef DEBUG
            pc.printf("Acceleration data from internal IMU now being sent: \r\n");
            pc.printf("ax: %f, ay: %f, az: %f\r\n", _arduino_imu.ax, _arduino_imu.ay, _arduino_imu.az);
            for (int i = 0; i < 12; i++)
                pc.printf("%u ", buffer[i]);
            pc.printf("\r\n");
        #endif

        ble_error_t err = _data_characterisic.set_buffer(*_server, buffer, sizeof(buffer));
        #ifdef DEBUG
            if (err) {
                pc.printf("write of values returned error %u\r\n", err);
            }
        #endif
    }


private:
    /**
     * Helper that construct an event handler from a member function of this
     * instance.
     */
    template<typename Arg>
    FunctionPointerWithContext<Arg> as_cb(void (Self::*member)(Arg))
    {
        return makeFunctionPointer(this, member);
    }

    /**
     * Read, Write, Notify, Indicate  Characteristic declaration helper.
     *
     * @tparam T type of data held by the characteristic.
     */
    template<typename T>
    class ReadWriteNotifyIndicateCharacteristic : public GattCharacteristic {
    public:
        /**
         * Construct a characteristic that can be read or written and emit
         * notification or indication.
         *
         * @param[in] uuid The UUID of the characteristic.
         * @param[in] initial_value Initial value contained by the characteristic.
         */
        ReadWriteNotifyIndicateCharacteristic(const UUID & uuid, const T& initial_value) :
            GattCharacteristic(
                /* UUID */ uuid,
                /* Initial value */ &_value,
                /* Value size */ sizeof(_value),
                /* Value capacity */ sizeof(_value),
                /* Properties */ GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ |
                                GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE |
                                GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY |
                                GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_INDICATE,
                /* Descriptors */ NULL,
                /* Num descriptors */ 0,
                /* variable len */ false
            ),
            _value(initial_value) {
        }

        /**
         * Get the value of this characteristic.
         *
         * @param[in] server GattServer instance that contain the characteristic
         * value.
         * @param[in] dst Variable that will receive the characteristic value.
         *
         * @return BLE_ERROR_NONE in case of success or an appropriate error code.
         */
        ble_error_t get(GattServer &server, T& dst) const
        {
            uint16_t value_length = sizeof(dst);
            return server.read(getValueHandle(), &dst, &value_length);
        }

        /**
         * Assign a new value to this characteristic.
         *
         * @param[in] server GattServer instance that will receive the new value.
         * @param[in] value The new value to set.
         * @param[in] local_only Flag that determine if the change should be kept
         * locally or forwarded to subscribed clients.
         */
        ble_error_t set(
            GattServer &server, const uint8_t &value, bool local_only = false
        ) const {
            return server.write(getValueHandle(), &value, sizeof(value), local_only);
        }

    private:
        uint8_t _value;
    };

    /**
     * Read, Notify, Indicate  Characteristic declaration helper.
     *
     */
    class ReadNotifyIndicateCharacteristic : public GattCharacteristic {
    public:
        /**
         * Construct a characteristic that can be read and emit
         * notification or indication.
         *
         * @param[in] uuid The UUID of the characteristic.
         * @param[in] initial_value Initial value contained by the characteristic.
         */
        ReadNotifyIndicateCharacteristic(const UUID & uuid) :
            GattCharacteristic(
                /* UUID */ uuid,
                /* Initial value */ _buffer,
                /* Value size */ sizeof(_buffer),
                /* Value capacity */ sizeof(_buffer),
                /* Properties */ GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ |
                                GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY |
                                GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_INDICATE,
                /* Descriptors */ NULL,
                /* Num descriptors */ 0,
                /* variable len */ false
            ) {}

        ble_error_t set_buffer(
            GattServer &server, uint8_t* buffer, uint16_t length, bool local_only = false
        ) const {
            return server.write(getValueHandle(), buffer, length, local_only);
        }

    private:
        uint8_t _buffer[2 * 6 * 4];
    };


    ReadWriteNotifyIndicateCharacteristic<uint8_t> _management_characterisic;
    ReadNotifyIndicateCharacteristic _data_characterisic;

    // list of the characteristics of the service
    GattCharacteristic* _characteristics[2];

    // service
    GattService _hockey_service;

    // to keep the id of the scheduled periodic read_sensors()
    int _periodic_read_sensors_id;

    GattServer* _server;
    events::EventQueue *_event_queue;

    // Reference to Arduino's internal IMU
    LSM9DS1 _arduino_imu;

    // Reference to external IMU
    MPU6050 _external_imu;
};


void fixNano33BLE() {
    // Needed on Nano 33 BLE, don't ask why
    CoreDebug->DEMCR = 0;
    NRF_CLOCK->TRACECONFIG = 0;
}

void initInternalI2C_Nano33BLE() {
    i2c_pullup = 1;
    i2c_vdd_enable = 1;
}

LSM9DS1 getInternalIMU() {
    LSM9DS1 imu(P0_14, P0_15); // these are internal SDA and SCL pins
    imu.begin();
    bool result = imu.begin(); // call twice, don't ask me why
    #ifdef DEBUG
        pc.printf("Result of imu initialization: %u\r\n", result);
    #endif
    //imu.calibration(); // often hangs
    return imu;
}

int main() {
    fixNano33BLE();
    initInternalI2C_Nano33BLE();

    LSM9DS1 imu = getInternalIMU();
    MPU6050 mpu6050;
    mpu6050.resetMPU6050();
    mpu6050.calibrateMPU6050(gyroBias, accelBias);
    mpu6050.initMPU6050();

    BLE &ble_interface = BLE::Instance();
    events::EventQueue event_queue;
    HockeyService hockey_service(imu, mpu6050);
    BLEProcess ble_process(event_queue, ble_interface);
    ble_process.on_init(callback(&hockey_service, &HockeyService::start));
    // bind the event queue to the ble interface, initialize the interface
    // and start advertising
    ble_process.start();

    // Process the event queue.
    event_queue.dispatch_forever();

    return 0;
}

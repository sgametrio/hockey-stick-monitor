import threading
from datetime import datetime
import random
from bson.json_util import dumps, RELAXED_JSON_OPTIONS
import time
import numpy as np
import pandas as pd
import queue

from flask import Flask, request, Response, render_template

app = Flask(__name__)

from flask_cors import CORS, cross_origin

cors = CORS(app)
app.config['CORS_HEADERS'] = 'Content-Type'
app.config['TEMPLATES_AUTO_RELOAD'] = True

from flask_pymongo import PyMongo

app.config["MONGO_URI"] = "mongodb+srv://ste:embeneciccie@hockeydb-nmtoi.mongodb.net/test?retryWrites=true&w=majority"
mongo = PyMongo(app)

import json
from bson import ObjectId
from bson.json_util import dumps


class JSONEncoder(json.JSONEncoder):
    def default(self, o):
        if isinstance(o, ObjectId):
            return str(o)
        return json.JSONEncoder.default(self, o)


class ComplementaryFilter:
    def __init__(self, dt, gyro_sensitivity=65.536):
        self.gyro_s = gyro_sensitivity
        self.dt = dt
        self.roll, self.pitch, self.yaw = 0., 0., 0.

    def step(self, sample):
        Ax, Ay, Az, mx, my, mz, wx, wy, wz = sample

        A_roll = np.arctan2(Ay, Az)
        A_pitch = np.arctan(-Ax, np.sqrt(Ay ** 2 + Az ** 2))

        Mx = mx * np.cos(A_pitch) + mz * np.sin(A_pitch)
        My = mx * np.sin(A_roll) * np.sin(A_pitch) + \
             my * np.cos(A_roll) - mz * np.sin(A_roll) * np.cos(A_pitch)
        M_yaw = np.arctan2(My, Mx)

        # roll = 0.98 * (CF_angle + gyro_data * t) + 0.02 * acc_data

        return self.roll, self.pitch, self.yaw


def h5store(filename, df, **kwargs):
    store = pd.HDFStore(filename)
    store.put('mydata', df)
    store.get_storer('mydata').attrs.metadata = kwargs
    store.close()


def h5load(store):
    data = store['mydata']
    metadata = store.get_storer('mydata').attrs.metadata
    return data, metadata


def assert_to_return(f):
    def _f(*args, **kwargs):
        try:
            return f(*args, **kwargs)
        except AssertionError as e:
            return e.args[0] if e.args else None

    return _f


positions = ['top', 'mid', 'bot']
sensors = ['acc', 'mag', 'gyr']
columns = [f"{s}_{pos}" for s in sensors for pos in positions]
extended_columns = [f"{IMU}_{axis}" for IMU in columns for axis in "xyz"]
print(extended_columns)


class Collector:
    def __init__(self):
        self.connected_devices = {}
        self.history = []

    @assert_to_return
    def register(self, uuid, content):
        assert type(content) == dict, f'Data received cannot be parsed to dictionary ({type(content)})'
        assert 'mgap' in content, 'Please specify the milliseconds gap parameter'
        assert 'time' in content, 'Please specify the start time'
        assert type(content['mgap']) == int, 'The specified milliseconds gap parameter is not an integer number'

        mgap = content['mgap']
        start_time = content['time']

        if uuid not in self.connected_devices:
            self.connected_devices[uuid] = Device(uuid, start_time, mgap)
        elif not self.connected_devices[uuid].active:
            self.connected_devices[uuid].enable(start_time, mgap)
        else:
            print(f"Device <{uuid}> is already enabled, check your code")

        return 'Registration started'

    # @assert_to_return
    def upload(self, uuid, content):
        connection_error = 'Please, enable the device with /start before sending data'
        assert uuid in self.connected_devices, connection_error
        assert self.connected_devices[uuid].active, connection_error

        assert type(content) == dict, f'Data received cannot be parsed to dictionary ({type(content)})'
        assert content.keys() == set(positions), \
            "Malformed dictionary, check keys"
        assert all([type(content[pos]) is dict for pos in positions]), \
            "Sensor data are not in dict format"

        assert all([type(content[pos][sens]) is list for sens in sensors for pos in positions]), \
            "Sensor data are not in list format"

        data = np.array([[content[pos][sens] for sens in sensors] for pos in positions])

        assert all([sensor_data.shape[-1] == 3 for sensor_data in data]), 'Each sensor should have 3 axis on the ' \
                                                                          'second dimension '
        assert len(set([sensor_data.shape for sensor_data in data])) == 1, 'Different shape for sensor readings'

        data = np.moveaxis(data, 2, 0)

        self.connected_devices[uuid].save(data)

        return 'Data uploaded successfully'

    def unregister(self, uuid, content):
        assert type(content) == dict, f'Data received cannot be parsed to dictionary ({type(content)})'

        if uuid in self.connected_devices:
            if self.connected_devices[uuid].active:
                self.connected_devices[uuid].disable()
            else:
                return f"Device <{uuid}> is already disabled, check your code"

        return "Registration stopped"


class Device:
    def __init__(self, uuid, start_time, mgap):
        self.uuid = uuid
        self.start_time = start_time
        self.active = True
        # self.data_history = queue.Queue()
        self.data_history = []  # block * packets * sensors * timestep * axis
        self.realtime_data = []  # packets * sensors * timestep * axis
        self.mgap = mgap

    def save(self, data):
        self.realtime_data.append(data)

    def enable(self, start_time, mgap):
        self.active = True
        self.start_time = start_time
        self.mgap = mgap

    def disable(self):
        self.active = False

        data = np.vstack(self.realtime_data)
        # timestep * position * sensor * axis
        print(f"Data: {data.shape}")

        assert len(sensors) == 3, 'Change the code, this works only for acc-mag-gyr IMUs'
        acc_mag = []
        # timestep * position * (sensor * axis)
        tuple_view = data.reshape(-1, len(positions), len(sensors) * 3)
        for i in range(len(positions)):
            Ax, Ay, Az, mx, my, mz, wx, wy, wz = np.moveaxis(tuple_view[:, i, :], 0, 1)
            A_roll = np.arctan2(Ay, Az)
            A_pitch = np.arctan(-Ax / np.sqrt(Ay ** 2 + Az ** 2))

            Mx = mx * np.cos(A_pitch) + mz * np.sin(A_pitch)
            My = mx * np.sin(A_roll) * np.sin(A_pitch) + \
                 my * np.cos(A_roll) - mz * np.sin(A_roll) * np.cos(A_pitch)
            M_yaw = np.arctan2(My, Mx)

            result = np.c_[A_roll, A_pitch, M_yaw]
            print(f"{np.moveaxis(tuple_view[:, i, :], 0, 1).shape} -> {result.shape}")
            acc_mag.append(result)
        angles = np.moveaxis(np.array(acc_mag), 1, 0)
        angles = np.moveaxis(angles[..., np.newaxis], -1, 2)
        # timestep * position * 1 * axis
        print(f"Angles: {angles.shape}")

        complete = np.concatenate([data, angles], axis=2)
        print(f"Final (acc-mag): {complete.shape}")

        CF_angles = []
        for pos in range(len(positions)):
            # 0 acc, 1 mag, 2 gyr, 3 compensated acc+mag
            CF_angle = [complete[0, pos, 2] * self.mgap]
            for i in range(1, tuple_view.shape[0]):
                CF_angle.append(0.98 * (CF_angle[-1] + complete[0, pos, 2] * self.mgap) + 0.02 * complete[0, pos, 3])
            CF_angles.append(CF_angle)

        refined_angles = np.moveaxis(np.array(CF_angles), 1, 0)
        refined_angles = np.moveaxis(refined_angles[..., np.newaxis], -1, 2)
        # timestep * position * 1 * axis
        print(f"Refined angles: {refined_angles.shape}")

        complete = np.concatenate([complete, refined_angles], axis=2)
        print(f"Final (acc-mag-gyr): {complete.shape}")

        # This supports different inter-packets size
        timings = [self.start_time]
        for pid, packet in enumerate(self.realtime_data):
            for sample in range(len(packet)):
                timings.append(timings[-1] + self.mgap)

        data = [{**{"time": timings[idr]},
                 **{pos:
                        {sens:
                             {axis: row[idp][ids][iax] for iax, axis in enumerate("xyz")}
                         for ids, sens in enumerate(sensors + ['accmag_data', 'compl_angles'])}
                    for idp, pos in enumerate(positions)}}
                for idr, row in enumerate(complete)]

        mongo.db.sticks.update_one({'uuid': self.uuid}, {
            '$push': {'registrations': {'start_time': self.start_time,
                                        'mgap': self.mgap,
                                        'data': data}
                      }}, upsert=True)

        self.realtime_data = []


collector = Collector()


# @app.before_first_request
# def activate_job():
#     def run_job():
#         while True:
#             print("Run recurring task")
#             time.sleep(3)
#
#     thread = threading.Thread(target=run_job)
#     thread.start()


@app.route('/', methods=['GET', 'POST'])
def index():
    return render_template('index.html')


@app.route('/api/start/<uuid>', methods=['GET', 'POST'])
def start_record(uuid):
    return collector.register(uuid, request.json)


@app.route('/api/add_message/<uuid>', methods=['GET', 'POST'])
def add_message(uuid):
    return collector.upload(uuid, request.json)


@app.route('/api/stop/<uuid>', methods=['GET', 'POST'])
def stop_record(uuid):
    return collector.unregister(uuid, request.json)


@app.route('/api/devices', methods=['GET'])
def get_devices():
    return dumps([x['uuid'] for x in mongo.db.sticks.find()])


@app.route('/api/devices/<uuid>', methods=['GET'])
def get_registrations(uuid):
    data = mongo.db.sticks.find({'uuid': uuid})[0]['registrations']
    return dumps([x['start_time'] for x in data])


@app.route('/api/devices/<uuid>/<registration>', methods=['GET'])
def get_registration(uuid, registration):
    data = [y['registrations'][int(registration)] for y in
            mongo.db.sticks.find({'uuid': uuid})]
    return dumps(data)


@app.route('/chart-data')
def chart_data():
    def generate_random_data():
        first_device = list(collector.connected_devices.values())[0]
        df = first_device.data_history[0].to_numpy()
        print("Restart")
        for data in df:
            json_data = json.dumps({'time': data[0],
                                    'x': data[1],
                                    'y': data[2],
                                    'z': data[3]})
            yield f"data:{json_data}\n\n"
            time.sleep(0.1)

    return Response(generate_random_data(), mimetype='text/event-stream')


if __name__ == '__main__':
    app.run()

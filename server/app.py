import time

import numpy as np
from pyquaternion import Quaternion
from scipy import integrate
import pandas as pd
from flask import Flask, request, Response, render_template

app = Flask(__name__)

from flask_cors import CORS

cors = CORS(app)
app.config['CORS_HEADERS'] = 'Content-Type'
app.config['TEMPLATES_AUTO_RELOAD'] = True

from flask_pymongo import PyMongo

# app.config["MONGO_URI"] = "mongodb+srv://ste:embeneciccie@hockeydb-nmtoi.mongodb.net/test?retryWrites=true&w=majority"
app.config['MONGO_DBNAME'] = 'sticks'  # name of database on mongo
app.config["MONGO_URI"] = "mongodb://127.0.0.1:27017/sticks"
mongo = PyMongo(app)

from threading import Lock

lock = Lock()

import json
from bson.json_util import dumps


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

        with lock:
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

    def calibrate(self, uuid, content):
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

        with lock:
            self.connected_devices[uuid].save_cal(data)

        return 'Data uploaded successfully'


class Device:
    def __init__(self, uuid, start_time, mgap):
        self.uuid = uuid
        self.start_time = start_time * 0.001
        self.active = True
        # self.data_history = queue.Queue()
        self.data_history = []  # block * packets * sensors * timestep * axis
        self.realtime_data = []  # packets * sensors * timestep * axis
        self.calib_data = []  # packets * sensors * timestep * axis

        self.mgap = mgap

    def save(self, data):
        self.realtime_data.append(data)

    def save_cal(self, data):
        self.calib_data.append(data)

    def enable(self, start_time, mgap):
        self.active = True
        self.start_time = start_time
        self.mgap = mgap * 0.001

    def disable(self):
        self.active = False

        data = np.vstack(self.realtime_data)
        if len(self.calib_data) > 0:
            cal_data = np.vstack(self.calib_data)

        # timestep * position * sensor * axis
        print(f"Data: {data.shape}")

        # my_array[:, [0, 1]] = my_array[:, [1, 0]]

        assert len(sensors) == 3, 'Change the code, this works only for acc-mag-gyr IMUs'
        acc_mag_arct = []
        # timestep * position * (sensor * axis)
        for i in range(len(positions)):
            A, M = np.moveaxis(data[:, i, :2, :], 0, 1)

            ax, ay, az = np.moveaxis(A, 0, 1)
            roll = np.arctan2(ay, az) * 180. / np.pi
            pitch = np.arctan2(-ax, np.sqrt(ay ** 2 + az ** 2)) * 180. / np.pi

            norm = np.linalg.norm(A, axis=1)[:, np.newaxis]
            A_norm = A / norm
            pitchA = -np.arcsin(A_norm[:, 0])
            Ay = A[:, 1] / np.cos(pitchA)
            rollA = np.arcsin(Ay / norm[:, 0])

            M = M / np.linalg.norm(M, axis=1)[:, np.newaxis]
            mx, my, mz = np.moveaxis(M, 0, 1)
            my = -my

            Mx = mx * np.cos(pitchA) + mz * np.sin(pitchA)
            My = mx * np.sin(rollA) * np.sin(pitchA) + \
                 my * np.cos(rollA) - mz * np.sin(rollA) * np.cos(pitchA)
            M_yaw = np.arctan2(-My, Mx)

            # M_yaw[M_yaw > 180] -= 360
            # M_yaw[M_yaw < -180] += 360

            result_arct = np.c_[roll, pitch, M_yaw]

            print(f"{data[:, i, :2, :].shape} -> {result_arct.shape}")
            acc_mag_arct.append(result_arct)

        angles_arct = np.moveaxis(np.array(acc_mag_arct), 1, 0)
        angles_arct = np.moveaxis(angles_arct[..., np.newaxis], -1, 2)
        angles_arct[np.isnan(angles_arct)] = 0.

        # timestep * position * 1 * axis
        print(f"Angles: {angles_arct.shape}")

        complete = np.concatenate([data, angles_arct], axis=2)
        print(f"Final (acc-mag): {complete.shape}")

        CF_angles = []
        for pos in range(len(positions)):
            # 0 acc, 1 mag, 2 gyr, 3 compensated acc+mag
            CF_angle = [[0, 0, 0]]
            for i in range(data.shape[0]):
                roll, pitch = CF_angle[-1][:2] + complete[i, pos, 2, :2] * self.mgap
                yaw = 0.98 * (CF_angle[-1][2] + complete[i, pos, 2, 2] * self.mgap) + 0.02 * complete[i, pos, 3, 2]
                CF_angle.append([roll, pitch, yaw])
            CF_angles.append(CF_angle[1:])

        refined_angles = np.moveaxis(np.array(CF_angles), 1, 0)
        refined_angles = np.moveaxis(refined_angles[..., np.newaxis], -1, 2)
        # refined_angles[np.isnan(refined_angles)] = 0
        # timestep * position * 1 * axis
        print(f"Refined angles: {refined_angles.shape}")

        complete = np.concatenate([complete, refined_angles], axis=2)
        print(f"Final (acc-mag-gyr): {complete.shape}")

        # This supports different inter-packets size
        timings = [self.start_time]
        for pid, packet in enumerate(self.realtime_data):
            for sample in range(len(packet)):
                timings.append(timings[-1] + self.mgap)

        a_tildes = []
        q_omegas = []
        q_cs = []
        dirty = []
        lib = []
        for pos in range(len(positions)):
            d = data[:, pos, :, :]  # timestep * sensor *  axis
            A, M, W = np.moveaxis(d, 0, 1)

            from fusion import Fusion
            f = Fusion(lambda start, end: self.mgap)

            if len(self.calib_data) > 0:
                M_cal = np.moveaxis(cal_data[:, pos, 1, :], 0, 1)
                f.calibrate(M_cal)
            fus = []
            for i, (accel, gyro, mag) in enumerate(zip(A, W, M)):
                f.update(accel, gyro, mag, ts=timings[i])
                fus.append([f.pitch, f.heading, f.roll])
            print(f.beta)
            lib.append(fus)

            dirty.append(integrate.cumtrapz(W, dx=self.mgap, axis=0, initial=0))

            acc_norm = np.linalg.norm(A, axis=1)[:, np.newaxis]
            An = A / acc_norm

            # Pitch and roll are obtainable with acceleration
            ax, ay, az = np.moveaxis(An, 0, 1)
            theta_x = -np.arctan2(az, np.sign(ay) * np.sqrt(ax ** 2 + ay ** 2))
            theta_z = -np.arctan2(-ax, ay)

            # Let's try to compensate with magnetometer
            # x is z, y is x, z is y
            pitch = -np.arcsin(An[:, 2])
            roll = np.arcsin(A[:, 0] / np.cos(pitch) / acc_norm[:, 0])
            M = M / np.linalg.norm(M, axis=1)[:, np.newaxis]
            mx, my, mz = np.moveaxis(M, 0, 1)
            mx = -mx
            Mz = mz * np.cos(pitch) + my * np.sin(pitch)
            Mx = mz * np.sin(roll) * np.sin(pitch) + \
                 mx * np.cos(roll) - my * np.sin(roll) * np.cos(pitch)
            M_yaw = np.arctan2(-Mx, Mz)
            a_tilde = np.c_[theta_x, M_yaw, theta_z]
            a_tildes.append(a_tilde)

            # Let's start to integrate with quats
            W_norm = np.linalg.norm(W, axis=1)[:, np.newaxis]
            W = W / W_norm

            # 3 axis gyro integration
            q_omega = [Quaternion(angle=0, axis=[1, 0, 0])]  # Rotation from body to world frame
            for i in range(len(W)):
                q_omega.append(q_omega[-1] * Quaternion(angle=self.mgap * W_norm[i], axis=W[i]))

            q_omegas.append([q.yaw_pitch_roll for q in q_omega[1:]])

            # Computing the tilt correction quaternion
            q_world = []
            for i in range(len(q_omega[:-1])):
                # TODO: a tilde o acc. normale?
                q = q_omega[i] * Quaternion((0, *a_tilde[i])) * q_omega[i].conjugate
                q_world.append(q.normalised.imaginary)
            q_world = np.array(q_world)

            vx, vy, vz = np.moveaxis(q_world, 0, 1)

            n = np.c_[-vz, [0] * len(vx), vx]
            n = n / np.linalg.norm(n, axis=1)[:, np.newaxis]

            # Applying a complementary filter
            q_c = []
            for i in range(len(q_omega[1:])):
                q = Quaternion(angle=0.02 * np.arccos(vy[i]), axis=n[i]) * q_omega[i]
                q_c.append(q.yaw_pitch_roll)

            print(f"{data[:, pos, :, :].shape} -> {np.array(q_c).shape}")

            q_cs.append(q_c)

        lib = np.moveaxis(np.array(lib), 1, 0)
        lib = np.moveaxis(lib[..., np.newaxis], -1, 2)
        complete = np.concatenate([complete, lib], axis=2)

        dirty = np.moveaxis(np.array(dirty), 1, 0)
        dirty = np.moveaxis(dirty[..., np.newaxis], -1, 2)
        complete = np.concatenate([complete, dirty], axis=2)

        a_tildes = np.moveaxis(np.array(a_tildes), 1, 0)
        a_tildes = np.moveaxis(a_tildes[..., np.newaxis], -1, 2)
        complete = np.concatenate([complete, a_tildes], axis=2)

        q_omegas = np.moveaxis(np.array(q_omegas), 1, 0)
        q_omegas = np.moveaxis(q_omegas[..., np.newaxis], -1, 2)
        q_omegas[np.isnan(q_omegas)] = 0
        complete = np.concatenate([complete, q_omegas], axis=2)

        q_cs = np.moveaxis(np.array(q_cs), 1, 0)
        q_cs = np.moveaxis(q_cs[..., np.newaxis], -1, 2)
        complete = np.concatenate([complete, q_cs], axis=2)

        new_insights = ['accmag_data_tan', 'compl_angles', 'lib', 'dirty', 'a_tildes', 'q_omegas', 'q_cs']
        data = [{**{"time": timings[idr]},
                 **{pos:
                        {sens:
                             {axis: row[idp][ids][iax] for iax, axis in enumerate("xyz")}
                         for ids, sens in enumerate(sensors + new_insights)}
                    for idp, pos in enumerate(positions)}}
                for idr, row in enumerate(complete)]

        mongo.db.sticks.update_one({'uuid': self.uuid}, {
            '$push': {'registrations': {'start_time': self.start_time,
                                        'mgap': self.mgap,
                                        'data': data}
                      }}, upsert=True)

        self.realtime_data = []
        self.calib_data = []


collector = Collector()


# @app.before_first_request
# def activate_job():
#    print(mongo)
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


@app.route('/api/calibrate/<uuid>', methods=['GET', 'POST'])
def calibrate(uuid):
    return collector.calibrate(uuid, request.json)

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
    app.run(threaded=False)

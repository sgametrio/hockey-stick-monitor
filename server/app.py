import datetime
import json
import random
import time
import numpy as np

from flask import Flask, request, Response, render_template

app = Flask(__name__)


class Collector:
    def __init__(self):
        self.connected_devices = {}

    def register(self, uuid):
        if uuid not in self.connected_devices:
            self.connected_devices[uuid] = Device(uuid)
        elif not self.connected_devices[uuid].active:
            self.connected_devices[uuid].active(True)
        else:
            print(f"Device <{uuid}> is already enabled, check your code")

    def upload(self, uuid, content):
        if uuid not in self.connected_devices or (not self.connected_devices[uuid].active):
            error = 'Please, enable the device with /start before sending data'
            print(error)
            return error

        assert type(content) == dict, f'Data received cannot be parsed to dictionary ({type(content)})'
        assert content.keys() == {'acc_bot', 'acc_top', 'gyr_bot', 'gyr_top', 'mgap'},"Malformed dictionary, check keys"
        assert type(content['mgap']) == int, 'The specified milliseconds gap parameter is not an integer number'
        assert all([type(content[sensor]) is list for sensor in ['acc_bot', 'acc_top', 'gyr_bot', 'gyr_top']]),\
            "Sensor data are not in list format"

        data = [np.array(content[sensor]) for sensor in ['acc_bot', 'acc_top', 'gyr_bot', 'gyr_top']]
        assert all([sensor_data.shape[1] == 3 for sensor_data in data]), 'Each sensor should have 3 axis on the ' \
                                                                         'second dimension '
        assert len(set([sensor_data.shape for sensor_data in data])) == 1, 'Different shape for sensor readings'

        # assertions on max min here..
        self.connected_devices[uuid].save(data, content['mgap'])

        return 'OK'

    def unregister(self, uuid):
        if uuid in self.connected_devices:
            if self.connected_devices[uuid].active:
                self.connected_devices[uuid].active(True)
            else:
                print(f"Device <{uuid}> is already disabled, check your code")



class Device:
    def __init__(self, uuid):
        self.uuid = uuid
        self.last_conn = time.time()
        self.active = True
        self._data = []

    def save(self, content):
        self.last_conn = time.time()
        self._data.append(content)

    def active(self, state):
        self.last_conn = time.time()
        self.active = state


collector = Collector()

@app.route('/')
def index():
    return render_template('index.html')


@app.route('/api/start/<uuid>', methods=['GET', 'POST'])
def start_record(uuid):
    collector.register(uuid)
    return 'OK'


@app.route('/api/add_message/<uuid>', methods=['GET', 'POST'])
def add_message(uuid):
    content = request.json
    return collector.upload(uuid, content)


@app.route('/api/stop/<uuid>', methods=['GET', 'POST'])
def stop_record(uuid):
    collector.unregister(uuid)
    return 'OK'


@app.route('/chart-data')
def chart_data():
    def generate_random_data():
        while True:
            json_data = json.dumps({'time': datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
                                    'value': random.random() * 100})
            yield f"data:{json_data}\n\n"
            time.sleep(1)

    return Response(generate_random_data(), mimetype='text/event-stream')


if __name__ == '__main__':
    app.run(debug=True, threaded=True)

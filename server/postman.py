import time

import requests as req

import numpy as np

uuid = "testyo"
host = "http://localhost:5000"
resp = req.post(host + "/api/start/" + uuid, json={"mgap": 15, 'time': time.time()})
print(resp.text)
for i in range(3):
    data = {"top": {"acc": np.full((10, 3), 0).tolist(),
                    "mag": np.full((10, 3), 1).tolist(),
                    "gyr": np.full((10, 3), 2).tolist()},
            "mid": {"acc": np.full((10, 3), 3).tolist(),
                    "mag": np.full((10, 3), 4).tolist(),
                    "gyr": np.full((10, 3), 5).tolist()},
            "bot": {"acc": np.full((10, 3), 6).tolist(),
                    "mag": np.full((10, 3), 7).tolist(),
                    "gyr": np.full((10, 3), 8).tolist()}}
    try:
        resp = req.post(host+ "/api/add_message/" + uuid, json=data)
        print(resp.text)
        if "/start" in resp.text:
            try:
                req.post(host+ "/api/start/" + uuid, json={"mgap": 15, 'time': time.time()})
            except Exception as e:
                print(e)
    except Exception as e:
        print(e)
    time.sleep(0.2)
resp = req.post(host + "/api/stop/" + uuid, json={"mgap": 15})
print(resp.text)
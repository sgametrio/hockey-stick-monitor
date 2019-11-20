import time

import requests as req

import numpy as np

resp = req.post("http://127.0.0.1:5000/api/start/1234")
print(resp.text)
while True:
    data = {"acc_bot": np.random.rand(10, 3).tolist(),
            "acc_top": np.random.rand(10, 3).tolist(),
            "gyr_bot": (np.random.rand(10, 3) * 2 * np.pi - np.pi).tolist(),
            "gyr_top": (np.random.rand(10, 3) * 2 * np.pi - np.pi).tolist(),
            "mgap": 15}
    try:
        resp = req.post("http://127.0.0.1:5000/api/add_message/1234", json=data)
        print(resp.text)
        if "/start" in resp.text:
            try:
                req.post("http://127.0.0.1:5000/api/start/1234")
            except Exception as e:
                print(e)
    except Exception as e:
        print(e)
    time.sleep(10)



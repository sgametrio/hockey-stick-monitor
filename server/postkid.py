import requests as req
resp = req.post("http://127.0.0.1:5000/")
print(resp.text)

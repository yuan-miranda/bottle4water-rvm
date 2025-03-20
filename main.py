# https://github.com/ultralytics/ultralytics
from ultralytics import YOLO

# // Default IP: http://192.168.4.1/

# // PLDTHOMEFIBRFDza62.4g
# // PLDTWIFIPv54q

import os
import requests
import cv2
import numpy as np

# change the url to the ip address of the esp32 camera
url = "http://192.168.78.218/capture"
response = requests.get(url)

img = np.array(bytearray(response.content), dtype=np.uint8)
img = cv2.imdecode(img, cv2.IMREAD_COLOR)
cv2.imwrite("image.jpg", img)

model = YOLO("yolo11x.pt")
# only bottle be outlined
results = model("image.jpg", save=True)

os.remove("image.jpg")

# show results
for result in results:
    xywh = result.boxes.xywh 
    xywhn = result.boxes.xywhn
    xyxy = result.boxes.xyxy
    xyxyn = result.boxes.xyxyn
    names = [result.names[cls.item()] for cls in result.boxes.cls.int()]
    confs = result.boxes.conf
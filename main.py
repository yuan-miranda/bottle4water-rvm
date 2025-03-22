# https://github.com/ultralytics/ultralytics
from ultralytics import YOLO
import os
import requests
import cv2
import numpy as np

ESP32_BOARD_IP = "192.168.1.13"
ESP32_CAMERA_IP = "192.168.1.13"

BOARD_URL = f"http://{ESP32_BOARD_IP}/opengate"
CAMERA_URL = f"http://{ESP32_CAMERA_IP}/capture"

model = YOLO("yolo11x.pt")

while True:
    try:
        response = requests.get(CAMERA_URL, timeout=5)
        img = np.array(bytearray(response.content), dtype=np.uint8)
        img = cv2.imdecode(img, cv2.IMREAD_COLOR)

        if img is None:
            print("Failed to decode image.")
            continue

        # bottle = 39
        result = model(img, classes=[39])[0]
        if len(result.boxes.xyxy) > 0:
            try:
                requests.post(BOARD_URL, timeout=5)
            except Exception as e:
                print("Error:", e)
                continue

    except Exception as e:
        print("Error:", e)
        continue
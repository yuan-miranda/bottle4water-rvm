# https://github.com/ultralytics/ultralytics
from ultralytics import YOLO
import os
import requests
import cv2
import numpy as np

url = "http://192.168.1.13/capture"

model = YOLO("yolo11x.pt")

while True:
    try:
        response = requests.get(url, timeout=5)
        img = np.array(bytearray(response.content), dtype=np.uint8)
        img = cv2.imdecode(img, cv2.IMREAD_COLOR)

        if img is None:
            print("Failed to decode image.")
            continue

        # bottle = 39
        results = model(img, classes=[39])
        
        # first result only
        if results:
            result = results[0]
            if len(result.boxes.xyxy) > 0:
                box = result.boxes.xyxy[0]
                conf = result.boxes.conf[0]
                cls = result.boxes.cls[0]
                label = f"{result.names[int(cls)]} {conf:.2f}"

                x1, y1, x2, y2 = map(int, box)
                cv2.rectangle(img, (x1, y1), (x2, y2), (0, 255, 0), 2)
                cv2.putText(img, label, (x1, y1 - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 2)
                
        cv2.imshow("ESP32", img)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    except Exception as e:
        print("Error:", e)
        continue

cv2.destroyAllWindows()
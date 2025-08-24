#!/usr/bin/env python3

import cv2
import argparse
import time
from datetime import datetime, timedelta, timezone
from pathlib import Path

def parse_source(dev):
    try:
        return int(dev)
    except Exception:
        return dev

def main():

    p = argparse.ArgumentParser()

    p.add_argument( '--source', '-s',
                   default=0, 
                   help="/dev/video#")
    
    args = p.parse_args()
    dev_cam = parse_source(args.source)

    # open cam
    cap = cv2.VideoCapture(dev_cam)
    if not cap.isOpened():
        print(f"Couldn't open the device : {args.source}")
        return 1
    
    print("Press 'ESC' or 'Q' to exit. Press 'S' to save frame")
    window_name = "Camera Check"
    cv2.namedWindow(window_name, cv2.WINDOW_AUTOSIZE)

    while True:
        # constant frame capture
        ret, frame = cap.read()
        if not ret:
            print("Couldn't receive picture.")
            break

        cv2.imshow(window_name, frame)
        key_pressed = cv2.waitKey(1) & 0xFF

        if 27 == key_pressed or ord('q') == key_pressed:
            break
        if ord('s') == key_pressed:
            output_path = Path("snapshots")
            output_path.mkdir(exist_ok=True)

            filename = output_path/f"snapshot_{datetime.now(timezone(timedelta(hours=3))).strftime("%Y%m%d_%H%M%S")}.jpg"
            cv2.imwrite(str(filename), frame)
            print(f"Snapshot saved to {filename}")

    cap.release()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()
#!/usr/bin/env python3

import cv2
import argparse
import time
import os
import csv

from datetime import datetime, timedelta, timezone
from pathlib import Path

# globals
TIMEZONE = timezone(timedelta(hours=3))

def main():
    parser = argparse.ArgumentParser()

    parser.add_argument('--source', '-s', default=0, help="/dev/video#")
    parser.add_argument('--min-area', type=int, default=1000, help="min motion area (in px)")
    parser.add_argument('--cooldown', '-cd', type=float, default=3.0, help="detection cooldown")
    
    args = parser.parse_args()
    
    try:
        source = int(args.source)
    except:
        source = args.source
    
    min_area = args.min_area
    cooldown = args.cooldown

    # open cam
    cam = cv2.VideoCapture(source)
    if not cam.isOpened():
        print(f"Couldn't open the camera device: {args.source}")
        return
    
    print("ESC / Q -> EXIT, S -> SAVE SNAPSHOT")
    
    # display only available on desktop
    has_display = os.environ.get("DISPLAY") is not None
    if has_display:
        cv2.namedWindow("Camera", cv2.WINDOW_AUTOSIZE)

    # motion detection
    background = None
    last_motion = 0
    
    # save detections
    os.makedirs("snapshots", exist_ok=True)
    csv_file = "detections.csv"
    
    # initial creation of CSV file
    if not os.path.exists(csv_file):
        with open(csv_file, "w") as f:
            f.write("timestamp,label,conf,x1,y1,x2,y2,snapshot\n")

    while cam.isOpened():
        ret, frame = cam.read()
        if not ret:
            print("Couldn't receive frame from camera")
            break

        # classical gray to get better detections
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        # blur to get rid of distortion
        gray = cv2.GaussianBlur(gray, (5, 5), 0)

        motion_box = None
        
        if background is None:
            background = gray
            continue
            
        # diff == motion occured
        diff = cv2.absdiff(background, gray)
        _, thresh = cv2.threshold(diff, 25, 255, cv2.THRESH_BINARY)
        
        # make rectangle wider
        kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (5, 5))
        thresh = cv2.dilate(thresh, kernel, iterations=2)

        # find boundaries
        contours, _ = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        
        biggest_area = 0
        for contour in contours:
            area = cv2.contourArea(contour)
            if area < min_area:
                continue
                
            x, y, w, h = cv2.boundingRect(contour)
            if area > biggest_area:
                biggest_area = area
                motion_box = (x, y, w, h)

        # update with all gray
        background = gray

        # draw rectangle if there is a motion (only desktop)
        display_frame = frame.copy()
        if motion_box:
            x, y, w, h = motion_box
            cv2.rectangle(display_frame, (x, y), (x+w, y+h), (0, 255, 0), 2)
            cv2.putText(display_frame, f"Motion: {w}x{h}", (x, y-10), 
                       cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)

        # show frame
        if has_display:
            cv2.imshow("Camera", display_frame)

        # save if cd elapsed
        now = time.time()
        if motion_box and (now - last_motion) >= cooldown:
            last_motion = now
            x, y, w, h = motion_box
            
            # check boundaries
            x = max(0, x)
            y = max(0, y)
            w = min(w, frame.shape[1] - x)
            h = min(h, frame.shape[0] - y)
            
            # save snapshot
            timestamp = datetime.now(TIMEZONE)
            filename = f"snapshots/motion_{timestamp.strftime('%Y%m%d_%H%M%S')}.jpg"
            cv2.imwrite(filename, frame)
            
            # save CSV
            with open(csv_file, "a", newline="") as f:
                writer = csv.writer(f)
                ts_str = timestamp.isoformat()
                writer.writerow([ts_str, "motion", "", x, y, x+w, y+h, filename])
            
            print(f"[{timestamp.strftime('%H:%M:%S')}] Motion captured!: {filename}")

        # key handling on desktop
        if has_display:
            key = cv2.waitKey(1) & 0xFF
            if key == 27 or key == 113:
                break
            elif key == 115:
                snap_name = f"snapshots/manual_{datetime.now(TIMEZONE).strftime('%Y%m%d_%H%M%S')}.jpg"
                cv2.imwrite(snap_name, frame)
                print(f"Snapshot saved: {snap_name}")
        else:
            time.sleep(0.01)

    cam.release()
    if has_display:
        cv2.destroyAllWindows()

if __name__ == "__main__":
    main()
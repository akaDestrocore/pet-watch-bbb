#!/usr/bin/env python3

import cv2 as cv
import serial
import time
import struct
import os
import numpy as np

CAM_ID = 0
TTY_DEV = "/dev/ttyS1"
BAUDRATE = 460800

def uart_config():
    try:
        os.system("config-pin P9_24 uart")
        os.system("config-pin P9_26 uart")
        print("UART pins configured successfully.")
    except:
        print("Could not configure UART pins")

def calc_crc16(input_data):
    crc = 0xFFFF
    for b in input_data:
        crc ^= b << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc <<= 1
            crc &= 0xFFFF
    return crc


def send_snapshot(tty_dev, snapshot_data):
    size = len(snapshot_data)

    # Create header for frame protocol: magic + size + CRC
    header_data = struct.pack('<BBBBBB', 0xAA, 0x55,
                              size & 0xFF, 
                              (size >> 8) & 0xFF,
                              (size >> 16) & 0xFF, 
                              (size >> 24) & 0xFF)
    
    crc = calc_crc16(header_data)
    header = header_data + struct.pack('<H', crc)

    # Send all parts to forwarder
    tty_dev.write(header)
    tty_dev.flush()
    tty_dev.write(snapshot_data)
    tty_dev.flush()
    crc = calc_crc16(snapshot_data)
    tty_dev.write(struct.pack('<H', crc))
    tty_dev.flush()
    print(f"{size} bytes sent.")


def main():
    uart_config()

    cap = cv.VideoCapture(CAM_ID, cv.CAP_V4L2)
    if not cap.isOpened():
        print("Cannot open camera")
        exit()
    
    # Setup cam parameters
    cap.set(cv.CAP_PROP_FOURCC, cv.VideoWriter_fourcc('M', 'J', 'P', 'G'))
    cap.set(cv.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv.CAP_PROP_FRAME_HEIGHT, 480)
    cap.set(cv.CAP_PROP_BUFFERSIZE, 1)
    cap.set(cv.CAP_PROP_FPS, 15) 

    # Check what resolution we actually got
    actual_width = int(cap.get(cv.CAP_PROP_FRAME_WIDTH))
    actual_height = int(cap.get(cv.CAP_PROP_FRAME_HEIGHT))
    print(f"Camera resolution: {actual_width}x{actual_height}")
    
    # Fall back to 640x480
    if actual_width < 1280:
        cap.set(cv.CAP_PROP_FRAME_WIDTH, 640)
        cap.set(cv.CAP_PROP_FRAME_HEIGHT, 480)
        cap.set(cv.CAP_PROP_BUFFERSIZE, 1)
        cap.set(cv.CAP_PROP_FPS, 10)
        print("Falling back to 640x480")
    
    cap.set(cv.CAP_PROP_AUTO_EXPOSURE, 1)
    cap.set(cv.CAP_PROP_AUTOFOCUS, 1)

    # try open serial
    try:
        pico_tty = serial.Serial(TTY_DEV, BAUDRATE)
    except:
        print(f"Could not open {TTY_DEV}")
        return
    
    bg = cv.createBackgroundSubtractorMOG2(history=300,
                                        varThreshold=20,
                                        detectShadows=False)

    kernel = cv.getStructuringElement(cv.MORPH_ELLIPSE, (3, 3))
    motion_history = []
    motion_history_size = 3

    last_snapshot_time = 0
    snapshot_count = 0
    frame_count = 0

    print(f"Starting PetWatch on camera device {CAM_ID}")
    print("Press Ctrl+C to stop")

    try:
        consecutive_failures = 0

        while True:
            ret, frame = cap.read()
            if not ret:
                consecutive_failures += 1
                if consecutive_failures > 5:
                    print("Camera timeout, reinitializing...")
                    cap.release()
                    time.sleep(1)
                    cap = cv.VideoCapture(CAM_ID, cv.CAP_V4L2)
                    cap.set(cv.CAP_PROP_FOURCC, cv.VideoWriter_fourcc('M', 'J', 'P', 'G'))
                    cap.set(cv.CAP_PROP_FRAME_WIDTH, 640)
                    cap.set(cv.CAP_PROP_FRAME_HEIGHT, 480)
                    cap.set(cv.CAP_PROP_BUFFERSIZE, 1)
                    cap.set(cv.CAP_PROP_FPS, 15)
                    consecutive_failures = 0
                time.sleep(0.05)
                continue
            consecutive_failures = 0
            frame_count += 1
            
            # Motion detection
            foreground_mask = bg.apply(frame)
            foreground_mask = cv.morphologyEx(foreground_mask, cv.MORPH_OPEN, kernel)
            contours, _ = cv.findContours(foreground_mask, cv.RETR_EXTERNAL, cv.CHAIN_APPROX_SIMPLE)

            # Calculate ROI
            motion_area = 0
            for contour in contours:
                area = cv.contourArea(contour)
                if area > 100:
                    motion_area += area

            # Add to history
            motion_history.append(motion_area)
            if len(motion_history) > motion_history_size:
                motion_history.pop(0)
            
            # Use avg motion for smoothing
            avg_motion = np.mean(motion_history) if motion_history else 0
            
            # Dynamic threshold
            current_time = time.time()
            time_since_last = current_time - last_snapshot_time
            motion_threshold = 300 if time_since_last > 10 else 500
            
            # Check for intense motion
            if avg_motion > motion_threshold and time_since_last > 1.5:
                jpeg_quality = 70 if avg_motion > 5000 else 85
                
                ret, jpeg_data = cv.imencode('.jpg', frame, 
                                            [cv.IMWRITE_JPEG_QUALITY, jpeg_quality])
                if ret:
                    send_snapshot(pico_tty, jpeg_data.tobytes())
                    snapshot_count += 1
                    last_snapshot_time = current_time
                    print(f"Motion detected: {int(avg_motion)} pixels")
            
    except KeyboardInterrupt:
        print(f"\nStopping PetWatch. Total snapshots sent: {snapshot_count}")

    finally:
        cap.release()
        cv.destroyAllWindows()
        pico_tty.close()

if __name__ == "__main__":
    main()
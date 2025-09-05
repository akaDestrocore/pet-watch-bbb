#!/usr/bin/env python3

import cv2 as cv
import serial
import time
import struct

DEV_ID = 0
TTY_DEV = "dev/ttyS1"
BAUDRATE = 115200

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
    header_data = struct.pack('<BBBBBB>', 0xAA, 0x55,
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
    data_crc = calc_crc16(snapshot_data)
    tty_dev.write(struct.pack('<H', data_crc))
    tty_dev.flush()
    print(f"{size} bytes sent.")


def main():
    cap = cv.VideoCapture(DEV_ID)
    if not cap.isOpened():
        print("Cannot open camera")
        exit()
    
    # Setup cam parameters
    cap.set(cv.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv.CAP_PROP_FRAME_HEIGHT, 480)

    pico_tty = serial.Serial(TTY_DEV, BAUDRATE)

    bg = cv.createBackgroundSubtractorMOG2()

    last_snapshot_time = 0
    snapshot_count = 0

    print(f"Starting PetWatch on camera device {DEV_ID}")
    print("Press Ctrl+C to stop")

    try:
        while True:
            ret, frame = cap.read()
            if not ret:
                continue
            
            # Motion detection
            foreground_mask = bg.apply(frame)
            contours, _ = cv.findContours(foreground_mask, 
                                          cv.RETR_EXTERNAL, 
                                          cv.CHAIN_APPROX_SIMPLE)

            # Calculate ROI
            motion_area = 0
            for contour in contours:
                area = cv.contourArea(contour)
                if area > 20:
                    motion_area += area

            cur_time = time.time()
            if (motion_area > 200 and cur_time - last_snapshot_time > 2):
                _, snapshot_data = cv.imencode('.jpg', frame, [cv.IMWRITE_JPEG_QUALITY, 85])
                snapshot_bytes = snapshot_data.tobytes()

                send_snapshot(pico_tty, snapshot_bytes)
                snapshot_count += 1
                last_snapshot_time = cur_time
                print("Motion detected in area {motion_area}")
            
    except KeyboardInterrupt:
        print("\nStopping PetWatch. Total snapshots sent: {snapshot_count}")

    finally:
        cap.release()
        cv.destroyAllWindows()

if __name__ == "__main__":
    main()
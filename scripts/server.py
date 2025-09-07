#!/usr/bin/env python3

from http.server import HTTPServer, BaseHTTPRequestHandler
import io
import torch
from PIL import Image
import smtplib
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
import datetime
import time

class SnapshotHandler(BaseHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        self.notification_emails = ["some@gmail.com"]
        self.target_classes = [15, 16, 17, 21]
        self.confidence_threshold = 0.25
        self.iou_threshold = 0.45
        self.smtp_server = "smtp.gmail.com"
        self.smtp_port = 587
        self.email_user = "another@gmail.com"
        self.email_password = "application_key"
        
        super().__init__(*args, **kwargs)

    def send_alarm_notification(self):
        try:
            msg = MIMEMultipart()
            msg['From'] = self.email_user
            msg['To'] = ", ".join(self.notification_emails)
            msg['Subject'] = "ALERT: Pet Detected in Monitoring Area"

            timestamp = datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')
            body = f"""
⚠️ PET DETECTED IN THE REGION OF INTEREST ⚠️

Timestamp: {timestamp}

A pet (cat, dog, sheep, or bear) has been detected in the monitoring area.
Please check the surveillance system immediately.

This is an automated alert from your pet detection system.
"""
            
            msg.attach(MIMEText(body, 'plain'))

            # Send email
            server = smtplib.SMTP(self.smtp_server, self.smtp_port)
            server.starttls()
            server.login(self.email_user, self.email_password)
            text = msg.as_string()
            server.sendmail(self.email_user, self.notification_emails, text)
            server.quit()
            
            print(f"Alarm notification sent to {', '.join(self.notification_emails)}")
        except Exception as e:
            print(f"Error sending alarm notification: {e}")

    def do_POST(self):
        if self.path == '/image':
            content_len = int(self.headers.get('Content-Length', 0))
            snapshot_data = self.rfile.read(content_len)
            
            # Analyze with YOLOv5
            image = Image.open(io.BytesIO(snapshot_data))
            results = SnapshotHandler.model(image)
            
            # Check for cats (class 15) or dogs (class 16), sheeps(17) and bears (21)
            detections = results.pandas().xyxy[0]
            pet_found = any(detection['class'] in self.target_classes for _, detection in detections.iterrows())
            
            if pet_found:
                response_body = "ALARM"
                self.send_response(200, 'ALARM')
                self.send_header('Content-Type', 'text/plain')
                self.send_header('Content-Length', str(len(response_body)))
                self.send_header('Connection', 'keep-alive')
                self.end_headers()
                self.wfile.write(response_body.encode('utf-8'))
                self.wfile.flush()
                print("PET DETECTED - ALARM SENT")
                time.sleep(0.5)
                try:
                    self.send_alarm_notification()
                except Exception as e:
                    print(f"Email notification failed, but alarm was sent: {e}")
            else:
                # Send NO_PETS response
                response_body = "NO_PETS"
                self.send_response(200)
                self.send_header('Content-Type', 'text/plain') 
                self.send_header('Content-Length', str(len(response_body)))
                self.send_header('Connection', 'keep-alive')
                self.end_headers()
                self.wfile.write(response_body.encode('utf-8'))
                self.wfile.flush()
                print("No pets detected - sending NO_PETS response")
                time.sleep(0.5)
    
    def log_message(self, format, *args):
        # mute spam
        pass

if __name__ == "__main__":
    print("Loading YOLOv5...")
    model = torch.hub.load('ultralytics/yolov5', 'yolov5l', pretrained=True)
    model.conf = 0.25
    model.iou = 0.45
    
    SnapshotHandler.model = model
    
    print("Starting optimized server on port 7702...")
    server = HTTPServer(('0.0.0.0', 7702), SnapshotHandler)
    server.serve_forever()
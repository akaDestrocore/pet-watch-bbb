#!/usr/bin/env python3

from http.server import HTTPServer, BaseHTTPRequestHandler
import io
import torch
from PIL import Image, ImageDraw, ImageFont
import smtplib
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
from email.mime.image import MIMEImage
import datetime

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

    def draw_detection_boxes(self, image, detections):
        draw = ImageDraw.Draw(image)
        try:
            font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 20)
        except (OSError, IOError):
            try:
                font = ImageFont.truetype("/System/Library/Fonts/Arial.ttf", 20)
            except (OSError, IOError):
                font = ImageFont.truetype("arial.ttf", 20)

        class_names = {15: 'Cat', 16: 'Dog', 17: 'Sheep', 21: 'Bear'}
        colors = {'Cat': 'green', 'Dog': 'blue', 'Sheep': 'red', 'Bear': 'orange'}

        for _, detection in detections.iterrows():
            if detection['class'] in self.target_classes:
                class_name = class_names.get(detection['class'], 'Pet')
                color = colors.get(class_name, 'green')
                # bounding box
                x1, y1, x2, y2 = int(detection['xmin']), int(detection['ymin']), int(detection['xmax']), int(detection['ymax'])
                draw.rectangle([x1, y1, x2, y2], outline=color, width=3)
                # label
                confidence = detection['confidence']
                label = f"{class_name} {confidence:.2f}"
                try:
                    bbox = draw.textbbox((0, 0), label, font=font)
                    text_width = bbox[2] - bbox[0]
                    text_height = bbox[3] - bbox[1]
                except AttributeError:
                    text_width, text_height = draw.textsize(label, font=font)
                
                # inside the box
                text_x = x1 + 5
                text_y = y1 + 5
                bg_x1, bg_y1 = text_x - 2, text_y - 2
                bg_x2, bg_y2 = text_x + text_width + 2, text_y + text_height + 2
                bg_x2 = min(bg_x2, x2 - 2)
                bg_y2 = min(bg_y2, y2 - 2)
                draw.rectangle([bg_x1, bg_y1, bg_x2, bg_y2], fill=color, outline=color)
                draw.text((text_x, text_y), label, fill='white', font=font)

        return image

    def send_alarm_notification(self, edited_img_data):
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

            # Add image
            framebox_attachment = MIMEImage(edited_img_data)
            framebox_attachment.add_header('Content-Disposition', 
                                          f'attachment; filename="pet_detected_annotated_{timestamp.replace(":", "-").replace(" ", "_")}.jpg"')
            msg.attach(framebox_attachment)

            # Send email
            server = smtplib.SMTP(self.smtp_server, self.smtp_port)
            server.starttls()
            server.login(self.email_user, self.email_password)
            text = msg.as_string()
            server.sendmail(self.email_user, self.notification_emails, text)
            server.quit()
            
            print(f"Alarm notification sent to {', '.join(self.notification_emails)}")
            return True
        except Exception as e:
            print(f"Error sending alarm notification: {e}")
            return False

    def do_POST(self):
        if self.path == '/image':
            try:
                content_len = int(self.headers.get('Content-Length', 0))
                snapshot_data = self.rfile.read(content_len)
                
                # Analyze with YOLOv5
                image = Image.open(io.BytesIO(snapshot_data))
                results = SnapshotHandler.model(image)
                
                # Check for cats (class 15) or dogs (class 16), sheeps(17) and bears (21)
                detections = results.pandas().xyxy[0]
                
                pet_detections = detections[detections['class'].isin(self.target_classes)]
                pet_found = len(pet_detections) > 0
                
                if pet_found:
                    # Create image
                    edited_img = image.copy()
                    edited_img = self.draw_detection_boxes(edited_img, pet_detections)
                    edited_img_data = io.BytesIO()
                    edited_img.save(edited_img_data, format='JPEG', quality=85)
                    edited_img_data.seek(0)
                    response_body = "ALARM"
                    self.send_response(200, 'ALARM')
                    self.send_header('Content-Type', 'text/plain')
                    self.send_header('Content-Length', str(len(response_body)))
                    self.send_header('Connection', 'close')
                    self.end_headers()
                    self.wfile.write(response_body.encode('utf-8'))
                    self.wfile.flush()
                    print("PET DETECTED - ALARM SENT")
                    for _, detection in pet_detections.iterrows():
                        class_names = {15: 'Cat', 16: 'Dog', 17: 'Sheep', 21: 'Bear'}
                        class_name = class_names.get(detection['class'], 'Pet')
                        print(f"{class_name} (conf: {detection['confidence']:.2f})")
                    try:
                        self.send_alarm_notification(edited_img_data.getvalue())
                    except Exception as e:
                        print(f"Email notification failed, but alarm was sent: {e}")
                        
                else:
                    response_body = "NO_PETS"
                    self.send_response(200)
                    self.send_header('Content-Type', 'text/plain') 
                    self.send_header('Content-Length', str(len(response_body)))
                    self.send_header('Connection', 'close')
                    self.end_headers()
                    self.wfile.write(response_body.encode('utf-8'))
                    self.wfile.flush()
                    print("No pets detected - sending NO_PETS response")
                
            except Exception as e:
                print(f"Error processing request: {e}")
                self.send_error(500, "Internal Server Error")
            
            finally:
                try:
                    self.connection.close()
                except:
                    pass
    
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
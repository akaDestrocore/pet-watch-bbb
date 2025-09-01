#!/usr/bin/env python3

from http.server import HTTPServer, BaseHTTPRequestHandler
import io
import torch
from PIL import Image

class SnapshotHandler(BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path == '/image':
            content_len = int(self.headers.get('Content-Length', 0))
            snapshot_data  = self.rfile.read(content_len)
            
            # Analyze with YOLOv5
            image = Image.open(io.BytesIO(snapshot_data ))
            results = model(image)
            
            # Check for cats (class 15) or dogs (class 16)
            detections = results.pandas().xyxy[0]
            pet_found = any(detection['class'] in [15, 16] for _, detection in detections.iterrows())
            
            # Send response
            self.send_response(200)
            self.send_header('Content-Type', 'text/plain')
            self.end_headers()
            
            if pet_found:
                self.wfile.write(b'Pet detected\nALARM')
                print("PET DETECTED - ALARM SENT")
            else:
                self.wfile.write(b'No pets detected')
    
    def log_message(self, format, *args):
        # mute spam
        pass

if __name__ == "__main__":
    print("Loading YOLOv5...")
    model = torch.hub.load('ultralytics/yolov5', 'yolov5s', pretrained=True)
    print("Starting server on port 7702...")
    
    server = HTTPServer(('0.0.0.0', 7702), SnapshotHandler)
    server.serve_forever()
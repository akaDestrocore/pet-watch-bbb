#!/usr/bin/env python3

from http.server import HTTPServer, BaseHTTPRequestHandler
import io

class SnapshotHandler(BaseHTTPRequestHandler):
    
    def do_POST(self):
        if self.path == '/image':
            content_len = int(self.headers.get('Content-Length', 0))
            snapshot_data = self.rfile.read(content_len)

            # Send response
            self.send_response(200)
            self.send_header('Content-Type', 'text/plain')
            self.end_headers()

            print("Snapshot received")
        

if __name__ == "__main__":
    print("Starting server on port 7702...")

    server = HTTPServer(('0.0.0.0', 7702), SnapshotHandler)
    server.serve_forever()
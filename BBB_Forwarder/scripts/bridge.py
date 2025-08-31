#!/usr/bin/env python3

from http.server import HTTPServer, BaseHTTPRequestHandler

class FileHandler(BaseHTTPRequestHandler):
    
    def do_POST(self):
        print("Received a POST request.")
        
        try:
            # Get data size
            content_length = int(self.headers.get('Content-Length', 0))
            print(f"Content length: {content_length} bytes")
            
            if content_length == 0:
                print("No data received!")
                self.send_response(400)
                self.send_header('Content-Type', 'text/plain')
                self.end_headers()
                self.wfile.write(b'No data received')
                return
            
            # Read the actual data
            post_data = self.rfile.read(content_length)
            print(f"Actually received: {len(post_data)} bytes")
            
            # Check file type
            if post_data.startswith(b'\xff\xd8'):
                print("Received a JPEG image.")
            else:
                print("Data received, incorrect format")
                # Dump 20 bytes
                print(f"First 20 bytes: {post_data[:20]}")
            
            # Save the file
            filename = "received_file.jpg"
            with open(filename, 'wb') as f:
                f.write(post_data)
            print(f"Saved data to {filename}")
            
            # Send success response
            self.send_response(200)
            self.send_header('Content-Type', 'text/plain')
            self.end_headers()
            self.wfile.write(f'Received {len(post_data)} bytes and saved to {filename}'.encode())
            
        except Exception as e:
            print(f"Error handling POST: {e}")
            self.send_response(500)
            self.send_header('Content-Type', 'text/plain')
            self.end_headers()
            self.wfile.write(f'Server error: {e}'.encode())
    
    def do_GET(self):
        print("Received a GET request.")
        
        self.send_response(200)
        self.send_header('Content-Type', 'text/html')
        self.end_headers()
        self.wfile.write(b'<h1>Test</h1><p>Send POST requests with file data here</p>')

def main():
    # Create server
    server = HTTPServer(('localhost', 7654), FileHandler)
    
    print("Starting server...")
    print("Visit: http://localhost:7654")
    print("Send POST requests with file data")
    print("Press Ctrl+C to stop")
    
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down...")
        server.server_close()

if __name__ == "__main__":
    main()
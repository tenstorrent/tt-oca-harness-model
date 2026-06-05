# #!/usr/bin/env python3
# """
# UART Terminal Client

# This script connects to a UART Terminal UI server socket and:
# - Displays received data on the terminal
# - Sends keyboard input to the socket
# """


"""
UART Terminal Client

This script connects to a UART Terminal UI server socket and:
- Displays received data on the terminal
- Sends keyboard input to the socket
- Closes cleanly on Ctrl+C
"""

import socket
import sys
import select
import time
import argparse
import threading
import signal
import re
from colorama import init

init(convert=True)

class UARTTerminalClient:
    def __init__(self, host='localhost', port=8888):
        self.host = host
        self.port = port
        self.socket = None
        self.running = False

    def connect(self):
        """Connect to the UART Terminal UI server"""
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.connect((self.host, self.port))
            #print(f"Connected to UART Terminal UI at {self.host}:{self.port}")
            return True
        except ConnectionRefusedError:
            print(f"Connection refused. Is the UART Terminal UI running at {self.host}:{self.port}?")
            return False
        except Exception as e:
            print(f"Connection error: {e}")
            return False

    def receive_data(self):
        """Thread function to receive and display data from the socket"""
        ansi_escape = re.compile(rb'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')
        while self.running:
            try:
                readable, _, _ = select.select([self.socket], [], [], 0.1)
                if self.socket in readable:
                    data = self.socket.recv(1024)
                    if not data:
                        print("\nConnection closed by server")
                        self.running = False
                        break
                    decoded = data.decode('utf-8', errors='replace')
                    sys.stdout.write(decoded)
                    sys.stdout.flush()
            except Exception as e:
                if self.running:
                    print(f"\nError receiving data: {e}")
                self.running = False
                break

    def send_keyboard_input(self):
        """Thread function to send keyboard input to the socket"""
        while self.running:
            try:
                readable, _, _ = select.select([sys.stdin], [], [], 0.1)
                if sys.stdin in readable:
                    char = sys.stdin.read(1)
                
                    if char == '\x03':  # Ctrl+C
                        raise KeyboardInterrupt
                    elif char == '\x7f':  # Backspace
                        sys.stdout.write('\b \b')
                        sys.stdout.flush()
                    elif char == '\x0d': # Enter
                        sys.stdout.write('\r\n')
                        sys.stdout.flush()
                    elif char == '\x09': # Tab
                        sys.stdout.write('\t')
                        sys.stdout.flush()
                    elif char == '\x1b':
                        select.select([sys.stdin], [], [], 0)
                        sys.stdin.read(1)  # '['
                        select.select([sys.stdin], [], [], 0)
                        sys.stdin.read(1)  # A/B/C/D
                        continue
                    elif char:
                        self.socket.sendall(char.encode('utf-8'))
            except KeyboardInterrupt:
                print("\nCtrl+C pressed — exiting UART client...")
                self.running = False
                break
            except Exception as e:
                if self.running:
                    print(f"\nError sending data: {e}")
                self.running = False
                break

    def stop(self):
        """Cleanly close the socket"""
        self.running = False
        if self.socket:
            try:
                self.socket.shutdown(socket.SHUT_RDWR)
            except Exception:
                pass
            self.socket.close()

    def run(self):
        """Run the terminal client"""
        if not self.connect():
            return False

        self.running = True

        # Create threads
        receive_thread = threading.Thread(target=self.receive_data, daemon=True)
        send_thread = threading.Thread(target=self.send_keyboard_input, daemon=True)
        receive_thread.start()
        send_thread.start()

        try:
            while self.running:
                time.sleep(0.1)
        except KeyboardInterrupt:
            print("\nKeyboardInterrupt detected — closing...")
        finally:
            self.stop()
            print("UART client closed.")
            sys.exit(0)


def main():
    parser = argparse.ArgumentParser(description='UART Terminal Client')
    parser.add_argument('--host', default='localhost', help='Server host (default: localhost)')
    parser.add_argument('--port', type=int, default=8888, help='Server port (default: 8888)')
    args = parser.parse_args()

    try:
        import termios, tty
        old_settings = termios.tcgetattr(sys.stdin)
        tty.setraw(sys.stdin.fileno())
        client = UARTTerminalClient(args.host, args.port)
        client.run()
    finally:
        try:
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_settings)
        except Exception:
            pass

if __name__ == "__main__":
    main()



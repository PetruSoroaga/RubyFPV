#!/usr/bin/env python3
#
# ruby_control_test.py - stand in for the Pi side of the Jetson control channel.
#
# Sends the same one line JSON messages that the video_source_udp backend of
# ruby_rt_vehicle sends, so the Jetson sender can be tested end to end before
# (or without) the patched vehicle binary.
#
# Examples:
#   ./ruby_control_test.py --host 192.168.10.2 --ping
#   ./ruby_control_test.py --host 192.168.10.2 --bitrate 4000000 --keyframe-ms 200
#   ./ruby_control_test.py --host 192.168.10.2 --idr
#   ./ruby_control_test.py --host 192.168.10.2 --sweep 2000000,6000000,12000000

import argparse
import json
import socket
import sys
import time


def send(sock, addr, obj, bWaitReply=False, fTimeout=1.0):
   szLine = json.dumps(obj) + "\n"
   sock.sendto(szLine.encode('utf-8'), addr)
   print("-> %s" % szLine.strip())
   if not bWaitReply:
      return None
   sock.settimeout(fTimeout)
   try:
      data, _ = sock.recvfrom(4096)
   except socket.timeout:
      print("<- (no reply)")
      return None
   szReply = data.decode('utf-8', 'ignore').strip()
   print("<- %s" % szReply)
   return szReply


def main():
   parser = argparse.ArgumentParser(description='RubyFPV Jetson control channel tester')
   parser.add_argument('--host', default='192.168.10.2', help='Jetson IP (default: %(default)s)')
   parser.add_argument('--port', type=int, default=5601, help='control port (default: %(default)s)')
   parser.add_argument('--bitrate', type=int, help='send {"bitrate_bps":N}')
   parser.add_argument('--keyframe-ms', type=int, help='send {"keyframe_ms":N}')
   parser.add_argument('--qp-delta', type=int, help='send {"qp_delta":N}')
   parser.add_argument('--fps', type=int, help='send {"fps":N}')
   parser.add_argument('--idr', action='store_true', help='send {"request_idr":1}')
   parser.add_argument('--ping', action='store_true', help='ping once and wait for the pong')
   parser.add_argument('--ping-loop', type=float, metavar='SEC',
                       help='ping every SEC seconds and print the round trip time')
   parser.add_argument('--sweep', metavar='BPS,BPS,...',
                       help='cycle through these bitrates, one every --sweep-interval seconds')
   parser.add_argument('--sweep-interval', type=float, default=5.0, help='default: %(default)s')
   args = parser.parse_args()

   sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
   addr = (args.host, args.port)
   print("Control target: %s:%d" % addr)

   bDidSomething = False

   if args.bitrate is not None:
      send(sock, addr, {'bitrate_bps': args.bitrate})
      bDidSomething = True
   if args.keyframe_ms is not None:
      send(sock, addr, {'keyframe_ms': args.keyframe_ms})
      bDidSomething = True
   if args.qp_delta is not None:
      send(sock, addr, {'qp_delta': args.qp_delta})
      bDidSomething = True
   if args.fps is not None:
      send(sock, addr, {'fps': args.fps})
      bDidSomething = True
   if args.idr:
      send(sock, addr, {'request_idr': 1})
      bDidSomething = True
   if args.ping:
      uNow = int(time.time() * 1000) & 0xFFFFFFFF
      send(sock, addr, {'ping': uNow}, bWaitReply=True)
      bDidSomething = True

   if args.sweep is not None:
      try:
         listBitrates = [int(s) for s in args.sweep.split(',') if len(s.strip()) > 0]
      except ValueError:
         print("Invalid --sweep list")
         return 1
      if 0 == len(listBitrates):
         print("Empty --sweep list")
         return 1
      print("Sweeping %s every %.1f sec, Ctrl-C to stop" % (listBitrates, args.sweep_interval))
      i = 0
      try:
         while True:
            send(sock, addr, {'bitrate_bps': listBitrates[i % len(listBitrates)]})
            i += 1
            time.sleep(args.sweep_interval)
      except KeyboardInterrupt:
         print("")
      bDidSomething = True

   if args.ping_loop is not None:
      print("Pinging every %.1f sec, Ctrl-C to stop" % args.ping_loop)
      try:
         while True:
            uNow = int(time.time() * 1000) & 0xFFFFFFFF
            fStart = time.time()
            szReply = send(sock, addr, {'ping': uNow}, bWaitReply=True)
            if szReply is not None:
               print("   round trip: %.1f ms" % ((time.time() - fStart) * 1000.0))
            time.sleep(args.ping_loop)
      except KeyboardInterrupt:
         print("")
      bDidSomething = True

   if not bDidSomething:
      parser.print_help()
      return 1
   return 0


if __name__ == '__main__':
   sys.exit(main())

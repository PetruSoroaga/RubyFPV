#!/usr/bin/env python3
#
# ruby_jetson_sender.py - RubyFPV Ethernet video source, Jetson side.
#
# Captures from a Jetson camera, encodes H.264 with the hardware encoder and
# streams it as RTP over UDP to the RubyFPV vehicle (Raspberry Pi), where the
# video_source_udp backend of ruby_rt_vehicle reads it (same wire format the
# OpenIPC/majestic backend already consumes on port 5600).
#
# A second UDP socket (control port, default 5601) receives one line JSON
# commands from the Pi and applies them live on the encoder, so Ruby's
# adaptive video link keeps driving the Jetson encoder:
#
#     {"bitrate_bps":6000000}
#     {"keyframe_ms":200}
#     {"qp_delta":-2}
#     {"fps":60}
#     {"request_idr":1}
#     {"ping":12345}         -> answered with {"pong":12345}
#
# See README.md in this folder for install, wiring and test instructions.

import argparse
import errno
import json
import signal
import socket
import sys
import threading
import time

import gi
gi.require_version('Gst', '1.0')
from gi.repository import Gst, GLib

try:
   gi.require_version('GstVideo', '1.0')
   from gi.repository import GstVideo
except (ValueError, ImportError):
   GstVideo = None


# ---------------------------------------------------------------------------
# logging (stdout, picked up by systemd/journald)
# ---------------------------------------------------------------------------

_log_lock = threading.Lock()

def log_line(fmt, *args):
   msg = (fmt % args) if args else fmt
   with _log_lock:
      sys.stdout.write("%s [JetsonSender] %s\n" % (time.strftime("%Y-%m-%d %H:%M:%S"), msg))
      sys.stdout.flush()

def log_softerror(fmt, *args):
   log_line("ERROR: " + fmt, *args)


# ---------------------------------------------------------------------------
# configuration
# ---------------------------------------------------------------------------

DEFAULTS = {
   'host': '192.168.10.1',     # RubyFPV vehicle (Pi) IP, or 'auto' to learn it
   'port': 5600,               # video RTP port on the Pi
   'control_port': 5601,       # control port we listen on
   'control_allow': '',        # if set, only accept control from this IP
   'source': 'argus',          # argus | v4l2 | test
   'sensor_id': 0,
   'device': '/dev/video0',
   'width': 1280,
   'height': 720,
   'fps': 60,
   'bitrate': 6000000,         # bps
   'min_bitrate': 250000,
   'max_bitrate': 25000000,
   'keyframe_ms': 200,
   'qp_map': 'ignore',         # ignore | range
   'qp_min': 10,
   'qp_max': 51,
   'encoder': 'auto',          # auto | nv | x264
   'profile': 'baseline',      # baseline | main | high  (nv encoder only)
   'control_rate': 1,          # nvv4l2h264enc: 0 variable, 1 constant
   'preset_level': 1,
   'mtu': 1400,
   'stats_interval': 10,       # seconds, 0 disables
   'watchdog': 5,              # seconds without encoded data before restart
   'restart_delay': 2,         # seconds before a pipeline restart
   'max_restart_delay': 10,
   'pipeline': '',             # full custom pipeline override
}

INT_KEYS = ('port', 'control_port', 'sensor_id', 'width', 'height', 'fps', 'bitrate',
            'min_bitrate', 'max_bitrate', 'keyframe_ms', 'qp_min', 'qp_max',
            'control_rate', 'preset_level', 'mtu', 'stats_interval', 'watchdog',
            'restart_delay', 'max_restart_delay')

NV_PROFILES = {'baseline': 0, 'main': 2, 'high': 4}


def clamp(value, vMin, vMax):
   if value < vMin:
      return vMin
   if value > vMax:
      return vMax
   return value


def load_config_file(szPath, cfg):
   # Very small 'key = value' parser, '#' starts a comment. Unknown keys are
   # reported but not fatal, so a config can be shared with future versions.
   try:
      fd = open(szPath, 'r')
   except IOError as e:
      if e.errno == errno.ENOENT:
         return False
      log_softerror("Can't read config file %s: %s", szPath, e)
      return False
   with fd:
      for iLine, szLine in enumerate(fd, 1):
         szLine = szLine.split('#', 1)[0].strip()
         if 0 == len(szLine):
            continue
         if '=' not in szLine:
            log_softerror("Config %s line %d: ignored (no '='): %s", szPath, iLine, szLine)
            continue
         szKey, szValue = szLine.split('=', 1)
         szKey = szKey.strip().lower().replace('-', '_')
         szValue = szValue.strip().strip('"').strip("'")
         if szKey not in DEFAULTS:
            log_softerror("Config %s line %d: unknown key '%s'", szPath, iLine, szKey)
            continue
         if szKey in INT_KEYS:
            try:
               cfg[szKey] = int(szValue)
            except ValueError:
               log_softerror("Config %s line %d: '%s' is not a number", szPath, iLine, szValue)
         else:
            cfg[szKey] = szValue
   log_line("Loaded config file %s", szPath)
   return True


def parse_args():
   parser = argparse.ArgumentParser(description='RubyFPV Jetson H.264 RTP sender')
   parser.add_argument('--config', default='/etc/ruby_jetson_sender.conf',
                       help='config file (default: %(default)s, optional)')
   for szKey, value in sorted(DEFAULTS.items()):
      szFlag = '--' + szKey.replace('_', '-')
      if szKey in INT_KEYS:
         parser.add_argument(szFlag, type=int, default=None, help='default: %s' % value)
      else:
         parser.add_argument(szFlag, default=None, help='default: %s' % (value if value != '' else 'none'))
   args = parser.parse_args()

   cfg = dict(DEFAULTS)
   load_config_file(args.config, cfg)
   # Command line wins over the config file.
   for szKey in DEFAULTS:
      value = getattr(args, szKey, None)
      if value is not None:
         cfg[szKey] = value

   if cfg['source'] not in ('argus', 'v4l2', 'test'):
      log_softerror("Unknown source '%s', falling back to 'argus'", cfg['source'])
      cfg['source'] = 'argus'
   if cfg['profile'] not in NV_PROFILES:
      log_softerror("Unknown profile '%s', falling back to 'baseline'", cfg['profile'])
      cfg['profile'] = 'baseline'
   if cfg['fps'] < 1:
      cfg['fps'] = 30
   cfg['bitrate'] = clamp(cfg['bitrate'], cfg['min_bitrate'], cfg['max_bitrate'])
   return cfg


# ---------------------------------------------------------------------------
# pipeline
# ---------------------------------------------------------------------------

def detect_encoder(szWanted):
   if szWanted in ('nv', 'x264'):
      return szWanted
   if Gst.ElementFactory.find('nvv4l2h264enc') is not None:
      return 'nv'
   log_line("nvv4l2h264enc not found, using software encoder x264enc (expect higher latency)")
   return 'x264'


def build_pipeline_desc(cfg, szEncoder, iGopFrames, szHost):
   if len(cfg['pipeline']) > 0:
      return cfg['pipeline']

   iW = cfg['width']
   iH = cfg['height']
   iFps = cfg['fps']

   if cfg['source'] == 'argus':
      szSource = ("nvarguscamerasrc sensor-id=%d do-timestamp=true ! "
                  "video/x-raw(memory:NVMM),width=%d,height=%d,framerate=%d/1,format=NV12"
                  % (cfg['sensor_id'], iW, iH, iFps))
   elif cfg['source'] == 'v4l2':
      szSource = ("v4l2src device=%s io-mode=2 do-timestamp=true ! "
                  "video/x-raw,width=%d,height=%d,framerate=%d/1"
                  % (cfg['device'], iW, iH, iFps))
   else:
      szSource = ("videotestsrc is-live=true pattern=smpte ! "
                  "video/x-raw,width=%d,height=%d,framerate=%d/1,format=I420"
                  % (iW, iH, iFps))

   if szEncoder == 'nv':
      if cfg['source'] != 'argus':
         szSource += " ! nvvidconv ! video/x-raw(memory:NVMM),format=NV12"
      szEnc = ("nvv4l2h264enc name=enc bitrate=%d peak-bitrate=%d control-rate=%d "
               "iframeinterval=%d idrinterval=%d insert-sps-pps=true insert-vui=true "
               "maxperf-enable=true preset-level=%d profile=%d"
               % (cfg['bitrate'], int(cfg['bitrate'] * 1.2), cfg['control_rate'],
                  iGopFrames, iGopFrames, cfg['preset_level'], NV_PROFILES[cfg['profile']]))
   else:
      if cfg['source'] == 'argus':
         szSource += " ! nvvidconv ! video/x-raw,format=I420"
      else:
         szSource += " ! videoconvert ! video/x-raw,format=I420"
      # x264enc takes kbit/s.
      szEnc = ("x264enc name=enc tune=zerolatency speed-preset=ultrafast bframes=0 "
               "bitrate=%d key-int-max=%d ! video/x-h264,profile=baseline"
               % (max(1, cfg['bitrate'] // 1000), iGopFrames))

   return ("%s ! %s ! h264parse config-interval=1 ! "
           "rtph264pay name=pay pt=96 mtu=%d config-interval=1 ! "
           "udpsink name=sink host=%s port=%d sync=false async=false"
           % (szSource, szEnc, cfg['mtu'], szHost, cfg['port']))


class VideoSender(object):

   def __init__(self, cfg, mainloop):
      self.cfg = cfg
      self.mainloop = mainloop
      self.pipeline = None
      self.enc = None
      self.sink = None
      self.bus = None
      self.szEncoder = detect_encoder(cfg['encoder'])
      self.uBitrate = cfg['bitrate']
      self.iKeyframeMs = cfg['keyframe_ms']
      self.iQPDelta = 0
      self.iFps = cfg['fps']
      self.szHost = cfg['host'] if cfg['host'] != 'auto' else ''
      self.bRestartPending = False
      self.iRestartDelay = cfg['restart_delay']
      self.uLastDataTime = 0
      self.uPacketsSent = 0
      self.uBytesSent = 0
      self.uLastStatsPackets = 0
      self.uLastStatsBytes = 0
      self.uLastStatsTime = time.time()
      self.bStopping = False
      self.bWarnedQPDelta = False
      self.uIdrCount = 0

   # --- helpers ---

   def gop_frames(self):
      iGop = int(round(self.iFps * self.iKeyframeMs / 1000.0))
      return clamp(iGop, 1, 1000)

   def has_property(self, element, szName):
      if element is None:
         return False
      return element.find_property(szName) is not None

   # --- lifecycle ---

   def start(self):
      if self.bStopping:
         return False
      szHost = self.szHost if len(self.szHost) > 0 else '127.0.0.1'
      szDesc = build_pipeline_desc(self.cfg, self.szEncoder, self.gop_frames(), szHost)
      log_line("Starting pipeline (%dx%d@%d, %.2f Mbps, keyframe %d ms -> gop %d frames, encoder %s)",
               self.cfg['width'], self.cfg['height'], self.iFps,
               self.uBitrate / 1000000.0, self.iKeyframeMs, self.gop_frames(), self.szEncoder)
      log_line("Pipeline: %s", szDesc)

      try:
         self.pipeline = Gst.parse_launch(szDesc)
      except GLib.Error as e:
         log_softerror("Failed to build pipeline: %s", e)
         self.pipeline = None
         self.schedule_restart("pipeline build failed")
         return False

      self.enc = self.pipeline.get_by_name('enc')
      self.sink = self.pipeline.get_by_name('sink')
      if self.enc is None:
         log_softerror("Pipeline has no element named 'enc', bitrate control will not work")
      if self.sink is None:
         log_softerror("Pipeline has no element named 'sink', video destination can't be changed")
      elif 0 == len(self.szHost):
         log_line("Video destination not known yet (host=auto), waiting for the first control packet from the Pi")

      if self.sink is not None:
         pad = self.sink.get_static_pad('sink')
         if pad is not None:
            pad.add_probe(Gst.PadProbeType.BUFFER, self._on_sink_buffer, None)

      self.bus = self.pipeline.get_bus()
      self.bus.add_signal_watch()
      self.bus.connect('message', self._on_bus_message)

      self.uLastDataTime = time.time()
      if Gst.StateChangeReturn.FAILURE == self.pipeline.set_state(Gst.State.PLAYING):
         log_softerror("Failed to set pipeline to PLAYING")
         self.schedule_restart("set_state failed")
         return False
      return True

   def stop(self):
      if self.pipeline is None:
         return
      log_line("Stopping pipeline")
      try:
         if self.bus is not None:
            self.bus.remove_signal_watch()
         self.pipeline.set_state(Gst.State.NULL)
      except Exception as e:
         log_softerror("Exception while stopping pipeline: %s", e)
      self.pipeline = None
      self.enc = None
      self.sink = None
      self.bus = None

   def schedule_restart(self, szReason):
      if self.bStopping or self.bRestartPending:
         return
      self.bRestartPending = True
      log_line("Restarting pipeline in %d sec (%s)", self.iRestartDelay, szReason)
      GLib.timeout_add_seconds(self.iRestartDelay, self._do_restart)
      self.iRestartDelay = min(self.iRestartDelay * 2, self.cfg['max_restart_delay'])

   def _do_restart(self):
      self.bRestartPending = False
      if self.bStopping:
         return False
      self.stop()
      self.start()
      return False

   def request_restart_now(self, szReason):
      # Called from the control thread, bounce it to the main loop.
      GLib.idle_add(self._restart_now_idle, szReason)

   def _restart_now_idle(self, szReason):
      if self.bStopping:
         return False
      log_line("Restarting pipeline (%s)", szReason)
      self.stop()
      self.start()
      return False

   # --- bus / probes ---

   def _on_bus_message(self, bus, message):
      t = message.type
      if t == Gst.MessageType.ERROR:
         err, dbg = message.parse_error()
         log_softerror("GStreamer error from %s: %s (%s)",
                       message.src.get_name() if message.src else '?', err.message, dbg)
         self.schedule_restart("gstreamer error")
      elif t == Gst.MessageType.WARNING:
         err, dbg = message.parse_warning()
         log_line("GStreamer warning from %s: %s",
                  message.src.get_name() if message.src else '?', err.message)
      elif t == Gst.MessageType.EOS:
         log_softerror("Unexpected end of stream")
         self.schedule_restart("end of stream")
      elif t == Gst.MessageType.STATE_CHANGED:
         if (self.pipeline is not None) and (message.src == self.pipeline):
            old, new, pending = message.parse_state_changed()
            if new == Gst.State.PLAYING:
               log_line("Pipeline is PLAYING")
               self.iRestartDelay = self.cfg['restart_delay']
      return True

   def _on_sink_buffer(self, pad, info, user_data):
      buf = info.get_buffer()
      if buf is not None:
         self.uPacketsSent += 1
         self.uBytesSent += buf.get_size()
      self.uLastDataTime = time.time()
      return Gst.PadProbeReturn.OK

   # --- periodic ---

   def on_watchdog(self):
      if self.bStopping:
         return False
      if (self.cfg['watchdog'] <= 0) or (self.pipeline is None) or self.bRestartPending:
         return True
      if 0 == len(self.szHost):
         return True
      if time.time() - self.uLastDataTime > self.cfg['watchdog']:
         log_softerror("No encoded data for %d sec", self.cfg['watchdog'])
         self.schedule_restart("no data from encoder")
      return True

   def on_stats(self):
      if self.bStopping:
         return False
      uNow = time.time()
      dt = uNow - self.uLastStatsTime
      if dt <= 0:
         return True
      uPackets = self.uPacketsSent - self.uLastStatsPackets
      uBytes = self.uBytesSent - self.uLastStatsBytes
      self.uLastStatsPackets = self.uPacketsSent
      self.uLastStatsBytes = self.uBytesSent
      self.uLastStatsTime = uNow
      log_line("Stats: %.2f Mbps out (%d pkts/sec) to %s:%d, encoder set to %.2f Mbps, keyframe %d ms, IDRs forced: %d",
               (uBytes * 8.0) / (dt * 1000000.0), int(uPackets / dt),
               self.szHost if len(self.szHost) > 0 else '(unknown)', self.cfg['port'],
               self.uBitrate / 1000000.0, self.iKeyframeMs, self.uIdrCount)
      return True

   # --- controls (called from the control thread) ---

   def set_destination(self, szHost):
      if szHost == self.szHost:
         return
      log_line("Video destination set to %s:%d", szHost, self.cfg['port'])
      self.szHost = szHost
      self.uLastDataTime = time.time()
      if self.sink is not None:
         self.sink.set_property('host', szHost)
      else:
         self.request_restart_now("video destination changed")

   def set_bitrate(self, uBitrateBps):
      uBitrateBps = clamp(int(uBitrateBps), self.cfg['min_bitrate'], self.cfg['max_bitrate'])
      if uBitrateBps == self.uBitrate:
         return
      self.uBitrate = uBitrateBps
      if self.enc is None:
         return
      try:
         if self.szEncoder == 'nv':
            self.enc.set_property('bitrate', uBitrateBps)
            if self.has_property(self.enc, 'peak-bitrate'):
               self.enc.set_property('peak-bitrate', int(uBitrateBps * 1.2))
         else:
            self.enc.set_property('bitrate', max(1, uBitrateBps // 1000))
      except Exception as e:
         log_softerror("Can't set bitrate: %s", e)
         return
      log_line("Set bitrate to %.2f Mbps", uBitrateBps / 1000000.0)

   def set_keyframe_ms(self, iKeyframeMs):
      iKeyframeMs = clamp(int(iKeyframeMs), 20, 20000)
      if iKeyframeMs == self.iKeyframeMs:
         return
      self.iKeyframeMs = iKeyframeMs
      iGop = self.gop_frames()
      if self.enc is None:
         return
      try:
         if self.szEncoder == 'nv':
            self.enc.set_property('iframeinterval', iGop)
            if self.has_property(self.enc, 'idrinterval'):
               self.enc.set_property('idrinterval', iGop)
         else:
            self.enc.set_property('key-int-max', iGop)
      except Exception as e:
         log_softerror("Can't set keyframe interval: %s", e)
         return
      log_line("Set keyframe interval to %d ms (%d frames)", iKeyframeMs, iGop)

   def set_qp_delta(self, iQPDelta):
      iQPDelta = clamp(int(iQPDelta), -30, 30)
      if iQPDelta == self.iQPDelta:
         return
      self.iQPDelta = iQPDelta
      if (self.cfg['qp_map'] != 'range') or (not self.has_property(self.enc, 'qp-range')):
         if not self.bWarnedQPDelta:
            self.bWarnedQPDelta = True
            log_line("qp_delta is not supported by this encoder (qp_map=%s), ignoring it (bitrate control still applies)",
                     self.cfg['qp_map'])
         return
      iMin = clamp(self.cfg['qp_min'] + iQPDelta, 0, 51)
      iMax = clamp(self.cfg['qp_max'] + iQPDelta, iMin, 51)
      szRange = "%d,%d:%d,%d:%d,%d" % (iMin, iMax, iMin, iMax, iMin, iMax)
      try:
         self.enc.set_property('qp-range', szRange)
      except Exception as e:
         log_softerror("Can't set qp-range: %s", e)
         return
      log_line("Mapped qp_delta %d to qp-range %s", iQPDelta, szRange)

   def set_fps(self, iFps):
      iFps = clamp(int(iFps), 1, 240)
      if iFps == self.iFps:
         return
      log_line("Frame rate change requested: %d -> %d fps, restarting pipeline", self.iFps, iFps)
      self.iFps = iFps
      self.cfg['fps'] = iFps
      self.request_restart_now("frame rate changed")

   def force_idr(self):
      if self.enc is None:
         return
      self.uIdrCount += 1
      for szName in ('force-IDR', 'force-idr', 'force-key-unit'):
         if self.has_property(self.enc, szName):
            try:
               self.enc.set_property(szName, True)
               return
            except Exception:
               pass
      if GstVideo is None:
         log_softerror("Can't force an IDR frame: no encoder property and GstVideo is not available")
         return
      pad = self.enc.get_static_pad('src')
      if pad is None:
         return
      try:
         event = GstVideo.video_event_new_upstream_force_key_unit(Gst.CLOCK_TIME_NONE, True, self.uIdrCount)
         pad.send_event(event)
      except Exception as e:
         log_softerror("Can't send force-key-unit event: %s", e)


# ---------------------------------------------------------------------------
# control channel (Pi -> Jetson)
# ---------------------------------------------------------------------------

class ControlServer(threading.Thread):

   def __init__(self, sender, cfg):
      threading.Thread.__init__(self)
      self.daemon = True
      self.sender = sender
      self.cfg = cfg
      self.bStop = False
      self.sock = None
      self.uMessages = 0
      self.szLastPeer = ''
      self.setUnknownKeys = set()

   def stop(self):
      self.bStop = True
      if self.sock is not None:
         try:
            self.sock.close()
         except Exception:
            pass

   def run(self):
      while not self.bStop:
         if not self._open_socket():
            time.sleep(2)
            continue
         while not self.bStop:
            try:
               data, addr = self.sock.recvfrom(4096)
            except socket.timeout:
               continue
            except OSError as e:
               if self.bStop:
                  return
               log_softerror("Control socket read error: %s", e)
               break
            self._on_datagram(data, addr)
         try:
            self.sock.close()
         except Exception:
            pass
         self.sock = None

   def _open_socket(self):
      try:
         self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
         self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
         self.sock.settimeout(1.0)
         self.sock.bind(('0.0.0.0', self.cfg['control_port']))
      except OSError as e:
         log_softerror("Can't open control socket on port %d: %s", self.cfg['control_port'], e)
         self.sock = None
         return False
      log_line("Listening for control messages on UDP port %d", self.cfg['control_port'])
      return True

   def _on_datagram(self, data, addr):
      szFrom = addr[0]
      if (len(self.cfg['control_allow']) > 0) and (szFrom != self.cfg['control_allow']):
         return
      if szFrom != self.szLastPeer:
         self.szLastPeer = szFrom
         log_line("Control channel peer is %s:%d", addr[0], addr[1])
      if self.cfg['host'] == 'auto':
         self.sender.set_destination(szFrom)

      szText = data.decode('utf-8', 'ignore')
      for szLine in szText.splitlines():
         szLine = szLine.strip().strip('\0')
         if 0 == len(szLine):
            continue
         try:
            cmd = json.loads(szLine)
         except ValueError:
            log_softerror("Ignoring malformed control message: %s", szLine[:160])
            continue
         if not isinstance(cmd, dict):
            continue
         self.uMessages += 1
         self._apply(cmd, addr)

   def _apply(self, cmd, addr):
      for szKey, value in cmd.items():
         try:
            if szKey == 'ping':
               self._reply({'pong': value}, addr)
            elif szKey in ('bitrate_bps', 'bitrate'):
               self.sender.set_bitrate(value)
            elif szKey == 'keyframe_ms':
               self.sender.set_keyframe_ms(value)
            elif szKey == 'qp_delta':
               self.sender.set_qp_delta(value)
            elif szKey == 'fps':
               self.sender.set_fps(value)
            elif szKey == 'request_idr':
               if value:
                  self.sender.force_idr()
            elif szKey not in self.setUnknownKeys:
               self.setUnknownKeys.add(szKey)
               log_line("Ignoring unknown control key '%s'", szKey)
         except (TypeError, ValueError) as e:
            log_softerror("Bad value for control key '%s': %s", szKey, e)

   def _reply(self, obj, addr):
      if self.sock is None:
         return
      try:
         self.sock.sendto((json.dumps(obj) + "\n").encode('utf-8'), addr)
      except OSError as e:
         log_softerror("Can't send control reply to %s:%d: %s", addr[0], addr[1], e)


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def main():
   cfg = parse_args()
   Gst.init(None)

   log_line("RubyFPV Jetson sender starting (video to %s:%d, control on UDP %d)",
            cfg['host'], cfg['port'], cfg['control_port'])

   mainloop = GLib.MainLoop()
   sender = VideoSender(cfg, mainloop)
   control = ControlServer(sender, cfg)

   def do_quit(*unused):
      if sender.bStopping:
         return False
      log_line("Shutting down")
      sender.bStopping = True
      control.stop()
      sender.stop()
      mainloop.quit()
      return False

   try:
      GLib.unix_signal_add(GLib.PRIORITY_DEFAULT, signal.SIGINT, do_quit)
      GLib.unix_signal_add(GLib.PRIORITY_DEFAULT, signal.SIGTERM, do_quit)
   except AttributeError:
      signal.signal(signal.SIGINT, lambda s, f: do_quit())
      signal.signal(signal.SIGTERM, lambda s, f: do_quit())

   control.start()
   sender.start()

   if cfg['watchdog'] > 0:
      GLib.timeout_add_seconds(1, sender.on_watchdog)
   if cfg['stats_interval'] > 0:
      GLib.timeout_add_seconds(cfg['stats_interval'], sender.on_stats)

   try:
      mainloop.run()
   except KeyboardInterrupt:
      do_quit()
   sender.stop()
   log_line("Stopped")
   return 0


if __name__ == '__main__':
   sys.exit(main())

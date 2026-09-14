# RubyFPV Jetson sender

Jetson side of the Ethernet video source: captures from a Jetson camera, encodes
H.264 with the hardware encoder (`nvv4l2h264enc`) and streams it as RTP over UDP
to the RubyFPV vehicle (Raspberry Pi), where the `video_source_udp` backend of
`ruby_rt_vehicle` reads it on port 5600 - the same wire format the OpenIPC
(majestic) backend already consumes.

A second UDP socket (default port 5601) receives control messages from the Pi so
Ruby's adaptive video link keeps driving the Jetson encoder in flight.

```
  Jetson Nano                                    Raspberry Pi (RubyFPV vehicle)
  ------------------                             ------------------------------
  nvarguscamerasrc                               ruby_rt_vehicle
    -> nvv4l2h264enc      RTP/H264 :5600  --->     video_source_udp
    -> rtph264pay                                    -> radio link -> controller
    -> udpsink
  control listener :5601  <--- JSON control -----  video_sources_set_video_bitrate
                                                    video_sources_set_keyframe
```

## Files

| File | Purpose |
| --- | --- |
| `ruby_jetson_sender.py` | The sender itself (GStreamer + control listener) |
| `ruby_jetson_sender.conf` | Default configuration, installed to `/etc/ruby_jetson_sender.conf` |
| `ruby_jetson_sender.service` | systemd unit |
| `install.sh` | Installs script + config + unit, enables the service |
| `setup_static_ip.sh` | Static IP on the direct Ethernet link (default `eth0` / `192.168.10.2/24`) |
| `ruby_control_test.py` | Stands in for the Pi: sends control messages, for testing |

## Requirements

Jetson (L4T / JetPack) with GStreamer and the Python GObject bindings:

```
sudo apt-get install -y python3-gi python3-gi-cairo gir1.2-gstreamer-1.0 \
    gir1.2-gst-plugins-base-1.0 gstreamer1.0-tools gstreamer1.0-plugins-good
gst-inspect-1.0 nvv4l2h264enc     # must print the encoder details
```

If `nvv4l2h264enc` is missing the sender falls back to `x264enc` (software) so it
still runs on a laptop or a Pi for bench tests, at a noticeably higher latency.

## Install

```
sudo ./setup_static_ip.sh eth0 192.168.10.2/24
sudo ./install.sh
sudoedit /etc/ruby_jetson_sender.conf      # check host, resolution, fps, bitrate
sudo systemctl start ruby_jetson_sender
journalctl -u ruby_jetson_sender -f
```

Run it in the foreground instead, while tuning:

```
sudo python3 ruby_jetson_sender.py --host 192.168.10.1 --width 1280 --height 720 --fps 60
```

Every config key is also a command line flag (`control_port` -> `--control-port`);
the command line wins over the config file.

## Control protocol (Pi -> Jetson, UDP 5601)

One JSON object per line, one or more lines per datagram:

| Message | Effect |
| --- | --- |
| `{"bitrate_bps":6000000}` | `enc.bitrate` (clamped to `min_bitrate`..`max_bitrate`) |
| `{"keyframe_ms":200}` | `iframeinterval`/`idrinterval`, converted to frames with the current fps |
| `{"qp_delta":-2}` | mapped to `qp-range` when `qp_map = range`, otherwise logged once and ignored |
| `{"fps":60}` | caps change, so the pipeline is restarted |
| `{"request_idr":1}` | forces an IDR (`force-IDR` property, else an upstream force-key-unit event) |
| `{"ping":12345}` | answered with `{"pong":12345}` to the sender address and port |

Unknown keys are logged once and ignored, so the vehicle side can add messages
without breaking an older Jetson.

With `host = auto` the video destination is taken from the source address of the
first control packet, which means the Pi does not have to know the Jetson
configuration - it just has to talk to it once.

## Notes on the encoder

- `nvv4l2h264enc` has no QP delta control equivalent to majestic's. `qp_delta` is
  ignored by default (`qp_map = ignore`); set `qp_map = range` to translate it
  into a shifted `qp-range`, which is coarser but does move quality.
- `control-rate = 1` is constant bitrate, which is what the adaptive link
  expects: when Ruby lowers the bitrate the output follows within a frame or two.
- `profile = baseline` is the safest default for the controller side decoder;
  `main` or `high` give better quality at the same bitrate if the decoder copes.
- `mtu = 1400` in `rtph264pay` keeps RTP packets below the 1500 byte link MTU, so
  nothing gets IP-fragmented on the way to the Pi.
- `insert-sps-pps=true` plus `config-interval=1` on both `h264parse` and
  `rtph264pay` means the Pi can pick the stream up mid-flight after a restart.

## Robustness

- Any GStreamer error or EOS restarts the pipeline with a backoff
  (`restart_delay` doubling up to `max_restart_delay`). A Pi that is down or
  unreachable surfaces as a `udpsink` error and is retried the same way.
- A watchdog restarts the pipeline if the encoder produces no data for
  `watchdog` seconds (camera unplugged, Argus daemon wedged).
- `stats_interval` logs measured output throughput, the bitrate currently set on
  the encoder, the keyframe interval and the number of forced IDRs.

## Testing

1. **Without RubyFPV** - on the Pi, confirm NALs arrive:

   ```
   gst-launch-1.0 udpsrc port=5600 ! application/x-rtp,encoding-name=H264 \
       ! rtph264depay ! h264parse ! fakesink dump=true
   ```

   Or decode it on any Linux box on the link:

   ```
   gst-launch-1.0 udpsrc port=5600 ! application/x-rtp,encoding-name=H264,payload=96 \
       ! rtpjitterbuffer latency=0 ! rtph264depay ! avdec_h264 ! autovideosink sync=false
   ```

2. **Without a camera** - `--source test` streams a test pattern, so the link and
   the control channel can be checked before the camera is wired up.

3. **Control channel** - from the Pi (or any machine on the link):

   ```
   ./ruby_control_test.py --host 192.168.10.2 --ping
   ./ruby_control_test.py --host 192.168.10.2 --bitrate 3000000
   ./ruby_control_test.py --host 192.168.10.2 --sweep 2000000,6000000,12000000
   ```

   The Jetson log must show `Set bitrate to ...` and the measured throughput in
   the next stats line must follow it.

4. **With the patched `ruby_rt_vehicle`** - look for the `[VideoSourceUDP]` lines
   in `logs/` on the Pi (socket open, first NAL, SPS/PPS, IDR), then check on the
   controller OSD that the bitrate moves when the radio link is degraded.

5. **Latency** - point the camera at a millisecond stopwatch on a screen and
   photograph screen and OSD together. Target is under 120 ms glass to glass at
   720p60; `preset_level = 1`, `control_rate = 1` and `sync=false` on the sink are
   the settings that matter most here.

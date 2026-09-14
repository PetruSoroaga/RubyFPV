# IP / Ethernet video source (vehicle side)

Lets a Ruby vehicle take its video from an external encoder reached over a regular
network link (Ethernet, USB-Ethernet) instead of from a local CSI camera.

The external encoder sends H264 or H265 as RTP over UDP to the vehicle. The vehicle
parses the RTP stream and feeds the NALs to the router exactly like it does for an
OpenIPC camera, so the whole radio pipeline (EC scheme, adaptive video, OSD stats)
works unchanged. Encoder settings travel back to the encoder on a second UDP port
as one line JSON messages, which is what keeps the adaptive video link working.

## Enabling it on the vehicle

1. Copy `ip_camera.conf.sample` to the vehicle as `/home/pi/ruby/config/ip_camera.conf`
   and edit the ports and the remote IP if needed.
2. Copy the rebuilt `ruby_rt_vehicle` and `ruby_start` binaries to `/home/pi/ruby/`.
   Reflashing the Ruby image is not needed.
3. Reboot the vehicle.

The config file is what activates the source. While it exists, camera detection reports
camera type `CAMERA_TYPE_IP` even on a board with no CSI camera attached, `hasCamera()`
returns true, and no local capture process (`ruby_capture_raspi`) is launched. Deleting
the file reverts to regular camera autodetection.

The camera type can also be forced from the controller, under Camera Settings, by setting
Camera Type to "IP Camera".

## Control protocol

The vehicle sends these one line JSON messages, UDP, to the encoder on `control_port`.
An encoder implementation must accept them in any order and at any time.

| Message | Meaning |
| --- | --- |
| `{"bitrate_bps":6000000}` | Target video bitrate. Sent by adaptive video, so it changes often. |
| `{"keyframe_ms":200}` | Keyframe interval in milliseconds. |
| `{"qp_delta":-2}` | I/P quantization delta. Ignore it with a log line if the encoder has no equivalent. |
| `{"fps":60}` | Target framerate. |
| `{"request_idr":1}` | Generate an IDR now. Sent after a reconnect and when the stream stalls. |
| `{"ping":123456}` | Health check. The encoder should reply `{"pong":123456}` to the same port. |

The reply goes back to `control_port` on the vehicle. Only `pong` is interpreted; any other
reply is logged and ignored.

## Network

Use static addresses on the direct link, for example vehicle `192.168.10.1` and
encoder `192.168.10.2`. Ruby manages its own network setup (`ruby_initdhcp`, the Network
menu), so add the static address for `eth0` outside of Ruby, through `dhcpcd.conf` or an
`ip addr add` at boot, rather than changing Ruby's network configuration.

Keep the RTP payload under the path MTU so packets are never fragmented. With a 1500 byte
MTU, an `mtu=1400` on the encoder's RTP payloader leaves comfortable headroom.

## Checking that it works

Confirm NALs are arriving, with Ruby stopped:

```
gst-launch-1.0 udpsrc port=5600 ! application/x-rtp,encoding-name=H264 ! rtph264depay ! h264parse ! fakesink dump=true
```

Then start Ruby and look for `[VideoSourceUDP]` lines in `/home/pi/ruby/logs/`. A healthy
start shows the socket opening, the remote encoder address being detected, the control
messages being sent, and an input data rate line every 10 seconds.

If no video arrives for 3 seconds the vehicle raises a video capture alarm on the controller
and asks for an IDR. After 10 seconds it reopens the input socket and pushes all the encoder
parameters again, which also covers the case of the encoder being restarted.

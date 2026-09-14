#!/bin/bash
#
# Installs the RubyFPV Jetson sender as a systemd service.
# Run on the Jetson:   sudo ./install.sh
#
# Installs:
#   /opt/ruby_jetson_sender/ruby_jetson_sender.py
#   /etc/ruby_jetson_sender.conf            (not overwritten if it exists)
#   /etc/systemd/system/ruby_jetson_sender.service

set -e

INSTALL_DIR=/opt/ruby_jetson_sender
CONFIG_FILE=/etc/ruby_jetson_sender.conf
SERVICE_FILE=/etc/systemd/system/ruby_jetson_sender.service
SRC_DIR="$(cd "$(dirname "$0")" && pwd)"

if [ "$(id -u)" != "0" ]; then
   echo "This script must run as root (use sudo)."
   exit 1
fi

echo "Checking dependencies..."
MISSING=""
python3 -c "import gi; gi.require_version('Gst','1.0')" 2>/dev/null || MISSING="$MISSING python3-gi gstreamer1.0-tools"
command -v gst-inspect-1.0 >/dev/null 2>&1 || MISSING="$MISSING gstreamer1.0-tools"
if [ -n "$MISSING" ]; then
   echo "Missing dependencies. Install them with:"
   echo "   sudo apt-get install -y python3-gi python3-gi-cairo gir1.2-gstreamer-1.0 gir1.2-gst-plugins-base-1.0 gstreamer1.0-tools gstreamer1.0-plugins-good"
   exit 1
fi

if ! gst-inspect-1.0 nvv4l2h264enc >/dev/null 2>&1; then
   echo "WARNING: nvv4l2h264enc not found. The sender will fall back to the"
   echo "         software encoder (x264enc), which adds latency."
   echo "         On a Jetson install: sudo apt-get install -y nvidia-l4t-gstreamer"
fi

echo "Installing to $INSTALL_DIR ..."
mkdir -p "$INSTALL_DIR"
install -m 0755 "$SRC_DIR/ruby_jetson_sender.py" "$INSTALL_DIR/ruby_jetson_sender.py"
install -m 0644 "$SRC_DIR/ruby_control_test.py" "$INSTALL_DIR/ruby_control_test.py"

if [ -f "$CONFIG_FILE" ]; then
   echo "Keeping existing $CONFIG_FILE (new defaults are in $INSTALL_DIR/ruby_jetson_sender.conf.default)"
   install -m 0644 "$SRC_DIR/ruby_jetson_sender.conf" "$INSTALL_DIR/ruby_jetson_sender.conf.default"
else
   install -m 0644 "$SRC_DIR/ruby_jetson_sender.conf" "$CONFIG_FILE"
   echo "Installed $CONFIG_FILE - review the host/port and camera settings there."
fi

install -m 0644 "$SRC_DIR/ruby_jetson_sender.service" "$SERVICE_FILE"
systemctl daemon-reload
systemctl enable ruby_jetson_sender

echo ""
echo "Done. Start it with:"
echo "   sudo systemctl start ruby_jetson_sender"
echo "   journalctl -u ruby_jetson_sender -f"

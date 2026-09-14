#!/bin/bash
#
# Gives the Jetson a static IP on the direct Ethernet link to the RubyFPV
# vehicle (Raspberry Pi). Run on the Jetson:
#
#    sudo ./setup_static_ip.sh [iface] [addr/prefix]
#
# Defaults: eth0 192.168.10.2/24  (the Pi is expected at 192.168.10.1)
#
# No gateway and no DNS are configured: this link carries video only, and the
# Jetson keeps using wifi/other interfaces for everything else.

set -e

IFACE="${1:-eth0}"
ADDR="${2:-192.168.10.2/24}"

if [ "$(id -u)" != "0" ]; then
   echo "This script must run as root (use sudo)."
   exit 1
fi

if ! ip link show "$IFACE" >/dev/null 2>&1; then
   echo "Interface $IFACE not found. Available:"
   ip -o link show | awk -F': ' '{print "   "$2}'
   exit 1
fi

if command -v nmcli >/dev/null 2>&1 && nmcli -t -f RUNNING general 2>/dev/null | grep -q running; then
   CON="ruby-link"
   echo "Configuring $IFACE via NetworkManager (connection '$CON')..."
   nmcli con delete "$CON" >/dev/null 2>&1 || true
   nmcli con add type ethernet con-name "$CON" ifname "$IFACE" \
      ipv4.method manual ipv4.addresses "$ADDR" ipv4.never-default yes \
      ipv6.method ignore connection.autoconnect yes
   nmcli con up "$CON"
else
   FILE="/etc/network/interfaces.d/ruby-$IFACE"
   echo "Configuring $IFACE via ifupdown ($FILE)..."
   cat > "$FILE" <<EOF
# RubyFPV direct video link - written by setup_static_ip.sh
auto $IFACE
iface $IFACE inet static
    address $ADDR
    mtu 1500
EOF
   ifdown "$IFACE" >/dev/null 2>&1 || true
   ifup "$IFACE"
fi

echo ""
ip -4 addr show "$IFACE" | sed 's/^/   /'
echo ""
echo "Now check the Pi answers:   ping -c 3 ${ADDR%%/*}"
echo "(replace with the Pi address, e.g. ping -c 3 192.168.10.1)"

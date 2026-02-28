#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "Setting up Fit server to start on boot (OpenRC)..."

# Copy OpenRC service
cp "$SCRIPT_DIR/fit-server" /etc/init.d/
chmod +x /etc/init.d/fit-server

# Add to default runlevel
rc-update add fit-server default

# Start service
rc-service fit-server start

echo "✓ Fit server installed and started"
echo "✓ Will auto-start on boot"
echo ""
echo "Web Interface: http://localhost:8080"
echo "Daemon Port: 9418"
echo ""
echo "Useful commands:"
echo "  rc-service fit-server status   - Check status"
echo "  rc-service fit-server stop     - Stop server"
echo "  rc-service fit-server restart  - Restart server"
echo "  docker-compose logs -f         - View logs"

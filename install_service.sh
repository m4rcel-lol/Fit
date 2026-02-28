#!/bin/bash

echo "Setting up Fit server to start on boot..."

# Copy systemd service
sudo cp fit-server.service /etc/systemd/system/

# Reload systemd
sudo systemctl daemon-reload

# Enable service
sudo systemctl enable fit-server.service

# Start service
sudo systemctl start fit-server.service

echo "✓ Fit server installed and started"
echo "✓ Will auto-start on boot"
echo ""
echo "Web Interface: http://localhost:8080"
echo "Daemon Port: 9418"
echo ""
echo "Useful commands:"
echo "  sudo systemctl status fit-server   - Check status"
echo "  sudo systemctl stop fit-server     - Stop server"
echo "  sudo systemctl restart fit-server  - Restart server"
echo "  docker-compose logs -f             - View logs"

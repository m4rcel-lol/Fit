#!/bin/bash
# Enhanced Fit Web Interface Startup Script

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║                                                              ║"
echo "║        Fit Web Interface - Enhanced Edition v1.1.0          ║"
echo "║                                                              ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Check if binaries exist
if [ ! -f "./bin/fitweb-enhanced" ]; then
    echo "Error: fitweb-enhanced binary not found. Building..."
    make web-enhanced
    if [ $? -ne 0 ]; then
        echo "Build failed. Please check for errors."
        exit 1
    fi
fi

echo "Starting enhanced web interface..."
echo ""
echo "Features:"
echo "  ✨ Enhanced commit browser with search"
echo "  📈 Repository statistics dashboard"
echo "  🌿 Branch management interface"
echo "  📄 File viewer with line numbers"
echo "  📱 Improved mobile responsive design"
echo "  🎨 Modern GitHub-style UI"
echo ""
echo "Access at: http://localhost:8080"
echo "Login: m5rcel / M@rc8l1257"
echo ""
echo "Press Ctrl+C to stop"
echo ""

./bin/fitweb-enhanced

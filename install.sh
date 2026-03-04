#!/bin/bash
# Fit VCS - Quick Installation Script
# Supports: Arch Linux, Alpine Linux, Ubuntu/Debian, Fedora

set -e

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║                                                              ║"
echo "║              Fit VCS - Installation Script                  ║"
echo "║                                                              ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Detect OS
if [ -f /etc/arch-release ]; then
    OS="arch"
elif [ -f /etc/alpine-release ]; then
    OS="alpine"
elif [ -f /etc/debian_version ]; then
    OS="debian"
elif [ -f /etc/fedora-release ]; then
    OS="fedora"
else
    echo "⚠️  Warning: Unknown distribution. Attempting generic installation..."
    OS="generic"
fi

echo "📋 Detected OS: $OS"
echo ""

# Install dependencies
echo "📦 Installing dependencies..."
case "$OS" in
    arch)
        sudo pacman -S --needed gcc make zlib openssl sqlite
        ;;
    alpine)
        sudo apk add gcc musl-dev make zlib-dev openssl-dev sqlite-dev
        ;;
    debian)
        sudo apt-get update
        sudo apt-get install -y gcc make zlib1g-dev libssl-dev libsqlite3-dev
        ;;
    fedora)
        sudo dnf install -y gcc make zlib-devel openssl-devel sqlite-devel
        ;;
    generic)
        echo "ℹ️  Please install: gcc, make, zlib-dev, openssl-dev, sqlite-dev"
        read -p "Press Enter to continue after installing dependencies..."
        ;;
esac

echo "✅ Dependencies installed"
echo ""

# Build
echo "🔨 Building Fit..."
make clean
make

if [ $? -ne 0 ]; then
    echo "❌ Build failed. Please check error messages above."
    exit 1
fi

echo "✅ Build successful"
echo ""

# Test
echo "🧪 Running tests..."
make test

if [ $? -ne 0 ]; then
    echo "⚠️  Some tests failed, but installation can continue"
else
    echo "✅ All tests passed"
fi
echo ""

# Install
echo "📥 Installing to /usr/local/bin..."
if [ "$OS" = "alpine" ]; then
    make install
else
    sudo make install
fi

if [ $? -ne 0 ]; then
    echo "❌ Installation failed"
    exit 1
fi

echo "✅ Installation complete!"
echo ""

# Install bash completion (optional)
if [ -d /etc/bash_completion.d ]; then
    echo "📝 Installing bash completion..."
    if [ "$OS" = "alpine" ]; then
        cp fit-completion.bash /etc/bash_completion.d/fit
    else
        sudo cp fit-completion.bash /etc/bash_completion.d/fit
    fi
    echo "✅ Bash completion installed"
    echo "   Run: source ~/.bashrc"
fi

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║                     Installation Complete!                   ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
echo "📚 Quick Start:"
echo "   fit init                    # Initialize repository"
echo "   fit snapshot -m 'Backup'    # Create backup"
echo "   fit log                     # View history"
echo ""
echo "🌐 Web Interface:"
echo "   ./start_web_enhanced.sh     # Start enhanced web UI"
echo "   Open http://localhost:8080"
echo ""
echo "📖 Documentation:"
echo "   fit help                    # Show all commands"
echo "   cat README.md              # Full documentation"
echo "   cat QUICKSTART.md          # Quick reference"
echo ""
echo "🎉 Happy versioning!"

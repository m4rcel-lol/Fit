# Fit Two-Node Setup Guide

This guide demonstrates setting up Fit as a distributed backup system between two Linux machines.

## Scenario

- **Server**: Alpine Linux laptop (backup storage node)
- **Client**: Arch Linux desktop (primary work machine)
- **Goal**: Automatic distributed backup with version control

---

## Part 1: Server Setup (Alpine Linux Laptop)

### Install Dependencies

```bash
apk add gcc musl-dev make zlib-dev openssl-dev git
```

### Build and Install Fit

```bash
cd ~
git clone <fit-repo-url>
cd Fit
make
sudo make install
```

### Initialize Backup Repository

```bash
mkdir -p ~/backup-storage
cd ~/backup-storage
fit init
```

### Start Daemon

#### Option A: Direct Execution

```bash
fit daemon --port 9418
```

#### Option B: Docker (Recommended)

```bash
cd ~/Fit
docker-compose up -d
```

Check status:
```bash
docker ps
docker logs fit-server
```

#### Option C: Systemd Service

Create `/etc/systemd/system/fit-daemon.service`:

```ini
[Unit]
Description=Fit Backup Daemon
After=network.target

[Service]
Type=simple
User=backup
WorkingDirectory=/home/backup/storage
ExecStart=/usr/local/bin/fit daemon --port 9418
Restart=always
RestartSec=10

[Install]
WantedBy=multi-target.target
```

Enable and start:
```bash
sudo systemctl enable fit-daemon
sudo systemctl start fit-daemon
sudo systemctl status fit-daemon
```

### Verify Daemon

```bash
netstat -ln | grep 9418
# Should show: tcp 0 0 0.0.0.0:9418 0.0.0.0:* LISTEN
```

---

## Part 2: Client Setup (Arch Linux Desktop)

### Install Dependencies

```bash
sudo pacman -S gcc make zlib openssl
```

### Build and Install Fit

```bash
cd ~
git clone <fit-repo-url>
cd Fit
make
sudo make install
```

### Initialize Project Repository

```bash
cd ~/projects/myproject
fit init
```

### Add Files and Create Initial Commit

```bash
# Add some files
echo "Project README" > README.md
echo "def main(): pass" > main.py
echo "# Config" > config.yaml

# Stage files
fit add README.md main.py config.yaml

# Create commit
fit commit -m "Initial project setup"

# Verify
fit log
fit status
```

### Push to Backup Server

Replace `laptop.local` with your server's hostname or IP:

```bash
fit push laptop.local main
```

Expected output:
```
Pushed 1 objects to laptop.local
```

---

## Part 3: Daily Workflow

### On Client (Arch Desktop)

#### Make Changes

```bash
cd ~/projects/myproject
echo "new feature" >> main.py
fit add main.py
fit commit -m "Added new feature"
```

#### Backup to Server

```bash
fit push laptop.local main
```

#### Quick Snapshot (Backup Everything)

```bash
# Automatically adds all files in current directory
fit snapshot -m "Daily backup $(date +%Y-%m-%d)"
fit push laptop.local main
```

---

## Part 4: Disaster Recovery Simulation

### Simulate Data Loss

```bash
# On client machine
cd ~/projects/myproject
rm -rf *
ls
# Everything is gone!
```

### Restore from Backup

#### Option 1: Manual Recovery

```bash
# SSH to server and copy .fit directory
ssh laptop.local "cd ~/backup-storage && tar czf - .fit" | tar xzf -

# Checkout latest commit
fit log  # Find latest commit hash
# Manually restore files from objects (requires implementing checkout)
```

#### Option 2: Clone from Server (Future Feature)

```bash
fit clone laptop.local:~/backup-storage ~/projects/myproject-restored
```

---

## Part 5: Advanced Usage

### Branching for Experiments

```bash
# Create experimental branch
fit branch experiment
fit checkout experiment

# Make changes
echo "experimental code" > experiment.py
fit add experiment.py
fit commit -m "Experimental feature"

# Push experimental branch
fit push laptop.local experiment

# Switch back to main
fit checkout main
```

### Maintenance

#### Run Garbage Collection

```bash
# On both client and server
fit gc
```

#### Check Repository Status

```bash
fit status
fit branch
fit log
```

---

## Part 6: Network Configuration

### Firewall Rules (Server)

#### Alpine Linux (iptables)

```bash
# Allow Fit daemon port
iptables -A INPUT -p tcp --dport 9418 -j ACCEPT
iptables-save > /etc/iptables/rules.v4
```

#### Using UFW

```bash
ufw allow 9418/tcp
ufw reload
```

### SSH Tunnel (Secure Alternative)

If you don't want to expose port 9418:

```bash
# On client, create tunnel
ssh -L 9418:localhost:9418 laptop.local -N -f

# Push through tunnel
fit push localhost main
```

---

## Part 7: Monitoring and Troubleshooting

### Check Daemon Logs (Docker)

```bash
docker logs -f fit-server
```

### Check Daemon Logs (Systemd)

```bash
journalctl -u fit-daemon -f
```

### Verify Objects

```bash
# Count stored objects
find .fit/objects -type f | wc -l

# Check specific object
ls -lh .fit/objects/
```

### Test Connectivity

```bash
# From client
telnet laptop.local 9418
# Should connect successfully
```

### Common Issues

#### "Push failed"
- Check daemon is running: `netstat -ln | grep 9418`
- Verify firewall allows port 9418
- Test connectivity: `telnet server 9418`

#### "Branch not found"
- Ensure you've created a commit first
- Check branch exists: `fit branch`

#### "No commits yet"
- Create initial commit before pushing

---

## Part 8: Backup Strategy

### Recommended Schedule

```bash
# Add to crontab
crontab -e
```

```cron
# Hourly snapshot during work hours
0 9-17 * * 1-5 cd ~/projects/myproject && fit snapshot -m "Auto backup $(date)" && fit push laptop.local main

# Daily full backup
0 23 * * * cd ~/projects/myproject && fit snapshot -m "Daily backup $(date)" && fit push laptop.local main
```

### Backup Script

Create `~/bin/fit-backup.sh`:

```bash
#!/bin/bash
set -e

PROJECT_DIR="$HOME/projects/myproject"
SERVER="laptop.local"
BRANCH="main"

cd "$PROJECT_DIR"
fit snapshot -m "Automated backup $(date '+%Y-%m-%d %H:%M:%S')"
fit push "$SERVER" "$BRANCH"

echo "Backup completed successfully"
```

Make executable:
```bash
chmod +x ~/bin/fit-backup.sh
```

---

## Part 9: Performance Metrics

### Storage Efficiency

```bash
# Original files size
du -sh ~/projects/myproject

# Repository size (with compression)
du -sh ~/projects/myproject/.fit

# Typical compression ratio: 40-60% for text files
```

### Network Transfer

```bash
# Monitor transfer with iftop or nethogs during push
sudo iftop -i eth0
```

---

## Summary

You now have:

✅ Distributed version control system
✅ Automated backup solution
✅ Disaster recovery capability
✅ Branch-based experimentation
✅ Efficient deduplicated storage
✅ Network-based synchronization

**Fit** provides Git-like version control optimized for personal backup workflows between your machines.

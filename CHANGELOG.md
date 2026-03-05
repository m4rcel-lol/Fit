# Changelog

All notable changes and improvements to the Fit project.

## [2.0.0] - 2026-03-05

### Major Changes

**BREAKING CHANGE:** Web interface completely removed - Fit is now CLI-only for improved focus and simplicity

### Added

#### New Commands
- **`fit tag`** - Full tag management
  - Create tags: `fit tag v1.0.0` or `fit tag create v1.0.0 -m "Release"`
  - List tags: `fit tag`
  - Delete tags: `fit tag delete v1.0.0`
  - Tag resolution for referencing commits

- **`fit remote`** - Remote repository management
  - Add remotes: `fit remote add origin server.local`
  - List remotes: `fit remote` or `fit remote list`
  - Remove remotes: `fit remote remove origin`
  - Remote URL storage in `.fit/config`

- **`fit stash`** - Save and restore uncommitted changes
  - Save: `fit stash` or `fit stash save -m "WIP"`
  - List: `fit stash list`
  - Apply: `fit stash pop`
  - Remove: `fit stash drop <stash-name>`
  - Timestamp-based stash management

- **`fit merge`** - Branch merging
  - Fast-forward merge: `fit merge feature-branch`
  - Automatic conflict detection
  - Already up-to-date detection
  - Tree checkout on successful merge

#### Improvements
- **Enhanced `fit status`** command
  - Shows HEAD commit with hash and message
  - Color-coded output (green for modified files)
  - File hashes displayed for staged changes
  - Helpful hints about what to do next
  - Better visual hierarchy

- **Better network error handling**
  - Detailed error messages with `perror()`
  - Connection status logging
  - Data transfer progress (bytes sent/received)
  - Failed operation explanations
  - Client connection notifications
  - Protocol version validation logging

- **Improved logging**
  - Object transfer progress
  - Branch update confirmations
  - Pack file operations status
  - Connection establishment messages

### Removed
- ❌ Web interface (both standard and enhanced editions)
- ❌ `make web` and `make web-enhanced` targets
- ❌ `fitweb` and `fitweb-enhanced` binaries
- ❌ SQLite dependency (was only for web authentication)
- ❌ Web startup scripts (`start_web.sh`, `start_web_enhanced.sh`)
- ❌ Port 8080 exposure in Docker
- ❌ Web-related documentation sections

### Changed
- Docker now only runs daemon (no web interface)
- Simplified installation (no SQLite dependency)
- Updated all documentation to remove web references
- Dockerfile optimized for daemon-only operation
- docker-compose.yml simplified to single service
- Makefile streamlined without web targets

### Fixed
- Network operations now check return values properly
- Better socket error handling and cleanup
- Pack file creation errors now reported to user
- File operations validated before use
- Improved error propagation in network stack

---

## [1.1.0] - 2024-12-XX

### Added - Core Features

#### New `diff` Command
- **Feature**: Compare differences between commits
- **Usage**: `fit diff <commit1> <commit2>` or `fit diff <commit>`
- **Benefits**:
  - View what changed between versions
  - Debug issues by comparing working states
  - Understand project evolution
- **Implementation**: Line-by-line diff for text files, tree comparison for directories

### Added - Enhanced Web Interface (REMOVED in v2.0.0)

#### New Enhanced Web Edition (`fitweb-enhanced`)
A significantly improved web interface with modern features:

**Navigation & UI**
- ✨ **Commit Search**: Real-time search through commit messages and authors
- 📊 **Enhanced Header**: Sticky navigation with quick access to all sections
- 📱 **Mobile Responsive**: Optimized for phones and tablets
- 🎨 **Improved Visual Design**: Better spacing, colors, and animations

**New Pages**
- 📈 **Statistics Dashboard**
- 🌿 **Branch Manager**
- 📄 **File Viewer**

**NOTE:** All web interface features were removed in v2.0.0 to focus on CLI experience

### Added - Developer Experience

#### Bash Completion Script
- Command completion
- Branch name completion
- Commit hash completion
- Context-aware suggestions
- **Usage**: `source fit-completion.bash`

#### Automated Installer (`install.sh`)
- Detects OS automatically (Arch, Alpine, Debian, Ubuntu, Fedora)
- Installs all dependencies
- Builds the project
- Runs tests
- Installs to `/usr/local/bin`
- Optional bash completion installation

### Technical Improvements

**Build System**
- Multi-target Makefile
- Better dependency management
- Cleaner output

**Code Quality**
- Modular diff implementation
- Better function organization
- Improved comments
- `.gitignore` for build artifacts

---

## [1.0.0] - Initial Release

### Core Features
- Content-addressable storage using SHA-256
- Object model: blobs, trees, commits
- zlib compression
- Custom TCP network protocol
- Distributed backup between machines
- Branch management
- Garbage collection

### Commands
- `fit init` - Initialize repository
- `fit add` - Stage files
- `fit commit` - Create commits
- `fit log` - View history
- `fit status` - Check status
- `fit branch` - Branch management
- `fit checkout` - Switch branches
- `fit push` - Push to remote
- `fit pull` - Pull from remote
- `fit clone` - Clone repository
- `fit restore` - Restore files
- `fit daemon` - Start server
- `fit gc` - Garbage collection
- `fit snapshot` - Quick backup

### Architecture
- SHA-256 hashing (vs Git's SHA-1)
- Simplified protocol
- Backup-first design
- Minimal dependencies (zlib, OpenSSL)

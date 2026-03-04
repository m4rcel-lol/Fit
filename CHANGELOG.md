# Changelog

All notable changes and improvements to the Fit project.

## [1.1.0] - 2026-03-04

### Added - Core Features

#### New `diff` Command
- **Feature**: Compare differences between commits
- **Usage**: `fit diff <commit1> <commit2>` or `fit diff <commit>`
- **Benefits**:
  - View what changed between versions
  - Debug issues by comparing working states
  - Understand project evolution
- **Implementation**: Line-by-line diff for text files, tree comparison for directories

### Added - Enhanced Web Interface

#### New Enhanced Web Edition (`fitweb-enhanced`)
A significantly improved web interface with modern features:

**Navigation & UI**
- ✨ **Commit Search**: Real-time search through commit messages and authors
- 📊 **Enhanced Header**: Sticky navigation with quick access to all sections
- 📱 **Mobile Responsive**: Optimized for phones and tablets
- 🎨 **Improved Visual Design**: Better spacing, colors, and animations

**New Pages**
- 📈 **Statistics Dashboard**:
  - Total commits count
  - Object count in repository
  - Branch count
  - Repository size on disk
  - Visual stat cards with icons

- 🌿 **Branch Manager**:
  - List all branches
  - Highlight current branch
  - Clean, organized display

- 📄 **File Viewer**:
  - Syntax-aware file display
  - Line numbers for easy reference
  - Download individual files
  - Better code readability

**Improvements**
- Better breadcrumb navigation
- File type icons (C, Python, JS, Markdown, etc.)
- Smoother hover effects and transitions
- Improved empty states
- Better error handling
- URL decoding for special characters
- HTML escaping for security

**How to Use**
```bash
# Build enhanced edition
make web-enhanced

# Or use startup script
./start_web_enhanced.sh
```

### Improved - Build System

#### Makefile Enhancements
- Support for multiple web interface versions
- Separate build targets:
  - `make web` - Standard interface
  - `make web-enhanced` - Enhanced interface
  - `make all` - Builds everything
- Better source file filtering
- Cleaner build output

### Improved - Documentation

#### README Updates
- Documented new `diff` command with examples
- Added Enhanced Web Interface section
- Feature comparison between standard and enhanced web UI
- Clear usage examples
- Better organization

#### New Files
- `start_web_enhanced.sh` - Convenient startup script for enhanced web
- `CHANGELOG.md` - This file, tracking all improvements
- `.gitignore` - Proper exclusion of build artifacts

### Technical Improvements

#### Code Organization
- New `src/diff.c` - Modular diff implementation
- New `src/web_enhanced.c` - Enhanced web interface
- Updated `include/fit.h` - Added diff function declarations
- Updated `src/main.c` - Integrated diff command

#### Code Quality
- Added proper gitignore for build artifacts
- Separated web interface versions for maintainability
- Better code comments
- Improved function organization

### Build Artifacts Now Excluded
- `/bin/` directory
- `/obj/` directory
- `*.o` object files
- `fitweb.db` database
- Test artifacts

## [1.0.0] - 2026-02-28

### Initial Release
- Content-addressable storage with SHA-256
- Object model: blobs, trees, commits
- zlib compression
- Index/staging area
- Branches and HEAD management
- Commit history traversal
- Network protocol for push/pull
- Garbage collection
- Web interface (standard edition)
- Docker deployment support
- Complete test suite

---

## Future Roadmap

### Planned Features
- [ ] `merge` command for branch merging
- [ ] `tag` command for release management
- [ ] Configuration file support (`.fitconfig`)
- [ ] Improved diff with context lines
- [ ] Syntax highlighting in web interface
- [ ] User management in web interface
- [ ] Dark/light theme toggle
- [ ] Web-based commit viewing
- [ ] API endpoints for programmatic access
- [ ] Performance optimizations
- [ ] Security enhancements (CSRF protection, rate limiting)

### Long-term Goals
- Delta compression
- End-to-end encryption
- GPG commit signing
- Smart protocol negotiation
- Shallow clones
- Submodule support

---

**Note**: Version numbers follow Semantic Versioning (SemVer): MAJOR.MINOR.PATCH
- MAJOR: Incompatible API changes
- MINOR: New features (backward compatible)
- PATCH: Bug fixes (backward compatible)

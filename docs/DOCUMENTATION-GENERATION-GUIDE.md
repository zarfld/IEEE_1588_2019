# Documentation Generation Guide

**Phase**: 08-Transition (Documentation)  
**Standards**: ISO/IEC/IEEE 12207:2017 (Transition Process)  
**Version**: 1.0.0-MVP

---

## Overview

This directory contains tools for generating comprehensive API documentation from the IEEE 1588-2019 PTP Library source code using Doxygen.

### Documentation Types

1. **API Reference** (Doxygen-generated)
   - Automatically extracted from source code comments
   - Class hierarchies and call graphs
   - Interactive HTML with search
   - XML output for integration with other tools

2. **User Guides** (Manual)
   - `08-transition/user-documentation/api-reference.md` - High-level API guide
   - `08-transition/user-documentation/integration-guide.md` - Integration walkthrough
   - `08-transition/training-materials/getting-started.md` - Quick start tutorial

3. **Technical Documentation** (Markdown)
   - Architecture specifications in `03-architecture/`
   - Design documents in `04-design/`
   - Requirements in `02-requirements/`

---

## Prerequisites

### Required Tools

**Doxygen** (version 1.9.0 or later)

- **Windows**: Download installer from https://www.doxygen.nl/download.html
- **Linux**: `sudo apt-get install doxygen`
- **macOS**: `brew install doxygen`

**Graphviz** (recommended for diagrams)

- **Windows**: Download from https://graphviz.org/download/
- **Linux**: `sudo apt-get install graphviz`
- **macOS**: `brew install graphviz`

**Python 3.7+** (for generation script)

- Required for `scripts/generate-doxygen.py`

### Verify Installation

```bash
# Check Doxygen
doxygen --version

# Check Graphviz
dot -V

# Check Python
python --version
```

---

## Quick Start

### Generate Documentation (Python Script)

```bash
# From repository root
python scripts/generate-doxygen.py

# Clean build
python scripts/generate-doxygen.py --clean

# With coverage report
python scripts/generate-doxygen.py --coverage
```

### Generate Documentation (CMake)

```bash
# Configure with docs enabled
cmake -B build -DIEEE1588_BUILD_DOCS=ON

# Generate documentation
cmake --build build --target docs

# View documentation
# Windows:
start build/docs/doxygen/html/index.html

# Linux:
xdg-open build/docs/doxygen/html/index.html

# macOS:
open build/docs/doxygen/html/index.html
```

### Generate Documentation (Manual)

```bash
# From repository root
doxygen Doxyfile

# Output will be in docs/doxygen/html/
```

---

## Documentation Structure

### Generated Output

```
docs/doxygen/
├── html/                      # HTML documentation
│   ├── index.html            # Main entry point
│   ├── annotated.html        # Class list
│   ├── classes.html          # Class index
│   ├── files.html            # File list
│   ├── namespaces.html       # Namespace list
│   ├── functions.html        # Function index
│   └── ...
├── xml/                       # XML output (for integration)
│   ├── index.xml
│   └── *.xml
├── warnings.log              # Doxygen warnings
└── IEEE1588_2019_PTP.tag     # Tag file for external refs
```

### Source Code Documentation

**Header files** (`include/`)
- Doxygen comments for all public APIs
- `@brief`, `@details`, `@param`, `@return` tags
- `@note` for important implementation details
- `@req` for requirements traceability
- `@ieee` for IEEE 1588-2019 section references

**Example**:
```cpp
/**
 * @file clocks.hpp
 * @brief IEEE 1588-2019 PTP Clock State Machines
 * @details Implements Ordinary Clock, Boundary Clock, and Transparent Clock
 *          state machines per IEEE 1588-2019 Sections 9 and 10.
 * 
 * @note Deterministic design: No dynamic allocation, no blocking calls
 * @ieee IEEE 1588-2019, Section 9.2 "PTP port state machines"
 * @req REQ-F-202 Deterministic BMCA
 */

/**
 * @brief PTP Port State Machine
 * @details Implements port state machine per IEEE 1588-2019 Section 9.2
 * 
 * State transitions:
 * - INITIALIZING → LISTENING (on POWERUP)
 * - LISTENING → MASTER/SLAVE/PASSIVE (on BMCA decision)
 * - MASTER → PASSIVE (on better master discovered)
 * - SLAVE → UNCALIBRATED → SLAVE (during synchronization)
 * 
 * @ieee IEEE 1588-2019, Section 9.2.6 "State decision algorithm"
 * @req REQ-F-001 Port state machine implementation
 */
class PtpPort {
    /**
     * @brief Process Announce message
     * @param msg Announce message to process
     * @param rx_timestamp Reception timestamp
     * @return PTPResult<void> Success or error
     * 
     * @details Updates foreign master list and triggers BMCA if needed.
     *          Non-blocking operation with bounded execution time.
     * 
     * @note Call from network receive thread with captured timestamp
     * @ieee IEEE 1588-2019, Section 9.5.2 "Announce message processing"
     */
    PTPResult<void> process_announce(
        const AnnounceMessage& msg,
        const Timestamp& rx_timestamp
    );
};
```

---

## Customizing Documentation

### Edit Doxyfile

Key configuration options in `Doxyfile`:

```
PROJECT_NAME           = "IEEE 1588-2019 PTP Library"
PROJECT_NUMBER         = 1.0.0-MVP
PROJECT_BRIEF          = "Hardware-agnostic PTPv2 implementation"
OUTPUT_DIRECTORY       = docs/doxygen

# What to extract
EXTRACT_ALL            = YES        # Document everything
EXTRACT_PRIVATE        = NO         # Skip private members
EXTRACT_STATIC         = YES        # Include static members

# Warnings
WARN_IF_UNDOCUMENTED   = YES        # Warn about missing docs
WARN_IF_DOC_ERROR      = YES        # Warn about doc errors

# Input sources
INPUT                  = include/ src/ README.md
RECURSIVE              = YES
FILE_PATTERNS          = *.cpp *.hpp *.h *.md

# Output formats
GENERATE_HTML          = YES
GENERATE_XML           = YES        # For external tools
GENERATE_LATEX         = NO

# Diagrams
HAVE_DOT               = YES        # Enable Graphviz
CLASS_GRAPH            = YES
COLLABORATION_GRAPH    = YES
CALL_GRAPH             = YES
CALLER_GRAPH           = YES
```

### Custom Aliases

Use custom Doxygen aliases for standards compliance:

```
@req REQ-F-001         # Link to requirement
@test TEST-UNIT-001    # Link to test case
@ieee Section 9.2      # Reference IEEE specification
@deterministic         # Highlight deterministic design
@realtime              # Highlight real-time constraints
```

Example usage:
```cpp
/**
 * @brief Adjust clock frequency
 * @param ppb_adjustment Frequency adjustment in parts per billion
 * @return PTPError Success or error code
 * 
 * @deterministic Bounded execution time: O(1)
 * @realtime Non-blocking operation
 * @req REQ-NF-007 Clock adjustment accuracy ±1 ppb
 * @ieee IEEE 1588-2019, Section 11.6 "Clock adjustment"
 */
PTPError adjust_frequency(double ppb_adjustment);
```

---

## Documentation Coverage

### Check Coverage

```bash
# Generate coverage report
python scripts/generate-doxygen.py --coverage
```

**Output example**:
```
============================================================
Documentation Coverage Report
============================================================
Classes: 45/50 (90.0%)
Functions: 234/250 (93.6%)
Variables: 89/100 (89.0%)
Enums: 12/15 (80.0%)
------------------------------------------------------------
Overall: 380/415 (91.6%)
============================================================
```

### Improving Coverage

1. **Add missing @brief tags**: Every public API should have brief description
2. **Document parameters**: Use `@param` for all function parameters
3. **Document return values**: Use `@return` for all non-void functions
4. **Add details**: Use `@details` for complex implementations
5. **Link to standards**: Use `@ieee` to reference specification sections

---

## Integration with CI/CD

### GitHub Actions Example

```yaml
name: Documentation

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]

jobs:
  docs:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Install Doxygen
      run: |
        sudo apt-get update
        sudo apt-get install -y doxygen graphviz
    
    - name: Generate Documentation
      run: python scripts/generate-doxygen.py --coverage
    
    - name: Check Warnings
      run: |
        if [ -s docs/doxygen/warnings.log ]; then
          echo "Documentation warnings found:"
          cat docs/doxygen/warnings.log
          exit 1
        fi
    
    - name: Upload Documentation
      uses: actions/upload-artifact@v3
      with:
        name: api-docs
        path: docs/doxygen/html/
    
    - name: Deploy to GitHub Pages (main branch only)
      if: github.ref == 'refs/heads/main'
      uses: peaceiris/actions-gh-pages@v3
      with:
        github_token: ${{ secrets.GITHUB_TOKEN }}
        publish_dir: docs/doxygen/html
```

---

## Troubleshooting

### Problem: "Doxygen not found"

**Solution**:
1. Install Doxygen (see Prerequisites)
2. Add Doxygen to PATH
3. Use absolute path: `DOXYGEN_EXECUTABLE=/path/to/doxygen`

### Problem: "No diagrams generated"

**Solution**:
1. Install Graphviz (see Prerequisites)
2. Verify `dot` is in PATH: `dot -V`
3. Check Doxyfile: `HAVE_DOT = YES`

### Problem: "Lots of warnings"

**Solution**:
1. Check `docs/doxygen/warnings.log`
2. Add missing documentation comments
3. Fix documentation syntax errors
4. Use `@brief` for all public APIs

### Problem: "Missing files in documentation"

**Solution**:
1. Check `INPUT` paths in Doxyfile
2. Verify `FILE_PATTERNS` includes your file extensions
3. Check `EXCLUDE_PATTERNS` doesn't exclude your files
4. Use `RECURSIVE = YES` for subdirectories

### Problem: "HTML output is empty"

**Solution**:
1. Check Doxygen output for errors
2. Verify `GENERATE_HTML = YES`
3. Verify input files are found
4. Check file permissions on output directory

---

## Publishing Documentation

### Option 1: GitHub Pages

```bash
# Build documentation
python scripts/generate-doxygen.py

# Deploy to gh-pages branch
cd docs/doxygen/html
git init
git add .
git commit -m "Update documentation"
git branch -M gh-pages
git remote add origin https://github.com/[org]/IEEE_1588_2019.git
git push -f origin gh-pages
```

### Option 2: Read the Docs

Create `docs/conf.py` for Sphinx integration:

```python
# Sphinx + Breathe configuration
project = 'IEEE 1588-2019 PTP Library'
extensions = ['breathe']
breathe_projects = {"IEEE1588_2019": "../doxygen/xml"}
breathe_default_project = "IEEE1588_2019"
```

### Option 3: Self-Hosted

```bash
# Copy to web server
scp -r docs/doxygen/html/* user@server:/var/www/docs/
```

---

## Documentation Maintenance

### Regular Tasks

1. **Weekly**: Generate docs and check for warnings
2. **Before release**: Full documentation review
3. **After API changes**: Update affected docs
4. **Quarterly**: Review documentation coverage

### Quality Checklist

- [ ] All public APIs have `@brief` description
- [ ] All parameters documented with `@param`
- [ ] All return values documented with `@return`
- [ ] Complex functions have `@details` explanation
- [ ] IEEE references added with `@ieee` tag
- [ ] Requirements traced with `@req` tag
- [ ] Code examples provided where helpful
- [ ] Diagrams generated correctly
- [ ] No Doxygen warnings
- [ ] Documentation coverage > 90%

---

## Additional Resources

### Doxygen Documentation

- Official Manual: https://www.doxygen.nl/manual/
- Commands Reference: https://www.doxygen.nl/manual/commands.html
- FAQ: https://www.doxygen.nl/manual/faq.html

### Best Practices

- **Be concise**: Brief descriptions should be one sentence
- **Be specific**: Use precise technical terms
- **Be helpful**: Add examples for complex APIs
- **Be consistent**: Use same style throughout
- **Link everything**: Use `@ref`, `@see` for cross-references

### Standards References

- ISO/IEC/IEEE 12207:2017 (Documentation requirements)
- IEEE 1588-2019 (PTP specification)
- Doxygen Style Guide: https://www.doxygen.nl/manual/docblocks.html

---

**For questions or issues with documentation generation, see:**
- Project README: `README.md`
- Contributing Guide: `CONTRIBUTING.md`
- Support: Open issue on GitHub

---

**Document Version**: 1.0.0  
**Last Updated**: 2025-11-11  
**Maintainer**: Development Team

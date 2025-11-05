# CLAUDE.md - Qurcuma Development Guide

## Overview

**Qurcuma** is a Qt-based molecular visualization and analysis GUI for computational chemistry file formats (VTF, XYZ, etc.).

**Purpose**: Interactive exploration of molecular structures and trajectories with 3D visualization support.

## Module Documentation

- **[src/CLAUDE.md](src/CLAUDE.md)** - VTF/XYZ parsers, 3D viewer, mouse interactions
- **[src/dialogs/CLAUDE.md](src/dialogs/CLAUDE.md)** - NMR spectrum dialog and analysis

## Improvements Tracking

See **[AIChangelog.md](AIChangelog.md)** for significant improvements by date.

## General Instructions

- Each source code dir has a CLAUDE.md with basic information of the code and logic
- **Keep CLAUDE.md files FOCUSED and CONCISE** - ONE clear idea per bullet, max 1-2 lines
  - ❌ DON'T: Multi-paragraph explanations, code examples, historical details
  - ✅ DO: Brief statements with links to detailed docs if needed
  - ✅ DO: "✅ **Feature name** - Brief description" for completed items
- Remove completed/resolved items after 2-3 updates (move to git history)
- Tasks corresponding to code must be placed in the correct CLAUDE.md file
- Each CLAUDE.md has a variable part (short-term info, bugs) and preserved part (permanent knowledge)
- **Instructions blocks** contain operator-defined future tasks and visions for code development
- Only include information important for ALL subdirectories in main CLAUDE.md
- Preserve new knowledge from conversations but keep it brief
- Always suggest improvements to existing code
- **Keep entries concise and focused to save tokens**
- **Keep git commits concise and focused**
- **Rule of thumb**: If a CLAUDE.md section exceeds 20 lines, consider if it's better placed elsewhere
## Workflow States
- **ADD**: Features to be added
- **WIP**: Currently being worked on
- **ADDED**: Basically implemented
- **TESTED**: Works (by operator feedback)
- **APPROVED**: Move to changelog, remove from CLAUDE.md

### Documentation Update Rules
- **Replace debugging details with architecture decisions** when issues are resolved
- **Remove unnecessary pointer addresses and crash investigation specifics**
- **Focus on architectural clarity** rather than technical debugging information
- **Document the "why" behind design decisions** for future reference
- **Eliminate redundant information** that doesn't add architectural value
- **Prioritize clean, maintainable documentation** over verbose troubleshooting history

## Development Guidelines

### Code Organization
- Each `src/` subdirectory contains detailed CLAUDE.md documentation
- Variable sections updated regularly with short-term information
- Preserved sections contain permanent knowledge and patterns
- Instructions blocks contain operator-defined future tasks and visions

### Implementation Standards

#### Educational-First Design Principles
- **Core functionality visibility**: Always provide clear, direct access to the computational chemistry implementation
- **Minimal abstraction layers**: Avoid unnecessary templates, inheritance hierarchies, or design patterns that obscure the scientific content
- **Algorithm transparency**: The actual mathematical/physical implementation should be easily locatable and readable
- **Documentation focus**: Emphasize *what* the code does scientifically, not just *how* it's structured
- **Learning-oriented comments**: Include references to equations, papers, and theoretical background in code comments
- **Method implementation clarity**: Each computational method should have a clear entry point with minimal indirection

#### Code Organization for Learning
- **Flat over hierarchical**: Prefer simple, direct implementations over complex class hierarchies
- **Self-contained modules**: Each computational method should be understandable without deep knowledge of the entire system
- **Clear naming**: Function and variable names should reflect their scientific meaning
- **Minimal templates**: Only use templates when absolutely necessary; prefer explicit types for clarity
- **Direct implementations**: Avoid hiding core algorithms behind layers of abstractions

#### Standard Development Practices
- Mark new functions as "Claude Generated" for traceability
- Document new functions briefly (doxygen ready) with scientific context
- Document existing undocumented functions if appearing regularly (briefly and doxygen ready)
- Remove TODO Hashtags and text if done and approved
- Use modern Qt6 patterns, avoid deprecated functions
- Implement comprehensive error handling and logging
- Debug output: `qDebug()` within `#ifdef DEBUG_ON #endif`
- Console output: Use `fmt`, avoid `std::cout`
- Maintain backward compatibility where possible
- **Always check and consider instructions blocks** in relevant CLAUDE.md files before implementing
- Reformulate and clarify task and vision entries if not already marked as CLAUDE formatted
- In case of compiler warning for deprecated functions, replace the old function call with the new one
- Implement timing analysis for complex functions
- Keep track of significant improvements in AIChangelog.md, one line per fact
- **Complex Architecture Documentation**: Factory patterns, dispatchers, and multi-step workflows require comprehensive inline documentation
- Maintain backward compatibility where possible

### Copyright and File Headers
- **Copyright ownership**: All copyright remains with Conrad Hübler as the project owner
- **Year updates**: Always update copyright year to current year when modifying files
- **Claude contributions**: Mark Claude-generated code sections but copyright stays with Conrad
- **Format**: `Copyright (C) 2015 - 2025 Conrad Hübler <Conrad.Huebler@gmx.net>`
- **AI acknowledgment**: Add Claude contribution notes in code comments, not copyright headers

### Important Notes
- nullptr-checks are good things, however if there is a crash, prevent the crash by checks doesn't solve the origin of the nullptr

### Build & Git Management

#### Build Directories
- **`debug/`** - Development build: full debug symbols, no optimizations, slower runtime
  - Use: Testing features, debugging crashes, development workflow
  - Build: `cmake --build debug 2>&1 | tail -20` for quick status
  - Executable: `./debug/qurcuma`

- **`release/`** - Optimized build: stripped symbols, O3 optimizations, fast runtime
  - Use: Performance testing, final deployment, production runs
  - Build: `cmake --build release 2>&1 | tail -20`
  - Executable: `./release/qurcuma`

#### Git Best Practices
- **Only commit source files**: Use `git add <file>` for specific files, never `git add -A` without review
- **Review before committing**: Always check `git diff` and `git status` to avoid accidental commits
- **Build before commit**: Ensure `cmake --build debug` succeeds and no compiler warnings/errors exist
- **Commit message format**: Start with action verb (Fix, Add, Improve, Refactor), follow with brief description
- **Include Co-Author info**: All commits include Claude contribution notes with proper attribution
- **Test artifacts stay local**: Build artifacts (test_vtf*, test_frame_detection, etc.) are ignored
- **.gitignore is comprehensive**: CMake files, build directories, test artifacts, OS files are all ignored


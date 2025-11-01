# AIChangelog - Qurcuma Improvements

## November 2025

### Directory Navigation UX (Phases 1-2 Complete)
- Added BreadcrumbBar widget: Clickable path segments replacing plain text label; users can jump to parent dirs by clicking breadcrumb items; Home shown as ~
- Enhanced Recent Files: Added RecentFileEntry struct with QDateTime timestamps; menu now groups by date (Today/Yesterday/This Week/Older); displays full context (filename with parent directory)
- Settings migration: Automatic migration from old QStringList to timestamped format; backward compatible with existing bookmarks

## October 2025

- Fixed VTF bond parsing: Changed QMap<int, VTFBond> to QVector<VTFBond> to support multiple bonds per atom (previously lost ~50% of bonds due to key collision)
- Fixed VTF frame parsing: Removed global parseError flag from main loop; now correctly processes all frame sections (was stopping after first conversion error, returning 0 frames instead of 3)
- Added test infrastructure: test_vtf_bonds.cpp (validates 199 bonds), test_vtf_frames.cpp (detects 3 frames), test_vtf_full.cpp (end-to-end validation)

#!/usr/bin/env python3
# Claude Generated 2026 - UX restructure verification (docs/UX-settings-audit.md).
"""What the UI can reach, before vs after (UX restructure audit).
For each revision: viewer methods called from UI code, SimulationConfig fields set in
buildConfig, nci::Options fields written by UI code, and MainWindow slots connected.
Prints the names present BEFORE but missing AFTER (candidates for lost settings) and
the ones new AFTER."""
import re, subprocess, sys
def files(rev, prefix='src'):
    out = subprocess.check_output(['git', 'ls-tree', '-r', '--name-only', rev, prefix], text=True)
    return [f for f in out.split() if f.endswith('.cpp')]
def show(rev, f):
    return subprocess.check_output(['git', 'show', f'{rev}:{f}'], text=True, errors='replace')
VIEWER = re.compile(r'\b(?:m_viewer|m_moleculeView|viewer|m_view)\s*->\s*(\w+)\s*\(')
CFG = re.compile(r'\bcfg\.(\w+)\s*=(?!=)')
NCIOPT = re.compile(r'\b(?:o|opts|options|m_options)\.(\w+)\s*=(?!=)')
SLOT = re.compile(r'&MainWindow::(\w+)')
def collect(rev):
    viewer, cfg, nci, slots = set(), set(), set(), set()
    for f in files(rev):
        t = show(rev, f)
        if f != 'src/view.cpp':
            viewer |= set(VIEWER.findall(t))
        if f.endswith('simulationcontrolwidget.cpp'):
            m = re.search(r'SimulationConfig SimulationControlWidget::buildConfig\(\) const\n\{(.*?)\n\}\n', t, re.S)
            if m: cfg |= set(CFG.findall(m.group(1)))
        if f.endswith(('displaypanel.cpp', 'nciwidget.cpp', 'mainwindow.cpp')):
            nci |= set(NCIOPT.findall(t))
        if f.endswith('mainwindow.cpp'):
            slots |= set(SLOT.findall(t))
    return {'viewer': viewer, 'cfg': cfg, 'nci': nci, 'slots': slots}
a, b = collect(sys.argv[1]), collect(sys.argv[2])
for k in a:
    lost, new = sorted(a[k] - b[k]), sorted(b[k] - a[k])
    print(f'== {k}: before {len(a[k])}, after {len(b[k])}')
    print('   missing after:', ', '.join(lost) if lost else '-')
    print('   new after:    ', ', '.join(new) if new else '-')

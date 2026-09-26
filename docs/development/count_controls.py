#!/usr/bin/env python3
# Claude Generated 2026 - UX restructure verification (docs/WP-ux-restructure.md).
"""Count control constructions per file (UX restructure verification).
Method (inventory 2026-09-25, 'grob'): every `new <Control>(` for buttons, checkboxes,
spin boxes, combo boxes and sliders, per source file. A control built in a loop
counts once. Usage: count_controls.py <git-rev> [<git-rev> ...]"""
import re, subprocess, sys, collections
CONTROLS = r'(QPushButton|QToolButton|QCheckBox|QRadioButton|QSpinBox|QDoubleSpinBox|QComboBox|QSlider|TemperatureSlider)'
pat = re.compile(r'\bnew\s+' + CONTROLS + r'\b')
def files(rev):
    out = subprocess.check_output(['git', 'ls-tree', '-r', '--name-only', rev, 'src'], text=True)
    return [f for f in out.split() if f.endswith('.cpp')]
def count(rev):
    res = {}
    for f in files(rev):
        text = subprocess.check_output(['git', 'show', f'{rev}:{f}'], text=True, errors='replace')
        n = len(pat.findall(text))
        if n: res[f] = n
    return res
revs = sys.argv[1:]
data = {r: count(r) for r in revs}
allf = sorted(set().union(*[d.keys() for d in data.values()]))
print('file\t' + '\t'.join(revs))
for f in allf:
    print(f + '\t' + '\t'.join(str(data[r].get(f, 0)) for r in revs))
print('TOTAL\t' + '\t'.join(str(sum(data[r].values())) for r in revs))

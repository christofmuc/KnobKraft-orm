#!/usr/bin/env python3
"""Build the complete static website locally. Never deploys or contacts GitHub."""
from pathlib import Path
import json
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    public = ROOT / 'public'
    # This script owns only this exact generated output directory.
    if public.is_symlink() or public.resolve() != ROOT.resolve() / 'public':
        raise SystemExit('Refusing to clean redirected output directory')
    subprocess.run([sys.executable, str(ROOT / 'scripts/generate_supported_synths.py'), '--check'], check=True, cwd=ROOT)
    if public.exists():
        shutil.rmtree(public)
    subprocess.run([sys.executable, '-m', 'mkdocs', 'build', '--strict'], check=True, cwd=ROOT)
    selected = json.loads((ROOT / 'website/publication.json').read_text(encoding='utf-8'))
    (public / 'assets').mkdir()
    (public / 'data').mkdir()
    shutil.copy2(ROOT / 'index.html', public / 'index.html')
    for asset in selected['landing_assets']:
        (public / 'assets' / asset).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / 'website/assets' / asset, public / 'assets' / asset)
    shutil.copy2(ROOT / 'data/supported-synths.json', public / 'data/supported-synths.json')
    (public / '.nojekyll').touch()
    subprocess.run([sys.executable, str(ROOT / 'scripts/check_website.py')], check=True, cwd=ROOT)
    print('Local website ready: public/ (serve this directory, not the repository)')


if __name__ == '__main__':
    main()

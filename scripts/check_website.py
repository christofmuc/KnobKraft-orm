#!/usr/bin/env python3
"""Check publication inventory and all local HTML/CSS links without network access."""
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlsplit
import json
import re

ROOT = Path(__file__).resolve().parents[1]
PUBLIC = ROOT / 'public'


class Document(HTMLParser):
    def __init__(self, text):
        super().__init__(convert_charrefs=True)
        self.links = []
        self.ids = set()
        self.feed(text)

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if 'id' in attrs:
            self.ids.add(attrs['id'])
        for key in ('href', 'src'):
            if attrs.get(key):
                self.links.append(attrs[key])


def page_output(source):
    path = Path(source)
    if path.stem in ('index', 'README'):
        return path.parent / 'index.html'
    return path.with_suffix('') / 'index.html'


def main():
    selection = json.loads((ROOT / 'website/publication.json').read_text(encoding='utf-8'))
    expected_pages = {'docs/' + page_output(p).as_posix() for p in selection['pages']}
    expected_pages |= {'index.html', 'docs/404.html'}
    actual_pages = {p.relative_to(PUBLIC).as_posix() for p in PUBLIC.rglob('*.html')}
    errors = []
    if actual_pages != expected_pages:
        errors.append(f'HTML inventory mismatch: extra={actual_pages - expected_pages}, missing={expected_pages - actual_pages}')
    explicit = set(selection['docs_assets']) | set(selection['shared_assets'])
    allowed_files = expected_pages | {'docs/' + p for p in explicit}
    allowed_files |= {'assets/' + p for p in selection['landing_assets']}
    allowed_files |= {'data/supported-synths.json', '.nojekyll', 'docs/search/search_index.json', 'docs/sitemap.xml', 'docs/sitemap.xml.gz'}
    for path in PUBLIC.rglob('*'):
        if not path.is_file():
            continue
        relative = path.relative_to(PUBLIC).as_posix()
        if relative not in allowed_files and not relative.startswith('docs/assets/'):
            errors.append(f'Unexpected published file: {relative}')
    docs = {p.resolve(): Document(p.read_text(encoding='utf-8')) for p in PUBLIC.rglob('*.html')}
    for path, document in docs.items():
        if path.name == '404.html':
            continue  # 404 is served at an arbitrary URL; Material uses canonical root links.
        for link in document.links:
            parsed = urlsplit(link)
            if parsed.scheme or parsed.netloc or link.startswith('#/'):
                continue
            destination = (path.parent / unquote(parsed.path)).resolve() if parsed.path else path
            if destination.is_dir():
                destination /= 'index.html'
            if not destination.is_relative_to(PUBLIC.resolve()):
                errors.append(f'{path.relative_to(PUBLIC)}: escapes site: {link}')
            elif not destination.exists():
                errors.append(f'{path.relative_to(PUBLIC)}: missing {link}')
            elif parsed.fragment and destination in docs and unquote(parsed.fragment) not in docs[destination].ids:
                errors.append(f'{path.relative_to(PUBLIC)}: missing anchor {link}')
    for path in PUBLIC.rglob('*.css'):
        for link in re.findall(r'url\([\"\']?([^\)\"\']+)', path.read_text(encoding='utf-8')):
            if urlsplit(link).scheme or link.startswith('#'):
                continue
            if not (path.parent / unquote(urlsplit(link).path)).exists():
                errors.append(f'{path.relative_to(PUBLIC)}: missing CSS asset {link}')
    search = json.loads((PUBLIC / 'docs/search/search_index.json').read_text(encoding='utf-8'))
    for doc in search['docs']:
        location = unquote(urlsplit(doc['location']).path)
        output = 'docs/' + location + ('index.html' if not location or location.endswith('/') else '')
        if output not in expected_pages:
            errors.append(f'Unexpected search page: {location}')
    if errors:
        raise SystemExit('\n'.join(errors))
    print(f'Checked {len(actual_pages)} HTML pages, local links/anchors, CSS assets and search inventory')


if __name__ == '__main__':
    main()

"""Explicit publication boundary, shared assets and single-source tutorial media."""
from pathlib import Path
import html
import json
import re

from mkdocs.exceptions import PluginError
from mkdocs.structure.files import File

ROOT = Path(__file__).resolve().parents[1]


def manifest():
    return json.loads((ROOT / 'website/publication.json').read_text(encoding='utf-8'))


def on_nav(nav, config, files):
    # MkDocs validates external nav links first. The theme's URL filter then
    # resolves this relative home link at every depth, including local previews.
    nav.items[0].url = '../'
    return nav


def on_files(files, config):
    selected = manifest()
    allowed = set(selected['pages'] + selected['docs_assets'])
    present = {file.src_uri for file in files}
    missing = allowed - present
    if missing:
        raise PluginError(f'Missing public sources: {sorted(missing)}')
    for file in list(files):
        # Preserve installed theme assets; only explicit repository docs may enter.
        in_docs = file.abs_src_path and Path(file.abs_src_path).resolve().is_relative_to(Path(config.docs_dir).resolve())
        if in_docs and file.src_uri not in allowed:
            files.remove(file)
    for destination, source in selected['shared_assets'].items():
        files.append(File.generated(config, destination, content=(ROOT / source).read_bytes()))
    return files


def on_page_markdown(markdown, page, config, files):
    tutorial = page.meta.get('tutorial')
    if not tutorial:
        return markdown
    records = json.loads((ROOT / 'website/tutorials.json').read_text(encoding='utf-8'))
    if tutorial not in records:
        raise PluginError(f'Missing tutorial metadata: {tutorial}')
    record = records[tutorial]
    meta = f'<p class="lesson-meta">{html.escape(record["basis"])} · Written guide reviewed {html.escape(record["reviewed"])}</p>'
    heading, _, rest = markdown.partition('\n')
    markdown = heading + '\n\n' + meta + '\n' + rest
    video = record.get('video')
    if video:
        if not re.fullmatch(r'[A-Za-z0-9_-]{11}', video['youtube_id']):
            raise PluginError('Invalid YouTube ID')
        title = html.escape(video['title'], quote=True)
        from mkdocs.utils import get_relative_url
        preview = get_relative_url('youtube-screenshot.PNG', page.file.dest_uri)
        watch_url = f'https://www.youtube.com/watch?v={video["youtube_id"]}'
        markdown += f'\n\n<a class="video-preview" href="{watch_url}"><img src="{preview}" alt="Preview of {title}"></a>\n\n'
        markdown += f'Demonstrated: **{video["demonstrated_version"]}**, {video["synth"]}. [Watch on YouTube]({watch_url}, "External link; YouTube receives data after you follow it").\n'
        if video.get('transcript'):
            # A reviewed local Markdown transcript must also be allowlisted.
            if video['transcript'] not in manifest()['pages']:
                raise PluginError('Transcript must be a selected public page')
            markdown += f'\n[Read the transcript]({get_relative_url(video["transcript"], page.file.src_uri)}).\n'
    return markdown

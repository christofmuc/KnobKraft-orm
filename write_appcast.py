import argparse
import base64
import os
import subprocess
import tempfile
from datetime import datetime, timezone

import markdown
import requests
from lxml import etree as ET

SPARKLE_NS = "http://www.andymatuschak.org/xml-namespaces/sparkle"
NSMAP = {"sparkle": SPARKLE_NS}

ET.register_namespace("sparkle", SPARKLE_NS)

access_token = os.getenv("APPCAST_ACCESS_TOKEN")

DEFAULT_CONFIGS = {
    "windows": {
        "appcast_url": "https://raw.githubusercontent.com/christofmuc/appcasts/master/KnobKraft-Orm/appcast.xml",
        "appcast_path": "KnobKraft-Orm/appcast.xml",
        "download_url_template": "https://github.com/christofmuc/KnobKraft-orm/releases/download/{version}/knobkraft_orm_setup_{version}.exe",
        "signature_attribute": f"{{{SPARKLE_NS}}}dsaSignature",
        "signature_file": "update.sig",
        "installer_arguments": "/SILENT /SP- /NOICONS /restartapplications",
        "content_type": "application/octet-stream",
        "length": "0",
        "channel_title": "KnobKraft Orm Updates (Windows)",
        "channel_description": "Release feed for KnobKraft Orm (Windows).",
        "sparkle_os": None,
        "minimum_system_version": None,
    },
    "mac": {
        "appcast_url": "https://raw.githubusercontent.com/christofmuc/appcasts/master/KnobKraft-Orm/appcast-macos.xml",
        "appcast_path": "KnobKraft-Orm/appcast-macos.xml",
        "download_url_template": "https://github.com/christofmuc/KnobKraft-orm/releases/download/{version}/KnobKraft_Orm-{version}-Darwin.dmg",
        "signature_attribute": f"{{{SPARKLE_NS}}}edSignature",
        "signature_file": "mac_update.sig",
        "installer_arguments": None,
        "content_type": "application/x-apple-diskimage",
        "length": "0",
        "channel_title": "KnobKraft Orm Updates (macOS)",
        "channel_description": "Release feed for KnobKraft Orm (macOS).",
        "sparkle_os": "macos",
        "minimum_system_version": "10.11",
    },
}


def download_file(url, save_path):
    response = requests.get(url)
    response.raise_for_status()
    with open(save_path, "wb") as file:
        file.write(response.content)


def create_empty_appcast(path, config):
    rss = ET.Element("rss", nsmap=NSMAP)
    rss.set("version", "2.0")
    channel = ET.SubElement(rss, "channel")
    ET.SubElement(channel, "title").text = config["channel_title"]
    ET.SubElement(channel, "link").text = "https://github.com/christofmuc/KnobKraft-orm"
    ET.SubElement(channel, "description").text = config["channel_description"]
    tree = ET.ElementTree(rss)
    tree.write(path, encoding="utf-8", xml_declaration=True, pretty_print=True)


def ensure_local_appcast(config, local_path):
    try:
        download_file(config["appcast_url"], local_path)
        return True
    except requests.HTTPError as exc:
        if exc.response.status_code == 404:
            create_empty_appcast(local_path, config)
            return False
        raise


def get_latest_git_tag():
    try:
        git_tag = (
            subprocess.check_output(["git", "describe", "--tags", "--abbrev=0"])
            .decode()
            .strip()
        )
        return git_tag
    except subprocess.CalledProcessError:
        return None


def get_current_time():
    now = datetime.now(timezone.utc).astimezone()
    formatted = now.strftime("%Y%m%d %H:%M:%S")
    tz_offset = int(now.utcoffset().total_seconds() / 3600)
    formatted += "%+d" % tz_offset
    return formatted


def read_signature(signature_file):
    with open(signature_file, "r", encoding="utf-8") as file:
        signature = file.read().strip()
    if ":" in signature:
        signature = signature.split(":")[-1].strip()
    return signature


def add_release(filename, version, sparkle_signature, config):
    tree = ET.parse(filename)
    root = tree.getroot()
    channel = root.find("channel")

    release_notes_link = (
        f"https://christofmuc.github.io/appcasts/KnobKraft-Orm/{version}.html"
    )
    download_url = config["download_url_template"].format(version=version)

    new_item = ET.Element("item")
    channel.insert(0, new_item)

    ET.SubElement(new_item, "title").text = f"Version {version}"
    ET.SubElement(new_item, f"{{{SPARKLE_NS}}}releaseNotesLink").text = release_notes_link
    ET.SubElement(new_item, "pubDate").text = get_current_time()

    enclosure = ET.SubElement(new_item, "enclosure")
    enclosure.set("url", download_url)
    enclosure.set(f"{{{SPARKLE_NS}}}version", version)
    enclosure.set(f"{{{SPARKLE_NS}}}shortVersionString", version)
    enclosure.set(config["signature_attribute"], sparkle_signature)
    if config.get("installer_arguments"):
        enclosure.set(
            f"{{{SPARKLE_NS}}}installerArguments", config["installer_arguments"]
        )
    if config.get("minimum_system_version"):
        enclosure.set(
            f"{{{SPARKLE_NS}}}minimumSystemVersion",
            config["minimum_system_version"],
        )
    if config.get("sparkle_os"):
        enclosure.set(f"{{{SPARKLE_NS}}}os", config["sparkle_os"])
    enclosure.set("length", config["length"])
    enclosure.set("type", config["content_type"])

    ET.indent(tree, space="    ")
    tree.write(filename, encoding="utf-8", xml_declaration=True, pretty_print=True)
    return ET.tostring(tree, encoding="utf-8", xml_declaration=True)


def get_file_sha(repo_owner, repo_name, file_path, access_token):
    api_url = f"https://api.github.com/repos/{repo_owner}/{repo_name}/contents/{file_path}"
    headers = {
        "Authorization": f"Bearer {access_token}",
        "Accept": "application/vnd.github.v3+json",
    }
    response = requests.get(api_url, headers=headers)
    if response.status_code == 404:
        return None
    response.raise_for_status()
    return response.json().get("sha")


def upload_to_github(updated_content, repo_owner, repo_name, file_path, sha=None):
    base64_content = base64.b64encode(updated_content).decode().strip()
    api_url = f"https://api.github.com/repos/{repo_owner}/{repo_name}/contents/{file_path}"
    headers = {
        "Authorization": f"Bearer {access_token}",
        "Accept": "application/vnd.github.v3+json",
    }
    data = {
        "message": f"Update {file_path}",
        "content": base64_content,
    }
    if sha:
        data["sha"] = sha
    response = requests.put(api_url, json=data, headers=headers)
    response.raise_for_status()


def convert_markdown_to_html(markdown_file):
    with open(markdown_file, "r", encoding="utf-8") as file:
        markdown_text = file.read()
    return markdown.markdown(markdown_text)


def build_config(args):
    config = DEFAULT_CONFIGS[args.platform].copy()
    if args.appcast_url:
        config["appcast_url"] = args.appcast_url
    if args.appcast_path:
        config["appcast_path"] = args.appcast_path
    if args.download_url_template:
        config["download_url_template"] = args.download_url_template
    if args.signature_attribute:
        config["signature_attribute"] = args.signature_attribute
    if args.signature_file:
        config["signature_file"] = args.signature_file
    if args.installer_arguments is not None:
        config["installer_arguments"] = args.installer_arguments or None
    if args.content_type:
        config["content_type"] = args.content_type
    if args.minimum_system_version is not None:
        config["minimum_system_version"] = args.minimum_system_version or None
    if args.sparkle_os is not None:
        config["sparkle_os"] = args.sparkle_os or None
    return config


def main():
    parser = argparse.ArgumentParser(
        description="Update the Sparkle appcast feed with a tagged release."
    )
    parser.add_argument(
        "--platform",
        choices=list(DEFAULT_CONFIGS.keys()),
        default="windows",
        help="Target platform whose defaults should be used.",
    )
    parser.add_argument("--appcast-url", help="Override the appcast download URL.")
    parser.add_argument("--appcast-path", help="Override the repository path to update.")
    parser.add_argument(
        "--download-url-template",
        help="Template for the downloadable artifact URL. Use {version} placeholder.",
    )
    parser.add_argument(
        "--signature-attribute",
        help="Fully qualified XML attribute for the Sparkle signature.",
    )
    parser.add_argument(
        "--signature-file",
        help="Path to the signature file generated by the signing tool.",
    )
    parser.add_argument(
        "--installer-arguments",
        help="Optional installer arguments to embed in the enclosure element.",
    )
    parser.add_argument(
        "--content-type",
        help="MIME type of the downloadable artifact.",
    )
    parser.add_argument(
        "--minimum-system-version",
        help="Minimum supported OS version reported to Sparkle.",
    )
    parser.add_argument(
        "--sparkle-os",
        help="sparkle:os attribute value for the enclosure.",
    )
    args = parser.parse_args()

    new_version = get_latest_git_tag()
    print(f"Latest Git tag used as appcast version: {new_version}")
    if new_version is None:
        raise Exception("Can't create release without version tag!")

    config = build_config(args)
    signature_file = config["signature_file"]
    sparkle_signature = read_signature(signature_file)
    print(f"Got sparkle signature as {sparkle_signature}")

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpfile = os.path.join(tmpdir, os.path.basename(config["appcast_path"]))
        existing = ensure_local_appcast(config, tmpfile)
        new_file = add_release(tmpfile, new_version, sparkle_signature, config)
        appcast_sha = None
        if existing:
            appcast_sha = get_file_sha(
                "christofmuc", "appcasts", config["appcast_path"], access_token
            )
        upload_to_github(
            new_file, "christofmuc", "appcasts", config["appcast_path"], appcast_sha
        )

    release_notes_md = os.path.join("release_notes", f"{new_version}.md")
    release_notes_as_html = convert_markdown_to_html(release_notes_md).encode("utf-8")
    release_notes_path = f"KnobKraft-Orm/{new_version}.html"
    release_notes_sha = get_file_sha(
        "christofmuc", "appcasts", release_notes_path, access_token
    )
    upload_to_github(
        release_notes_as_html,
        "christofmuc",
        "appcasts",
        release_notes_path,
        release_notes_sha,
    )


if __name__ == "__main__":
    main()

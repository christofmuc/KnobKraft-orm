import base64
import os
import subprocess
import tempfile
from datetime import datetime, timezone

import requests
from lxml import etree as ET

access_token = os.getenv("APPCAST_ACCESS_TOKEN")


def download_file(url, save_path):
    response = requests.get(url)
    response.raise_for_status()
    with open(save_path, 'wb') as file:
        file.write(response.content)


def get_latest_git_tag():
    try:
        git_tag = subprocess.check_output(['git', 'describe', '--tags', '--abbrev=0']).decode().strip()
        return git_tag
    except subprocess.CalledProcessError:
        return None


def get_current_time():
    now = datetime.now(timezone.utc).astimezone()
    formatted = now.strftime('%Y%m%d %H:%M:%S')
    tz_offset = int(now.utcoffset().total_seconds() / 3600)
    formatted += '%+d' % tz_offset
    return formatted


def add_release(filename, version, sparkle_signature):
    # Load the XML file
    tree = ET.parse(filename)
    root = tree.getroot()

    # Define the new item data
    new_item_data = {
        'title': f'Version {version}',
        'releaseNotesLink': f'https://christofmuc.github.io/appcasts/KnobKraft-Orm/{version}.html',
        'pubDate': get_current_time(),
        'url': f'https://github.com/christofmuc/KnobKraft-orm/releases/download/{version}/knobkraft_orm_setup_{version}.exe',
        'version': f'{version}',
        'dsaSignature': sparkle_signature,
        'installerArguments': '/SILENT /SP- /NOICONS /restartapplications',
        'length': '0',
        'type': 'application/octet-stream'
    }

    # Create a new item
    new_item = ET.SubElement(root.find('channel'), 'item')

    # Add sub-elements to the new item
    ET.SubElement(new_item, 'title').text = new_item_data['title']
    ET.SubElement(new_item, '{http://www.andymatuschak.org/xml-namespaces/sparkle}releaseNotesLink').text = new_item_data['releaseNotesLink']
    ET.SubElement(new_item, 'pubDate').text = new_item_data['pubDate']

    enclosure = ET.SubElement(new_item, 'enclosure')
    enclosure.set('url', new_item_data['url'])
    enclosure.set('{http://www.andymatuschak.org/xml-namespaces/sparkle}version', new_item_data['version'])
    enclosure.set('{http://www.andymatuschak.org/xml-namespaces/sparkle}dsaSignature', new_item_data['dsaSignature'])
    enclosure.set('{http://www.andymatuschak.org/xml-namespaces/sparkle}installerArguments', new_item_data['installerArguments'])
    enclosure.set('length', new_item_data['length'])
    enclosure.set('type', new_item_data['type'])

    # Pretty-print the entire tree
    ET.indent(tree, space="    ")

    # Write the updated XML back to the file
    tree.write(filename, encoding='utf-8', xml_declaration=True, pretty_print=True)
    return ET.tostring(tree, encoding='utf-8', xml_declaration=True)


def get_file_sha(repo_owner, repo_name, file_path, access_token):
    # Create the API URL to get the file information
    api_url = f"https://api.github.com/repos/{repo_owner}/{repo_name}/contents/{file_path}"

    # Prepare the headers for the API request
    headers = {
        "Authorization": f"Bearer {access_token}",
        "Accept": "application/vnd.github.v3+json"
    }

    # Send the API request to get the file information
    response = requests.get(api_url, headers=headers)
    response.raise_for_status()

    # Get the SHA of the file
    file_info = response.json()
    sha = file_info.get('sha')

    return sha


def upload_to_github(updated_xml, repo_owner, repo_name, file_path):
    # Encode the XML content as base64
    base64_content = base64.b64encode(updated_xml).decode().strip()

    # Create the API URL to update the file
    api_url = f"https://api.github.com/repos/{repo_owner}/{repo_name}/contents/{file_path}"

    # Prepare the headers and data for the API request
    headers = {
        "Authorization": f"Bearer {access_token}",
        "Accept": "application/vnd.github.v3+json"
    }
    data = {
        "message": f"Update {file_path}",
        "content": base64_content,
        "sha": get_file_sha(repo_owner, repo_name, file_path, access_token)
    }

    # Send the API request to update the file
    response = requests.put(api_url, json=data, headers=headers)
    response.raise_for_status()


if __name__ == "__main__":
    new_version = get_latest_git_tag()
    print(f"Latest Git tag used as appcast version: {new_version}")
    if new_version is None:
        raise Exception("Can't create release without version tag!")

    # Read signature file
    with open("update.sig", 'r') as file:
        sparkle_signature = file.read()
    print(f"Got sparkle signature as {sparkle_signature}")

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpfile = os.path.join(tmpdir, "appcast.xml")
        download_file("https://raw.githubusercontent.com/christofmuc/appcasts/master/KnobKraft-Orm/test_appcast.xml", tmpfile)
        new_file = add_release(tmpfile, new_version, sparkle_signature)
        upload_to_github(new_file, "christofmuc", "appcasts", "KnobKraft-Orm/test_appcast.xml")

import re
import sys

def extract_sysex_from_log(input_file, output_file):
    #sysex_pattern = re.compile(r'.*From\s+.+Sysex\s+([0-9a-fA-F\s]+)')
    sysex_pattern = re.compile(r'.*From.*(F0(\s[0-9A-F]{2})*\sF7)')

    with open(input_file, 'r', encoding='utf-8') as infile, open(output_file, 'wb') as outfile:
        for line in infile:
            match = sysex_pattern.search(line)
            if match:
                hex_data = match.group(1).split()
                binary_data = bytes(int(h, 16) for h in hex_data)
                outfile.write(binary_data)

    print(f"Extracted SysEx messages written to {output_file}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python script.py <input_log_file> <output_syx_file>")
        sys.exit(1)

    extract_sysex_from_log(sys.argv[1], sys.argv[2])

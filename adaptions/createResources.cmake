#
# Replace the JUCE BinaryBuilder with something more CMAKE
# This magic is from https://stackoverflow.com/questions/11813271/embed-resources-eg-shader-code-images-into-executable-library-with-cmake
#
# Creates C resources file from files in given directory
message("Output is '${OUTPUT}' and Dir is '${DIR}'")
file(WRITE ${OUTPUT} "")
# Collect input files
file(GLOB bins ${DIR})
# Iterate through input files
foreach(bin ${bins})
	# Get short filename
	string(REGEX MATCH "([^/]+)$" filename ${bin})
	# Replace filename spaces & extension separator for C compatibility
	string(REGEX REPLACE "\\.| |-" "_" filename ${filename})
	# Read hex data from file
	file(READ ${bin} filedata HEX)
	# Convert hex data for C compatibility
	string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," filedata ${filedata})
	# Append data to output file
	file(APPEND ${OUTPUT} "const unsigned char ${filename}[] = {${filedata}};\nconst unsigned ${filename}_size = sizeof(${filename});\n")
endforeach()


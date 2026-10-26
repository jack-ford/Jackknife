# Generate a portable C++ asset so library users need no runtime working directory
# or external network file. Only regenerated when the binary asset changes.
file(READ "${INPUT}" networkHex HEX)
string(REGEX REPLACE "(................................)" "\\1\n" networkLines "${networkHex}")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," networkBytes "${networkLines}")
file(WRITE "${OUTPUT}" "namespace eval::nnue {\nextern const unsigned char EmbeddedNetwork[] = {\n${networkBytes}\n};\n}\n")

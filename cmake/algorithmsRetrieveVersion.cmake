# Retrieve version identification.
#
# Must be included from the main CMakeLists file. Sets the following variables:
#
#   - _algorithms_version
#

# read version number from file
file(READ "version_number" _algorithms_version)
string(STRIP "${_algorithms_version}" _algorithms_version)

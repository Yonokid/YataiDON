#!/bin/bash

# YataiDON Run Script
# This script runs the YataiDON executable with AddressSanitizer/LeakSanitizer suppressions

# Color output for better readability
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get the directory where the script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Paths
EXECUTABLE="$SCRIPT_DIR/YataiDON"
LSAN_SUPP="$SCRIPT_DIR/lsan.supp"

# Check if executable exists
if [ ! -f "$EXECUTABLE" ]; then
    echo -e "${RED}Error: Executable not found at $EXECUTABLE${NC}"
    echo -e "${YELLOW}Did you build the project? The executable should be in the root directory.${NC}"
    echo -e "${YELLOW}You can copy it from build/bin/YataiDON to the root directory.${NC}"
    exit 1
fi

# Check if executable is executable
if [ ! -x "$EXECUTABLE" ]; then
    echo -e "${YELLOW}Warning: Executable doesn't have execute permissions. Adding them...${NC}"
    chmod +x "$EXECUTABLE"
fi

# Check if lsan.supp exists
if [ ! -f "$LSAN_SUPP" ]; then
    echo -e "${RED}Error: Leak sanitizer suppressions file not found at $LSAN_SUPP${NC}"
    exit 1
fi

# Set up sanitizer options
export LSAN_OPTIONS="suppressions=$LSAN_SUPP:print_suppressions=0"
export ASAN_OPTIONS="detect_leaks=1:halt_on_error=0:log_path=asan.log"

echo -e "${GREEN}Starting YataiDON...${NC}"
echo -e "${YELLOW}Leak Sanitizer suppressions: $LSAN_SUPP${NC}"
echo ""

# Run the executable from the root directory
cd "$SCRIPT_DIR"
"$EXECUTABLE" "$@"

# Capture exit code
EXIT_CODE=$?

echo ""
if [ $EXIT_CODE -eq 0 ]; then
    echo -e "${GREEN}YataiDON exited successfully.${NC}"
else
    echo -e "${RED}YataiDON exited with code $EXIT_CODE${NC}"
fi

exit $EXIT_CODE

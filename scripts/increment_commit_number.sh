#!/bin/bash

# File to store the commit number
COMMIT_NUMBER_FILE="commit_number.txt"

# Check if the file exists
if [ ! -f "$COMMIT_NUMBER_FILE" ]; then
    echo "1" > "$COMMIT_NUMBER_FILE"  # Initialize with 1 if file doesn't exist
fi

# Read the current commit number
current_number=$(cat "$COMMIT_NUMBER_FILE")

# Increment the commit number
new_number=$((current_number + 1))

# Write the new number back to the file
echo "$new_number" > "$COMMIT_NUMBER_FILE"

# Optionally, you can also update the Makefile here if needed
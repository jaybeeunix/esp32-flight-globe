#!/usr/bin/env python3
"""
dedup_routes.py - Parse multi-leg flight routes and deduplicate bidirectional legs.

Example:
    Input:
        CMI-ORD-LHR-ORD-CMI
        LAX-JFK
        JFK-LAX

    Output:
        CMI-ORD
        ORD-LHR
        LAX-JFK

    Note:
        'CMI-ORD' and 'ORD-CMI' are recognized as duplicates of the same route.
"""

import sys
import os
import argparse
import re

def parse_route_line(line: str):
    """
    Parses a route line (e.g., 'CMI-ORD-LHR-ORD-CMI' or 'ORD - SFO')
    into a list of airport codes.
    Strips comments, quotes, and whitespace.
    """
    # Remove comments
    line = re.sub(r'#.*$', '', line)
    # Strip quotes and whitespace
    line = line.strip().strip('"\'')
    if not line:
        return []

    # Split by hyphen (or multiple hyphens / dashes / arrows / slashes)
    parts = re.split(r'[\s\-–—>]+', line)
    # Filter out empty tokens and uppercase
    airports = [p.strip().upper() for p in parts if p.strip()]
    return airports

def extract_unique_legs(lines, canonical=False, sort_routes=False):
    """
    Takes iterable of lines and returns a list of unique route legs.
    Bidirectional duplicates (A-B and B-A) are deduplicated.
    """
    seen_legs = set()
    unique_routes = []
    total_legs_found = 0

    for line in lines:
        airports = parse_route_line(line)
        if len(airports) < 2:
            continue

        # Extract consecutive pairs (legs)
        for i in range(len(airports) - 1):
            orig = airports[i]
            dest = airports[i + 1]

            if orig == dest:
                # Ignore self-loops like ORD-ORD unless desired
                continue

            total_legs_found += 1

            # Key for bidirectional matching: order-independent tuple
            pair_key = tuple(sorted([orig, dest]))

            if pair_key not in seen_legs:
                seen_legs.add(pair_key)
                if canonical:
                    # Output in alphabetical order (e.g. CMI-ORD)
                    formatted_route = f"{pair_key[0]}-{pair_key[1]}"
                else:
                    # Preserve original first-seen direction
                    formatted_route = f"{orig}-{dest}"
                unique_routes.append(formatted_route)

    if sort_routes:
        unique_routes.sort()

    return unique_routes, total_legs_found

def main():
    parser = argparse.ArgumentParser(
        description="Extract single flight legs from multi-hop route lines and deduplicate bidirectional routes."
    )
    parser.add_argument(
        "input",
        nargs="?",
        default="-",
        help="Input text file path (defaults to reading from standard input / pipe)"
    )
    parser.add_argument(
        "-o", "--output",
        default=None,
        help="Output file path (defaults to standard output)"
    )
    parser.add_argument(
        "-c", "--canonical",
        action="store_true",
        help="Format legs with airport codes in alphabetical order (e.g., always 'CMI-ORD' instead of 'ORD-CMI')"
    )
    parser.add_argument(
        "-s", "--sort",
        action="store_true",
        help="Sort the resulting unique routes alphabetically"
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Print summary statistics to stderr"
    )

    args = parser.parse_args()

    # Read input
    if args.input == "-" or not os.path.exists(args.input):
        if args.input != "-":
            print(f"Error: file not found '{args.input}'", file=sys.stderr)
            sys.exit(1)
        lines = sys.stdin.readlines()
    else:
        with open(args.input, "r", encoding="utf-8", errors="ignore") as f:
            lines = f.readlines()

    unique_routes, total_legs = extract_unique_legs(
        lines,
        canonical=args.canonical,
        sort_routes=args.sort
    )

    # Output results
    output_text = "\n".join(unique_routes) + ("\n" if unique_routes else "")

    if args.output:
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(output_text)
    else:
        sys.stdout.write(output_text)

    if args.verbose:
        duplicates_removed = total_legs - len(unique_routes)
        sys.stderr.write(
            f"\n[Summary]\n"
            f"Total legs extracted: {total_legs}\n"
            f"Unique routes:        {len(unique_routes)}\n"
            f"Duplicates removed:   {duplicates_removed}\n"
        )

if __name__ == "__main__":
    main()

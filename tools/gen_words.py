#!/usr/bin/env python3
"""Generate words.h from words.txt for takt1ccname mod."""
import os, sys

def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    src_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, "words.txt")
    dst_path = sys.argv[2] if len(sys.argv) > 2 else os.path.join(root, "words.h")

    if not os.path.exists(src_path):
        sys.stderr.write(f"Error: {src_path} not found\n")
        sys.exit(1)

    words = []
    with open(src_path, "r", encoding="utf-8") as f:
        for line_num, line in enumerate(f, 1):
            line = line.rstrip("\r\n")
            if not line or line.startswith("#"):
                continue
            if len(line) > 4:
                sys.stderr.write(f"Warning: line {line_num} '{line}' exceeds 4 chars, truncating\n")
                line = line[:4]
            # Pad to 4 chars with space
            word = line.ljust(4)[:4]
            words.append(word)

    if len(words) != 64:
        sys.stderr.write(f"Error: words.txt must contain exactly 64 words, found {len(words)}\n")
        sys.exit(1)

    with open(dst_path, "w", encoding="utf-8") as f:
        f.write("/* Auto-generated from words.txt by tools/gen_words.py. Do not edit. */\n")
        f.write("#ifndef WORDS_H\n#define WORDS_H\n\n")
        f.write("#define NUM_WORDS 64\n\n")
        f.write("static const char WORDS[NUM_WORDS][5] = {\n")
        for i, w in enumerate(words):
            # Escape backslashes and double quotes if any
            esc = w.replace("\\", "\\\\").replace('"', '\\"')
            comma = "," if i < len(words) - 1 else ""
            f.write(f'    "{esc}"{comma} /* {i:2d} */\n')
        f.write("};\n\n")
        f.write("#endif /* WORDS_H */\n")

    print(f"Generated {dst_path} with {len(words)} words.")

if __name__ == "__main__":
    main()

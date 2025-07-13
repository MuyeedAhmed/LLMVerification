import os
import re
import sys
import pandas as pd
from collections import defaultdict

# CATEGORY_KEYWORDS = {
#     "Memory": ["use-after-free", "leak", "deallocated", "malloc", "free"],
#     "Null Dereference": ["null", "nullptr", "null pointer", "dereference"],
#     "Dead Store": ["dead store"],
#     "Unix API": ["open", "close", "fopen", "fclose", "file descriptor"],
#     "Uninitialized": ["uninitialized"],
#     "Security": ["overflow", "security", "buffer", "format string"],
#     "Logic": ["always true", "always false", "tautology", "redundant"],
#     "Syntax Error": ["expected", "extraneous", "invalid syntax", "parse error"],
#     "Unsafe Cast": ["implicit conversion", "incompatible type", "loss of", "cast"],
#     "Undeclared Identifier": ["undeclared identifier", "implicit function declaration", "call to undeclared"],
#     "Return Issue": ["does not return a value", "no return", "missing return"],
#     "Initializer Error": ["not a compile-time constant", "initializer element"],
#     "Assembly Constraint": ["invalid output constraint", "asm"],
#     "Standard Compliance": ["C99", "ISO C", "before C99", "standards before"],
#     "Struct Member Access": ["no member named", "incompatible struct", "invalid field"],
#     "Logic": ["always true", "always false", "tautology", "redundant"],
# }

CATEGORY_KEYWORDS = {
    "Syntax": ["expected", "extraneous", "invalid syntax", "parse error"],
    "Null Dereference": ["null", "nullptr", "null pointer", "dereference"],
    "Undeclared identifier": ["undeclared identifier", "implicit function declaration", "call to undeclared"],
    "Invalid conversion": ["implicit conversion", "incompatible type", "loss of", "cast"],
    "Free release memory": ["double free", "freeing", "already freed"],
    "Use after free": ["use-after-free", "access after free", "dangling pointer"],
}


def classify_issue(message):
    msg = message.lower()
    for category, keywords in CATEGORY_KEYWORDS.items():
        if any(keyword in msg for keyword in keywords):
            return category
    return "Uncategorized"

def parse_clang_report_summary(file_path):
    summary = defaultdict(int)
    with open(file_path, "r") as f:
        content = f.read()

    pattern = r'^(.*?):(\d+):\d+:\s+(warning|error):\s+(.*)$'
    for match in re.finditer(pattern, content, re.MULTILINE):
        _, _, _, message = match.groups()
        category = classify_issue(message)
        summary[category] += 1

    file_name = os.path.basename(file_path)
    result = {
        "Report": file_name,
        "Suffix": "llm" if "llm" in file_name else "original",
    }
    result.update(summary)
    return result

def generate_summary(folder_path, output_file="ExcelFiles/FFmpeg_Clang_Summary.xlsx"):
    all_summaries = []
    for root, _, files in os.walk(folder_path):
        for file in files:
            if file.endswith(".txt"):
                file_path = os.path.join(root, file)
                summary = parse_clang_report_summary(file_path)
                all_summaries.append(summary)

    df = pd.DataFrame(all_summaries).fillna(0)
    df.to_excel(output_file, index=False)


generate_summary("Clang_Reports/FFmpeg")

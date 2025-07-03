import os
import re
import sys
import pandas as pd

CATEGORY_KEYWORDS = {
    "Memory": ["use-after-free", "leak", "deallocated", "malloc", "free"],
    "Null Dereference": ["null", "nullptr", "null pointer", "dereference"],
    "Dead Store": ["dead store"],
    "Unix API": ["open", "close", "fopen", "fclose", "file descriptor"],
    "Uninitialized": ["uninitialized"],
    "Security": ["overflow", "security", "buffer", "format string"],
    "Logic": ["always true", "always false", "tautology", "redundant"],
    "Syntax Error": ["expected", "extraneous", "invalid syntax", "parse error"],
    "Unsafe Cast": ["implicit conversion", "incompatible type", "loss of", "cast"],
    "Undeclared Identifier": ["undeclared identifier", "implicit function declaration", "call to undeclared"],
    "Return Issue": ["does not return a value", "no return", "missing return"],
    "Initializer Error": ["not a compile-time constant", "initializer element"],
    "Assembly Constraint": ["invalid output constraint", "asm"],
    "Standard Compliance": ["C99", "ISO C", "before C99", "standards before"],
    "Struct Member Access": ["no member named", "incompatible struct", "invalid field"],
    "Logic": ["always true", "always false", "tautology", "redundant"],
}

def classify_issue(message):
    msg = message.lower()
    for category, keywords in CATEGORY_KEYWORDS.items():
        if any(keyword in msg for keyword in keywords):
            return category
    return "Uncategorized"

def parse_clang_report(file_path, code_by="Human"):
    issues = []
    with open(file_path, "r") as f:
        content = f.read()

    pattern = r'^(.*?):(\d+):\d+:\s+(warning|error):\s+(.*)$'
    for match in re.finditer(pattern, content, re.MULTILINE):
        file, line, level, message = match.groups()
        category = classify_issue(message)
        issues.append({
            "CodeBy": code_by,
            "File": os.path.basename(file),
            "Line": int(line),
            "Level": level,
            "Message": message,
            "Category": category,
            "Report": os.path.basename(file_path)
        })
    return issues

def save_to_excel(issues, output_file="clang_analysis_summary.xlsx"):
    df = pd.DataFrame(issues)
    df.sort_values(by=["Category", "File", "Line"], inplace=True)
    df.to_excel(output_file, index=False)


# issues = parse_clang_report(report_file)
# save_to_excel(issues)

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
# }

CATEGORY_KEYWORDS = {
    "Syntax": ["expected", "extraneous", "invalid syntax", "parse error"],
    "Null Dereference": ["null pointer dereference", "dereference of a null pointer"],
    "Undeclared identifier": ["undeclared identifier", "implicit function declaration", "call to undeclared"],
    "Unsafe Cast": ["implicit conversion", "incompatible type", "conversion loses", "loses precision", "conversion from", "conversion to", 
        "casting a non-structure type to a structure type", "no member named", "incompatible struct", "implicit conversion", "incompatible type", "loss of", "cast"
        ],
    "Free release memory": ["double free", "freeing", "already freed"],
    "Use after free": ["use-after-free", "access after free", "dangling pointer"],
    "Buffer overflow": ["out-of-bound", "out of bounds", "buffer overflow","access outside array bounds", "alpha.security.arraybound"],
    "Semantic error": ["'break' statement not in loop", "'continue' statement not in loop", "control reaches end of non-void function", "return with a value in void function", "incompatible return type",
        "unreachable code", "case label not within a switch statement", "jump to case label crosses initialization", "too few arguments", "too many arguments",
        "expects", "but", "were provided", "non-void function must return", "void function should not return", "return type mismatch", "function does not return a value", "missing return"
        ], 
    "Syntax Error": ["expected", "extraneous", "invalid syntax", "parse error"],
}

UNCATEGORIZED_KEYWORDS = {  
    "Memory": ["leak", "deallocated", "malloc", "free"],
    "Unix API": ["open", "close", "fopen", "fclose", "file descriptor"],
    "Uninitialized": ["uninitialized"],
    "Logic": ["always true", "always false", "tautology", "redundant"],
    "Initializer Error": ["not a compile-time constant", "initializer element"],
    "Assembly Constraint": ["invalid output constraint", "asm"],
    "Standard Compliance": ["C99", "ISO C", "before C99", "standards before"],
}


def classify_issue(message):
    msg = message.lower()
    for category, keywords in CATEGORY_KEYWORDS.items():
        if any(keyword in msg for keyword in keywords):
            return category
    
    C = "Uncategorized"
    for category, keywords in UNCATEGORIZED_KEYWORDS.items():
        if any(keyword in msg for keyword in keywords):
            C = category
            # break
    if "DeadStores" in message:
        return None
    with open("Uncategorized_Issues.txt", "a") as f:
        f.write(f"{C}:\t\t{message}\n")
    return "Uncategorized"

def parse_clang_report_summary(file_path, commit_info):
    changed_filename = commit_info["Changed File"].values[0]
    change_function = commit_info["Changed Functions"].values[0]
    if change_function is None:
        start_loc = -1
        end_loc = -1
    elif "llm" in file_path:
        start_loc = commit_info["llm_start"].values[0]
        end_loc = commit_info["llm_end"].values[0]
    else:
        start_loc = commit_info["orig_start"].values[0]
        end_loc = commit_info["orig_end"].values[0]

    summary = defaultdict(int)
    with open(file_path, "r") as f:
        content = f.read()

    pattern = r'^(.*?):(\d+):\d+:\s+(warning|error):\s+(.*)$'
    for match in re.finditer(pattern, content, re.MULTILINE):
        full_path, line_str, level, message = match.groups()
        line_num = int(line_str)

        report_file = os.path.basename(full_path)
        expected_file = os.path.basename(changed_filename)

        if report_file != expected_file:
            continue
        if start_loc != -1 and (line_num < start_loc or line_num > end_loc):
            continue

        category = classify_issue(message)
        if category == None:
            continue
        summary[category] += 1

    file_name = os.path.basename(file_path)
    result = {
        "Commit Hash" : file_name.split("_")[0],
        "Report": file_name,
        "Suffix": "llm" if "llm" in file_name else "original",
    }
    result.update(summary)
    return result

def generate_summary(excelFile, folder_path, output_file="ExcelFiles/FFmpeg_Clang_Summary.xlsx"):
    Commit_DF = pd.read_excel(excelFile)
    all_summaries = []
    for root, _, files in os.walk(folder_path):
        for file in files:
            if file.endswith(".txt"):
                filename = file.split("_")[0]
                commit_info = Commit_DF.loc[Commit_DF["Commit Hash"] == filename]
                if commit_info.empty:
                    print("No commit info found for:", filename)
                    # continue
                file_path = os.path.join(root, file)
                summary = parse_clang_report_summary(file_path, commit_info)
                all_summaries.append(summary)

    df = pd.DataFrame(all_summaries).fillna(0)
    merged_df = pd.merge(Commit_DF, df, on="Commit Hash", how="left")
    merged_df = merged_df.fillna(0)
    merged_df.to_excel(output_file, index=False)


generate_summary("ExcelFiles/FFmpeg_fl.xlsx", "Clang_Reports/FFmpeg")

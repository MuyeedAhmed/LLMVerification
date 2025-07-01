import subprocess
import tempfile
import re
import os
import pandas as pd
import time

def extract_c_content(code):
    try:
        lines = code.splitlines()

        c_code = []
        inside_c_block = False

        for line in lines:
            if line.strip() == "```c":
                inside_c_block = True
                continue
            if line.strip() == "```":
                if inside_c_block:
                    break

            if inside_c_block:
                c_code.append(line)
        return c_code
        
    except Exception as e:
        print(f"Error: {e}")
        return code

def get_function_start_line_from_ctags(temp_path, function_name):
    try:
        output = subprocess.check_output(["ctags", "-n", "--fields=+ne", "-o", "-", temp_path]).decode()
        for line in output.splitlines():
            parts = line.split()
            if len(parts) >= 4 and parts[0] == function_name:
                match = re.search(r'line:(\d+)', line)
                if match:
                    return int(match.group(1))
    except subprocess.CalledProcessError as e:
        print(f"[!] ctags error: {e}")
    return None

def get_function_end_line_from_braces(lines, start_line):
    brace_count = 0
    inside = False

    for i in range(start_line - 1, len(lines)):
        line = re.sub(r'//.*|/\*.*?\*/|".*?"', '', lines[i])

        for char in line:
            if char == '{':
                brace_count += 1
                inside = True
            elif char == '}':
                brace_count -= 1

        if inside and brace_count == 0:
            return i + 1

    return None



def ReplaceFunctionAtOriginalLine(content, function_name, changed_function_code_stripped):
    with tempfile.NamedTemporaryFile(delete=False, suffix=".c", mode='w') as tmp:
        tmp.write(content)
        temp_path = tmp.name
    lines = content.splitlines()
    start_line = get_function_start_line_from_ctags(temp_path, function_name)
    print(f"Function '{function_name}' starts at line: {start_line}")
    end_line = get_function_end_line_from_braces(lines, start_line)
    print(f"Function '{function_name}' ends at line: {end_line}")
    changed_lines = changed_function_code_stripped if isinstance(changed_function_code_stripped, list) else [changed_function_code_stripped]
    new_lines = lines[:start_line - 1] + changed_lines + lines[end_line:]

    return "\n".join(new_lines) + "\n"


def ReplaceFunctionAtOriginalLine_WithRE(content, function_name, changed_function_code_stripped):
    pattern = rf'(?:\b[\w\s\*]+)?\b{function_name}\s*\([^)]*\)\s*\{{(?:[^{{}}]*|\{{[^{{}}]*\}})*\}}'
    print(f"Searching for function pattern: {pattern}")
    match = re.search(pattern, content, flags=re.DOTALL)
    print(f"Match found: {match is not None}")
    if not match:
        return content, None

    start_index = match.start()
    end_index = match.end()
    line_number = content[:start_index].count('\n') + 1

    lines = content.splitlines()
    removed_lines = content[start_index:end_index].count('\n') + 1

    changed_lines = changed_function_code_stripped if isinstance(changed_function_code_stripped, list) else [changed_function_code_stripped]
    new_lines = lines[:line_number - 1] + changed_lines + lines[line_number - 1 + removed_lines:]

    return "\n".join(new_lines) + "\n", line_number



def MergeLLMOutput(commit, change_file_dir, changed_function):
    change_file_name = change_file_dir.split("/")[-1].split(".")[0]

    source_file = os.path.join("FileHistory", commit, f"{change_file_name}_original.c")
    source_file_llm_function = os.path.join("FileHistory", commit, f"{change_file_name}_llm_function.c")
    destination_file = os.path.join("FileHistory", commit, f"{change_file_name}_llm.c")

    with open(source_file, 'r') as file:
        source_code = file.read()

    with open(source_file_llm_function, 'r') as file:
        changed_function_code = file.read()

    changed_function_code_stripped = extract_c_content(changed_function_code)

    updated_code = ReplaceFunctionAtOriginalLine(
        source_code,
        changed_function,
        changed_function_code_stripped
    )

    with open(destination_file, "w") as f_out:
        f_out.write(updated_code)
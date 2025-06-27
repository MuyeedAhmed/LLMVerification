import os
import re
import pandas as pd
from openai import OpenAI
client = OpenAI()

def RunLLM(commit_df, commit):
    df = commit_df[commit_df["Commit Hash"] == commit]
    message = df["Commit Message"].values[0]
    change_file_dir = df["Changed File"].values[0]
    change_file_name = change_file_dir.split("/")[-1].split(".")[0]
    changed_function = df["Changed Functions"].values[0]

    source_file = os.path.join("FileHistory", commit, f"{change_file_name}_original.c")
    destination_file = os.path.join("FileHistory", commit, f"{change_file_name}_llm_function.c")
    with open(source_file, 'r') as file:
        source_code = file.read()

    updated_prompt = f"""
    Fix Requirement: {message}
    Function:{changed_function}
    C file:
    ```
    {source_code}
    ```
    Give me the update {changed_function} function.
    """
    
    completion = client.chat.completions.create(
        model="gpt-4o",
        messages=[
            {"role": "system", "content": "You are an expert in C programming language. You are working on leetcode problems, and you need to implement certain functions based on the prompts."},
            {"role": "user", "content": updated_prompt}
        ],
        max_tokens=2500,
        temperature=0.5
    )

    output = completion.choices[0].message.content.strip()
    with open(destination_file, "w") as f_out:
        f_out.writelines(output)

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

def RemoveFunctionFromCode(content, function_name):
    pattern = rf'\b\w+\s+\**{function_name}\s*\([^)]*\)\s*\{{(?:[^{{}}]*|\{{[^{{}}]*\}})*\}}'
    return re.sub(pattern, '', content, flags=re.DOTALL)


def MergeLLMOutput(commit_df, commit):
    df = commit_df[commit_df["Commit Hash"] == commit]
    message = df["Commit Message"].values[0]
    change_file_dir = df["Changed File"].values[0]
    change_file_name = change_file_dir.split("/")[-1].split(".")[0]
    changed_function = df["Changed Functions"].values[0]

    source_file = os.path.join("FileHistory", commit, f"{change_file_name}_original.c")
    source_file_llm_function = os.path.join("FileHistory", commit, f"{change_file_name}_llm_function.c")
    destination_file = os.path.join("FileHistory", commit, f"{change_file_name}_llm.c")

    with open(source_file, 'r') as file:
        source_code = file.read()
    source_code_removed_function = RemoveFunctionFromCode(source_code, changed_function)

    with open(source_file_llm_function, 'r') as file:
        changed_function_code = file.read()

    changed_function_code_stripped = extract_c_content(changed_function_code)

    updated_code = source_code_removed_function.rstrip() + "\n\n" + "\n".join(changed_function_code_stripped) + "\n"

    with open(destination_file, "w") as f_out:
        f_out.write(updated_code)


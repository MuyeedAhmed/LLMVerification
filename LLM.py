import os
import re
import pandas as pd
from openai import OpenAI
import openai
import time

client = OpenAI()
openai.api_key = os.getenv("OPENAI_API_KEY")



def RunLLM(commit, message, changed_function, change_file_dir):
    change_file_name = change_file_dir.split("/")[-1].split(".")[0]
    source_file = os.path.join("FileHistory", commit, f"{change_file_name}_original.c")
    destination_file = os.path.join("FileHistory", commit, f"{change_file_name}_llm_function.c")

    if not os.path.exists(source_file):
        print(f"Source file not found: {source_file}")
        return

    updated_prompt = f"""
    Fix Requirement: {message}
    Function:{changed_function}
    Please return only the updated definition of the function '{changed_function}' as a single C function. Do not include the full source file or any extra commentary.
    """

    upload = openai.files.create(
        file=open(source_file, "rb"),
        purpose="assistants"
    )
    file_id = upload.id
    print(f"Uploaded file: {file_id}")
    assistant = client.beta.assistants.create(
        name="C Programmer",
        instructions="You are an expert in C programming language. You are working on open-source projects, and you need to implement certain functions based on the prompts.",
        tools=[{"type": "code_interpreter"}],
        model="gpt-4o-2024-05-13",
    )
    assistant_id = assistant.id
    print(f"Created assistant: {assistant_id}")

    thread = openai.beta.threads.create()
    thread_id = thread.id

    openai.beta.threads.messages.create(
        thread_id=thread_id,
        role="user",
        content=updated_prompt,
        attachments=[
            {
                "file_id": file_id,
                "tools": [{"type": "code_interpreter"}]
            }
        ]
    )

    run = client.beta.threads.runs.create(
        thread_id=thread_id,
        assistant_id=assistant_id
    )


    while True:
        run_status = openai.beta.threads.runs.retrieve(
            thread_id=thread_id,
            run_id=run.id
        )
        if run_status.status == "completed":
            break
        elif run_status.status in ("failed", "expired", "cancelled"):
            raise RuntimeError(f"Run failed with status: {run_status.status}")
        time.sleep(2)

    messages = openai.beta.threads.messages.list(thread_id=thread_id)

    with open(destination_file, "w", encoding="utf-8") as f:
        for message in reversed(messages.data):
            for content in message.content:
                if content.type == "text":
                    f.write(content.text.value + "\n")

    deleted = openai.files.delete(file_id)
    print(f"Deleted file: {deleted}")

    print(upload.id, upload.filename, upload.purpose)
    DeleteAllFileLLM()


def RunLLM_WithoutUpload(commit, message, changed_function, change_file_dir):
    change_file_name = change_file_dir.split("/")[-1].split(".")[0]
    
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
    Please return only the updated definition of the function '{changed_function}' as a single C function. Do not include the full source file or any extra commentary.
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


def DeleteAllFileLLM():
    files = openai.files.list().data
    for f in files:
        print(f"Deleting {f.id} ({f.filename})...")
        openai.files.delete(f.id)

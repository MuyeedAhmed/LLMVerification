from openai import OpenAI
import csv
import shutil
import os

client = OpenAI()


def ask_chatgpt(fileId, projName, dirName, fileName, vectorStore, prompt):
    assistant = client.beta.assistants.create(
        name="C Programmer",
        instructions="You are an expert in C programming language. You are working on open-source projects, and you need to implement certain functions based on the prompts.",
        tools=[{"type": "file_search"}],
        model="gpt-4o-2024-05-13",
    )

    vector_store = client.beta.vector_stores.create(name=vectorStore)

    # Ready the files for upload to OpenAI
    file_paths = [
        os.path.join(projName, dirName, f)
        for f in os.listdir(os.path.join(projName, dirName))
        if f.endswith(".c") and f != "new.c" and f != "human.c"
    ]
    bufaux_path = os.path.join(projName, dirName, "reference.txt")
    if os.path.isfile(bufaux_path):
        file_paths.append(bufaux_path)
    print(file_paths)
    file_streams = [open(path, "rb") for path in file_paths]

    # Upload the files to the vector store
    file_batch = client.beta.vector_stores.file_batches.upload_and_poll(
        vector_store_id=vector_store.id, files=file_streams
    )

    # Print the status and the file counts of the batch to see the result of this operation.
    print(file_batch.status)
    print(file_batch.file_counts)

    assistant = client.beta.assistants.update(
        assistant_id=assistant.id,
        tool_resources={"file_search": {"vector_store_ids": [vector_store.id]}},
    )

    thread = client.beta.threads.create()
    message = client.beta.threads.messages.create(
        thread_id=thread.id,
        role="user",
        content=prompt,
    )

    run = client.beta.threads.runs.create_and_poll(
        thread_id=thread.id,
        assistant_id=assistant.id,
    )

    if run.status == "completed":
        messages = client.beta.threads.messages.list(thread_id=thread.id)
        print(f"Number of messages: {len(messages.data)}")
        question = messages.data[1].content[0].text.value
        print(f"Question: {question}")
        response = messages.data[0].content[0].text.value
        print(f"Response: {response}")
        file_name = f"chatgpt.c"
        with open(file_name, "w") as file:
            file.write(response)
        shutil.move(file_name, f"{projName}/{dirName}")
        with open(f"{projName}/{dirName}/prompt.txt", "a") as f:
            f.write(f"\n\nActual prompt:\n{prompt}")
        print(f"Generated C file: {file_name}")
    else:
        print(run.status)
    cleanup_assistant(assistant, vector_store, thread)

def cleanup_assistant(assistant, vector_store, thread):
    ass_response = client.beta.assistants.delete(assistant.id)
    print(ass_response)
    vec_response = client.beta.vector_stores.delete(vector_store_id=vector_store.id)
    print(vec_response)
    thread_response = client.beta.threads.delete(thread.id)
    print(thread_response)

if __name__ == "__main__":
    with open("bison_copy.csv", "r", newline="") as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            fileName = f"{row['Project']}-{row['Version (before changes)']}.c"
            files = row["File"]
            vectorStore = f"{row['Project']}_{row['Version (before changes)']}"
            dirName = row["Dirname"]
            task = row["Task"]
            prompt = ""
            if task == "New function":
                prompt = f"Implement a function in the provided {fileName}: {row['Function declaration']}, such that it {row['Function description']}. Provide only the functional implementation. Do not include any explaination or use case."
            else:
                prompt = f"Modify the function in the provided old.c: {row['Function declaration']}, such that it {row['Function description']}. Provide only the functional implementation. Do not include any explaination or use case."
                # prompt = f"Modify the function ({row['Function declaration']}) in the provided old.c such that it {row['Function description']}. Provide only the functional implementation. Do not include any explaination or use case."
                # prompt = f"Modify the function in the provided old.c such that it fixes the following issue: {row['Function description']}. Provide only the functional implementation/fix. Do not include any explaination or use case."
            # print(f"Files: {files},\n Dirname: {dirName},\n Prompt: {prompt}\n")
            ask_chatgpt(row['ID'],row['Project'], dirName, fileName, vectorStore, prompt)

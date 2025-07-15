import os
import shutil

base_dir = "FFmpeg_22"
filtered_dir = "FFmpegFiltered/"

dest_original = os.path.join(filtered_dir, "Original")
dest_human = os.path.join(filtered_dir, "Human")
dest_chatgpt = os.path.join(filtered_dir, "ChatGPT")

os.makedirs(dest_original, exist_ok=True)
os.makedirs(dest_human, exist_ok=True)
os.makedirs(dest_chatgpt, exist_ok=True)

for subfolder in os.listdir(base_dir):
    subfolder_path = os.path.join(base_dir, subfolder)
    if not os.path.isdir(subfolder_path):
        continue
    fixed_file = next((f for f in os.listdir(subfolder_path) if f.endswith('_llm.c')), None)
    if fixed_file:
        base_name = fixed_file.replace('_llm.c', '')

        original_file = f"{base_name}_original.c"
        human_file = f"{base_name}_human.c"

        fixed_path = os.path.join(subfolder_path, fixed_file)
        original_path = os.path.join(subfolder_path, original_file)
        human_path = os.path.join(subfolder_path, human_file)

        if os.path.exists(original_path):
            shutil.copyfile(original_path, os.path.join(dest_original, f"{subfolder}.c"))
        if os.path.exists(human_path):
            shutil.copyfile(human_path, os.path.join(dest_human, f"{subfolder}.c"))
        if os.path.exists(fixed_path):
            shutil.copyfile(fixed_path, os.path.join(dest_chatgpt, f"{subfolder}.c"))


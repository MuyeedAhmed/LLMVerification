
import os
import shutil

base_dir = "FFmpeg"

llm = "qwen3"

for subfolder in os.listdir(base_dir):
    subfolder_path = os.path.join(base_dir, subfolder)
    if not os.path.isdir(subfolder_path):
        continue
    original_file = next((f for f in os.listdir(subfolder_path) if f.endswith(f'_{llm}.c')), None)
    renamed_file = original_file.replace(f'_{llm}.c', f'_{llm}_function.c')

    if original_file:
        os.rename(os.path.join(subfolder_path, original_file), os.path.join(subfolder_path, renamed_file))


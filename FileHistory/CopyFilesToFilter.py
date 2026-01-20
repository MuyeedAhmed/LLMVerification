import os
import shutil

base_dir = "FFmpeg"
filtered_dir = "FFmpegFiltered/"
llm = "ministral"

dest_original = os.path.join(filtered_dir, "Original")
dest_human = os.path.join(filtered_dir, "Human")
dest_llm = os.path.join(filtered_dir, llm)

os.makedirs(dest_original, exist_ok=True)
os.makedirs(dest_human, exist_ok=True)
os.makedirs(dest_llm, exist_ok=True)

for subfolder in os.listdir(base_dir):
    subfolder_path = os.path.join(base_dir, subfolder)
    if not os.path.isdir(subfolder_path):
        continue
    fixed_file = next((f for f in os.listdir(subfolder_path) if f.endswith(f'_{llm}.c')), None)
    
    if fixed_file:
        base_name = fixed_file.replace(f'_{llm}.c', '')
    elif fixed_file is None:
        fixed_file = next((f for f in os.listdir(subfolder_path) if f.endswith(f'_{llm}_function.c')), None)
        if fixed_file:
            base_name = fixed_file.replace(f'_{llm}_function.c', '')
        else:
            print(f"No LLM file found in {subfolder_path}, skipping.")
            continue
        
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
        shutil.copyfile(fixed_path, os.path.join(dest_llm, f"{subfolder}.c"))


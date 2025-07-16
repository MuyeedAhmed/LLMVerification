import os
import shutil

def duplicate_original_files(root_dir="FileHistory/FFmpeg"):
    for subdir, _, files in os.walk(root_dir):
        for file in files:
            if file.endswith("_original.c"):
                original_path = os.path.join(subdir, file)
                base_name = file[:-len("_original.c")]
                copied_name = base_name + "_llm.c"
                copied_path = os.path.join(subdir, copied_name)
                
                shutil.copyfile(original_path, copied_path)
                print(f"Copied: {original_path} -> {copied_path}")

if __name__ == "__main__":
    duplicate_original_files()
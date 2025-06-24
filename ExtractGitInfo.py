import subprocess
import shutil, os
import re

class ExtractGitInfo:
    def __init__(self, github_link, repo_path):
        self.github_link = github_link
        self.repo_path = repo_path

    def gitCloneRepo(self):
        try:
            subprocess.run(['git', 'clone', self.github_link, self.repo_path], check=True)
            print(f"Repository cloned to {self.repo_path}")
        except subprocess.CalledProcessError as e:
            print(f"Error cloning repository: {e}")
    
    def gitCheckoutMaster(self):
        try:
            subprocess.run(['git', 'checkout', 'master'], cwd=self.repo_path, check=True)
        except subprocess.CalledProcessError as e:
            try:
                subprocess.run(['git', 'checkout', 'main'], cwd=self.repo_path, check=True)
            except subprocess.CalledProcessError as e:
                print("Neither master nor main branch found. Please check the repository.")
                raise e
                
    def GetCommitsByDate(self, since_date, until_date, commit_count=100):
        try:
            log_result = subprocess.run(
                ["git", "log", f"--since={since_date}", f"--until={until_date}", "-n", f"{commit_count}", "--format=%H"],
                cwd=self.repo_path,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=True
            )
            commit_hashes = log_result.stdout.strip().splitlines()
            return commit_hashes
        except subprocess.CalledProcessError as e:
            print(f"Error fetching commits: {e.stderr}")
            return []
    
    def GetCommitDate(self, commit_hash):
        try:
            date_result = subprocess.run(
                ["git", "show", "-s", "--format=%cd", "--date=short", commit_hash],
                cwd=self.repo_path,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=True
            )
            return date_result.stdout.strip()
        except subprocess.CalledProcessError as e:
            print(f"Error fetching commit date for {commit_hash}: {e.stderr}")
            return None
    
    def GetCommitMessages(self, commit_hash):
        try:
            message_result = subprocess.run(
                ["git", "log", "-1", "--format=%s", commit_hash],
                cwd=self.repo_path,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=True
            )
            return message_result.stdout.strip()
        except subprocess.CalledProcessError as e:
            print(f"Error fetching commit message for {commit_hash}: {e.stderr}")
            return None
    
    def GetChangedFiles(self, commit_hash):
        try:
            show_result = subprocess.run(
                ["git", "show", "--name-only", "--format=", commit_hash],
                cwd=self.repo_path,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=True
            )
            changed_files = list(filter(None, show_result.stdout.strip().splitlines()))
            return changed_files
        except subprocess.CalledProcessError as e:
            print(f"Error fetching changed files for {commit_hash}: {e.stderr}")
            return []
    
    def GetParentCommit(self, commit_hash):
        try:
            parent_result = subprocess.run(
                ["git", "rev-parse", f"{commit_hash}^"],
                cwd=self.repo_path,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=True
            )
            return parent_result.stdout.strip()
        except subprocess.CalledProcessError as e:
            print(f"Error fetching parent commit for {commit_hash}: {e.stderr}")
            return None

    def CopyChangedFilesFromCommit(self, commit_hash, destination_dir):
        changed_files = self.GetChangedFiles(commit_hash)
        if not changed_files:
            print(f"No changed files found for {commit_hash}")
            return
        try:
            for file in changed_files:
                filename = os.path.basename(file)
                name, ext = os.path.splitext(filename)
                source_path = f"{self.repo_path}/{file}"
                if ext != ".c":
                    continue
                if os.path.exists(source_path):
                    original_copy = os.path.join(destination_dir, f"{name}_human{ext}")
                    print(f"Copying changed file: {source_path} to {original_copy}")
                    shutil.copyfile(source_path, original_copy)
        except subprocess.CalledProcessError as e:
            print(f"Error copying changed files: {e}")

    def CopyChangedFilesFromParent(self, commit_hash, destination_dir):
        parent_commit = self.GetParentCommit(commit_hash)
        if not parent_commit:
            print(f"No parent commit found for {commit_hash}")
            return
        
        try:
            subprocess.run(["git", "checkout", parent_commit], cwd=self.repo_path, check=True)
            changed_files = self.GetChangedFiles(commit_hash)
            
            for file in changed_files:
                filename = os.path.basename(file)
                name, ext = os.path.splitext(filename)
                if ext != ".c":
                    continue
                source_path = f"{self.repo_path}/{file}"
                destination_path = f"{destination_dir}/{filename}"
                if os.path.exists(source_path):
                    original_copy = os.path.join(destination_dir, f"{name}_original{ext}")
                    shutil.copyfile(source_path, original_copy)
                else:
                    print(f"File {source_path} does not exist. Skipping copy.")
        except subprocess.CalledProcessError as e:
            print(f"Error copying changed files: {e}")

    def GetChangedFunctions(self, commit_hash):
        try:
            diff_result = subprocess.run(
                ["git", "diff", commit_hash + "^!", "--unified=0"],
                cwd=self.repo_path,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=True
            )
            diff_result_strip = diff_result.stdout.strip()
            diff_output = diff_result.stdout
            hunk_header_pattern = re.compile(r'^@@ .*@@\s+(.*)$', re.MULTILINE)

            functions = set()
            for match in hunk_header_pattern.finditer(diff_output):
                line = match.group(1).strip()
                # Heuristically extract the function name from a C declaration
                func_match = re.match(r'(?:static\s+)?(?:inline\s+)?[^\(\)]+\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\(', line)
                if func_match:
                    functions.add(func_match.group(1))
                    
            return diff_result.stdout.strip(), list(functions)
        except subprocess.CalledProcessError as e:
            print(f"Error fetching changed functions for {commit_hash}: {e.stderr}")
            return None

from pathlib import Path
import re
import subprocess
import os

TEMPLATE_DIR = Path("C:/Users/James/Documents/synth_stuff/DaisyExamples/pod/JamesTemplate")

assert TEMPLATE_DIR.exists()

# Make sure we're in the pod directory
if not os.getcwd().endswith("C:/Users/James/Documents/synth_stuff/DaisyExamples/pod"):
    print("Moving to the pod directory...")
    os.chdir("C:/Users/James/Documents/synth_stuff/DaisyExamples/pod")

# get new project name from arg used in python command invocation, --project_name <name>
import sys

def get_new_project_name():
    if "--project_name" in sys.argv:
        project_name_index = sys.argv.index("--project_name") + 1
        if project_name_index < len(sys.argv):
            return sys.argv[project_name_index]
    else:
        raise ValueError("No project name provided. Use --project_name <name> to specify a project name.")

def replace_in_file(file_path, old_str, new_str):
    with open(file_path, 'r') as file:
        content = file.read()
    content = content.replace(old_str, new_str)
    with open(file_path, 'w') as file:
        file.write(content)
    print(f"Replaced '{old_str}' with '{new_str}' in {file_path}")

# Get the new project name from command line arguments
new_project_name = get_new_project_name()
# Camel Case the new project name only if it contains hyphens, underscores, or spaces

if re.search(r'[-_ ]', new_project_name):
    words = re.split(r'[-_ ]+', new_project_name)
    new_project_name = ''.join(word.capitalize() for word in words)

#Copy all the template files to the new project directory using pathlib
new_project_dir = TEMPLATE_DIR.parent / new_project_name

#Raise a value error if the new project directory already exists
if new_project_dir.exists():
    raise ValueError(f"The project directory '{new_project_dir}' already exists. Please choose a different project name.")  

if not new_project_dir.exists():
    new_project_dir.mkdir(parents=True)
for file in TEMPLATE_DIR.glob("**/*"):
    # Skip any files in the 'build' directory
    if file.is_file() and 'build' not in file.parts:
        new_file_path = new_project_dir / file.relative_to(TEMPLATE_DIR)
        new_file_path.parent.mkdir(parents=True, exist_ok=True)  # Ensure parent directories exist
        with open(file, 'r') as src_file:
            content = src_file.read()
        with open(new_file_path, 'w') as dest_file:
            dest_file.write(content.replace("JamesTemplate", new_project_name))
        print(f"Copied {file} to {new_file_path}")

# Replace all instances of "JamesTemplate" in the new project directory with the new project name
for file in new_project_dir.glob("**/*"):
    if file.is_file():
        replace_in_file(file, "JamesTemplate", new_project_name)

# Rename JamesTemplate.cpp to the new project name
cpp_file = Path(new_project_dir / "JamesTemplate.cpp")
cpp_file_new = cpp_file.with_name(f"{new_project_name}.cpp")
if cpp_file.exists():   
    cpp_file.rename(cpp_file_new)
    print(f"Renamed {cpp_file} to {cpp_file_new}")
# subprocess.run(["code", str(new_project_dir)])
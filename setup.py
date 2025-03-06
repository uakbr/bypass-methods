import os
import sys
import subprocess
import shutil
from pathlib import Path

def print_header(text):
    """Print a formatted header."""
    print("\n" + "=" * 60)
    print(" " * 5 + text)
    print("=" * 60 + "\n")

def check_python_version():
    """Check if Python version is 3.6 or higher."""
    major, minor = sys.version_info[:2]
    if major < 3 or (major == 3 and minor < 6):
        print("Error: Python 3.6 or higher is required")
        print(f"Current Python version: {major}.{minor}")
        return False
    return True

def install_requirements():
    """Install required packages."""
    print_header("Installing required packages")
    
    # Define the requirements
    requirements = [
        "keyboard==0.13.5",
        "Pillow==10.2.0",
        "psutil==5.9.6",
        "comtypes==1.1.14",
        "pywin32==306",
    ]
    
    # Create requirements.txt file
    with open("requirements_accessibility.txt", "w") as req_file:
        req_file.write("\n".join(requirements))
    
    # Install requirements
    try:
        subprocess.check_call([sys.executable, "-m", "pip", "install", "-r", "requirements_accessibility.txt"])
        print("✓ Required packages installed successfully")
        return True
    except subprocess.CalledProcessError as e:
        print(f"Error installing required packages: {e}")
        return False

def setup_virtual_environment():
    """Set up a virtual environment."""
    print_header("Setting up virtual environment")
    
    venv_dir = "venv"
    if os.path.exists(venv_dir):
        print(f"Virtual environment '{venv_dir}' already exists")
        return True
    
    try:
        subprocess.check_call([sys.executable, "-m", "venv", venv_dir])
        print(f"✓ Virtual environment created at '{venv_dir}'")
        
        # Get the path to the virtual environment's Python executable
        if sys.platform == "win32":
            venv_python = os.path.join(venv_dir, "Scripts", "python.exe")
        else:
            venv_python = os.path.join(venv_dir, "bin", "python")
        
        # Install requirements in the virtual environment
        subprocess.check_call([venv_python, "-m", "pip", "install", "--upgrade", "pip"])
        subprocess.check_call([venv_python, "-m", "pip", "install", "-r", "requirements_accessibility.txt"])
        
        print(f"✓ Required packages installed in virtual environment")
        return True
    except subprocess.CalledProcessError as e:
        print(f"Error setting up virtual environment: {e}")
        return False

def create_launcher():
    """Create launcher script for the application."""
    print_header("Creating launcher script")
    
    launcher_content = """@echo off
echo Starting UndownUnlock Accessibility Controller...
echo.

:: Activate virtual environment
call venv\\Scripts\\activate.bat

:: Run the application
python accessibility_controller.py

:: Deactivate virtual environment when done
call venv\\Scripts\\deactivate.bat
"""
    
    with open("launch.bat", "w") as launcher_file:
        launcher_file.write(launcher_content)
    
    print("✓ Launcher script 'launch.bat' created")
    return True

def main():
    """Main setup function."""
    print_header("UndownUnlock Accessibility Setup")
    
    # Check Python version
    if not check_python_version():
        return False
    
    # Install required packages
    if not install_requirements():
        return False
    
    # Set up virtual environment
    if not setup_virtual_environment():
        return False
    
    # Create launcher script
    if not create_launcher():
        return False
    
    print_header("Setup Completed Successfully")
    print("To start the application, run:")
    print("  - On Windows: Double-click 'launch.bat' or run it from the command line")
    print("\nThe following keyboard shortcuts are available when the application is running:")
    print("  Ctrl+Tab: Cycle to next window")
    print("  Ctrl+Shift+Tab: Cycle to previous window")
    print("  Ctrl+L: Focus LockDown Browser")
    print("  Ctrl+O: Focus other window")
    print("  Ctrl+Shift+S: Take screenshot")
    print("  Ctrl+M: Minimize all except LockDown Browser")
    print("  Ctrl+R: Restore all windows")
    print("\nPress Ctrl+C in the terminal to exit the application")
    
    return True

if __name__ == "__main__":
    success = main()
    if not success:
        print("\nSetup failed. Please check the errors above and try again.")
        sys.exit(1) 
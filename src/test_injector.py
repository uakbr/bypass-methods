#!/usr/bin/env python3
import os
import sys
import time
import argparse
import psutil
import ctypes
from ctypes import wintypes

# Windows API constants and functions
PROCESS_ALL_ACCESS = 0x1F0FFF
VIRTUAL_MEM = 0x1000 | 0x2000  # MEM_COMMIT | MEM_RESERVE
PAGE_READWRITE = 0x04

# Load Windows API functions
kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)

# Define function prototypes
kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
kernel32.OpenProcess.restype = wintypes.HANDLE

kernel32.VirtualAllocEx.argtypes = [wintypes.HANDLE, wintypes.LPVOID, ctypes.c_size_t, wintypes.DWORD, wintypes.DWORD]
kernel32.VirtualAllocEx.restype = wintypes.LPVOID

kernel32.WriteProcessMemory.argtypes = [wintypes.HANDLE, wintypes.LPVOID, wintypes.LPCVOID, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
kernel32.WriteProcessMemory.restype = wintypes.BOOL

kernel32.GetProcAddress.argtypes = [wintypes.HMODULE, wintypes.LPCSTR]
kernel32.GetProcAddress.restype = wintypes.LPVOID

kernel32.GetModuleHandleA.argtypes = [wintypes.LPCSTR]
kernel32.GetModuleHandleA.restype = wintypes.HMODULE

kernel32.CreateRemoteThread.argtypes = [wintypes.HANDLE, wintypes.LPVOID, ctypes.c_size_t, wintypes.LPVOID, wintypes.LPVOID, wintypes.DWORD, wintypes.LPDWORD]
kernel32.CreateRemoteThread.restype = wintypes.HANDLE

kernel32.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
kernel32.WaitForSingleObject.restype = wintypes.DWORD

def inject_dll(pid, dll_path):
    """
    Inject a DLL into a target process.
    
    Args:
        pid: Process ID of the target process
        dll_path: Path to the DLL to inject
    
    Returns:
        True if successful, False otherwise
    """
    # Get the full path to the DLL
    dll_path = os.path.abspath(dll_path)
    
    # Check if the DLL exists
    if not os.path.exists(dll_path):
        print(f"Error: DLL not found at {dll_path}")
        return False
    
    # Open the target process
    h_process = kernel32.OpenProcess(PROCESS_ALL_ACCESS, False, pid)
    if not h_process:
        print(f"Error: Failed to open process {pid}")
        return False
    
    try:
        # Allocate memory for the DLL path in the target process
        dll_path_bytes = (dll_path.encode('ascii') + b'\0')
        address = kernel32.VirtualAllocEx(h_process, None, len(dll_path_bytes), VIRTUAL_MEM, PAGE_READWRITE)
        if not address:
            print("Error: Failed to allocate memory in target process")
            return False
        
        # Write the DLL path to the allocated memory
        written = ctypes.c_size_t()
        result = kernel32.WriteProcessMemory(h_process, address, dll_path_bytes, len(dll_path_bytes), ctypes.byref(written))
        if not result or written.value != len(dll_path_bytes):
            print("Error: Failed to write to process memory")
            return False
        
        # Get the address of LoadLibraryA
        h_kernel32 = kernel32.GetModuleHandleA(b"kernel32.dll")
        h_loadlib = kernel32.GetProcAddress(h_kernel32, b"LoadLibraryA")
        if not h_loadlib:
            print("Error: Failed to get address of LoadLibraryA")
            return False
        
        # Create a remote thread that calls LoadLibraryA with the DLL path
        thread_id = wintypes.DWORD()
        h_thread = kernel32.CreateRemoteThread(h_process, None, 0, h_loadlib, address, 0, ctypes.byref(thread_id))
        if not h_thread:
            print("Error: Failed to create remote thread")
            return False
        
        # Wait for the thread to complete
        kernel32.WaitForSingleObject(h_thread, 0xFFFFFFFF)  # INFINITE
        
        # Clean up
        kernel32.CloseHandle(h_thread)
        
        print(f"Successfully injected {dll_path} into process {pid}")
        return True
        
    finally:
        # Close the process handle
        kernel32.CloseHandle(h_process)

def find_process_by_name(process_name):
    """
    Find a process by name.
    
    Args:
        process_name: Name of the process to find
    
    Returns:
        Process ID if found, None otherwise
    """
    for proc in psutil.process_iter(['pid', 'name']):
        if process_name.lower() in proc.info['name'].lower():
            return proc.info['pid']
    return None

def list_processes():
    """List all processes with their PIDs."""
    print("Available processes:")
    print("PID\tName")
    print("-" * 50)
    for proc in psutil.process_iter(['pid', 'name']):
        print(f"{proc.info['pid']}\t{proc.info['name']}")

def main():
    parser = argparse.ArgumentParser(description="Inject the UndownUnlock DirectX Hook DLL into a target process")
    parser.add_argument("--pid", type=int, help="Process ID of the target process")
    parser.add_argument("--name", type=str, help="Name of the target process")
    parser.add_argument("--dll", type=str, default=".\\bin\\UndownUnlockDXHook.dll", help="Path to the DLL to inject")
    parser.add_argument("--wait", action="store_true", help="Wait for the process to start if not found")
    parser.add_argument("--list", action="store_true", help="List available processes")
    
    args = parser.parse_args()
    
    # List processes if requested
    if args.list:
        list_processes()
        return 0
    
    # Get the target process ID
    pid = args.pid
    if not pid and args.name:
        pid = find_process_by_name(args.name)
        
        if not pid and args.wait:
            print(f"Waiting for process {args.name} to start...")
            while not pid:
                time.sleep(1)
                pid = find_process_by_name(args.name)
    
    if not pid:
        print("Error: No target process specified or found")
        return 1
    
    # Inject the DLL
    success = inject_dll(pid, args.dll)
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main()) 
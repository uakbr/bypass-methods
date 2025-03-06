import os
import sys
import time
import logging
import keyboard
import threading
from PIL import ImageGrab, Image
from datetime import datetime

# Import the accessibility manager
from accessibility_manager import AccessibilityManager

# Set up logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler("accessibility_controller.log"),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger("AccessibilityController")

class AccessibilityController:
    """
    Controller for the Accessibility-based window management system.
    Provides keyboard shortcuts for managing window focus and captures.
    """
    
    def __init__(self):
        """Initialize the controller with an accessibility manager."""
        self.accessibility_manager = AccessibilityManager()
        self.running = False
        self.keyboard_thread = None
        self.target_app_name = "LockDown Browser"
        self.alt_windows = []
        self.screenshot_dir = "screenshots"
        
        # Ensure screenshot directory exists
        if not os.path.exists(self.screenshot_dir):
            os.makedirs(self.screenshot_dir)
    
    def start(self):
        """Start the controller and accessibility services."""
        if self.running:
            return
            
        logger.info("Starting accessibility controller...")
        
        # Start the accessibility manager
        self.accessibility_manager.start()
        
        # Set up keyboard hooks
        self.setup_keyboard_hooks()
        
        # Start keyboard listener in a thread
        self.running = True
        self.keyboard_thread = threading.Thread(target=self.keyboard_listener)
        self.keyboard_thread.daemon = True
        self.keyboard_thread.start()
        
        logger.info("Accessibility controller started.")
    
    def stop(self):
        """Stop the controller and accessibility services."""
        if not self.running:
            return
            
        logger.info("Stopping accessibility controller...")
        
        # Stop the keyboard listener
        self.running = False
        if self.keyboard_thread:
            self.keyboard_thread.join(timeout=1.0)
        
        # Remove all keyboard hooks
        self.remove_keyboard_hooks()
        
        # Stop the accessibility manager
        self.accessibility_manager.stop()
        
        logger.info("Accessibility controller stopped.")
    
    def setup_keyboard_hooks(self):
        """Set up keyboard hotkeys for controlling the application."""
        try:
            # Window cycling
            keyboard.add_hotkey('ctrl+tab', self.cycle_next_window)
            keyboard.add_hotkey('ctrl+shift+tab', self.cycle_prev_window)
            
            # Focus LockDown Browser (when specified)
            keyboard.add_hotkey('ctrl+l', self.focus_lockdown_browser)
            
            # Focus other window
            keyboard.add_hotkey('ctrl+o', self.focus_other_window)
            
            # Screenshot functionality
            keyboard.add_hotkey('ctrl+shift+s', self.take_screenshot)
            
            # Minimize all except target app
            keyboard.add_hotkey('ctrl+m', self.minimize_all_except_target)
            
            # Restore all windows
            keyboard.add_hotkey('ctrl+r', self.restore_all_windows)
            
            logger.info("Keyboard hooks set up successfully.")
        except Exception as e:
            logger.error(f"Failed to set up keyboard hooks: {e}")
    
    def remove_keyboard_hooks(self):
        """Remove all keyboard hooks."""
        try:
            # Clear all keyboard hooks
            keyboard.unhook_all()
            logger.info("Keyboard hooks removed.")
        except Exception as e:
            logger.error(f"Failed to remove keyboard hooks: {e}")
    
    def keyboard_listener(self):
        """Background thread for keyboard event processing."""
        try:
            while self.running:
                time.sleep(0.1)  # Prevent high CPU usage
        except Exception as e:
            logger.error(f"Error in keyboard listener: {e}")
        finally:
            logger.info("Keyboard listener stopped.")
    
    def cycle_next_window(self):
        """Cycle to the next window."""
        try:
            logger.info("Cycling to next window")
            self.accessibility_manager.cycle_windows("next")
        except Exception as e:
            logger.error(f"Error cycling to next window: {e}")
    
    def cycle_prev_window(self):
        """Cycle to the previous window."""
        try:
            logger.info("Cycling to previous window")
            self.accessibility_manager.cycle_windows("previous")
        except Exception as e:
            logger.error(f"Error cycling to previous window: {e}")
    
    def focus_lockdown_browser(self):
        """Focus the LockDown Browser window if it exists."""
        try:
            logger.info("Attempting to focus LockDown Browser")
            lockdown_window = self.accessibility_manager.find_window_by_name(self.target_app_name, True)
            if lockdown_window:
                self.accessibility_manager.set_window_focus(lockdown_window)
                logger.info("LockDown Browser focused")
            else:
                logger.info("LockDown Browser window not found")
        except Exception as e:
            logger.error(f"Error focusing LockDown Browser: {e}")
    
    def focus_other_window(self):
        """Focus another window from the alt windows list."""
        try:
            # Get all windows
            windows = self.accessibility_manager.get_all_windows()
            
            # Filter out the LockDown Browser
            filtered_windows = [w for w in windows if self.target_app_name not in getattr(w, 'CurrentName', '')]
            
            if filtered_windows:
                # Focus the first non-LockDown Browser window
                self.accessibility_manager.set_window_focus(filtered_windows[0])
                logger.info(f"Focused other window: {getattr(filtered_windows[0], 'CurrentName', 'Unknown')}")
            else:
                logger.info("No other windows found to focus")
        except Exception as e:
            logger.error(f"Error focusing other window: {e}")
    
    def take_screenshot(self):
        """Take a screenshot of the current screen."""
        try:
            logger.info("Taking screenshot")
            
            # Generate a filename with timestamp
            timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
            filename = os.path.join(self.screenshot_dir, f"screenshot_{timestamp}.png")
            
            # Take the screenshot
            screenshot = ImageGrab.grab()
            screenshot.save(filename)
            
            logger.info(f"Screenshot saved to {filename}")
        except Exception as e:
            logger.error(f"Error taking screenshot: {e}")
    
    def minimize_all_except_target(self):
        """Minimize all windows except the target application."""
        try:
            logger.info(f"Minimizing all windows except {self.target_app_name}")
            self.accessibility_manager.minimize_all_windows_except(self.target_app_name)
        except Exception as e:
            logger.error(f"Error minimizing windows: {e}")
    
    def restore_all_windows(self):
        """Restore all previously minimized windows."""
        try:
            logger.info("Restoring all windows")
            self.accessibility_manager.restore_all_windows()
        except Exception as e:
            logger.error(f"Error restoring windows: {e}")


if __name__ == "__main__":
    try:
        # Create and start the controller
        controller = AccessibilityController()
        controller.start()
        
        print("UndownUnlock Accessibility Controller started")
        print("Use the following keyboard shortcuts:")
        print("  Ctrl+Tab: Cycle to next window")
        print("  Ctrl+Shift+Tab: Cycle to previous window")
        print("  Ctrl+L: Focus LockDown Browser")
        print("  Ctrl+O: Focus other window")
        print("  Ctrl+Shift+S: Take screenshot")
        print("  Ctrl+M: Minimize all except LockDown Browser")
        print("  Ctrl+R: Restore all windows")
        print("Press Ctrl+C to exit")
        
        # Keep the main thread alive
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nExiting...")
    finally:
        # Clean up
        if 'controller' in locals():
            controller.stop() 
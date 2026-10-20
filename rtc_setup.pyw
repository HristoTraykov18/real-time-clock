"""
Real Time Clock file setup and build launcher.

Prepares a self contained, compile ready copy of the project in Real_Time_Clock_Compile and
drives PlatformIO against that copy, so the sources in the repository are never modified.
The preparation itself and the message block live in rtc_setup_utils.

Run from the project root:  pythonw rtc_setup.pyw
"""

import importlib.util
import os
import re
import shlex
import shutil
import subprocess
import sys
import tkinter as tk
from tkinter import ttk

from rtc_setup_utils import PROJECT_FILE, CompileFolder, StatusArea


CREATE_NEW_CONSOLE = 0x00000010  # Opens the spawned command in its own console window on Windows


def command_chain(steps):
    """
    Assemble the single shell line that runs every step inside one terminal window.

    `steps` is a list of (prompt, command) pairs, run in order. A step with a prompt is offered
    only when everything before it succeeded, and runs only when the answer is not N, so a failed
    build never reaches the upload. A step without a prompt runs as soon as the one before it
    succeeded. Built from the last step backwards, because each prompt has to wrap all that follows
    """
    is_windows = os.name == "nt"
    quote = subprocess.list2cmdline if is_windows else shlex.join
    chain = ""

    for prompt, command in reversed(steps):
        step = quote(command) + (f" && {chain}" if chain else "")

        if prompt is not None:
            step = (f'(choice /c YN /n /m "{prompt} [Y/N] " & if not errorlevel 2 ({step}))'
                    if is_windows else
                    f'{{ read -p "{prompt} [Y/n] " reply; [ "$reply" != n ] && '
                    f'[ "$reply" != N ]; }} && {{ {step}; }}')

        chain = step

    return chain


def find_platformio():
    """
    Locate PlatformIO Core and return the command prefix that launches it.
    Returns None when Core cannot be found.
    """
    for executable_name in ("platformio", "pio"):
        found = shutil.which(executable_name)

        if found:
            return [found]

    # Virtual environment created by the PlatformIO IDE installer
    core_dir = os.environ.get("PLATFORMIO_CORE_DIR") or \
        os.path.join(os.path.expanduser("~"), ".platformio")
    penv_path = os.path.join(core_dir, "penv",
                             "Scripts" if os.name == "nt" else "bin",
                             "platformio.exe" if os.name == "nt" else "platformio")

    if os.path.isfile(penv_path):
        return [penv_path]

    # Installed with pip into the interpreter running this script
    if importlib.util.find_spec("platformio") is not None:
        interpreter = sys.executable

        # A .pyw runs under pythonw, which has no console and would swallow the output
        if os.name == "nt" and os.path.basename(interpreter).lower() == "pythonw.exe":
            interpreter = os.path.join(os.path.dirname(interpreter), "python.exe")

        return [interpreter, "-m", "platformio"]

    return None


def read_environments():
    """ Read the PlatformIO environment names, so the dropdown never drifts from the ini """
    try:
        with open(PROJECT_FILE, "r", encoding="utf8") as current_file:
            return re.findall(r"^\[env:(.+?)\]", current_file.read(), re.MULTILINE)
    except OSError:
        return []


def spawn_terminal(title, steps):
    """
    Run the steps in one terminal window that stays open until it is dismissed.
    Returns the Popen of the terminal itself, so the caller can tell when it is closed
    """
    chain = command_chain(steps)

    if os.name == "nt":
        # Passed as a single string, because a list would be run through list2cmdline, which
        # escapes the inner quotes as \" and cmd.exe reads those backslashes literally
        return subprocess.Popen(f'cmd /c "title {title} & {chain} & echo. & pause"',
                                creationflags=CREATE_NEW_CONSOLE)

    for terminal in (["x-terminal-emulator", "-e"], ["gnome-terminal", "--"], ["xterm", "-e"]):
        if shutil.which(terminal[0]):
            return subprocess.Popen(
                terminal + ["bash", "-c", f"{chain}; echo; read -p 'Press enter to close'"])

    return subprocess.Popen(chain, shell=True)  # No terminal emulator available, run detached


class SetupApp:
    """ The setup window. Collects the build options, then drives CompileFolder and PlatformIO """

    ICON_FILE = os.path.join("data", "neonLogoIcon.ico")
    LAST_ID_FILE = "last_id.txt"

    TEXT_FONT = "Verdana 8 bold"
    BUTTON_WIDTH = 28  # Fits the longest caption
    PROCESS_POLL_INTERVAL = 500  # How often the spawned terminal is checked, in milliseconds

    def __init__(self):
        self.environments = read_environments()
        self.timezone_value = 2  # Default timezone offset (+2)
        self.clock_id_value = None
        self.action_buttons = []
        self.running_process = None  # Popen of the terminal a build or upload runs in
        self.pio_command = None

        self.root = tk.Tk()
        self.root.resizable(0, 0)
        self.root.title("Real Time Clock setup")
        self.root.option_add("*Font", self.TEXT_FONT)

        self.is_production_setup = tk.IntVar()
        self.is_edit_id = tk.IntVar(value=0)
        self.is_auto_monitor = tk.IntVar(value=1)
        self.selected_env = tk.StringVar(
            value=self.environments[0] if self.environments else "")

        try:
            self.root.iconbitmap(self.ICON_FILE)
        except tk.TclError:
            pass

        self.set_app_widgets()
        self.center_window()

    def adjust_clock_id(self, delta):
        """ Increment or decrement the clock ID value, clamped to 1-99999 """
        self.clock_id_value = max(1, min(99999, self.clock_id_value + delta))
        self.id_display.config(text=f"{self.clock_id_value:05d}")

    def adjust_timezone(self, delta):
        """ Increment or decrement the timezone value, wrapping at the +/-12 boundary """
        self.timezone_value += delta

        if self.timezone_value > 12:
            self.timezone_value = -12
        elif self.timezone_value < -12:
            self.timezone_value = 12

        display = f"+{self.timezone_value}" if self.timezone_value >= 0 else str(self.timezone_value)
        self.timezone_display.config(text=display)

    def center_window(self):
        """ Size the window to its content and center it on the screen """
        self.root.update_idletasks()
        width = max(self.root.winfo_reqwidth(), 400)
        height = self.root.winfo_reqheight()
        pos_x = (self.root.winfo_screenwidth() - width) // 2
        pos_y = (self.root.winfo_screenheight() - height) // 2

        self.root.wm_minsize(width=width, height=height)
        self.root.geometry(f"{width}x{height}+{pos_x}+{pos_y}")

    def create_stepper_row(self, text, initial_value, display_width, adjust):
        """
        Build a [caption] [-] [value] [+] row and return it together with its value label.
        The clock ID and timezone rows differ only in caption, width and callback
        """
        frame = tk.Frame(self.root)
        tk.Label(frame, text=text).pack(side=tk.LEFT, padx=(0, 4))
        tk.Button(frame, text="-", width=2, command=lambda: adjust(-1)).pack(side=tk.LEFT)
        display = tk.Label(frame, text=initial_value, width=display_width,
                           relief=tk.SUNKEN, anchor="center")
        display.pack(side=tk.LEFT, padx=2)
        tk.Button(frame, text="+", width=2, command=lambda: adjust(1)).pack(side=tk.LEFT)

        return frame, display

    def launch_terminal_task(self, title, steps):
        """
        Run PlatformIO steps in one terminal and lock the action buttons until it closes.
        `steps` is a list of (prompt, arguments) pairs, each one missing the Core command prefix
        """
        if self.pio_command is None:  # Kept once found, looked up again while it is missing
            self.pio_command = find_platformio()

        if self.pio_command is None:
            self.status.show("PlatformIO Core was not found\nInstall it, or add pio to PATH")

            return False

        self.running_process = spawn_terminal(
            title, [(prompt, self.pio_command + arguments) for prompt, arguments in steps])
        self.set_action_buttons_enabled(False)
        self.watch_running_process()

        return True

    def monitor_arguments(self):
        """ PlatformIO arguments that open the serial monitor for the selected environment """
        return ["device", "monitor", "-d", os.path.abspath("."), "-e", self.selected_env.get()]

    def prepare_compile_folder(self, prepare_step):
        """
        Refresh the part of the compile folder that one build needs, before that build starts.
        `prepare_step` is the CompileFolder method that copies and edits the files of that build.
        Appends its progress to the status area and returns False when the copy was not made
        """
        if not self.environments:
            self.status.show(f"No environments found in {PROJECT_FILE}")

            return False

        is_edit_id = self.is_edit_id.get() == 1
        folder = CompileFolder(environment=self.selected_env.get(),
                               timezone=self.timezone_value,
                               clock_id=self.clock_id_value if is_edit_id else None,
                               production=self.is_production_setup.get() == 1)

        try:
            for line in prepare_step(folder):
                self.status += line
        except Exception as error:
            self.status.show(f"Failed to prepare the compile folder\n{error}")

            return False

        # The ID is written into Globals.h, so only a firmware build consumes one
        if is_edit_id and prepare_step is CompileFolder.prepare_firmware:
            self.save_and_increment_clock_id()

        return True

    def run_arguments(self, *targets):
        """ PlatformIO run arguments against the compile folder, extended with the given targets """
        arguments = ["run", "-d", os.path.abspath(CompileFolder.COMPILE_FOLDER),
                     "-e", self.selected_env.get()]

        for target in targets:
            arguments += ["-t", target]

        return arguments

    def run_build_and_upload(self, title, prepare_step, build_targets, upload_targets):
        """
        Refresh the files this build needs, then build and offer the upload in the same terminal.
        A failed build short circuits the chain, so the upload is never offered. With Auto-Monitor
        ticked the serial monitor follows the upload in that same terminal
        """
        self.status.clear()
        environment = self.selected_env.get()

        if not environment:
            self.status.show("No PlatformIO environment selected")

            return

        if not self.prepare_compile_folder(prepare_step):
            return

        steps = [(None, self.run_arguments(*build_targets)),
                 ("Upload now?", self.run_arguments(*upload_targets))]

        if self.is_auto_monitor.get() == 1:
            steps.append((None, self.monitor_arguments()))

        if not self.launch_terminal_task(title, steps):
            return

        self.status += f"{title} started for {environment}"
        self.status.show()

    def run_monitor(self):
        """
        Open a serial monitor terminal for the selected environment.
        Reads the repository platformio.ini, so the compile folder does not have to be prepared
        """
        self.status.clear()
        environment = self.selected_env.get()

        if not environment:
            self.status.show("No PlatformIO environment selected")

            return

        if not self.launch_terminal_task("Serial Monitor", [(None, self.monitor_arguments())]):
            return

        self.status += f"Serial Monitor Started for {environment}"
        self.status.show()

    def save_and_increment_clock_id(self):
        """ Write the incremented ID to last_id.txt and update the UI display """
        self.clock_id_value += 1

        with open(self.LAST_ID_FILE, "w", encoding="utf8") as current_file:
            current_file.write(str(self.clock_id_value))

        self.id_display.config(text=f"{self.clock_id_value:05d}")

    def set_action_buttons_enabled(self, is_enabled):
        """ Grey out the build, upload and monitor buttons while a terminal is open """
        for button in self.action_buttons:
            button.config(state=tk.NORMAL if is_enabled else tk.DISABLED)

    def set_app_widgets(self):
        """ Create widgets in the application """
        cb = tk.Checkbutton(self.root, text="Test Environment",
                            variable=self.is_production_setup, onvalue=0, offvalue=1)
        self.cb_edit_id = tk.Checkbutton(self.root, text="Edit Clock ID",
                                         variable=self.is_edit_id, command=self.toggle_edit_id)
        self.id_frame, self.id_display = self.create_stepper_row(
            "Clock ID:", "-----", 6, self.adjust_clock_id)
        timezone_frame, self.timezone_display = self.create_stepper_row(
            "Timezone offset:", "+2", 4, self.adjust_timezone)

        env_label = tk.Label(self.root,
                             text="Select PlatformIO Environment\n(Sets the Build Flags and the RTC Modules)")
        longest_env = max((len(name) for name in self.environments), default=self.BUTTON_WIDTH)
        self.env_combobox = ttk.Combobox(self.root, textvariable=self.selected_env,
                                         width=longest_env + 2, state="readonly",
                                         values=self.environments)

        # Each build refreshes only the files it owns, then offers its own upload in the
        # same terminal. Buttons are greyed out while that terminal is open
        build_buttons = []

        for text, title, prepare, build_targets, upload_targets in (
                ("Build and Upload Firmware", "Firmware build and upload",
                 CompileFolder.prepare_firmware, (), ("upload",)),
                ("Build and Upload File System", "File system build and upload",
                 CompileFolder.prepare_filesystem, ("buildfs",), ("uploadfs",))):
            build_buttons.append(tk.Button(
                self.root, text=text, width=self.BUTTON_WIDTH,
                command=lambda t=title, p=prepare, b=build_targets, u=upload_targets:
                    self.run_build_and_upload(t, p, b, u)))

        auto_monitor_cb = tk.Checkbutton(self.root, text="Auto-Monitor",
                                         variable=self.is_auto_monitor)
        monitor_button = tk.Button(self.root, text="Serial Monitor", width=self.BUTTON_WIDTH,
                                   command=self.run_monitor)
        self.action_buttons += build_buttons + [monitor_button]
        self.status = StatusArea(self.root)

        for element in [cb, self.cb_edit_id, timezone_frame, env_label, self.env_combobox,
                        *build_buttons, auto_monitor_cb, monitor_button, self.status]:
            element.pack(pady=3)

        self.env_combobox.focus()

    def toggle_edit_id(self):
        """ Show or hide the clock ID row depending on the checkbox state """
        if self.is_edit_id.get() != 1:
            self.id_frame.pack_forget()
            self.center_window()

            return

        try:
            with open(self.LAST_ID_FILE, "r", encoding="utf8") as current_file:
                self.clock_id_value = int(current_file.read().strip())
                self.id_display.config(text=f"{self.clock_id_value:05d}")
        except (OSError, ValueError) as error:
            error_detail = ("Create it in the project root" if isinstance(error, OSError)
                            else "Verify the file is not empty")
            self.is_edit_id.set(0)
            self.status.show(f"Could not read \"{self.LAST_ID_FILE}\"\n{error_detail}")

            return

        self.id_frame.pack(after=self.cb_edit_id, pady=2)
        self.center_window()

    def watch_running_process(self):
        """ Re-enable the buttons once the spawned terminal is gone """
        if self.running_process is not None and self.running_process.poll() is None:
            self.root.after(self.PROCESS_POLL_INTERVAL, self.watch_running_process)

            return

        self.running_process = None
        self.set_action_buttons_enabled(True)

    def start(self):
        self.root.mainloop()


if __name__ == "__main__":
    SetupApp().start()

"""
Real Time Clock file setup and build launcher.

Prepares a self contained, compile ready copy of the project in Real_Time_Clock_Compile and
drives PlatformIO against that copy, so the sources in the repository are never modified.
The preparation itself and the message block live in rtc_setup_utils.

Run from the project root:  pythonw rtc_setup.pyw
"""

import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import tkinter as tk
from tkinter import ttk

from rtc_setup_utils import (ACCENT_RED, AUDIO_VERSIONS, MODULES, PLATFORMS, PROJECT_FILE, TASK_RUNNER,
                             TEXT_COLOR, WINDOW_BACKGROUND, CompileFolder, StatusArea, ToggleSwitch)


CREATE_NEW_CONSOLE = 0x00000010  # Opens the spawned command in its own console window on Windows


def console_python():
    """
    The interpreter that runs console work. A .pyw runs under pythonw, which has no console and
    would swallow the output, so its python.exe sibling is used instead
    """
    interpreter = sys.executable

    if os.name == "nt" and os.path.basename(interpreter).lower() == "pythonw.exe":
        interpreter = os.path.join(os.path.dirname(interpreter), "python.exe")

    return interpreter


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
        return [console_python(), "-m", "platformio"]

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
    The steps are written to a task file that TASK_RUNNER (rtc_setup_utils.run_terminal_task)
    runs inside the new terminal, so the prompts and retries work the same on every system.
    Returns the Popen of the terminal itself, so the caller can tell when it is closed
    """
    with tempfile.NamedTemporaryFile("w", encoding="utf8", prefix="rtc_task_", suffix=".json",
                                     delete=False) as task_file:
        json.dump({"title": title, "steps": steps}, task_file)

    runner = [console_python(), TASK_RUNNER, "--run-task", task_file.name]

    if os.name == "nt":
        return subprocess.Popen(runner, creationflags=CREATE_NEW_CONSOLE)

    for terminal in (["x-terminal-emulator", "-e"], ["gnome-terminal", "--"], ["xterm", "-e"]):
        if shutil.which(terminal[0]):
            return subprocess.Popen(terminal + runner)

    return subprocess.Popen(runner)  # No terminal emulator available: prompts read no input and count as no


class SetupApp:
    """ The setup window. Collects the build options, then drives CompileFolder and PlatformIO """

    ICON_FILE = os.path.join("data", "neonLogoIcon.ico")
    LAST_ID_FILE = "last_id.txt"

    TEXT_FONT = "Verdana 8 bold"
    BUTTON_WIDTH = 28  # Fits the longest caption
    MODULE_COLUMNS = 3  # Columns of the module checkbox grid
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
        self.set_colors()

        self.is_production_setup = tk.IntVar()
        self.is_edit_id = tk.IntVar(value=0)
        self.is_auto_monitor = tk.IntVar(value=1)
        self.is_auto_upload = tk.IntVar(value=1)
        self.platform = tk.StringVar(value=PLATFORMS["ESP32"])
        self.module_selections = {label: tk.IntVar(value=0) for label in MODULES}
        self.is_audio_module = tk.IntVar(value=0)
        self.audio_version = tk.StringVar(value="Base")

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

    def compile_folder(self):
        """ CompileFolder for the processor, modules and options currently selected in the window """
        modules = [MODULES[label] for label in sorted(MODULES) if self.module_selections[label].get() == 1]

        if self.is_audio_module.get() == 1:
            modules.append(AUDIO_VERSIONS[self.audio_version.get()])

        return CompileFolder(platform=self.platform.get(),
                             modules=modules,
                             timezone=self.timezone_value,
                             clock_id=self.clock_id_value if self.is_edit_id.get() == 1 else None,
                             production=self.is_production_setup.get() == 1)

    def create_stepper_row(self, parent, text, initial_value, display_width, adjust):
        """
        Build a [caption] [-] [value] [+] row in `parent` and return it together with its value label.
        The clock ID and timezone rows differ only in caption, width and callback
        """
        frame = tk.Frame(parent)
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
        `steps` is a list of dicts: "arguments" (PlatformIO arguments without the Core command
        prefix), optional "prompt" (asked before the step) and optional "retry_prompt" (asked
        after the step fails, until it succeeds or the answer is N)
        """
        if self.pio_command is None:  # Kept once found, looked up again while it is missing
            self.pio_command = find_platformio()

        if self.pio_command is None:
            self.status.show("PlatformIO Core was not found\nInstall it, or add pio to PATH")

            return False

        self.running_process = spawn_terminal(
            title, [{"command": self.pio_command + step["arguments"],
                     "prompt": step.get("prompt"),
                     "retry_prompt": step.get("retry_prompt")} for step in steps])
        self.set_action_buttons_enabled(False)
        self.watch_running_process()

        return True

    def monitor_arguments(self):
        """
        PlatformIO arguments that open the serial monitor. The plain platform environment of the
        repository platformio.ini is enough: board, speed and filters do not depend on the modules
        """
        return ["device", "monitor", "-d", os.path.abspath("."), "-e", self.platform.get()]

    def platform_is_known(self):
        """ Report a missing platform environment in platformio.ini and return False """
        if self.platform.get() in self.environments:
            return True

        self.status.show(f"Environment {self.platform.get()} not found in {PROJECT_FILE}")

        return False

    def prepare_compile_folder(self, folder, prepare_step):
        """
        Refresh the part of the compile folder that one build needs, before that build starts.
        `prepare_step` is the CompileFolder method that copies and edits the files of that build.
        Appends its progress to the status area and returns False when the copy was not made
        """
        if not self.platform_is_known():
            return False

        try:
            for line in prepare_step(folder):
                self.status += line
        except Exception as error:
            self.status.show(f"Failed to prepare the compile folder\n{error}")

            return False

        # The ID is written into Globals.h, so only a firmware build consumes one
        if self.is_edit_id.get() == 1 and prepare_step is CompileFolder.prepare_firmware:
            self.save_and_increment_clock_id()

        return True

    @staticmethod
    def reserve_space(slot, content):
        """
        Fix `slot` to the size of `content` (a widget inside it), so showing and hiding `content`
        with pack() / pack_forget() keeps the space and never resizes the window
        """
        content.pack()
        content.update_idletasks()
        slot.config(width=content.winfo_reqwidth(), height=content.winfo_reqheight())
        slot.pack_propagate(False)
        content.pack_forget()

    def run_arguments(self, environment, *targets):
        """ PlatformIO run arguments against the compile folder, extended with the given targets """
        arguments = ["run", "-d", os.path.abspath(CompileFolder.COMPILE_FOLDER), "-e", environment]

        for target in targets:
            arguments += ["-t", target]

        return arguments

    def run_build_and_upload(self, title, prepare_step, build_targets, upload_targets):
        """
        Refresh the files this build needs, then build and upload in the same terminal.
        With Auto-Upload ticked the upload starts right after a successful build, otherwise it is
        offered with a prompt. A failed upload offers a retry until the answer is N; a failed build
        ends the task without a retry. With Auto-Monitor ticked the serial monitor follows a
        successful upload in that same terminal
        """
        self.status.clear()
        folder = self.compile_folder()
        environment = folder.environment

        if not self.prepare_compile_folder(folder, prepare_step):
            return

        steps = [{"arguments": self.run_arguments(environment, *build_targets)},
                 {"arguments": self.run_arguments(environment, *upload_targets),
                  "prompt": None if self.is_auto_upload.get() == 1 else "Upload now?",
                  "retry_prompt": "Upload failed. Retry?"}]

        if self.is_auto_monitor.get() == 1:
            steps.append({"arguments": self.monitor_arguments()})

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
        environment = self.platform.get()

        if not self.platform_is_known():
            return

        if not self.launch_terminal_task("Serial Monitor", [{"arguments": self.monitor_arguments()}]):
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
        cb = ttk.Checkbutton(self.root, text="Test Environment",
                             variable=self.is_production_setup, onvalue=0, offvalue=1)
        self.cb_edit_id = ttk.Checkbutton(self.root, text="Edit Clock ID",
                                          variable=self.is_edit_id, command=self.toggle_edit_id)
        # The clock ID row and the audio version switch keep their space while hidden (see reserve_space)
        id_slot = tk.Frame(self.root)
        self.id_frame, self.id_display = self.create_stepper_row(
            id_slot, "Clock ID:", "-----", 6, self.adjust_clock_id)
        timezone_frame, self.timezone_display = self.create_stepper_row(
            self.root, "Timezone Offset:", "+2", 4, self.adjust_timezone)

        processor_label = tk.Label(self.root, text="Processor")
        self.platform_switch = ToggleSwitch(self.root, self.platform, *PLATFORMS.items())

        modules_label = tk.Label(self.root, text="Modules")
        modules_frame = tk.Frame(self.root)

        for index, label in enumerate(sorted(MODULES)):  # Alphabetical, row by row
            ttk.Checkbutton(modules_frame, text=label, variable=self.module_selections[label]).grid(
                row=index // self.MODULE_COLUMNS, column=index % self.MODULE_COLUMNS, sticky="w", padx=4)

        self.cb_audio = ttk.Checkbutton(self.root, text="Audio Module", variable=self.is_audio_module,
                                        command=self.toggle_audio_module)
        audio_slot = tk.Frame(self.root)
        self.audio_switch = ToggleSwitch(audio_slot, self.audio_version,
                                         *[(label, label) for label in AUDIO_VERSIONS])
        self.audio_switch.set_enabled(False)  # Only the base version exists for now

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

        auto_upload_cb = ttk.Checkbutton(self.root, text="Auto-Upload",
                                         variable=self.is_auto_upload)
        auto_monitor_cb = ttk.Checkbutton(self.root, text="Auto-Monitor",
                                          variable=self.is_auto_monitor)
        monitor_button = tk.Button(self.root, text="Serial Monitor", width=self.BUTTON_WIDTH,
                                   command=self.run_monitor)
        self.action_buttons += build_buttons + [monitor_button]
        self.status = StatusArea(self.root)

        for element in [cb, self.cb_edit_id, id_slot, timezone_frame, processor_label,
                        self.platform_switch, modules_label, modules_frame, self.cb_audio, audio_slot,
                        auto_upload_cb, *build_buttons, auto_monitor_cb, monitor_button, self.status]:
            element.pack(pady=3)

        self.reserve_space(id_slot, self.id_frame)
        self.reserve_space(audio_slot, self.audio_switch)

    def set_colors(self):
        """
        Dark window with white text. Tk widgets take the colours from the option database; the
        checkboxes are ttk widgets in the "clam" theme, which draws a red X on a white box
        """
        self.root.configure(background=WINDOW_BACKGROUND)

        for option, value in (("*Font", self.TEXT_FONT), ("*Background", WINDOW_BACKGROUND),
                              ("*Foreground", TEXT_COLOR), ("*activeBackground", "#555"),
                              ("*activeForeground", TEXT_COLOR), ("*highlightBackground", WINDOW_BACKGROUND)):
            self.root.option_add(option, value)

        style = ttk.Style(self.root)
        style.theme_use("clam")
        style.configure("TCheckbutton", font=self.TEXT_FONT, background=WINDOW_BACKGROUND,
                        foreground=TEXT_COLOR, indicatorbackground="white",
                        indicatorforeground=ACCENT_RED, focuscolor=WINDOW_BACKGROUND)
        style.map("TCheckbutton", background=[("active", WINDOW_BACKGROUND)],
                  indicatorbackground=[("pressed", "white"), ("active", "white")])

    def toggle_audio_module(self):
        """
        The audio module runs on the ESP32 only: while it is ticked the processor switch is set to the
        ESP32 and disabled, and the Base / Extended switch is shown under the checkbox
        """
        if self.is_audio_module.get() == 1:
            self.platform.set(PLATFORMS["ESP32"])
            self.platform_switch.set_enabled(False)
            self.audio_switch.pack()
        else:
            self.platform_switch.set_enabled(True)
            self.audio_switch.pack_forget()

    def toggle_edit_id(self):
        """ Show or hide the clock ID row depending on the checkbox state """
        if self.is_edit_id.get() != 1:
            self.id_frame.pack_forget()

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

        self.id_frame.pack()

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

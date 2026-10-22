"""
Support classes for the Real Time Clock setup launcher.
"""

import configparser
import importlib
import json
import os
import re
import shutil
import site
import subprocess
import sys
import tkinter as tk


PROJECT_FILE = "platformio.ini"  # PlatformIO configuration, read at the repository root
SECRETS_FILE = "rtc_secrets.ini"  # Local credentials, ignored by git
SECRETS_EXAMPLE_FILE = "rtc_secrets.example.ini"  # Committed template of SECRETS_FILE
MINIFIER_PACKAGES = ["rjsmin", "rcssmin"]  # Webpage minifiers, installed with pip on first use
TASK_RUNNER = os.path.abspath(__file__)  # Run as a script inside the build terminal, see run_terminal_task()
BUILD_CONFIG_FILE = "rtc_build.ini"  # Environment of the current build, loaded by platformio.ini (extra_configs)

# Processors offered by the launcher: label -> plain platform environment in platformio.ini
PLATFORMS = {"ESP32": "esp32", "ESP8266": "esp8266"}

# Modules offered by the launcher: label -> (environment name token, platformio.ini section)
MODULES = {"Brightness": ("bright", "brightness_module"),
           "GPS": ("gps", "gps_module"),
           "Temperature": ("temp", "temperature_module")}

# Audio module versions, ESP32 only: label -> (environment name token, platformio.ini section)
AUDIO_VERSIONS = {"Base": ("audio_base", "audio_module_base"),
                  "Extended": ("audio_extended", "audio_module_extended")}


def ask(question):
    """
    Ask a yes/no question in the terminal. Enter, Y or any answer other than N counts as yes.
    A closed input (no console attached) counts as no
    """
    try:
        return input(f"{question} [Y/n] ").strip().lower() != "n"
    except EOFError:
        return False


def import_minifiers():
    """
    Import rjsmin and rcssmin, installing them with pip into the interpreter that runs the
    launcher when they are missing. Returns (rjsmin, rcssmin, installed_now).
    Raises RuntimeError when pip fails, so a file system build never ships unminified files
    """
    try:
        import rcssmin
        import rjsmin

        return rjsmin, rcssmin, False
    except ImportError:
        pass

    result = subprocess.run([sys.executable, "-m", "pip", "install", *MINIFIER_PACKAGES],
                            capture_output=True, text=True,
                            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))

    if result.returncode != 0:
        details = (result.stderr or result.stdout).strip().splitlines()[-3:]
        raise RuntimeError("pip install " + " ".join(MINIFIER_PACKAGES) + " failed\n" + "\n".join(details))

    # pip falls back to a per-user install when site-packages is not writable. That folder is on
    # sys.path only if it existed when Python started, so add it before importing again
    user_site = site.getusersitepackages()

    if os.path.isdir(user_site) and user_site not in sys.path:
        site.addsitedir(user_site)

    importlib.invalidate_caches()

    import rcssmin
    import rjsmin

    return rjsmin, rcssmin, True


def run_step(step):
    """
    Run one terminal task step and return True when it succeeded.
    A step with a prompt runs only when the answer is not N. A failed step with a retry prompt is
    offered again until it succeeds or the answer is N; a failed step without one is not retried
    """
    if step.get("prompt") and not ask(step["prompt"]):
        return False

    while True:
        try:
            if subprocess.run(step["command"]).returncode == 0:
                return True
        except OSError as error:
            print(error)

        if not step.get("retry_prompt") or not ask(step["retry_prompt"]):
            return False


def run_terminal_task(task_path):
    """
    Run the steps of a task file in the current terminal, then wait for Enter so the output stays
    readable. The task file is JSON written by the launcher and deleted once read:
    {"title": str, "steps": [{"command": [str, ...], "prompt": str|null, "retry_prompt": str|null}]}
    The first failed or declined step ends the task, so a failed build never reaches the upload
    and a declined or failed upload never opens the serial monitor
    """
    with open(task_path, encoding="utf8") as task_file:
        task = json.load(task_file)

    os.remove(task_path)
    set_terminal_title(task["title"])

    try:
        for step in task["steps"]:
            if not run_step(step):
                break
    except KeyboardInterrupt:  # Ctrl+C, e.g. to leave the serial monitor
        print("\nStopped")

    try:
        input("\nPress Enter to close")
    except (EOFError, KeyboardInterrupt):
        pass


def set_terminal_title(title):
    """ Show the task name in the title bar of the terminal window """
    if os.name == "nt":
        import ctypes

        ctypes.windll.kernel32.SetConsoleTitleW(title)
    else:
        sys.stdout.write(f"\33]0;{title}\a")
        sys.stdout.flush()


class CompileFolder:
    """
    Builds the self contained, compile ready copy of the project.

    Every input arrives through the constructor and every completed step appends a line to
    `report`, so the caller decides how progress is displayed. Only paths under COMPILE_FOLDER
    are written to, which is what keeps the repository sources untouched.
    """

    COMPILE_FOLDER = "Real_Time_Clock_Compile"

    # Each build owns a set of folders and refreshes only those, so the two never wipe each other
    FIRMWARE_DIRS = ["include", "lib", "src"]
    FILESYSTEM_DIRS = ["data"]

    DATA_DIR = "data"
    GLOBALS_HEADER = os.path.join("include", "Globals.h")
    # Sources whose tm1637.showNumber() colon value changes in test environment builds
    TEST_DISPLAY_SOURCES = [os.path.join("src", "CoreUtils.cpp"), os.path.join("src", "DisplayUtils.cpp")]
    SETTINGS_XML = os.path.join("data", "espSettings.xml")

    # Webpage files with addon markers
    WEBPAGE_MAIN_FILES = ["body.html", "mainScript.js", "mainStyle.css"]

    # Module tokens as they appear inside the PlatformIO environment names
    MODULE_TOKENS = ["audio", "bright", "gps", "temp"]

    # Marker templates
    # The content between the markers of a module is used when the module is not built
    HTML_PLACEHOLDER = re.compile(
        r"[ \t]*<!-- PLACEHOLDER FOR (?P<name>[\w.]+) -->[ \t]*\r?\n?"
        r"(?P<default>.*?)"
        r"[ \t]*<!-- END OF PLACEHOLDER FOR (?P=name) -->[ \t]*\r?\n?",
        re.DOTALL)
    MAIN_FILE_END_MARKER = "/* END OF MAIN {filename} FILE */"

    def __init__(self, platform, modules, timezone, clock_id=None, production=False):
        """
        `platform` is a value of PLATFORMS, `modules` a list of (token, section) pairs taken from
        MODULES and AUDIO_VERSIONS, in the order the environment name lists them
        """
        self.platform = platform
        self.module_entries = list(modules)
        self.timezone = timezone
        self.clock_id = clock_id
        self.production = production
        self.report = []

    @property
    def environment(self):
        """ PlatformIO environment of this build: the platform followed by the module tokens """
        tokens = [token for token, _ in self.module_entries]

        return "_".join([self.platform] + (tokens or ["only"]))

    @property
    def modules(self):
        """ Module tokens carried by the PlatformIO environment name (used to pick the addons) """
        return [token for token in self.environment.split("_") if token in self.MODULE_TOKENS]

    def add_addon_content(self):
        """ Inline the addon files of the active modules into the copied webpage files """
        addon_source_dir = self.DATA_DIR  # Addons are read from the untouched sources
        active_modules = self.modules

        def replace_placeholder(match):
            """
            Swap a placeholder block for its addon content when the module is built.
            Otherwise keep the content that sits between the markers, dropping only the
            marker comments themselves
            """
            addon_name = match.group("name")
            module = addon_name.split("Addon")[0].lower()
            default_content = match.group("default")

            if module not in active_modules:
                return default_content

            addon_path = os.path.join(addon_source_dir, addon_name)

            if not os.path.isfile(addon_path):
                return default_content  # Nothing to inline, the default still applies

            with open(addon_path, "r", encoding="utf8") as addon_file:
                return addon_file.read().rstrip("\r\n") + "\n"

        def append_addons(content, filename):
            """ Drop the content left after the end marker and append content of the active modules """
            marker = self.MAIN_FILE_END_MARKER.format(filename=filename)
            marker_index = content.find(marker)

            if marker_index != -1:
                content = content[:marker_index]

            extension = os.path.splitext(filename)[1]

            for module in active_modules:
                addon_path = os.path.join(addon_source_dir, module + "Addon" + extension)

                if os.path.isfile(addon_path):
                    with open(addon_path, "r", encoding="utf8") as addon_file:
                        content += "\n" + addon_file.read()

            return content

        for filename in self.WEBPAGE_MAIN_FILES:
            target_path = self.compiled(self.DATA_DIR, filename)

            if not os.path.isfile(target_path):
                continue

            if filename.endswith(".html"):
                self.rewrite_file(target_path,
                                  lambda content: self.HTML_PLACEHOLDER.sub(replace_placeholder, content))
            else:
                self.rewrite_file(target_path,
                                  lambda content, name=filename: append_addons(content, name))

        self.report.append("Module content added to the webpage files")

    def apply_substitutions(self, path, *pairs):
        """ Apply regular expression replacements to a copied file, in the given order """
        def transform(content):
            for pattern, replacement in pairs:
                content = re.sub(pattern, replacement, content)

            return content

        self.rewrite_file(path, transform)

    def compiled(self, *parts):
        """ Path to something inside the compile folder """
        return os.path.join(self.COMPILE_FOLDER, *parts)

    def copy_sources(self, source_dirs):
        """
        Copy the given source folders into the compile folder.
        The PlatformIO configuration comes along every time, because both builds are run against it
        """
        for source_dir in source_dirs:
            destination = self.compiled(source_dir)

            if source_dir == self.DATA_DIR:
                # Superseded files and addons are not copied; Addons get inlined
                shutil.copytree(source_dir, destination,
                                ignore=shutil.ignore_patterns("*.old.*", "*Addon.*"))
            else:
                shutil.copytree(source_dir, destination)

        shutil.copy(PROJECT_FILE, self.COMPILE_FOLDER)
        self.report.append(f"Copied {', '.join(source_dirs)} to the compile folder")

    def edit_timezone(self):
        """ Write the selected timezone offset into the copied espSettings.xml """
        self.apply_substitutions(self.compiled(self.SETTINGS_XML),
                                 (r"(<timezoneHoursOffset>).*?(</timezoneHoursOffset>)",
                                  rf"\g<1>{self.timezone}\g<2>"))

        self.report.append(f"Timezone set to {self.timezone}")

    def minify_webpage_files(self):
        """
        Minify the javascript and css files inside the compile folder with rjsmin and rcssmin,
        installing the two packages first when they are missing
        """
        rjsmin, rcssmin, installed_now = import_minifiers()

        if installed_now:
            self.report.append("Installed " + " and ".join(MINIFIER_PACKAGES))

        minifiers = {".js": rjsmin.jsmin, ".css": rcssmin.cssmin}
        data_path = self.compiled(self.DATA_DIR)

        for filename in sorted(os.listdir(data_path)):
            minify = minifiers.get(os.path.splitext(filename)[1])

            if minify is not None:
                self.rewrite_file(os.path.join(data_path, filename), minify)

        self.report.append("Webpage files optimized")

    def prepare_filesystem(self):
        """
        Rebuild the `data` folder of the compile copy and return the progress lines.
        Raises on failure, leaving the folder half built. The lines collected before the failure
        stay in `report`, so a caller can still show how far it got
        """
        self.report = []

        self.reset_folder(self.FILESYSTEM_DIRS)
        self.copy_sources(self.FILESYSTEM_DIRS)
        self.write_build_config()
        self.edit_timezone()
        self.add_addon_content()
        self.minify_webpage_files()

        return self.report

    def prepare_firmware(self):
        """
        Rebuild the `include`, `lib` and `src` folders of the compile copy and return the
        progress lines. Raises on failure, leaving the folders half built. The lines collected
        before the failure stay in `report`, so a caller can still show how far it got
        """
        self.report = []

        self.reset_folder(self.FIRMWARE_DIRS)
        self.copy_sources(self.FIRMWARE_DIRS)
        self.write_build_config()
        self.set_debug_messages()
        self.set_test_display()
        self.set_credentials()

        return self.report

    def read_secrets(self):
        """
        Read the access point password and the software update login from SECRETS_FILE.
        Raises when the file, the [secrets] section or a value is missing, or a value cannot be
        written into a C string literal
        """
        parser = configparser.ConfigParser(interpolation=None)  # Passwords may contain %

        if not parser.read(SECRETS_FILE, encoding="utf8"):
            raise FileNotFoundError(f"{SECRETS_FILE} not found. Copy {SECRETS_EXAMPLE_FILE} and set the values")

        if not parser.has_section("secrets"):
            raise ValueError(f"{SECRETS_FILE} has no [secrets] section")

        secrets = {key: parser.get("secrets", key, fallback="").strip()
                   for key in ("ap_password", "ota_user", "ota_password")}

        for key, value in secrets.items():
            if not value or any(char in value for char in '"\\') or not value.isprintable() or not value.isascii():
                raise ValueError(f"{SECRETS_FILE}: {key} must be printable ASCII without quotes or backslashes")

        if not 8 <= len(secrets["ap_password"]) <= 63:
            raise ValueError(f"{SECRETS_FILE}: ap_password must have 8 to 63 characters")

        return secrets

    def reset_folder(self, source_dirs):
        """
        Create the compile folder and drop only the source folders that are about to be rewritten.
        The PlatformIO cache and the folders owned by the other build are left untouched
        """
        os.makedirs(self.COMPILE_FOLDER, exist_ok=True)

        for source_dir in source_dirs:
            path = self.compiled(source_dir)

            if os.path.isdir(path):
                shutil.rmtree(path)

    @staticmethod
    def rewrite_file(path, transform):
        """
        Apply a text transformation to a file, opening it once.
        Truncate after the write to prevent trailing remains
        if the content shrinks (e. g. JS and CSS files).
        """
        with open(path, "r+", encoding="utf8") as current_file:
            content = current_file.read()
            current_file.seek(0)
            current_file.write(transform(content))
            current_file.truncate()

    def set_credentials(self):
        """
        Set OTA_USER and OTA_PASS in the copied Globals.h from SECRETS_FILE, and ESP_SSID and
        ESP_PASS as well when a clock ID was supplied
        """
        secrets = self.read_secrets()

        def c_string(value):
            """ Replacement that keeps the declaration and swaps only the quoted value """
            return lambda match: f'{match.group(1)}"{value}"'

        substitutions = [(r'(OTA_USER\s*=\s*)"[^"]*"', c_string(secrets["ota_user"])),
                         (r'(OTA_PASS\s*=\s*)"[^"]*"', c_string(secrets["ota_password"]))]

        if self.clock_id is not None:
            ssid = f"NBG_CLOCK_{self.clock_id:05d}"
            substitutions += [(r'(ESP_SSID\s*=\s*)"[^"]*"', c_string(ssid)),
                              (r'(ESP_PASS\s*=\s*)"[^"]*"', c_string(secrets["ap_password"]))]

        self.apply_substitutions(self.compiled(self.GLOBALS_HEADER), *substitutions)
        self.report.append("Software update login set from " + SECRETS_FILE)
        self.report.append(f"Clock ID set to {ssid}" if self.clock_id is not None else "Clock ID not changed")

    def set_debug_messages(self):
        """ Turn the debug serial output off for production firmware, leave it on otherwise """
        if not self.production:
            self.report.append("Debug messages enabled")

            return

        self.apply_substitutions(self.compiled(self.GLOBALS_HEADER),
                                 (r"(DEBUG_MESSAGES\s*=\s*)true", r"\1false"))

        self.report.append("Debug messages disabled")

    def set_test_display(self):
        """
        Test environment builds only: change the second tm1637.showNumber() argument from 32 to 64
        in the copied CoreUtils.cpp and DisplayUtils.cpp. Other values and commented out calls stay
        """
        if self.production:
            return

        # One call per line; the prefix cannot contain "/", so calls after a // comment are skipped
        pattern = r"(?m)^([^/\n]*?\btm1637\.showNumber\([^,()]*,\s*)32\b"

        for source in self.TEST_DISPLAY_SOURCES:
            self.apply_substitutions(self.compiled(source), (pattern, r"\g<1>64"))

        self.report.append("Test display: showNumber() colon value 32 changed to 64")

    def write_build_config(self):
        """
        Write the environment of this build to BUILD_CONFIG_FILE in the compile folder, where
        platformio.ini loads it through extra_configs. It extends the plain platform environment
        with the sections of the selected modules; the vendored libraries of the other modules
        stay ignored. Raises ValueError for the audio module on a platform other than the ESP32
        """
        selected = [section for _, section in self.module_entries]
        all_sections = [section for _, section in list(MODULES.values()) + list(AUDIO_VERSIONS.values())]
        audio_sections = [section for _, section in AUDIO_VERSIONS.values()]

        if self.platform != PLATFORMS["ESP32"] and any(section in audio_sections for section in selected):
            raise ValueError("The audio module is available for the ESP32 only")

        base = f"env:{self.platform}"
        unselected = [section for section in all_sections if section not in selected]

        def option(key, values):
            """ One multi-line option, one value per line """
            return [f"{key:<16} ="] + [f"    {value}" for value in values]

        def references(name, sections):
            """ ${section.name} for each section """
            return [f"${{{section}.{name}}}" for section in sections]

        # The platform's own filter removes every module source; the selected ones are added back
        lines = [f"; Generated by rtc_setup for {self.environment}. Rewritten before every build, do not edit",
                 f"[env:{self.environment}]",
                 f"{'extends':<16} = {base}",
                 *option("build_flags", [f"${{{base}.build_flags}}"] + references("build_flags", selected)),
                 *option("lib_deps", [f"${{{base}.lib_deps}}"] + references("lib_deps", selected)),
                 *option("lib_ignore", references("lib_ignore_off", unselected)),
                 *option("build_src_filter",
                         [f"${{{base}.build_src_filter}}"] + references("src_filter", selected))]

        with open(self.compiled(BUILD_CONFIG_FILE), "w", encoding="utf8") as config_file:
            config_file.write("\n".join(lines) + "\n")

        self.report.append(f"Build environment {self.environment} written to {BUILD_CONFIG_FILE}")


class StatusArea(tk.Frame):
    """
    The message block at the bottom of the window.

    Two labels share one grid cell and only the relevant one is raised, so the block reserves
    the height of a single report rather than stacking two. Owns its own auto clear timer,
    scheduled on itself, so it needs no reference back to the window.
    """

    SHOW_DURATION = 5000
    HEIGHT = 7  # Reserved height
    WRAP_LENGTH = 380

    def __init__(self, master):
        super().__init__(master)

        self.success_message = ""
        self.after_id = None

        self.success_label = tk.Label(self, fg="#2eb82e", height=self.HEIGHT, justify=tk.LEFT,
                                      anchor="n", wraplength=self.WRAP_LENGTH)
        self.error_label = tk.Label(self, fg="#fa0a00", height=self.HEIGHT, justify=tk.LEFT,
                                    anchor="n", wraplength=self.WRAP_LENGTH)

        for label in (self.success_label, self.error_label):
            label.grid(row=0, column=0, sticky="nsew")

    def __iadd__(self, line):
        """ Add one line to the success message that the next show() will display """
        self.success_message += line + "\n"
        return self

    def cancel_auto_clear(self):
        """ Drop a pending auto clear, so an old timer can never wipe a fresh message """
        if self.after_id is not None:
            self.after_cancel(self.after_id)
            self.after_id = None

    def clear(self):
        """ Wipe both labels and forget the accumulated message """
        self.cancel_auto_clear()
        self.success_message = ""
        self.success_label.config(text="")
        self.error_label.config(text="")
        self.success_label.lift()

    def show(self, error=None):
        """ Display the status message """
        self.success_label.config(text=self.success_message)
        self.error_label.config(text=error or "")
        (self.error_label if error else self.success_label).lift()
        self.cancel_auto_clear()
        self.after_id = self.after(self.SHOW_DURATION, self.clear)


if __name__ == "__main__" and len(sys.argv) == 3 and sys.argv[1] == "--run-task":
    run_terminal_task(sys.argv[2])

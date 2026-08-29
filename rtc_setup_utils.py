"""
Support classes for the Real Time Clock setup launcher.
"""

import os
import re
import shutil
import tkinter as tk


PROJECT_FILE = "platformio.ini"  # PlatformIO configuration, read at the repository root


class CompileFolder:
    """
    Builds the self contained, compile ready copy of the project.

    Every input arrives through the constructor and every completed step appends a line to
    `report`, so the caller decides how progress is displayed. Only paths under COMPILE_FOLDER
    are written to, which is what keeps the repository sources untouched.
    """

    COMPILE_FOLDER = "Real_Time_Clock_Compile"
    SOURCE_DIRS = ["data", "include", "lib", "src"]
    DATA_DIR = "data"
    GLOBALS_HEADER = os.path.join("include", "Globals.h")
    SETTINGS_XML = os.path.join("data", "espSettings.xml")

    # Webpage files with addon markers
    WEBPAGE_MAIN_FILES = ["body.html", "mainScript.js", "mainStyle.css"]

    # Module tokens as they appear inside the PlatformIO environment names
    MODULE_TOKENS = ["brightness", "gps", "temp"]

    # Marker templates
    # The content between the markers of a module is used when the module is not built
    HTML_PLACEHOLDER = re.compile(
        r"[ \t]*<!-- PLACEHOLDER FOR (?P<name>[\w.]+) -->[ \t]*\r?\n?"
        r"(?P<default>.*?)"
        r"[ \t]*<!-- END OF PLACEHOLDER FOR (?P=name) -->[ \t]*\r?\n?",
        re.DOTALL)
    MAIN_FILE_END_MARKER = "/* END OF MAIN {filename} FILE */"

    def __init__(self, environment, timezone, clock_id=None, production=False):
        self.environment = environment
        self.timezone = timezone
        self.clock_id = clock_id
        self.production = production
        self.report = []

    @property
    def modules(self):
        """ Module tokens carried by the PlatformIO environment name """
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

    def copy_sources(self):
        """ Copy every source folder and the PlatformIO configuration into the compile folder """
        for source_dir in self.SOURCE_DIRS:
            destination = self.compiled(source_dir)

            if source_dir == self.DATA_DIR:
                # Superseded files and addons are not copied; Addons get inlined
                shutil.copytree(source_dir, destination,
                                ignore=shutil.ignore_patterns("*.old.*", "*Addon.*"))
            else:
                shutil.copytree(source_dir, destination)

        shutil.copy(PROJECT_FILE, self.COMPILE_FOLDER)
        self.report.append("Sources copied to the compile folder")

    def edit_timezone(self):
        """ Write the selected timezone offset into the copied espSettings.xml """
        self.apply_substitutions(self.compiled(self.SETTINGS_XML),
                                 (r"(<timezoneHoursOffset>).*?(</timezoneHoursOffset>)",
                                  rf"\g<1>{self.timezone}\g<2>"))

        self.report.append(f"Timezone set to {self.timezone}")

    def minify_source(self, content, extension):
        """ Strip comments and collapse whitespace in a javascript or css file """
        output = []
        in_block_comment = False

        for line in content.splitlines():
            if in_block_comment:
                end_index = line.find("*/")

                if end_index == -1:
                    continue

                line = line[end_index + 2:]
                in_block_comment = False

            while "/*" in line:
                start = line.find("/*")
                end = line.find("*/", start + 2)

                if end == -1:
                    line = line[:start]
                    in_block_comment = True
                    break

                line = line[:start] + line[end + 2:]

            if extension == ".js":
                line = self.strip_line_comment(line)

            line = re.sub(r"\s+", " ", line).strip()

            if line:
                output.append(line)

        return " ".join(output)

    def minify_webpage_files(self):
        """ Minify the javascript and css files inside the compile folder """
        data_path = self.compiled(self.DATA_DIR)

        for filename in sorted(os.listdir(data_path)):
            extension = os.path.splitext(filename)[1]

            if extension not in (".js", ".css"):
                continue

            file_path = os.path.join(data_path, filename)

            self.rewrite_file(file_path,
                              lambda content, ext=extension: self.minify_source(content, ext))

        self.report.append("Webpage files optimized")

    def prepare(self):
        """
        Rebuild the compile folder from the repository sources and return the progress lines.
        Raises on failure, leaving the folder half built. The lines collected before the failure
        stay in `report`, so a caller can still show how far it got
        """
        self.report = []

        self.reset_folder()
        self.copy_sources()
        self.edit_timezone()
        self.set_debug_messages()
        self.set_credentials()
        self.add_addon_content()
        self.minify_webpage_files()

        return self.report

    def reset_folder(self):
        """ Create the compile folder, or empty it, keeping the PlatformIO cache for fast rebuilds """
        if not os.path.exists(self.COMPILE_FOLDER):
            os.makedirs(self.COMPILE_FOLDER)

            return

        for entry in os.listdir(self.COMPILE_FOLDER):
            if entry == ".pio":
                continue

            path = self.compiled(entry)

            if os.path.isdir(path):
                shutil.rmtree(path)
            else:
                os.remove(path)

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
        """ Set ESP_SSID and ESP_PASS in the copied Globals.h, when a clock ID was supplied """
        if self.clock_id is None:
            self.report.append("Clock ID not changed")

            return

        ssid = f"NBG_CLOCK_{self.clock_id:05d}"

        self.apply_substitutions(self.compiled(self.GLOBALS_HEADER),
                                 (r'(ESP_SSID\s*=\s*)"[^"]*"', rf'\1"{ssid}"'),
                                 (r'(ESP_PASS\s*=\s*)"[^"]*"', r'\1"NEON1234"'))

        self.report.append(f"Clock ID set to {ssid}")

    def set_debug_messages(self):
        """ Turn the debug serial output off for production firmware, leave it on otherwise """
        if not self.production:
            self.report.append("Debug messages enabled")

            return

        self.apply_substitutions(self.compiled(self.GLOBALS_HEADER),
                                 (r"(DEBUG_MESSAGES\s*=\s*)true", r"\1false"))

        self.report.append("Debug messages disabled")

    @staticmethod
    def strip_line_comment(line):
        """ Remove a trailing // comment while leaving protocol separators and strings alone """
        in_single = in_double = False
        index = 0

        while index < len(line) - 1:
            char = line[index]

            if char == "\\":
                index += 2
                continue

            if char == "'" and not in_double:
                in_single = not in_single
            elif char == '"' and not in_single:
                in_double = not in_double
            elif char == "/" and line[index + 1] == "/" and not in_single and not in_double:
                if index == 0 or line[index - 1] != ":":  # Keep http:// and friends
                    return line[:index]

            index += 1

        return line


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

import os
import tkinter as tk
from shutil import copy


class FileSetupApp:
    info_label_text = ""

    # Constants
    MODULES = {"gps": "GPS", "ls": "Light_Sensitivity", "temp": "Temperature"}
    WEBPAGE_MAIN_FILES = ["index.html", "mainScript.js", "mainStyle.css"]
    TEXT_FONT = "Verdana 8 bold"
    COMPILE_READY_FOLDER = "./Real_Time_Clock_Compile/Real_Time_Clock/"
    MAIN_RTC_INO_FILE = "Real_Time_Clock.ino"
    SELECTED_MODULES_HEADER = "SelectedModules.h"
    PLATFORM_HEADER = "Platform.h"
    TEXT_SHOW_DURATION = 3500

    def __init__(self):
        os.chdir("./Real_Time_Clock")

        # Find the compilation folder (in case I rename it from Real_Time_Clock_Compile)
        if not os.path.exists(self.COMPILE_READY_FOLDER):
            with os.scandir("./") as entities:
                for dir_entity in entities:
                    if "Compil" in dir_entity.name:
                        self.COMPILE_READY_FOLDER = "./" + dir_entity.name + "/Real_Time_Clock/"
                        break

        WINDOW_WIDTH = 400
        WINDOW_HEIGHT = 275
        WINDOW_POS_X = 575
        WINDOW_POS_Y = 300
        self.root = tk.Tk()
        self.root.resizable(0, 0)
        self.root.wm_minsize(width=WINDOW_WIDTH, height=WINDOW_HEIGHT)
        self.root.title("RTC file setup")
        self.root.geometry(
            f"{WINDOW_WIDTH}x{WINDOW_HEIGHT}+{WINDOW_POS_X}+{WINDOW_POS_Y}")
        # Event handler for return click
        self.root.bind("<Return>", self.set_files)
        self.is_production_setup = tk.IntVar()  # tkinter integer class
        self.modules_input_value = tk.StringVar()  # tkinter string class

        try:
            self.root.iconbitmap("./data/neonLogoIcon.ico")
        except:
            pass

        self.set_app_widgets()

    def add_content_to_webpage_file(self, dest_folder, addons_list):
        """ Add the addon files to the main webpage files """
        # Addon files appended in the markers.
        # If thefile doesn't exist in the compilation folder, only it's marker is removed
        webpage_addon_filenames = ["gpsAddon.html"]
        # Placeholder in the main HTML file
        html_placeholder_start_marker = "<!-- PLACEHOLDER FOR "
        html_placeholder_end_marker = "<!-- END OF PLACEHOLDER FOR "
        end_of_placeholder_marker = " -->"
        js_and_css_marker = "/* END OF MAIN "  # Marker in the main JS and CSS files
        end_of_js_and_css_marker = " FILE */"
        requested_addons_filenames = []

        # Create list with filenames of addons that are requested
        for filename in webpage_addon_filenames:
            for addon_substr in [a + "Addon" for a in addons_list]:
                if addon_substr in filename:
                    requested_addons_filenames.append(filename)

        for main_filename in self.WEBPAGE_MAIN_FILES:
            webpage_main_file = dest_folder + main_filename

            # Open each main webpage file
            with open(webpage_main_file, "r+", encoding="utf8") as current_file:
                lines = current_file.readlines()

                current_file.seek(0)
                current_file.truncate()

                if main_filename.endswith(".html"):  # HTML files
                    is_between_markers = False

                    for line in lines:
                        is_end_marker = False

                        for filename in webpage_addon_filenames:  # For each addon file check for it's markers
                            if (html_placeholder_start_marker + filename + end_of_placeholder_marker) in line:
                                is_between_markers = True

                                break
                            elif (html_placeholder_end_marker + filename + end_of_placeholder_marker) in line:
                                is_between_markers = False
                                is_end_marker = True

                                if filename in requested_addons_filenames:  # Insert the content of the addon file
                                    with open("./data/" + filename, "r", encoding="utf8") as file_to_write:
                                        lines_to_add = file_to_write.readlines()

                                        for line_to_add in lines_to_add:
                                            current_file.write(line_to_add)

                                    # Remove the file from the list for less iterations
                                    requested_addons_filenames.remove(filename)
                                    webpage_addon_filenames.remove(filename)

                                break

                        if is_end_marker:  # Don't write the end markers
                            continue

                        if not is_between_markers:  # Write the main file content
                            current_file.write(line)

                else:  # JS and CSS files
                    passed_file_marker = False

                    for line in lines:
                        for main_filename in self.WEBPAGE_MAIN_FILES:
                            requested_filename = False
                            # Skip the marker line for the end of the main file,
                            # delete the lines after it
                            # and append the addon files after the end of file mark
                            if (js_and_css_marker + main_filename + end_of_js_and_css_marker) in line:
                                passed_file_marker = True

                                # Only append the requested addons
                                for filename in webpage_addon_filenames:
                                    if filename in requested_addons_filenames and \
                                        ((filename.endswith(".js") and main_filename.endswith(".js")) or
                                         (filename.endswith(".css") and main_filename.endswith(".css"))):
                                        requested_filename = True
                                        with open("./data/" + filename, "r", encoding="utf8") as file_to_write:
                                            lines_to_add = file_to_write.readlines()

                                            for line_to_add in lines_to_add:
                                                current_file.write(line_to_add)

                                        # Remove the file from the list for less iterations
                                        requested_addons_filenames.remove(
                                            filename)
                                        webpage_addon_filenames.remove(
                                            filename)

                                        break

                                if requested_filename:
                                    break

                        if passed_file_marker:  # Clear the content after the file end marker
                            break
                        else:  # Write the main file content
                            current_file.write(line)

    def clear_compile_ready_folder(self):
        """ Clear the content of the compilation folder and it's subfolders (keep the subfolders) """
        with os.scandir(self.COMPILE_READY_FOLDER) as entities:
            for dir_entity in entities:
                actual_dir_entity = self.COMPILE_READY_FOLDER + dir_entity.name

                if os.path.isdir(actual_dir_entity):
                    with os.scandir(actual_dir_entity + "/") as sub_entities:
                        for sub_entity in sub_entities:
                            os.remove(actual_dir_entity +
                                      "/" + sub_entity.name)
                else:
                    os.remove(actual_dir_entity)

    def clear_empty_label_text(self):
        self.info_label_text = ""
        self.info_label.config(text="")

    def disable_info_messages(self):
        """ Comment the info messages definition in MSVDC.h to prevent debug messages in production code """
        INFO_MESSAGES_DEFINITION = "#define RTC_INFO_MESSAGES"

        with open(self.COMPILE_READY_FOLDER + self.MAIN_RTC_INO_FILE, "r+", encoding="utf8") as current_file:
            lines = current_file.readlines()

            current_file.seek(0)
            current_file.truncate()

            for line in lines:
                if INFO_MESSAGES_DEFINITION in line:
                    line = "// " + line

                current_file.write(line)

    def copy_files_for_compilation(self, should_add_modules):
        """ Copy all needed files in the compilation folder """
        requested_modules = self.modules_input_value.lower().split(" ")

        # Scan the MAIN folder for 'data' subfolder
        with os.scandir("./") as entities:
            # Append the main sketch file to the end to prevent issues with the variables' changes
            entities_names = list(map(lambda x: x.name, entities))

            entities_names.pop(entities_names.index(self.MAIN_RTC_INO_FILE))
            entities_names.append(self.MAIN_RTC_INO_FILE)

            for dir_entity_name in entities_names:
                if os.path.isdir(dir_entity_name) and dir_entity_name == "data":
                    # Set dest_dir to './Real_Time_Clock/{COMPILATION folder}/data/'
                    dest_dir = self.COMPILE_READY_FOLDER + dir_entity_name + "/"

                    # Create 'data' subfolder in the COMPILATION folder if it doesn't exist
                    if not os.path.exists(dest_dir):
                        os.mkdir(dest_dir)

                    modules_list = list(self.MODULES.values())

                    # Copy the main webpage files first in the COMPILATION folder
                    # Modules' logic will be added to them
                    for webpage_main_file in self.WEBPAGE_MAIN_FILES:
                        try:
                            copy("./" + dir_entity_name + "/" +
                                 webpage_main_file, dest_dir)
                        except IOError as err:
                            self.error_label_text = f"Unable to copy {webpage_main_file}\n{err}"
                            self.error_label.config(text=self.error_label_text)
                            return

                    # Edit the list because of the naming differences
                    for i, module_name in enumerate(modules_list):
                        modules_list[i] = module_name.replace("_", "")

                    # Copy the webpage files from '{MAIN folder}/data/' to './{COMPILATION folder}/data/'
                    with os.scandir("./" + dir_entity_name) as sub_entities:
                        for sub_entity in sub_entities:
                            sub_entity_name = sub_entity.name

                            # Skip the old files
                            if ".old" not in sub_entity_name and sub_entity_name not in self.WEBPAGE_MAIN_FILES:
                                src_dir = "./" + dir_entity_name + "/" + sub_entity_name
                                is_module_file = False

                                for module in modules_list:  # GPS, Temperature, LightSensitivity
                                    if (module + "Addon").lower() in sub_entity_name.lower():
                                        is_module_file = True

                                if not is_module_file:
                                    copy(src_dir, dest_dir)

                    if should_add_modules:
                        modules_to_append = []

                        # Set modules to be appended to the webpage files
                        if requested_modules != ['']:
                            modules_to_append = [
                                self.MODULES[rm].lower() for rm in requested_modules]
                        else:
                            modules_to_append = [module.lower()
                                                 for module in self.MODULES.values()]

                        self.add_content_to_webpage_file(
                            self.COMPILE_READY_FOLDER + dir_entity_name + "/", modules_to_append)

                # Copy the rest of the sketch files
                elif not os.path.isdir(dir_entity_name) and \
                        (dir_entity_name.endswith(".ino") or dir_entity_name.endswith(".h")):
                    copy("./" + dir_entity_name, self.COMPILE_READY_FOLDER)

                    if dir_entity_name == self.MAIN_RTC_INO_FILE and self.is_production_setup.get() == 1:
                        self.disable_info_messages()

                    if dir_entity_name == self.SELECTED_MODULES_HEADER and should_add_modules:
                        # Modules' specific definitions
                        self.define_requested_modules(requested_modules)

    def create_compilation_folder(self):
        """ Create the compile ready folder and subfolders inside it """
        first_slash = self.COMPILE_READY_FOLDER.find("/") + 1

        for folder in (self.COMPILE_READY_FOLDER[first_slash:] + "data").split("/"):
            os.mkdir(folder)
            os.chdir(folder)

        for folder in (self.COMPILE_READY_FOLDER[first_slash:] + "data").split("/"):
            os.chdir("../")

    def define_requested_modules(self, modules_to_activate):
        """ Create definitions for the requested modules """
        # Open the SelectedModules.h file in the COMPILATION folder
        with open(self.COMPILE_READY_FOLDER + self.SELECTED_MODULES_HEADER, "r+", encoding="utf8") as current_file:
            # Get the first lines before the modules' definitions (max lines in the file - number of modules)
            lines = current_file.readlines()
            lines_len = len(lines)

            current_file.seek(0)
            current_file.truncate()

            for line in lines[0:lines_len - len(self.MODULES)]:
                current_file.write(line)

            define_all_modules = (modules_to_activate == [''])
            definition_start = ''
            DEFINITION_END = '_MODULE\n'

            # Set all definitions
            for module in self.MODULES:
                if define_all_modules or module in modules_to_activate:
                    definition_start = '#define  '
                else:
                    definition_start = '// #define  '

                current_file.write(
                    definition_start + self.MODULES[module].upper() + DEFINITION_END)

            self.info_label_text += "Defined required modules in SelectedModules.h!\n"

    def optimize_webpage_files(self):
        """ Optimize javascript and css files (basically make them .min) """
        os.chdir("./data")  # Change directory to edit webpage files
        comment_marks = ["//", "/*"]

        with os.scandir("./") as entities:
            for dir_entity in entities:
                dir_entity_name = dir_entity.name

                if dir_entity_name.endswith(".js") or dir_entity_name.endswith(".css"):
                    with open(dir_entity_name, "r+", encoding="utf8") as current_file:
                        is_multiline_comment = False
                        lines = current_file.readlines()

                        current_file.seek(0)
                        current_file.truncate()

                        for current_line in lines:
                            comment_start = -1
                            comment_end = -1

                            # Remove comments
                            for comment_mark in comment_marks:
                                if is_multiline_comment and current_line.find("*/") != -1:
                                    comment_start = 0
                                    comment_end = current_line.find("*/") + 2
                                    is_multiline_comment = False
                                    break

                                if comment_mark in current_line:
                                    if current_line.find(comment_mark) != current_line.find(comment_mark + "\")") \
                                            and current_line.find(comment_mark) != -1:
                                        comment_start = current_line.find(
                                            comment_mark)

                                    if comment_mark == "/*":
                                        comment_end = current_line.find(
                                            "*/", comment_start)

                                        if comment_end != -1:
                                            comment_end += 2
                                        else:
                                            is_multiline_comment = True
                                    break

                            if comment_start != -1:
                                current_line = current_line[:comment_start]

                                if comment_end != -1:
                                    current_line += current_line[comment_end:]

                            remove_space = True

                            # Replace new line characters with spaces
                            # and reduce spaces to no more than one consecutive
                            for symbol in current_line:
                                if symbol == " " or symbol == "\n" or symbol == os.linesep:
                                    if remove_space:
                                        symbol = ""
                                    else:
                                        symbol = " "
                                        remove_space = True
                                else:
                                    remove_space = False
                                current_file.write(symbol)

        self.info_label_text += "Webpage files optimized!"
        self.root.after(self.TEXT_SHOW_DURATION, self.clear_empty_label_text)

    def set_files(self, event):
        """ Calls most of the other methods - can be considered as the main method """
        self.info_label_text = ""
        self.info_label.config(text="")
        self.error_label.config(text="")

        if not os.path.exists(self.COMPILE_READY_FOLDER):
            self.create_compilation_folder()
        else:
            self.clear_compile_ready_folder()

        self.modules_input_value = self.modules_input.get()
        modules_input_is_dash = self.modules_input_value == "-"
        should_add_modules = len(
            self.modules_input_value) == 0 or not modules_input_is_dash

        # Prepare the files for compilation
        if modules_input_is_dash or should_add_modules or self.modules_input_value in self.MODULES or \
            (self.modules_input_value.find(" ") != -1 and
             any(module in self.MODULES for module in self.modules_input_value.split(" "))):
            try:
                self.copy_files_for_compilation(should_add_modules)
            except:
                self.error_label.config(
                    text="Failed to copy files to compile ready folder!")
                return

            self.info_label_text += "Files moved from main to compile ready folder!\n"

            # Change directory to optimize the files
            os.chdir(self.COMPILE_READY_FOLDER)
            self.optimize_webpage_files()
            # Go back to main directory in case the setup is run again
            os.chdir("../../../")
            self.info_label.config(text=self.info_label_text)

        # Invalid input
        elif self.modules_input_value not in self.MODULES:
            self.error_label.config(text="Invalid module")

        # Invalid separation
        else:
            self.error_label.config(text="Separate module names with space")

    def set_app_widgets(self):
        """ Create widgets in the application """
        self.cb = tk.Checkbutton(self.root, text="Production setup", font=self.TEXT_FONT,
                                 variable=self.is_production_setup, onvalue=1, offvalue=0)
        main_label_text = "Modules to use (options are "
        modules_keys = list(self.MODULES.keys())

        # Add all modules keys in the main label text
        for i, module_key in enumerate(modules_keys):
            if i < len(modules_keys) - 2:
                main_label_text += f"{module_key}, "
            elif i == len(modules_keys) - 2:
                main_label_text += f"{module_key} and "
            else:
                main_label_text += f"{module_key})\n"

        main_label_text += "Separate them with space"
        self.main_label = tk.Label(
            self.root, text=main_label_text, font=self.TEXT_FONT)
        self.modules_input = tk.Entry(self.root, font=self.TEXT_FONT)
        self.button = tk.Button(
            self.root, text="Set files", font=self.TEXT_FONT)
        hint_label_text = ""

        # Create the hint label text
        for i in modules_keys:
            hint_label_text += f"{i} for {self.MODULES[i]}\n"

        hint_label_text += "empty for all\ndash for none"
        self.hint_label = tk.Label(
            self.root, text=hint_label_text, font=self.TEXT_FONT)
        self.info_label = tk.Label(
            self.root, font=self.TEXT_FONT, fg="#2eb82e")
        self.error_label = tk.Label(
            self.root, font=self.TEXT_FONT, fg="#fa0a00")
        # Event handler for LMB click on the button
        self.button.bind("<Button-1>", self.set_files)

        for element in [self.cb, self.main_label, self.modules_input, self.button, self.hint_label,
                        self.info_label, self.error_label]:
            element.pack()

        self.modules_input.focus()

    def start(self):
        self.root.mainloop()


FileSetupApp().start()

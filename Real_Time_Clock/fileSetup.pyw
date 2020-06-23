import os, platform, tkinter as tk
from shutil import copy

class FileSetupApp:
    modules = {"gps":"GPS", "temp":"Temperature", "ls":"Light_Sensitivity"}
    text_font = "Verdana 8 bold"
    info_label_text = ""
    compile_ready_folder = "./Real_Time_Clock_Compile/Real_Time_Clock/"


    # Constructor
    def __init__(self):
        # Find the compilation folder (in case I rename it from Real_Time_Clock_Compile)
        with os.scandir("./") as entities:
            for dir_entity in entities:
                if "Compil" in dir_entity.name:
                    self.compile_ready_folder = "./" + dir_entity.name + "/Real_Time_Clock/"
                    break

        self.root = tk.Tk()
        self.root.resizable(0, 0)   
        self.root.wm_minsize(width=350, height=180)
        self.root.title("File setup script")
        self.root.geometry("350x180+550+300")
        self.root.bind("<Return>", self.setFiles) # Event handler for return click
        self.move_files_from_dev_folder = tk.IntVar() # tkinter integer class
        self.modules_input_value = tk.StringVar() # tkinter string class

        try:
            self.root.iconbitmap("./data/neonLogoicon.ico")
        except:
            pass

        self.setAppWidgets()


    # Creates the compile ready folder and subfolders inside it
    def createCompileReadyFolder(self):
        first_slash = self.compile_ready_folder.find("/") + 1
        for folder in (self.compile_ready_folder[first_slash:] + "data").split("/"):
            os.mkdir(folder)
            os.chdir(folder)
        
        for folder in (self.compile_ready_folder[first_slash:] + "data").split("/"):
            os.chdir("../")


    # Optimizes sketch files skipping the No_[module_Addon].ino ones since they are OK
    def optimizeSketchFiles(self):
        debug_info_markers = ["Serial.print(", "Serial.println("] # Debugging functions

        with os.scandir("./") as entities:
            for dir_entity in entities:
                if (not os.path.isdir(dir_entity.name) and dir_entity.name.endswith(".ino") 
                    and not dir_entity.name.startswith("No_")):
                    with open(dir_entity.name, "r+", encoding="utf8") as current_file:
                        lines = current_file.readlines()
                        current_file.seek(0)
                        current_file.truncate()

                        for line in lines: # Remove the lines containing markers (debug info)
                            skip_line = False
                            for marker in debug_info_markers:
                                if marker in line:
                                    skip_line = True
                                    break
                            
                            if not skip_line:
                                current_file.write(line)
        
        self.info_label_text += "Sketch files optimized!\n"
        self.info_label.config(text=self.info_label_text)


    # Optimizes javascript and css files (basically makes them .min)
    def optimizeWebpageFiles(self):
        os.chdir("./data") # Change directory to edit webpage files
        comment_marks = ["//", "/*"]

        with os.scandir("./") as entities:
            for dir_entity in entities:
                if dir_entity.name.endswith(".js") or dir_entity.name.endswith(".css"):
                    with open(dir_entity.name, "r+", encoding="utf8") as current_file:
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
                                        comment_start = current_line.find(comment_mark)

                                    if comment_mark == "/*":
                                        comment_end = current_line.find("*/", comment_start)

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
        self.info_label.config(text=self.info_label_text)
    

    def setFiles(self, event):
        self.info_label_text = ""
        self.info_label.config(text=self.info_label_text)
        self.error_label.config(text="")

        # Move files from dev to main directory
        if self.move_files_from_dev_folder.get() == 1:
            development_folder = "./Real_Time_Clock_Development/Real_Time_Clock/"

            try:
                with os.scandir(development_folder) as entities:
                    for dir_entity in entities:
                        if os.path.isdir(development_folder + dir_entity.name):
                            if not os.path.exists(dir_entity.name):
                                os.mkdir(dir_entity.name)

                            with os.scandir(development_folder + dir_entity.name) as sub_entities:
                                for sub_entity in sub_entities:
                                    src_dir = development_folder + dir_entity.name + "/" + sub_entity.name
                                    dest_dir = os.curdir + "/" + dir_entity.name + "/"
                                    copy(src_dir, dest_dir)
                        else:
                            src_dir = development_folder + "/" + dir_entity.name
                            copy(src_dir, os.curdir)
            except:
                self.error_label_text = "Failed to copy files from dev to main folder!"
                self.error_label.config(text=self.error_label_text)
                return
                
            self.info_label_text = "Files copied from dev to main folder!\n"
            self.info_label.config(text=self.info_label_text)

        if not os.path.exists(self.compile_ready_folder):
            self.createCompileReadyFolder()
        else:
            # Clear the compile ready folder
            with os.scandir(self.compile_ready_folder) as entities:
                for dir_entity in entities:
                    actual_dir_entity = self.compile_ready_folder + dir_entity.name
                    
                    if not os.path.isdir(actual_dir_entity):
                        os.remove(actual_dir_entity)
                    else:
                        with os.scandir(actual_dir_entity + "/") as sub_entities:
                            for sub_entity in sub_entities:
                                os.remove(actual_dir_entity + "/" + sub_entity.name)
        
        self.modules_input_value = self.modules_input.get()
        add_any_modules = len(self.modules_input_value) == 0

        # Prepare the files for compilation
        if self.modules_input_value == "-":
            self.info_label_text += "No files moved to compile ready folder!"
            self.info_label.config(text=self.info_label_text)
        elif add_any_modules or self.modules_input_value in self.modules or (self.modules_input_value.find(" ") != -1 and  \
            any(module in self.modules for module in self.modules_input_value.split(" "))):
            try:
                with os.scandir("./") as entities:
                    for dir_entity in entities:

                        # Move the webpage files
                        if os.path.isdir(dir_entity.name) and dir_entity.name == "data":
                            dest_dir = self.compile_ready_folder + dir_entity.name + "/"

                            if not os.path.exists(dest_dir):
                                os.mkdir(dest_dir)
                            
                            with os.scandir("./" + dir_entity.name) as sub_entities:
                                for sub_entity in sub_entities:
                                    if ".old" not in sub_entity.name:
                                        src_dir = "./" + dir_entity.name + "/" + sub_entity.name
                                        copy(src_dir, dest_dir)
                        
                        # Move sketch modules
                        elif dir_entity.name.endswith("_Addon.ino"):
                            entity_name_starts_with_no = dir_entity.name.startswith("No_")

                            if add_any_modules and entity_name_starts_with_no:
                                copy("./" + dir_entity.name, self.compile_ready_folder)
                            else:
                                for module in (self.modules):
                                    if self.modules[module] in dir_entity.name:
                                        include_module = module in self.modules_input_value.lower().split(" ")

                                        if (include_module and not entity_name_starts_with_no) or \
                                            (not include_module and entity_name_starts_with_no):
                                            copy("./" + dir_entity.name, self.compile_ready_folder)
                                            break

                         # Move the rest of the sketch files
                        elif dir_entity.name.endswith(".ino"):
                            copy("./" + dir_entity.name, self.compile_ready_folder)

            except:
                self.error_label.config(text="Failed to copy files to compile ready folder!")
                return

            self.info_label_text += "Files moved from main to compile ready folder!\n"
            self.info_label.config(text=self.info_label_text)
            
            os.chdir(self.compile_ready_folder) # Change directory to optimize the files
            self.optimizeSketchFiles()
            self.optimizeWebpageFiles()
            os.chdir("../../../") # Go back to main directory in case the setup is run again

        # Invalid input
        elif self.modules_input_value not in self.modules:
            self.error_label.config(text="Invalid module")
        
        # Invalid separation
        else:
            self.error_label.config(text="Separate module names with space")

    
    # Create widgets in the application
    def setAppWidgets(self):
        self.cb = tk.Checkbutton(self.root, text="Move files from dev directory", font=self.text_font, \
                                 variable=self.move_files_from_dev_folder, onvalue=1, offvalue=0)
        self.main_label = tk.Label(self.root, text="Modules to use (options are gps, temp and ls)", font=self.text_font)
        self.modules_input = tk.Entry(self.root, font=self.text_font)
        self.button = tk.Button(self.root, text="Set files", font=self.text_font)
        self.hint_label = tk.Label(self.root, text="temp for temperature\nls for light sensitivity", font=self.text_font)
        self.info_label = tk.Label(self.root, font=self.text_font, fg="#2eb82e")
        self.error_label = tk.Label(self.root, font=self.text_font, fg="#fa0a00")
        self.button.bind("<Button-1>", self.setFiles) # Event handler for LMB click on the button

        for element in [self.cb, self.main_label, self.modules_input, self.button, self.hint_label, \
            self.info_label, self.error_label]:
            element.pack()
        
        self.modules_input.focus()


    def start(self):
        self.root.mainloop()


FileSetupApp().start()

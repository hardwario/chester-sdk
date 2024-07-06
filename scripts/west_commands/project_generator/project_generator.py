#
# Copyright (c) 2024 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

import yaml
import re
import os
import sys
from jinja2 import Environment, FileSystemLoader
from west import log
import subprocess
import colorama


class ProjectGenerator:
    def __init__(self, topdir, prj_folder_name):
        self.topdir = topdir
        self.project_name = ""
        self.prj_folder_name = prj_folder_name
        self.dir = self.check_vendor()
        self.app_dir = os.path.join(self.dir, "applications")
        self.prj_dir = os.path.join(self.app_dir, self.prj_folder_name)
        self.file_status = {"created": [], "updated": [], "error": [], "yaml_error": []}
        self.log = {"Template": None, "Variant": None}

        # Setup Jinja environment
        jinja_templates_dir = "/scripts/west_commands/project_generator/jinja_templates"
        jinja_templates_folder = os.path.join(
            self.topdir, "chester", *jinja_templates_dir.split("/")
        )
        self.env = Environment(
            loader=FileSystemLoader(jinja_templates_folder),
            extensions=["jinja2.ext.do"],
        )
        self.env.globals["log"] = {
            "wrn": self.log_warning_wrapper,
            "err": self.log_error_wrapper,
        }

    def clang_format_file(self, file_path):
        # Check if the file extension is.c or.h
        if not file_path.endswith((".c", ".h")):
            return
        try:
            # Run clang-format using subprocess
            subprocess.run(["clang-format", "-i", file_path], check=True)
        except Exception as e:
            log.err(f"Clang Format '{file_path}'. {e}")

    def group_by_dependency(self, data):
        try:
            # Create a dictionary to group parameters and commands by their 'depends_on' value
            grouped_data = {"parameters": {}, "commands": {}}

            # Group parameters and commands
            for key in grouped_data.keys():
                items = data.get(key) or []
                for item in items:
                    depends_on = item.get("depends_on", None)
                    grouped_data[key].setdefault(
                        depends_on if depends_on else "no_dependency", []
                    ).append(item)

            return grouped_data
        except Exception as e:
            log.err(f"'group_by_dependency' {e}")

    def log_warning_wrapper(self, message):
        log_result = log.wrn(message)
        return ""

    def log_error_wrapper(self, message):
        if not "prj.conf" in self.file_status["error"]:
            self.file_status["error"].append("prj.conf")
        self.file_status["yaml_error"].append(
            f"Feature '{message}' unknown. Check your project.yaml!"
        )
        return ""

    def handler_variant(self, project_data, variant):
        try:
            project_data["project"]["variant"] = variant.replace("-", " ")
            corr = False
            for app_variant in project_data["variants"]:
                if app_variant["name"].replace(
                    "CHESTER ", ""
                ).upper() == variant.upper().replace("-", " "):
                    corr = True
                    # Log
                    self.log["Variant"] = (
                        f"{variant.upper().replace('-', ' ')} (HARDWARIO VARIANT)"
                    )
                    project_data["project"]["fw_bundle"] = app_variant["fw_bundle"]
                    project_data["project"]["fw_version"] = app_variant["fw_version"]
                    project_data["project"]["fw_name"] = app_variant["name"]
                    if "features" in app_variant:
                        # Append applications variant features
                        for app_feature in app_variant["features"]:
                            if app_feature not in project_data["features"]:
                                project_data["features"].append(app_feature)
                    if "extras" in app_variant:
                        # Append applications variant extras
                        for app_feature in app_variant["extras"]:
                            if app_feature not in project_data["extras"]:
                                project_data["extras"].append(app_feature)
            # Remove duplicates from project_data['features']
            if "features" in project_data:
                project_data["features"] = list(set(project_data["features"]))
            # Remove duplicates from project_data['features']
            if "extras" in project_data:
                project_data["extras"] = list(set(project_data["extras"]))
            if corr:
                return project_data
            else:
                log.err(f"Variant '{variant}' not found. Check your project.yaml.")
                sys.exit(1)
        except Exception as e:
            log.err(f"No variant found in your project.yaml. Error: {e}")
            sys.exit(1)

    def validate_yaml(self, data_yaml):
        parameter = data_yaml["parameters"]
        # PARAMETERS
        for param in parameter:
            name = param.get("name")
            type_ = param.get("type")
            help_ = param.get("help")
            default = param.get("default")
            if type_ not in [
                "int",
                "float",
                "bool",
                "string",
                "enum",
                "array[bool]",
                "array[int]",
                "array[float]",
            ]:
                self.file_status["yaml_error"].append(
                    f"Parameter '{name}' 'type' {type_} is not valid."
                )
            if not name:
                self.file_status["yaml_error"].append(f"Parameter missing 'name'.")
            if not type_:
                self.file_status["yaml_error"].append(
                    f"Parameter '{name}' missing 'type'."
                )
            if not help_ and not type_ == "enum":
                self.file_status["yaml_error"].append(
                    f"Parameter '{name}' missing 'help'."
                )
            # INT
            if type_ == "int":
                min_val = param.get("min")
                max_val = param.get("max")
                if min_val is None or not isinstance(min_val, int):
                    self.file_status["yaml_error"].append(
                        f"Parameter '{name}' missing or invalid 'min'."
                    )
                if max_val is None or not isinstance(max_val, int):
                    self.file_status["yaml_error"].append(
                        f"Parameter '{name}' missing or invalid 'max'."
                    )
                if default is None or not isinstance(default, int):
                    self.file_status["yaml_error"].append(
                        f"Parameter '{name}' missing or invalid 'default'."
                    )
                if min_val is not None and max_val is not None and min_val > max_val:
                    self.file_status["yaml_error"].append(
                        f"Parameter '{name}' has 'min' greater than 'max'."
                    )
                if default > max_val or default < min_val:
                    self.file_status["yaml_error"].append(
                        f"Parameter '{name}' 'default' isn't in range [{min_val}...{max_val}]."
                    )
            # FLOAT
            elif type_ == "float":
                min_val = param.get("min")
                max_val = param.get("max")
                if min_val is None or not isinstance(min_val, (int, float)):
                    self.file_status["yaml_error"].append(
                        f"Parameter '{name}' missing or invalid 'min'."
                    )
                if max_val is None or not isinstance(max_val, (int, float)):
                    self.file_status["yaml_error"].append(
                        f"Parameter '{name}' missing or invalid 'max'."
                    )
                if min_val is not None and max_val is not None and min_val > max_val:
                    self.file_status["yaml_error"].append(
                        f"Parameter '{name}' has 'min' greater than 'max'."
                    )
                if default is None or not isinstance(default, (int, float)):
                    self.file_status["yaml_error"].append(
                        f"Parameter '{name}' missing or invalid 'default'."
                    )
                if default > max_val or default < min_val:
                    self.file_status["yaml_error"].append(
                        f"Parameter '{name}' 'default' isn't in range [{min_val}...{max_val}]."
                    )
            # STRING
            elif type_ == "string":
                buffer_size = param.get("buffer_size")
                if buffer_size is None or not isinstance(buffer_size, int):
                    self.file_status["yaml_error"].append(
                        f"Parameter '{name}' missing or invalid 'buffer_size'."
                    )
                if buffer_size < len(default) + 1:
                    self.file_status["yaml_error"].append(
                        f"Parameter '{name}' 'buffer_size' is sized incorrectly."
                    )
                if default is None or not isinstance(default, str):
                    self.file_status["yaml_error"].append(
                        f"Parameter '{name}' missing or invalid 'default'."
                    )
            # BOOL
            elif type_ == "bool":
                if default is None or not isinstance(default, bool):
                    self.file_status["yaml_error"].append(
                        f"Parameter '{name}' missing or invalid 'default'."
                    )

            elif type_.startswith("array"):
                len_ = param.get("len")
                if len_ is None or not isinstance(len_, int):
                    self.file_status["yaml_error"].append(
                        f"Parameter '{name}' missing or invalid 'len'."
                    )

                elem_type = type_[6:-1]
                # ARRAY[INT]
                if elem_type == "int":
                    min_val = param.get("min")
                    max_val = param.get("max")
                    len_val = param.get("len")
                    if default is not None:
                        if len(default) != len_val:
                            self.file_status["yaml_error"].append(
                                f"Parameter '{name}' 'len' is sized incorrectly."
                            )
                    if min_val is None or not isinstance(min_val, int):
                        self.file_status["yaml_error"].append(
                            f"Parameter '{name}' missing or invalid 'min'."
                        )
                    if max_val is None or not isinstance(max_val, int):
                        self.file_status["yaml_error"].append(
                            f"Parameter '{name}' missing or invalid 'max'."
                        )
                    if default is None:
                        pass  # default is None, which is allowed
                    elif isinstance(default, list):
                        for item in default:
                            if not isinstance(item, int):
                                self.file_status["yaml_error"].append(
                                    f"Parameter '{name}' has non-int element in 'default'."
                                )
                            if item > max_val or item < min_val:
                                self.file_status["yaml_error"].append(
                                    f"Parameter '{name}' 'default' isn't in range [{min_val}...{max_val}]."
                                )
                    else:
                        self.file_status["yaml_error"].append(
                            f"Parameter '{name}' missing or invalid 'default'."
                        )
                # ARRAY[FLOAT]
                elif elem_type == "float":
                    min_val = param.get("min")
                    max_val = param.get("max")
                    len_val = param.get("len")
                    if default is not None:
                        if len(default) != len_val:
                            self.file_status["yaml_error"].append(
                                f"Parameter '{name}' 'len' is sized incorrectly."
                            )
                    if min_val is None or not isinstance(min_val, (int, float)):
                        self.file_status["yaml_error"].append(
                            f"Parameter '{name}' missing or invalid 'min'."
                        )
                    if max_val is None or not isinstance(max_val, (int, float)):
                        self.file_status["yaml_error"].append(
                            f"Parameter '{name}' missing or invalid 'max'."
                        )
                    if default is None:
                        pass  # default is None, which is allowed
                    elif isinstance(default, list):
                        for item in default:
                            if not isinstance(item, (int, float)):
                                self.file_status["yaml_error"].append(
                                    f"Parameter '{name}' has non-float element in 'default'."
                                )
                            if item > max_val or item < min_val:
                                self.file_status["yaml_error"].append(
                                    f"Parameter '{name}' 'default' isn't in range [{min_val}...{max_val}]."
                                )
                    else:
                        self.file_status["yaml_error"].append(
                            f"Parameter '{name}' missing or invalid 'default'."
                        )

                # ARRAY[BOOL]
                elif elem_type == "bool":
                    len_val = param.get("len")
                    if default is not None:
                        if len(default) != len_val:
                            self.file_status["yaml_error"].append(
                                f"Parameter '{name}' 'len' is sized incorrectly."
                            )
                    if default is None:
                        pass  # default is [{min_val}...{max_val}] None, which is allowed
                    elif isinstance(default, list):
                        for item in default:
                            if not isinstance(item, bool):
                                self.file_status["yaml_error"].append(
                                    f"Parameter '{name}' has non-bool element in 'default'."
                                )
                    else:
                        self.file_status["yaml_error"].append(
                            f"Parameter '{name}' missing or invalid 'default'."
                        )
            # ENUM
            if type_ == "enum":
                valueset = param.get("valueset")
                if not valueset:
                    self.file_status["yaml_error"].append(
                        f"Parameter '{name}' missing 'valueset'."
                    )
                if not isinstance(valueset, list):
                    self.file_status["yaml_error"].append(
                        f"Parameter '{name}' 'valueset' is not a list."
                    )
                for value in valueset:
                    if not isinstance(value, str):
                        self.file_status["yaml_error"].append(
                            f"Parameter '{name}' 'valueset' contains non-string value '{value}'."
                        )
                if param.get("related"):
                    related = param.get("related")
                    if not isinstance(related, list):
                        self.file_status["yaml_error"].append(
                            f"Parameter '{name}' 'related' is not a list."
                        )
                    for rel in related:
                        rel_name = rel.get("name")
                        rel_default = rel.get("default")
                        rel_help = rel.get("help")
                        if not rel_name:
                            self.file_status["yaml_error"].append(
                                f"Parameter '{name}' related parameter missing 'name'."
                            )
                        if not rel_default:
                            self.file_status["yaml_error"].append(
                                f"Parameter '{name}' related parameter '{rel_name}' missing 'default'."
                            )
                        if not rel_help:
                            self.file_status["yaml_error"].append(
                                f"Parameter '{name}' related parameter '{rel_name}' missing 'help'."
                            )
                        if rel_default not in valueset:
                            self.file_status["yaml_error"].append(
                                f"Parameter '{name}' related parameter '{rel_name}' 'default' '{rel_default}' is not in 'valueset'"
                            )

        # COMMANDS
        if data_yaml["commands"]:
            commands = data_yaml["commands"]
            for command in commands:
                name = command.get("name")
                callback = command.get("callback")
                help_ = command.get("help")
                if not name or name is None:
                    self.file_status["yaml_error"].append(f"Command missing 'name'.")
                if not callback or callback is None:
                    self.file_status["yaml_error"].append(
                        f"Command '{name}' missing 'callback'."
                    )
                if not help_ or help_ is None:
                    self.file_status["yaml_error"].append(
                        f"Command '{name}' missing 'help'."
                    )
                if not "(" in callback or ")" not in callback:
                    self.file_status["yaml_error"].append(
                        f"Command '{name}' 'callback' missing parentheses '()'."
                    )

        # EXTRAS
        if data_yaml["extras"]:
            extras = data_yaml["extras"]
            for extra in extras:
                if not "=" in extra:
                    self.file_status["yaml_error"].append(
                        f"Extra '{extra}' missing '='."
                    )

    def check_vendor(self):
        # Checks if vendor folder exists
        dir_app_vendor = os.path.join(self.topdir, "vendor")
        dir_app_chester = os.path.join(self.topdir, "chester")
        if "vendor" in os.listdir(self.topdir):
            return dir_app_vendor
        else:
            return dir_app_chester

    def generate_project_folder(self, app_dir, project_name):
        # Create the project directory if it doesn't exist
        project_dir = os.path.join(app_dir, project_name)
        if not os.path.exists(project_dir):
            os.makedirs(project_dir)
        else:
            log.wrn("This project name already exists")
        return project_dir

    def write_to_file(self, list, output_file, type):
        # Write in a file
        with open(output_file, type) as file:
            for line in list:
                file.write(line)

    def transform_to_slug(self, text):
        # Convert to lower
        slug = text.lower()

        # Remove special caracters
        slug = re.sub(r"[^a-zA-Z0-9\s]", "", slug)

        # Replace spaces with hyphens
        slug = slug.replace(" ", "-")
        return slug

    def cbor(self):
        data = self.yaml_source(self.prj_folder_name, "app", "project.yaml")
        decoder = self.yaml_source(self.prj_folder_name, "cbor", "cbor-decoder.yaml")
        encoder = self.yaml_source(self.prj_folder_name, "cbor", "cbor-encoder.yaml")
        merged_dict = {"decoder": decoder, "encoder": encoder, **data}
        # Logs
        self.project_name = data["project"]["fw_name"]

        try:
            project_name = self.transform_to_slug(data["project"]["fw_name"])
        except:
            log.err(
                "Project 'name' not found in project.yaml. Check your project.yaml."
            )
            sys.exit(1)  # Close run

        # Generate app_cbor.c
        self.generate_file(
            "src",
            "app_cbor_test.c",
            "app_cbor_v2_c.j2",
            **merged_dict,
        )
        # Generate app_cbor.c
        self.generate_file(
            "src",
            "app_cbor_test.h",
            "app_cbor_v2_h.j2",
            **merged_dict,
        )
        # Logs
        self.logs_print()
        return True

    def generate_cmake(self, data):
        try:
            # Load template
            template = self.env.get_template("CMakeLists.j2")
            # CMake path
            cmake_file_path = os.path.join(self.prj_dir, "CMakeLists.txt")
            # Walking into files
            sources = []
            src_dir = os.path.join(self.prj_dir, "src")
            for root, dirs, files in os.walk(src_dir):
                # Collect sources
                for file in files:
                    if file.endswith(".c") or file == "msg_key.h" or file == "app_codec.h":
                        file_path = os.path.relpath(os.path.join(root, file), src_dir)
                        sources.append(file_path)
            # Render the template with data
            rendered_template = template.render(
                project_name=self.project_name,
                sources=sources,
                data=data,
            )
            if not os.path.exists(cmake_file_path):
                try:
                    # Write
                    with open(cmake_file_path, "w") as f:
                        f.write(rendered_template)
                    # Log
                    self.file_status["created"].append("CMakeLists.txt")
                except:
                    # Log
                    self.file_status["error"].append("CMakeLists.txt")
            else:
                try:
                    # Opening destination file
                    with open(cmake_file_path, "r") as file:
                        existing_content = file.read()
                    # Preserved code blocks
                    rendered_template = self.handle_preserved_blocks(
                        existing_content, rendered_template, "CMakeLists.txt"
                    )
                    # Write
                    with open(cmake_file_path, "w") as f:
                        f.write(rendered_template)
                    # Log
                    self.file_status["updated"].append("CMakeLists.txt")
                except:
                    # Log
                    self.file_status["error"].append("CMakeLists.txt")
        except Exception as e:
            log.err(f"CMakeLists.txt unsuccessfully created. Error: {e}")

    def yaml_source(self, name, mode, ext):
        if mode == "app":
            # YAML file
            yaml_dir = os.path.join(self.prj_dir, ext)
        elif mode == "cbor":
            # YAML file
            codec_dir = os.path.join(self.prj_dir, "codec")
            yaml_dir = os.path.join(codec_dir, ext)
        elif mode == "apps":
            apps_dir = os.path.join(self.app_dir, name)
            yaml_dir = apps_dir
        try:
            if ext != "cbor-encoder.yaml":
                with open(yaml_dir, "r") as stream:
                    data = yaml.safe_load(stream)
                    return data
            else:
                try:
                    with open(yaml_dir, "r") as stream:
                        data = yaml.safe_load(stream)
                        return data
                except:    
                    log.wrn('The cbor-encoder.yaml file was not found in the project folder.')          
                    return None

        except Exception as e:
            log.err(f"The {ext} file was not found in the project folder. Error: {e}")
            sys.exit(1)
            data = None

    def handle_preserved_blocks(self, existing_content, new_content, out_dir):
        try:
            # Define patterns and block templates based on file extension
            if out_dir.split(".")[-1] in ["c", "h", "overlay"]:
                file_extension = "c"
            else:
                file_extension = "other"
            patterns = {
                "c": {
                    "PATTERN": r"/\* ### Preserved code \"(.*?)\" \(begin\) \*/(.*?)\s*/\* \^\^\^ Preserved code \"\1\" \(end\) \*/",
                    "BLOCK": lambda block_name: rf"/\* ### Preserved code \"{block_name}\" \(begin\) \*/(.*?)/\* \^\^\^ Preserved code \"{block_name}\" \(end\) \*/",
                    "NEW_BLOCK": lambda block_name, block_content: f'/* ### Preserved code "{block_name}" (begin) */{block_content}',
                    "END_BLOCK": lambda indentation, block_name: f'{indentation}/* ^^^ Preserved code "{block_name}" (end) */',
                },
                "other": {
                    "PATTERN": r"# ### Preserved code \"(.*?)\" \(begin\)(.*?)\s*# \^\^\^ Preserved code \"\1\" \(end\)",
                    "BLOCK": lambda block_name: rf"# ### Preserved code \"{block_name}\" \(begin\)(.*?)# \^\^\^ Preserved code \"{block_name}\" \(end\)",
                    "NEW_BLOCK": lambda block_name, block_content: f'# ### Preserved code "{block_name}" (begin){block_content}',
                    "END_BLOCK": lambda indentation, block_name: f'{indentation}# ^^^ Preserved code "{block_name}" (end)',
                },
            }

            pattern = patterns.get(file_extension, {}).get("PATTERN")
            block = patterns.get(file_extension, {}).get("BLOCK")
            new_block = patterns.get(file_extension, {}).get("NEW_BLOCK")
            end_block = patterns.get(file_extension, {}).get("END_BLOCK")

            if pattern and block and new_block and end_block:
                # Find and capture all preserved code blocks in existing content
                preserved_blocks = re.findall(
                    pattern, existing_content, flags=re.DOTALL
                )
                # Replace preserved code blocks in the new content
                for block_name, block_content in preserved_blocks:
                    # Find corresponding block in new content
                    block_pattern = block(block_name)
                    new_block_content = new_block(block_name, block_content)
                    if block_content.strip():  # If block content is not empty
                        indentation = re.match(r"^(\s*)", block_content).group(1)
                    else:  # If block content is empty, copy indentation from existing code
                        existing_block_match = re.search(
                            block_pattern, new_content, flags=re.DOTALL
                        )
                        if existing_block_match:
                            existing_block_content = existing_block_match.group(1)
                            indentation = re.match(
                                r"^(\s*)", existing_block_content
                            ).group(1)
                    new_block_content += end_block(indentation, block_name)
                    new_content = re.sub(
                        block_pattern, new_block_content, new_content, flags=re.DOTALL
                    )
                return new_content

        except Exception as e:
            log.err(f"Preserved code: {e}")
            return None

    def generate_file(
        self,
        src_dir,
        out_dir,
        jinja_path,
        **kwargs,
    ):
        # Sorting
        if kwargs["features"] is not None:
            kwargs["features"] = sorted(kwargs["features"])
        if kwargs["extras"] is not None:
            kwargs["extras"] = sorted(kwargs["extras"])

        try:
            # Load template
            template = self.env.get_template(jinja_path)
            # Render the template with data
            rendered_code = template.render(
                setting_pfx=self.project_name,
                data=kwargs,
                data_dependency=self.group_by_dependency(kwargs),
            )
            # Dir source
            src_dir = os.path.join(self.prj_dir, src_dir)
            if not os.path.exists(src_dir):
                os.makedirs(src_dir)

            destination = os.path.join(src_dir, out_dir)
            if not os.path.exists(destination):
                # Write rendered code to destination file
                self.write_to_file(rendered_code, destination, "w")
                self.clang_format_file(destination)
                self.file_status["created"].append(out_dir)
            else:
                # Opening destination file
                with open(destination, "r") as file:
                    existing_content = file.read()

                # Preserved code blocks
                new_content = self.handle_preserved_blocks(
                    existing_content, rendered_code, out_dir
                )

                # Write modified content to destination file
                self.write_to_file(new_content, destination, "w")
                self.clang_format_file(destination)
                if not out_dir in self.file_status["error"]:
                    self.file_status["updated"].append(out_dir)

        except Exception as e:
            self.file_status["error"].append(f"{out_dir} : {e}")
            return exit

    def logs_print(self):
        # Sort
        for status in ["created", "updated", "error", "yaml_error"]:
            self.file_status[status].sort(key=lambda x: x.lower())

        # Project log
        log.msg(f"Project: {self.project_name}", color=colorama.Fore.LIGHTWHITE_EX)
        for key in ["Variant", "Template"]:
            if self.log.get(key):
                log.msg(f"{key}: {self.log[key]} ", color=colorama.Fore.LIGHTWHITE_EX)
        log.msg(f"Path: {self.prj_dir}", color=colorama.Fore.LIGHTWHITE_EX)

        # Successful creation log information
        for status, color in [
            ("created", colorama.Fore.LIGHTGREEN_EX),
            ("updated", colorama.Fore.LIGHTCYAN_EX),
            ("error", colorama.Fore.RED),
            ("yaml_error", colorama.Fore.RED),
        ]:
            files = self.file_status.get(status, [])
            if len(files) > 0:
                words = status.split("_")
                # Capitalize the first letter of each word
                capitalized_status = [word.capitalize() for word in words]
                # Join the words back with underscores
                status = " ".join(capitalized_status)
                log.msg(f"{status}:", color=color)
                half_index = (len(files) + 1) // 2
                column1, column2 = files[:half_index], files[half_index:]
                for i in range(half_index):
                    col1_item, col2_item = (
                        column1[i] if i < len(column1) else "",
                        column2[i] if i < len(column2) else "",
                    )
                    col1_display, col2_display = (
                        f"• {col1_item}" if col1_item else "",
                        f"• {col2_item}" if col2_item else "",
                    )
                    log.msg(
                        f"{col1_display:<20} {col2_display}",
                        color=color,
                    )

    def create(self, template):
        # Setup dirs
        prj_dir = self.generate_project_folder(self.app_dir, self.prj_folder_name)

        if template == None:
            template = "minimal"
        else:
            template = template.replace("-", "_")

        # Load YAML
        yaml_dir = os.path.join(prj_dir, "project.yaml")

        try:
            # Load template
            template = self.env.get_template("template_" + template + ".j2")
        except:
            log.err(
                f"Template {template} not found. Check the templates available '--list'."
            )

        # Render the template with data
        rendered_template = template.render(project_name=self.prj_folder_name)
        if not os.path.exists(yaml_dir):
            try:
                with open(yaml_dir, "w") as f:
                    f.write(rendered_template)
            except:
                log.err("Problem during project.yaml generation")
                sys.exit(1)
        else:
            log.err(
                "The project.yaml file already exists in your project folder. Aborting..."
            )
            sys.exit(1)
            return False

        # Log
        data = self.yaml_source(self.prj_folder_name, "app", "project.yaml")
        self.project_name = data["project"]["fw_name"]
        self.log["Variant"] = data["project"]["variant"]

        self.logs_print()

        return True

    def create_scratch(self, variant):
        # Setup data
        data = self.yaml_source(self.prj_folder_name, "app", "project.yaml")
        # Log
        self.log["Variant"] = f"{data['project']['variant']} (PRIVATE VARIANT)"
        if variant != None:
            data = self.handler_variant(project_data=data, variant=variant)
        try:
            self.project_name = self.transform_to_slug(data["project"]["fw_name"])
        except:
            log.err(
                "Field 'project' 'fw_name' not found in project.yaml. Check your project.yaml."
            )
            sys.exit(1)  # Close run

        try:
            # List of files to generate
            files_to_generate = [
                {"dir": "src", "name": "app_send.c", "template": "app_send_c.j2"},
                {"dir": "src", "name": "app_send.h", "template": "app_send_h.j2"},
                {"dir": "src", "name": "app_handler.c", "template": "app_handler_c.j2"},
                {"dir": "src", "name": "app_handler.h", "template": "app_handler_h.j2"},
                {"dir": "src", "name": "app_init.c", "template": "app_init_c.j2"},
                {"dir": "src", "name": "app_init.h", "template": "app_init_h.j2"},
                {"dir": "src", "name": "app_data.c", "template": "app_data_c.j2"},
                {"dir": "src", "name": "app_data.h", "template": "app_data_h.j2"},
                {"dir": "src", "name": "app_sensor.c", "template": "app_sensor_c.j2"},
                {"dir": "src", "name": "app_sensor.h", "template": "app_sensor_h.j2"},
                {"dir": "src", "name": "app_power.c", "template": "app_power_c.j2"},
                {"dir": "src", "name": "app_power.h", "template": "app_power_h.j2"},
                {"dir": "src", "name": "app_work.c", "template": "app_work_c.j2"},
                {"dir": "src", "name": "app_work.h", "template": "app_work_h.j2"},
                {"dir": "src", "name": "app_cbor.c", "template": "app_cbor_c.j2"},
                {"dir": "src", "name": "app_cbor.h", "template": "app_cbor_h.j2"},
                {
                    "dir": "child_image",
                    "name": "mcuboot.conf",
                    "template": "mcuboot_conf.j2",
                },
                {
                    "dir": "codec",
                    "name": "cbor-decoder.yaml",
                    "template": "cbor_decoder_yaml.j2",
                },
                {"dir": "src", "name": "msg_key.h", "template": "msg_key.j2"},
                {"dir": "src", "name": "main.c", "template": "main_c.j2"},
            ]

            # Generate each file
            for file_info in files_to_generate:
                self.generate_file(
                    file_info["dir"], file_info["name"], file_info["template"], **data
                )

            if len(self.file_status["error"]) == 0:
                return True
            else:
                return False

        except Exception as e:
            log.err(e)
            return False

    def update(self, variant):
        # Setup data
        data = self.yaml_source(self.prj_folder_name, "app", "project.yaml")

        # Log
        self.log["Variant"] = f"{data['project']['variant']} (CUSTOM VARIANT)"

        # Handle data
        if variant != None:
            data = self.handler_variant(project_data=data, variant=variant)
        try:
            self.project_name = self.transform_to_slug(data["project"]["fw_name"])
        except:
            log.err(
                "Field project 'fw_name' not found in project.yaml. Check your project.yaml."
            )
            sys.exit(1)  # Close run

        # Validade yaml
        self.validate_yaml(data)
        if self.file_status["yaml_error"]:
            raise ValueError()

        try:
            # List of files to generate
            files_to_generate = [
                {
                    "dir": "src",
                    "name": "app_config.c",
                    "template": "app_config_new_c.j2",
                },
                {
                    "dir": "src",
                    "name": "app_config.h",
                    "template": "app_config_new_h.j2",
                },
                {"dir": "src", "name": "app_shell.c", "template": "app_shell_new_c.j2"},
                {"dir": "", "name": "prj.conf", "template": "prj_conf.j2"},
                {"dir": "", "name": "app.overlay", "template": "app_overlay.j2"},
                {"dir": "src", "name": "feature.h", "template": "feature_h.j2"},
                {"dir": "src", "name": "variant.h", "template": "variant_h.j2"},
                {"dir": "", "name": "Kconfig", "template": "k_config.j2"},
            ]

            # Generate each file
            for file_info in files_to_generate:
                self.generate_file(
                    file_info["dir"], file_info["name"], file_info["template"], **data
                )

            # Generate CMakeLists.txt
            self.generate_cmake(data)

            # Update Log
            self.project_name = data["project"]["fw_name"]

            if len(self.file_status["error"]) == 0:
                return True
            else:
                return False

        except:
            return False

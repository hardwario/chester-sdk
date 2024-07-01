#
# Copyright (c) 2024 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

from textwrap import dedent
from west.commands import WestCommand
from west import log
from west.configuration import config
from west.manifest import Manifest
from west.util import WestNotFound
import os
import sys
from project_generator import ProjectGenerator
import colorama


def get_west_workspace_root():
    try:
        # Load the manifest. This operation will fail if not run inside a west workspace.
        manifest = Manifest.from_file()

        # The topdir attribute of the manifest object gives you the absolute path to the workspace root.
        return manifest.topdir

    except WestNotFound:
        # Handle the case where the script is not run inside a west workspace
        log.err(
            "Error: Not inside a west workspace. Please run this within a west-managed directory."
        )
        sys.exit(1)


class ProjectUpdate(WestCommand):

    def __init__(self):
        super().__init__(
            "chester-update",  # gets stored as self.name
            "Update Hardwario Skeleton Project based in a YAML",  # self.help
            # self.description:
            dedent(
                """
            This helper command updates Hardwario Skeleton Project based in a YAML."""
            ),
        )

    def do_add_parser(self, parser_adder):
        parser = parser_adder.add_parser(
            self.name, help=self.help, description=self.description
        )
        parser.add_argument(
            "name",
            nargs="?",
            help="Name of the project",
            default=os.path.basename(os.getcwd()),
        )
        parser.add_argument("--variant", help="Variant of the project")
        return parser

    def do_run(self, args, unknown_args):
        topdir = get_west_workspace_root()
        try:
            project_generator = ProjectGenerator(topdir, prj_folder_name=args.name)
            if project_generator.update(variant=args.variant):
                project_generator.logs_print()
                log.msg(
                    "★ Project successfully updated.",
                    color=colorama.Fore.LIGHTWHITE_EX,
                )
            else:
                project_generator.logs_print()
                log.err("⊗ Project unsuccessfully updated.")
        except Exception as e:
            project_generator.logs_print()
            log.err(f"⊗ Project unsuccessfully updated. Error: {e}")


class ProjectInit(WestCommand):

    def __init__(self):
        super().__init__(
            "chester-init",  # gets stored as self.name
            "Initiate Hardwario Skeleton YAML",  # self.help
            # self.description:
            dedent(
                """
            This helper command initiates Hardwario Skeleton Project based in a YAML."""
            ),
        )

    def do_add_parser(self, parser_adder):
        parser = parser_adder.add_parser(
            self.name, help=self.help, description=self.description
        )
        parser.add_argument("name", nargs="?", help="Name of the project to be created")
        parser.add_argument(
            "--template", help="Template example for project.yaml creation"
        )
        parser.add_argument(
            "--list", action="store_true", help="List available templates"
        )
        return parser

    def do_run(self, args, unknown_args):
        topdir = get_west_workspace_root()
        if args.list:
            templates_dir = os.path.join(
                topdir,
                "chester",
                "scripts",
                "west_commands",
                "project_generator",
                "jinja_templates",
            )
            if not os.path.isdir(templates_dir):
                log.err(f"Error: {templates_dir} directory not found.")
                return

            log.inf("List of available templates:", colorize=True)
            templates = [
                file
                for file in os.listdir(templates_dir)
                if file.startswith("template_")
            ]
            for template in templates:
                log.msg(
                    f'• {template.replace("template_", "").replace(".j2", "")}',
                    color=colorama.Fore.LIGHTWHITE_EX,
                )
        else:
            if not args.name:
                log.err("<name> argument is required. Try `west chester-init <name>`")
                return
            try:
                project_generator = ProjectGenerator(topdir, prj_folder_name=args.name)
                if project_generator.create(template=args.template):
                    log.msg(
                        "★ Project successfully created.",
                        color=colorama.Fore.LIGHTWHITE_EX,
                    )
            except Exception as e:
                log.err(f"⊗ Project unsuccessfully created. Exception: {e}")


class ProjectCreate(WestCommand):

    def __init__(self):
        super().__init__(
            "chester-create",  # gets stored as self.name
            "Create Hardwario Skeleton Project based in a YAML",  # self.help
            # self.description:
            dedent(
                """
            This helper command creates Hardwario Skeleton Project based in a YAML."""
            ),
        )

    def do_add_parser(self, parser_adder):
        parser = parser_adder.add_parser(
            self.name, help=self.help, description=self.description
        )
        parser.add_argument(
            "name",
            nargs="?",
            help="Name of the project",
            default=os.path.basename(os.getcwd()),
        )
        parser.add_argument("--variant", help="Variant of the project")
        return parser

    def do_run(self, args, unknown_args):
        topdir = get_west_workspace_root()
        try:
            project_generator = ProjectGenerator(topdir, prj_folder_name=args.name)
            if project_generator.create_scratch(
                variant=args.variant
            ) and project_generator.update(variant=args.variant):
                project_generator.logs_print()
                log.msg(
                    "★ Project successfully created from scratch.",
                    color=colorama.Fore.LIGHTWHITE_EX,
                )
            else:
                log.err(
                    f"⊗ Project unsuccessfully created from scratch. Exception: {e}"
                )
        except Exception as e:
            project_generator.logs_print()
            log.err(f"⊗ Project unsuccessfully created from scratch. Exception: {e}")


class ProjectCbor(WestCommand):

    def __init__(self):
        super().__init__(
            "chester-cbor",  # gets stored as self.name
            "Update CBOR based in a YAML",  # self.help
            # self.description:
            dedent(
                """
            This helper command generates Hardwario Skeleton Project based in a YAML."""
            ),
        )

    def do_add_parser(self, parser_adder):
        parser = parser_adder.add_parser(
            self.name, help=self.help, description=self.description
        )
        parser.add_argument(
            "name",
            nargs="?",
            help="Name of the project",
            default=os.path.basename(os.getcwd()),
        )
        return parser

    def do_run(self, args, unknown_args):
        topdir = get_west_workspace_root()
        try:
            project_generator = ProjectGenerator(topdir, prj_folder_name=args.name)
            if project_generator.cbor():
                log.inf("★ Cbor successfully updated.", colorize=True)
        except Exception as e:
            log.err(f"⊗ Cbor unsuccessfully updated. Exception: {e}")

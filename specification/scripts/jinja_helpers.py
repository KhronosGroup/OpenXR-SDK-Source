#!/usr/bin/env python3 -i
#
# Copyright (c) 2019-2025 The Khronos Group Inc.
# Copyright (c) 2019 Collabora, Ltd.
#
# SPDX-License-Identifier: Apache-2.0

"""Provides functionality to use Jinja2 when generating C/C++ code, while eliminating the need to import Jinja2 from any other file."""

import re
from pathlib import Path

_ADDED_TO_PATH = False


def _add_to_path():
    global _ADDED_TO_PATH
    if not _ADDED_TO_PATH:
        import sys
        # Find Jinja2 in source tree, as last resort.
        sys.path.append(
            str(
                Path(__file__).resolve().parent.parent.parent / "external" /
                "python"))
        _ADDED_TO_PATH = True


_WHITESPACE = re.compile(r"[\s\n]+")


def _undecorate(name):
    """Undecorate a name by removing the leading Xr and making it lowercase."""
    lower = name.lower()
    assert lower.startswith('xr')
    return lower[2:]


def _quote_string(s):
    return f'"{s}"'


def _base_name(name):
    return name[2:]


def _collapse_whitespace(s):
    return _WHITESPACE.sub(" ", s)


def _protect_begin(entity):
    if entity.protect_value:
        return f"#if {entity.protect_string}"
    return ""


def _protect_end(entity):
    if entity.protect_value:
        return f"#endif // {entity.protect_string}"
    return ""

def _remove_prefix(s: str, prefix: str):
    if s.startswith(prefix):
        return s[len(prefix):]
    return s


def make_jinja_environment(file_with_templates_as_sibs=None, search_path=None):
    """Create a Jinja2 environment customized to generate C/C++ headers/code for Khronos APIs.

    Delimiters have been changed from Jinja2 defaults to permit better interoperability with
    editors and other tooling expecting C/C++ code, by combining them with comments:

    - Blocks are bracketed like /*% block_contents %*/ instead of {% block_contents %}
    - Variable outputs are bracketed like /*{ var }*/ instead of {{ var }}
    - Line statements start with //# instead of just #
    - Line comments start with //## instead of just ##

    Other details:

    - autoescape is turned off because this isn't HTML.
    - trailing newline kept
    - blocks are trimmed for easier markup.
    - the loader is a file system loader, building a search path from file_with_templates_as_sibs
      (if your template is a sibling of your source file, just pass file_with_templates_as_sibs=__file__),
      and search_path (an iterable if you want more control)

    Provided filters:

    - quote_string - wrap something in double-quotes
    - undecorate - same as the generator utility function: remove leading two-character API prefix and make lowercase.
    - base_name - just removes leading two-character API prefix
    - collapse_whitespace - minimizes internal whitespace with a regex

    Provided functions used as globals:

    - protect_begin(entity) and protect_end(entity) - use in a pair, and pass an entity
      (function/command, struct, etc), and if it is noted with protect="something" in the XML,
      the appropriate #if and #endif will be printed (for protect_begin and protect_end respectively.)

    You may further add globals and filters to the returned environment.
    """

    _add_to_path()
    from jinja2 import Environment, FileSystemLoader

    search_paths = []
    if file_with_templates_as_sibs:
        search_paths.append(
            str(Path(file_with_templates_as_sibs).parent))
    if search_path:
        search_paths.extend(search_path)
    env = Environment(keep_trailing_newline=True,
                      trim_blocks=True,
                      block_start_string="/*%",
                      block_end_string="%*/",
                      variable_start_string="/*{",
                      variable_end_string="}*/",
                      line_statement_prefix="//#",
                      line_comment_prefix="//##",
                      autoescape=False,
                      loader=FileSystemLoader(search_paths))
    env.filters['quote_string'] = _quote_string
    env.filters['undecorate'] = _undecorate
    env.filters['base_name'] = _base_name
    env.filters['collapse_whitespace'] = _collapse_whitespace
    env.filters['remove_prefix'] = _remove_prefix
    env.globals['protect_begin'] = _protect_begin
    env.globals['protect_end'] = _protect_end

    return env


class JinjaTemplate:
    def __init__(self, env, fn):
        """Load and parse a Jinja2 template given a Jinja2 environment and the template file name.

        Create the environment using make_jinja_environment().

        Syntax errors are caught, have their details printed, then are re-raised (to stop execution).
        """
        _add_to_path()
        from jinja2 import TemplateSyntaxError
        try:
            self.template = env.get_template(fn)
        except TemplateSyntaxError as e:
            print("Jinja2 template syntax error during parse: {}:{} error: {}".
                  format(e.filename, e.lineno, e.message))
            raise e

    def render(self, *args, **kwargs):
        """Render the Jinja2 template with the provided context.

        All arguments are passed through; this just wraps the Jinja2 template render method
        to handle syntax error exceptions so that Jinja2 does not need to be imported anywhere
        but this file.
        """
        _add_to_path()
        from jinja2 import TemplateSyntaxError

        try:
            return self.template.render(*args, **kwargs)
        except TemplateSyntaxError as e:
            print(
                "Jinja2 template syntax error during render: {}:{} error: {}".
                format(e.filename, e.lineno, e.message))
            raise e

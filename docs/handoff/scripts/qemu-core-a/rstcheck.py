"""Parse an RST file with docutils; Sphinx's code-block maps to code."""
import sys
import docutils.core
from docutils.parsers.rst import directives
from docutils.parsers.rst.directives.body import CodeBlock

directives.register_directive("code-block", CodeBlock)
src = open(sys.argv[1], encoding="utf-8").read()
docutils.core.publish_doctree(
    src, settings_overrides={"report_level": 1, "halt_level": 2})
print("RST OK:", sys.argv[1])

#!/usr/bin/env python
#
# Copyright (c) 2012, JT Olds <hello@jtolds.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

"""
  Pants
  http://www.pants-lang.org/

  Main module
"""

__author__ = "JT Olds"
__author_email__ = "hello@jtolds.com"

import sys
from ast.parse import parse
from ir.convert import convert
from cps.transform import transform
from cps.cleanup import cleanup
from ir.types import null_val

def main(argv):
  sys.setrecursionlimit(10000)
  print cleanup(transform(convert(parse(sys.stdin)))).format()

if __name__ == "__main__":
  sys.exit(main(sys.argv))

#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# @file getopt.q GetOpt example program

/*  getopt.q Copyright 2010 - 2023 David Nichols

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

%modern

sub usage() {
    printf(
"usage: %s [options]
  -s,--string=ARG  returns a list of strings in 'string'
  -i,--int=ARG     returns an added integer sum of the arguments in 'int'
  -f,--float=ARG   returns a floating point value in 'float'
  -b,--bool=ARG    returns a boolean value in 'bool'
  -d,--date=ARG    returns a date value in 'date'
  -t               returns a boolean value in 'test'
  -o,--opt[=ARG]   returns an integer sum of arguments in 'opt'\n",
        get_script_name());
    exit(1);
}

const Opts =
    ( # --string,-s will give a list of strings
      "string"  : "string,s=s@",
      # --int,-i    will give an added integer sum of the arguments
      "int"     : "int,i=i+",
      # --float,-f  will give a floating point value
      "float"   : "float,f=f",
      # --bool,-b   will give a boolean value
      "bool"    : "bool,b=b",
      # --date,-d   will give a date value
      "date"    : "date,d=d",
      # -t          will give a boolean value
      "test"    : "t",
      # --opt,-o    will give an integer sum of arguments
      "opt"     : "opt,o:i+",
      "help"    : "help,h",
    );

sub process_command_line() {
    if (!elements ARGV)
        usage();

    GetOpt g(Opts);

    # NOTE: by passing a reference to the list, the arguments parsed will be removed from the list
    # NOTE: calling GetOpt::parse3() means that errors will cause the script to exit immediately
    #       with an informative message
    hash o = g.parse3(\ARGV);
    if (o.help)
        usage();
    print("qore GetOpt class test script\n");
    printf("o: %N\n", o);
    printf("ARGV: %N\n", ARGV);
}

process_command_line();

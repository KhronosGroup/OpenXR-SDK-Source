# HTML Diff Script
This is a first cut at a script to compare HTML specifications.
Usage is simply:
```
htmldiff file1.html file2.html > diff.html
```
The script does not copy any CSS and images required by the input specs,
so it's best to generate the output in the same directory as one of the inputs.

The scripts used require Python and Perl.
Additionally, the python 'utidylib' module and the underlying libtidy C library are required,
which may make it challenging to run the scripts on non-Linux platforms.
It is not known if these requirements can easily be removed.

## Usage Notes
- For Debian/Ubuntu distros, install the `python-utidylib` package, which should also install the `libtidy` packages.
- The directory containing the `htmldiff.pl` Perl script needs to be in the `PATH`.

The scripts are taken from the code backing the

    http://services.w3.org/htmldiff

website.

## Files
* `htmldiff` is the Python driver script.
* `htmldiff.pl` is the Perl script which generates the diff after
preprocessing of the input HTML by `htmldiff`.
* `htmldiff.orig` is the original Python script from
the website as a CGI script.
This script was modified to run at the command line and saved as `htmldiff`.

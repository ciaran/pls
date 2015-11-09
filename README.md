pls
====

Implore a command to work.

![pls](https://raw.githubusercontent.com/ciaran/pls/gh-pages/examples.gif)

Description
-----------

Run a specified command and open printed file/line numbers in your editor.

`pls` runs the utility given on the command line, and checks each line in the output for a file/line number. On termination of the utility, a file can be selected to open.


Examples
--------

  - Run a `make` build and look for errors:

    ```
    pls make
    ```

  - Run `mocha` tests and ignore system scripts in selection:

    ```
    pls mocha -e
    ```

  - Run a `make` build task and specify a search path for opening files:

    ```
    pls make -p src
    ```

  - Check for errors in a log file:

    ```
    pls < errors.log
    ```

Compilation
-----------

`pls` requires the PCRE development headers, which can be installed using your
 system’s package manager, e.g.:

 `brew install pcre`

 `apt-get install libpcre3-dev`

With PCRE available a simple `make` should suffice.

Configuration
-------------

### Editor

By default `pls` will try to infer how to open selected files based on your `$EDITOR` value, falling back on `vim` if it can’t recognise one.

You can specify an editor command using the `$PLS` environment variable. The first `%d` will be replaced with the line number, and the second `%d` with the column number (or 0 if not available). If a `%s` is present it will be replaced with the filename, otherwise the file will be appended to the end of the command.

For example:

   - ```vim +'call cursor(%d, %d)'```


### Patterns

A few line matching patterns are included by default, but more can be added by creating a `~/.plsrc` file, with one regular expression pattern per line (with no delimiters).

A pattern should have up to 3 captures – the first being the filename, the second the line number, and the third the column number.


Options
-------

  - `-a`: Show selection interface even if utility exits with 0 status

  - `-l`: Set initial selection to the last matched path

  - `-e`: Only select existing filenames
      Useful if build output may include system header files etc. which you don’t want to include in the selection list.

  - `-p`: Add path to the list of directories searched for selected files
    This can be used when files may be in include paths.


Thanks
------

Includes some code from [yank](https://github.com/mptre/yank), by [Anton Lindqvist](https://github.com/mptre).

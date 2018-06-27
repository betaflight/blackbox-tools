#!/bin/sh

astyle --style=kr --recursive $(pwd)/src/\*.h $(pwd)/src/\*.c $(pwd)/test/\*.h $(pwd)/test/\*.c --indent=spaces=4 --indent-after-parens

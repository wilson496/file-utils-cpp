*******************************************************************************

README.txt
CSC 360 - Fall 2019
Programming Assignment 3 (p3)

Author: Cameron Wilson
Created: 2019-11-29

*******************************************************************************



Files Included
-------------------------------------------------------------------------------
- diskinfo.cpp
- disklist.cpp
- diskget.cpp
- diskput.cpp
- MAKEFILE
- README.txt

diskfix.cpp is not part of this submission as it was not implemented.

NOTE: This has been tested on linux.csc.uvic.ca and designed to work primarily within that environment
      and should have no issue running on that server provided the instructions are followed.
      

Each executable will include the list of arguments required for it to function. 

Failure to provide the exact arguments in the exact specific order will result in the program(s) failing.

Comments are provided within the code detailing the design of the program as well as
the functionality of each section of code. 

Test images are provided in the tests/ directory. I may have heard Dr. Pan indicate that
we should provide the test image for the markers. If not, feel free to ignore them.

The tests directory includes test.img from the lecture resources folder and test-utc.img, which
Dr. Pan posted in the Connex forums.



Programs that support subdirectories will be highlighted within the Deployment instructions.


Deployment Instructions
-------------------------------------------------------------------------------
1. Open Terminal on UNIX OS

2. Navigate to directory containing submission files.

3. Compile submission files with the following command:

        make

4. To run each file, use the following respective format for each program:

        For diskinfo:
            ./diskinfo <disk image file>

        For disklist:
            ./disklist <disk image file>

        Subdirectories? No. disklist can list subdirectories IN the ROOT folder, but cannot list contents within subdirectories.

        For diskget:
            ./diskget <disk image file> <file copying from on disk image file> <file copying to in current directory>

        Subdirectories? No. This is not supported. diskget can only retrieve files from the root directory.

        For diskput:
            ./diskput <disk image file> <local file to write to disk image> <name of written local file on disk image>

        Subdirectories? No. This is not supported. diskput can only write files to the root directory.

   Where <disk image file> is the disk image provided that provides the file system being analyzed.

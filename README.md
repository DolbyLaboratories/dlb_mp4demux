# dlb_mp4demux

The dlb_mp4demux is a library developed by Dolby under the BSD-3 license. The library can cooperate with frontend program to generate a tool to efficiently demultiplex ISO base media file format (aka mp4 container) into elementary streams (including AC3, EC3, AC4 and DoVi).

## Getting Started

These instructions help you get a copy of the project up and running on your local machine for development and testing purposes.

### Folder Structure

The "dlb_mp4demux" folder consists of:

- README.md        This file.
- doc/             Doxygen documentation of the dlb_mp4demux.
- frontend/        MP4Demuxer frontend.
- include/         Necessary header files of the dlb_mp4demux library.
- make/            Makefiles and Visual Studio projects/solutions for building the Dolby MP4 demultiplexer library with frontends and test harnesses.
- src/             Contains the MP4 demultiplexer source code.
- test/            Test harnesses for unit and developer system tests including test signals.

### Prerequisites

For Windows platform development, Visual Studio 2010 must be installed with SP1.

### Building instructions

#### Using the makefiles (on Linux)

    After cloning the dlb_mp4demux repository to your local machine, go to the appropriate directory, depending on your machine OS and architecture, such as:
    "cd dlb_mp4demux/make/mp4demuxer<architecture>"

    Then build one of the following make targets:
    "make mp4demuxer_release"
    "make mp4demuxer_debug"

#### Using the Visual Studio Solutions(on Windows)

    From a Visual Studio 2010 command line window:
    Go to a directory of your choice:
    "cd dlb_mp4demux\make\mp4demuxer\windows<architecture>"
	"devenv mp4demuxer_2010.sln /rebuild debug/release"

## Release Note

See the [ReleaseNote](ReleaseNote) file for details
	
## License

This project is licensed under the BSD-3 License - see the [LICENSE](LICENSE) file for details
# DOLBY MP4 STREAMING DEMUXER LIBRARARY

The Dolby MP4 streaming demuxer (dlb_mp4demux) lib is a software implementation
of a demuxer of fragmented or unfragmented mp4 file format. 
It is designed for use on architectures with limited resources.

## VERSION HISTORY

v1.0, 01 Jul 2017, Initial Version

## STRUCTURE

  ./README.txt
   - This file.

  ./make/
    - Library project files.

  ./include/
   - Public header files of the 'mp4d' library.
  ./include/mp4d_types.h
   - mp4d data types used across different layers of the API.
  ./include/mp4d_demux.h
  ./include/mp4d_trackreader.h
   - High level APIs for movie and track access
  ./include/mp4d_nav.h
  ./include/mp4d_buffer.h
   - Low level APIs for atom handling and box parsing.

  ./src/
   - The source code of the 'mp4d' library.

  ./frontend/mp4demuxer.c
   - Implementation of an MP4 file demuxer tool. It uses the
     'mp4d' library. It writes media data into elementary stream files.

  ./test/
   - Contains unit test executables and Unittest system tests of the package.
  
## Library Features:

  * Single pass demuxing for fragmented or unfragmented files
  * API providing access to demux, trackreader, and box reader (navigator)
    objects, all multi-instantiable
  * Robust implementation to skip unknown boxes, ignore unrecognized content in
    known boxes, return errors on corrupt data.
  * Read and demux the following formats:
      fragmented or unfragmented MP4 File Format(.mp4, .m4a, .m4v)
  * Extract media samples in the following formats:
     **Video H.264
     **Video H.265
     **Video Dolby Vision
     **Audio Dolby Digital Plus
     **Audio Dolby Digital
     **Audio Dolby AC4
     **Audio HE AAC v2
  * Support for edit lists if the mapping between CTS and presentation time
    is monotonically increasing.
  * Per sample access to
      ** Decoding Time Stamp (DTS)
      ** Composition Time Stamp (CTS)
      ** Presentation Time Stamp (CTS after applying edit lists)
      ** Sync sample information
  * Seeking inside fragments or unfragmented movies
  * Random access to fragments by evaluating 'mfro' and 'mfra' boxes.
  * Provide access to the following metadata
      ** File type information from 'ftyp' and 'styp' boxes.
      ** Dolby static metadata.
      ** ID3v2 metadata as ID3v2 tag from 'moov/meta/ID32' boxes.
      ** Unfragmented items referenced by 'moov/meta/iloc' and stored in
        'moov/meta/idat'.

## Frontends Sample Code

   mp4demuxer sample frontend to read local fragmented or unfragmented mp4 
   file format and output elementary streams.
 
## KNOWN LIMITATIONS/ISSUES

None

## DIAGRAM

   *Design UML
![](doc/design-UML.violet.png)

   *File Function
![](doc/file_info.png)
![](doc/file_source.png)

   *Demux or Playback workflow
![](doc/play.png)
![](doc/playback.png)

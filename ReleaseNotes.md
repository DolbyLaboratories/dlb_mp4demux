## v1.0.0 Library Features:

  * Single pass demultiplexing for fragmented or unfragmented files
  * API providing access to demultiplexer, trackreader, and box reader (navigator)
    objects, all multi-instantiable
  * Robust implementation to skip unknown boxes, ignore unrecognized content in
    known boxes, and return errors on corrupt data.
  * Read and demultiplex the following formats:
      fragmented or unfragmented MP4 file format(.mp4, .m4a, .m4v)
  * Extract media samples in the following formats:
    * Video H.264
    * Video H.265
    * Video Dolby Vision
    * Audio AAC-LC
    * Audio Dolby Digital
    * Audio Dolby Digital Plus
    * Audio Dolby AC4
  * Support edit lists if the mapping between CTS and presentation time
    is monotonically increasing.
  * Per sample access to
    * Decoding Time Stamp (DTS)
    * Composition Time Stamp (CTS)
    * Presentation Time Stamp (CTS after applying edit lists)
    * Sync sample information
  * Seek inside fragments or unfragmented files
  * Random access to fragments by evaluating 'mfro' and 'mfra' boxes.
  * Provide access to the following metadata
    * File type information from 'ftyp' and 'styp' boxes.
    * Dolby static metadata.
    * ID3v2 metadata as ID3v2 tag from 'moov/meta/ID32' boxes.
    * Unfragmented items referenced by 'moov/meta/iloc' and stored in 'moov/meta/idat'.

## Frontend Sample Code

   mp4demuxer sample frontend to read local fragmented or unfragmented mp4 
   file format and output elementary streams.
 
## Known limitations/issues

   None

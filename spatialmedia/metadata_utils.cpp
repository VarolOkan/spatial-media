/*****************************************************************************
 * 
 * Copyright 2016 Varol Okan. All rights reserved.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 ****************************************************************************/

#include <iostream>

#include "mpeg/constants.h"
#include "mpeg/mpeg4_container.h"
#include "mpeg/sa3d.h"

#include "metadata_utils.h"

const char *SPHERICAL_TAGS_LIST[] = {
    "Spherical",
    "Stitched",
    "StitchingSoftware",
    "ProjectionType",
    "SourceCount",
    "StereoMode",
    "InitialViewHeadingDegrees",
    "InitialViewPitchDegrees",
    "InitialViewRollDegrees",
    "Timestamp",
    "CroppedAreaImageWidthPixels",
    "CroppedAreaImageHeightPixels",
    "FullPanoWidthPixels",
    "FullPanoHeightPixels",
    "CroppedAreaLeftPixels",
    "CroppedAreaTopPixels",
};


Metadata::Metadata ( )
{
  m_pAudio = NULL;
}

Metadata::~Metadata ( )
{

}

void Metadata::setVideoXML ( std::string &str )
{
  m_strVideoXML = str;
}

void Metadata::setAudio ( void *pAudio )
{
  m_pAudio = pAudio;
}

ParsedMetadata::ParsedMetadata ( )
  : Metadata ( )
{
  m_iNumAudioChannels = 0;
}

ParsedMetadata::~ParsedMetadata ( )
{
}


Utils::Utils ( )
{

}

Utils::~Utils ( )
{

}

/*
SPHERICAL_PREFIX = "{http://ns.google.com/videos/1.0/spherical/}"
SPHERICAL_TAGS = dict()
for tag in SPHERICAL_TAGS_LIST:
    SPHERICAL_TAGS[SPHERICAL_PREFIX + tag] = tag

integer_regex_group = "(\d+)"
crop_regex = "^{0}$".format(":".join([integer_regex_group] * 6))
*/


Box *Utils::spherical_uuid ( std::string &strMetadata )
{
  // Constructs a uuid containing spherical metadata.
  Box *p = new Box;
  // a box containing spherical metadata.
//  if ( strUUID.length ( ) != 16 )
//    std::cerr << "ERROR: Data mismatch" << std::endl;
  int iSize = strMetadata.size ( );
  const uint8_t *pMetadata = reinterpret_cast<const uint8_t*>(strMetadata.c_str());

  memcpy ( p->m_name, constants::TAG_UUID, 4 );
  p->m_iHeaderSize  = 8;
  p->m_iContentSize = 0;
  p->m_pContents    = new uint8_t[iSize + 16 + 1];
  memcpy ( p->m_pContents, SPHERICAL_UUID_ID, 16 );
  memcpy ((p->m_pContents+16),  pMetadata, iSize );
  p->m_iContentSize=iSize+16;

  return p;
}

bool Utils::mpeg4_add_spherical ( Mpeg4Container *pMPEG4, std::fstream &inFile, std::string &strMetadata )
{
  // Adds a spherical uuid box to an mpeg4 file for all video tracks.
  //
  // pMPEG4 : Mpeg4 file structure to add metadata.
  // inFile : file handle, Source for uncached file contents.
  // strMetadata: string, xml metadata to inject into spherical tag.
  if ( ! pMPEG4 )
    return false;

  bool bAdded = false;
  Container *pMoov = (Container *)pMPEG4->m_pMoovBox;
  if ( ! pMoov )
    return false;

  std::vector<Box *>::iterator it = pMoov->m_listContents.begin ( );
  while ( it != pMoov->m_listContents.end ( ) )  {
    Container *pBox = (Container *)*it++;
    if ( memcmp ( pBox->m_name, constants::TAG_TRAK, 4 ) == 0 )  {
      bAdded = false;
      pBox->remove ( constants::TAG_UUID );

      std::vector<Box *>::iterator it2 = pBox->m_listContents.begin ( );
      while ( it2 != pBox->m_listContents.end ( ) )  {
        Container *pSub = (Container *)*it2++;
        if ( memcmp ( pSub->m_name, constants::TAG_MDIA, 4 ) != 0 )
          continue;

        std::vector<Box *>::iterator it3 = pSub->m_listContents.begin ( );
        while ( it3 != pSub->m_listContents.end ( ) )  {
          Box *pMDIA = *it3++;
          if  ( memcmp ( pMDIA->m_name, constants::TAG_HDLR, 4 ) != 0 )
            continue;

          char name[4];
          int iPos = pMDIA->content_start ( ) + 8;
          inFile.seekg( iPos );
          inFile.read ( name, 4 );
          if ( memcmp ( name, constants::TRAK_TYPE_VIDE, 4 ) == 0 )  {
            bAdded = true;
            break;
          }
        }        
        if ( bAdded )  {
          if ( ! pBox->add ( spherical_uuid ( strMetadata ) ) )
            return true;
          break;
        }
      }
    }
  }
  pMPEG4->resize ( );
  return true;
}

bool Utils::mpeg4_add_spatial_audio ( Mpeg4Container *pMPEG4, std::fstream &inFile, AudioMetadata *pAudio )
{
  // pMPEG4 is Mpeg4 file structure to add metadata.
  // inFile: file handle, Source for uncached file contents.
  // pAudio: dictionary ('ambisonic_type': string, 'ambisonic_order': int),
  //                      Supports 'periphonic' ambisonic type only.
  if ( ! pMPEG4 )
    return false;

  Container *pMoov = (Container *)pMPEG4->m_pMoovBox;
  if ( ! pMoov )
    return false;

  std::vector<Box *>::iterator it = pMoov->m_listContents.begin ( );
  while ( it != pMoov->m_listContents.end ( ) )  {
    Container *pBox = (Container *)*it++;
    if ( memcmp ( pBox->m_name, constants::TAG_TRAK, 4 ) != 0 )
      continue;
    
    std::vector<Box *>::iterator it2 = pBox->m_listContents.begin ( );
    while ( it2 != pBox->m_listContents.end ( ) )  {
      Container *pSub = (Container *)*it2++;
      if ( memcmp ( pSub->m_name, constants::TAG_MDIA, 4 ) != 0 )
        continue;

      std::vector<Box *>::iterator it3 = pSub->m_listContents.begin ( );
      while ( it3 != pSub->m_listContents.end ( ) )  {
        Box *pMDIA = *it3++;
        if  ( memcmp ( pMDIA->m_name, constants::TAG_HDLR, 4 ) != 0 )
          continue;

        char name[4];
        int iPos = pMDIA->content_start ( ) + 8;
        inFile.seekg( iPos );
        inFile.read ( name, 4 );
        if ( memcmp ( name, constants::TAG_SOUN, 4 ) == 0 )
          return inject_spatial_audio_atom ( inFile, pSub, pAudio ); 
      }
    }
  }
  return true;
}

bool Utils::mpeg4_add_audio_metadata ( Mpeg4Container *pMPEG4, std::fstream &inFile, AudioMetadata *pAudio )
{
  int num_audio_tracks = get_num_audio_tracks ( pMPEG4, inFile );
  if ( num_audio_tracks > 1 )  {
    std::cerr << "Error: Expected 1 audio track. Found " << num_audio_tracks << std::endl;
    return false;
  }
  return mpeg4_add_spatial_audio ( pMPEG4, inFile, pAudio );
}

bool inArray ( char *pName, const char **ppArray, int iSize )  {
  

  return false;
}

bool Utils::inject_spatial_audio_atom( std::fstream &inFile, Box *pAudioMediaAtom, AudioMetadata *pAudio )
{
  if ( ! pAudioMediaAtom || ! pAudio )
    return false;
  
  int iArraySize = sizeof ( constants::SOUND_SAMPLE_DESCRIPTIONS );
  Container *pMediaAtom = (Container *)pAudioMediaAtom;
  
  std::vector<Box *>::iterator it = pMediaAtom->m_listContents.begin ( );
  while ( it != pMediaAtom->m_listContents.end ( ) )  {
    Container *pAtom = (Container *)*it++;
    if ( memcmp ( pAtom->m_name, constants::TAG_MINF, 4 ) != 0 )
      continue;
    
    std::vector<Box *>::iterator it2 = pAtom->m_listContents.begin ( );
    while ( it2 != pAtom->m_listContents.end ( ) )  {
      Container *pElement = (Container *)*it2++;
      if ( memcmp ( pElement->m_name, constants::TAG_STBL, 4 ) != 0 )
        continue;
      
      std::vector<Box *>::iterator it3 = pElement->m_listContents.begin ( );
      while ( it3 != pElement->m_listContents.end ( ) )  {
        Container *pSub = (Container *)*it3++;
        if  ( memcmp ( pSub->m_name, constants::TAG_STSD, 4 ) != 0 )
          continue;

        std::vector<Box *>::iterator it4 = pSub->m_listContents.begin ( );
        while ( it4 != pSub->m_listContents.end ( ) )  {
          Container *pSample = (Container *)*it4++;
          if  (  inArray ( pSample->m_name, constants::SOUND_SAMPLE_DESCRIPTIONS, iArraySize ) )  {
            inFile.seekg ( pSample->m_iPosition + pSample->m_iHeaderSize + 16 );
            std::string strAmbisonicType= pAudio->ambisonic_type;
            uint32_t iAmbisonicOrder    = pAudio->ambisonic_order;
            int iNumChannels = get_num_audio_channels ( pSub, inFile );
            int iNumAmbisonicComponents = get_expected_num_audio_components ( strAmbisonicType, iAmbisonicOrder );
            if ( iNumChannels != iNumAmbisonicComponents )  {
              std::cerr << "Error: Found " << iNumChannels << " audio channel(s). ";
              std::cerr << "Expected " << iNumAmbisonicComponents << " chanel(s) for ";
              std::cerr << strAmbisonicType << " ambisonics of order " << iAmbisonicOrder << "." << std::endl;
              return false;
            }
            Box *pSA3DAtom =  SA3DBox::create ( iNumChannels, *pAudio );
            pSample->m_listContents.push_back ( pSA3DAtom );
          }
        }
      }
    }
  }
  return true;
}

void Utils::parse_spherical_xml ( Box * ) // return sphericalDictionary
{
/*
def parse_spherical_xml(contents, console):
    """Returns spherical metadata for a set of xml data.

    Args:
      contents: string, spherical metadata xml contents.

    Returns:
      dictionary containing the parsed spherical metadata values.
    """
    try:
        parsed_xml = xml.etree.ElementTree.XML(contents)
    except xml.etree.ElementTree.ParseError:
        try:
            index = contents.find("<rdf:SphericalVideo")
            if index != -1:
                index += len("<rdf:SphericalVideo")
                contents = contents[:index] + RDF_PREFIX + contents[index:]
            parsed_xml = xml.etree.ElementTree.XML(contents)
            console("\t\tWarning missing rdf prefix:", RDF_PREFIX)
        except xml.etree.ElementTree.ParseError as e:
            console("\t\tParser Error on XML")
            console(e)
            console(contents)
            return

    sphericalDictionary = dict()
    for child in parsed_xml.getchildren():
        if child.tag in SPHERICAL_TAGS.keys():
            console("\t\t" + SPHERICAL_TAGS[child.tag]
                    + " = " + child.text)
            sphericalDictionary[SPHERICAL_TAGS[child.tag]] = child.text
        else:
            tag = child.tag
            if child.tag[:len(spherical_prefix)] == spherical_prefix:
                tag = child.tag[len(spherical_prefix):]
            console("\t\tUnknown: " + tag + " = " + child.text)

    return sphericalDictionary
*/
}

void Utils::parse_spherical_mpeg4 ( Mpeg4Container *,std::fstream & ) // return metadata
{
/*
def parse_spherical_mpeg4(mpeg4_file, fh, console):
    """Returns spherical metadata for a loaded mpeg4 file.

    Args:
      mpeg4_file: mpeg4, loaded mpeg4 file contents.
      fh: file handle, file handle for uncached file contents.

    Returns:
      Dictionary stored as (trackName, metadataDictionary)
    """
    metadata = ParsedMetadata()
    track_num = 0
    for element in mpeg4_file.moov_box.contents:
        if element.name == mpeg.constants.TAG_TRAK:
            trackName = "Track %d" % track_num
            console("\t%s" % trackName)
            track_num += 1
            for sub_element in element.contents:
                if sub_element.name == mpeg.constants.TAG_UUID:
                    if sub_element.contents:
                        sub_element_id = sub_element.contents[:16]
                    else:
                        fh.seek(sub_element.content_start())
                        sub_element_id = fh.read(16)

                    if sub_element_id == SPHERICAL_UUID_ID:
                        if sub_element.contents:
                            contents = sub_element.contents[16:]
                        else:
                            contents = fh.read(sub_element.content_size - 16)
                        metadata.video[trackName] = \
                            parse_spherical_xml(contents, console)

            if sub_element.name == mpeg.constants.TAG_MDIA:
                for mdia_sub_element in sub_element.contents:
                    if mdia_sub_element.name != mpeg.constants.TAG_MINF:
                        continue
                    for stbl_elem in mdia_sub_element.contents:
                        if stbl_elem.name != mpeg.constants.TAG_STBL:
                            continue
                        for stsd_elem in stbl_elem.contents:
                            if stsd_elem.name != mpeg.constants.TAG_STSD:
                                continue
                            for sa3d_container_elem in stsd_elem.contents:
                                if sa3d_container_elem.name not in \
                                        mpeg.constants.SOUND_SAMPLE_DESCRIPTIONS:
                                    continue
                                metadata.num_audio_channels = \
                                    get_num_audio_channels(stsd_elem, fh)
                                for sa3d_elem in sa3d_container_elem.contents:
                                    if sa3d_elem.name == mpeg.constants.TAG_SA3D:
                                        sa3d_elem.print_box(console)
                                        metadata.audio = sa3d_elem
    return metadata
*/
}

void Utils::parse_mpeg4 ( std::string & )
{
/*
def parse_mpeg4(input_file, console):
    with open(input_file, "rb") as in_fh:
        mpeg4_file = mpeg.load(in_fh)
        if mpeg4_file is None:
            console("Error, file could not be opened.")
            return

        console("Loaded file...")
        return parse_spherical_mpeg4(mpeg4_file, in_fh, console)

    console("Error \"" + input_file + "\" does not exist or do not have "
            "permission.")
*/
}

void Utils::inject_mpeg4 ( std::string &, std::string &, Box * )
{
/*
def inject_mpeg4(input_file, output_file, metadata, console):
    with open(input_file, "rb") as in_fh:

        mpeg4_file = mpeg.load(in_fh)
        if mpeg4_file is None:
            console("Error file could not be opened.")

        if not mpeg4_add_spherical(mpeg4_file, in_fh, metadata.video):
            console("Error failed to insert spherical data")

        if metadata.audio:
            if not mpeg4_add_audio_metadata(
                mpeg4_file, in_fh, metadata.audio, console):
                    console("Error failed to insert spatial audio data")

        console("Saved file settings")
        parse_spherical_mpeg4(mpeg4_file, in_fh, console)

        with open(output_file, "wb") as out_fh:
            mpeg4_file.save(in_fh, out_fh)
        return

    console("Error file: \"" + input_file + "\" does not exist or do not have "
            "permission.")
*/
}

void Utils::parse_metadata ( std::string & )
{
/*
def parse_metadata(src, console):
    infile = os.path.abspath(src)

    try:
        in_fh = open(infile, "rb")
        in_fh.close()
    except:
        console("Error: " + infile +
                " does not exist or we do not have permission")

    console("Processing: " + infile)
    extension = os.path.splitext(infile)[1].lower()

    if extension in MPEG_FILE_EXTENSIONS:
        return parse_mpeg4(infile, console)

    console("Unknown file type")
    return None
*/
}

void Utils::inject_metadata ( std::string &, std::string &, Metadata * )
{
/*
def inject_metadata(src, dest, metadata, console):
    infile = os.path.abspath(src)
    outfile = os.path.abspath(dest)

    if infile == outfile:
        return "Input and output cannot be the same"

    try:
        in_fh = open(infile, "rb")
        in_fh.close()
    except:
        console("Error: " + infile +
                " does not exist or we do not have permission")
        return

    console("Processing: " + infile)

    extension = os.path.splitext(infile)[1].lower()

    if (extension in MPEG_FILE_EXTENSIONS):
        inject_mpeg4(infile, outfile, metadata, console)
        return

    console("Unknown file type")
*/
}

std::string &Utils::generate_spherical_xml ( SpatialMedia::Parser::enMode, int * )
{
/*
def generate_spherical_xml(stereo=None, crop=None):
    # Configure inject xml.
    additional_xml = ""
    if stereo == "top-bottom":
        additional_xml += SPHERICAL_XML_CONTENTS_TOP_BOTTOM

    if stereo == "left-right":
        additional_xml += SPHERICAL_XML_CONTENTS_LEFT_RIGHT

    if crop:
        crop_match = re.match(crop_regex, crop)
        if not crop_match:
            print "Error: Invalid crop params: {crop}".format(crop=crop)
            return False
        else:
            cropped_width_pixels = int(crop_match.group(1))
            cropped_height_pixels = int(crop_match.group(2))
            full_width_pixels = int(crop_match.group(3))
            full_height_pixels = int(crop_match.group(4))
            cropped_offset_left_pixels = int(crop_match.group(5))
            cropped_offset_top_pixels = int(crop_match.group(6))

            # This should never happen based on the crop regex.
            if full_width_pixels <= 0 or full_height_pixels <= 0:
                print "Error with crop params: full pano dimensions are "\
                        "invalid: width = {width} height = {height}".format(
                            width=full_width_pixels,
                            height=full_height_pixels)
                return False

            if (cropped_width_pixels <= 0 or
                    cropped_height_pixels <= 0 or
                    cropped_width_pixels > full_width_pixels or
                    cropped_height_pixels > full_height_pixels):
                print "Error with crop params: cropped area dimensions are "\
                        "invalid: width = {width} height = {height}".format(
                            width=cropped_width_pixels,
                            height=cropped_height_pixels)
                return False

            # We are pretty restrictive and don't allow anything strange. There
            # could be use-cases for a horizontal offset that essentially
            # translates the domain, but we don't support this (so that no
            # extra work has to be done on the client).
            total_width = cropped_offset_left_pixels + cropped_width_pixels
            total_height = cropped_offset_top_pixels + cropped_height_pixels
            if (cropped_offset_left_pixels < 0 or
                    cropped_offset_top_pixels < 0 or
                    total_width > full_width_pixels or
                    total_height > full_height_pixels):
                    print "Error with crop params: cropped area offsets are "\
                            "invalid: left = {left} top = {top} "\
                            "left+cropped width: {total_width} "\
                            "top+cropped height: {total_height}".format(
                                left=cropped_offset_left_pixels,
                                top=cropped_offset_top_pixels,
                                total_width=total_width,
                                total_height=total_height)
                    return False

            additional_xml += SPHERICAL_XML_CONTENTS_CROP_FORMAT.format(
                cropped_width_pixels, cropped_height_pixels,
                full_width_pixels, full_height_pixels,
                cropped_offset_left_pixels, cropped_offset_top_pixels)

    spherical_xml = (SPHERICAL_XML_HEADER +
                     SPHERICAL_XML_CONTENTS +
                     additional_xml +
                     SPHERICAL_XML_FOOTER)
    return spherical_xml
*/
  return m_strSphericalXML;
}

void Utils::get_descriptor_length ( std::fstream & )
{
/*
def get_descriptor_length(in_fh):
    """Derives the length of the MP4 elementary stream descriptor at the
       current position in the input file.
    """
    descriptor_length = 0
    for i in range(4):
        size_byte = struct.unpack(">c", in_fh.read(1))[0]
        descriptor_length = (descriptor_length << 7 |
                             ord(size_byte) & int("0x7f", 0))
        if (ord(size_byte) != int("0x80", 0)):
            break
    return descriptor_length
*/
}

int32_t Utils::get_expected_num_audio_components ( std::string &, uint32_t )
{
/*
def get_expected_num_audio_components(ambisonics_type, ambisonics_order):
    """ Returns the expected number of ambisonic components for a given
        ambisonic type and ambisonic order.
    """
    if (ambisonics_type == 'periphonic'):
        return ((ambisonics_order + 1) * (ambisonics_order + 1))
    else:
        return -1

def get_num_audio_channels(stsd, in_fh):
    if stsd.name != mpeg.constants.TAG_STSD:
        print "get_num_audio_channels should be given a STSD box"
        return -1
    for sample_description in stsd.contents:
        if sample_description.name == mpeg.constants.TAG_MP4A:
            return get_aac_num_channels(sample_description, in_fh)
        elif sample_description.name in mpeg.constants.SOUND_SAMPLE_DESCRIPTIONS:
            return get_sample_description_num_channels(sample_description, in_fh)
    return -1
*/
  return -1;
}

int32_t Utils::get_num_audio_channels ( Box *, std::fstream & )
{
/*
def get_num_audio_channels(stsd, in_fh):
    if stsd.name != mpeg.constants.TAG_STSD:
        print "get_num_audio_channels should be given a STSD box"
        return -1
    for sample_description in stsd.contents:
        if sample_description.name == mpeg.constants.TAG_MP4A:
            return get_aac_num_channels(sample_description, in_fh)
        elif sample_description.name in mpeg.constants.SOUND_SAMPLE_DESCRIPTIONS:
            return get_sample_description_num_channels(sample_description, in_fh)
    return -1
*/
  return -1;
}

uint32_t Utils::get_sample_description_num_channels ( Box *, std::fstream & )
{
/*
def get_sample_description_num_channels(sample_description, in_fh):
    """Reads the number of audio channels from a sound sample description.
    """
    p = in_fh.tell()
    in_fh.seek(sample_description.content_start() + 8)

    version = struct.unpack(">h", in_fh.read(2))[0]
    revision_level = struct.unpack(">h", in_fh.read(2))[0]
    vendor = struct.unpack(">i", in_fh.read(4))[0]
    if version == 0:
        num_audio_channels = struct.unpack(">h", in_fh.read(2))[0]
        sample_size_bytes = struct.unpack(">h", in_fh.read(2))[0]
    elif version == 1:
        num_audio_channels = struct.unpack(">h", in_fh.read(2))[0]
        sample_size_bytes = struct.unpack(">h", in_fh.read(2))[0]
        samples_per_packet = struct.unpack(">i", in_fh.read(4))[0]
        bytes_per_packet = struct.unpack(">i", in_fh.read(4))[0]
        bytes_per_frame = struct.unpack(">i", in_fh.read(4))[0]
        bytes_per_sample = struct.unpack(">i", in_fh.read(4))[0]
    elif version == 2:
        always_3 = struct.unpack(">h", in_fh.read(2))[0]
        always_16 = struct.unpack(">h", in_fh.read(2))[0]
        always_minus_2 = struct.unpack(">h", in_fh.read(2))[0]
        always_0 = struct.unpack(">h", in_fh.read(2))[0]
        always_65536 = struct.unpack(">i", in_fh.read(4))[0]
        size_of_struct_only = struct.unpack(">i", in_fh.read(4))[0]
        audio_sample_rate = struct.unpack(">d", in_fh.read(8))[0]
        num_audio_channels = struct.unpack(">i", in_fh.read(4))[0]
    else:
        print "Unsupported version for " + sample_description.name + " box"
        return -1

    in_fh.seek(p)
    return num_audio_channels
*/
  return 0;
}

int32_t Utils::get_aac_num_channels ( Box *, std::fstream & )
{
/*
def get_aac_num_channels(box, in_fh):
    """Reads the number of audio channels from AAC's AudioSpecificConfig
       descriptor within the esds child box of the input mp4a or wave box.
    """
    p = in_fh.tell()
    if box.name not in [mpeg.constants.TAG_MP4A, mpeg.constants.TAG_WAVE]:
        return -1

    for element in box.contents:
        if element.name == mpeg.constants.TAG_WAVE:
            # Handle .mov with AAC audio, where the structure is:
            #     stsd -> mp4a -> wave -> esds
            channel_configuration = get_aac_num_channels(element, in_fh)
            break

        if element.name != mpeg.constants.TAG_ESDS:
          continue
        in_fh.seek(element.content_start() + 4)
        descriptor_tag = struct.unpack(">c", in_fh.read(1))[0]

        # Verify the read descriptor is an elementary stream descriptor
        if ord(descriptor_tag) != 3:  # Not an MP4 elementary stream.
            print "Error: failed to read elementary stream descriptor."
            return -1
        get_descriptor_length(in_fh)
        in_fh.seek(3, 1)  # Seek to the decoder configuration descriptor
        config_descriptor_tag = struct.unpack(">c", in_fh.read(1))[0]

        # Verify the read descriptor is a decoder config. descriptor.
        if ord(config_descriptor_tag) != 4:
            print "Error: failed to read decoder config. descriptor."
            return -1
        get_descriptor_length(in_fh)
        in_fh.seek(13, 1) # offset to the decoder specific config descriptor.
        decoder_specific_descriptor_tag = struct.unpack(">c", in_fh.read(1))[0]

        # Verify the read descriptor is a decoder specific info descriptor
        if ord(decoder_specific_descriptor_tag) != 5:
            print "Error: failed to read MP4 audio decoder specific config."
            return -1
        audio_specific_descriptor_size = get_descriptor_length(in_fh)
        assert audio_specific_descriptor_size >= 2
        decoder_descriptor = struct.unpack(">h", in_fh.read(2))[0]
        object_type = (int("F800", 16) & decoder_descriptor) >> 11
        sampling_frequency_index = (int("0780", 16) & decoder_descriptor) >> 7
        if sampling_frequency_index == 0:
            # TODO: If the sample rate is 96kHz an additional 24 bit offset
            # value here specifies the actual sample rate.
            print "Error: Greater than 48khz audio is currently not supported."
            return -1
        channel_configuration = (int("0078", 16) & decoder_descriptor) >> 3
    in_fh.seek(p)
    return channel_configuration
*/
  return -1;
}

uint32_t Utils::get_num_audio_tracks ( Mpeg4Container *, std::fstream & )
{
/*
def get_num_audio_tracks(mpeg4_file, in_fh):
    """ Returns the number of audio track in the input mpeg4 file. """
    num_audio_tracks = 0
    for element in mpeg4_file.moov_box.contents:
        if (element.name == mpeg.constants.TAG_TRAK):
            for sub_element in element.contents:
                if (sub_element.name != mpeg.constants.TAG_MDIA):
                    continue
                for mdia_sub_element in sub_element.contents:
                    if (mdia_sub_element.name != mpeg.constants.TAG_HDLR):
                        continue
                    position = mdia_sub_element.content_start() + 8
                    in_fh.seek(position)
                    if (in_fh.read(4) == mpeg.constants.TAG_SOUN):
                        num_audio_tracks += 1
    return num_audio_tracks
*/
  return 0;
}

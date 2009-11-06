/*--------------------------------------------------------------------------
Copyright (c) 2009, Code Aurora Forum. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of Code Aurora nor
      the names of its contributors may be used to endorse or promote
      products derived from this software without specific prior written
      permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
--------------------------------------------------------------------------*/
#include "MP4_Utils.h"
#include "omx_vdec.h"
# include <stdio.h>
#include "cutils/properties.h"


/* -----------------------------------------------------------------------
** Forward Declarations
** ----------------------------------------------------------------------- */

/* =======================================================================
**                            Function Definitions
** ======================================================================= */

/*<EJECT>*/
/*===========================================================================
FUNCTION:
  MP4_Utils constructor

DESCRIPTION:
  Constructs an instance of the Mpeg4 Utilitys.

RETURN VALUE:
  None.
===========================================================================*/
MP4_Utils::MP4_Utils()
{
   char property_value[PROPERTY_VALUE_MAX] = {0};

   m_SrcWidth = 0;
   m_SrcHeight = 0;
   m_default_profile_chk = true;
   m_default_level_chk = true;

   if(0 != property_get("persist.omxvideo.profilecheck", property_value, NULL))
   {
       if(!strcmp(property_value, "false"))
       {
           m_default_profile_chk = false;
       }
   }
   else
   {
       QTV_MSG_PRIO(QTVDIAG_GENERAL, QTVDIAG_PRIO_ERROR, "MP4_Utils:: Constr failed in \
           getting value for the Android property [persist.omxvideo.profilecheck]");
   }

   if(0 != property_get("persist.omxvideo.levelcheck", property_value, NULL))
   {
       if(!strcmp(property_value, "false"))
       {
           m_default_level_chk = false;
       }
   }
   else
   {
       QTV_MSG_PRIO(QTVDIAG_GENERAL, QTVDIAG_PRIO_ERROR, "MP4_Utils:: Constr failed in \
           getting value for the Android property [persist.omxvideo.levelcheck]");
   }


}

/* <EJECT> */
/*===========================================================================

FUNCTION:
  MP4_Utils destructor

DESCRIPTION:
  Destructs an instance of the Mpeg4 Utilities.

RETURN VALUE:
  None.
===========================================================================*/
MP4_Utils::~MP4_Utils()
{
}

/* <EJECT> */
/*===========================================================================
FUNCTION:
  read_bit_field

DESCRIPTION:
  This helper function reads a field of given size (in bits) out of a raw
  bitstream.

INPUT/OUTPUT PARAMETERS:
  posPtr:   Pointer to posInfo structure, containing current stream position
            information

  size:     Size (in bits) of the field to be read; assumed size <= 32

  NOTE: The bitPos is the next available bit position in the byte pointed to
        by the bytePtr. The bit with the least significant position in the byte
        is considered bit number 0.

RETURN VALUE:
  Value of the bit field required (stored in a 32-bit value, right adjusted).

SIDE EFFECTS:
  None.
---------------------------------------------------------------------------*/
uint32 MP4_Utils::read_bit_field(posInfoType * posPtr, uint32 size) {
   uint8 *bits = &posPtr->bytePtr[0];
   uint32 bitBuf =
       (bits[0] << 24) | (bits[1] << 16) | (bits[2] << 8) | bits[3];

   uint32 value = (bitBuf >> (32 - posPtr->bitPos - size)) & MASK(size);

   /* Update the offset in preparation for next field    */
   posPtr->bitPos += size;

   while (posPtr->bitPos >= 8) {
      posPtr->bitPos -= 8;
      posPtr->bytePtr++;
   }
   return value;

}

/* <EJECT> */
/*===========================================================================
FUNCTION:
  find_code

DESCRIPTION:
  This helper function searches a bitstream for a specific 4 byte code.

INPUT/OUTPUT PARAMETERS:
  bytePtr:          pointer to starting location in the bitstream
  size:             size (in bytes) of the bitstream
  codeMask:         mask for the code we are looking for
  referenceCode:    code we are looking for

RETURN VALUE:
  Pointer to a valid location if the code is found; 0 otherwise.

SIDE EFFECTS:
  None.
---------------------------------------------------------------------------*/
static uint8 *find_code
    (uint8 * bytePtr, uint32 size, uint32 codeMask, uint32 referenceCode) {
   uint32 code = 0xFFFFFFFF;
   for (uint32 i = 0; i < size; i++) {
      code <<= 8;
      code |= *bytePtr++;

      if ((code & codeMask) == referenceCode) {
         return bytePtr;
      }
   }

   printf("Unable to find code\n");

   return NULL;
}

/*
=============================================================================
FUNCTION:
  populateHeightNWidthFromShortHeader

DESCRIPTION:
  This function parses the short header and populates frame height and width
  into MP4_Utils.

INPUT/OUTPUT PARAMETERS:
  psBits - pointer to input stream of bits

RETURN VALUE:
  Error code

SIDE EFFECTS:
  None.

=============================================================================
*/
int16 MP4_Utils::populateHeightNWidthFromShortHeader(mp4StreamType * psBits) {
   bool extended_ptype = false;
   bool opptype_present = false;
   bool fCustomSourceFormat = false;
   uint32 marker_bit;
   uint32 source_format;
   m_posInfo.bitPos = 0;
   m_posInfo.bytePtr = psBits->data;
   m_dataBeginPtr = psBits->data;
   //22 -> short_video_start_marker
   if (SHORT_VIDEO_START_MARKER != read_bit_field(&m_posInfo, 22))
      return MP4_INVALID_VOL_PARAM;
   //8 -> temporal_reference
   //1 -> marker bit
   //1 -> split_screen_indicator
   //1 -> document_camera_indicator
   //1 -> full_picture_freeze_release
   read_bit_field(&m_posInfo, 13);
   source_format = read_bit_field(&m_posInfo, 3);
   switch (source_format) {
   case 1:
      // sub-QCIF
      m_SrcWidth = 128;
      m_SrcHeight = 96;
      break;

   case 2:
      // QCIF
      m_SrcWidth = 176;
      m_SrcHeight = 144;
      break;

   case 3:
      // CIF
      m_SrcWidth = 352;
      m_SrcHeight = 288;
      break;

   case 4:
      // 4CIF
      m_SrcWidth = 704;
      m_SrcHeight = 576;
      break;

   case 5:
      // 16CIF
      m_SrcWidth = 1408;
      m_SrcHeight = 1152;
      break;

   case 7:
      extended_ptype = true;
      break;

   default:
      return MP4_INVALID_VOL_PARAM;
   }

   if (extended_ptype) {
      /* Plus PTYPE (PLUSPTYPE)
       ** This codeword of 12 or 30 bits is comprised of up to three subfields:
       ** UFEP, OPPTYPE, and MPPTYPE.  OPPTYPE is present only if UFEP has a
       ** particular value.
       */

      /* Update Full Extended PTYPE (UFEP) */
      uint32 ufep = read_bit_field(&m_posInfo, 3);
      switch (ufep) {
      case 0:
         /* Only MMPTYPE fields are included in current picture header, the
          ** optional part of PLUSPTYPE (OPPTYPE) is not present
          */
         opptype_present = false;
         break;

      case 1:
         /* all extended PTYPE fields (OPPTYPE and MPPTYPE) are included in
          ** current picture header
          */
         opptype_present = true;
         break;

      default:
         return MP4ERROR_UNSUPPORTED_UFEP;
      }

      if (opptype_present) {
         /* The Optional Part of PLUSPTYPE (OPPTYPE) (18 bits) */
         /* source_format */
         source_format = read_bit_field(&m_posInfo, 3);
         switch (source_format) {
         case 1:
            /* sub-QCIF */
            m_SrcWidth = 128;
            m_SrcHeight = 96;
            break;

         case 2:
            /* QCIF */
            m_SrcWidth = 176;
            m_SrcHeight = 144;
            break;

         case 3:
            /* CIF */
            m_SrcWidth = 352;
            m_SrcHeight = 288;
            break;

         case 4:
            /* 4CIF */
            m_SrcWidth = 704;
            m_SrcHeight = 576;
            break;

         case 5:
            /* 16CIF */
            m_SrcWidth = 1408;
            m_SrcHeight = 1152;
            break;

         case 6:
            /* custom source format */
            fCustomSourceFormat = true;
            break;

         default:
            return MP4ERROR_UNSUPPORTED_SOURCE_FORMAT;
         }

         /* Custom PCF */
         read_bit_field(&m_posInfo, 1);

         /* Continue parsing to determine whether H.263 Profile 1,2, or 3 is present.
          ** Only Baseline profile P0 is supported
          ** Baseline profile doesn't have any ANNEX supported.
          ** This information is used initialize the DSP. First parse past the
          ** unsupported optional custom PCF and Annexes D, E, and F.
          */
         uint32 PCF_Annex_D_E_F = read_bit_field(&m_posInfo, 3);
         if (PCF_Annex_D_E_F != 0)
            return MP4ERROR_UNSUPPORTED_SOURCE_FORMAT;

         /* Parse past bit for Annex I, J, K, N, R, S, T */
         uint32 PCF_Annex_I_J_K_N_R_S_T =
             read_bit_field(&m_posInfo, 7);
         if (PCF_Annex_I_J_K_N_R_S_T != 0)
            return MP4ERROR_UNSUPPORTED_SOURCE_FORMAT;

         /* Parse past one marker bit, and three reserved bits */
         read_bit_field(&m_posInfo, 4);

         /* Parse past the 9-bit MPPTYPE */
         read_bit_field(&m_posInfo, 9);

         /* Read CPM bit */
         uint32 continuous_presence_multipoint =
             read_bit_field(&m_posInfo, 1);
         if (fCustomSourceFormat) {
            if (continuous_presence_multipoint) {
               /* PSBI always follows immediately after CPM if CPM = "1", so parse
                ** past the PSBI.
                */
               read_bit_field(&m_posInfo, 2);
            }
            /* Extract the width and height from the Custom Picture Format (CPFMT) */
            uint32 pixel_aspect_ration_code =
                read_bit_field(&m_posInfo, 4);
            if (pixel_aspect_ration_code == 0)
               return MP4_INVALID_VOL_PARAM;

            uint32 picture_width_indication =
                read_bit_field(&m_posInfo, 9);
            m_SrcWidth =
                ((picture_width_indication & 0x1FF) +
                 1) << 2;

            marker_bit = read_bit_field(&m_posInfo, 1);
            if (marker_bit == 0)
               return MP4_INVALID_VOL_PARAM;

            uint32 picture_height_indication =
                read_bit_field(&m_posInfo, 9);
            m_SrcHeight =
                (picture_height_indication & 0x1FF) << 2;
            QTV_MSG_PRIO1(QTVDIAG_GENERAL,
                     QTVDIAG_PRIO_FATAL,
                     "m_SrcHeight =  %d\n",
                     m_SrcHeight);
            QTV_MSG_PRIO1(QTVDIAG_GENERAL,
                     QTVDIAG_PRIO_FATAL,
                     "m_SrcWidth =  %d\n", m_SrcWidth);
         }
      } else {
         /* UFEP must be "001" for INTRA picture types */
         return MP4_INVALID_VOL_PARAM;
      }
   }
   if (m_SrcWidth * m_SrcHeight >
       MP4_MAX_DECODE_WIDTH * MP4_MAX_DECODE_HEIGHT) {
      /* Frame dimesions greater than maximum size supported */
      QTV_MSG_PRIO2(QTVDIAG_GENERAL, QTVDIAG_PRIO_ERROR,
               "Frame Dimensions not supported %d %d",
               m_SrcWidth, m_SrcHeight);
      return MP4ERROR_UNSUPPORTED_SOURCE_FORMAT;
   }
   return MP4ERROR_SUCCESS;
}

/* <EJECT> */
/*===========================================================================

FUNCTION:
  populateHeightNWidthFromVolHeader

DESCRIPTION:
  This function parses the VOL header and populates frame height and width
  into MP4_Utils.

INPUT/OUTPUT PARAMETERS:
  psBits - pointer to input stream of bits

RETURN VALUE:
  Error code

SIDE EFFECTS:
  None.

===========================================================================*/

bool MP4_Utils::parseHeader(mp4StreamType * psBits) {
   uint32 profile_and_level_indication = 0;
   uint32 ver_id = 1,sprite_enable = 0;
   long hxw = 0;
//  ASSERT( psBits != NULL );
//  ASSERT( psBits->data != NULL );
   m_posInfo.bitPos = 0;
   m_posInfo.bytePtr = psBits->data;
   m_dataBeginPtr = psBits->data;
   m_posInfo.bytePtr = find_code(m_posInfo.bytePtr,
                  psBits->numBytes,
                  VIDEO_OBJECT_LAYER_START_CODE_MASK,
                  VIDEO_OBJECT_LAYER_START_CODE);

   if (m_posInfo.bytePtr == NULL) {
      printf
          ("Unable to find VIDEO_OBJECT_LAYER_START_CODE,returning MP4_INVALID_VOL_PARAM \n");
      m_posInfo.bitPos = 0;
      m_posInfo.bytePtr = psBits->data;
      m_posInfo.bytePtr = find_code(m_posInfo.bytePtr,
                     psBits->numBytes,
                     SHORT_HEADER_MASK,
                     SHORT_HEADER_START_CODE);
      if (m_posInfo.bytePtr) {
         if (MP4ERROR_SUCCESS ==
             populateHeightNWidthFromShortHeader(psBits))
            return true;
         else {
            printf("Error in parsing the short header \n");
            return false;
         }
      } else {
         printf
             ("Unable to find VIDEO_OBJECT_LAYER_START_CODE or SHORT_HEADER_START_CODE returning MP4_INVALID_VOL_PARAM \n");
         m_posInfo.bitPos = 0;
         m_posInfo.bytePtr = psBits->data;
         return false;
      }
   }
   // 1 -> random accessible VOL
   read_bit_field(&m_posInfo, 1);

   // 8 -> video_object_type indication
   profile_and_level_indication = read_bit_field(&m_posInfo, 8);

   // 1 -> is_object_layer_identifier
   if (read_bit_field(&m_posInfo, 1)) {
      // 4 -> video_object_layer_verid
      // 3 -> video_object_layer_priority
      ver_id = read_bit_field (&m_posInfo, 4);
      read_bit_field(&m_posInfo, 3);
   }
   // 4 -> aspect_ratio_info
   if (EXTENDED_PAR == read_bit_field(&m_posInfo, 4)) {
      //8 -> par_width
      //8 -> par_height
      read_bit_field(&m_posInfo, 16);
   }
   //1 -> vol_control_parameters
   if (read_bit_field(&m_posInfo, 1)) {
      //2 -> chroma_format
      //1 -> low_delay
      read_bit_field(&m_posInfo, 3);
      //1-> vbv_parameters
      if (read_bit_field(&m_posInfo, 1)) {
         //15 -> first_half_bit_rate
         //1 -> marker_bit
         //15 -> latter_half_bit_rate
         //1 -> marker_bit
         //15 -> first_half_vbv_buffer_size
         //1 -> marker_bit
         //3 -> latter_half_vbv_buffer_size
         //11 -> first_half_vbv_occupancy
         //1 -> marker_bit
         //15 -> latter_half_vbv_occupancy
         //1 -> marker_bit
         read_bit_field(&m_posInfo, 79);
      }
   }
   if (MPEG4_SHAPE_RECTANGULAR !=
       (unsigned char)read_bit_field(&m_posInfo, 2)) {
      printf("returning NON_RECTANGULAR_SHAPE \n");
      return false;
   }
   //1 -> marker bit
   read_bit_field(&m_posInfo, 1);
   //16 -> vop_time_increment_resolution
   unsigned short time_increment_res =
       (unsigned short)read_bit_field(&m_posInfo, 16);
   int i, j;
   int nBitsTime;
   // claculating VOP resolution
   i = time_increment_res - 1;
   j = 0;
   while (i) {
      j++;
      i >>= 1;
   }
   if (j)
      nBitsTime = j;
   else
      nBitsTime = 1;

   //1 -> marker_bit
   read_bit_field(&m_posInfo, 1);
   //1 -> fixed_vop_rate
   if (read_bit_field(&m_posInfo, 1)) {
      //nBitsTime -> fixed_vop_increment
      read_bit_field(&m_posInfo, nBitsTime);
   }
   if (1 != read_bit_field(&m_posInfo, 1))
      return false;
   m_SrcWidth = read_bit_field(&m_posInfo, 13);
   if (1 != read_bit_field(&m_posInfo, 1))
      return false;
   m_SrcHeight = read_bit_field(&m_posInfo, 13);
   /* marker_bit*/
   read_bit_field (&m_posInfo, 1);
   /* interlaced*/
   read_bit_field (&m_posInfo, 1);
   /* obmc_disable*/
   read_bit_field (&m_posInfo, 1);
   /* Nr. of bits for sprite_enabled is 1 for version 1, and 2 for
   ** version 2, according to p. 114, Table v2-2. */
   /* sprite_enable*/
   if(ver_id == 1) {
      sprite_enable = read_bit_field (&m_posInfo, 1);
   }
   else {
      sprite_enable = read_bit_field (&m_posInfo, 2);
   }
   if (sprite_enable) {
       QTV_MSG_PRIO(QTVDIAG_GENERAL, QTVDIAG_PRIO_ERROR,"No Support for Sprite Enabled clips\n");
       return false;
   }
   /* not_8_bit*/
   if ( read_bit_field (&m_posInfo, 1) )
   {
     /* quant_precision*/
     read_bit_field (&m_posInfo, 4);
     /* bits_per_pixel*/
     read_bit_field (&m_posInfo, 4);
   }
   /* quant_type*/
   if (read_bit_field (&m_posInfo, 1)) {
     /*load_intra_quant_mat*/
     if (read_bit_field (&m_posInfo, 1)) {
       unsigned char cnt = 2, data;
       /*intra_quant_mat */
       read_bit_field (&m_posInfo, 8);
       data = read_bit_field (&m_posInfo, 8);
       while (data && cnt < 64) {
         data = read_bit_field (&m_posInfo, 8);
         cnt++;
       }
     }
     /*load_non_intra_quant_mat*/
     if (read_bit_field (&m_posInfo, 1)) {
       unsigned char cnt = 2, data;
       /*non_intra_quant_mat */
       read_bit_field (&m_posInfo, 8);
       data = read_bit_field (&m_posInfo, 8);
       while (data && cnt < 64) {
         data = read_bit_field (&m_posInfo, 8);
         cnt++;
       }
     }
   }
   if ( ver_id != 1 )
   {
     /* quarter_sample*/
     read_bit_field (&m_posInfo, 1);
   }
   /* complexity_estimation_disable*/
   read_bit_field (&m_posInfo, 1);
   /* resync_marker_disable*/
   read_bit_field (&m_posInfo, 1);
   /* data_partitioned*/
   if ( read_bit_field (&m_posInfo, 1) ) {
     hxw = m_SrcWidth* m_SrcHeight;
     if(hxw > (OMX_CORE_WVGA_WIDTH*OMX_CORE_WVGA_HEIGHT)) {
       QTV_MSG_PRIO(QTVDIAG_GENERAL, QTVDIAG_PRIO_ERROR,"Data partition clips not supported for Greater than WVGA resolution \n");
       return false;
     }
   }

   // not doing the remaining parsing
   return validate_profile_and_level(profile_and_level_indication);
}

/*
=============================================================================
FUNCTION:
  HasFrame

DESCRIPTION:
  This function parses the buffer in the OMX_BUFFERHEADER to check if there is a VOP

INPUT/OUTPUT PARAMETERS:
  buffer - pointer to OMX buffer header

RETURN VALUE:
  true if the buffer contains a VOP, false otherwise.

SIDE EFFECTS:
  None.

=============================================================================
*/
bool MP4_Utils::HasFrame(OMX_IN OMX_BUFFERHEADERTYPE * buffer)
{
   return find_code(buffer->pBuffer, buffer->nFilledLen,
          VOP_START_CODE_MASK, VOP_START_CODE) != NULL;
}

/*===========================================================================
FUNCTION:
  validate_profile_and_level

DESCRIPTION:
  This function validate the profile and level that is supported.

INPUT/OUTPUT PARAMETERS:
  uint32 profile_and_level_indication

RETURN VALUE:
  false it it's not supported
  true otherwise

SIDE EFFECTS:
  None.
===========================================================================*/
bool MP4_Utils::validate_profile_and_level(uint32 profile_and_level_indication)
{
   QTV_MSG_PRIO1(QTVDIAG_GENERAL, QTVDIAG_PRIO_MED,
            "MP4 profile and level %d\n",
            profile_and_level_indication);
   if ((m_default_profile_chk && m_default_level_chk)
       && (profile_and_level_indication != SIMPLE_PROFILE_LEVEL0)
       && (profile_and_level_indication != SIMPLE_PROFILE_LEVEL1)
       && (profile_and_level_indication != SIMPLE_PROFILE_LEVEL2)
       && (profile_and_level_indication != SIMPLE_PROFILE_LEVEL3)
       && (profile_and_level_indication != SIMPLE_PROFILE_LEVEL4A)) {
      QTV_MSG_PRIO(QTVDIAG_GENERAL, QTVDIAG_PRIO_FATAL,
              "Caution: INVALID_PROFILE_AND_LEVEL \n");
      return false;
   }
   return true;
}

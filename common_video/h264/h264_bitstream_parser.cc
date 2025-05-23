/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "common_video/h264/h264_bitstream_parser.h"

#include <stdlib.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "api/array_view.h"
#include "common_video/h264/h264_common.h"
#include "common_video/h264/pps_parser.h"
#include "common_video/h264/sps_parser.h"
#include "rtc_base/bitstream_reader.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

constexpr int kMaxAbsQpDeltaValue = 51;
constexpr int kMinQpValue = 0;
constexpr int kMaxQpValue = 51;

}  // namespace

H264BitstreamParser::H264BitstreamParser() = default;
H264BitstreamParser::~H264BitstreamParser() = default;

H264BitstreamParser::Result H264BitstreamParser::ParseNonParameterSetNalu(
    ArrayView<const uint8_t> source,
    uint8_t nalu_type) {
  if (!sps_ || !pps_)
    return kInvalidStream;

  last_slice_qp_delta_ = std::nullopt;
  const std::vector<uint8_t> slice_rbsp = H264::ParseRbsp(source);
  if (slice_rbsp.size() < H264::kNaluTypeSize)
    return kInvalidStream;

  BitstreamReader slice_reader(slice_rbsp);
  slice_reader.ConsumeBits(H264::kNaluTypeSize * 8);

  // Check to see if this is an IDR slice, which has an extra field to parse
  // out.
  bool is_idr = (source[0] & 0x0F) == H264::NaluType::kIdr;
  uint8_t nal_ref_idc = (source[0] & 0x60) >> 5;

  uint32_t num_ref_idx_l0_active_minus1 =
      pps_->num_ref_idx_l0_default_active_minus1;
  uint32_t num_ref_idx_l1_active_minus1 =
      pps_->num_ref_idx_l1_default_active_minus1;

  // first_mb_in_slice: ue(v)
  slice_reader.ReadExponentialGolomb();
  // slice_type: ue(v)
  uint32_t slice_type = slice_reader.ReadExponentialGolomb();
  // slice_type's 5..9 range is used to indicate that all slices of a picture
  // have the same value of slice_type % 5, we don't care about that, so we map
  // to the corresponding 0..4 range.
  slice_type %= 5;
  // pic_parameter_set_id: ue(v)
  slice_reader.ReadExponentialGolomb();
  if (sps_->separate_colour_plane_flag == 1) {
    // colour_plane_id
    slice_reader.ConsumeBits(2);
  }
  // frame_num: u(v)
  // Represented by log2_max_frame_num bits.
  slice_reader.ConsumeBits(sps_->log2_max_frame_num);
  bool field_pic_flag = false;
  if (sps_->frame_mbs_only_flag == 0) {
    // field_pic_flag: u(1)
    field_pic_flag = slice_reader.Read<bool>();
    if (field_pic_flag) {
      // bottom_field_flag: u(1)
      slice_reader.ConsumeBits(1);
    }
  }
  if (is_idr) {
    // idr_pic_id: ue(v)
    slice_reader.ReadExponentialGolomb();
  }
  // pic_order_cnt_lsb: u(v)
  // Represented by sps_.log2_max_pic_order_cnt_lsb bits.
  if (sps_->pic_order_cnt_type == 0) {
    slice_reader.ConsumeBits(sps_->log2_max_pic_order_cnt_lsb);
    if (pps_->bottom_field_pic_order_in_frame_present_flag && !field_pic_flag) {
      // delta_pic_order_cnt_bottom: se(v)
      slice_reader.ReadExponentialGolomb();
    }
  }
  if (sps_->pic_order_cnt_type == 1 &&
      !sps_->delta_pic_order_always_zero_flag) {
    // delta_pic_order_cnt[0]: se(v)
    slice_reader.ReadExponentialGolomb();
    if (pps_->bottom_field_pic_order_in_frame_present_flag && !field_pic_flag) {
      // delta_pic_order_cnt[1]: se(v)
      slice_reader.ReadExponentialGolomb();
    }
  }
  if (pps_->redundant_pic_cnt_present_flag) {
    // redundant_pic_cnt: ue(v)
    slice_reader.ReadExponentialGolomb();
  }
  if (slice_type == H264::SliceType::kB) {
    // direct_spatial_mv_pred_flag: u(1)
    slice_reader.ConsumeBits(1);
  }
  switch (slice_type) {
    case H264::SliceType::kP:
    case H264::SliceType::kB:
    case H264::SliceType::kSp:
      // num_ref_idx_active_override_flag: u(1)
      if (slice_reader.Read<bool>()) {
        // num_ref_idx_l0_active_minus1: ue(v)
        num_ref_idx_l0_active_minus1 = slice_reader.ReadExponentialGolomb();
        if (!slice_reader.Ok() ||
            num_ref_idx_l0_active_minus1 > H264::kMaxReferenceIndex) {
          return kInvalidStream;
        }
        if (slice_type == H264::SliceType::kB) {
          // num_ref_idx_l1_active_minus1: ue(v)
          num_ref_idx_l1_active_minus1 = slice_reader.ReadExponentialGolomb();
          if (!slice_reader.Ok() ||
              num_ref_idx_l1_active_minus1 > H264::kMaxReferenceIndex) {
            return kInvalidStream;
          }
        }
      }
      break;
    default:
      break;
  }
  if (!slice_reader.Ok()) {
    return kInvalidStream;
  }
  // assume nal_unit_type != 20 && nal_unit_type != 21:
  if (nalu_type == 20 || nalu_type == 21) {
    RTC_LOG(LS_ERROR) << "Unsupported nal unit type.";
    return kUnsupportedStream;
  }
  // if (nal_unit_type == 20 || nal_unit_type == 21)
  //   ref_pic_list_mvc_modification()
  // else
  {
    // ref_pic_list_modification():
    // `slice_type` checks here don't use named constants as they aren't named
    // in the spec for this segment. Keeping them consistent makes it easier to
    // verify that they are both the same.
    if (slice_type % 5 != 2 && slice_type % 5 != 4) {
      // ref_pic_list_modification_flag_l0: u(1)
      if (slice_reader.Read<bool>()) {
        uint32_t modification_of_pic_nums_idc;
        do {
          // modification_of_pic_nums_idc: ue(v)
          modification_of_pic_nums_idc = slice_reader.ReadExponentialGolomb();
          if (modification_of_pic_nums_idc == 0 ||
              modification_of_pic_nums_idc == 1) {
            // abs_diff_pic_num_minus1: ue(v)
            slice_reader.ReadExponentialGolomb();
          } else if (modification_of_pic_nums_idc == 2) {
            // long_term_pic_num: ue(v)
            slice_reader.ReadExponentialGolomb();
          }
        } while (modification_of_pic_nums_idc != 3 && slice_reader.Ok());
      }
    }
    if (slice_type % 5 == 1) {
      // ref_pic_list_modification_flag_l1: u(1)
      if (slice_reader.Read<bool>()) {
        uint32_t modification_of_pic_nums_idc;
        do {
          // modification_of_pic_nums_idc: ue(v)
          modification_of_pic_nums_idc = slice_reader.ReadExponentialGolomb();
          if (modification_of_pic_nums_idc == 0 ||
              modification_of_pic_nums_idc == 1) {
            // abs_diff_pic_num_minus1: ue(v)
            slice_reader.ReadExponentialGolomb();
          } else if (modification_of_pic_nums_idc == 2) {
            // long_term_pic_num: ue(v)
            slice_reader.ReadExponentialGolomb();
          }
        } while (modification_of_pic_nums_idc != 3 && slice_reader.Ok());
      }
    }
  }
  if (!slice_reader.Ok()) {
    return kInvalidStream;
  }
  if ((pps_->weighted_pred_flag && (slice_type == H264::SliceType::kP ||
                                    slice_type == H264::SliceType::kSp)) ||
      (pps_->weighted_bipred_idc == 1 && slice_type == H264::SliceType::kB)) {
    // pred_weight_table()
    // luma_log2_weight_denom: ue(v)
    slice_reader.ReadExponentialGolomb();

    // If separate_colour_plane_flag is equal to 0, ChromaArrayType is set equal
    // to chroma_format_idc. Otherwise(separate_colour_plane_flag is equal to
    // 1), ChromaArrayType is set equal to 0.
    uint8_t chroma_array_type =
        sps_->separate_colour_plane_flag == 0 ? sps_->chroma_format_idc : 0;

    if (chroma_array_type != 0) {
      // chroma_log2_weight_denom: ue(v)
      slice_reader.ReadExponentialGolomb();
    }

    for (uint32_t i = 0; i <= num_ref_idx_l0_active_minus1; i++) {
      //    luma_weight_l0_flag 2 u(1)
      if (slice_reader.Read<bool>()) {
        // luma_weight_l0[i] 2 se(v)
        slice_reader.ReadExponentialGolomb();
        // luma_offset_l0[i] 2 se(v)
        slice_reader.ReadExponentialGolomb();
      }
      if (chroma_array_type != 0) {
        // chroma_weight_l0_flag: u(1)
        if (slice_reader.Read<bool>()) {
          for (uint8_t j = 0; j < 2; j++) {
            // chroma_weight_l0[i][j] 2 se(v)
            slice_reader.ReadExponentialGolomb();
            // chroma_offset_l0[i][j] 2 se(v)
            slice_reader.ReadExponentialGolomb();
          }
        }
      }
    }
    if (slice_type % 5 == 1) {
      for (uint32_t i = 0; i <= num_ref_idx_l1_active_minus1; i++) {
        // luma_weight_l1_flag: u(1)
        if (slice_reader.Read<bool>()) {
          // luma_weight_l1[i] 2 se(v)
          slice_reader.ReadExponentialGolomb();
          // luma_offset_l1[i] 2 se(v)
          slice_reader.ReadExponentialGolomb();
        }
        if (chroma_array_type != 0) {
          // chroma_weight_l1_flag: u(1)
          if (slice_reader.Read<bool>()) {
            for (uint8_t j = 0; j < 2; j++) {
              // chroma_weight_l1[i][j] 2 se(v)
              slice_reader.ReadExponentialGolomb();
              // chroma_offset_l1[i][j] 2 se(v)
              slice_reader.ReadExponentialGolomb();
            }
          }
        }
      }
    }
  }
  if (nal_ref_idc != 0) {
    // dec_ref_pic_marking():
    if (is_idr) {
      // no_output_of_prior_pics_flag: u(1)
      // long_term_reference_flag: u(1)
      slice_reader.ConsumeBits(2);
    } else {
      // adaptive_ref_pic_marking_mode_flag: u(1)
      if (slice_reader.Read<bool>()) {
        uint32_t memory_management_control_operation;
        do {
          // memory_management_control_operation: ue(v)
          memory_management_control_operation =
              slice_reader.ReadExponentialGolomb();
          if (memory_management_control_operation == 1 ||
              memory_management_control_operation == 3) {
            // difference_of_pic_nums_minus1: ue(v)
            slice_reader.ReadExponentialGolomb();
          }
          if (memory_management_control_operation == 2) {
            // long_term_pic_num: ue(v)
            slice_reader.ReadExponentialGolomb();
          }
          if (memory_management_control_operation == 3 ||
              memory_management_control_operation == 6) {
            // long_term_frame_idx: ue(v)
            slice_reader.ReadExponentialGolomb();
          }
          if (memory_management_control_operation == 4) {
            // max_long_term_frame_idx_plus1: ue(v)
            slice_reader.ReadExponentialGolomb();
          }
        } while (memory_management_control_operation != 0 && slice_reader.Ok());
      }
    }
  }
  if (pps_->entropy_coding_mode_flag && slice_type != H264::SliceType::kI &&
      slice_type != H264::SliceType::kSi) {
    // cabac_init_idc: ue(v)
    slice_reader.ReadExponentialGolomb();
  }

  int last_slice_qp_delta = slice_reader.ReadSignedExponentialGolomb();
  if (!slice_reader.Ok()) {
    return kInvalidStream;
  }
  if (abs(last_slice_qp_delta) > kMaxAbsQpDeltaValue) {
    // Something has gone wrong, and the parsed value is invalid.
    RTC_LOG(LS_WARNING) << "Parsed QP value out of range.";
    return kInvalidStream;
  }

  last_slice_qp_delta_ = last_slice_qp_delta;
  return kOk;
}

void H264BitstreamParser::ParseSlice(ArrayView<const uint8_t> slice) {
  if (slice.empty()) {
    return;
  }
  H264::NaluType nalu_type = H264::ParseNaluType(slice[0]);
  switch (nalu_type) {
    case H264::NaluType::kSps: {
      sps_ = SpsParser::ParseSps(slice.subview(H264::kNaluTypeSize));
      if (!sps_)
        RTC_DLOG(LS_WARNING) << "Unable to parse SPS from H264 bitstream.";
      break;
    }
    case H264::NaluType::kPps: {
      pps_ = PpsParser::ParsePps(slice.subview(H264::kNaluTypeSize));
      if (!pps_)
        RTC_DLOG(LS_WARNING) << "Unable to parse PPS from H264 bitstream.";
      break;
    }
    case H264::NaluType::kAud:
    case H264::NaluType::kFiller:
    case H264::NaluType::kSei:
    case H264::NaluType::kPrefix:
      break;  // Ignore these nalus, as we don't care about their contents.
    default:
      Result res = ParseNonParameterSetNalu(slice, nalu_type);
      if (res != kOk)
        RTC_DLOG(LS_INFO) << "Failed to parse bitstream. NAL type "
                          << static_cast<int>(nalu_type) << ", error: " << res;
      break;
  }
}

void H264BitstreamParser::ParseBitstream(ArrayView<const uint8_t> bitstream) {
  std::vector<H264::NaluIndex> nalu_indices = H264::FindNaluIndices(bitstream);
  for (const H264::NaluIndex& index : nalu_indices)
    ParseSlice(
        bitstream.subview(index.payload_start_offset, index.payload_size));
}

std::optional<int> H264BitstreamParser::GetLastSliceQp() const {
  if (!last_slice_qp_delta_ || !pps_)
    return std::nullopt;
  const int qp = 26 + pps_->pic_init_qp_minus26 + *last_slice_qp_delta_;
  if (qp < kMinQpValue || qp > kMaxQpValue) {
    RTC_LOG(LS_ERROR) << "Parsed invalid QP from bitstream.";
    return std::nullopt;
  }
  return qp;
}

}  // namespace webrtc

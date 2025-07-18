/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/packet/chunk/data_chunk.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "api/array_view.h"
#include "net/dcsctp/common/internal_types.h"
#include "net/dcsctp/packet/bounded_byte_reader.h"
#include "net/dcsctp/packet/bounded_byte_writer.h"
#include "net/dcsctp/packet/chunk/data_common.h"
#include "net/dcsctp/packet/data.h"
#include "net/dcsctp/public/types.h"
#include "rtc_base/strings/string_builder.h"

namespace dcsctp {

// https://tools.ietf.org/html/rfc4960#section-3.3.1

//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |   Type = 0    | Reserved|U|B|E|    Length                     |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                              TSN                              |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |      Stream Identifier S      |   Stream Sequence Number n    |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                  Payload Protocol Identifier                  |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  \                                                               \
//  /                 User Data (seq n of Stream S)                 /
//  \                                                               \
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
constexpr int DataChunk::kType;

std::optional<DataChunk> DataChunk::Parse(
    webrtc::ArrayView<const uint8_t> data) {
  std::optional<BoundedByteReader<kHeaderSize>> reader = ParseTLV(data);
  if (!reader.has_value()) {
    return std::nullopt;
  }

  uint8_t flags = reader->Load8<1>();
  TSN tsn(reader->Load32<4>());
  StreamID stream_identifier(reader->Load16<8>());
  SSN ssn(reader->Load16<10>());
  PPID ppid(reader->Load32<12>());

  Options options;
  options.is_end = Data::IsEnd((flags & (1 << kFlagsBitEnd)) != 0);
  options.is_beginning =
      Data::IsBeginning((flags & (1 << kFlagsBitBeginning)) != 0);
  options.is_unordered = IsUnordered((flags & (1 << kFlagsBitUnordered)) != 0);
  options.immediate_ack =
      ImmediateAckFlag((flags & (1 << kFlagsBitImmediateAck)) != 0);

  return DataChunk(tsn, stream_identifier, ssn, ppid,
                   std::vector<uint8_t>(reader->variable_data().begin(),
                                        reader->variable_data().end()),
                   options);
}

void DataChunk::SerializeTo(std::vector<uint8_t>& out) const {
  BoundedByteWriter<kHeaderSize> writer = AllocateTLV(out, payload().size());

  writer.Store8<1>(
      (*options().is_end ? (1 << kFlagsBitEnd) : 0) |
      (*options().is_beginning ? (1 << kFlagsBitBeginning) : 0) |
      (*options().is_unordered ? (1 << kFlagsBitUnordered) : 0) |
      (*options().immediate_ack ? (1 << kFlagsBitImmediateAck) : 0));
  writer.Store32<4>(*tsn());
  writer.Store16<8>(*stream_id());
  writer.Store16<10>(*ssn());
  writer.Store32<12>(*ppid());

  writer.CopyToVariableData(payload());
}

std::string DataChunk::ToString() const {
  webrtc::StringBuilder sb;
  sb << "DATA, type=" << (options().is_unordered ? "unordered" : "ordered")
     << "::"
     << (*options().is_beginning && *options().is_end ? "complete"
         : *options().is_beginning                    ? "first"
         : *options().is_end                          ? "last"
                                                      : "middle")
     << ", tsn=" << *tsn() << ", sid=" << *stream_id() << ", ssn=" << *ssn()
     << ", ppid=" << *ppid() << ", length=" << payload().size();
  return sb.Release();
}

}  // namespace dcsctp

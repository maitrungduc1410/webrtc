/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/data_channel_controller.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "api/data_channel_event_observer_interface.h"
#include "api/data_channel_interface.h"
#include "api/make_ref_counted.h"
#include "api/priority.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/transport/data_channel_transport_interface.h"
#include "api/units/timestamp.h"
#include "media/sctp/sctp_transport_internal.h"
#include "pc/peer_connection_internal.h"
#include "pc/sctp_data_channel.h"
#include "pc/sctp_utils.h"
#include "pc/test/mock_peer_connection_internal.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/null_socket_server.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/run_loop.h"

namespace webrtc {

namespace {

using Message = DataChannelEventObserverInterface::Message;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SizeIs;

constexpr uint8_t kSomeData[] = {5, 4, 3, 2, 1};

class MockDataChannelTransport : public DataChannelTransportInterface {
 public:
  ~MockDataChannelTransport() override {}

  MOCK_METHOD(RTCError,
              OpenChannel,
              (int channel_id, PriorityValue priority),
              (override));
  MOCK_METHOD(RTCError,
              SendData,
              (int channel_id,
               const SendDataParams& params,
               const CopyOnWriteBuffer& buffer),
              (override));
  MOCK_METHOD(RTCError, CloseChannel, (int channel_id), (override));
  MOCK_METHOD(void, SetDataSink, (DataChannelSink * sink), (override));
  MOCK_METHOD(bool, IsReadyToSend, (), (const, override));
  MOCK_METHOD(size_t, buffered_amount, (int channel_id), (const, override));
  MOCK_METHOD(size_t,
              buffered_amount_low_threshold,
              (int channel_id),
              (const, override));
  MOCK_METHOD(void,
              SetBufferedAmountLowThreshold,
              (int channel_id, size_t bytes),
              (override));
};

class MockDataChannelEventObserver : public DataChannelEventObserverInterface {
 public:
  MOCK_METHOD(void,
              OnMessage,
              (const DataChannelEventObserverInterface::Message& message),
              (override));
};

// Convenience class for tests to ensure that shutdown methods for DCC
// are consistently called. In practice SdpOfferAnswerHandler will call
// TeardownDataChannelTransport_n on the network thread when destroying the
// data channel transport and PeerConnection calls PrepareForShutdown() from
// within PeerConnection::Close(). The DataChannelControllerForTest class mimics
// behavior by calling those methods from within its destructor.
class DataChannelControllerForTest : public DataChannelController {
 public:
  explicit DataChannelControllerForTest(
      PeerConnectionInternal* pc,
      DataChannelTransportInterface* transport = nullptr)
      : DataChannelController(pc) {
    if (transport) {
      network_thread()->BlockingCall(
          [&] { SetupDataChannelTransport_n(transport); });
    }
  }

  ~DataChannelControllerForTest() override {
    network_thread()->BlockingCall(
        [&] { TeardownDataChannelTransport_n(RTCError::OK()); });
    PrepareForShutdown();
  }
};

class DataChannelControllerTest : public ::testing::Test {
 protected:
  DataChannelControllerTest()
      : network_thread_(std::make_unique<NullSocketServer>()) {
    network_thread_.Start();
    pc_ = make_ref_counted<NiceMock<MockPeerConnectionInternal>>();
    ON_CALL(*pc_, signaling_thread).WillByDefault(Return(Thread::Current()));
    ON_CALL(*pc_, network_thread).WillByDefault(Return(&network_thread_));
  }

  ~DataChannelControllerTest() override {
    run_loop_.Flush();
    network_thread_.Stop();
  }

  ScopedBaseFakeClock clock_;
  test::RunLoop run_loop_;
  Thread network_thread_;
  scoped_refptr<NiceMock<MockPeerConnectionInternal>> pc_;
};

TEST_F(DataChannelControllerTest, CreateAndDestroy) {
  DataChannelControllerForTest dcc(pc_.get());
}

TEST_F(DataChannelControllerTest, CreateDataChannelEarlyRelease) {
  DataChannelControllerForTest dcc(pc_.get());
  auto ret = dcc.InternalCreateDataChannelWithProxy(
      "label", InternalDataChannelInit(DataChannelInit()));
  ASSERT_TRUE(ret.ok());
  auto channel = ret.MoveValue();
  // DCC still holds a reference to the channel. Release this reference early.
  channel = nullptr;
}

TEST_F(DataChannelControllerTest, CreateDataChannelEarlyClose) {
  DataChannelControllerForTest dcc(pc_.get());
  EXPECT_FALSE(dcc.HasDataChannels());
  EXPECT_FALSE(dcc.HasUsedDataChannels());
  auto ret = dcc.InternalCreateDataChannelWithProxy(
      "label", InternalDataChannelInit(DataChannelInit()));
  ASSERT_TRUE(ret.ok());
  auto channel = ret.MoveValue();
  EXPECT_TRUE(dcc.HasDataChannels());
  EXPECT_TRUE(dcc.HasUsedDataChannels());
  channel->Close();
  run_loop_.Flush();
  EXPECT_FALSE(dcc.HasDataChannels());
  EXPECT_TRUE(dcc.HasUsedDataChannels());
}

TEST_F(DataChannelControllerTest, CreateDataChannelLateRelease) {
  auto dcc = std::make_unique<DataChannelControllerForTest>(pc_.get());
  auto ret = dcc->InternalCreateDataChannelWithProxy(
      "label", InternalDataChannelInit(DataChannelInit()));
  ASSERT_TRUE(ret.ok());
  auto channel = ret.MoveValue();
  dcc.reset();
  channel = nullptr;
}

TEST_F(DataChannelControllerTest, CloseAfterControllerDestroyed) {
  auto dcc = std::make_unique<DataChannelControllerForTest>(pc_.get());
  auto ret = dcc->InternalCreateDataChannelWithProxy(
      "label", InternalDataChannelInit(DataChannelInit()));
  ASSERT_TRUE(ret.ok());
  auto channel = ret.MoveValue();
  dcc.reset();
  channel->Close();
}

// Allocate the maximum number of data channels and then one more.
// The last allocation should fail.
TEST_F(DataChannelControllerTest, MaxChannels) {
  NiceMock<MockDataChannelTransport> transport;
  int channel_id = 0;

  ON_CALL(*pc_, GetSctpSslRole_n).WillByDefault([&]() {
    return std::optional<SSLRole>((channel_id & 1) ? SSL_SERVER : SSL_CLIENT);
  });

  DataChannelControllerForTest dcc(pc_.get(), &transport);

  // Allocate the maximum number of channels + 1. Inside the loop, the creation
  // process will allocate a stream id for each channel.
  for (channel_id = 0; channel_id <= kMaxSctpStreams; ++channel_id) {
    auto ret = dcc.InternalCreateDataChannelWithProxy(
        "label", InternalDataChannelInit(DataChannelInit()));
    if (channel_id == kMaxSctpStreams) {
      // We've reached the maximum and the previous call should have failed.
      EXPECT_FALSE(ret.ok());
    } else {
      // We're still working on saturating the pool. Things should be working.
      EXPECT_TRUE(ret.ok());
    }
  }
}

TEST_F(DataChannelControllerTest, BufferedAmountIncludesFromTransport) {
  NiceMock<MockDataChannelTransport> transport;
  EXPECT_CALL(transport, buffered_amount(0)).WillOnce(Return(4711));
  ON_CALL(*pc_, GetSctpSslRole_n).WillByDefault([&]() { return SSL_CLIENT; });

  DataChannelControllerForTest dcc(pc_.get(), &transport);
  auto dc = dcc.InternalCreateDataChannelWithProxy(
                   "label", InternalDataChannelInit(DataChannelInit()))
                .MoveValue();
  EXPECT_EQ(dc->buffered_amount(), 4711u);
}

// Test that while a data channel is in the `kClosing` state, its StreamId does
// not get re-used for new channels. Only once the state reaches `kClosed`
// should a StreamId be available again for allocation.
TEST_F(DataChannelControllerTest, NoStreamIdReuseWhileClosing) {
  ON_CALL(*pc_, GetSctpSslRole_n).WillByDefault([&]() { return SSL_CLIENT; });

  NiceMock<MockDataChannelTransport> transport;  // Wider scope than `dcc`.
  DataChannelControllerForTest dcc(pc_.get(), &transport);

  // Create the first channel and check that we got the expected, first sid.
  auto channel1 = dcc.InternalCreateDataChannelWithProxy(
                         "label", InternalDataChannelInit(DataChannelInit()))
                      .MoveValue();
  ASSERT_EQ(channel1->id(), 0);

  // Start closing the channel and make sure its state is `kClosing`
  channel1->Close();
  ASSERT_EQ(channel1->state(), DataChannelInterface::DataState::kClosing);

  // Create a second channel and make sure we get a new StreamId, not the same
  // as that of channel1.
  auto channel2 = dcc.InternalCreateDataChannelWithProxy(
                         "label2", InternalDataChannelInit(DataChannelInit()))
                      .MoveValue();
  ASSERT_NE(channel2->id(), channel1->id());  // In practice the id will be 2.

  // Simulate the acknowledgement of the channel closing from the transport.
  // This completes the closing operation of channel1.
  pc_->network_thread()->BlockingCall([&] { dcc.OnChannelClosed(0); });
  run_loop_.Flush();
  ASSERT_EQ(channel1->state(), DataChannelInterface::DataState::kClosed);

  // Now create a third channel. This time, the id of the first channel should
  // be available again and therefore the ids of the first and third channels
  // should be the same.
  auto channel3 = dcc.InternalCreateDataChannelWithProxy(
                         "label3", InternalDataChannelInit(DataChannelInit()))
                      .MoveValue();
  EXPECT_EQ(channel3->id(), channel1->id());
}

TEST_F(DataChannelControllerTest, ObserverNotifiedOnStringMessageSent) {
  NiceMock<MockDataChannelTransport> transport;
  DataChannelControllerForTest dcc(pc_.get(), &transport);

  std::vector<Message> messages;
  auto observer = std::make_unique<NiceMock<MockDataChannelEventObserver>>();
  ON_CALL(*observer, OnMessage).WillByDefault([&](const Message& m) {
    messages.push_back(m);
  });
  network_thread_.BlockingCall(
      [&]() { dcc.SetEventObserver(std::move(observer)); });

  RTCErrorOr<scoped_refptr<DataChannelInterface>> ret =
      dcc.InternalCreateDataChannelWithProxy(
          "TestingSomeSendStuff",
          InternalDataChannelInit({.negotiated = true, .id = 5}));
  ASSERT_TRUE(ret.ok());
  auto channel = ret.MoveValue();

  clock_.SetTime(Timestamp::Millis(123));
  network_thread_.BlockingCall([&]() {
    dcc.SendData(StreamId(5), {.type = DataMessageType::kText},
                 CopyOnWriteBuffer(kSomeData));
  });

  channel->Close();
  run_loop_.Flush();

  ASSERT_THAT(messages, SizeIs(1));
  EXPECT_EQ(messages[0].unix_timestamp_ms(), 123);
  EXPECT_EQ(messages[0].datachannel_id(), 5);
  EXPECT_EQ(messages[0].label(), "TestingSomeSendStuff");
  EXPECT_EQ(messages[0].direction(), Message::Direction::kSend);
  EXPECT_EQ(messages[0].data_type(), Message::DataType::kString);
  EXPECT_THAT(messages[0].data(), ElementsAreArray(kSomeData));
}

TEST_F(DataChannelControllerTest, ObserverNotifiedOnBinaryMessageSent) {
  NiceMock<MockDataChannelTransport> transport;
  DataChannelControllerForTest dcc(pc_.get(), &transport);

  std::vector<Message> messages;
  auto observer = std::make_unique<NiceMock<MockDataChannelEventObserver>>();
  ON_CALL(*observer, OnMessage).WillByDefault([&](const Message& m) {
    messages.push_back(m);
  });
  network_thread_.BlockingCall(
      [&]() { dcc.SetEventObserver(std::move(observer)); });

  RTCErrorOr<scoped_refptr<DataChannelInterface>> ret =
      dcc.InternalCreateDataChannelWithProxy(
          "TestingSomeSendStuff",
          InternalDataChannelInit({.negotiated = true, .id = 5}));
  ASSERT_TRUE(ret.ok());
  auto channel = ret.MoveValue();

  clock_.SetTime(Timestamp::Millis(123));
  network_thread_.BlockingCall([&]() {
    dcc.SendData(StreamId(5), {.type = DataMessageType::kBinary},
                 CopyOnWriteBuffer(kSomeData));
  });

  channel->Close();
  run_loop_.Flush();

  ASSERT_THAT(messages, SizeIs(1));
  EXPECT_EQ(messages[0].unix_timestamp_ms(), 123);
  EXPECT_EQ(messages[0].datachannel_id(), 5);
  EXPECT_EQ(messages[0].label(), "TestingSomeSendStuff");
  EXPECT_EQ(messages[0].direction(), Message::Direction::kSend);
  EXPECT_EQ(messages[0].data_type(), Message::DataType::kBinary);
  EXPECT_THAT(messages[0].data(), ElementsAreArray(kSomeData));
}

TEST_F(DataChannelControllerTest, ObserverNotNotifiedOnControlMessageSent) {
  NiceMock<MockDataChannelTransport> transport;
  DataChannelControllerForTest dcc(pc_.get(), &transport);

  std::vector<Message> messages;
  auto observer = std::make_unique<NiceMock<MockDataChannelEventObserver>>();
  ON_CALL(*observer, OnMessage).WillByDefault([&](const Message& m) {
    messages.push_back(m);
  });
  network_thread_.BlockingCall(
      [&]() { dcc.SetEventObserver(std::move(observer)); });

  RTCErrorOr<scoped_refptr<DataChannelInterface>> ret =
      dcc.InternalCreateDataChannelWithProxy(
          "TestingSomeSendStuff",
          InternalDataChannelInit({.negotiated = true, .id = 5}));
  ASSERT_TRUE(ret.ok());
  auto channel = ret.MoveValue();

  network_thread_.BlockingCall([&]() {
    dcc.SendData(StreamId(5), {.type = DataMessageType::kControl},
                 CopyOnWriteBuffer(kSomeData));
  });

  channel->Close();
  run_loop_.Flush();

  ASSERT_TRUE(messages.empty());
}

TEST_F(DataChannelControllerTest, ObserverNotNotifiedOnTransportFailed) {
  NiceMock<MockDataChannelTransport> transport;
  ON_CALL(transport, SendData)
      .WillByDefault(Return(RTCError(RTCErrorType::INVALID_STATE)));
  DataChannelControllerForTest dcc(pc_.get(), &transport);

  std::vector<Message> messages;
  auto observer = std::make_unique<NiceMock<MockDataChannelEventObserver>>();
  ON_CALL(*observer, OnMessage).WillByDefault([&](const Message& m) {
    messages.push_back(m);
  });
  network_thread_.BlockingCall(
      [&]() { dcc.SetEventObserver(std::move(observer)); });

  RTCErrorOr<scoped_refptr<DataChannelInterface>> ret =
      dcc.InternalCreateDataChannelWithProxy(
          "TestingSomeSendStuff",
          InternalDataChannelInit({.negotiated = true, .id = 5}));
  ASSERT_TRUE(ret.ok());
  auto channel = ret.MoveValue();

  network_thread_.BlockingCall([&]() {
    dcc.SendData(StreamId(5), {.type = DataMessageType::kText},
                 CopyOnWriteBuffer(kSomeData));
  });

  channel->Close();
  run_loop_.Flush();

  ASSERT_TRUE(messages.empty());
}

TEST_F(DataChannelControllerTest, ObserverNotifiedOnStringMessageReceived) {
  NiceMock<MockDataChannelTransport> transport;
  DataChannelControllerForTest dcc(pc_.get(), &transport);

  std::vector<Message> messages;
  auto observer = std::make_unique<NiceMock<MockDataChannelEventObserver>>();
  ON_CALL(*observer, OnMessage).WillByDefault([&](const Message& m) {
    messages.push_back(m);
  });
  network_thread_.BlockingCall(
      [&]() { dcc.SetEventObserver(std::move(observer)); });

  RTCErrorOr<scoped_refptr<DataChannelInterface>> ret =
      dcc.InternalCreateDataChannelWithProxy(
          "TestingSomeReceiveStuff",
          InternalDataChannelInit({.negotiated = true, .id = 5}));
  ASSERT_TRUE(ret.ok());
  auto channel = ret.MoveValue();

  clock_.SetTime(Timestamp::Millis(123));
  network_thread_.BlockingCall([&]() {
    dcc.OnDataReceived(5, DataMessageType::kText, CopyOnWriteBuffer(kSomeData));
  });

  channel->Close();
  run_loop_.Flush();

  ASSERT_THAT(messages, SizeIs(1));
  EXPECT_EQ(messages[0].unix_timestamp_ms(), 123);
  EXPECT_EQ(messages[0].datachannel_id(), 5);
  EXPECT_EQ(messages[0].label(), "TestingSomeReceiveStuff");
  EXPECT_EQ(messages[0].direction(), Message::Direction::kReceive);
  EXPECT_EQ(messages[0].data_type(), Message::DataType::kString);
  EXPECT_THAT(messages[0].data(), ElementsAreArray(kSomeData));
}

TEST_F(DataChannelControllerTest, ObserverNotifiedOnBinaryMessageReceived) {
  NiceMock<MockDataChannelTransport> transport;
  DataChannelControllerForTest dcc(pc_.get(), &transport);

  std::vector<Message> messages;
  auto observer = std::make_unique<NiceMock<MockDataChannelEventObserver>>();
  ON_CALL(*observer, OnMessage).WillByDefault([&](const Message& m) {
    messages.push_back(m);
  });
  network_thread_.BlockingCall(
      [&]() { dcc.SetEventObserver(std::move(observer)); });

  RTCErrorOr<scoped_refptr<DataChannelInterface>> ret =
      dcc.InternalCreateDataChannelWithProxy(
          "TestingSomeReceiveStuff",
          InternalDataChannelInit({.negotiated = true, .id = 5}));
  ASSERT_TRUE(ret.ok());
  auto channel = ret.MoveValue();

  clock_.SetTime(Timestamp::Millis(123));
  network_thread_.BlockingCall([&]() {
    dcc.OnDataReceived(5, DataMessageType::kBinary,
                       CopyOnWriteBuffer(kSomeData));
  });

  channel->Close();
  run_loop_.Flush();

  ASSERT_THAT(messages, SizeIs(1));
  EXPECT_EQ(messages[0].unix_timestamp_ms(), 123);
  EXPECT_EQ(messages[0].datachannel_id(), 5);
  EXPECT_EQ(messages[0].label(), "TestingSomeReceiveStuff");
  EXPECT_EQ(messages[0].direction(), Message::Direction::kReceive);
  EXPECT_EQ(messages[0].data_type(), Message::DataType::kBinary);
  EXPECT_THAT(messages[0].data(), ElementsAreArray(kSomeData));
}

TEST_F(DataChannelControllerTest, ObserverNotNotifiedOnControlMessageReceived) {
  NiceMock<MockDataChannelTransport> transport;
  DataChannelControllerForTest dcc(pc_.get(), &transport);

  std::vector<Message> messages;
  auto observer = std::make_unique<NiceMock<MockDataChannelEventObserver>>();
  ON_CALL(*observer, OnMessage).WillByDefault([&](const Message& m) {
    messages.push_back(m);
  });
  network_thread_.BlockingCall(
      [&]() { dcc.SetEventObserver(std::move(observer)); });

  RTCErrorOr<scoped_refptr<DataChannelInterface>> ret =
      dcc.InternalCreateDataChannelWithProxy(
          "TestingSomeReceiveStuff",
          InternalDataChannelInit({.negotiated = true, .id = 5}));
  ASSERT_TRUE(ret.ok());
  auto channel = ret.MoveValue();

  network_thread_.BlockingCall([&]() {
    dcc.OnDataReceived(5, DataMessageType::kControl,
                       CopyOnWriteBuffer(kSomeData));
  });

  channel->Close();
  run_loop_.Flush();

  EXPECT_THAT(messages, IsEmpty());
}

TEST_F(DataChannelControllerTest, ObserverNotNotifiedOnUnknownId) {
  NiceMock<MockDataChannelTransport> transport;
  DataChannelControllerForTest dcc(pc_.get(), &transport);

  std::vector<Message> messages;
  auto observer = std::make_unique<NiceMock<MockDataChannelEventObserver>>();
  ON_CALL(*observer, OnMessage).WillByDefault([&](const Message& m) {
    messages.push_back(m);
  });
  network_thread_.BlockingCall(
      [&]() { dcc.SetEventObserver(std::move(observer)); });

  RTCErrorOr<scoped_refptr<DataChannelInterface>> ret =
      dcc.InternalCreateDataChannelWithProxy(
          "TestingSomeReceiveStuff",
          InternalDataChannelInit({.negotiated = true, .id = 5}));
  ASSERT_TRUE(ret.ok());
  auto channel = ret.MoveValue();

  network_thread_.BlockingCall([&]() {
    dcc.OnDataReceived(3, DataMessageType::kText, CopyOnWriteBuffer(kSomeData));
  });

  channel->Close();
  run_loop_.Flush();

  EXPECT_THAT(messages, IsEmpty());
}

}  // namespace
}  // namespace webrtc

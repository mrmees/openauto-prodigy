# open-androidauto Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a Qt-native C++17 Android Auto protocol library (`libs/open-androidauto/`) that replaces aasdk, implementing Transport → Messenger → Session bottom-up with tests at each layer.

**Architecture:** Layered protocol stack — ITransport (byte I/O) → Messenger (framing, encryption, multiplexing) → AASession (FSM lifecycle, channel registry). Single Qt thread for all protocol state. Signals/slots replace Boost.ASIO strands and custom Promises. Channel handlers are application-provided via a single `IChannelHandler` interface.

**Tech Stack:** Qt 6 (Core, Network), OpenSSL (memory BIO TLS 1.2), protobuf (proto2, 108 ported .proto files), C++17, CMake

**Design doc:** `docs/plans/2026-02-23-open-androidauto-design.md`
**Protocol reference:** `~/claude/reference/android-auto-protocol.md`
**Existing code being replaced:** `libs/aasdk/`

---

## Phase 1: Project Scaffolding

### Task 1.1: Create directory structure and CMakeLists.txt

**Files:**
- Create: `libs/open-androidauto/CMakeLists.txt`
- Create: `libs/open-androidauto/include/oaa/Version.hpp`
- Create: `libs/open-androidauto/tests/CMakeLists.txt`
- Create: `libs/open-androidauto/tests/test_version.cpp`

**Step 1: Create directory tree**

```bash
mkdir -p libs/open-androidauto/{include/oaa/{Transport,Messenger,Channel,Session},src/{Transport,Messenger,Channel,Session},proto,tests}
```

**Step 2: Write Version.hpp**

```cpp
// libs/open-androidauto/include/oaa/Version.hpp
#pragma once
#include <cstdint>

namespace oaa {

constexpr uint16_t PROTOCOL_MAJOR = 1;
constexpr uint16_t PROTOCOL_MINOR = 7;

constexpr uint16_t FRAME_MAX_PAYLOAD = 0x4000; // 16384 bytes
constexpr uint16_t FRAME_HEADER_SIZE = 2;
constexpr uint16_t FRAME_SIZE_SHORT = 2;
constexpr uint16_t FRAME_SIZE_EXTENDED = 6;
constexpr int TLS_OVERHEAD = 29;
constexpr int BIO_BUFFER_SIZE = 20480;

} // namespace oaa
```

**Step 3: Write top-level CMakeLists.txt**

```cmake
# libs/open-androidauto/CMakeLists.txt
cmake_minimum_required(VERSION 3.22)
project(open-androidauto VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network)
find_package(OpenSSL REQUIRED)

add_library(open-androidauto STATIC)

target_include_directories(open-androidauto PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(open-androidauto PUBLIC
    Qt6::Core
    Qt6::Network
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Tests
option(OAA_BUILD_TESTS "Build open-androidauto tests" ON)
if(OAA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

The tests CMakeLists.txt:

```cmake
# libs/open-androidauto/tests/CMakeLists.txt
find_package(Qt6 REQUIRED COMPONENTS Test)

function(oaa_add_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE open-androidauto Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

oaa_add_test(test_version test_version.cpp)
```

**Step 4: Write the version test**

```cpp
// libs/open-androidauto/tests/test_version.cpp
#include <QtTest/QtTest>
#include <oaa/Version.hpp>

class TestVersion : public QObject {
    Q_OBJECT
private slots:
    void testProtocolVersion() {
        QCOMPARE(oaa::PROTOCOL_MAJOR, uint16_t(1));
        QCOMPARE(oaa::PROTOCOL_MINOR, uint16_t(7));
    }
    void testFrameConstants() {
        QCOMPARE(oaa::FRAME_MAX_PAYLOAD, uint16_t(0x4000));
        QCOMPARE(oaa::FRAME_HEADER_SIZE, uint16_t(2));
        QCOMPARE(oaa::FRAME_SIZE_SHORT, uint16_t(2));
        QCOMPARE(oaa::FRAME_SIZE_EXTENDED, uint16_t(6));
    }
};

QTEST_MAIN(TestVersion)
#include "test_version.moc"
```

**Step 5: Build and run test**

```bash
cd libs/open-androidauto && mkdir -p build && cd build && cmake .. && make -j$(nproc)
ctest --output-on-failure
```
Expected: 1 test passes.

**Step 6: Commit**

```bash
git add libs/open-androidauto/
git commit -m "feat(oaa): scaffold open-androidauto library with version constants"
```

---

## Phase 2: Transport Layer

### Task 2.1: ITransport interface

**Files:**
- Create: `libs/open-androidauto/include/oaa/Transport/ITransport.hpp`

**Step 1: Write ITransport**

```cpp
// libs/open-androidauto/include/oaa/Transport/ITransport.hpp
#pragma once
#include <QObject>
#include <QByteArray>

namespace oaa {

class ITransport : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual ~ITransport() = default;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void write(const QByteArray& data) = 0;
    virtual bool isConnected() const = 0;

signals:
    void dataReceived(const QByteArray& data);
    void connected();
    void disconnected();
    void error(const QString& message);
};

} // namespace oaa
```

No test needed — pure interface. Tested via concrete implementations.

**Step 2: Commit**

```bash
git add libs/open-androidauto/include/oaa/Transport/ITransport.hpp
git commit -m "feat(oaa): add ITransport interface"
```

### Task 2.2: TCPTransport

**Files:**
- Create: `libs/open-androidauto/include/oaa/Transport/TCPTransport.hpp`
- Create: `libs/open-androidauto/src/Transport/TCPTransport.cpp`
- Create: `libs/open-androidauto/tests/test_tcp_transport.cpp`
- Modify: `libs/open-androidauto/CMakeLists.txt` (add source)
- Modify: `libs/open-androidauto/tests/CMakeLists.txt` (add test)

**Step 1: Write the failing test**

Test uses a local QTcpServer to verify TCPTransport connects, sends, receives, and disconnects.

```cpp
// libs/open-androidauto/tests/test_tcp_transport.cpp
#include <QtTest/QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSignalSpy>
#include <oaa/Transport/TCPTransport.hpp>

class TestTCPTransport : public QObject {
    Q_OBJECT
private slots:
    void testConnectAndSend() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        quint16 port = server.serverPort();

        oaa::TCPTransport transport;
        QSignalSpy connectedSpy(&transport, &oaa::ITransport::connected);
        QSignalSpy dataSpy(&transport, &oaa::ITransport::dataReceived);

        transport.connectToHost(QHostAddress::LocalHost, port);
        transport.start();

        // Accept server-side connection
        QVERIFY(server.waitForNewConnection(1000));
        QTcpSocket* serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket);
        QTRY_COMPARE(connectedSpy.count(), 1);

        // Send from transport to server
        transport.write(QByteArray("hello"));
        QVERIFY(serverSocket->waitForReadyRead(1000));
        QCOMPARE(serverSocket->readAll(), QByteArray("hello"));

        // Send from server to transport
        serverSocket->write(QByteArray("world"));
        serverSocket->flush();
        QTRY_COMPARE(dataSpy.count(), 1);
        QCOMPARE(dataSpy.first().first().toByteArray(), QByteArray("world"));

        transport.stop();
    }

    void testDisconnect() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        quint16 port = server.serverPort();

        oaa::TCPTransport transport;
        QSignalSpy disconnectedSpy(&transport, &oaa::ITransport::disconnected);

        transport.connectToHost(QHostAddress::LocalHost, port);
        transport.start();
        QVERIFY(server.waitForNewConnection(1000));
        QTcpSocket* serverSocket = server.nextPendingConnection();
        QTRY_VERIFY(transport.isConnected());

        // Server disconnects
        serverSocket->close();
        QTRY_COMPARE(disconnectedSpy.count(), 1);
        QVERIFY(!transport.isConnected());
    }
};

QTEST_MAIN(TestTCPTransport)
#include "test_tcp_transport.moc"
```

**Step 2: Run test — verify it fails** (TCPTransport doesn't exist yet)

**Step 3: Implement TCPTransport**

```cpp
// libs/open-androidauto/include/oaa/Transport/TCPTransport.hpp
#pragma once
#include <oaa/Transport/ITransport.hpp>
#include <QTcpSocket>
#include <QHostAddress>

namespace oaa {

class TCPTransport : public ITransport {
    Q_OBJECT
public:
    explicit TCPTransport(QObject* parent = nullptr);
    ~TCPTransport() override;

    void connectToHost(const QHostAddress& address, quint16 port);
    void setSocket(QTcpSocket* socket); // for pre-connected sockets (server-accepted)

    void start() override;
    void stop() override;
    void write(const QByteArray& data) override;
    bool isConnected() const override;

private:
    QTcpSocket* socket_ = nullptr;
    bool ownsSocket_ = true;

    void onReadyRead();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);
};

} // namespace oaa
```

```cpp
// libs/open-androidauto/src/Transport/TCPTransport.cpp
#include <oaa/Transport/TCPTransport.hpp>

namespace oaa {

TCPTransport::TCPTransport(QObject* parent)
    : ITransport(parent) {}

TCPTransport::~TCPTransport() { stop(); }

void TCPTransport::connectToHost(const QHostAddress& address, quint16 port) {
    if (!socket_) {
        socket_ = new QTcpSocket(this);
        ownsSocket_ = true;
    }
    socket_->connectToHost(address, port);
}

void TCPTransport::setSocket(QTcpSocket* socket) {
    socket_ = socket;
    socket_->setParent(this);
    ownsSocket_ = true;
}

void TCPTransport::start() {
    if (!socket_) return;
    connect(socket_, &QTcpSocket::readyRead, this, &TCPTransport::onReadyRead);
    connect(socket_, &QTcpSocket::disconnected, this, &TCPTransport::onDisconnected);
    connect(socket_, &QAbstractSocket::errorOccurred, this, &TCPTransport::onError);
    connect(socket_, &QTcpSocket::connected, this, &ITransport::connected);

    if (socket_->state() == QAbstractSocket::ConnectedState) {
        emit connected();
    }
}

void TCPTransport::stop() {
    if (socket_) {
        socket_->disconnect(this);
        if (socket_->state() != QAbstractSocket::UnconnectedState) {
            socket_->close();
        }
    }
}

void TCPTransport::write(const QByteArray& data) {
    if (socket_ && socket_->state() == QAbstractSocket::ConnectedState) {
        socket_->write(data);
    }
}

bool TCPTransport::isConnected() const {
    return socket_ && socket_->state() == QAbstractSocket::ConnectedState;
}

void TCPTransport::onReadyRead() {
    emit dataReceived(socket_->readAll());
}

void TCPTransport::onDisconnected() {
    emit disconnected();
}

void TCPTransport::onError(QAbstractSocket::SocketError) {
    emit error(socket_->errorString());
}

} // namespace oaa
```

**Step 4: Update CMakeLists.txt** — add `src/Transport/TCPTransport.cpp` to library sources, add test.

**Step 5: Build and run tests**

```bash
cd libs/open-androidauto/build && cmake .. && make -j$(nproc) && ctest --output-on-failure
```
Expected: All tests pass.

**Step 6: Commit**

```bash
git add libs/open-androidauto/
git commit -m "feat(oaa): implement TCPTransport with QTcpSocket"
```

### Task 2.3: ReplayTransport (for integration testing)

**Files:**
- Create: `libs/open-androidauto/include/oaa/Transport/ReplayTransport.hpp`
- Create: `libs/open-androidauto/src/Transport/ReplayTransport.cpp`
- Create: `libs/open-androidauto/tests/test_replay_transport.cpp`

**Step 1: Write the failing test**

```cpp
// libs/open-androidauto/tests/test_replay_transport.cpp
#include <QtTest/QtTest>
#include <QSignalSpy>
#include <oaa/Transport/ReplayTransport.hpp>

class TestReplayTransport : public QObject {
    Q_OBJECT
private slots:
    void testFeedData() {
        oaa::ReplayTransport transport;
        QSignalSpy dataSpy(&transport, &oaa::ITransport::dataReceived);

        transport.start();
        transport.feedData(QByteArray("\x00\x0b\x00\x04\x08\x01", 6));

        QTRY_COMPARE(dataSpy.count(), 1);
        QCOMPARE(dataSpy.first().first().toByteArray(),
                 QByteArray("\x00\x0b\x00\x04\x08\x01", 6));
    }

    void testWriteCapture() {
        oaa::ReplayTransport transport;
        transport.start();
        transport.write(QByteArray("sent-data"));
        QCOMPARE(transport.writtenData().size(), 1);
        QCOMPARE(transport.writtenData().first(), QByteArray("sent-data"));
    }
};

QTEST_MAIN(TestReplayTransport)
#include "test_replay_transport.moc"
```

**Step 2: Implement ReplayTransport**

ReplayTransport is a test double. `feedData()` simulates incoming bytes (emits `dataReceived`). `write()` captures outgoing bytes for assertion. No real socket.

```cpp
// libs/open-androidauto/include/oaa/Transport/ReplayTransport.hpp
#pragma once
#include <oaa/Transport/ITransport.hpp>
#include <QList>

namespace oaa {

class ReplayTransport : public ITransport {
    Q_OBJECT
public:
    explicit ReplayTransport(QObject* parent = nullptr);

    void start() override;
    void stop() override;
    void write(const QByteArray& data) override;
    bool isConnected() const override;

    // Test API
    void feedData(const QByteArray& data);
    void simulateConnect();
    void simulateDisconnect();
    QList<QByteArray> writtenData() const;
    void clearWritten();

private:
    bool connected_ = false;
    QList<QByteArray> written_;
};

} // namespace oaa
```

Implementation is straightforward — `feedData` emits `dataReceived`, `write` appends to `written_`, `simulateConnect/Disconnect` emit signals and toggle state.

**Step 3: Build, test, commit**

```bash
git commit -m "feat(oaa): add ReplayTransport for testing"
```

---

## Phase 3: Framing Layer

The core protocol engine. This is where the AA wire format lives.

### Task 3.1: Frame types and header parsing

**Files:**
- Create: `libs/open-androidauto/include/oaa/Messenger/FrameType.hpp`
- Create: `libs/open-androidauto/include/oaa/Messenger/FrameHeader.hpp`
- Create: `libs/open-androidauto/src/Messenger/FrameHeader.cpp`
- Create: `libs/open-androidauto/tests/test_frame_header.cpp`

**Step 1: Write FrameType enums**

```cpp
// libs/open-androidauto/include/oaa/Messenger/FrameType.hpp
#pragma once
#include <cstdint>

namespace oaa {

enum class FrameType : uint8_t {
    Middle = 0x00,
    First  = 0x01,
    Last   = 0x02,
    Bulk   = 0x03  // First | Last
};

enum class EncryptionType : uint8_t {
    Plain     = 0x00,
    Encrypted = 0x08
};

enum class MessageType : uint8_t {
    Specific = 0x00,  // channel-specific data
    Control  = 0x04   // control channel protocol messages
};

} // namespace oaa
```

**Step 2: Write the failing test**

```cpp
// libs/open-androidauto/tests/test_frame_header.cpp
#include <QtTest/QtTest>
#include <oaa/Messenger/FrameHeader.hpp>

class TestFrameHeader : public QObject {
    Q_OBJECT
private slots:
    void testParseBulkPlainControl() {
        // Channel 0, BULK | CONTROL | PLAIN = 0x07
        QByteArray raw("\x00\x07", 2);
        oaa::FrameHeader hdr = oaa::FrameHeader::parse(raw);
        QCOMPARE(hdr.channelId, uint8_t(0));
        QCOMPARE(hdr.frameType, oaa::FrameType::Bulk);
        QCOMPARE(hdr.encryptionType, oaa::EncryptionType::Plain);
        QCOMPARE(hdr.messageType, oaa::MessageType::Control);
    }

    void testParseFirstEncryptedSpecific() {
        // Channel 3 (video), FIRST | ENCRYPTED | SPECIFIC = 0x09
        QByteArray raw("\x03\x09", 2);
        oaa::FrameHeader hdr = oaa::FrameHeader::parse(raw);
        QCOMPARE(hdr.channelId, uint8_t(3));
        QCOMPARE(hdr.frameType, oaa::FrameType::First);
        QCOMPARE(hdr.encryptionType, oaa::EncryptionType::Encrypted);
        QCOMPARE(hdr.messageType, oaa::MessageType::Specific);
    }

    void testSerialize() {
        oaa::FrameHeader hdr{0, oaa::FrameType::Bulk,
                             oaa::EncryptionType::Plain,
                             oaa::MessageType::Control};
        QByteArray data = hdr.serialize();
        QCOMPARE(data.size(), 2);
        QCOMPARE(uint8_t(data[0]), uint8_t(0x00));
        QCOMPARE(uint8_t(data[1]), uint8_t(0x07));
    }

    void testRoundTrip() {
        oaa::FrameHeader original{4, oaa::FrameType::Middle,
                                   oaa::EncryptionType::Encrypted,
                                   oaa::MessageType::Specific};
        QByteArray wire = original.serialize();
        oaa::FrameHeader parsed = oaa::FrameHeader::parse(wire);
        QCOMPARE(parsed.channelId, original.channelId);
        QCOMPARE(parsed.frameType, original.frameType);
        QCOMPARE(parsed.encryptionType, original.encryptionType);
        QCOMPARE(parsed.messageType, original.messageType);
    }

    void testSizeFieldLength() {
        // FIRST and BULK use extended (6-byte) size for FIRST
        // Actually: FIRST = 6 bytes, MIDDLE/LAST = 2 bytes, BULK = 2 bytes
        QCOMPARE(oaa::FrameHeader::sizeFieldLength(oaa::FrameType::First), 6);
        QCOMPARE(oaa::FrameHeader::sizeFieldLength(oaa::FrameType::Middle), 2);
        QCOMPARE(oaa::FrameHeader::sizeFieldLength(oaa::FrameType::Last), 2);
        QCOMPARE(oaa::FrameHeader::sizeFieldLength(oaa::FrameType::Bulk), 2);
    }
};

QTEST_MAIN(TestFrameHeader)
#include "test_frame_header.moc"
```

**Step 3: Implement FrameHeader**

```cpp
// libs/open-androidauto/include/oaa/Messenger/FrameHeader.hpp
#pragma once
#include <oaa/Messenger/FrameType.hpp>
#include <QByteArray>

namespace oaa {

struct FrameHeader {
    uint8_t channelId = 0;
    FrameType frameType = FrameType::Bulk;
    EncryptionType encryptionType = EncryptionType::Plain;
    MessageType messageType = MessageType::Specific;

    static FrameHeader parse(const QByteArray& data);
    QByteArray serialize() const;
    static int sizeFieldLength(FrameType type);
};

} // namespace oaa
```

```cpp
// libs/open-androidauto/src/Messenger/FrameHeader.cpp
#include <oaa/Messenger/FrameHeader.hpp>

namespace oaa {

FrameHeader FrameHeader::parse(const QByteArray& data) {
    FrameHeader hdr;
    hdr.channelId = static_cast<uint8_t>(data[0]);
    uint8_t flags = static_cast<uint8_t>(data[1]);
    hdr.frameType = static_cast<FrameType>(flags & 0x03);
    hdr.messageType = static_cast<MessageType>(flags & 0x04);
    hdr.encryptionType = static_cast<EncryptionType>(flags & 0x08);
    return hdr;
}

QByteArray FrameHeader::serialize() const {
    QByteArray data(2, '\0');
    data[0] = static_cast<char>(channelId);
    data[1] = static_cast<char>(
        static_cast<uint8_t>(frameType) |
        static_cast<uint8_t>(messageType) |
        static_cast<uint8_t>(encryptionType));
    return data;
}

int FrameHeader::sizeFieldLength(FrameType type) {
    return (type == FrameType::First) ? 6 : 2;
}

} // namespace oaa
```

**Step 4: Build, test, commit**

```bash
git commit -m "feat(oaa): implement FrameHeader parse/serialize with wire format"
```

### Task 3.2: FrameSerializer (outgoing message → frames)

**Files:**
- Create: `libs/open-androidauto/include/oaa/Messenger/FrameSerializer.hpp`
- Create: `libs/open-androidauto/src/Messenger/FrameSerializer.cpp`
- Create: `libs/open-androidauto/tests/test_frame_serializer.cpp`

**Step 1: Write failing tests**

Tests cover:
1. Small message → single BULK frame
2. Exactly 16384-byte message → single BULK frame
3. 16385-byte message → FIRST + LAST frames
4. Large message → FIRST + MIDDLE(s) + LAST

Each test verifies the wire bytes: header, size field, payload content.

```cpp
// Key test case structure:
void testSmallMessage_singleBulk() {
    // 10-byte payload → BULK frame
    // Wire: [channelId, flags, sizeHi, sizeLo, ...payload...]
    QByteArray payload(10, 'A');
    auto frames = oaa::FrameSerializer::serialize(
        0, oaa::MessageType::Control, oaa::EncryptionType::Plain, payload);
    QCOMPARE(frames.size(), 1);
    // Verify header: channel=0, flags=BULK|CONTROL|PLAIN = 0x07
    QCOMPARE(uint8_t(frames[0][0]), uint8_t(0x00));
    QCOMPARE(uint8_t(frames[0][1]), uint8_t(0x07));
    // Size field: 2 bytes BE = 0x000A
    QCOMPARE(uint8_t(frames[0][2]), uint8_t(0x00));
    QCOMPARE(uint8_t(frames[0][3]), uint8_t(0x0A));
    // Payload starts at offset 4
    QCOMPARE(frames[0].mid(4), payload);
}

void testLargeMessage_multiFrame() {
    // 20000-byte payload → FIRST(16384) + LAST(3616)
    QByteArray payload(20000, 'B');
    auto frames = oaa::FrameSerializer::serialize(
        3, oaa::MessageType::Specific, oaa::EncryptionType::Encrypted, payload);
    QCOMPARE(frames.size(), 2);
    // FIRST frame: header(2) + size(6) + payload(16384)
    QCOMPARE(uint8_t(frames[0][1]) & 0x03, uint8_t(0x01)); // FIRST
    // LAST frame: header(2) + size(2) + payload(3616)
    QCOMPARE(uint8_t(frames[1][1]) & 0x03, uint8_t(0x02)); // LAST
}
```

**Step 2: Implement FrameSerializer**

`FrameSerializer::serialize()` is a static method that takes (channelId, messageType, encryptionType, payload) and returns `QList<QByteArray>` — one entry per frame. Each frame is header + size + payload bytes (encryption is NOT done here — that happens in Messenger).

Key logic:
- payload <= 16384 → single BULK frame, 2-byte size
- payload > 16384 → FIRST frame with 6-byte size (2-byte frame size + 4-byte total), then MIDDLE frames, then LAST frame, each with 2-byte size
- Frame payload chunks are exactly FRAME_MAX_PAYLOAD except the last

**Step 3: Build, test, commit**

```bash
git commit -m "feat(oaa): implement FrameSerializer for outgoing message fragmentation"
```

### Task 3.3: FrameAssembler (incoming frames → messages)

**Files:**
- Create: `libs/open-androidauto/include/oaa/Messenger/FrameAssembler.hpp`
- Create: `libs/open-androidauto/src/Messenger/FrameAssembler.cpp`
- Create: `libs/open-androidauto/tests/test_frame_assembler.cpp`

**Step 1: Write failing tests**

Tests cover:
1. Single BULK frame → complete message emitted
2. FIRST + LAST → complete message emitted after LAST
3. FIRST + MIDDLE + LAST → complete message emitted after LAST
4. Interleaved channels — channel 3 FIRST, channel 4 BULK, channel 3 LAST → both complete
5. Error: MIDDLE without FIRST → discarded, logged
6. Error: duplicate FIRST → discard previous partial, start new

```cpp
// Key test: interleaved channels
void testInterleavedChannels() {
    oaa::FrameAssembler assembler;
    QSignalSpy spy(&assembler, &oaa::FrameAssembler::messageAssembled);

    // Channel 3 FIRST (partial)
    assembler.onFrame(makeFirstFrame(3, partA, totalSize));
    QCOMPARE(spy.count(), 0);

    // Channel 4 BULK (complete, different channel)
    assembler.onFrame(makeBulkFrame(4, payloadB));
    QCOMPARE(spy.count(), 1); // channel 4 complete

    // Channel 3 LAST (completes channel 3)
    assembler.onFrame(makeLastFrame(3, partB));
    QCOMPARE(spy.count(), 2); // channel 3 now complete
}
```

**Step 2: Implement FrameAssembler**

FrameAssembler is a QObject that receives parsed frames and emits `messageAssembled(uint8_t channelId, MessageType messageType, QByteArray payload)` when a complete message is ready.

Internal state: `QHash<uint8_t, QByteArray>` — one partial buffer per channel ID. FIRST creates an entry, MIDDLE/LAST append, LAST/BULK emit and clear.

**Step 3: Build, test, commit**

```bash
git commit -m "feat(oaa): implement FrameAssembler with interleaved channel support"
```

### Task 3.4: Frame byte-stream parser

**Files:**
- Create: `libs/open-androidauto/include/oaa/Messenger/FrameParser.hpp`
- Create: `libs/open-androidauto/src/Messenger/FrameParser.cpp`
- Create: `libs/open-androidauto/tests/test_frame_parser.cpp`

This sits between the transport (raw bytes) and FrameAssembler (parsed frames). It accumulates bytes from `dataReceived` and emits complete frames.

**Step 1: Write failing tests**

Tests cover:
1. Complete frame arrives in one chunk → emitted immediately
2. Frame arrives byte-by-byte → emitted when complete
3. Two frames arrive in one chunk → both emitted
4. Partial frame header → waits for more data

**Step 2: Implement FrameParser**

State machine:
1. **ReadHeader** — accumulate 2 bytes → parse FrameHeader → determine size field length
2. **ReadSize** — accumulate 2 or 6 bytes → parse frame payload size
3. **ReadPayload** — accumulate N bytes → emit frame → back to ReadHeader

The parser emits `frameParsed(FrameHeader header, QByteArray payload)`.

**Step 3: Build, test, commit**

```bash
git commit -m "feat(oaa): implement FrameParser byte-stream to frame decoder"
```

---

## Phase 4: Cryptor and Encryption Policy

### Task 4.1: Cryptor (OpenSSL memory BIO TLS 1.2)

**Files:**
- Create: `libs/open-androidauto/include/oaa/Messenger/Cryptor.hpp`
- Create: `libs/open-androidauto/src/Messenger/Cryptor.cpp`
- Create: `libs/open-androidauto/tests/test_cryptor.cpp`

**Step 1: Write failing test**

Test the handshake between two Cryptor instances (one client, one server) using memory BIOs — no real network needed.

```cpp
void testHandshakeBetweenPeers() {
    oaa::Cryptor client;
    oaa::Cryptor server;
    client.init(oaa::Cryptor::Role::Client);
    server.init(oaa::Cryptor::Role::Server);

    // Drive handshake
    while (!client.isActive() || !server.isActive()) {
        QByteArray clientOut = client.readHandshakeBuffer();
        if (!clientOut.isEmpty()) server.writeHandshakeBuffer(clientOut);
        server.doHandshake();

        QByteArray serverOut = server.readHandshakeBuffer();
        if (!serverOut.isEmpty()) client.writeHandshakeBuffer(serverOut);
        client.doHandshake();
    }

    QVERIFY(client.isActive());
    QVERIFY(server.isActive());
}

void testEncryptDecrypt() {
    // After handshake...
    QByteArray plaintext("Hello AA Protocol");
    QByteArray ciphertext = client.encrypt(plaintext);
    QVERIFY(ciphertext != plaintext);
    QByteArray decrypted = server.decrypt(ciphertext, ciphertext.size());
    QCOMPARE(decrypted, plaintext);
}
```

**Step 2: Implement Cryptor**

Port the certificate and key from `libs/aasdk/src/Messenger/Cryptor.cpp:281-327`. Key methods:

- `init(Role)` — create SSL_CTX, load cert/key, create memory BIOs, set client/server state
- `doHandshake()` → `SSL_do_handshake()`, returns true when complete
- `readHandshakeBuffer()` → `BIO_read(writeBIO)` — outgoing TLS bytes
- `writeHandshakeBuffer(data)` → `BIO_write(readBIO)` — incoming TLS bytes
- `encrypt(plaintext)` → `SSL_write()` then `BIO_read(writeBIO)` → ciphertext
- `decrypt(ciphertext, frameLength)` → `BIO_write(readBIO)` then `SSL_read()` → plaintext, using `frameLength - 29` as expected plaintext size, reading in 2048-byte chunks
- `isActive()` — true after handshake completes

**Critical:** Use `TLS_client_method()` (not deprecated `TLSv1_2_client_method()`). Set `SSL_VERIFY_NONE`. Memory BIO buffer limit = 20480 bytes.

**Step 3: Build, test, commit**

```bash
git commit -m "feat(oaa): implement Cryptor with OpenSSL memory BIO TLS 1.2"
```

### Task 4.2: EncryptionPolicy (table-driven)

**Files:**
- Create: `libs/open-androidauto/include/oaa/Messenger/EncryptionPolicy.hpp`
- Create: `libs/open-androidauto/src/Messenger/EncryptionPolicy.cpp`
- Create: `libs/open-androidauto/tests/test_encryption_policy.cpp`

**Step 1: Write failing tests**

```cpp
void testPreSslAlwaysPlain() {
    oaa::EncryptionPolicy policy;
    // Before SSL, everything is plain
    QCOMPARE(policy.shouldEncrypt(0, 0x0001, false), false); // VERSION_REQUEST
    QCOMPARE(policy.shouldEncrypt(0, 0x0005, false), false); // SERVICE_DISCOVERY
    QCOMPARE(policy.shouldEncrypt(3, 0x8000, false), false); // VIDEO SETUP
}

void testPostSslControlExceptions() {
    oaa::EncryptionPolicy policy;
    // After SSL, version/handshake/auth still plain
    QCOMPARE(policy.shouldEncrypt(0, 0x0001, true), false); // VERSION_REQUEST
    QCOMPARE(policy.shouldEncrypt(0, 0x0002, true), false); // VERSION_RESPONSE
    QCOMPARE(policy.shouldEncrypt(0, 0x0003, true), false); // SSL_HANDSHAKE
    QCOMPARE(policy.shouldEncrypt(0, 0x0004, true), false); // AUTH_COMPLETE
    QCOMPARE(policy.shouldEncrypt(0, 0x000b, true), false); // PING_REQUEST
    QCOMPARE(policy.shouldEncrypt(0, 0x000c, true), false); // PING_RESPONSE
}

void testPostSslNormalEncrypted() {
    oaa::EncryptionPolicy policy;
    QCOMPARE(policy.shouldEncrypt(0, 0x0005, true), true);  // SERVICE_DISCOVERY
    QCOMPARE(policy.shouldEncrypt(0, 0x0007, true), true);  // CHANNEL_OPEN
    QCOMPARE(policy.shouldEncrypt(3, 0x8000, true), true);  // VIDEO SETUP
    QCOMPARE(policy.shouldEncrypt(1, 0x8001, true), true);  // INPUT EVENT
}
```

**Step 2: Implement EncryptionPolicy**

Simple logic:
- If `!sslActive` → always false
- If `sslActive` && channelId==0 && messageId in {0x0001..0x0004, 0x000b, 0x000c} → false
- Otherwise → true

**Step 3: Build, test, commit**

```bash
git commit -m "feat(oaa): implement table-driven EncryptionPolicy"
```

---

## Phase 5: Messenger

The Messenger integrates transport, framing, crypto, and encryption policy into a single send/receive pipeline.

### Task 5.1: Messenger implementation

**Files:**
- Create: `libs/open-androidauto/include/oaa/Messenger/Messenger.hpp`
- Create: `libs/open-androidauto/src/Messenger/Messenger.cpp`
- Create: `libs/open-androidauto/tests/test_messenger.cpp`

**Step 1: Write failing tests**

Use ReplayTransport to test Messenger without a real connection:

```cpp
void testSendPlainMessage() {
    oaa::ReplayTransport transport;
    oaa::Messenger messenger(&transport);
    transport.start();
    transport.simulateConnect();
    messenger.start();

    // Send a plain control message (VERSION_REQUEST)
    QByteArray payload("\x00\x01\x00\x01", 4); // major=1, minor=1
    messenger.sendMessage(0, 0x0001, payload);

    // Verify transport received a BULK frame
    auto written = transport.writtenData();
    QCOMPARE(written.size(), 1);
    // Header: ch=0, flags=BULK|CONTROL|PLAIN = 0x07
    QCOMPARE(uint8_t(written[0][0]), uint8_t(0x00));
    QCOMPARE(uint8_t(written[0][1]), uint8_t(0x07));
}

void testReceivePlainMessage() {
    oaa::ReplayTransport transport;
    oaa::Messenger messenger(&transport);
    QSignalSpy msgSpy(&messenger, &oaa::Messenger::messageReceived);
    transport.start();
    transport.simulateConnect();
    messenger.start();

    // Feed a BULK frame: ch=0, VERSION_RESPONSE, payload = major(2B) + minor(2B) + status(2B)
    QByteArray frame;
    frame.append('\x00');         // channel 0
    frame.append('\x07');         // BULK | CONTROL | PLAIN
    frame.append('\x00');         // size high
    frame.append('\x06');         // size low = 6
    frame.append('\x00').append('\x01'); // major = 1
    frame.append('\x00').append('\x01'); // minor = 1
    frame.append('\x00').append('\x00'); // status = MATCH
    transport.feedData(frame);

    QTRY_COMPARE(msgSpy.count(), 1);
    QCOMPARE(msgSpy[0][0].value<uint8_t>(), uint8_t(0));      // channelId
    // messageId is first 2 bytes of payload after frame parse
    // Actually — Messenger strips the 2-byte messageId from the payload
    QCOMPARE(msgSpy[0][1].value<uint16_t>(), uint16_t(0x0001)); // major as messageId
}
```

**Note:** The message format after frame reassembly is: `[messageId(2B BE)] [protobuf payload]`. The Messenger parses the 2-byte message ID and delivers `(channelId, messageId, remainingPayload)` via signal. For version exchange, the "payload" is raw binary (4 bytes: major + minor), not protobuf.

**Step 2: Implement Messenger**

```cpp
// libs/open-androidauto/include/oaa/Messenger/Messenger.hpp
#pragma once
#include <QObject>
#include <QQueue>
#include <oaa/Transport/ITransport.hpp>
#include <oaa/Messenger/Cryptor.hpp>
#include <oaa/Messenger/FrameParser.hpp>
#include <oaa/Messenger/FrameAssembler.hpp>
#include <oaa/Messenger/FrameSerializer.hpp>
#include <oaa/Messenger/EncryptionPolicy.hpp>

namespace oaa {

class Messenger : public QObject {
    Q_OBJECT
public:
    Messenger(ITransport* transport, QObject* parent = nullptr);

    void start();
    void stop();

    void sendMessage(uint8_t channelId, uint16_t messageId,
                     const QByteArray& payload);

    // Raw send (for SSL handshake data — no messageId prefix)
    void sendRaw(uint8_t channelId, uint16_t flags,
                 const QByteArray& data);

    void startHandshake();
    bool isEncrypted() const;
    Cryptor* cryptor();

signals:
    void messageReceived(uint8_t channelId, uint16_t messageId,
                         const QByteArray& payload);
    void handshakeComplete();
    void transportError(const QString& message);

private:
    ITransport* transport_;
    Cryptor cryptor_;
    FrameParser parser_;
    FrameAssembler assembler_;
    EncryptionPolicy encryptionPolicy_;

    struct SendItem {
        uint8_t channelId;
        MessageType messageType;
        EncryptionType encryptionType;
        QByteArray payload; // messageId + protobuf, pre-composed
    };
    QQueue<SendItem> sendQueue_;
    bool sending_ = false;

    void onFrameParsed(const FrameHeader& header, const QByteArray& payload);
    void onMessageAssembled(uint8_t channelId, MessageType messageType,
                            const QByteArray& payload);
    void processSendQueue();
};

} // namespace oaa
```

Key internal flow:

**Receive path:**
1. `ITransport::dataReceived` → `FrameParser::onData` → `FrameParser::frameParsed`
2. `frameParsed` → `Messenger::onFrameParsed` — decrypt if needed, forward to assembler
3. `FrameAssembler::messageAssembled` → `Messenger::onMessageAssembled`
4. Parse 2-byte BE messageId from payload head → emit `messageReceived(channelId, messageId, remainingPayload)`

**Send path:**
1. `sendMessage(channelId, messageId, payload)` — prepend 2-byte BE messageId, determine encryption via EncryptionPolicy, enqueue
2. `processSendQueue()` — dequeue, call `FrameSerializer::serialize()` to get frame list, encrypt payloads if needed via Cryptor, write each frame to transport

**SSL handshake integration:**
- `startHandshake()` — init Cryptor, call `doHandshake()`, write handshake bytes as channel 0, messageId 0x0003, PLAIN
- When receiving messageId 0x0003 on channel 0: feed to Cryptor's writeHandshakeBuffer, drive handshake, emit `handshakeComplete` when done

**Step 3: Build, test, commit**

```bash
git commit -m "feat(oaa): implement Messenger with framing, encryption, and send queue"
```

---

## Phase 6: Proto Files

### Task 6.1: Port proto2 files from aasdk

**Files:**
- Copy: `libs/aasdk/aasdk_proto/*.proto` → `libs/open-androidauto/proto/`
- Modify: `libs/open-androidauto/CMakeLists.txt` (add protobuf_generate)

**Step 1: Copy all 108 proto files**

```bash
cp libs/aasdk/aasdk_proto/*.proto libs/open-androidauto/proto/
# Remove CMakeLists.txt if copied
rm -f libs/open-androidauto/proto/CMakeLists.txt
```

**Step 2: Update proto package names**

Change `package aasdk.proto.messages;` (or similar) to `package oaa.proto;` in all files for clean namespace separation. Use sed for bulk rename.

**Step 3: Add protobuf generation to CMakeLists.txt**

```cmake
# In libs/open-androidauto/CMakeLists.txt
find_package(Protobuf REQUIRED)

file(GLOB PROTO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

target_sources(open-androidauto PRIVATE ${PROTO_SRCS})
target_include_directories(open-androidauto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(open-androidauto PUBLIC protobuf::libprotobuf)
```

**Step 4: Build and verify proto generation**

```bash
cd libs/open-androidauto/build && cmake .. && make -j$(nproc)
```
Expected: All 108 .proto files compile, generated headers in build dir.

**Step 5: Commit**

```bash
git add libs/open-androidauto/proto/
git commit -m "feat(oaa): port 108 proto2 files from aasdk"
```

---

## Phase 7: Channel Interfaces and ControlChannel

### Task 7.1: IChannelHandler and IAVChannelHandler interfaces

**Files:**
- Create: `libs/open-androidauto/include/oaa/Channel/IChannelHandler.hpp`
- Create: `libs/open-androidauto/include/oaa/Channel/IAVChannelHandler.hpp`

**Step 1: Write interfaces**

```cpp
// libs/open-androidauto/include/oaa/Channel/IChannelHandler.hpp
#pragma once
#include <QObject>
#include <QByteArray>

namespace oaa {

class IChannelHandler : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual ~IChannelHandler() = default;

    virtual uint8_t channelId() const = 0;
    virtual void onChannelOpened() = 0;
    virtual void onChannelClosed() = 0;
    virtual void onMessage(uint16_t messageId, const QByteArray& payload) = 0;

signals:
    void sendRequested(uint8_t channelId, uint16_t messageId,
                       const QByteArray& payload);
    void unknownMessage(uint16_t messageId, const QByteArray& payload);
};

} // namespace oaa
```

```cpp
// libs/open-androidauto/include/oaa/Channel/IAVChannelHandler.hpp
#pragma once
#include <oaa/Channel/IChannelHandler.hpp>

namespace oaa {

class IAVChannelHandler : public IChannelHandler {
    Q_OBJECT
public:
    using IChannelHandler::IChannelHandler;
    virtual void onMediaData(const QByteArray& data, uint64_t timestamp) = 0;
    virtual bool canAcceptMedia() const = 0;
};

} // namespace oaa
```

No tests — pure interfaces. Tested through ControlChannel and session.

**Step 2: Commit**

```bash
git commit -m "feat(oaa): add IChannelHandler and IAVChannelHandler interfaces"
```

### Task 7.2: ControlChannel (built-in protocol handler)

**Files:**
- Create: `libs/open-androidauto/include/oaa/Channel/ControlChannel.hpp`
- Create: `libs/open-androidauto/src/Channel/ControlChannel.cpp`
- Create: `libs/open-androidauto/tests/test_control_channel.cpp`

**Step 1: Write failing tests**

```cpp
void testVersionRequest() {
    oaa::ControlChannel ctrl;
    QSignalSpy sendSpy(&ctrl, &oaa::IChannelHandler::sendRequested);

    ctrl.sendVersionRequest();

    QCOMPARE(sendSpy.count(), 1);
    auto args = sendSpy.first();
    QCOMPARE(args[0].value<uint8_t>(), uint8_t(0));          // channel 0
    QCOMPARE(args[1].value<uint16_t>(), uint16_t(0x0001));   // VERSION_REQUEST
    QByteArray payload = args[2].toByteArray();
    QCOMPARE(payload.size(), 4);
    // major=1 (BE), minor=7 (BE)
    QCOMPARE(uint8_t(payload[0]), uint8_t(0x00));
    QCOMPARE(uint8_t(payload[1]), uint8_t(0x01));
    QCOMPARE(uint8_t(payload[2]), uint8_t(0x00));
    QCOMPARE(uint8_t(payload[3]), uint8_t(0x07));
}

void testVersionResponseParsing() {
    oaa::ControlChannel ctrl;
    QSignalSpy versionSpy(&ctrl, &oaa::ControlChannel::versionReceived);

    // Feed VERSION_RESPONSE: major=1, minor=7, status=MATCH(0x0000)
    QByteArray payload("\x00\x01\x00\x07\x00\x00", 6);
    ctrl.onMessage(0x0002, payload);

    QCOMPARE(versionSpy.count(), 1);
    QCOMPARE(versionSpy[0][0].value<uint16_t>(), uint16_t(1)); // major
    QCOMPARE(versionSpy[0][1].value<uint16_t>(), uint16_t(7)); // minor
    QCOMPARE(versionSpy[0][2].toBool(), true);                  // match
}

void testPingResponse() {
    oaa::ControlChannel ctrl;
    QSignalSpy sendSpy(&ctrl, &oaa::IChannelHandler::sendRequested);

    // Simulate receiving PING_REQUEST
    // Protobuf: field 1 (int64) = varint-encoded timestamp
    QByteArray pingPayload; // construct protobuf PingRequest
    ctrl.onMessage(0x000b, pingPayload);

    // Should emit PING_RESPONSE (0x000c) with same timestamp
    QCOMPARE(sendSpy.count(), 1);
    QCOMPARE(sendSpy[0][1].value<uint16_t>(), uint16_t(0x000c));
}
```

**Step 2: Implement ControlChannel**

Handles:
- `0x0001` VERSION_REQUEST — `sendVersionRequest()` sends raw binary (not protobuf)
- `0x0002` VERSION_RESPONSE — parse raw binary, emit `versionReceived(major, minor, match)`
- `0x0003` SSL_HANDSHAKE — forward to session (via signal) for Cryptor integration
- `0x0004` AUTH_COMPLETE — emit `authComplete(status)`
- `0x0005` SERVICE_DISCOVERY_REQUEST — emit `serviceDiscoveryRequested(protobuf)`
- `0x0006` SERVICE_DISCOVERY_RESPONSE — used when we implement phone-side (skip for now)
- `0x0007` CHANNEL_OPEN_REQUEST — emit `channelOpenRequested(channelId, payload)`
- `0x0008` CHANNEL_OPEN_RESPONSE — emit signal
- `0x000b` PING_REQUEST — auto-respond with PING_RESPONSE (0x000c), same timestamp
- `0x000c` PING_RESPONSE — emit `pongReceived(timestamp)`
- `0x000f` SHUTDOWN_REQUEST — emit `shutdownRequested(reason)`
- `0x0010` SHUTDOWN_RESPONSE — emit `shutdownAcknowledged()`
- `0x0012` AUDIO_FOCUS_REQUEST — emit signal for application handling
- Default — emit `unknownMessage(messageId, payload)`

**Step 3: Build, test, commit**

```bash
git commit -m "feat(oaa): implement ControlChannel with version, ping, and channel open"
```

---

## Phase 8: AASession (State Machine)

### Task 8.1: SessionConfig and SessionState

**Files:**
- Create: `libs/open-androidauto/include/oaa/Session/SessionState.hpp`
- Create: `libs/open-androidauto/include/oaa/Session/SessionConfig.hpp`

```cpp
// libs/open-androidauto/include/oaa/Session/SessionState.hpp
#pragma once

namespace oaa {

enum class SessionState {
    Idle,
    Connecting,
    VersionExchange,
    TLSHandshake,
    ServiceDiscovery,
    Active,
    ShuttingDown,
    Disconnected
};

enum class DisconnectReason {
    Normal,           // clean shutdown
    PingTimeout,      // missed pings
    TransportError,   // TCP error
    VersionMismatch,  // phone rejected version
    Timeout,          // state timeout
    UserRequested     // app called stop()
};

} // namespace oaa
```

```cpp
// libs/open-androidauto/include/oaa/Session/SessionConfig.hpp
#pragma once
#include <QString>
#include <cstdint>

namespace oaa {

struct SessionConfig {
    uint16_t protocolMajor = 1;
    uint16_t protocolMinor = 7;

    QString headUnitName = "OpenAuto Prodigy";
    QString carModel = "Universal";
    QString carYear = "2025";
    QString carSerial = "0000000000000001";
    bool leftHandDrive = true;
    QString manufacturer = "OpenAuto";
    QString model = "Prodigy";
    QString swBuild = "dev";
    QString swVersion = "0.4.0";
    bool canPlayNativeMediaDuringVr = true;

    // Timeouts (ms)
    int versionTimeout = 5000;
    int handshakeTimeout = 10000;
    int discoveryTimeout = 10000;
    int pingInterval = 5000;     // overridden by phone's connection_configuration
    int pingTimeout = 15000;     // 3 missed pings
};

} // namespace oaa
```

No tests needed — data-only structs. Commit together.

### Task 8.2: AASession state machine

**Files:**
- Create: `libs/open-androidauto/include/oaa/Session/AASession.hpp`
- Create: `libs/open-androidauto/src/Session/AASession.cpp`
- Create: `libs/open-androidauto/tests/test_session_fsm.cpp`

**Step 1: Write failing tests**

Test the state machine transitions using ReplayTransport:

```cpp
void testLifecycleToVersionExchange() {
    oaa::ReplayTransport transport;
    oaa::SessionConfig config;
    oaa::AASession session(&transport, config);

    QCOMPARE(session.state(), oaa::SessionState::Idle);

    transport.simulateConnect();
    session.start();

    // After start + connect, should send VERSION_REQUEST and be in VersionExchange
    QCOMPARE(session.state(), oaa::SessionState::VersionExchange);
    QVERIFY(!transport.writtenData().isEmpty()); // VERSION_REQUEST sent
}

void testVersionResponseAdvancesToHandshake() {
    // Setup session in VersionExchange state...
    // Feed VERSION_RESPONSE (match) via transport
    // Verify state advances to TLSHandshake
}

void testPingTimeout() {
    // Setup session in Active state with short ping timeout
    // Don't send any pings
    // Verify session transitions to Disconnected after timeout
}

void testChannelOpenRegistered() {
    // Register a handler for channel 3
    // Feed CHANNEL_OPEN_REQUEST for channel 3
    // Verify CHANNEL_OPEN_RESPONSE with status=OK sent
    // Verify handler's onChannelOpened() called
}

void testChannelOpenUnregistered() {
    // Don't register channel 9
    // Feed CHANNEL_OPEN_REQUEST for channel 9
    // Verify CHANNEL_OPEN_RESPONSE with status=FAIL sent
    // Verify channelOpenRejected signal emitted
}

void testGracefulShutdown() {
    // Session in Active state
    // Call session.stop()
    // Verify SHUTDOWN_REQUEST sent
    // Verify state = ShuttingDown
    // Feed SHUTDOWN_RESPONSE
    // Verify state = Disconnected
}
```

**Step 2: Implement AASession**

```cpp
// libs/open-androidauto/include/oaa/Session/AASession.hpp
#pragma once
#include <QObject>
#include <QTimer>
#include <QHash>
#include <oaa/Transport/ITransport.hpp>
#include <oaa/Messenger/Messenger.hpp>
#include <oaa/Channel/IChannelHandler.hpp>
#include <oaa/Channel/ControlChannel.hpp>
#include <oaa/Session/SessionState.hpp>
#include <oaa/Session/SessionConfig.hpp>

namespace oaa {

class AASession : public QObject {
    Q_OBJECT
public:
    AASession(ITransport* transport, const SessionConfig& config,
              QObject* parent = nullptr);
    ~AASession() override;

    void start();
    void stop();

    void registerChannel(uint8_t channelId, IChannelHandler* handler);
    SessionState state() const;
    Messenger* messenger() const;

signals:
    void stateChanged(oaa::SessionState newState);
    void channelOpened(uint8_t channelId);
    void channelOpenRejected(uint8_t channelId);
    void disconnected(oaa::DisconnectReason reason);

private:
    void setState(SessionState newState);

    // State handlers
    void onTransportConnected();
    void onVersionReceived(uint16_t major, uint16_t minor, bool match);
    void onHandshakeComplete();
    void onServiceDiscoveryRequested(const QByteArray& payload);
    void onChannelOpenRequested(uint8_t channelId, const QByteArray& payload);
    void onMessage(uint8_t channelId, uint16_t messageId,
                   const QByteArray& payload);
    void onPingTick();
    void onPongReceived(int64_t timestamp);
    void onShutdownRequested(int reason);
    void onStateTimeout();

    SessionConfig config_;
    Messenger* messenger_;
    ControlChannel* controlChannel_;
    QHash<uint8_t, IChannelHandler*> channels_;
    SessionState state_ = SessionState::Idle;

    QTimer stateTimer_;    // per-state timeout
    QTimer pingTimer_;     // periodic ping
    int missedPings_ = 0;
};

} // namespace oaa
```

Key state transitions:
- `Idle` → `start()` called → wait for transport `connected` signal
- `Connecting` → transport connected → send VERSION_REQUEST → `VersionExchange`
- `VersionExchange` → VERSION_RESPONSE received → `startHandshake()` → `TLSHandshake`
- `TLSHandshake` → handshake complete → send AUTH_COMPLETE → `ServiceDiscovery`
- `ServiceDiscovery` → SERVICE_DISCOVERY_REQUEST received → build and send response → `Active`
- `Active` → pings flow, channels open/close, media streams
- `ShuttingDown` → SHUTDOWN_RESPONSE received or timeout → `Disconnected`

**Step 3: Build, test, commit**

```bash
git commit -m "feat(oaa): implement AASession FSM with channel registry and ping"
```

### Task 8.3: Service discovery response builder

**Files:**
- Modify: `libs/open-androidauto/src/Session/AASession.cpp` (add SD response building)
- Create: `libs/open-androidauto/tests/test_service_discovery.cpp`

Test that `onServiceDiscoveryRequested` builds a proper `ServiceDiscoveryResponse` protobuf from `SessionConfig` + registered channel handlers. Verify all required fields are populated, channel descriptors match registered handlers.

This uses the proto files from Phase 6 — the generated ServiceDiscoveryResponse message.

```bash
git commit -m "feat(oaa): implement service discovery response builder"
```

---

## Phase Summary

| Phase | Tasks | What it delivers |
|-------|-------|-----------------|
| 1 | 1.1 | Scaffolding, CMake, Version constants |
| 2 | 2.1-2.3 | ITransport, TCPTransport, ReplayTransport |
| 3 | 3.1-3.4 | FrameHeader, FrameSerializer, FrameAssembler, FrameParser |
| 4 | 4.1-4.2 | Cryptor (TLS 1.2), EncryptionPolicy |
| 5 | 5.1 | Messenger (integrates layers 1-4) |
| 6 | 6.1 | 108 proto2 files ported |
| 7 | 7.1-7.2 | IChannelHandler, IAVChannelHandler, ControlChannel |
| 8 | 8.1-8.3 | AASession FSM, channel registry, ping, service discovery |

**Total:** ~17 tasks, ~20 source files, ~12 test files

After Phase 8, the library is functionally complete for wireless AA. The application (Prodigy) can then implement concrete channel handlers (Video, Audio, Input, Sensor) against `IChannelHandler`/`IAVChannelHandler` and swap `AndroidAutoService` to use `oaa::AASession`.

**Not included in this plan** (future work):
- USBTransport (AOAP) — Phase 9 when USB support is needed
- Concrete channel handler migration in Prodigy — separate plan
- aasdk removal — after migration is tested on Pi

---

## Build Integration Note

During development, open-androidauto builds standalone in `libs/open-androidauto/build/`. When ready for integration, add to the main project's `CMakeLists.txt`:

```cmake
add_subdirectory(libs/open-androidauto)
# Then in target_link_libraries:
target_link_libraries(openauto-prodigy PRIVATE open-androidauto)
```

This replaces the current `aasdk` and `aasdk_proto` link targets. Boost dependency is dropped entirely.

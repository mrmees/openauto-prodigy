# Phase 4: Cryptor and Encryption Policy

**Context:** Building the encryption layer for `libs/open-androidauto/`. Phases 1-3 complete — library has transport layer and framing layer (FrameHeader, FrameSerializer, FrameAssembler, FrameParser).

**Design doc:** `docs/plans/2026-02-23-open-androidauto-design.md` (see "Cryptor" section)
**Protocol reference:** `~/claude/reference/android-auto-protocol.md` (see "Encryption Layer" section)

## Encryption Summary

AA uses application-layer TLS 1.2 over OpenSSL memory BIOs (NOT socket-level TLS). TLS records are carried as AA frame payloads on channel 0 with messageId 0x0003.

- SSL method: `TLS_client_method()` (HU is TLS client)
- Certificate: Hardcoded X.509 + RSA 2048-bit key (from JVC Kenwood, carried forward from aasdk)
- Verification: `SSL_VERIFY_NONE`
- Memory BIO buffer limit: 20,480 bytes
- TLS overhead for decrypt size estimation: 29 bytes
- Decrypt reads in 2048-byte chunks

Pre-SSL messages (always PLAIN): VERSION_REQUEST (0x0001), VERSION_RESPONSE (0x0002), SSL_HANDSHAKE (0x0003), AUTH_COMPLETE (0x0004)
Post-SSL exceptions (still PLAIN): PING_REQUEST (0x000b), PING_RESPONSE (0x000c)
Everything else post-SSL: ENCRYPTED

---

- [x] **Implement Cryptor (OpenSSL memory BIO TLS 1.2) with tests.** *(Done — 5 tests: handshake, encrypt/decrypt, 50KB payload, multi-message, deinit safety. 8/8 oaa tests pass.)* Write `include/oaa/Messenger/Cryptor.hpp` and `src/Messenger/Cryptor.cpp`. The Cryptor class (QObject, namespace oaa) manages application-layer TLS 1.2. It has an enum `Role { Client, Server }`. Public methods: `init(Role)` — creates SSL_CTX with TLS_client_method() (or TLS_server_method() for Role::Server used in tests), loads PEM cert+key from embedded strings, creates SSL instance, creates memory BIO pair, sets BIO buffer limit to 20480, calls SSL_set_connect_state (Client) or SSL_set_accept_state (Server), sets SSL_VERIFY_NONE. `deinit()` — frees SSL, CTX, cert, key. `doHandshake()` → bool — calls SSL_do_handshake(), returns true when SSL_ERROR_NONE (handshake complete), false on SSL_ERROR_WANT_READ (need more data). `readHandshakeBuffer()` → QByteArray — reads pending bytes from write BIO (outgoing TLS data). `writeHandshakeBuffer(const QByteArray&)` — writes incoming TLS data to read BIO. `encrypt(const QByteArray& plaintext)` → QByteArray — SSL_write plaintext, then BIO_read from write BIO to get ciphertext. `decrypt(const QByteArray& ciphertext, int frameLength)` → QByteArray — BIO_write ciphertext to read BIO, then SSL_read with estimated size (frameLength - 29) in 2048-byte chunks. `isActive()` → bool. The certificate and private key are the exact PEM strings from `libs/aasdk/src/Messenger/Cryptor.cpp` lines 281-327 (the JVC Kenwood / Google Automotive cert). Copy them verbatim as static const std::string members. Write `tests/test_cryptor.cpp`: (1) `testHandshakeBetweenPeers` — create Client and Server Cryptor, drive handshake loop: client.doHandshake(), read client output → feed to server.writeHandshakeBuffer(), server.doHandshake(), read server output → feed to client.writeHandshakeBuffer(), repeat until both isActive(). (2) `testEncryptDecrypt` — after handshake, client.encrypt("Hello AA") → ciphertext, verify ciphertext != plaintext, server.decrypt(ciphertext, ciphertext.size()) → verify matches "Hello AA". (3) `testLargePayload` — encrypt/decrypt a 50000-byte payload. (4) `testMultipleMessages` — encrypt/decrypt 3 sequential messages, verify order preserved. Add OpenSSL::SSL and OpenSSL::Crypto to test link if not already inherited. Build and run. Commit: `feat(oaa): implement Cryptor with OpenSSL memory BIO TLS 1.2`

- [ ] **Implement EncryptionPolicy (table-driven) with tests.** Write `include/oaa/Messenger/EncryptionPolicy.hpp` and `src/Messenger/EncryptionPolicy.cpp`. Simple class with one method: `bool shouldEncrypt(uint8_t channelId, uint16_t messageId, bool sslActive) const`. Logic: if !sslActive → return false. If sslActive AND channelId==0 AND messageId is one of {0x0001, 0x0002, 0x0003, 0x0004, 0x000b, 0x000c} → return false. Otherwise → return true. Write `tests/test_encryption_policy.cpp`: (1) `testPreSslAlwaysPlain` — verify shouldEncrypt returns false for various channel/message combos when sslActive=false. (2) `testPostSslControlExceptions` — verify false for VERSION_REQUEST(0x0001), VERSION_RESPONSE(0x0002), SSL_HANDSHAKE(0x0003), AUTH_COMPLETE(0x0004), PING_REQUEST(0x000b), PING_RESPONSE(0x000c) on channel 0 when sslActive=true. (3) `testPostSslNormalEncrypted` — verify true for SERVICE_DISCOVERY(0x0005), CHANNEL_OPEN(0x0007), VIDEO_SETUP(0x8000 on ch3), INPUT_EVENT(0x8001 on ch1) when sslActive=true. (4) `testNonControlChannelAlwaysEncryptedPostSsl` — verify true for any messageId on channels 1-8 when sslActive=true. Build and run. Commit: `feat(oaa): implement table-driven EncryptionPolicy`

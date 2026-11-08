/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static java.util.Collections.singletonList;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;

import android.util.Base64;
import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.security.GeneralSecurityException;
import java.security.KeyFactory;
import java.security.KeyStore;
import java.security.PrivateKey;
import java.security.cert.Certificate;
import java.security.cert.CertificateFactory;
import java.security.spec.PKCS8EncodedKeySpec;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;
import javax.net.ssl.KeyManagerFactory;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSocket;
import org.junit.Before;
import org.junit.Test;
import org.webrtc.PeerConnection.TlsCertPolicy;

/** Unit tests for {@link PeerConnection}. */
public class PeerConnectionTest {
  private static final int TIMEOUT_SECONDS = 10;

  private static class EmptyObserver implements PeerConnection.Observer {
    @Override
    public void onSignalingChange(PeerConnection.SignalingState newState) {}

    @Override
    public void onIceConnectionChange(PeerConnection.IceConnectionState newState) {}

    @Override
    public void onIceConnectionReceivingChange(boolean receiving) {}

    @Override
    public void onIceGatheringChange(PeerConnection.IceGatheringState newState) {}

    @Override
    public void onIceCandidate(IceCandidate candidate) {}

    @Override
    public void onIceCandidatesRemoved(IceCandidate[] candidates) {}

    @Override
    public void onAddStream(MediaStream stream) {}

    @Override
    public void onRemoveStream(MediaStream stream) {}

    @Override
    public void onDataChannel(DataChannel dataChannel) {}

    @Override
    public void onRenegotiationNeeded() {}
  }

  private static class FailingSdpObserver implements SdpObserver {
    @Override
    public void onCreateSuccess(SessionDescription sdp) {}

    @Override
    public void onSetSuccess() {}

    @Override
    public void onCreateFailure(String error) {
      throw new AssertionError("Unexpected onCreateFailure: " + error);
    }

    @Override
    public void onSetFailure(String error) {
      throw new AssertionError("Unexpected onSetFailure: " + error);
    }
  }

  @Before
  public void setUp() {
    PeerConnectionFactory.initialize(PeerConnectionFactory.InitializationOptions
                                         .builder(InstrumentationRegistry.getTargetContext())
                                         .setNativeLibraryName(TestConstants.NATIVE_LIBRARY)
                                         .createInitializationOptions());
  }

  @Test
  @SmallTest
  public void testIceServerChanged() throws Exception {
    PeerConnection.IceServer iceServer1 =
        PeerConnection.IceServer.builder("turn:fake.example.com")
            .setUsername("fakeUsername")
            .setPassword("fakePassword")
            .setTlsCertPolicy(TlsCertPolicy.TLS_CERT_POLICY_SECURE)
            .setHostname("fakeHostname")
            .setTlsAlpnProtocols(singletonList("fakeTlsAlpnProtocol"))
            .setTlsEllipticCurves(singletonList("fakeTlsEllipticCurve"))
            .createIceServer();
    // Same as iceServer1.
    PeerConnection.IceServer iceServer2 =
        PeerConnection.IceServer.builder("turn:fake.example.com")
            .setUsername("fakeUsername")
            .setPassword("fakePassword")
            .setTlsCertPolicy(TlsCertPolicy.TLS_CERT_POLICY_SECURE)
            .setHostname("fakeHostname")
            .setTlsAlpnProtocols(singletonList("fakeTlsAlpnProtocol"))
            .setTlsEllipticCurves(singletonList("fakeTlsEllipticCurve"))
            .createIceServer();
    // Differs from iceServer1 by the url.
    PeerConnection.IceServer iceServer3 =
        PeerConnection.IceServer.builder("turn:fake.example2.com")
            .setUsername("fakeUsername")
            .setPassword("fakePassword")
            .setTlsCertPolicy(TlsCertPolicy.TLS_CERT_POLICY_SECURE)
            .setHostname("fakeHostname")
            .setTlsAlpnProtocols(singletonList("fakeTlsAlpnProtocol"))
            .setTlsEllipticCurves(singletonList("fakeTlsEllipticCurve"))
            .createIceServer();
    // Differs from iceServer1 by the username.
    PeerConnection.IceServer iceServer4 =
        PeerConnection.IceServer.builder("turn:fake.example.com")
            .setUsername("fakeUsername2")
            .setPassword("fakePassword")
            .setTlsCertPolicy(TlsCertPolicy.TLS_CERT_POLICY_SECURE)
            .setHostname("fakeHostname")
            .setTlsAlpnProtocols(singletonList("fakeTlsAlpnProtocol"))
            .setTlsEllipticCurves(singletonList("fakeTlsEllipticCurve"))
            .createIceServer();
    // Differs from iceServer1 by the password.
    PeerConnection.IceServer iceServer5 =
        PeerConnection.IceServer.builder("turn:fake.example.com")
            .setUsername("fakeUsername")
            .setPassword("fakePassword2")
            .setTlsCertPolicy(TlsCertPolicy.TLS_CERT_POLICY_SECURE)
            .setHostname("fakeHostname")
            .setTlsAlpnProtocols(singletonList("fakeTlsAlpnProtocol"))
            .setTlsEllipticCurves(singletonList("fakeTlsEllipticCurve"))
            .createIceServer();
    // Differs from iceServer1 by the TLS certificate policy.
    PeerConnection.IceServer iceServer6 =
        PeerConnection.IceServer.builder("turn:fake.example.com")
            .setUsername("fakeUsername")
            .setPassword("fakePassword")
            .setTlsCertPolicy(TlsCertPolicy.TLS_CERT_POLICY_INSECURE_NO_CHECK)
            .setHostname("fakeHostname")
            .setTlsAlpnProtocols(singletonList("fakeTlsAlpnProtocol"))
            .setTlsEllipticCurves(singletonList("fakeTlsEllipticCurve"))
            .createIceServer();
    // Differs from iceServer1 by the hostname.
    PeerConnection.IceServer iceServer7 =
        PeerConnection.IceServer.builder("turn:fake.example.com")
            .setUsername("fakeUsername")
            .setPassword("fakePassword")
            .setTlsCertPolicy(TlsCertPolicy.TLS_CERT_POLICY_INSECURE_NO_CHECK)
            .setHostname("fakeHostname2")
            .setTlsAlpnProtocols(singletonList("fakeTlsAlpnProtocol"))
            .setTlsEllipticCurves(singletonList("fakeTlsEllipticCurve"))
            .createIceServer();
    // Differs from iceServer1 by the TLS ALPN.
    PeerConnection.IceServer iceServer8 =
        PeerConnection.IceServer.builder("turn:fake.example.com")
            .setUsername("fakeUsername")
            .setPassword("fakePassword")
            .setTlsCertPolicy(TlsCertPolicy.TLS_CERT_POLICY_INSECURE_NO_CHECK)
            .setHostname("fakeHostname")
            .setTlsAlpnProtocols(singletonList("fakeTlsAlpnProtocol2"))
            .setTlsEllipticCurves(singletonList("fakeTlsEllipticCurve"))
            .createIceServer();
    // Differs from iceServer1 by the TLS elliptic curve.
    PeerConnection.IceServer iceServer9 =
        PeerConnection.IceServer.builder("turn:fake.example.com")
            .setUsername("fakeUsername")
            .setPassword("fakePassword")
            .setTlsCertPolicy(TlsCertPolicy.TLS_CERT_POLICY_INSECURE_NO_CHECK)
            .setHostname("fakeHostname")
            .setTlsAlpnProtocols(singletonList("fakeTlsAlpnProtocol"))
            .setTlsEllipticCurves(singletonList("fakeTlsEllipticCurve2"))
            .createIceServer();

    assertTrue(iceServer1.equals(iceServer2));
    assertFalse(iceServer1.equals(iceServer3));
    assertFalse(iceServer1.equals(iceServer4));
    assertFalse(iceServer1.equals(iceServer5));
    assertFalse(iceServer1.equals(iceServer6));
    assertFalse(iceServer1.equals(iceServer7));
    assertFalse(iceServer1.equals(iceServer8));
    assertFalse(iceServer1.equals(iceServer9));
  }

  // TODO(fischman) MOAR test ideas:
  // - Test that PC.removeStream() works; requires a second
  //   createOffer/createAnswer dance.
  // - audit each place that uses `constraints` for specifying non-trivial
  //   constraints (and ensure they're honored).
  // - test error cases
  // - ensure reasonable coverage of jni code is achieved.  Coverage is
  //   extra-important because of all the free-text (class/method names, etc)
  //   in JNI-style programming; make sure no typos!
  // - Test that shutdown mid-interaction is crash-free.

  // Tests that the JNI glue between Java and C++ does not crash when creating a PeerConnection.
  @Test
  @SmallTest
  public void testCreationWithConfig() throws Exception {
    PeerConnectionFactory factory = PeerConnectionFactory.builder().createPeerConnectionFactory();
    List<PeerConnection.IceServer> iceServers = Arrays.asList(
        PeerConnection.IceServer.builder("stun:stun.l.google.com:19302").createIceServer(),
        PeerConnection.IceServer.builder("turn:fake.example.com")
            .setUsername("fakeUsername")
            .setPassword("fakePassword")
            .createIceServer());
    PeerConnection.RTCConfiguration config = new PeerConnection.RTCConfiguration(iceServers);
    config.sdpSemantics = PeerConnection.SdpSemantics.UNIFIED_PLAN;

    // Test configuration options.
    config.continualGatheringPolicy = PeerConnection.ContinualGatheringPolicy.GATHER_CONTINUALLY;

    PeerConnection offeringPC =
        factory.createPeerConnection(config, mock(PeerConnection.Observer.class));
    assertNotNull(offeringPC);
  }

  @Test
  @SmallTest
  public void testCreationWithCertificate() throws Exception {
    PeerConnectionFactory factory = PeerConnectionFactory.builder().createPeerConnectionFactory();
    PeerConnection.RTCConfiguration config = new PeerConnection.RTCConfiguration(Arrays.asList());
    config.sdpSemantics = PeerConnection.SdpSemantics.UNIFIED_PLAN;

    // Test certificate.
    RtcCertificatePem originalCert = RtcCertificatePem.generateCertificate();
    config.certificate = originalCert;

    PeerConnection offeringPC =
        factory.createPeerConnection(config, mock(PeerConnection.Observer.class));

    RtcCertificatePem restoredCert = offeringPC.getCertificate();
    assertEquals(originalCert.privateKey, restoredCert.privateKey);
    assertEquals(originalCert.certificate, restoredCert.certificate);
  }

  @Test
  @SmallTest
  public void testCreationWithCryptoOptions() throws Exception {
    PeerConnectionFactory factory = PeerConnectionFactory.builder().createPeerConnectionFactory();
    PeerConnection.RTCConfiguration config = new PeerConnection.RTCConfiguration(Arrays.asList());
    config.sdpSemantics = PeerConnection.SdpSemantics.UNIFIED_PLAN;

    assertNull(config.cryptoOptions);

    CryptoOptions cryptoOptions = CryptoOptions.builder()
                                      .setEnableGcmCryptoSuites(true)
                                      .setEnableAes128Sha1_32CryptoCipher(true)
                                      .setEnableEncryptedRtpHeaderExtensions(true)
                                      .setRequireFrameEncryption(true)
                                      .createCryptoOptions();
    config.cryptoOptions = cryptoOptions;

    PeerConnection offeringPC =
        factory.createPeerConnection(config, mock(PeerConnection.Observer.class));
    assertNotNull(offeringPC);
  }

  @Test
  @SmallTest
  public void testDisposeOnFactoryCreationThreadWaitsForTeardown() throws Exception {
    // Creating the factory wraps this thread as a webrtc::Thread, but it is still an application
    // thread on which dispose() waits for the teardown.
    PeerConnectionFactory factory = PeerConnectionFactory.builder().createPeerConnectionFactory();
    PeerConnection.RTCConfiguration config =
        new PeerConnection.RTCConfiguration(Collections.emptyList());
    CountDownLatch closed = new CountDownLatch(1);
    PeerConnection pc =
        factory.createPeerConnection(
            config,
            new EmptyObserver() {
              @Override
              public void onIceConnectionChange(PeerConnection.IceConnectionState newState) {
                if (newState == PeerConnection.IceConnectionState.CLOSED) {
                  closed.countDown();
                }
              }
            });
    assertNotNull(pc);
    pc.dispose();
    // The teardown closes the PeerConnection before dispose() returns.
    assertEquals(0, closed.getCount());
    factory.dispose();
  }

  @Test
  @SmallTest
  public void testDisposeInSdpObserverCallback() throws Exception {
    PeerConnectionFactory factory = PeerConnectionFactory.builder().createPeerConnectionFactory();
    PeerConnection.RTCConfiguration config =
        new PeerConnection.RTCConfiguration(Collections.emptyList());
    PeerConnection pc = factory.createPeerConnection(config, new EmptyObserver());
    assertNotNull(pc);
    pc.addTransceiver(MediaStreamTrack.MediaType.MEDIA_TYPE_AUDIO);
    // Populate cached Java wrappers so that their disposal is exercised too.
    assertFalse(pc.getSenders().isEmpty());
    assertFalse(pc.getReceivers().isEmpty());
    assertFalse(pc.getTransceivers().isEmpty());

    CountDownLatch disposed = new CountDownLatch(1);
    pc.createOffer(
        new FailingSdpObserver() {
          @Override
          public void onCreateSuccess(SessionDescription sdp) {
            pc.setLocalDescription(
                new FailingSdpObserver() {
                  @Override
                  public void onSetSuccess() {
                    pc.dispose();
                    factory.dispose();
                    disposed.countDown();
                  }
                },
                sdp);
          }
        },
        new MediaConstraints());
    assertTrue(disposed.await(TIMEOUT_SECONDS, TimeUnit.SECONDS));
    // Waits for the teardown that the callback started.
    factory.dispose();
  }

  @Test
  @SmallTest
  public void testDisposeInStatsCallback() throws Exception {
    PeerConnectionFactory factory = PeerConnectionFactory.builder().createPeerConnectionFactory();
    PeerConnection.RTCConfiguration config =
        new PeerConnection.RTCConfiguration(Collections.emptyList());
    PeerConnection pc = factory.createPeerConnection(config, new EmptyObserver());
    assertNotNull(pc);
    pc.addTransceiver(MediaStreamTrack.MediaType.MEDIA_TYPE_AUDIO);
    assertFalse(pc.getReceivers().isEmpty());

    CountDownLatch disposed = new CountDownLatch(1);
    pc.getStats(
        report -> {
          assertNotNull(report);
          pc.dispose();
          factory.dispose();
          disposed.countDown();
        });
    assertTrue(disposed.await(TIMEOUT_SECONDS, TimeUnit.SECONDS));
    // Waits for the teardown that the callback started.
    factory.dispose();
  }

  @Test
  @SmallTest
  public void testDisposeInObserverCallback() throws Exception {
    PeerConnectionFactory factory = PeerConnectionFactory.builder().createPeerConnectionFactory();
    PeerConnection.RTCConfiguration config =
        new PeerConnection.RTCConfiguration(Collections.emptyList());
    AtomicReference<PeerConnection> pcRef = new AtomicReference<>();
    CountDownLatch disposed = new CountDownLatch(1);
    PeerConnection pc =
        factory.createPeerConnection(
            config,
            new EmptyObserver() {
              @Override
              public void onSignalingChange(PeerConnection.SignalingState newState) {
                if (newState == PeerConnection.SignalingState.HAVE_LOCAL_OFFER) {
                  pcRef.get().dispose();
                  disposed.countDown();
                }
              }
            });
    assertNotNull(pc);
    pcRef.set(pc);
    pc.addTransceiver(MediaStreamTrack.MediaType.MEDIA_TYPE_AUDIO);

    AtomicReference<SessionDescription> offer = new AtomicReference<>();
    CountDownLatch offerCreated = new CountDownLatch(1);
    pc.createOffer(
        new FailingSdpObserver() {
          @Override
          public void onCreateSuccess(SessionDescription sdp) {
            offer.set(sdp);
            offerCreated.countDown();
          }
        },
        new MediaConstraints());
    assertTrue(offerCreated.await(TIMEOUT_SECONDS, TimeUnit.SECONDS));

    // onSignalingChange() runs on the signaling thread while this thread is still blocked in
    // setLocalDescription().
    pc.setLocalDescription(new FailingSdpObserver(), offer.get());
    assertTrue(disposed.await(TIMEOUT_SECONDS, TimeUnit.SECONDS));
    // Runs after the PeerConnection teardown, which may still be pending.
    factory.dispose();
  }

  @Test
  @SmallTest
  public void testDisposeInCertificateVerifierCallback() throws Exception {
    // The SSLCertificateVerifier only verifies TURN server certificates, so a local TLS server acts
    // as the TURN server. The verifier runs on the network thread during the TLS handshake.
    try (ServerSocket tlsServer = createLoopbackTlsServer()) {
      // Allow loopback interfaces, which are needed to reach the TLS server.
      PeerConnectionFactory.Options options = new PeerConnectionFactory.Options();
      options.networkIgnoreMask = 0;
      PeerConnectionFactory factory =
          PeerConnectionFactory.builder().setOptions(options).createPeerConnectionFactory();
      String turnsUrl = "turns:127.0.0.1:" + tlsServer.getLocalPort() + "?transport=tcp";
      PeerConnection.IceServer turnServer =
          PeerConnection.IceServer.builder(turnsUrl)
              .setUsername("username")
              .setPassword("password")
              .createIceServer();
      PeerConnection.RTCConfiguration config =
          new PeerConnection.RTCConfiguration(singletonList(turnServer));

      AtomicReference<PeerConnection> pcRef = new AtomicReference<>();
      CountDownLatch disposed = new CountDownLatch(1);
      PeerConnectionDependencies deps =
          PeerConnectionDependencies.builder(new EmptyObserver())
              .setSSLCertificateVerifier(
                  certificate -> {
                    pcRef.get().dispose();
                    factory.dispose();
                    disposed.countDown();
                    return false;
                  })
              .createPeerConnectionDependencies();
      PeerConnection pc = factory.createPeerConnection(config, deps);
      assertNotNull(pc);
      pcRef.set(pc);
      pc.addTransceiver(MediaStreamTrack.MediaType.MEDIA_TYPE_AUDIO);
      assertFalse(pc.getReceivers().isEmpty());

      // Setting the local description starts ICE gathering, which connects to the TLS server.
      pc.setLocalDescription(new FailingSdpObserver());
      try (SSLSocket connection = (SSLSocket) tlsServer.accept()) {
        connection.setSoTimeout(TIMEOUT_SECONDS * 1000);
        connection.startHandshake();
      } catch (IOException e) {
        // Expected, since the verifier rejects the certificate.
      }
      assertTrue(disposed.await(TIMEOUT_SECONDS, TimeUnit.SECONDS));
      // Waits for the teardown that the callback started.
      factory.dispose();
    }
  }

  /** Creates a TLS server socket on 127.0.0.1 that uses a new self-signed certificate. */
  private static ServerSocket createLoopbackTlsServer()
      throws GeneralSecurityException, IOException {
    RtcCertificatePem pem = RtcCertificatePem.generateCertificate();
    PrivateKey privateKey =
        KeyFactory.getInstance("EC")
            .generatePrivate(new PKCS8EncodedKeySpec(decodePem(pem.privateKey)));
    Certificate certificate =
        CertificateFactory.getInstance("X.509")
            .generateCertificate(new ByteArrayInputStream(decodePem(pem.certificate)));
    char[] password = "password".toCharArray();
    KeyStore keyStore = KeyStore.getInstance(KeyStore.getDefaultType());
    keyStore.load(null, null);
    keyStore.setKeyEntry("server", privateKey, password, new Certificate[] {certificate});
    KeyManagerFactory keyManagerFactory =
        KeyManagerFactory.getInstance(KeyManagerFactory.getDefaultAlgorithm());
    keyManagerFactory.init(keyStore, password);
    SSLContext sslContext = SSLContext.getInstance("TLS");
    sslContext.init(keyManagerFactory.getKeyManagers(), null, null);

    InetAddress loopback = InetAddress.getByAddress(new byte[] {127, 0, 0, 1});
    ServerSocket serverSocket =
        sslContext
            .getServerSocketFactory()
            .createServerSocket(/* port= */ 0, /* backlog= */ 1, loopback);
    serverSocket.setSoTimeout(TIMEOUT_SECONDS * 1000);
    return serverSocket;
  }

  /** Returns the DER encoding of a single PEM block. */
  private static byte[] decodePem(String pem) {
    return Base64.decode(pem.replaceAll("-----[^-]*-----", ""), Base64.DEFAULT);
  }
}

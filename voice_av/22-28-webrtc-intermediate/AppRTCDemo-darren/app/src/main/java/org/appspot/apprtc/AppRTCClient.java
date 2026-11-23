/*
 *  Copyright 2013 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.appspot.apprtc;

import org.webrtc.IceCandidate;
import org.webrtc.PeerConnection;
import org.webrtc.SessionDescription;

import java.util.List;

/**
 * AppRTCClient is the interface representing an AppRTC client.
 * 自定义的与服务端进行通讯和消息回调的接口
 */
public interface AppRTCClient {
  /**
   * Struct holding the connection parameters of an AppRTC room.
   */
  class RoomConnectionParameters {
    public final String roomUrl;
    public final String roomId;
    public final boolean loopback;
    public final String urlParameters;
    public RoomConnectionParameters(String roomUrl, String roomId, boolean loopback, String urlParameters) {
      this.roomUrl = roomUrl;
      this.roomId = roomId;
      this.loopback = loopback;
      this.urlParameters = urlParameters;
    }
    public RoomConnectionParameters(String roomUrl, String roomId, boolean loopback) {
      this(roomUrl, roomId, loopback, null /* urlParameters */);
    }
  }

  /**
   * Asynchronously connect to an AppRTC room URL using supplied connection
   * parameters. Once connection is established onConnectedToRoom()
   * callback with room parameters is invoked.
   */
  void connectToRoom(RoomConnectionParameters connectionParameters);

  /**
   * Send offer SDP to the other participant.
   */
  void sendOfferSdp(final SessionDescription sdp);

  /**
   * Send answer SDP to the other participant.
   */
  void sendAnswerSdp(final SessionDescription sdp);

  /**
   * Send Ice candidate to the other participant.
   */
  void sendLocalIceCandidate(final IceCandidate candidate);

  /**
   * Send removed ICE candidates to the other participant.
   */
  void sendLocalIceCandidateRemovals(final IceCandidate[] candidates);

  /**
   * Disconnect from room.
   */
  void disconnectFromRoom();

  /**
   * Struct holding the signaling parameters of an AppRTC room.
   */
  class SignalingParameters {
    public final List<PeerConnection.IceServer> iceServers;  //stun服务器地址信息，由pc_config得到
    public final boolean initiator; // 发起者
    public final String clientId;   // 客户端id，同一个房间可以有多个clientId
    public final String wssUrl;     // 比如wss:\/\/imtest.webrtc110.com:8089\/ws
    public final String wssPostUrl; // wss_post_url":"https:\/\/imtest.webrtc110.com:8089"
    public final SessionDescription offerSdp; // sdp信息
    public final List<IceCandidate> iceCandidates;  //ice候选，可以有多个ice？

    public SignalingParameters(List<PeerConnection.IceServer> iceServers,
                               boolean initiator, String clientId, String wssUrl, String wssPostUrl,
                               SessionDescription offerSdp, List<IceCandidate> iceCandidates) {
      this.iceServers = iceServers;
      this.initiator = initiator;
      this.clientId = clientId;
      this.wssUrl = wssUrl;
      this.wssPostUrl = wssPostUrl;
      this.offerSdp = offerSdp;
      this.iceCandidates = iceCandidates;
    }
  }

  /**
   * Callback interface for messages delivered on signaling channel.
   * 信号回调
   *
   * <p>Methods are guaranteed to be invoked on the UI thread of |activity|.
   */
  interface SignalingEvents {
    /**
     * Callback fired once the room's signaling parameters
     * SignalingParameters are extracted.
     */
    void onConnectedToRoom(final SignalingParameters params);

    /**
     * Callback fired once remote SDP is received.
     */
    void onRemoteDescription(final SessionDescription sdp);

    /**
     * Callback fired once remote Ice candidate is received.
     */
    void onRemoteIceCandidate(final IceCandidate candidate);

    /**
     * Callback fired once remote Ice candidate removals are received.
     */
    void onRemoteIceCandidatesRemoved(final IceCandidate[] candidates);

    /**
     * Callback fired once channel is closed.
     */
    void onChannelClose();

    /**
     * Callback fired once channel error happened.
     */
    void onChannelError(final String description);
  }
}

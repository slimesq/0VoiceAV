// We make use of this 'server' variable to provide the address of the
// REST Janus API. By default, in this example we assume that Janus is
// co-located with the web server hosting the HTML pages but listening
// on a different port (8088, the default for HTTP in Janus), which is
// why we make use of the 'window.location.hostname' base address. Since
// Janus can also do HTTPS, and considering we don't really want to make
// use of HTTP for Janus if your demos are served on HTTPS, we also rely
// on the 'window.location.protocol' prefix to build the variable, in
// particular to also change the port used to contact Janus (8088 for
// HTTP and 8089 for HTTPS, if enabled).
// In case you place Janus behind an Apache frontend (as we did on the
// online demos at http://janus.conf.meetecho.com) you can just use a
// relative path for the variable, e.g.:
//
// 		var server = "/janus";
//
// which will take care of this on its own.
//
//
// If you want to use the WebSockets frontend to Janus, instead, you'll
// have to pass a different kind of address, e.g.:
//
// 		var server = "ws://" + window.location.hostname + ":8188";
//
// Of course this assumes that support for WebSockets has been built in
// when compiling the server. WebSockets support has not been tested
// as much as the REST API, so handle with care!
//
//
// If you have multiple options available, and want to let the library
// autodetect the best way to contact your server (or pool of servers),
// you can also pass an array of servers, e.g., to provide alternative
// means of access (e.g., try WebSockets first and, if that fails, fall
// back to plain HTTP) or just have failover servers:
//
//		var server = [
//			"ws://" + window.location.hostname + ":8188",
//			"/janus"
//		];
//
// This will tell the library to try connecting to each of the servers
// in the presented order. The first working server will be used for
// the whole session.
//
var server = null;
var server = "ws://192.168.2.132:8188";//"wss://" + "192.168.2.132" + ":8989";
// server = "wss://111.229.231.225:8989";
var janus = null;
var sfutest = null;
var opaqueId = "videoroomtest-" + Janus.randomString(12);

var myid = null;
var mystream = null;
// We use this other ID just to map our subscriptions to us
var mypvtid = null;

var feeds = [];	// 远端handle的集合
var bitrateTimer = [];	// 对端biterate集合

var doSimulcast = (getQueryStringValue("simulcast") === "yes" || getQueryStringValue("simulcast") === "true");
var doSimulcast2 = (getQueryStringValue("simulcast2") === "yes" || getQueryStringValue("simulcast2") === "true");

var myroom = 1234;
var isCameraClose = 0;
var resolution = null;
var myusername = null;
var appId = 0;
var isCreateRoom = 0;
var MaxPublishers = 9;
var mysecret = null;
// 1. 获取登录界面的参数
myroom = parseInt(getQueryStringValue("roomId"));		// 1.1 房间ID
isCameraClose = parseInt(getQueryStringValue("isCameraClose"));	// 是否是视频通话
// var selfUserType = 0;//getParameterByName("userType");
resolution = getQueryStringValue("resolution");
myusername = getQueryStringValue("userNameId");//'client_' +randomString(3)	// 1.2 用户名
appId = getQueryStringValue("appId");
isCreateRoom = parseInt(getQueryStringValue("isCreateRoom"), 0);	// 1.3 是否创建房间
mysecret = Janus.randomString(8);

// 加载页面就执行
$(document).ready(function () {

	$("#leave-room").click(function () {
        leaveRoom();
    });
	// Initialize the library (all console debuggers enabled)
	Janus.init({
		debug: "all", callback: function () {
			// Use a button to start the demo
			// Make sure the browser supports WebRTC
			if (!Janus.isWebrtcSupported()) {	// 检测思否支持webrtc	
				bootbox.alert("No WebRTC support... ");
				return;		// 返回前面页
			}
			// Create session
			janus = new Janus(
				{
					server: server,
					success: function () {
						// Attach to VideoRoom plugin
						janus.attach(		// 作为发布者attach 每个通路都要自己attach
							{
								plugin: "janus.plugin.videoroom",
								opaqueId: opaqueId,
								success: function (pluginHandle) {
									sfutest = pluginHandle;
									Janus.log("Plugin attached! (" + sfutest.getPlugin() + ", id=" + sfutest.getId() + ")");
									Janus.log("  -- This is a publisher/manager");
									// Prepare the username registration
									if(0 === isCreateRoom) {
										registerUsername();	// 加入房间
									}
									else {
										createRoom();		// 创建房间再加入
									} 	
								},
								error: function (error) {
									Janus.error("  -- Error attaching plugin...", error);
									bootbox.alert("Error attaching plugin... " + error);
								},
								consentDialog: function (on) {
									Janus.debug("Consent dialog should be " + (on ? "on" : "off") + " now");
									// if (on) {
									// 	// Darken screen and show hint
									// 	$.blockUI({
									// 		message: '<div><img src="up_arrow.png"/></div>',
									// 		css: {
									// 			border: 'none',
									// 			padding: '15px',
									// 			backgroundColor: 'transparent',
									// 			color: '#aaa',
									// 			top: '10px',
									// 			left: (navigator.mozGetUserMedia ? '-100px' : '300px')
									// 		}
									// 	});
									// } else {
									// 	// Restore screen
									// 	$.unblockUI();
									// }
								},
								iceState: function (state) {
									Janus.log("ICE state changed to " + state);
								},
								mediaState: function (medium, on) {
									Janus.log("Janus " + (on ? "started" : "stopped") + " receiving our " + medium);
								},
								webrtcState: function (on) {
									Janus.log("Janus says our WebRTC PeerConnection is " + (on ? "up" : "down") + " now");
									// $("#videolocal").parent().parent().unblock();
									if (!on)
										return;
									$('#publish').remove();
									// This controls allows us to override the global room bitrate cap
									$('#bitrate').parent().parent().removeClass('hide').show();
									$('#bitrate a').click(function () {
										var id = $(this).attr("id");
										var bitrate = parseInt(id) * 1000;
										if (bitrate === 0) {
											Janus.log("Not limiting bandwidth via REMB");
										} else {
											Janus.log("Capping bandwidth to " + bitrate + " via REMB");
										}
										$('#bitrateset').html($(this).html() + '<span class="caret"></span>').parent().removeClass('open');
										sfutest.send({ message: { request: "configure", bitrate: bitrate } });
										return false;
									});
								},
								onmessage: function (msg, jsep) {
									Janus.debug(" ::: Got a message (publisher) :::", msg);
									var event = msg["videoroom"];
									Janus.debug("Event: " + event);
									if (event) {
										if (event === "joined") {
											// Publisher/manager created, negotiate WebRTC and attach to existing feeds, if any
											myid = msg["id"];
											mypvtid = msg["private_id"];
											Janus.log("Successfully joined room " + msg["room"] + " with ID " + myid);
											publishOwnFeed(true);
											// Any new feed to attach to?
											if (msg["publishers"]) {
												var list = msg["publishers"];
												Janus.debug("Got a list of available publishers/feeds:", list);
												for (var f in list) {
													var id = list[f]["id"];
													var display = list[f]["display"];
													var audio = list[f]["audio_codec"];
													var video = list[f]["video_codec"];
													Janus.debug("  >> [" + id + "] " + display + " (audio: " + audio + ", video: " + video + ")");
													newRemoteFeed(id, display, audio, video);
												}
											}
										} else if (event === "destroyed") {
											// The room has been destroyed
											Janus.warn("The room has been destroyed!");
											backLogin();		// 返回登录页
										} else if (event === "event") {
											// Any new feed to attach to?
											if (msg["publishers"]) {
												var list = msg["publishers"];
												Janus.debug("Got a list of available publishers/feeds:", list);
												for (var f in list) {
													var id = list[f]["id"];
													var display = list[f]["display"];
													var audio = list[f]["audio_codec"];
													var video = list[f]["video_codec"];
													Janus.debug("  >> [" + id + "] " + display + " (audio: " + audio + ", video: " + video + ")");
													newRemoteFeed(id, display, audio, video);
												}
											} else if (msg["leaving"]) {
												// One of the publishers has gone away?
												var leaving = msg["leaving"];
												Janus.log("Publisher left: " + leaving);
												var remoteFeed = null;
												for (var i = 1; i < MaxPublishers; i++) {
													if (feeds[i] && feeds[i].rfid == leaving) {
														remoteFeed = feeds[i];
														break;
													}
												}
												if (remoteFeed != null) {
													Janus.debug("Feed " + remoteFeed.rfid + " (" + remoteFeed.rfdisplay + ") has left the room, detaching");
													$('#remote' + remoteFeed.rfindex).empty().hide();
													$('#videoremote' + remoteFeed.rfindex).empty();
													feeds[remoteFeed.rfindex] = null;
													remoteFeed.detach();
												}
											} else if (msg["unpublished"]) {
												// One of the publishers has unpublished?
												var unpublished = msg["unpublished"];
												Janus.log("Publisher left: " + unpublished);
												if (unpublished === 'ok') {
													// That's us
													sfutest.hangup();		// 这里是发布者的响应
													return;
												}
												var remoteFeed = null;
												for (var i = 1; i < MaxPublishers; i++) {
													if (feeds[i] && feeds[i].rfid == unpublished) {
														remoteFeed = feeds[i];	// 找到订阅者
														break;
													}
												}
												if (remoteFeed != null) {
													Janus.debug("Feed " + remoteFeed.rfid + " (" + remoteFeed.rfdisplay + ") has left the room, detaching");
													$('#remote' + remoteFeed.rfindex).empty().hide();
													$('#videoremote' + remoteFeed.rfindex).empty();
													feeds[remoteFeed.rfindex] = null;
													remoteFeed.detach();	// detach 
												}
											} else if (msg["error"]) {
												if (msg["error_code"] === 426) {		// 不存在
													// This is a "no such room" error: give a more meaningful description
													// bootbox.alert(
														// "<p>Apparently room <code>" + myroom + "</code> (the one this demo uses as a test room) " +
														// "does not exist...</p><p>Do you have an updated <code>janus.plugin.videoroom.jcfg</code> " +
														// "configuration file? If not, make sure you copy the details of room <code>" + myroom + "</code> " +
														// "from that sample in your current configuration file, then restart Janus and try again.",
														// backLogin()
													// );
													swal({
														title: "加入房间错误",
														text:  "房间" + myroom + "不存在,请先创建再加入",
														type: "error",
														showCancelButton: false,
														confirmButtonText: "返回登录页",
														closeOnConfirm: true
													}, function () {
														backLogin();
														// isCreateRoom = true;	// 策略问题，不存在可以创建再加入
														// createRoom();
													});
													
												} else {
													bootbox.alert(msg["error"]);
													backLogin();
												}
											}
										}
									}
									if (jsep) {
										Janus.debug("Handling SDP as well...", jsep);
										sfutest.handleRemoteJsep({ jsep: jsep });
										// Check if any of the media we wanted to publish has
										// been rejected (e.g., wrong or unsupported codec)
										var audio = msg["audio_codec"];
										if (mystream && mystream.getAudioTracks() && mystream.getAudioTracks().length > 0 && !audio) {
											// Audio has been rejected
											toastr.warning("Our audio stream has been rejected, viewers won't hear us");
										}
										var video = msg["video_codec"];
										if (mystream && mystream.getVideoTracks() && mystream.getVideoTracks().length > 0 && !video) {
											// Video has been rejected
											toastr.warning("Our video stream has been rejected, viewers won't see us");
											// Hide the webcam video
											$('#myvideo').hide();
											$('#videolocal').append(
												'<div class="no-video-container">' +
												'<i class="fa fa-video-camera fa-5 no-video-icon" style="height: 100%;"></i>' +
												'<span class="no-video-text" style="font-size: 16px;">Video rejected, no webcam</span>' +
												'</div>');
										}
									}
								},
								onlocalstream: function (stream) {
									Janus.debug(" ::: Got a local stream :::", stream);
									mystream = stream;
									$('#videojoin').hide();
									$('#videos').removeClass('hide').show();
									if ($('#myvideo').length === 0) {
										$('#videolocal').append('<video class="rounded centered" id="myvideo" width="100%" height="100%" autoplay playsinline muted="muted"/>');
										// Add a 'mute' button
										$('#videolocal').append('<button class="btn btn-warning btn-xs" id="muteaudio" style="position: absolute; bottom: 0px; left: 0px; margin: 10px;">MuteAudio</button>');
										$('#videolocal').append('<button class="btn btn-warning btn-xs" id="mutevideo" style="position: absolute; bottom: 0px; left: 80px; margin: 10px;">MuteVideo</button>');
										$('#muteaudio').click(toggleMuteAudio);
										$('#mutevideo').click(toggleMuteVideo);
										// Add an 'unpublish' button
										$('#videolocal').append('<button class="btn btn-warning btn-xs" id="unpublish" style="position: absolute; bottom: 0px; right: 0px; margin: 15px;">Unpublish</button>');
										$('#unpublish').click(unpublishOwnFeed);
									}
									$('#publisher').removeClass('hide').html(myusername).show();
									Janus.attachMediaStream($('#myvideo').get(0), stream);
									$("#myvideo").get(0).muted = "muted";
									if (sfutest.webrtcStuff.pc.iceConnectionState !== "completed" &&
										sfutest.webrtcStuff.pc.iceConnectionState !== "connected") {
										// $("#videolocal").parent().parent().block({	// 提示正在发布中
										// 	message: '<b>Publishing...</b>',
										// 	css: {
										// 		border: 'none',
										// 		backgroundColor: 'transparent',
										// 		color: 'white'
										// 	}
										// });
									}
									var videoTracks = stream.getVideoTracks();
									if (!videoTracks || videoTracks.length === 0) {
										// No webcam
										$('#myvideo').hide();
										if ($('#videolocal .no-video-container').length === 0) {
											$('#videolocal').append(
												'<div class="no-video-container">' +
												'<i class="fa fa-video-camera fa-5 no-video-icon"></i>' +
												'<span class="no-video-text">No webcam available</span>' +
												'</div>');
										}
									} else {
										$('#videolocal .no-video-container').remove();
										$('#myvideo').removeClass('hide').show();
									}
								},
								onremotestream: function (stream) {
									// The publisher stream is sendonly, we don't expect anything here
								},
								oncleanup: function () {
									Janus.log(" ::: Got a cleanup notification: we are unpublished now :::");
									mystream = null;
									$('#videolocal').html('<button id="publish" class="btn btn-primary">Publish</button>');
									$('#publish').click(function () { publishOwnFeed(true); });
									// $("#videolocal").parent().parent().unblock();
									$('#bitrate').parent().parent().addClass('hide');
									$('#bitrate a').unbind('click');
								}
							});
					},
					error: function (error) {
						Janus.error(error);
						bootbox.alert(error, function () {
							window.location.reload();
						});
					},
					destroyed: function () {
						window.location.reload();
					},
					autoLeaveRoom: function () {
						if(isCreateRoom == true) {		// 主要是销毁房间
							var sleep = function(time) {
								var startTime = new Date().getTime() + parseInt(time, 10);
								while(new Date().getTime() < startTime) {}
							};
							
							
							if(sfutest != undefined) {
								var destroy = {
									request: "destroy",
									secret: mysecret,
									room: myroom
								};	
								sfutest.send({ message: destroy });	 // 响应事件{"videoroom" : "destroyed","room" : <unique numeric ID>}			
								sfutest = null;	
								sleep(1000); // 延时函数，单位ms		// 同步阻塞1秒 先这么做吧
							}
						}
					}
				});
		}
	});
});

function checkEnter(field, event) {
	var theCode = event.keyCode ? event.keyCode : event.which ? event.which : event.charCode;
	if (theCode == 13) {
		registerUsername();
		return false;
	} else {
		return true;
	}
}

function backLogin() {
	if(sfutest !== undefined) {
		// sfutest.hangup();
	}
	
	// window.location.href = "login.html";
	var param = "roomId=" + myroom + "&isCameraClose="
		+ (isCameraClose ? 1 : 0) ;
	if (resolution != null && resolution != "") {
		param += "&resolution=" + resolution;
	}
	window.location.href = 'index.html?' + param;
}

function createRoom() {
	// Create a new room
	// var desc = $('#desc').val();
	role = "publisher";
	var create = {					// 创建房间的信令
		request: "create",
		room: myroom,		// 我们登录页面填写的房间号码
		description: "0voice room",
		bitrate: 500000,
		secret: mysecret,		// 如果create的时候用了，那destroy的时候也要用
		publishers: MaxPublishers
	};
	sfutest.send({ message: create, success: function(result) {
		var event = result["videoroom"];
		Janus.debug("Event: " + event);
		var error = result["error"];
		if(error !== undefined) {	// 存在错误
			swal({
				title: "创建房间错误",
				text:  error,
				type: "error",
				showCancelButton: false,
				confirmButtonText: "退出",
				closeOnConfirm: true
			}, function () {
				backLogin();
			});
		}
		if(event) {
			room = result["room"];
			if(room !== undefined)
			{	// 登录房间
				Janus.log("Video room have been created: " + room);
				var register = {
					request: "join",
					room: room,
					ptype: "publisher",
					display: myusername
				};
				sfutest.send({ message: register });
			}
		}
	}});
}
// 加入已经存在的房间
function registerUsername() {
	var register = {
		request: "join",
		room: myroom,
		ptype: "publisher",
		display: myusername
	};	
	sfutest.send({ message: register });
}

function leaveRoom(){
	if(isCreateRoom == true) {	// 创建者销毁房间
		bootbox.confirm({
			message: "你是会议的创建者，退出会议将会导致所有人退出该会议",
			buttons: {
				confirm: {
					label: '继续退出',
					className: 'btn-success'
				},
				cancel: {
					label: '留在会议',
					className: 'btn-danger'
				}
			},
			callback: function (result) {
				console.log('This was leave in the callback: ' + result);
				if(result == true) {
					var destroy = {
						request: "destroy",
						secret: mysecret,	
						room: myroom
					};	
					sfutest.send({ message: destroy });	 // 响应事件{"videoroom" : "destroyed","room" : <unique numeric ID>}			
				}
			}
		});
	} else {				// 如果只是加入者则leave房间
		// var leave = {
		// 	request: "leave"
		// };	
		// sfutest.send({ message: leave });	// 响应{"videoroom" : "event",			"leaving" : "ok"}
		backLogin();	// 会触发janus.js window.addEventListener(eventName, function(event)的事件响应，destroy session
	}	
}

function publishOwnFeed(useAudio) {
	// Publish our stream
	$('#publish').attr('disabled', true).unbind('click');
	sfutest.createOffer(
		{
			// Add data:true here if you want to publish datachannels as well
			media: { audioRecv: false, videoRecv: false, audioSend: useAudio, videoSend: true },	// Publishers are sendonly
			// If you want to test simulcasting (Chrome and Firefox only), then
			// pass a ?simulcast=true when opening this demo page: it will turn
			// the following 'simulcast' property to pass to janus.js to true
			simulcast: doSimulcast,
			simulcast2: doSimulcast2,
			success: function (jsep) {
				Janus.debug("Got publisher SDP!", jsep);
				var publish = { request: "configure", audio: useAudio, video: true };
				// You can force a specific codec to use when publishing by using the
				// audiocodec and videocodec properties, for instance:
				// 		publish["audiocodec"] = "opus"
				// to force Opus as the audio codec to use, or:
				// 		publish["videocodec"] = "vp9"
				// to force VP9 as the videocodec to use. In both case, though, forcing
				// a codec will only work if: (1) the codec is actually in the SDP (and
				// so the browser supports it), and (2) the codec is in the list of
				// allowed codecs in a room. With respect to the point (2) above,
				// refer to the text in janus.plugin.videoroom.jcfg for more details
				sfutest.send({ message: publish, jsep: jsep });	// publish + offer sdp
			},
			error: function (error) {
				Janus.error("WebRTC error:", error);
				if (useAudio) {
					publishOwnFeed(false);
				} else {
					bootbox.alert("WebRTC error... " + error.message);
					$('#publish').removeAttr('disabled').click(function () { publishOwnFeed(true); });
				}
			}
		});
}

function toggleMuteAudio() {
	var muted = sfutest.isAudioMuted();
	Janus.log((muted ? "Unmuting" : "Muting") + " local audio...");
	if (muted)
		sfutest.unmuteAudio();
	else
		sfutest.muteAudio();
	muted = sfutest.isAudioMuted();
	$('#muteaudio').html(muted ? "UnmuteAudio" : "MuteAudio");
}

function toggleMuteVideo() {
	var muted = sfutest.isVideoMuted();
	Janus.log((muted ? "Unmuting" : "Muting") + " local video...");
	if (muted)
		sfutest.unmuteVideo();
	else
		sfutest.muteVideo();
	muted = sfutest.isVideoMuted();
	$('#mutevideo').html(muted ? "UnmuteVideo" : "MuteVideo");
}



function unpublishOwnFeed() {
	// Unpublish our stream
	$('#unpublish').attr('disabled', true).unbind('click');
	var unpublish = { request: "unpublish" };
	sfutest.send({ message: unpublish });
}
// 订阅
function newRemoteFeed(id, display, audio, video) {
	// A new feed has been published, create a new plugin handle and attach to it as a subscriber
	var remoteFeed = null;
	janus.attach(		// 作为订阅者attach
		{
			plugin: "janus.plugin.videoroom",
			opaqueId: opaqueId,
			success: function (pluginHandle) {
				remoteFeed = pluginHandle;
				remoteFeed.simulcastStarted = false;
				Janus.log("Plugin attached! (" + remoteFeed.getPlugin() + ", id=" + remoteFeed.getId() + ")");
				Janus.log("  -- This is a subscriber");
				// We wait for the plugin to send us an offer
				var subscribe = {
					request: "join",
					room: myroom,
					ptype: "subscriber",
					feed: id,
					private_id: mypvtid
				};
				// In case you don't want to receive audio, video or data, even if the
				// publisher is sending them, set the 'offer_audio', 'offer_video' or
				// 'offer_data' properties to false (they're true by default), e.g.:
				// 		subscribe["offer_video"] = false;
				// For example, if the publisher is VP8 and this is Safari, let's avoid video
				if (Janus.webRTCAdapter.browserDetails.browser === "safari" &&
					(video === "vp9" || (video === "vp8" && !Janus.safariVp8))) {
					if (video)
						video = video.toUpperCase()
					toastr.warning("Publisher is using " + video + ", but Safari doesn't support it: disabling video");
					subscribe["offer_video"] = false;
				}
				remoteFeed.videoCodec = video;
				remoteFeed.send({ message: subscribe });
			},
			error: function (error) {
				Janus.error("  -- Error attaching plugin...", error);
				bootbox.alert("Error attaching plugin... " + error);
			},
			onmessage: function (msg, jsep) {
				Janus.debug(" ::: Got a message (subscriber) :::", msg);
				var event = msg["videoroom"];
				Janus.debug("Event: " + event);
				if (msg["error"]) {
					bootbox.alert(msg["error"]);
				} else if (event) {
					if (event === "attached") {
						// Subscriber created and attached
						for (var i = 1; i < MaxPublishers; i++) {
							if (!feeds[i]) {
								feeds[i] = remoteFeed;
								remoteFeed.rfindex = i;
								break;
							}
						}
						remoteFeed.rfid = msg["id"];
						remoteFeed.rfdisplay = msg["display"];
						if (!remoteFeed.spinner) {
							var target = document.getElementById('videoremote' + remoteFeed.rfindex);
							remoteFeed.spinner = new Spinner({ top: 100 }).spin(target);
						} else {
							remoteFeed.spinner.spin();
						}
						Janus.log("Successfully attached to feed " + remoteFeed.rfid + " (" + remoteFeed.rfdisplay + ") in room " + msg["room"]);
						$('#remote' + remoteFeed.rfindex).removeClass('hide').html(remoteFeed.rfdisplay).show();
					} else if (event === "event") {
						// Check if we got an event on a simulcast-related event from this publisher
						var substream = msg["substream"];
						var temporal = msg["temporal"];
						if ((substream !== null && substream !== undefined) || (temporal !== null && temporal !== undefined)) {
							if (!remoteFeed.simulcastStarted) {
								remoteFeed.simulcastStarted = true;
								// Add some new buttons
								addSimulcastButtons(remoteFeed.rfindex, remoteFeed.videoCodec === "vp8" || remoteFeed.videoCodec === "h264");
							}
							// We just received notice that there's been a switch, update the buttons
							updateSimulcastButtons(remoteFeed.rfindex, substream, temporal);
						}
					} else {
						// What has just happened?
					}
				}
				if (jsep) {		// 对方(Janus服务器)发送offer
					Janus.debug("Handling SDP as well...", jsep);
					// Answer and attach
					remoteFeed.createAnswer(
						{
							jsep: jsep,
							// Add data:true here if you want to subscribe to datachannels as well
							// (obviously only works if the publisher offered them in the first place)
							media: { audioSend: false, videoSend: false },	// We want recvonly audio/video
							success: function (jsep) {
								Janus.debug("Got SDP!", jsep);
								var body = { request: "start", room: myroom };
								remoteFeed.send({ message: body, jsep: jsep });
							},
							error: function (error) {
								Janus.error("WebRTC error:", error);
								bootbox.alert("WebRTC error... " + error.message);
							}
						});
				}
			},
			iceState: function (state) {
				Janus.log("ICE state of this WebRTC PeerConnection (feed #" + remoteFeed.rfindex + ") changed to " + state);
			},
			webrtcState: function (on) {
				Janus.log("Janus says this WebRTC PeerConnection (feed #" + remoteFeed.rfindex + ") is " + (on ? "up" : "down") + " now");
			},
			onlocalstream: function (stream) {
				// The subscriber stream is recvonly, we don't expect anything here
			},
			onremotestream: function (stream) {
				Janus.debug("Remote feed #" + remoteFeed.rfindex + ", stream:", stream);
				var addButtons = false;
				if ($('#remotevideo' + remoteFeed.rfindex).length === 0) {
					addButtons = true;
					// No remote video yet
					$('#videoremote' + remoteFeed.rfindex).append('<video class="rounded centered" id="waitingvideo' + remoteFeed.rfindex + '" width=320 height=240 />');
					$('#videoremote' + remoteFeed.rfindex).append('<video class="rounded centered relative hide" id="remotevideo' + remoteFeed.rfindex + '" width="100%" height="100%" autoplay playsinline/>');
					$('#videoremote' + remoteFeed.rfindex).append(
						'<span class="label label-primary hide" id="curres' + remoteFeed.rfindex + '" style="position: absolute; bottom: 0px; left: 0px; margin: 15px;"></span>' +
						'<span class="label label-info hide" id="curbitrate' + remoteFeed.rfindex + '" style="position: absolute; bottom: 0px; right: 0px; margin: 15px;"></span>');
					// Show the video, hide the spinner and show the resolution when we get a playing event
					$("#remotevideo" + remoteFeed.rfindex).bind("playing", function () {
						if (remoteFeed.spinner)
							remoteFeed.spinner.stop();
						remoteFeed.spinner = null;
						$('#waitingvideo' + remoteFeed.rfindex).remove();
						if (this.videoWidth)
							$('#remotevideo' + remoteFeed.rfindex).removeClass('hide').show();
						var width = this.videoWidth;
						var height = this.videoHeight;
						$('#curres' + remoteFeed.rfindex).removeClass('hide').text(width + 'x' + height).show();
						if (Janus.webRTCAdapter.browserDetails.browser === "firefox") {
							// Firefox Stable has a bug: width and height are not immediately available after a playing
							setTimeout(function () {
								var width = $("#remotevideo" + remoteFeed.rfindex).get(0).videoWidth;
								var height = $("#remotevideo" + remoteFeed.rfindex).get(0).videoHeight;
								$('#curres' + remoteFeed.rfindex).removeClass('hide').text(width + 'x' + height).show();
							}, 2000);
						}
					});
				}
				Janus.attachMediaStream($('#remotevideo' + remoteFeed.rfindex).get(0), stream);
				var videoTracks = stream.getVideoTracks();
				if (!videoTracks || videoTracks.length === 0) {
					// No remote video
					$('#remotevideo' + remoteFeed.rfindex).hide();
					if ($('#videoremote' + remoteFeed.rfindex + ' .no-video-container').length === 0) {
						$('#videoremote' + remoteFeed.rfindex).append(
							'<div class="no-video-container">' +
							'<i class="fa fa-video-camera fa-5 no-video-icon"></i>' +
							'<span class="no-video-text">No remote video available</span>' +
							'</div>');
					}
				} else {
					$('#videoremote' + remoteFeed.rfindex + ' .no-video-container').remove();
					$('#remotevideo' + remoteFeed.rfindex).removeClass('hide').show();
				}
				if (!addButtons)
					return;
				if (Janus.webRTCAdapter.browserDetails.browser === "chrome" || Janus.webRTCAdapter.browserDetails.browser === "firefox" ||
					Janus.webRTCAdapter.browserDetails.browser === "safari") {
					$('#curbitrate' + remoteFeed.rfindex).removeClass('hide').show();
					bitrateTimer[remoteFeed.rfindex] = setInterval(function () {
						// Display updated bitrate, if supported
						var bitrate = remoteFeed.getBitrate();
						$('#curbitrate' + remoteFeed.rfindex).text(bitrate);
						// Check if the resolution changed too
						var width = $("#remotevideo" + remoteFeed.rfindex).get(0).videoWidth;
						var height = $("#remotevideo" + remoteFeed.rfindex).get(0).videoHeight;
						if (width > 0 && height > 0)
							$('#curres' + remoteFeed.rfindex).removeClass('hide').text(width + 'x' + height).show();
					}, 1000);
				}
			},
			oncleanup: function () {
				Janus.log(" ::: Got a cleanup notification (remote feed " + id + ") :::");
				if (remoteFeed.spinner)
					remoteFeed.spinner.stop();
				remoteFeed.spinner = null;
				$('#remotevideo' + remoteFeed.rfindex).remove();
				$('#waitingvideo' + remoteFeed.rfindex).remove();
				$('#novideo' + remoteFeed.rfindex).remove();
				$('#curbitrate' + remoteFeed.rfindex).remove();
				$('#curres' + remoteFeed.rfindex).remove();
				if (bitrateTimer[remoteFeed.rfindex])
					clearInterval(bitrateTimer[remoteFeed.rfindex]);
				bitrateTimer[remoteFeed.rfindex] = null;
				remoteFeed.simulcastStarted = false;
				$('#simulcast' + remoteFeed.rfindex).remove();
			}
		});
}

// Helper to parse query string
function getQueryStringValue(name) {
	name = name.replace(/[\[]/, "\\[").replace(/[\]]/, "\\]");
	var regex = new RegExp("[\\?&]" + name + "=([^&#]*)"),
		results = regex.exec(location.search);
	return results === null ? "" : decodeURIComponent(results[1].replace(/\+/g, " "));
}

// Helpers to create Simulcast-related UI, if enabled
function addSimulcastButtons(feed, temporal) {
	var index = feed;
	$('#remote' + index).parent().append(
		'<div id="simulcast' + index + '" class="btn-group-vertical btn-group-vertical-xs pull-right">' +
		'	<div class"row">' +
		'		<div class="btn-group btn-group-xs" style="width: 100%">' +
		'			<button id="sl' + index + '-2" type="button" class="btn btn-primary" data-toggle="tooltip" title="Switch to higher quality" style="width: 33%">SL 2</button>' +
		'			<button id="sl' + index + '-1" type="button" class="btn btn-primary" data-toggle="tooltip" title="Switch to normal quality" style="width: 33%">SL 1</button>' +
		'			<button id="sl' + index + '-0" type="button" class="btn btn-primary" data-toggle="tooltip" title="Switch to lower quality" style="width: 34%">SL 0</button>' +
		'		</div>' +
		'	</div>' +
		'	<div class"row">' +
		'		<div class="btn-group btn-group-xs hide" style="width: 100%">' +
		'			<button id="tl' + index + '-2" type="button" class="btn btn-primary" data-toggle="tooltip" title="Cap to temporal layer 2" style="width: 34%">TL 2</button>' +
		'			<button id="tl' + index + '-1" type="button" class="btn btn-primary" data-toggle="tooltip" title="Cap to temporal layer 1" style="width: 33%">TL 1</button>' +
		'			<button id="tl' + index + '-0" type="button" class="btn btn-primary" data-toggle="tooltip" title="Cap to temporal layer 0" style="width: 33%">TL 0</button>' +
		'		</div>' +
		'	</div>' +
		'</div>'
	);
	// Enable the simulcast selection buttons
	$('#sl' + index + '-0').removeClass('btn-primary btn-success').addClass('btn-primary')
		.unbind('click').click(function () {
			toastr.info("Switching simulcast substream, wait for it... (lower quality)", null, { timeOut: 2000 });
			if (!$('#sl' + index + '-2').hasClass('btn-success'))
				$('#sl' + index + '-2').removeClass('btn-primary btn-info').addClass('btn-primary');
			if (!$('#sl' + index + '-1').hasClass('btn-success'))
				$('#sl' + index + '-1').removeClass('btn-primary btn-info').addClass('btn-primary');
			$('#sl' + index + '-0').removeClass('btn-primary btn-info btn-success').addClass('btn-info');
			feeds[index].send({ message: { request: "configure", substream: 0 } });
		});
	$('#sl' + index + '-1').removeClass('btn-primary btn-success').addClass('btn-primary')
		.unbind('click').click(function () {
			toastr.info("Switching simulcast substream, wait for it... (normal quality)", null, { timeOut: 2000 });
			if (!$('#sl' + index + '-2').hasClass('btn-success'))
				$('#sl' + index + '-2').removeClass('btn-primary btn-info').addClass('btn-primary');
			$('#sl' + index + '-1').removeClass('btn-primary btn-info btn-success').addClass('btn-info');
			if (!$('#sl' + index + '-0').hasClass('btn-success'))
				$('#sl' + index + '-0').removeClass('btn-primary btn-info').addClass('btn-primary');
			feeds[index].send({ message: { request: "configure", substream: 1 } });
		});
	$('#sl' + index + '-2').removeClass('btn-primary btn-success').addClass('btn-primary')
		.unbind('click').click(function () {
			toastr.info("Switching simulcast substream, wait for it... (higher quality)", null, { timeOut: 2000 });
			$('#sl' + index + '-2').removeClass('btn-primary btn-info btn-success').addClass('btn-info');
			if (!$('#sl' + index + '-1').hasClass('btn-success'))
				$('#sl' + index + '-1').removeClass('btn-primary btn-info').addClass('btn-primary');
			if (!$('#sl' + index + '-0').hasClass('btn-success'))
				$('#sl' + index + '-0').removeClass('btn-primary btn-info').addClass('btn-primary');
			feeds[index].send({ message: { request: "configure", substream: 2 } });
		});
	if (!temporal)	// No temporal layer support
		return;
	$('#tl' + index + '-0').parent().removeClass('hide');
	$('#tl' + index + '-0').removeClass('btn-primary btn-success').addClass('btn-primary')
		.unbind('click').click(function () {
			toastr.info("Capping simulcast temporal layer, wait for it... (lowest FPS)", null, { timeOut: 2000 });
			if (!$('#tl' + index + '-2').hasClass('btn-success'))
				$('#tl' + index + '-2').removeClass('btn-primary btn-info').addClass('btn-primary');
			if (!$('#tl' + index + '-1').hasClass('btn-success'))
				$('#tl' + index + '-1').removeClass('btn-primary btn-info').addClass('btn-primary');
			$('#tl' + index + '-0').removeClass('btn-primary btn-info btn-success').addClass('btn-info');
			feeds[index].send({ message: { request: "configure", temporal: 0 } });
		});
	$('#tl' + index + '-1').removeClass('btn-primary btn-success').addClass('btn-primary')
		.unbind('click').click(function () {
			toastr.info("Capping simulcast temporal layer, wait for it... (medium FPS)", null, { timeOut: 2000 });
			if (!$('#tl' + index + '-2').hasClass('btn-success'))
				$('#tl' + index + '-2').removeClass('btn-primary btn-info').addClass('btn-primary');
			$('#tl' + index + '-1').removeClass('btn-primary btn-info').addClass('btn-info');
			if (!$('#tl' + index + '-0').hasClass('btn-success'))
				$('#tl' + index + '-0').removeClass('btn-primary btn-info').addClass('btn-primary');
			feeds[index].send({ message: { request: "configure", temporal: 1 } });
		});
	$('#tl' + index + '-2').removeClass('btn-primary btn-success').addClass('btn-primary')
		.unbind('click').click(function () {
			toastr.info("Capping simulcast temporal layer, wait for it... (highest FPS)", null, { timeOut: 2000 });
			$('#tl' + index + '-2').removeClass('btn-primary btn-info btn-success').addClass('btn-info');
			if (!$('#tl' + index + '-1').hasClass('btn-success'))
				$('#tl' + index + '-1').removeClass('btn-primary btn-info').addClass('btn-primary');
			if (!$('#tl' + index + '-0').hasClass('btn-success'))
				$('#tl' + index + '-0').removeClass('btn-primary btn-info').addClass('btn-primary');
			feeds[index].send({ message: { request: "configure", temporal: 2 } });
		});
}

function updateSimulcastButtons(feed, substream, temporal) {
	// Check the substream
	var index = feed;
	if (substream === 0) {
		toastr.success("Switched simulcast substream! (lower quality)", null, { timeOut: 2000 });
		$('#sl' + index + '-2').removeClass('btn-primary btn-success').addClass('btn-primary');
		$('#sl' + index + '-1').removeClass('btn-primary btn-success').addClass('btn-primary');
		$('#sl' + index + '-0').removeClass('btn-primary btn-info btn-success').addClass('btn-success');
	} else if (substream === 1) {
		toastr.success("Switched simulcast substream! (normal quality)", null, { timeOut: 2000 });
		$('#sl' + index + '-2').removeClass('btn-primary btn-success').addClass('btn-primary');
		$('#sl' + index + '-1').removeClass('btn-primary btn-info btn-success').addClass('btn-success');
		$('#sl' + index + '-0').removeClass('btn-primary btn-success').addClass('btn-primary');
	} else if (substream === 2) {
		toastr.success("Switched simulcast substream! (higher quality)", null, { timeOut: 2000 });
		$('#sl' + index + '-2').removeClass('btn-primary btn-info btn-success').addClass('btn-success');
		$('#sl' + index + '-1').removeClass('btn-primary btn-success').addClass('btn-primary');
		$('#sl' + index + '-0').removeClass('btn-primary btn-success').addClass('btn-primary');
	}
	// Check the temporal layer
	if (temporal === 0) {
		toastr.success("Capped simulcast temporal layer! (lowest FPS)", null, { timeOut: 2000 });
		$('#tl' + index + '-2').removeClass('btn-primary btn-success').addClass('btn-primary');
		$('#tl' + index + '-1').removeClass('btn-primary btn-success').addClass('btn-primary');
		$('#tl' + index + '-0').removeClass('btn-primary btn-info btn-success').addClass('btn-success');
	} else if (temporal === 1) {
		toastr.success("Capped simulcast temporal layer! (medium FPS)", null, { timeOut: 2000 });
		$('#tl' + index + '-2').removeClass('btn-primary btn-success').addClass('btn-primary');
		$('#tl' + index + '-1').removeClass('btn-primary btn-info btn-success').addClass('btn-success');
		$('#tl' + index + '-0').removeClass('btn-primary btn-success').addClass('btn-primary');
	} else if (temporal === 2) {
		toastr.success("Capped simulcast temporal layer! (highest FPS)", null, { timeOut: 2000 });
		$('#tl' + index + '-2').removeClass('btn-primary btn-info btn-success').addClass('btn-success');
		$('#tl' + index + '-1').removeClass('btn-primary btn-success').addClass('btn-primary');
		$('#tl' + index + '-0').removeClass('btn-primary btn-success').addClass('btn-primary');
	}
}

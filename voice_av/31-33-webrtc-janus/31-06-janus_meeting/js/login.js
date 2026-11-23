(function (win, global_config) {
    var util = win.util;
    var localStorage = util.localStorage;
    var roomId = localStorage.get("roomId");
    var conUserName = localStorage.get("conUserName");
    var resolution = localStorage.get("resolution");
    var isCameraClose = false;
    var userType = 0;       // 强制为普通模式       比如后续添加 订阅者模式(加入会议只是看别人聊天)
    var mediaArea =  $(".voice-mediaarea");
    var talkTypeRadio =  $("#voice-talkType");
    var voiceConvId =  $("#voice-roomId");      // 房间ID
    var userNameId =  $("#voice-userNameId");   // 用户名
    

    conUserName = ''
    voiceConvId.val(roomId);
    userNameId.val(conUserName);
    //页面的自适应
    function pageAdaptation() {
        bodyHeight = win.innerHeight;
        $("body").css({"height": bodyHeight})
    }
    pageAdaptation();
    win.onresize = pageAdaptation;

    //分辨率样式
    if (resolution != null && resolution != "") {
        $(".voice-media-resolution input[value='" + resolution + "']").attr("checked", true);
    } else {
        $(".voice-media-resolution input[value='640*480']").attr("checked", true);
    }


    //事件处理
    // 分辨率设置的按钮点击事件
    $(".voice-media-btn").click(function () {
        $(this).css('display', 'none');
        mediaArea.css('display', 'block');
        mediaArea[0].tabIndex = 0;
        mediaArea[0].focus();
        mediaArea[0].style.outline = "none";
    });

    // 分辨率设置的窗口失去焦点事件
    mediaArea.blur(function () {
        $(this).css('display', 'none');
        $(".voice-media-btn").css('display', 'block');
    });
    // 是否关闭摄像头change事件
    // $('input[name=voice-talkType][type=radio]').change(
    // talkTypeRadio.change(function (e) {
    $('input[name=voice-talkType][type=radio]').change(function (e) {   // 视频通话和音频通话的选择
        var type = $(this).attr('data-video-call');
        if (type=='video') {
            isCameraClose = false;
        } else {
            isCameraClose = true;
        }
    });
   
    // 会议ID输入框获得焦点事件
    voiceConvId.focus(function () {
        this.placeholder = '';
        if (this.value == '请在这里输入会议ID') {
            this.value = '';
        }
    });
    // 会议ID输入框失去焦点事件
    voiceConvId.blur(function () {
        if (this.value == '') {
            this.placeholder = '请在这里输入会议ID';
        }
    });

    // 用户名输入框获得焦点事件
    userNameId.focus(function () {
        this.placeholder = '';
        if (this.value == '请在这里输入用户名') {
            this.value = '';
        }
    });
    // 用户名输入框失去焦点事件
    userNameId.blur(function () {
        if (this.value == '') {
            this.placeholder = '请在这里输入用户名';
        }
    });

    

    // 页面回车事件——加入会议
    document.onkeydown = function (e) {
        var ev = document.all ? window.event : e;
        if (ev.keyCode == 13) {
            joinConversation(global_config);
        }
    };
    // 单纯是加入房间，如果房间不存在则返回错误信息
    $("#join-room").click(function () {
        joinConversation(global_config, 0)
    });
    // 创建并加入房间，如果房间已经存在则返回错误信息
    $("#create-room").click(function () {
        joinConversation(global_config, 1)
    });

    function joinConversation(global_config, isCreateRoom) {
        // 会议ID
        var roomId = $('#voice-roomId').val();
        if (roomId == "" || roomId == "请输入会议ID") {     // 目前一定是要数字， 可以配置janus支持roomid为string类型
            util.swal("请输入会议ID");
            return;
        }

        var userNameId = $('#voice-userNameId').val();
        // if (userNameId == "" || roomId == "请输入用户名") {
        //     util.swal("请输入用户名");
        //     return;
        // }
         // 生成随机数字字符串
        function randomString(strLength) {
            var result = [];
            strLength = strLength || 5;
            var charSet = "0123456789";
            while (strLength--) {
                result.push(charSet.charAt(Math.floor(Math.random() * charSet.length)));
            }
            return result.join("");
        }

        // 正常身份
        // var userType = 0;
        // 分辨率
        var resolution = $("input[name='media_radio']:checked").val();
        // var userId = util.guid();
        // var userId = randomString(10);
        var appId = global_config.appId;
        // var tpl = 'roomId={roomId}&userNameId={userNameId}&isCameraClose={isCameraClose}&resolution={resolution}&userId={userId}&appId={appId}&userType={userType}';
        // var tpl = 'roomId={roomId}&isCameraClose={isCameraClose}&resolution={resolution}&userId={userId}&appId={appId}&userType={userType}';
        // 添加参数时需要两个地方都添加 tpl和queryString
        var tpl = 'roomId={roomId}&userNameId={userNameId}&isCameraClose={isCameraClose}&resolution={resolution}&appId={appId}&isCreateRoom={isCreateRoom}';
        var queryString = util.formatTpl(tpl, {
            roomId: roomId,     // 房间id
            userNameId:userNameId,  // 用户名
            isCameraClose: isCameraClose,   //是否关闭摄像头，此时则为语音通话
            resolution: resolution,         // 分辨率
            // userId: userId,
            appId: appId,                   // 租户id，开发给第三方使用时才需要，可以先不理会
            isCreateRoom: isCreateRoom      // 是否要新建创建，true:新建房间; false加入已经存在的房间
            // userType: userType
        });
        win.location.href = 'videoroom.html?' + queryString;        // 网页跳转
    }
})(window, global_config);

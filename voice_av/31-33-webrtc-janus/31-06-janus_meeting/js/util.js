(function (win) {
    var util = new Object();
    util.localStorage = {
        set: function (key,value) {
            localStorage.setItem(key,value)
            return localStorage
        },
        get: function (name) {
            return localStorage.getItem(name)
        },
        del: function (name) {
            return localStorage.removeItem(name)
        },
        clear: function () {
            return localStorage.clear()
        }
    };
    util.getParameterByName = function (name, url) {
        if (!url)
            url = window.location.href;
        name = name.replace(/[\[\]]/g, "\\$&");

        var regex = new RegExp("[?&]" + name + "(=([^&#]*)|&|#|$)", "i"),
            results = regex.exec(url);

        if (!results)
            return null;
        if (!results[2])
            return '';
        return decodeURIComponent(results[2].replace(/\+/g, " "));
    };
    util.formatSeconds = function (value) {
        var theTime = parseInt(value);// s
        var theTime1 = 0;// m
        var theTime2 = 0;// h
        if (theTime >= 60) {
            theTime1 = parseInt(theTime / 60);
            theTime = parseInt(theTime % 60);
            if (theTime1 >= 60) {
                theTime2 = parseInt(theTime1 / 60);
                theTime1 = parseInt(theTime1 % 60);
            }
        }
        var result = "" + parseInt(theTime) < 10 ? "0" + theTime : theTime;
        // if (theTime1 > 0)
        {
            result = "" + (parseInt(theTime1) < 10 ? "0" + theTime1 : theTime1)
                + ":" + result;
        }
        if (theTime2 > 0) {
            result = "" + (parseInt(theTime2) < 10 ? "0" + theTime2 : theTime2)
                + ":" + result;
        }
        return result;
    };
    util.guid = function () {
        function s4() {
            return Math.floor((1 + Math.random()) * 0x10000).toString(16)
                .substring(1);
        }

        var arr = [];
        for (var i = 0; i < 8; i++) {
            var item = s4();
            arr.push(item)
        }
        return arr.join("-")
    };
    util.formatTime = function (template, date) {
        var dateObj = {
            "M+": date.getMonth() + 1,
            "d+": date.getDate(),
            "h+": date.getHours(),
            "m+": date.getMinutes(),
            "s+": date.getSeconds(),
            "S": date.getMilliseconds()
        }
        if (/(y+)/.test(template)) {
            var newYear = (date.getFullYear() + "").substr(4 - RegExp.$1.length)
            template = template.replace(RegExp.$1, newYear);
        }
        for (var k  in dateObj) {
            if (new RegExp("(" + k + ")").test(template)) {
                var newDate = (RegExp.$1.length == 1) ? (dateObj[k]) : (("00" + dateObj[k]).substr(("" + dateObj[k]).length))
                template = template.replace(RegExp.$1, newDate);
            }

        }
        return template
    }
    util.formatTpl = function (tpl, data) {
        for (key in data) {
            tpl = tpl.replace("{" + key + "}", data[key])
        }
        return tpl
    };
    util.swal = function (str) {
        swal(str);
    };
    win.util = util
})(window)

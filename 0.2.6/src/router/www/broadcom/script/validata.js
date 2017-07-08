/**
 * Created by zhangxin on 2017/6/1.
 */
String.prototype.trim=function(){
    return this.replace(/(^\s*)|(\s*$)/g, "");
}
var validate=new Object();
/**
 * 判断值是否为空
 * @param _ele 要判断的输入框 document.getElementById();
 * @param param_name  要判断的文本框的名称
 */
validate.isNull=function (_ele,param_name) {
    var val=_ele.value.trim();
    if(val.length==0||val==null||val==undefined||val==""){
        var error_ele='<p class="error-text">'+param_name+'不能为空</p>';
        $(_ele).parent().addClass("has-error").find(".error-text").remove();
        $(_ele).parent().append(error_ele);
        return false;
    }else{
        $(_ele).parent().removeClass("has-error").find(".error-text").remove();
        return true;
    }
}
/**
 * 验证 6-32位由字母、数字、下划线组成的字符串
 * @param _ele 要判断的输入框 document.getElementById();
 * @param param_name 要判断的文本框的名称
 * 正则
 * ^[a-zA-Z]\w{5,33}$
 */
validate.length_6_32=function (_ele,param_name) {
    var val=_ele.value.trim();
    reg=/^[a-zA-Z]\w{5,33}$/;
    if(val.length==0||val==null||val==undefined||val==""){
        var error_ele='<p class="error-text">'+param_name+'不能为空</p>';
        $(_ele).parent().addClass("has-error").find(".error-text").remove();
        $(_ele).parent().append(error_ele);
        return false;
    }else{
        if(!reg.test(val)){
            var error_ele='<p class="error-text">'+param_name+'为6-32位由字母、数字、下划线组成</p>';
            $(_ele).parent().addClass("has-error").find(".error-text").remove();
            $(_ele).parent().append(error_ele);
            return false;
        }else{
            $(_ele).parent().removeClass("has-error").find(".error-text").remove();
            return true;
        }
    }
}
/**
 * 验证IP地址
 * @param _ele 要判断的输入框 document.getElementById();
 * @param param_name 要判断的文本框的名称
 * 正则
 * /^(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])(\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])){3}$/
 */
validate.isIP=function (_ele,param_name) {
    var val=_ele.value.trim();
    reg=/^(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])(\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])){3}$/;
    if(val.length==0||val==null||val==undefined||val==""){
        var error_ele='<p class="error-text">'+param_name+'不能为空</p>';
        $(_ele).parent().addClass("has-error").find(".error-text").remove();
        $(_ele).parent().append(error_ele);
        return false;
    }else{
        if(!reg.test(val)){
            var error_ele='<p class="error-text">'+param_name+'不正确</p>';
            $(_ele).parent().addClass("has-error").find(".error-text").remove();
            $(_ele).parent().append(error_ele);
            return false;
        }else{
            $(_ele).parent().removeClass("has-error").find(".error-text").remove();
            return true;
        }
    }
}


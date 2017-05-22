/**
 * Created by zhangxin on 2017/4/8.
 */
$(function(){
    /*横向菜单切换标记*/
    $(".nav").on("click","a",function () {
        $(this).parent().addClass("active").siblings().removeClass("active");
    });
    /*计算page-wrapper的高度，和footer的位置*/
    var window_heigth=$(window).outerHeight(true),
    top_height=$(".page-header").outerHeight(true),
    footer_height=$(".page-footer").outerHeight(true);
    var wrapper_height=window_heigth-top_height-footer_height;
    $(".page-wrapper").css({"min-height":wrapper_height});
    $(window).resize(function () {
        window_heigth=$(window).height(),
        top_height=$(".page-header").outerHeight(true),
        footer_height=$(".page-footer").outerHeight(true);
        var wrapper_height=window_heigth-top_height-footer_height;
        $(".page-wrapper").css({"min-height":wrapper_height});
    });
    /*无线密码-开关按钮事件*/
    if($(this).children("input[type=checkbox]").is(":checked")){
        $("#pwd_input").removeAttr("readonly");
    }else{
        $("#pwd_input").attr("readonly","readonly");
    }
    $("#pwd_switch").on("click",function () {
        if($(this).children("input[type=checkbox]").is(":checked")){
            $("#pwd_input").removeAttr("readonly");
        }else{
            $("#pwd_input").attr("readonly","readonly");
        }
    });
    /*左侧折叠菜单*/
    $(".el-menu").on("click",".el-submenu__title",function () {
        var _this= $(this);
        _this.parent().addClass('is-opened').siblings().removeClass("is-opened");
    }).on("click","li.el-menu-item",function (e) {
        $(".el-menu-item").removeClass("is-active");
        $(this).addClass("is-active");
        e.stopPropagation();
    });
});
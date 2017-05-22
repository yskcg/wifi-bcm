$(function () {
    $('nav').on('click', 'li', function () {
        $(this).children('ul').slideToggle(function() {
            $(this).toggleClass('in out');
        });
        
        $(this).siblings().find('ul').slideUp(function() {
            $(this).removeClass('in').addClass('out');
        });
    });
    $('nav ul li ul li').click(function(e) {
        e.stopPropagation();
    });
})
/*
function footerAlign() {
  $('footer').css('display', 'block');
  $('footer').css('height', 'auto');
  var footerHeight = $('footer').outerHeight();
  $('body').css('padding-bottom', footerHeight);
  $('footer').css('height', footerHeight);
}

$( window ).resize(function() {
  footerAlign();
});

$(document).ready(function(){
  footerAlign();
});*/

function ping()
{
  var hostname = $("#host").val();
  var index;
  $.post("/ping", "host=" + hostname, function(data) {
    if (data.substr(0, 1) != "{") {
      // errors
      $("#ping_result").val(data);
      return;
    }
    index = JSON.parse(data)["index"];

    for (var i = 1; i <= 10; i++) {
      setTimeout(() => {
        $.get("/ping?id=" + index, function(data) {
          parsed = JSON.parse(data);
          if (parsed["status"] == "ok") {
            $("#ping_result").val(atob(parsed["result"]));
          }
          else {
            $("#ping_result").val(parsed["status"]);
          }
        });
      }, i * 800);
    }
  });
  return true;
}

function refresh_status()
{
  $.get("/status", function(data) {
    var parsed = JSON.parse(data);
    $("#overview").html(parsed["status"]);
    if (parsed["status"] == "ok") {
      $("#lan_ip").html(parsed["lan_ip"]);
      $("#cpu_usage").html(parsed["cpu_usage"]);
      $("#bandwidth").html(parsed["bandwidth"]);
    }
  });
  setTimeout(refresh_status, 5000);
}

$(document).ready(function() {
  refresh_status();
});

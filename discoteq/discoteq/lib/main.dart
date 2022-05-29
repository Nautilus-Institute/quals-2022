import 'package:flutter/material.dart';
import 'dart:convert';
import 'dart:io';
import 'dart:math';
import 'dart:developer';
import 'dart:collection';

import 'package:uuid/uuid.dart';
import 'package:rfw/formats.dart' show parseLibraryFile, decodeLibraryBlob;
import 'package:http/http.dart' as http;
import 'package:flutter/services.dart';

import 'package:rfw/rfw.dart';
import 'package:url_launcher/url_launcher.dart';

import 'package:flutter/foundation.dart' show kIsWeb;
import 'package:web_socket_channel/web_socket_channel.dart';
import 'package:shared_preferences/shared_preferences.dart';

bool isRunningInWeb = false;

const int time_between_messages = 5000;

final config = <String, Object>{
  //TODO change your hostname here
  "base_url": "http://127.0.0.1:8080",
  "auto": false,
};

void main(List<String> args) {
  String? token = null;
  if (args != null) {
    if (args.length > 0) {
      config["base_url"] = args[0];
    }
    if (args.length > 1) {
      config["use_token"] = args[1];
    }
    if (args.length > 2 && args[2] == "auto") {
      config["auto"] = true;
    }
    if (args.length > 3 && config["auto"] == true) {
      config["auto_data"] = File(args[3]);
    }
  }
  isRunningInWeb = kIsWeb;

  FlutterError.onError = (FlutterErrorDetails details) {
    FlutterError.presentError(details);
    if (!kIsWeb && config["auto"] == true) {
      SystemChannels.platform.invokeMethod('SystemNavigator.pop');
    }
  };
  
  runApp(MaterialApp(
    title: 'Discoteq',
    theme: ThemeData(
      primarySwatch: Colors.blue,
    ),
    home: MainApp(),
  ));
}

Map<String, String> cookieJar = {};


Future<http.Response> makeRequest(String method, String url, {Map<String, String>? headers=null, String? body=null, followRedirects=true}) async {
  final client = http.Client();

  final request = http.Request(method, Uri.parse(url));
  if (body != null) {
    request.body = body;
  }
  if (headers != null) {
    request.headers.addAll(headers);
  }
  request.followRedirects = followRedirects;

  return await http.Response.fromStream(await client.send(request));

}

Future <String> getRequestWithCookies(String url, {Map<String, String>? headers: null}) async {
  //debugPrint("making request for $url");
  final res = await makeRequestWithCookies("GET", url, headers: headers);
  if (res.statusCode != 200) {
    throw Exception('Request for ${url} failed with ${res.statusCode} code');
  }
  return res.body;
}
Future <String> postRequestWithCookies(String url, {Map<String, String>? headers: null, String? body: null}) async {
  final res = await makeRequestWithCookies("POST", url, headers: headers, body: body);
  if (res.statusCode != 200) {
    throw Exception('Request for ${url} failed with ${res.statusCode} code');
  }
  return res.body;
}

Future <http.Response> makeRequestWithCookies(String method, String url, {Map<String, String>? headers=null, String? body=null}) async {
  if (isRunningInWeb) {
    //Cookies handled by browser
    return makeRequest(method, url, headers: headers, body: body);
  }

  final headersWithCookies = <String,String>{};
  if (headers != null) {
    headersWithCookies.addAll(headers);
  }

  var hostname = Uri.parse(url).host;
  //debugPrint("hostname $hostname");

  var cookie = cookieJar[hostname];

  if (cookie != null) {
    //debugPrint("Using old cookie $cookie");
    headersWithCookies['cookie'] = cookie;
  }

  var response = await makeRequest(method, url,
      headers: headersWithCookies, body: body, followRedirects: false);
  //debugPrint('${response.headers}');
  final location = response.headers["location"];
  if (location != null) {
    response = await makeRequestWithCookies(method, location,
        headers: headers, body: body, );
  }

  var newCookie = response.headers['set-cookie']?.split(';')[0];
  if (newCookie != null) {
    //debugPrint("Setting cookie $newCookie");
    cookieJar[hostname] = newCookie;
  }
  return response;
}

_showSnackbar(context, text) {
  final snackBar = SnackBar(
    content: Text(text),
  );
  ScaffoldMessenger.of(context).showSnackBar(snackBar);
}

void rebuildAllChildren(BuildContext context) {
  void rebuild(Element el) {
    el.markNeedsBuild();
    el.visitChildren(rebuild);
  }
  (context as Element).visitChildren(rebuild);
}

class MainApp extends StatefulWidget {
  const MainApp({Key? key}) : super(key: key);

  @override
  State<MainApp> createState() => _MainAppState();
}

class _MainAppState extends State<MainApp> {
  String _view = "login";
  String? _user = null;
  String? _token = null;
  String? _ticket = null;
  bool _is_admin = false;
  var _prefs = null;

  final String base_url = config["base_url"]! as String;
  String api_url = "";

  @override
  void initState() {
    api_url = base_url + '/api';
    _view = "login";
    _loadPrefs();
  }

  void _loadPrefs() async {
    _prefs = await SharedPreferences.getInstance();
    if (_prefs == null) return;
    var user   = _prefs.getString('user');
    var ticket = _prefs.getString('ticket') ?? "ADMIN_TICKET";
    bool is_admin = false;
    //debugPrint("use token ${config["use_token"]}");
    var token = config["use_token"] ?? _prefs.getString('token');
    if (token != null) {
      var res = await postRequestWithCookies(api_url + "/login",
        body: jsonEncode(<String, String>{
              "token": token,
        })
      );
      var jres = jsonDecode(res);
      token = jres["new_token"] ?? token;
      user = jres["username"] ?? user;
      is_admin = jres["is_admin"] ?? false;
    }
    //debugPrint("Loaded _prefs $user $token");
    if (user != null && token != null) {
      _afterLogin(user, ticket, token, is_admin);
    }
  }

  void _afterLogin(String user, String ticket, String token, bool is_admin) async {
    //debugPrint("After login $user $token");
    setState((){
      _view = "home";
      _user = user;
      _token = token;
      _ticket = ticket;
      _is_admin = is_admin;
    });
    if (_prefs != null) {
      await _prefs.setString("user", user);
      await _prefs.setString("ticket", ticket);
      await _prefs.setString("token", token);
      //debugPrint("wrote prefs");
    }
  }

  void _backToHome() {
    setState((){
      _view = "home";
    });
  }

  _logout() async {
    setState((){
      _view = "login";
      _user = null;
      _token = null;
    });
    if (_prefs != null) {
      await _prefs.remove("user");
      await _prefs.remove("token");
    }
  }

  _adminPanel() {
    setState((){
      _view = "admin";
    });
  }

  @override
  Widget build(BuildContext context) {
    if (_view == "login") {
      return Scaffold(
        appBar: AppBar(
            title: const Text("Discoteq Register"),
            automaticallyImplyLeading: false,
            ),
        body: Container(
            alignment: Alignment.center,
            child: Container(
              constraints: BoxConstraints(maxWidth: 500),
              child: LoginPage(
                  api_url: api_url,
                  next: _afterLogin,
                  ticket: _ticket,
              ),
            ),
        ),
      );
    }
    if (_view == "home") {
      return Scaffold(
        appBar: AppBar(title: const Text("Discoteq")),
        body: Container(
            decoration: BoxDecoration(
                image: DecorationImage(
                    image: AssetImage("assets/disco.png"),
                    fit: BoxFit.contain,
                ),
            ),
            alignment: Alignment.center,
            child: Container(
              constraints: BoxConstraints(maxWidth: 500),
              child: HomePage(
                base_url: base_url,
                user: _user!,
                token: _token!,
                ticket: _ticket!,
                prefs: _prefs),
            ),
        ),
        drawer: Drawer(
            child: ListView(
                padding: EdgeInsets.zero,
                children: [
                  DrawerHeader(
                    decoration: BoxDecoration(
                      color: Colors.blue,
                    ),
                    child: ListView(
                      children:[
                        Text(_user!, style: TextStyle(
                          color: Colors.white,
                          fontWeight: FontWeight.w500,
                          fontSize: 30)
                        ),
                        Text('Current Username', style: TextStyle(
                          color: Colors.white,
                          fontSize: 10)
                        ),
                      ],
                    ),
                  ),
                  ListTile(
                    leading: Icon(Icons.content_copy),
                    title: const Text('Copy Username To Clipboard'),
                    onTap: () {
                      Navigator.pop(context);
                      Clipboard.setData(ClipboardData(text: _user!));
                      _showSnackbar(context, "Copied Username To Clipboard");
                    }
                  ),
                  ListTile(
                    leading: Icon(Icons.account_box),
                    title: const Text('Change Username'),
                    onTap: () {
                      Navigator.pop(context);
                      _logout();
                    },
                  ),
                  if (kIsWeb) ListTile(
                      leading: Icon(Icons.download),
                      title: const Text('Download Desktop Application'),
                      onTap: () {
                        if (kIsWeb) {
                          String url = base_url + "/discoteq_desktop.zip";
                          launch(url);
                        }
                      }
                  ),
                  if (_is_admin) ListTile(
                    leading: Icon(Icons.admin_panel_settings),
                    title: const Text('Admin Panel'),
                    onTap: () {
                      Navigator.pop(context);
                      _adminPanel();
                    },
                  ),
                ],
            ),
        ),
      );
    }
    if (_view == "admin") {
      return Scaffold(
        appBar: AppBar(
            title: const Text("Discoteq Admin Panel"),
            automaticallyImplyLeading: false,
            ),
        body: Container(
            alignment: Alignment.center,
            child: Container(
              constraints: BoxConstraints(maxWidth: 500),
              child: AdminPage(
                  api_url: api_url,
                  done: _backToHome,
                  ticket: _ticket,
              ),
            ),
        ),
      );
    }
    throw Exception('ERROR bad view $_view');
  }
}

class URLImage extends StatelessWidget {
  const URLImage({Key? key, required this.url}) : super(key: key);

  final String url;

  @override
  Widget build(BuildContext context) {
    return Image(
        image: NetworkImage(url)
    );
  }

  static Widget FromSource(BuildContext context, DataSource source) {
    return Image.network(
        source.v<String>(<Object>["url"])!,
        width: 270,
        //height: 600,
        fit: BoxFit.fill,
        errorBuilder: (BuildContext context, Object error, StackTrace? stackTrace) {
          return Expanded(
            child: Text(
              "$error",
              style: TextStyle(color: Colors.red)
            ),
          );
        },
        loadingBuilder: (BuildContext context, Widget child, ImageChunkEvent? loadingProgress) {
          if (loadingProgress == null) return child;
          return Center(
            child: CircularProgressIndicator(
              value: loadingProgress.expectedTotalBytes != null
                  ? loadingProgress.cumulativeBytesLoaded /
                      loadingProgress.expectedTotalBytes!
                  : null,
            ),
          );
        },
    );
  }
}

Object? GetDataSourceValueWithKey(DataSource source, List<Object> key) {

  //debugPrint("Getting val $key");

  if (source.isList(key)) {
    List<Object> out = [];
    int length = source.length(key);
    for (var i=0; i<length; i++) {
      final subkey = <Object>[...key, i];
      final val = GetDataSourceValueWithKey(source, subkey);
      if (val == null)
        break;
      out.add(val);
    }
    return out;
  }

  return source.v<String>(key)
      ?? source.v<int>(key)
      ?? source.v<double>(key)
      ?? source.v<bool>(key);
}
Object? GetDataSourceValue(DataSource source, String key) {
  return GetDataSourceValueWithKey(source, <Object>[key]);
}
T? GetDataSourceValueType<T extends Object>(DataSource source, String key) {
  return source.v<T>(<Object>[key]);
}

class ApiMapper extends StatefulWidget {
  ApiMapper({Key? key,
    required this.url,
    required this.jsonKey,
    required this.dataKey,
    this.loading,
    //this.query,
    this.onLoaded,
  }) : super(key: key);

  final String url;
  final String jsonKey;
  final String dataKey;
  final Widget? loading;
  //final String? query;
  final VoidCallback? onLoaded;

  @override
  State<ApiMapper> createState() => _ApiMapperState();

  static Widget FromSource(BuildContext context, DataSource source) {
    return ApiMapper(
      url: GetDataSourceValueType<String>(source, "url")!,
      jsonKey: GetDataSourceValueType<String>(source, "jsonKey")!,
      dataKey: GetDataSourceValueType<String>(source, "dataKey")!,
      //query: GetDataSourceValueType<String>(source, "query"),
      onLoaded: source.voidHandler(<Object>["onLoaded"]),
    );
  }
}

class _ApiMapperState extends State<ApiMapper> {
  var _data = null;
  var _api = null;
  Map<String, dynamic>? _result = null;
  bool _loaded = false;

  @override
  void initState() {
    super.initState();
  }

  _loadRequest(String api_url) async {
    String url = api_url + widget.url;
    final jsonres = await getRequestWithCookies(url);
    _result = jsonDecode(jsonres);
    _updateIfReady();
  }

  _updateIfReady() {
    if (_loaded || _data == null || _result == null)
      return;

    _loaded = true;

    dynamic resultForKey = _result![widget.jsonKey];
    //debugPrint("result $_result");
    if (resultForKey == null)
      return;
    final DynamicList values = [];

    for (var i=0; i<resultForKey.length; i++) {
      values.add(resultForKey[i]);
    }
    //debugPrint("Updating ${widget.dataKey} with $values");
    _data.update(widget.dataKey, values);
    widget.onLoaded?.call();
  }

  @override
  Widget build(BuildContext context) {
    if (_data == null) {
      final chat = context.findAncestorStateOfType<_ChatWidgetState>();
      if (chat != null) {
        _data = chat.data;
        _loadRequest(chat.api_url);
      }
    }

    if (widget.loading != null)
      return widget.loading!;
    return CircularProgressIndicator();
  }
}


GetLocalWidgets() => <String, LocalWidgetBuilder>{
  'URLImage':URLImage.FromSource,
  'ApiMapper':ApiMapper.FromSource,
};


class HomePage extends StatefulWidget {
  HomePage({Key? key, 
    required this.base_url,
    required this.user,
    required this.token,
    required this.ticket,
    this.prefs = null,
  }) : super(key: key) {
    api_url = base_url + "/api";
  }

  final String base_url;
  String api_url = "";
  final String user;
  final String token;
  final String ticket;
  final prefs;

  @override
  State<HomePage> createState() => _HomePageState();
}

class ChatWidgetInfo {
  ChatWidgetInfo({required this.base_url, required this.widget_path, this.data}) {

  }

  final String base_url;
  final String widget_path;
  final Map<String, dynamic>? data;

}

T getRandomElement<T>(List<T> list) {
    final random = new Random();
    var i = random.nextInt(list.length);
    return list[i];
}

class _HomePageState extends State<HomePage> {
  WebSocketChannel? _channel = null;
  var _subscription = null;
  bool _connecting = true;
  String? _error = null;
  String? _warning = null;
  String _lastInputText = "";
  final _peers = <String>[];
  final _chatqueue = Queue<ChatWidgetInfo>();
  bool _hasTimer = false;
  List<String>? _auto_text = null;

  final _chatlog = <ChatWidgetInfo>[];
  final TextEditingController _inputController = TextEditingController();
  final TextEditingController _addContactController = TextEditingController();
  final ScrollController _listScrollController = ScrollController();

  @override
  void initState() {
    super.initState();

    if (widget.prefs != null || config["auto"] != true) {
      final saved_contacts = widget.prefs.getStringList('contacts');
      if (saved_contacts != null) {
        for (var c in saved_contacts) {
          _contacts[c] = false;
        }
      }
    }
    //debugPrint("connecting to websocket");
    _channel = WebSocketChannel.connect(Uri.parse('ws'+widget.api_url.substring(4)+'/ws/'));
    if (_channel == null) {
      _setError("Unable to connect to server");
      return;
    }
    _subscription = _channel!.stream.listen(_onMessage, onDone: (){
      _setError("Connection to server lost");
    });
    _channel!.sink.add(jsonEncode(<String,String>{
      "type": "token",
      "token": widget.token,
      "ticket": widget.ticket,
    }));
  }

  @override
  void dispose() {
    _subscription?.cancel();
    _channel?.sink.close();
    super.dispose();
  }

  void _setError(error) {
    //debugPrint("in set error");
    debugPrint("Error $error");
    setState((){
      _error = error;
    });
    if (!kIsWeb && config["auto"] == true) {
      SystemChannels.platform.invokeMethod('SystemNavigator.pop');
    }
  }

  void _setWarning(String? warn) {
    if (warn != null) {
      debugPrint("Warning $warn");
    }
    setState((){
      _warning = warn;
    });
  }

  void _addChatWidget(message) {
    if (!kIsWeb && config["auto"] == true) {
      final author = message.data?["author"]?["user"];
      if (author == widget.user) {
        return;
      }

      setState((){
        _chatlog.clear();
      });
    }
    Future.delayed(const Duration(milliseconds: 200), () {
      setState((){
        _chatlog.add(message);
        //debugPrint("Added widget to chatlog");
      });
    });

    Future.delayed(const Duration(milliseconds: 500), () {
      if (_chatlog.length > 0 && _listScrollController.hasClients) {
        _listScrollController.animateTo(
            _listScrollController.position.maxScrollExtent+1000,
            duration: Duration(milliseconds: 300), curve: Curves.easeOut);
      }
    });
  }

  _getUserInfo() {
    return <String,String>{
      "user": widget.user,
      "platform": kIsWeb? "web" : "desktop",
    };
  }

  void _autoRespond(msg) async {
    //debugPrint("Doing auto response to $msg");
    final author = msg["author"]?["user"];
    if (!(author is String)) {
      return;
    }

    if (_auto_text == null) {
      if (config["auto_data"] == null) {
        _auto_text = ["yo whats up"];
      } else {
        final file = await (config["auto_data"]! as File);
        final contents = await file.readAsString();
        //debugPrint("Got contents $contents");
        LineSplitter ls = new LineSplitter();
        _auto_text = ls.convert(contents.trim());
      }
    }

    await Future.delayed(Duration(seconds: 3));

    _inputController.text = getRandomElement(_auto_text!);
    await _sendChat(<String>[author]);
  }

  void _triggerAutoQueue() {
    if (kIsWeb) { return; }
    if (_chatqueue.length == 0) {
      return;
    }
    var info = _chatqueue.removeFirst();
    _addChatWidget(info);
    Future.delayed(const Duration(milliseconds: 1000), () {
      final msgUser = info.data?["author"]?["user"];
      if (msgUser != null && msgUser != widget.user) {
        _autoRespond(info.data);
      }
    });
  }

  void _startAutoQueueTimer() {
    if (kIsWeb) { return; }

    if (!_hasTimer && _chatqueue.length > 0) {
      _triggerAutoQueue();
    }

    _hasTimer = true;
    Future.delayed(const Duration(milliseconds: time_between_messages), () {
      if (kIsWeb) { return; }
      _triggerAutoQueue();
      _startAutoQueueTimer();
    });
  }

  void _onMessage(data) async {
    final msg = jsonDecode(data);
    //debugPrint("Got message $msg");
    final error = msg["error"];
    if (error != null) {
      _setError(error);
      return;
    }

    final type = msg["type"];
    if (type == null) {
      _setError("Unknown message $type");
      return;
    }

    if (type == "auth") {
      setState((){
        _connecting = false;
      });
      return;
    }

    if (type == "ratelimit") {
      _setWarning(msg["message"] ?? "Message failed due to ratelimit");
      _inputController.text = _lastInputText;
      return;
    }

    if (type == "widget") {
      //debugPrint("Creating chat widget");
      var info = ChatWidgetInfo(
          base_url: widget.base_url,
          widget_path: msg["widget"]!,
          data: msg,
      );
      if (!kIsWeb && config["auto"] == true) {
        _chatqueue.addLast(info);
        if (!_hasTimer) {
          _startAutoQueueTimer();
        }
        return;
      }
      _addChatWidget(info);
    }

  }

  _getTargets(onConfirm) {
    if (_contacts.length == 0) {
      _getNewUserName(onAdd: () {;
        _showPeersMenu(doConfirm: true, onConfirm:onConfirm);
      });
      return null;
    }
    var targets = _contacts.keys.where((k) => _contacts[k]!);
    if (targets.length == 0) {
      _showPeersMenu(doConfirm: true, onConfirm:onConfirm);
      return null;
    }
    return targets.toList();
  }

  _createPoll(String title, List<String> options) async {
    final targets = _getTargets(()=>_createPoll(title, options));
    if (targets == null)
      return;
    final jres = await postRequestWithCookies(widget.api_url + "/poll/create",
      body:jsonEncode(<String,Object>{
        "options":options,
      }
    ));
    final res = jsonDecode(jres);
    String? poll_id = res["poll_id"];
    //debugPrint("created poll $poll_id");
    _channel!.sink.add(jsonEncode(<String,Object>{
      "type": "widget",
      "widget": "/widget/poll",
      "author": _getUserInfo(),
      "recipients": targets,
      "data": <String,String>{
        "title": title,
        "apiGet": "/api/poll/options?poll=$poll_id",
        "apiVote": "/api/poll/vote?poll=$poll_id",
      },
    }));
  }

  _sendChat(List<String>? targets) async {
    if (targets == null) {
      targets = _getTargets(()=>_sendChat(null));
    }
    if (targets == null)
      return;
    final text = _inputController.text;
    if (text == "") return;

    var sent = false;

    _setWarning(null);

    for (var token in text.split(RegExp(r'\s+'))) {
      Uri? uri = null;
      try {
        uri = Uri.parse(token);
      } catch (err) { }
      if (uri == null)
        continue;
      if (uri.scheme == "http" || uri.scheme == "https") {
        //debugPrint("Trying to get info about $uri");
        final path = uri.path.toLowerCase();
        if (
               path.endsWith('.jpg')
            || path.endsWith('.jpeg')
            || path.endsWith('.png')
            || path.endsWith('.gif')
        ) {
          _channel!.sink.add(jsonEncode(<String,Object>{
            "type": "widget",
            "widget": "/widget/imagemessage",
            "author": _getUserInfo(),
            "recipients": targets,
            "data": <String,String>{
              "message": text,
              "image": uri.toString(),
            },
          }));
          sent = true;
          break;
        }
      }
    }


    if (!sent) {
      //debugPrint("sending chat $text");
      _channel!.sink.add(jsonEncode(<String,Object>{
        "type": "widget",
        "widget": "/widget/chatmessage",
        "author": _getUserInfo(),
        "recipients": targets,
        "data": <String,String>{
          "message": text,
        },
      }));
    }
    if (_chatlog.length > 0 && _listScrollController.hasClients) {
      _listScrollController.animateTo(
          _listScrollController.position.maxScrollExtent+1000,
          duration: Duration(milliseconds: 300), curve: Curves.easeOut);
    }
    _lastInputText = text;
    _inputController.clear();
  }

  var _contacts = <String,bool>{

  };

  Color _getColor(Set<MaterialState> states) {
    const Set<MaterialState> interactiveStates = <MaterialState>{
      MaterialState.pressed,
      MaterialState.hovered,
      MaterialState.focused,
    };
    if (states.any(interactiveStates.contains)) {
      return Colors.blue;
    }
    return Colors.red;
  }
  _getNewUserName({onAdd: null}) async {
    _addContactController.clear();
    await showDialog(context: context,
      builder: (context) {
        var addName = () async{
          final user = _addContactController.text;
          if (user.length == 0) {
            _showSnackbar(context, 'No username');
            return;
          }
          _contacts[user] = true;
          if (widget.prefs != null) {
            await widget.prefs.setStringList('contacts', _contacts.keys.toList());
          }
          //debugPrint("added $user");
          Navigator.pop(context);
          if (onAdd != null) {
            onAdd();
          }
        };
        return AlertDialog(
          title: Text('Add Contact'),
          content: TextField(
            onSubmitted: (_) => addName(),
            controller: _addContactController,
            decoration: InputDecoration(hintText: "User ID"),
          ),
          actions: <Widget>[
            FlatButton(
              textColor: Colors.black,
              child: Text('Cancel'),
              onPressed: () {
                Navigator.pop(context);
              },
            ),
            FlatButton(
              color: Colors.green,
              textColor: Colors.white,
              child: Text('Add'),
              onPressed: addName,
            ),
          ],
        );
      }
    );
  }

  _removePeersMenu() async {
    await showDialog(
      context: this.context,
      builder: (BuildContext context) {
        final toRemove = <String>{};
        return StatefulBuilder(
          builder: (BuildContext context, StateSetter setState) {
            var contacts = <Widget>[];

            for (var contact in _contacts.keys) {
              contacts.add(ListTile(
                title: Text(contact),
                leading: Checkbox(
                  checkColor: Colors.white,
                  fillColor: MaterialStateProperty.resolveWith(_getColor),
                  value: toRemove.contains(contact),
                  onChanged: (bool? val) async {
                    if (val == null) return;
                    setState((){
                      if (toRemove.contains(contact)) {
                        toRemove.remove(contact);
                      } else {
                        toRemove.add(contact);
                      }
                    });
                  },
                )
              ));
            }

            return AlertDialog(
              title: const Text("Delete Contacts"),
              content: SingleChildScrollView(
                child: ListBody(children: contacts),
              ),
              actions: <Widget>[
                FlatButton(
                  textColor: Colors.black,
                  child: Text('Cancel'),
                  onPressed: () {
                    Navigator.pop(context);
                  },
                ),
                FlatButton(
                  color: Colors.red,
                  textColor: Colors.white,
                  child: Text('Remove'),
                  onPressed: toRemove.length>0 ? (() async {
                    if (toRemove.length > 0) {
                      for (var r in toRemove) {
                        _contacts.remove(r);
                      }
                      if (widget.prefs != null) {
                        await widget.prefs.setStringList('contacts', _contacts.keys.toList());
                      }
                    }
                    Navigator.pop(context);
                  }) : null,
                ),
              ],
            );
          }
        );
      }
    );
  }


  _createPollMenu() async {
    final TextEditingController pollTitle = TextEditingController();
    var optionControllers = <TextEditingController>[];
    for (var i=0; i<4; i++) {
      optionControllers.add(TextEditingController());
    }
    await showDialog(
      context: this.context,
      builder: (BuildContext context) {
        var submit = () async {
          final title = pollTitle.text;
          if (title.length == 0) {
            _showSnackbar(context, 'No poll title');
            return;
          }

          final options = optionControllers.where((c)=>c.text.length>0)
              .map((c)=>c.text).toList();
          if (options.length == 0) {
            _showSnackbar(context, 'No poll options');
            return;
          }
          Navigator.pop(context);
          await _createPoll(title, options);
        };
        var options = <Widget>[];
        for (var c in optionControllers) {
          options.add(TextField(
            controller: c,
            decoration: InputDecoration(hintText: "Poll Option")
          ));
        }
        return AlertDialog(
          title: Text('Create Poll'),
          content: SingleChildScrollView(child: ListBody(children: [
            TextField(
              controller: pollTitle,
              decoration: InputDecoration(hintText: "Poll Title"),
            ),
            ...options,
          ])),
          actions: <Widget>[
            FlatButton(
              textColor: Colors.black,
              child: Text('Cancel'),
              onPressed: () {
                Navigator.pop(context);
              },
            ),
            FlatButton(
              color: Colors.green,
              textColor: Colors.white,
              child: Text('Send'),
              onPressed: submit,
            ),
          ],
        );
      }
    );
  }

  _showPeersMenu({doConfirm: false, onConfirm: null}) async {
    await showDialog(
      context: this.context,
      builder: (BuildContext context) {
        var allContacts = _contacts;
        return StatefulBuilder(
          builder: (BuildContext context, StateSetter setState) {
            var contacts = <Widget>[];
            var hasSelected = _contacts.keys.any((k) => _contacts[k]!);

           if (allContacts.length == 0) {
              contacts.add(ListTile(
                title: Padding(padding: EdgeInsets.only(left: 10), 
                  child: Text(
                  "No recipients...",
                  style:TextStyle(color: Colors.grey)
                ))
              ));
            }

            for (var contact in allContacts.entries) {
              contacts.add(ListTile(
                  title: Text(contact.key),
                  leading: Checkbox(
                      checkColor: Colors.white,
                      fillColor: MaterialStateProperty.resolveWith(_getColor),
                      value: contact.value,
                      onChanged: (bool? val) async {
                        if (val == null) return;
                        setState((){
                          allContacts[contact.key] = val;
                        });
                      },
                  )
              ));
            }

            if (_contacts.length > 0) {
              contacts.add(SimpleDialogOption(
                child: Text('Edit contacts',
                  style:TextStyle(color: Colors.black)
                ),
                onPressed: () async {
                  Navigator.pop(context);
                  await _removePeersMenu();
                  _showPeersMenu(doConfirm: doConfirm);
                },
              ));
            }
            contacts.add(SimpleDialogOption(
              child: Text('Add contact',
                style:TextStyle(color: Colors.blue)
              ),
              onPressed: () async {
                Navigator.pop(context);
                await _getNewUserName();
                _showPeersMenu(doConfirm: doConfirm);
              },
            ));

            return AlertDialog(
              title: const Text("Select Recipients"),
              content: SingleChildScrollView(
                child: ListBody(children: contacts),
              ),
              actions: <Widget>[
                FlatButton(
                  textColor: Colors.black,
                  child: Text(doConfirm? 'Cancel' : 'Done'),
                  onPressed: () {
                    Navigator.pop(context);
                  },
                ),
                if (doConfirm) FlatButton(
                  color: Colors.green,
                  textColor: Colors.white,
                  child: Text('Confirm'),
                  onPressed: hasSelected ? (() async {
                    if (onConfirm != null) {
                      onConfirm();
                    }
                    Navigator.pop(context);
                  }) : null,
                ),
              ],
            );
          },
        );// StatefulBuilder
      }
    );
  }

  _buildMessages(BuildContext context) {
    //debugPrint("Building messages: ${_chatlog.length} messages");
    var max_item_index = 0;
    List<Widget> messages = [];

    messages.add(const CircularProgressIndicator());
    return Flexible(child:
      _chatlog.length > 0?
      ListView.builder(
        padding: EdgeInsets.all(10),
        itemCount: _chatlog.length,
        itemBuilder: (context, index) {
          max_item_index = index;
          try {
          final info = _chatlog[index];
          //debugPrint("In item builder for $index ${info.data}");
          ChatWidget w = ChatWidget(
              base_url: info.base_url,
              widget_path: info.widget_path,
              data: info.data,
          );
          //debugPrint("Made widget $index");
          return Row(children: [
            Container(
              child: w,
              padding: EdgeInsets.fromLTRB(15, 10, 15, 10),
              width: 300,
              decoration: BoxDecoration(
                color: Colors.grey[300],
                borderRadius: BorderRadius.circular(8),
              ),
              margin: EdgeInsets.only(
                bottom: index == _chatlog.length? 20 : 10,
                right: 10,
              ),
            ),
          ],
          mainAxisAlignment: w.user == widget.user?
            MainAxisAlignment.end : MainAxisAlignment.start,
          );
          } catch(e) {
            return const CircularProgressIndicator();
          }
        },
        //reverse: true,
        controller: _listScrollController,
      )
      : Center(child: Text("No messages yet...", style:TextStyle(color: Colors.black)))
    );
    if (!kIsWeb && config["auto"] == true) {
      rebuildAllChildren(context);
    }
  }

  _buildInput() {
    return Container(child:
      Row(children: [
        Material(child:
          Container(
            margin: EdgeInsets.symmetric(horizontal: 1),
            child: IconButton(
              icon: Icon(Icons.group),
              onPressed: () {
                _showPeersMenu(doConfirm: false);
              },
              color: Colors.blue,
            ),
          ),
          color: Colors.white,
        ),
        Material(child:
          Container(
            margin: EdgeInsets.symmetric(horizontal: 1),
            child: IconButton(
              icon: Icon(Icons.poll),
              onPressed: () {
                //_createPoll("What's your favorite kind of shell?",["🐚","#"]);
                _createPollMenu();
              },
              color: Colors.blue,
            ),
          ),
          color: Colors.white,
        ),
        Flexible(child:
          Container(child:
            TextField(
              onSubmitted: (_) => _sendChat(null),
              style: TextStyle(color: Colors.blue, fontSize: 15),
              controller: _inputController,
              decoration: InputDecoration.collapsed(
                  //border: OutlineInputBorder(),
                  hintText: 'Type Your Message...',
                  hintStyle: TextStyle(color: Colors.grey),
              ),
              //focusNode
            ),
          ),
        ),
        Material(child:
          Container(
            margin: EdgeInsets.symmetric(horizontal: 8),
            child: IconButton(
              icon: Icon(Icons.send),
              onPressed: () => _sendChat(null),
              color: Colors.blue,
            ),
          ),
          color: Colors.white,
        ),
      ]),
      width: double.infinity,
      height: 50,
      decoration: BoxDecoration(
        border: Border(top:
          BorderSide(color: Colors.grey, width: 0.5)
        ),
        color: Colors.white
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    if (_error != null) {
      return Container(
          child: Text(_error!, style: TextStyle(color: Colors.red)),
      );

    }
    if (_connecting) {
      return Container(
          child: CircularProgressIndicator(),
      );
    }
    return Stack(
      children: [
        Column(
          children: [
            _buildMessages(context),
            if (_warning != null) Container(
                child: Text(_warning!, style: TextStyle(color: Colors.red))
            ),
            _buildInput(),
          ],
        ),
      ]
    );
  }
}

class AdminPage extends StatefulWidget {
  const AdminPage({Key? key, required this.api_url, this.done, this.ticket}) : super(key: key);

  final String api_url;
  final Function? done;
  final String? ticket;

  @override
  State<AdminPage> createState() => _AdminPageState();
}

class _AdminPageState extends State<AdminPage> {
  TextEditingController _ticketController = TextEditingController();
  bool gettingFlag = false;
  String? _error = null;
  String? _flag = null;

  @override
  void initState() {
    super.initState();
    if (widget.ticket != null) {
      _ticketController.text = widget.ticket!;
    }
  }


  void _getFlag() async {
    if (gettingFlag) return;
    final ticket = _ticketController.text;
    if (ticket == '') {
      setState((){
        gettingFlag = false;
        _error = 'No ticket entered';
      });
      return;
    }

    setState((){
      gettingFlag = true;
    });
    //debugPrint('Doing login');
    //debugPrint(name);
    try {
      var res = await postRequestWithCookies(widget.api_url + "/flag",
        body: jsonEncode(<String, String>{
          "ticket": ticket,
        })
      );
      //debugPrint('got res');
      //debugPrint('$res');
      var res_j = jsonDecode(res);


      String? error = res_j["error"];
      String? flag = res_j["flag"];

      if (error != null) {
        debugPrint("Error $error");
        setState((){
          gettingFlag = false;
          _error = '$error';
        });
        return;
      }


      setState((){
        _flag = flag;
        _error = null;
        gettingFlag = false;
      });

      Clipboard.setData(ClipboardData(text: flag!));
      _showSnackbar(context, "Copied Flag To Clipboard!");
    } catch (err) {
      debugPrint("Error $err");
      setState((){
        gettingFlag = false;
        _error = '$err';
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    final flagF = gettingFlag? null : _getFlag;
    return Padding(
        padding: const EdgeInsets.all(10),
        child: ListView(
            children: <Widget>[
              Container(
                  alignment: Alignment.center,
                  padding: const EdgeInsets.all(10),
                  child: const Text(
                      'Admin Panel',
                      style: TextStyle(
                          color: Colors.blue,
                          fontWeight: FontWeight.w500,
                          fontSize: 30),
                  )),
              Container(
                  padding: const EdgeInsets.all(10),
                  child: TextFormField(
                      controller: _ticketController,
                      decoration: const InputDecoration(
                          border: OutlineInputBorder(),
                          labelText: 'Ticket Please',
                      ),
                      onFieldSubmitted: (_) => _getFlag(),
                  ),
              ),
              Container(
                  height: 50,
                  padding: const EdgeInsets.fromLTRB(10,0,10,0),
                  child: ElevatedButton(
                      child: gettingFlag?
                        CircularProgressIndicator() : const Text('Get Flag'),
                      onPressed: flagF,
                  )
              ),
              if (_flag != null)
                Container(
                    alignment: Alignment.center,
                    padding: const EdgeInsets.all(10),
                    child: Text(
                        _flag!,
                        style: TextStyle(fontSize: 20),
                    )),
              if (_error != null)
                Container(
                    alignment: Alignment.center,
                    padding: const EdgeInsets.all(10),
                    child: Text(_error!),
                ),
              if (widget.done != null)
                Container(
                    height: 30,
                    padding: const EdgeInsets.fromLTRB(10,10,10,0),
                    child: ElevatedButton(
                        child: gettingFlag?
                          CircularProgressIndicator() : const Text('Back'),
                        onPressed: ()=>{widget.done!()},
                    )
                ),
            ],
          ));
  }
}

class LoginPage extends StatefulWidget {
  const LoginPage({Key? key, required this.api_url, this.next, this.ticket}) : super(key: key);

  final String api_url;
  final Function? next;
  final String? ticket;

  @override
  State<LoginPage> createState() => _LoginPageState();
}

class _LoginPageState extends State<LoginPage> {
  TextEditingController _nameController = TextEditingController();
  TextEditingController _ticketController = TextEditingController();
  bool doingLogin = false;
  String? _error = null;

  @override
  void initState() {
    super.initState();
    if (widget.ticket != null) {
      _ticketController.text = widget.ticket!;
    }
  }


  void _doLogin() async {
    if (doingLogin) return;
    final name = _nameController.text;
    if (name == '') {
      setState((){
        doingLogin = false;
        _error = 'No username entered';
      });
      return;
    }
    final ticket = _ticketController.text;
    if (ticket == '') {
      setState((){
        doingLogin = false;
        _error = 'No ticket entered';
      });
      return;
    }

    setState((){
      doingLogin = true;
    });
    //debugPrint('Doing login');
    //debugPrint(name);
    try {
      var res = await postRequestWithCookies(widget.api_url + "/register",
        body: jsonEncode(<String, String>{
              "username": name,
              "ticket": ticket,
        })
      );
      //debugPrint('got res');
      //debugPrint('$res');
      var res_j = jsonDecode(res);


      String? error = res_j["error"];
      String? user = res_j["username"];
      bool is_admin = res_j["is_admin"] ?? false;
      String? token = null;

      if (error == null) {
        res = await getRequestWithCookies(widget.api_url + "/token");
        res_j = jsonDecode(res);
        token = res_j["new_token"];
        error = res_j["error"];
      }

      if (error == null) {
        if (user == null || token == null)
          error = "Unable to register user";
      }
      if (error != null) {
        debugPrint("Error $error");
        setState((){
          doingLogin = false;
          _error = '$error';
        });
        return;
      }



      widget.next!(user, ticket, token, is_admin);
    } catch (err) {
      debugPrint("Error $err");
      setState((){
        doingLogin = false;
        _error = '$err';
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    final loginF = doingLogin? null : _doLogin;
    return Padding(
        padding: const EdgeInsets.all(10),
        child: ListView(
            children: <Widget>[
              Container(
                  alignment: Alignment.center,
                  padding: const EdgeInsets.all(10),
                  child: const Text(
                      'Discoteq',
                      style: TextStyle(
                          color: Colors.blue,
                          fontWeight: FontWeight.w500,
                          fontSize: 30),
                  )),
              Container(
                  alignment: Alignment.center,
                  padding: const EdgeInsets.all(10),
                  child: const Text(
                      'Select Username',
                      style: TextStyle(fontSize: 20),
                  )),
              Container(
                  padding: const EdgeInsets.all(10),
                  child: TextFormField(
                      controller: _nameController,
                      decoration: const InputDecoration(
                          border: OutlineInputBorder(),
                          labelText: 'Username',
                      ),
                      //onFieldSubmitted: (_) => _doLogin(),
                  ),
              ),
              Container(
                  padding: const EdgeInsets.all(10),
                  child: TextFormField(
                      controller: _ticketController,
                      decoration: const InputDecoration(
                          border: OutlineInputBorder(),
                          labelText: 'Ticket',
                      ),
                      onFieldSubmitted: (_) => _doLogin(),
                  ),
              ),
              Container(
                  height: 50,
                  padding: const EdgeInsets.fromLTRB(10,0,10,0),
                  child: ElevatedButton(
                      child: doingLogin?
                        CircularProgressIndicator() : const Text('Login'),
                      onPressed: loginF,
                  )
              ),
              if (_error != null)
                Container(
                    alignment: Alignment.center,
                    padding: const EdgeInsets.all(10),
                    child: Text(_error!),
                ),
            ],
          ));
  }
}


class RemoteChatWidget {
  RemoteChatWidget({required this.widget}) {
    runtime.update(const LibraryName(<String>['core', 'widgets']), createCoreWidgets());
    runtime.update(const LibraryName(<String>['core', 'material']), createMaterialWidgets());
    runtime.update(const LibraryName(<String>['local']), LocalWidgetLibrary(GetLocalWidgets()));
    runtime.update(const LibraryName(<String>['remote']), widget);
  }
  final RemoteWidgetLibrary widget;
  final Runtime runtime = Runtime();
}

final chatWidgetCache = <String, RemoteChatWidget>{};

Future<RemoteChatWidget> getChatWidget(String url) async {
  final existing = chatWidgetCache[url];
  if (config["auto"] != true && existing != null) return existing;

  //debugPrint("Loading remote chat widget $url");

  final response = await makeRequest("GET", url);

  //final txt = response.body;
  //final lib = parseLibraryFile(txt);
  final lib = decodeLibraryBlob(response.bodyBytes);
  final widget = RemoteChatWidget(widget: lib);
  chatWidgetCache[url] = widget;
  return widget;
}

class ChatWidget extends StatefulWidget {
  ChatWidget({Key? key,
    required this.base_url,
    required this.widget_path,
    this.data,
  }) : super(key: key)
  {
    user = data?["author"]?["user"];
  }

  final String base_url;
  final String widget_path;
  final Map<String, dynamic>? data;
  String? user;

  @override
  State<ChatWidget> createState() => _ChatWidgetState();
}

class _ChatWidgetState extends State<ChatWidget> {
  final DynamicContent data = DynamicContent();
  Future<RemoteChatWidget>? _remoteChatWidget;
  String api_url = "";

  @override
  void initState() {
    super.initState();
    api_url = widget.base_url;// + "/api";
    _remoteChatWidget = getChatWidget(widget.base_url + widget.widget_path);
    if (widget.data != null) {
      for (var e in widget.data!.entries) {
        data.update(e.key, e.value);
      }
      data.update('state', <String,Object>{});
    }
  }

  @override
  Widget build(BuildContext context) {
    return FutureBuilder<RemoteChatWidget>(
      future: _remoteChatWidget!,
      builder: (context, snapshot) {
        if (snapshot.hasData) {
          final rw = snapshot.data!;
          return RemoteWidget(
            runtime: rw.runtime,
            data: data,
            widget: const FullyQualifiedWidgetName(
                const LibraryName(<String>['remote']), 'root'),
            onEvent: (String name, DynamicMap arguments) async {
              //debugPrint('user triggered event "$name" with data: $arguments');
              //debugPrint('after ${arguments["after"].runtimeType}');
              if (name == "api_post") {
                String url = "${api_url}${arguments["path"]}";

                await postRequestWithCookies(url, body:jsonEncode(arguments["body"]));
              }
            },
          );
        } else if (snapshot.hasError) {
          return Text('${snapshot.error} ${snapshot.stackTrace}');
        }
        return const CircularProgressIndicator();
      },
    );
  }
}


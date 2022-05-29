import 'dart:io';

import 'package:rfw/formats.dart';

void main() async {

  final dir = Directory('../server/widgets_src');
  final entries = await dir.list().toList();
  for (var f in entries) {
    if (!f.path.endsWith('txt'))
      continue;
    final txt = File(f.path).readAsStringSync();
    final new_path = '../server/widgets/' + f.path.substring(0,f.path.length-7).split("/").last;
    File(new_path).writeAsBytesSync(encodeLibraryBlob(parseLibraryFile(txt)));
  }
}

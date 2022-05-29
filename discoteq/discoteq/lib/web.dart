import 'package:flutter/material.dart';
import 'main.dart' show MainApp;

void main() {
  runApp(MaterialApp(
    title: 'Discoteq',
    theme: ThemeData(
      primarySwatch: Colors.blue,
    ),
    home: MainApp(),
  ));
}

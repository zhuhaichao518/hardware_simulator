import 'package:flutter/material.dart';
import 'dart:async';

import 'package:hardware_simulator/hardware_simulator.dart';
void main() {
  runApp(MyApp());
}

class MyApp extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(title: Text('Hardware Simulator')),
        body: SimulatorScreen(),
      ),
    );
  }
}

class SimulatorScreen extends StatefulWidget {
  @override
  _SimulatorScreenState createState() => _SimulatorScreenState();
}

class _SimulatorScreenState extends State<SimulatorScreen> {
  final TextEditingController xController = TextEditingController();
  final TextEditingController yController = TextEditingController();
  final TextEditingController relXController = TextEditingController();
  final TextEditingController relYController = TextEditingController();
  final TextEditingController keyController = TextEditingController();

  final HardwareSimulator hardwareSimulator = HardwareSimulator();

  void _moveMouseAbsolute() {
    double x = double.tryParse(xController.text) ?? 0;
    double y = double.tryParse(yController.text) ?? 0;
    hardwareSimulator.getMouse().performMouseMoveAbsl(x, y, 0);
  }

  void _moveMouseRelative() {
    double deltaX = double.tryParse(relXController.text) ?? 0;
    double deltaY = double.tryParse(relYController.text) ?? 0;
    hardwareSimulator.getMouse().performMouseMoveRelative(deltaX, deltaY, 0);
  }

  void _pressKey() {
    _transferFocus();
    int keyCode = int.tryParse(keyController.text) ?? 0; // Assuming key code is an integer
    hardwareSimulator.getKeyboard().performKeyEvent(keyCode, true); // Press down
    Future.delayed(Duration(milliseconds: 100), () {
      hardwareSimulator.getKeyboard().performKeyEvent(keyCode, false); // Release
    });
  }

  FocusNode? _textFieldFocusNode;

  @override
  void initState() {
    super.initState();
    _textFieldFocusNode = FocusNode();
  }

  @override
  void dispose() {
    _textFieldFocusNode?.dispose();
    super.dispose();
  }

  void _transferFocus() {
    FocusScope.of(context).requestFocus(_textFieldFocusNode);
  }

  @override
  Widget build(BuildContext context) {
    return Column(
      mainAxisAlignment: MainAxisAlignment.center,
      children: [
        // First Row: Absolute Mouse Move
        Row(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Expanded(child: TextField(controller: xController, decoration: InputDecoration(labelText: 'X (% from left)'))),
            Expanded(child: TextField(controller: yController, decoration: InputDecoration(labelText: 'Y (% from top)'))),
            ElevatedButton(onPressed: _moveMouseAbsolute, child: Text('Move Mouse to X, Y')),
          ],
        ),
        SizedBox(height: 20),

        // Second Row: Relative Mouse Move
        Row(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Expanded(child: TextField(controller: relXController, decoration: InputDecoration(labelText: 'Delta X'))),
            Expanded(child: TextField(controller: relYController, decoration: InputDecoration(labelText: 'Delta Y'))),
            ElevatedButton(onPressed: _moveMouseRelative, child: Text('Move Mouse Relative')),
          ],
        ),
        SizedBox(height: 20),
        // Third Row: Keyboard Action
        Row(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Expanded(child: TextField(controller: keyController, decoration: InputDecoration(labelText: 'Key Code'))),
            ElevatedButton(onPressed: _pressKey, child: Text('Press to Simulate Keyboard Input')),
          ],
        ),
        SizedBox(height: 20),
        TextField(
          focusNode: _textFieldFocusNode,
          decoration: InputDecoration(
            hintText: 'Simulated keyboard input',
          ),
        ),
      ],
    );
  }
}
